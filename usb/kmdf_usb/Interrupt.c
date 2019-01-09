
#include "private.h"
#include "interrupt.tmh"


_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
KmdfUsbConfigContReaderForInterruptEndPoint(
	_In_ PDEVICE_CONTEXT DeviceContext
)
/*++

Routine Description:

	This routine configures a continuous reader on the
	interrupt endpoint. It's called from the PrepareHarware event.

Arguments:


Return Value:

	NT status value

--*/
{
	WDF_USB_CONTINUOUS_READER_CONFIG contReaderConfig;
	NTSTATUS status;

	WDF_USB_CONTINUOUS_READER_CONFIG_INIT(&contReaderConfig,
		KmdfUsbEvtUsbInterruptPipeReadComplete,
		DeviceContext,    // Context
		sizeof(UCHAR));   // TransferLength

	contReaderConfig.EvtUsbTargetPipeReadersFailed = KmdfUsbEvtUsbInterruptReadersFailed;

	// Reader requests are not posted to the target automatically.
	// Driver must explictly call WdfIoTargetStart to kick start the
	// reader.  In this sample, it's done in D0Entry.
	// By defaut, framework queues two requests to the target
	// endpoint. Driver can configure up to 10 requests with CONFIG macro.
	status = WdfUsbTargetPipeConfigContinuousReader(DeviceContext->InterruptPipe, &contReaderConfig);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "KmdfUsbConfigContReaderForInterruptEndPoint failed %x\n", status);
		return status;
	}

	return status;
}


VOID
KmdfUsbEvtUsbInterruptPipeReadComplete(
	WDFUSBPIPE  Pipe,
	WDFMEMORY   Buffer,
	size_t      NumBytesTransferred,
	WDFCONTEXT  Context
)
/*++

Routine Description:

	This the completion routine of the continour reader. This can
	called concurrently on multiprocessor system if there are
	more than one readers configured. So make sure to protect
	access to global resources.

Arguments:

	Buffer - This buffer is freed when this call returns.
			 If the driver wants to delay processing of the buffer, it
			 can take an additional referrence.

	Context - Provided in the WDF_USB_CONTINUOUS_READER_CONFIG_INIT macro

Return Value:

	NT status value

--*/
{
	PUCHAR          switchState = NULL;
	WDFDEVICE       device;
	PDEVICE_CONTEXT pDeviceContext = Context;

	UNREFERENCED_PARAMETER(Pipe);

	device = WdfObjectContextGetObject(pDeviceContext);

	//
	// Make sure that there is data in the read packet.  Depending on the device
	// specification, it is possible for it to return a 0 length read in
	// certain conditions.
	//

	if (NumBytesTransferred == 0) {
		TraceEvents(TRACE_LEVEL_WARNING, DBG_INIT,
			"KmdfUsbEvtUsbInterruptPipeReadComplete Zero length read "
			"occured on the Interrupt Pipe's Continuous Reader\n"
		);
		return;
	}


	NT_ASSERT(NumBytesTransferred == sizeof(UCHAR));

	switchState = WdfMemoryGetBuffer(Buffer, NULL);

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT,
		"KmdfUsbEvtUsbInterruptPipeReadComplete SwitchState %x\n",
		*switchState);

	pDeviceContext->CurrentSwitchState = *switchState;

	//
	// Handle any pending Interrupt Message IOCTLs. Note that the OSR USB device
	// will generate an interrupt message when the the device resumes from a low
	// power state. So if the Interrupt Message IOCTL was sent after the device
	// has gone to a low power state, the pending Interrupt Message IOCTL will
	// get completed in the function call below, before the user twiddles the
	// dip switches on the OSR USB device. If this is not the desired behavior
	// for your driver, then you could handle this condition by maintaining a
	// state variable on D0Entry to track interrupt messages caused by power up.
	//
	// OsrUsbIoctlGetInterruptMessage(device, STATUS_SUCCESS);

}


BOOLEAN
KmdfUsbEvtUsbInterruptReadersFailed(
	_In_ WDFUSBPIPE Pipe,
	_In_ NTSTATUS Status,
	_In_ USBD_STATUS UsbdStatus
)
{
	WDFDEVICE device = WdfIoTargetGetDevice(WdfUsbTargetPipeGetIoTarget(Pipe));
	PDEVICE_CONTEXT pDeviceContext = GetDeviceContext(device);

	UNREFERENCED_PARAMETER(UsbdStatus);

	//
	// Clear the current switch state.
	//
	pDeviceContext->CurrentSwitchState = 0;

	//
	// Service the pending interrupt switch change request
	//
	// OsrUsbIoctlGetInterruptMessage(device, Status);

	return TRUE;
}