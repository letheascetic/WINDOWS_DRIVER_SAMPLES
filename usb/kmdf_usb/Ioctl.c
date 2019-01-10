
#include "private.h"
#include "ioctl.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, KmdfUsbEvtIoDeviceControl)
#pragma alloc_text(PAGE, ResetPipe)
#pragma alloc_text(PAGE, ResetDevice)
#pragma alloc_text(PAGE, ReenumerateDevice)
#pragma alloc_text(PAGE, GetBarGraphState)
#pragma alloc_text(PAGE, SetBarGraphState)
#pragma alloc_text(PAGE, GetSevenSegmentState)
#pragma alloc_text(PAGE, SetSevenSegmentState)
#pragma alloc_text(PAGE, GetSwitchState)
#endif

VOID
KmdfUsbEvtIoDeviceControl(
	_In_ WDFQUEUE   Queue,
	_In_ WDFREQUEST Request,
	_In_ size_t     OutputBufferLength,
	_In_ size_t     InputBufferLength,
	_In_ ULONG      IoControlCode
)
/*++

Routine Description:

	This event is called when the framework receives IRP_MJ_DEVICE_CONTROL requests from the system.

Arguments:

	Queue - Handle to the framework queue object that is associated with the I/O request.

	Request - Handle to a framework request object.

	OutputBufferLength - length of the request's output buffer,
						if an output buffer is available.
	InputBufferLength - length of the request's input buffer,
						if an input buffer is available.

	IoControlCode - the driver-defined or system-defined I/O control code (IOCTL) that is associated with the request.

Return Value:

	VOID

--*/
{
	WDFDEVICE           device;
	PDEVICE_CONTEXT     pDevContext;
	size_t              bytesReturned = 0;
	PBAR_GRAPH_STATE    barGraphState = NULL;
	PSWITCH_STATE       switchState = NULL;
	PUCHAR              sevenSegment = NULL;
	BOOLEAN             requestPending = FALSE;
	NTSTATUS            status = STATUS_INVALID_DEVICE_REQUEST;

	UNREFERENCED_PARAMETER(InputBufferLength);
	UNREFERENCED_PARAMETER(OutputBufferLength);

	// If your driver is at the top of its driver stack, EvtIoDeviceControl is called at IRQL = PASSIVE_LEVEL.
	_IRQL_limited_to_(PASSIVE_LEVEL);

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "--> KmdfUsbEvtIoDeviceControl\n");

	// initialize variables
	device = WdfIoQueueGetDevice(Queue);
	pDevContext = GetDeviceContext(device);

	switch (IoControlCode) {

	case IOCTL_KMDFUSB_GET_CONFIG_DESCRIPTOR: 
	{
		// 获取配置描述符
		// 1) 获取配置描述符长度
		// 2) 获取IO请求的输出缓冲区地址
		// 3) 获取配置描述符，存储至IO请求的输出缓冲区

		PUSB_CONFIGURATION_DESCRIPTOR   configurationDescriptor = NULL;
		USHORT                          requiredSize = 0;

		// 获取配置描述符长度requiredSize
		status = WdfUsbTargetDeviceRetrieveConfigDescriptor(
			pDevContext->UsbDevice,
			NULL,
			&requiredSize);

		if (status != STATUS_BUFFER_TOO_SMALL) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "WdfUsbTargetDeviceRetrieveConfigDescriptor failed 0x%x\n", status);
			break;
		}

		// 获取request的输出缓冲区
		// Get the buffer - make sure the buffer is big enough
		status = WdfRequestRetrieveOutputBuffer(Request,
			(size_t)requiredSize,			// 缓冲区的最小长度
			&configurationDescriptor,		// request输出缓冲区的地址
			NULL);

		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "WdfRequestRetrieveOutputBuffer failed 0x%x\n", status);
			break;
		}

		// 获取配置描述符，存储到configurationDescriptor
		status = WdfUsbTargetDeviceRetrieveConfigDescriptor(
			pDevContext->UsbDevice,
			configurationDescriptor,
			&requiredSize);

		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "WdfUsbTargetDeviceRetrieveConfigDescriptor failed 0x%x\n", status);
			break;
		}

		bytesReturned = requiredSize;
	}
		break;

	case IOCTL_KMDFUSB_RESET_DEVICE:
		// 重置设备
		status = ResetDevice(device);
		break;

	case IOCTL_KMDFUSB_REENUMERATE_DEVICE:
		// 重新枚举USB设备
		status = ReenumerateDevice(pDevContext);
		bytesReturned = 0;
		break;

	case IOCTL_KMDFUSB_GET_BAR_GRAPH_DISPLAY:
		// Make sure the caller's output buffer is large enough to hold the state of the bar graph
		status = WdfRequestRetrieveOutputBuffer(Request,
			sizeof(BAR_GRAPH_STATE),
			&barGraphState,
			NULL);

		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "User's output buffer is too small for this IOCTL, expecting an BAR_GRAPH_STATE\n");
			break;
		}
		//
		// Call our function to get the bar graph state
		//
		status = GetBarGraphState(pDevContext, barGraphState);

		// If we succeeded return the user their data
		if (NT_SUCCESS(status)) {
			bytesReturned = sizeof(BAR_GRAPH_STATE);
		}
		else {
			bytesReturned = 0;
		}
		break;

	case IOCTL_KMDFUSB_SET_BAR_GRAPH_DISPLAY:

		status = WdfRequestRetrieveInputBuffer(Request,
			sizeof(BAR_GRAPH_STATE),
			&barGraphState,
			NULL);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL,
				"User's input buffer is too small for this IOCTL, expecting an BAR_GRAPH_STATE\n");
			break;
		}

		//
		// Call our routine to set the bar graph state
		//
		status = SetBarGraphState(pDevContext, barGraphState);

		//
		// There's no data returned for this call
		//
		bytesReturned = 0;
		break;

	case IOCTL_KMDFUSB_GET_7_SEGMENT_DISPLAY:

		status = WdfRequestRetrieveOutputBuffer(Request,
			sizeof(UCHAR),
			&sevenSegment,
			NULL);

		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL,
				"User's output buffer is too small for this IOCTL, expecting an UCHAR\n");
			break;
		}

		//
		// Call our function to get the 7 segment state
		//
		status = GetSevenSegmentState(pDevContext, sevenSegment);

		//
		// If we succeeded return the user their data
		//
		if (NT_SUCCESS(status)) {

			bytesReturned = sizeof(UCHAR);

		}
		else {

			bytesReturned = 0;

		}
		break;

	case IOCTL_KMDFUSB_SET_7_SEGMENT_DISPLAY:

		status = WdfRequestRetrieveInputBuffer(Request,
			sizeof(UCHAR),
			&sevenSegment,
			NULL);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL,
				"User's input buffer is too small for this IOCTL, expecting an UCHAR\n");
			bytesReturned = sizeof(UCHAR);
			break;
		}

		//
		// Call our routine to set the 7 segment state
		//
		status = SetSevenSegmentState(pDevContext, sevenSegment);

		//
		// There's no data returned for this call
		//
		bytesReturned = 0;
		break;

	case IOCTL_KMDFUSB_READ_SWITCHES:

		status = WdfRequestRetrieveOutputBuffer(Request,
			sizeof(SWITCH_STATE),
			&switchState,
			NULL);// BufferLength

		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL,
				"User's output buffer is too small for this IOCTL, expecting a SWITCH_STATE\n");
			bytesReturned = sizeof(SWITCH_STATE);
			break;

		}

		//
		// Call our routine to get the state of the switches
		//
		status = GetSwitchState(pDevContext, switchState);

		//
		// If successful, return the user their data
		//
		if (NT_SUCCESS(status)) {

			bytesReturned = sizeof(SWITCH_STATE);

		}
		else {
			//
			// Don't return any data
			//
			bytesReturned = 0;
		}
		break;

	case IOCTL_KMDFUSB_GET_INTERRUPT_MESSAGE:

		//
		// Forward the request to an interrupt message queue and dont complete
		// the request until an interrupt from the USB device occurs.
		//
		status = WdfRequestForwardToIoQueue(Request, pDevContext->InterruptMsgQueue);
		if (NT_SUCCESS(status)) {
			requestPending = TRUE;
		}

		break;

	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	if (requestPending == FALSE) {
		WdfRequestCompleteWithInformation(Request, status, bytesReturned);
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "<-- OsrFxEvtIoDeviceControl\n");

	return;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
ResetPipe(
	_In_ WDFUSBPIPE             Pipe
)
/*++

Routine Description:

	This routine resets the pipe.

Arguments:

	Pipe - framework pipe handle

Return Value:

	NT status value

--*/
{
	NTSTATUS          status;

	PAGED_CODE();

	//
	//  This routine synchronously submits a URB_FUNCTION_RESET_PIPE
	// request down the stack.
	//
	status = WdfUsbTargetPipeResetSynchronously(Pipe,
		WDF_NO_HANDLE,	// WDFREQUEST
		NULL			// PWDF_REQUEST_SEND_OPTIONS
	);

	if (NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "ResetPipe - success\n");
		status = STATUS_SUCCESS;
	}
	else {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "ResetPipe - failed\n");
	}

	return status;
}

VOID
StopAllPipes(
	IN PDEVICE_CONTEXT DeviceContext
)
{
	// The WdfUsbTargetPipeGetIoTarget method returns a handle to the I/O target object that is associated with a specified USB pipe.
	// The WdfIoTargetStop method stops sending queued requests to a local or remote I/O target.
	// WdfIoTargetCancelSentIo，停止目标对象的措施类型
	// 表明在停止目标对象前，取消已经在目标对象队列中的请求，等待所有请求完成，最后返回

	WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(DeviceContext->InterruptPipe),
		WdfIoTargetCancelSentIo);
	WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(DeviceContext->BulkReadPipe),
		WdfIoTargetCancelSentIo);
	WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(DeviceContext->BulkWritePipe),
		WdfIoTargetCancelSentIo);
}

NTSTATUS
StartAllPipes(
	IN PDEVICE_CONTEXT DeviceContext
)
{
	NTSTATUS status;

	status = WdfIoTargetStart(WdfUsbTargetPipeGetIoTarget(DeviceContext->InterruptPipe));
	if (!NT_SUCCESS(status)) {
		return status;
	}

	status = WdfIoTargetStart(WdfUsbTargetPipeGetIoTarget(DeviceContext->BulkReadPipe));
	if (!NT_SUCCESS(status)) {
		return status;
	}

	status = WdfIoTargetStart(WdfUsbTargetPipeGetIoTarget(DeviceContext->BulkWritePipe));
	if (!NT_SUCCESS(status)) {
		return status;
	}

	return status;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
ResetDevice(
	_In_ WDFDEVICE Device
)
/*++

Routine Description:

	This routine calls WdfUsbTargetDeviceResetPortSynchronously to reset the device if it's still connected.

Arguments:

	Device - Handle to a framework device

Return Value:

	NT status value

--*/
{
	PDEVICE_CONTEXT pDeviceContext;
	NTSTATUS status;

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "--> ResetDevice\n");

	pDeviceContext = GetDeviceContext(Device);

	// 获取同步锁，等待时间无限，防止多线程重入
	status = WdfWaitLockAcquire(pDeviceContext->ResetDeviceWaitLock, NULL);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "ResetDevice - could not acquire lock\n");
		return status;
	}

	// 停止所有管道
	StopAllPipes(pDeviceContext);

	// 同步方式重置USB设备
	status = WdfUsbTargetDeviceResetPortSynchronously(pDeviceContext->UsbDevice);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "ResetDevice failed - 0x%x\n", status);
	}

	// 启动所有管道
	status = StartAllPipes(pDeviceContext);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "Failed to start all pipes - 0x%x\n", status);
	}

	// 释放锁
	WdfWaitLockRelease(pDeviceContext->ResetDeviceWaitLock);

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "<-- ResetDevice\n");
	return status;
}


_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
ReenumerateDevice(
	_In_ PDEVICE_CONTEXT DevContext
)
/*++

Routine Description

	This routine re-enumerates the USB device.

Arguments:

	pDevContext - One of our device extensions

Return Value:

	NT status value

--*/
{
	NTSTATUS status;
	WDF_USB_CONTROL_SETUP_PACKET    controlSetupPacket;
	WDF_REQUEST_SEND_OPTIONS        sendOptions;
	GUID                            activity;

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "--> ReenumerateDevice\n");

	// 初始化WDF_REQUEST_SEND_OPTIONS结构体，开启指定的功能标志位
	// 这里开启的是WDF_REQUEST_SEND_OPTION_TIMEOUT标志位，开启后WDF_REQUEST_SEND_OPTIONS结构体的Timeout成员值才有效
	WDF_REQUEST_SEND_OPTIONS_INIT(
		&sendOptions,
		WDF_REQUEST_SEND_OPTION_TIMEOUT
	);

	// 设置Timeout的值，经过一段时间后请求还没完成，则框架会取消该请求
	// Timeout为负数，表示与当前系统时间的时间间隔
	// Timeout为正，表示自1601-01-01计时的绝对时间
	// Timeout为0，表示不设置超时时间
	// 如果请求因超时被取消，框架为该请求返回状态码为STATUS_IO_TIMEOUT
	// 如果请求超时，但在框架取消前请求完成了，则返回的状态码就不是STATUS_IO_TIMEOUT
	WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
		&sendOptions,
		DEFAULT_CONTROL_TRANSFER_TIMEOUT
	);

	// 初始化WDF_USB_CONTROL_SETUP_PACKET结构体，用于vendor自定义的控制传输
	WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(
		&controlSetupPacket,			// WDF_USB_CONTROL_SETUP_PACKET结构体
		BmRequestHostToDevice,			// 方向，主机到设备、设备到主机，填充Packet.bm.Request.Dir
		BmRequestToDevice,				// 接收者（设备、接口、端点或其他），填充Packet.bm.Request.Recipient
		KMDFUSB_REENUMERATE,			// 填充WDF_USB_CONTROL_SETUP_PACKET结构体的Packet.bRequest
		0,								// 填充WDF_USB_CONTROL_SETUP_PACKET结构体的Packet.wValue.Value
		0);								// 填充WDF_USB_CONTROL_SETUP_PACKET结构体的Packet.wIndex.Value


	status = WdfUsbTargetDeviceSendControlTransferSynchronously(
		DevContext->UsbDevice,
		WDF_NO_HANDLE, // Optional WDFREQUEST
		&sendOptions,
		&controlSetupPacket,
		NULL,		// MemoryDescriptor
		NULL);		// BytesTransferred

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL,
			"ReenumerateDevice: Failed to Reenumerate - 0x%x \n", status);
	}

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "<-- ReenumerateDevice\n");

	//
	// Send event to eventlog
	//

	activity = DeviceToActivityId(WdfObjectContextGetObject(DevContext));

	/*
		EventWriteDeviceReenumerated(&activity,
		DevContext->DeviceName,
		DevContext->Location,
		status);
	*/

	return status;

}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
GetBarGraphState(
	_In_ PDEVICE_CONTEXT DevContext,
	_Out_ PBAR_GRAPH_STATE BarGraphState
)
/*++

Routine Description

	This routine gets the state of the bar graph on the board

Arguments:

	DevContext - One of our device extensions

	BarGraphState - Struct that receives the bar graph's state

Return Value:

	NT status value

--*/
{
	NTSTATUS status;
	WDF_USB_CONTROL_SETUP_PACKET    controlSetupPacket;
	WDF_REQUEST_SEND_OPTIONS        sendOptions;
	WDF_MEMORY_DESCRIPTOR memDesc;
	ULONG    bytesTransferred;

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "--> GetBarGraphState\n");

	WDF_REQUEST_SEND_OPTIONS_INIT(
		&sendOptions,
		WDF_REQUEST_SEND_OPTION_TIMEOUT
	);

	WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
		&sendOptions,
		DEFAULT_CONTROL_TRANSFER_TIMEOUT
	);

	WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(&controlSetupPacket,
		BmRequestDeviceToHost,
		BmRequestToDevice,
		KMDFUSB_READ_BARGRAPH_DISPLAY, // Request
		0, // Value
		0); // Index

//
// Set the buffer to 0, the board will OR in everything that is set
//
	BarGraphState->BarsAsUChar = 0;


	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memDesc,
		BarGraphState,
		sizeof(BAR_GRAPH_STATE));

	status = WdfUsbTargetDeviceSendControlTransferSynchronously(
		DevContext->UsbDevice,
		WDF_NO_HANDLE, // Optional WDFREQUEST
		&sendOptions,
		&controlSetupPacket,
		&memDesc,
		&bytesTransferred);

	if (!NT_SUCCESS(status)) {

		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL,
			"GetBarGraphState: Failed to GetBarGraphState - 0x%x \n", status);

	}
	else {

		TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL,
			"GetBarGraphState: LED mask is 0x%x\n", BarGraphState->BarsAsUChar);
	}

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "<-- GetBarGraphState\n");

	return status;

}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
SetBarGraphState(
	_In_ PDEVICE_CONTEXT DevContext,
	_In_ PBAR_GRAPH_STATE BarGraphState
)
/*++

Routine Description

	This routine sets the state of the bar graph on the board

Arguments:

	DevContext - One of our device extensions

	BarGraphState - Struct that describes the bar graph's desired state

Return Value:

	NT status value

--*/
{
	NTSTATUS status;
	WDF_USB_CONTROL_SETUP_PACKET    controlSetupPacket;
	WDF_REQUEST_SEND_OPTIONS        sendOptions;
	WDF_MEMORY_DESCRIPTOR memDesc;
	ULONG    bytesTransferred;

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "--> SetBarGraphState\n");

	WDF_REQUEST_SEND_OPTIONS_INIT(
		&sendOptions,
		WDF_REQUEST_SEND_OPTION_TIMEOUT
	);

	WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
		&sendOptions,
		DEFAULT_CONTROL_TRANSFER_TIMEOUT
	);

	WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(&controlSetupPacket,
		BmRequestHostToDevice,
		BmRequestToDevice,
		KMDFUSB_SET_BARGRAPH_DISPLAY, // Request
		0, // Value
		0); // Index

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memDesc,
		BarGraphState,
		sizeof(BAR_GRAPH_STATE));

	status = WdfUsbTargetDeviceSendControlTransferSynchronously(
		DevContext->UsbDevice,
		NULL, // Optional WDFREQUEST
		&sendOptions,
		&controlSetupPacket,
		&memDesc,
		&bytesTransferred);

	if (!NT_SUCCESS(status)) {

		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL,
			"SetBarGraphState: Failed - 0x%x \n", status);

	}
	else {

		TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL,
			"SetBarGraphState: LED mask is 0x%x\n", BarGraphState->BarsAsUChar);
	}

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "<-- SetBarGraphState\n");

	return status;

}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
GetSevenSegmentState(
	_In_ PDEVICE_CONTEXT DevContext,
	_Out_ PUCHAR SevenSegment
)
/*++

Routine Description

	This routine gets the state of the 7 segment display on the board
	by sending a synchronous control command.

	NOTE: It's not a good practice to send a synchronous request in the
		  context of the user thread because if the transfer takes long
		  time to complete, you end up holding the user thread.

		  I'm choosing to do synchronous transfer because a) I know this one
		  completes immediately b) and for demonstration.

Arguments:

	DevContext - One of our device extensions

	SevenSegment - receives the state of the 7 segment display

Return Value:

	NT status value

--*/
{
	NTSTATUS status;
	WDF_USB_CONTROL_SETUP_PACKET    controlSetupPacket;
	WDF_REQUEST_SEND_OPTIONS        sendOptions;

	WDF_MEMORY_DESCRIPTOR memDesc;
	ULONG    bytesTransferred;

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "GetSetSevenSegmentState: Enter\n");

	PAGED_CODE();

	WDF_REQUEST_SEND_OPTIONS_INIT(
		&sendOptions,
		WDF_REQUEST_SEND_OPTION_TIMEOUT
	);

	WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
		&sendOptions,
		DEFAULT_CONTROL_TRANSFER_TIMEOUT
	);

	WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(&controlSetupPacket,
		BmRequestDeviceToHost,
		BmRequestToDevice,
		KMDFUSB_READ_7SEGMENT_DISPLAY, // Request
		0, // Value
		0); // Index

//
// Set the buffer to 0, the board will OR in everything that is set
//
	*SevenSegment = 0;

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memDesc,
		SevenSegment,
		sizeof(UCHAR));

	status = WdfUsbTargetDeviceSendControlTransferSynchronously(
		DevContext->UsbDevice,
		NULL, // Optional WDFREQUEST
		&sendOptions,
		&controlSetupPacket,
		&memDesc,
		&bytesTransferred);

	if (!NT_SUCCESS(status)) {

		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL,
			"GetSevenSegmentState: Failed to get 7 Segment state - 0x%x \n", status);
	}
	else {

		TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL,
			"GetSevenSegmentState: 7 Segment mask is 0x%x\n", *SevenSegment);
	}

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "GetSetSevenSegmentState: Exit\n");

	return status;

}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
SetSevenSegmentState(
	_In_ PDEVICE_CONTEXT DevContext,
	_In_ PUCHAR SevenSegment
)
/*++

Routine Description

	This routine sets the state of the 7 segment display on the board

Arguments:

	DevContext - One of our device extensions

	SevenSegment - desired state of the 7 segment display

Return Value:

	NT status value

--*/
{
	NTSTATUS status;
	WDF_USB_CONTROL_SETUP_PACKET    controlSetupPacket;
	WDF_REQUEST_SEND_OPTIONS        sendOptions;
	WDF_MEMORY_DESCRIPTOR memDesc;
	ULONG    bytesTransferred;

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "--> SetSevenSegmentState\n");

	WDF_REQUEST_SEND_OPTIONS_INIT(
		&sendOptions,
		WDF_REQUEST_SEND_OPTION_TIMEOUT
	);

	WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
		&sendOptions,
		DEFAULT_CONTROL_TRANSFER_TIMEOUT
	);

	WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(&controlSetupPacket,
		BmRequestHostToDevice,
		BmRequestToDevice,
		KMDFUSB_SET_7SEGMENT_DISPLAY, // Request
		0, // Value
		0); // Index

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memDesc,
		SevenSegment,
		sizeof(UCHAR));

	status = WdfUsbTargetDeviceSendControlTransferSynchronously(
		DevContext->UsbDevice,
		NULL, // Optional WDFREQUEST
		&sendOptions,
		&controlSetupPacket,
		&memDesc,
		&bytesTransferred);

	if (!NT_SUCCESS(status)) {

		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL,
			"SetSevenSegmentState: Failed to set 7 Segment state - 0x%x \n", status);

	}
	else {

		TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL,
			"SetSevenSegmentState: 7 Segment mask is 0x%x\n", *SevenSegment);

	}

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "<-- SetSevenSegmentState\n");

	return status;

}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
GetSwitchState(
	_In_ PDEVICE_CONTEXT DevContext,
	_In_ PSWITCH_STATE SwitchState
)
/*++

Routine Description

	This routine gets the state of the switches on the board

Arguments:

	DevContext - One of our device extensions

Return Value:

	NT status value

--*/
{
	NTSTATUS status;
	WDF_USB_CONTROL_SETUP_PACKET    controlSetupPacket;
	WDF_REQUEST_SEND_OPTIONS        sendOptions;
	WDF_MEMORY_DESCRIPTOR memDesc;
	ULONG    bytesTransferred;

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "--> GetSwitchState\n");

	PAGED_CODE();

	WDF_REQUEST_SEND_OPTIONS_INIT(
		&sendOptions,
		WDF_REQUEST_SEND_OPTION_TIMEOUT
	);

	WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(
		&sendOptions,
		DEFAULT_CONTROL_TRANSFER_TIMEOUT
	);

	WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(&controlSetupPacket,
		BmRequestDeviceToHost,
		BmRequestToDevice,
		KMDFUSB_READ_SWITCHES, // Request
		0, // Value
		0); // Index

	SwitchState->SwitchesAsUChar = 0;

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memDesc,
		SwitchState,
		sizeof(SWITCH_STATE));

	status = WdfUsbTargetDeviceSendControlTransferSynchronously(
		DevContext->UsbDevice,
		NULL, // Optional WDFREQUEST
		&sendOptions,
		&controlSetupPacket,
		&memDesc,
		&bytesTransferred);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL,
			"GetSwitchState: Failed to Get switches - 0x%x \n", status);

	}
	else {
		TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL,
			"GetSwitchState: Switch mask is 0x%x\n", SwitchState->SwitchesAsUChar);
	}

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "<-- GetSwitchState\n");

	return status;

}

VOID
KmdfUsbIoctlGetInterruptMessage(
	_In_ WDFDEVICE Device,
	_In_ NTSTATUS  ReaderStatus
)
/*++

Routine Description

	This method handles the completion of the pended request for the IOCTL
	IOCTL_KMDFUSB_GET_INTERRUPT_MESSAGE.

Arguments:

	Device - Handle to a framework device.

Return Value:

	None.

--*/
{
	NTSTATUS            status;
	WDFREQUEST          request;
	PDEVICE_CONTEXT     pDevContext;
	size_t              bytesReturned = 0;
	PSWITCH_STATE       switchState = NULL;

	pDevContext = GetDeviceContext(Device);

	do {
		// 从指定队列获取下一个待处理的IO请求
		status = WdfIoQueueRetrieveNextRequest(pDevContext->InterruptMsgQueue, &request);

		if (NT_SUCCESS(status)) {
			// 获取当前request的输出缓冲区到switchState
			status = WdfRequestRetrieveOutputBuffer(request,
				sizeof(SWITCH_STATE),
				&switchState,
				NULL);	// BufferLength

			if (!NT_SUCCESS(status)) {
				TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "User's output buffer is too small for this IOCTL, expecting a SWITCH_STATE\n");
				bytesReturned = sizeof(SWITCH_STATE);
			}
			else {
				// Copy the state information saved by the continuous reader.
				if (NT_SUCCESS(ReaderStatus)) {
					switchState->SwitchesAsUChar = pDevContext->CurrentSwitchState;
					bytesReturned = sizeof(SWITCH_STATE);
				}
				else {
					bytesReturned = 0;
				}
			}

			// Complete the request.  If we failed to get the output buffer then
			// complete with that status.  Otherwise complete with the status from the reader.
			WdfRequestCompleteWithInformation(request, NT_SUCCESS(status) ? ReaderStatus : status, bytesReturned);
			status = STATUS_SUCCESS;

		}
		else if (status != STATUS_NO_MORE_ENTRIES) {
			KdPrint(("WdfIoQueueRetrieveNextRequest status %08x\n", status));
		}

		request = NULL;

	} while (status == STATUS_SUCCESS);

	return;

}
