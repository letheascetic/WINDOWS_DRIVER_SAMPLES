
#include "driver.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, EchoEvtDeviceAdd)
#endif

// DriverEntry ��������ں���
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


// EvtDeviceAdd�ص��������豸�����ִ�еĵ�һ���ص�����
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

	// ʵ�ʵĹ�����EchoDeviceCreate�����
	status = EchoDeviceCreate(DeviceInit);

	return status;
}