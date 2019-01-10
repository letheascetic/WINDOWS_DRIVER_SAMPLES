
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

	// 初始化contReaderConfig结构体
	// KmdfUsbEvtUsbInterruptPipeReadComplete：EvtUsbTargetPipeReadComplete回调函数
	// Context：EvtUsbTargetPipeReadComplete回调函数中的参数
	// TransferLength：从设备可以获取数据的最大字节长度
	WDF_USB_CONTINUOUS_READER_CONFIG_INIT(&contReaderConfig,
		KmdfUsbEvtUsbInterruptPipeReadComplete,
		DeviceContext,    // Context
		sizeof(UCHAR));   // TransferLength

	// 设置EvtUsbTargetPipeReadersFailed回调函数
	contReaderConfig.EvtUsbTargetPipeReadersFailed = KmdfUsbEvtUsbInterruptReadersFailed;

	// Reader requests are not posted to the target automatically.
	// Driver must explictly call WdfIoTargetStart to kick start the
	// reader.  In this sample, it's done in D0Entry.
	// By defaut, framework queues two requests to the target
	// endpoint. Driver can configure up to 10 requests with CONFIG macro.

	// 配置从DeviceContext->InterruptPipe连续读数据
	// 配置的pipe可以是bulk pipe或interrupt pipe，且pipe必须连接输入端点
	// 配置完后，调用WdfIoTargetStart/WdfIoTargetStop来启动/停止
	// 如果在EvtDevicePrepareHardware回调函数中中配置连续读，则应该在EvtDeviceD0Entry回调中调用WdfIoTargetStart启动，并在EvtDeviceD0Exit回调中调用WdfIoTargetStop停止
	// 每次成功完成一个IO读请求，则调用EvtUsbTargetPipeReadComplete回调函数
	// 当处理其中一个IO读请求失败时，EvtUsbTargetPipeReadersFailed回调函数会在所有IO读请求完成后被调用
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

	Buffer - WDFMEMORY句柄，指向包含数据的缓冲区
	NumBytesTransferred - 缓冲区中的数据长度（字节数）
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

	// Make sure that there is data in the read packet.  Depending on the device
	// specification, it is possible for it to return a 0 length read in
	// certain conditions.
	if (NumBytesTransferred == 0) {
		TraceEvents(TRACE_LEVEL_WARNING, DBG_INIT, "KmdfUsbEvtUsbInterruptPipeReadComplete Zero length read occured on the Interrupt Pipe's Continuous Reader\n");
		return;
	}

	NT_ASSERT(NumBytesTransferred == sizeof(UCHAR));

	// Buffer是WDFMEMORY句柄，指向包含数据的缓冲区
	// WdfMemoryGetBuffer返回该WDFMEMORY句柄的指针
	switchState = WdfMemoryGetBuffer(Buffer, NULL);

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT, "KmdfUsbEvtUsbInterruptPipeReadComplete SwitchState %x\n", *switchState);

	pDeviceContext->CurrentSwitchState = *switchState;

	// Handle any pending Interrupt Message IOCTLs. Note that the OSR USB device
	// will generate an interrupt message when the the device resumes from a low
	// power state. So if the Interrupt Message IOCTL was sent after the device
	// has gone to a low power state, the pending Interrupt Message IOCTL will
	// get completed in the function call below, before the user twiddles the
	// dip switches on the OSR USB device. If this is not the desired behavior
	// for your driver, then you could handle this condition by maintaining a
	// state variable on D0Entry to track interrupt messages caused by power up.

	KmdfUsbIoctlGetInterruptMessage(device, STATUS_SUCCESS);

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

	// Clear the current switch state.
	pDeviceContext->CurrentSwitchState = 0;

	// Service the pending interrupt switch change request
	KmdfUsbIoctlGetInterruptMessage(device, Status);

	// EvtUsbTargetPipeReadersFailed回调函数返回值：
	// TRUE：框架会重置对应的USB管道，并重新启动连续读
	// FALSE：框架不会重置和重启
	return TRUE;
}