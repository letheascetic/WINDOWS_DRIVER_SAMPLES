/*++

Module Name:

    queue.c

Abstract:

    This file contains the queue entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "private.h"
#include "queue.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, KmdfUsbQueueInitialize)
#endif

NTSTATUS
KmdfUsbQueueInitialize(
    _In_ WDFDEVICE Device
    )
/*++

Routine Description:


     The I/O dispatch callbacks for the frameworks device object
     are configured in this function.

     A single default I/O Queue is configured for parallel request
     processing, and a driver context memory allocation is created
     to hold our structure QUEUE_CONTEXT.

Arguments:

    Device - Handle to a framework device object.

Return Value:

    VOID

--*/
{
	NTSTATUS status;
	WDF_IO_QUEUE_CONFIG queueConfig;
	PDEVICE_CONTEXT pContext;

    PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> KmdfUsbQueueInitialize\n");

	pContext = GetDeviceContext(Device);

	// 创建框架Queue对象。
	// 注意事项：1. 调用WdfIoQueueCreate时候第一个参数Device被默认为Queue对象的父对象，父对象负责并维护子对象的生命周期。
	//				程序不必维护Queue对象。它只需创建，无需销毁。
	//			 2. 可设置其他的父对象，在参数WDF_OBJECT_ATTRIBUTES中设置。
	//			 3. Queue对象类型有：并行、串行、手动三种。
	//			 4. 执行WdfDeviceConfigureRequestDispatching以令某一类型的请求都排队到此Queue。

	// 创建默认并发队列，处理DeviceIOControl命令。
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
	queueConfig.EvtIoDeviceControl = DeviceIoControlParallel;
	status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &pContext->IoControlEntryQueue);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfIoQueueCreate failed with status 0x%08x\n", status);
		return status;
	}

	// 第二个队列：串列化队列，处理默认队列转发给它的需要串行处理的命令。
	WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchSequential);
	queueConfig.EvtIoDeviceControl = DeviceIoControlSerial;
	status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &pContext->IoControlSerialQueue);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfIoQueueCreate failed with status 0x%08x\n", status);
		return status;
	}

	// 第三个队列：串列化队列（读写操作）
	WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchSequential);
	//queueConfig.EvtIoWrite = BulkWrite;
	//queueConfig.EvtIoRead = BulkRead;
	status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &pContext->IoRWQueue);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfIoQueueCreate failed with status 0x%08x\n", status);
		return status;
	}

	// 配置第三个队列，只接受读和写两种请求。
	status = WdfDeviceConfigureRequestDispatching(Device, pContext->IoRWQueue, WdfRequestTypeWrite);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfDeviceConfigureRequestDispatching failed with status 0x%08x\n", status);
		return status;
	}
	status = WdfDeviceConfigureRequestDispatching(Device, pContext->IoRWQueue, WdfRequestTypeRead);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfDeviceConfigureRequestDispatching failed with status 0x%08x\n", status);
		return status;
	}

	// 第四个队列：手动队列。这个队列中保存应用程序与驱动进行同步的请求。
	// 当驱动有消息通知应用程序时，即从手动队列中提取一个请求完成之。
	WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);
	status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &pContext->AppSyncManualQueue);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfIoQueueCreate for manual queue failed with status 0x%08x\n", status);
		return status;
	}

	// 第五个队列：手动队列。这个队列用于处理中断数据，当驱动收到中断数据时，就从队列中提取一个请求并完成之。
	// 鼠标、键盘等中断设备，需用这种形式。
	WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);
	status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &pContext->InterruptManualQueue);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfIoQueueCreate for manual queue failed with status 0x%08x\n", status);
		return status;
	}
    
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- KmdfUsbQueueInitialize\n");
    return status;
}

VOID
KmdfUsbEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
/*++

Routine Description:

    This event is invoked when the framework receives IRP_MJ_DEVICE_CONTROL request.

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    OutputBufferLength - Size of the output buffer in bytes

    InputBufferLength - Size of the input buffer in bytes

    IoControlCode - I/O control code.

Return Value:

    VOID

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_QUEUE, 
                "%!FUNC! Queue 0x%p, Request 0x%p OutputBufferLength %d InputBufferLength %d IoControlCode %d", 
                Queue, Request, (int) OutputBufferLength, (int) InputBufferLength, IoControlCode);

    WdfRequestComplete(Request, STATUS_SUCCESS);

    return;
}

VOID
KmdfUsbEvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
)
/*++

Routine Description:

    This event is invoked for a power-managed queue before the device leaves the working state (D0).

Arguments:

    Queue -  Handle to the framework queue object that is associated with the
             I/O request.

    Request - Handle to a framework request object.

    ActionFlags - A bitwise OR of one or more WDF_REQUEST_STOP_ACTION_FLAGS-typed flags
                  that identify the reason that the callback function is being called
                  and whether the request is cancelable.

Return Value:

    VOID

--*/
{
    TraceEvents(TRACE_LEVEL_INFORMATION, 
                TRACE_QUEUE, 
                "%!FUNC! Queue 0x%p, Request 0x%p ActionFlags %d", 
                Queue, Request, ActionFlags);

    //
    // In most cases, the EvtIoStop callback function completes, cancels, or postpones
    // further processing of the I/O request.
    //
    // Typically, the driver uses the following rules:
    //
    // - If the driver owns the I/O request, it either postpones further processing
    //   of the request and calls WdfRequestStopAcknowledge, or it calls WdfRequestComplete
    //   with a completion status value of STATUS_SUCCESS or STATUS_CANCELLED.
    //  
    //   The driver must call WdfRequestComplete only once, to either complete or cancel
    //   the request. To ensure that another thread does not call WdfRequestComplete
    //   for the same request, the EvtIoStop callback must synchronize with the driver's
    //   other event callback functions, for instance by using interlocked operations.
    //
    // - If the driver has forwarded the I/O request to an I/O target, it either calls
    //   WdfRequestCancelSentRequest to attempt to cancel the request, or it postpones
    //   further processing of the request and calls WdfRequestStopAcknowledge.
    //
    // A driver might choose to take no action in EvtIoStop for requests that are
    // guaranteed to complete in a small amount of time. For example, the driver might
    // take no action for requests that are completed in one of the driver抯 request handlers.
    //

    return;
}

// 完成一个同步Request，并用相关信息填充这个Request
void CompleteSyncRequest(WDFDEVICE Device, DRIVER_SYNC_ORDER_TYPE type, int info)
{
	// NTSTATUS status;
	WDFREQUEST Request;
	if (NT_SUCCESS(GetOneSyncRequest(Device, &Request)))
	{
		PDriverSyncPackt pData = NULL;

		if (!NT_SUCCESS(WdfRequestRetrieveOutputBuffer(Request, sizeof(DriverSyncPackt), &(void*)pData, NULL)))
			WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
		else {

			// 填充Output结构内容
			pData->type = type;
			pData->info = info;
			WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(DriverSyncPackt));
		}
	}
}

// 从同步队列中取得一个有效Request。
NTSTATUS GetOneSyncRequest(WDFDEVICE Device, WDFREQUEST* pRequest)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	PDEVICE_CONTEXT pContext = GetDeviceContext(Device);
	ASSERT(pRequest);
	*pRequest = NULL;

	if (pContext->AppSyncManualQueue)
		status = WdfIoQueueRetrieveNextRequest(pContext->AppSyncManualQueue, pRequest);

	return status;
}

// 停止中断读操作
NTSTATUS InterruptReadStop(WDFDEVICE Device)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDFUSBPIPE	pipeInt = NULL;
	WDFREQUEST Request = NULL;
	PDEVICE_CONTEXT pContext = GetDeviceContext(Device);

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "--> InterruptReadStop\n");

	ASSERT(Device);
	pipeInt = GetInterruptPipe(TRUE, Device);

	if (pipeInt)
		// 如果还有未完成的IO操作，都Cancel掉。
		WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(pipeInt), WdfIoTargetCancelSentIo);

	// 完成在手动队列中的所有未完成Request。
	// 如果Queue处于未启动状态，会返回STATUS_WDF_PAUSED；
	// 如果已启动，则会挨个取得其Entry，直到返回STATUS_NO_MORE_ENTRIES。	
	do {
		status = WdfIoQueueRetrieveNextRequest(pContext->InterruptManualQueue, &Request);

		if (NT_SUCCESS(status))
		{
			WdfRequestComplete(Request, STATUS_SUCCESS);
		}
	} while (status != STATUS_NO_MORE_ENTRIES && status != STATUS_WDF_PAUSED);

	return STATUS_SUCCESS;
}

void ClearSyncQueue(WDFDEVICE Device)
{
	NTSTATUS status;
	WDFREQUEST Request = NULL;
	PDEVICE_CONTEXT pContext = GetDeviceContext(Device);

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "--> ClearSyncQueue\n");

	// 清空同步队列中的所有同步Request。此部分逻辑与上面函数相同。
	do {
		status = WdfIoQueueRetrieveNextRequest(pContext->AppSyncManualQueue, &Request);

		if (NT_SUCCESS(status))
			WdfRequestComplete(Request, STATUS_SUCCESS);

	} while (status != STATUS_NO_MORE_ENTRIES && status != STATUS_WDF_PAUSED);
}

// 取得中断管道。
// 数bIn决定获取input还是Output管道。
WDFUSBPIPE GetInterruptPipe(BOOLEAN bInPipe, WDFDEVICE Device)
{
	DEVICE_CONTEXT* pContext = NULL;
	WDFUSBINTERFACE Interface = NULL;
	WDFUSBPIPE	pipe = NULL;
	BYTE byNumPipes;
	BYTE by;

	WDF_USB_PIPE_INFORMATION pipeInfo;

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "--> GetInterruptPipe\n");

	ASSERT(Device);
	pContext = GetDeviceContext(Device);
	ASSERT(pContext);
	Interface = pContext->UsbInterface;
	ASSERT(Interface);

	// 从设备环境块中获取
	if (bInPipe)
		pipe = pContext->UsbIntInPipe;
	else
		pipe = pContext->UsbIntOutPipe;

	if (NULL == pipe)
	{
		byNumPipes = WdfUsbInterfaceGetNumConfiguredPipes(Interface);// 管道总数
		WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);

		// 枚举管道，并判断管道属性
		for (by = 0; by < byNumPipes; by++)
		{
			pipe = WdfUsbInterfaceGetConfiguredPipe(Interface, by, &pipeInfo);
			if (pipe)
			{
				if (pipeInfo.PipeType == WdfUsbPipeTypeInterrupt &&
					bInPipe == WdfUsbTargetPipeIsInEndpoint(pipe))
				{
					if (bInPipe)
						pContext->UsbIntInPipe = pipe;
					else
						pContext->UsbIntOutPipe = pipe;

					break;
				}

				pipe = NULL;
			}
		}
	}

	return pipe;
}

NTSTATUS SetLEDs(IN WDFDEVICE Device, IN UCHAR chSet)
{
	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_CONTEXT pContext = GetDeviceContext(Device);
	WDF_USB_CONTROL_SETUP_PACKET controlPacket;
	WDF_MEMORY_DESCRIPTOR hMemDes;

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "--> SetLEDs %c\n", chSet);
	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&hMemDes, &chSet, sizeof(UCHAR));

	WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(
		&controlPacket,
		BmRequestHostToDevice,
		BmRequestToDevice,
		0xD1, // Vendor命令
		0, 0);

	status = WdfUsbTargetDeviceSendControlTransferSynchronously(
		pContext->UsbDevice,
		NULL,
		NULL,
		&controlPacket,
		&hMemDes,
		NULL);

	return status;
}

NTSTATUS GetDigitron(IN WDFDEVICE Device, OUT UCHAR* pchGet)
{
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	PDEVICE_CONTEXT pContext = GetDeviceContext(Device);
	WDF_USB_CONTROL_SETUP_PACKET controlPacket;
	WDFMEMORY hMem = NULL;
	WDFREQUEST newRequest = NULL;

	ASSERT(pchGet);
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "--> GetDigitron\n");

	// 构造内存描述符
	ntStatus = WdfMemoryCreatePreallocated(WDF_NO_OBJECT_ATTRIBUTES, pchGet, sizeof(UCHAR), &hMem);
	if (!NT_SUCCESS(ntStatus))
		return ntStatus;

	// 初始化控制命令
	WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(
		&controlPacket,
		BmRequestDeviceToHost,// input命令
		BmRequestToDevice,
		0xD4,// Vendor 命令D4
		0, 0);

	// 创建新的WDF REQUEST对象。
	ntStatus = WdfRequestCreate(NULL, NULL, &newRequest);
	if (!NT_SUCCESS(ntStatus))
		return ntStatus;

	WdfUsbTargetDeviceFormatRequestForControlTransfer(pContext->UsbDevice, newRequest, &controlPacket, hMem, NULL);

	if (NT_SUCCESS(ntStatus))
	{
		// 同步发送
		WDF_REQUEST_SEND_OPTIONS opt;
		WDF_REQUEST_SEND_OPTIONS_INIT(&opt, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
		if (WdfRequestSend(newRequest, WdfDeviceGetIoTarget(Device), &opt))
		{
			WDF_REQUEST_COMPLETION_PARAMS par;
			WDF_REQUEST_COMPLETION_PARAMS_INIT(&par);
			WdfRequestGetCompletionParams(newRequest, &par);

			// 判断读取到的字符长度。
			if (sizeof(UCHAR) != par.Parameters.Usb.Completion->Parameters.DeviceControlTransfer.Length)
				ntStatus = STATUS_UNSUCCESSFUL;
		}
		else
			ntStatus = STATUS_UNSUCCESSFUL;
	}

	// 通过WdfXxxCreate创建的对象，必须删除
	WdfObjectDelete(newRequest);

	return ntStatus;
}

NTSTATUS GetLEDs(IN WDFDEVICE Device, OUT UCHAR* pchGet)
{
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	PDEVICE_CONTEXT pContext = GetDeviceContext(Device);
	WDF_USB_CONTROL_SETUP_PACKET controlPacket;
	WDFMEMORY hMem = NULL;
	WDFREQUEST newRequest = NULL;

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "--> GetLEDs\n");
	ASSERT(pchGet);

	// 构造内存描述符
	ntStatus = WdfMemoryCreatePreallocated(WDF_NO_OBJECT_ATTRIBUTES, pchGet, sizeof(UCHAR), &hMem);
	if (!NT_SUCCESS(ntStatus))
		return ntStatus;

	// 初始化控制命令
	WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(
		&controlPacket,
		BmRequestDeviceToHost,// input命令
		BmRequestToDevice,
		0xD3,// Vendor 命令D3
		0, 0);

	// 创建WDF REQUEST对象。
	ntStatus = WdfRequestCreate(NULL, NULL, &newRequest);
	if (!NT_SUCCESS(ntStatus))
		return ntStatus;

	WdfUsbTargetDeviceFormatRequestForControlTransfer(pContext->UsbDevice, newRequest, &controlPacket, hMem, NULL);

	if (NT_SUCCESS(ntStatus))
	{
		// 同步发送
		WDF_REQUEST_SEND_OPTIONS opt;
		WDF_REQUEST_SEND_OPTIONS_INIT(&opt, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
		if (WdfRequestSend(newRequest, WdfDeviceGetIoTarget(Device), &opt))
		{
			WDF_REQUEST_COMPLETION_PARAMS par;
			WDF_REQUEST_COMPLETION_PARAMS_INIT(&par);
			WdfRequestGetCompletionParams(newRequest, &par);

			// 判断读取到的字符长度。
			if (sizeof(UCHAR) != par.Parameters.Usb.Completion->Parameters.DeviceControlTransfer.Length)
				ntStatus = STATUS_UNSUCCESSFUL;
		}
		else
			ntStatus = STATUS_UNSUCCESSFUL;
	}

	// 通过WdfXxxCreate创建的对象，必须删除
	WdfObjectDelete(newRequest);

	return ntStatus;
}

NTSTATUS SetDigitron(IN WDFDEVICE Device, IN UCHAR chSet)
{
	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_CONTEXT pContext = GetDeviceContext(Device);
	WDF_USB_CONTROL_SETUP_PACKET controlPacket;
	WDF_MEMORY_DESCRIPTOR hMemDes;

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "--> SetDigitron %d\n", chSet);
	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&hMemDes, &chSet, sizeof(UCHAR));

	WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(
		&controlPacket,
		BmRequestHostToDevice,
		BmRequestToDevice,
		0xD2, // Vendor命令
		0,
		0);

	status = WdfUsbTargetDeviceSendControlTransferSynchronously(
		pContext->UsbDevice,
		NULL,
		NULL,
		&controlPacket,
		&hMemDes,
		NULL);

	return status;
}