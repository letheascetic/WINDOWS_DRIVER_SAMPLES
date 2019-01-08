/*++

Module Name:

    driver.c

Abstract:

    This file contains the driver entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "private.h"
#include "driver.tmh"

PFN_IO_GET_ACTIVITY_ID_IRP g_pIoGetActivityIdIrp;
PFN_IO_SET_DEVICE_INTERFACE_PROPERTY_DATA g_pIoSetDeviceInterfacePropertyData;

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, KmdfUsbEvtDriverContextCleanup)
#endif


NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:
    DriverEntry initializes the driver and is the first routine called by the
    system after the driver is loaded. DriverEntry specifies the other entry
    points in the function driver, such as EvtDevice and DriverUnload.

Parameters Description:

    DriverObject - represents the instance of the function driver that is loaded
    into memory. DriverEntry must initialize members of DriverObject before it
    returns to the caller. DriverObject is allocated by the system before the
    driver is loaded, and it is released by the system after the system unloads
    the function driver from memory.

    RegistryPath - represents the driver specific path in the Registry.
    The function driver can use the path to store driver related data between
    reboots. The path does not store hardware instance specific data.

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise.

--*/
{
    WDF_DRIVER_CONFIG		config;
    NTSTATUS				status;
    WDF_OBJECT_ATTRIBUTES	attributes;
	UNICODE_STRING          funcName;

    //
    // Initialize WPP Tracing
    //
    WPP_INIT_TRACING( DriverObject, RegistryPath );

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT, "KMDF USB Driver Sample - Driver Framework Edition.\n");

	//
	// IRP activity ID functions are available on some versions, save them into
	// globals (or NULL if not available)
	//
	RtlInitUnicodeString(&funcName, L"IoGetActivityIdIrp");
	g_pIoGetActivityIdIrp = (PFN_IO_GET_ACTIVITY_ID_IRP)(ULONG_PTR)
		MmGetSystemRoutineAddress(&funcName);

	//
	// The Device interface property set is available on some version, save it
	// into globals (or NULL if not available)
	//
	RtlInitUnicodeString(&funcName, L"IoSetDeviceInterfacePropertyData");
	g_pIoSetDeviceInterfacePropertyData = (PFN_IO_SET_DEVICE_INTERFACE_PROPERTY_DATA)(ULONG_PTR)
		MmGetSystemRoutineAddress(&funcName);

    //
    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = KmdfUsbEvtDriverContextCleanup;

    WDF_DRIVER_CONFIG_INIT(&config,
                           KmdfUsbEvtDeviceAdd
                           );

    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             &attributes,
                             &config,
                             WDF_NO_HANDLE
                             );

    if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_INIT, "WdfDriverCreate failed with status 0x%x\n", status);
		//
		// Cleanup tracing here because DriverContextCleanup will not be called
		// as we have failed to create WDFDRIVER object itself.
		// Please note that if your return failure from DriverEntry after the
		// WDFDRIVER object is created successfully, you don't have to
		// call WPP cleanup because in those cases DriverContextCleanup
		// will be executed when the framework deletes the DriverObject.
		//
        WPP_CLEANUP(DriverObject);
        return status;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT, "%!FUNC! Exit");

    return status;
}


VOID
KmdfUsbEvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
    )
/*++
Routine Description:

    Free all the resources allocated in DriverEntry.

Arguments:

    DriverObject - handle to a WDF Driver object.

Return Value:

    VOID.

--*/
{
	//
	// EvtCleanupCallback for WDFDRIVER is always called at PASSIVE_LEVEL
	//
	_IRQL_limited_to_(PASSIVE_LEVEL);

    PAGED_CODE ();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT, "--> KmdfUsbEvtDriverContextCleanup\n");

    //
    // Stop WPP Tracing
    //
    WPP_CLEANUP( WdfDriverWdmGetDriverObject( (WDFDRIVER) DriverObject) );

	UNREFERENCED_PARAMETER(DriverObject);

}
