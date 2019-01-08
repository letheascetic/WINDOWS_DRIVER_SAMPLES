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

	// �������Queue����
	// ע�����1. ����WdfIoQueueCreateʱ���һ������Device��Ĭ��ΪQueue����ĸ����󣬸�������ά���Ӷ�����������ڡ�
	//				���򲻱�ά��Queue������ֻ�贴�����������١�
	//			 2. �����������ĸ������ڲ���WDF_OBJECT_ATTRIBUTES�����á�
	//			 3. Queue���������У����С����С��ֶ����֡�
	//			 4. ִ��WdfDeviceConfigureRequestDispatching����ĳһ���͵������Ŷӵ���Queue��

	// ����Ĭ�ϲ������У�����DeviceIOControl���
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
	queueConfig.EvtIoDeviceControl = DeviceIoControlParallel;
	status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &pContext->IoControlEntryQueue);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfIoQueueCreate failed with status 0x%08x\n", status);
		return status;
	}

	// �ڶ������У����л����У�����Ĭ�϶���ת����������Ҫ���д�������
	WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchSequential);
	queueConfig.EvtIoDeviceControl = DeviceIoControlSerial;
	status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &pContext->IoControlSerialQueue);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfIoQueueCreate failed with status 0x%08x\n", status);
		return status;
	}

	// ���������У����л����У���д������
	WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchSequential);
	//queueConfig.EvtIoWrite = BulkWrite;
	//queueConfig.EvtIoRead = BulkRead;
	status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &pContext->IoRWQueue);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfIoQueueCreate failed with status 0x%08x\n", status);
		return status;
	}

	// ���õ��������У�ֻ���ܶ���д��������
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

	// ���ĸ����У��ֶ����С���������б���Ӧ�ó�������������ͬ��������
	// ����������Ϣ֪ͨӦ�ó���ʱ�������ֶ���������ȡһ���������֮��
	WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);
	status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &pContext->AppSyncManualQueue);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfIoQueueCreate for manual queue failed with status 0x%08x\n", status);
		return status;
	}

	// ��������У��ֶ����С�����������ڴ����ж����ݣ��������յ��ж�����ʱ���ʹӶ�������ȡһ���������֮��
	// ��ꡢ���̵��ж��豸������������ʽ��
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
    // take no action for requests that are completed in one of the driver�s request handlers.
    //

    return;
}

// ���һ��ͬ��Request�����������Ϣ������Request
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

			// ���Output�ṹ����
			pData->type = type;
			pData->info = info;
			WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, sizeof(DriverSyncPackt));
		}
	}
}

// ��ͬ��������ȡ��һ����ЧRequest��
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

// ֹͣ�ж϶�����
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
		// �������δ��ɵ�IO��������Cancel����
		WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(pipeInt), WdfIoTargetCancelSentIo);

	// ������ֶ������е�����δ���Request��
	// ���Queue����δ����״̬���᷵��STATUS_WDF_PAUSED��
	// �������������ᰤ��ȡ����Entry��ֱ������STATUS_NO_MORE_ENTRIES��	
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

	// ���ͬ�������е�����ͬ��Request���˲����߼������溯����ͬ��
	do {
		status = WdfIoQueueRetrieveNextRequest(pContext->AppSyncManualQueue, &Request);

		if (NT_SUCCESS(status))
			WdfRequestComplete(Request, STATUS_SUCCESS);

	} while (status != STATUS_NO_MORE_ENTRIES && status != STATUS_WDF_PAUSED);
}

// ȡ���жϹܵ���
// ��bIn������ȡinput����Output�ܵ���
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

	// ���豸�������л�ȡ
	if (bInPipe)
		pipe = pContext->UsbIntInPipe;
	else
		pipe = pContext->UsbIntOutPipe;

	if (NULL == pipe)
	{
		byNumPipes = WdfUsbInterfaceGetNumConfiguredPipes(Interface);// �ܵ�����
		WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);

		// ö�ٹܵ������жϹܵ�����
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
		0xD1, // Vendor����
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

	// �����ڴ�������
	ntStatus = WdfMemoryCreatePreallocated(WDF_NO_OBJECT_ATTRIBUTES, pchGet, sizeof(UCHAR), &hMem);
	if (!NT_SUCCESS(ntStatus))
		return ntStatus;

	// ��ʼ����������
	WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(
		&controlPacket,
		BmRequestDeviceToHost,// input����
		BmRequestToDevice,
		0xD4,// Vendor ����D4
		0, 0);

	// �����µ�WDF REQUEST����
	ntStatus = WdfRequestCreate(NULL, NULL, &newRequest);
	if (!NT_SUCCESS(ntStatus))
		return ntStatus;

	WdfUsbTargetDeviceFormatRequestForControlTransfer(pContext->UsbDevice, newRequest, &controlPacket, hMem, NULL);

	if (NT_SUCCESS(ntStatus))
	{
		// ͬ������
		WDF_REQUEST_SEND_OPTIONS opt;
		WDF_REQUEST_SEND_OPTIONS_INIT(&opt, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
		if (WdfRequestSend(newRequest, WdfDeviceGetIoTarget(Device), &opt))
		{
			WDF_REQUEST_COMPLETION_PARAMS par;
			WDF_REQUEST_COMPLETION_PARAMS_INIT(&par);
			WdfRequestGetCompletionParams(newRequest, &par);

			// �ж϶�ȡ�����ַ����ȡ�
			if (sizeof(UCHAR) != par.Parameters.Usb.Completion->Parameters.DeviceControlTransfer.Length)
				ntStatus = STATUS_UNSUCCESSFUL;
		}
		else
			ntStatus = STATUS_UNSUCCESSFUL;
	}

	// ͨ��WdfXxxCreate�����Ķ��󣬱���ɾ��
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

	// �����ڴ�������
	ntStatus = WdfMemoryCreatePreallocated(WDF_NO_OBJECT_ATTRIBUTES, pchGet, sizeof(UCHAR), &hMem);
	if (!NT_SUCCESS(ntStatus))
		return ntStatus;

	// ��ʼ����������
	WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(
		&controlPacket,
		BmRequestDeviceToHost,// input����
		BmRequestToDevice,
		0xD3,// Vendor ����D3
		0, 0);

	// ����WDF REQUEST����
	ntStatus = WdfRequestCreate(NULL, NULL, &newRequest);
	if (!NT_SUCCESS(ntStatus))
		return ntStatus;

	WdfUsbTargetDeviceFormatRequestForControlTransfer(pContext->UsbDevice, newRequest, &controlPacket, hMem, NULL);

	if (NT_SUCCESS(ntStatus))
	{
		// ͬ������
		WDF_REQUEST_SEND_OPTIONS opt;
		WDF_REQUEST_SEND_OPTIONS_INIT(&opt, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
		if (WdfRequestSend(newRequest, WdfDeviceGetIoTarget(Device), &opt))
		{
			WDF_REQUEST_COMPLETION_PARAMS par;
			WDF_REQUEST_COMPLETION_PARAMS_INIT(&par);
			WdfRequestGetCompletionParams(newRequest, &par);

			// �ж϶�ȡ�����ַ����ȡ�
			if (sizeof(UCHAR) != par.Parameters.Usb.Completion->Parameters.DeviceControlTransfer.Length)
				ntStatus = STATUS_UNSUCCESSFUL;
		}
		else
			ntStatus = STATUS_UNSUCCESSFUL;
	}

	// ͨ��WdfXxxCreate�����Ķ��󣬱���ɾ��
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
		0xD2, // Vendor����
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