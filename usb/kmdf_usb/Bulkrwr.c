
#include "private.h"
#include "bulkrwr.tmh"

#pragma warning(disable:4267)

VOID
KmdfUsbEvtIoRead(
	_In_ WDFQUEUE         Queue,
	_In_ WDFREQUEST       Request,
	_In_ size_t           Length
)
/*++

Routine Description:

	Called by the framework when it receives Read or Write requests.

Arguments:

	Queue - Default queue handle
	Request - Handle to the read/write request
	Lenght - Length of the data buffer associated with the request.
				 The default property of the queue is to not dispatch
				 zero lenght read & write requests to the driver and
				 complete is with status success. So we will never get
				 a zero length request.

Return Value:


--*/
{
	WDFUSBPIPE                  pipe;
	NTSTATUS                    status;
	WDFMEMORY                   reqMemory;
	PDEVICE_CONTEXT             pDeviceContext;
	GUID                        activity;

	UNREFERENCED_PARAMETER(Queue);

	// 
	// Log read start event, using IRP activity ID if available or request
	// handle otherwise.
	//
	activity = RequestToActivityId(Request);
	// EventWriteReadStart(&activity, WdfIoQueueGetDevice(Queue), (ULONG)Length);

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_READ, "-->OsrFxEvtIoRead\n");

	//
	// First validate input parameters.
	//
	if (Length > TEST_BOARD_TRANSFER_BUFFER_SIZE) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_READ, "Transfer exceeds %d\n",
			TEST_BOARD_TRANSFER_BUFFER_SIZE);
		status = STATUS_INVALID_PARAMETER;
		goto Exit;
	}

	pDeviceContext = GetDeviceContext(WdfIoQueueGetDevice(Queue));

	pipe = pDeviceContext->BulkReadPipe;

	status = WdfRequestRetrieveOutputMemory(Request, &reqMemory);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_READ,
			"WdfRequestRetrieveOutputMemory failed %!STATUS!\n", status);
		goto Exit;
	}

	//
	// The format call validates to make sure that you are reading or
	// writing to the right pipe type, sets the appropriate transfer flags,
	// creates an URB and initializes the request.
	//
	status = WdfUsbTargetPipeFormatRequestForRead(pipe,
		Request,
		reqMemory,
		NULL // Offsets
	);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_READ,
			"WdfUsbTargetPipeFormatRequestForRead failed 0x%x\n", status);
		goto Exit;
	}

	WdfRequestSetCompletionRoutine(
		Request,
		EvtRequestReadCompletionRoutine,
		pipe);
	//
	// Send the request asynchronously.
	//
	if (WdfRequestSend(Request, WdfUsbTargetPipeGetIoTarget(pipe), WDF_NO_SEND_OPTIONS) == FALSE) {
		//
		// Framework couldn't send the request for some reason.
		//
		TraceEvents(TRACE_LEVEL_ERROR, DBG_READ, "WdfRequestSend failed\n");
		status = WdfRequestGetStatus(Request);
		goto Exit;
	}


Exit:
	if (!NT_SUCCESS(status)) {
		//
		// log event read failed
		//
		// EventWriteReadFail(&activity, WdfIoQueueGetDevice(Queue), status);
		WdfRequestCompleteWithInformation(Request, status, 0);
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_READ, "<-- OsrFxEvtIoRead\n");

	return;
}


VOID
EvtRequestReadCompletionRoutine(
	_In_ WDFREQUEST                  Request,
	_In_ WDFIOTARGET                 Target,
	_In_ PWDF_REQUEST_COMPLETION_PARAMS CompletionParams,
	_In_ WDFCONTEXT                  Context
)
/*++

Routine Description:

	This is the completion routine for reads
	If the irp completes with success, we check if we
	need to recirculate this irp for another stage of
	transfer.

Arguments:

	Context - Driver supplied context
	Device - Device handle
	Request - Request handle
	Params - request completion params

Return Value:
	None

--*/
{
	NTSTATUS    status;
	size_t      bytesRead = 0;
	GUID        activity;
	PWDF_USB_REQUEST_COMPLETION_PARAMS usbCompletionParams;

	UNREFERENCED_PARAMETER(Target);
	UNREFERENCED_PARAMETER(Context);

	status = CompletionParams->IoStatus.Status;

	usbCompletionParams = CompletionParams->Parameters.Usb.Completion;

	bytesRead = usbCompletionParams->Parameters.PipeRead.Length;

	if (NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_READ, "Number of bytes read: %I64d\n", (INT64)bytesRead);
	}
	else {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_READ, "Read failed - request status 0x%x UsbdStatus 0x%x\n", status, usbCompletionParams->UsbdStatus);
	}

	//
	// Log read stop event, using IRP activity ID if available or request
	// handle otherwise.
	//

	activity = RequestToActivityId(Request);
	/*
	EventWriteReadStop(&activity,
		WdfIoQueueGetDevice(WdfRequestGetIoQueue(Request)),
		bytesRead,
		status,
		usbCompletionParams->UsbdStatus);
	*/

	WdfRequestCompleteWithInformation(Request, status, bytesRead);

	return;
}


VOID
KmdfUsbEvtIoStop(
	_In_ WDFQUEUE         Queue,
	_In_ WDFREQUEST       Request,
	_In_ ULONG            ActionFlags
)
/*++

Routine Description:

	This callback is invoked on every inflight request when the device
	is suspended or removed. Since our inflight read and write requests
	are actually pending in the target device, we will just acknowledge
	its presence. Until we acknowledge, complete, or requeue the requests
	framework will wait before allowing the device suspend or remove to
	proceeed. When the underlying USB stack gets the request to suspend or
	remove, it will fail all the pending requests.

Arguments:

	Queue - handle to queue object that is associated with the I/O request

	Request - handle to a request object

	ActionFlags - bitwise OR of one or more WDF_REQUEST_STOP_ACTION_FLAGS flags

Return Value:
	None

--*/
{
	UNREFERENCED_PARAMETER(Queue);
	UNREFERENCED_PARAMETER(ActionFlags);

	if (ActionFlags &  WdfRequestStopActionSuspend) {
		WdfRequestStopAcknowledge(Request, FALSE); // Don't requeue
	}
	else if (ActionFlags &  WdfRequestStopActionPurge) {
		WdfRequestCancelSentRequest(Request);
	}
	return;
}