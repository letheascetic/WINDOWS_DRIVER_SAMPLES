
#include "driver.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, EchoEvtDeviceAdd)
#endif

// DriverEntry 驱动的入口函数
NTSTATUS 
DriverEntry(
	IN PDRIVER_OBJECT  DriverObject, 
	IN PUNICODE_STRING RegistryPath
	)
{
	WDF_DRIVER_CONFIG config;
	NTSTATUS status;

	WDF_DRIVER_CONFIG_INIT(
		&config, 
		EchoEvtDeviceAdd);

	status = WdfDriverCreate(
		DriverObject, 
		RegistryPath, 
		WDF_NO_OBJECT_ATTRIBUTES,
		&config,
		WDF_NO_HANDLE);

	if (!NT_SUCCESS(status)) {
		KdPrint(("Error: WdfDriverCreate failed 0x%x\n", status));
		return status;
	}

	return status;

}


// EvtDeviceAdd回调函数，设备插入后执行的第一个回调函数
NTSTATUS
EchoEvtDeviceAdd(
	IN WDFDRIVER       Driver,
	IN PWDFDEVICE_INIT DeviceInit
)
{
	NTSTATUS status;

	UNREFERENCED_PARAMETER(Driver);

	PAGED_CODE();

	KdPrint(("Enter  EchoEvtDeviceAdd\n"));

	// 实际的工作在EchoDeviceCreate中完成
	status = EchoDeviceCreate(DeviceInit);

	return status;
}