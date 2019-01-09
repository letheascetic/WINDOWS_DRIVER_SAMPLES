/*++

Module Name:

    device.c - Device handling events for example driver.

Abstract:

   This file contains the device entry points and callbacks.
    
Environment:

    Kernel-mode Driver Framework

--*/

#include "private.h"
#include "device.tmh"
#include <devpkey.h>

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, KmdfUsbEvtDeviceAdd)
#pragma alloc_text (PAGE, KmdfUsbEvtDevicePrepareHardware)
#pragma alloc_text(PAGE, KmdfUsbEvtDeviceD0Exit)
#pragma alloc_text(PAGE, KmdfUsbSetPowerPolicy)
#pragma alloc_text (PAGE, SelectInterfaces)
#pragma alloc_text (PAGE, GetDeviceEventLoggingNames)
#endif


NTSTATUS
KmdfUsbEvtDeviceAdd(
	_In_    WDFDRIVER       Driver,
	_Inout_ PWDFDEVICE_INIT DeviceInit
)
/*++
Routine Description:

	EvtDeviceAdd is called by the framework in response to AddDevice
	call from the PnP manager. We create and initialize a device object to
	represent a new instance of the device.

Arguments:

	Driver - Handle to a framework driver object created in DriverEntry

	DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

	NTSTATUS

--*/
{
	WDF_PNPPOWER_EVENT_CALLBACKS        pnpPowerCallbacks;
	WDF_OBJECT_ATTRIBUTES               attributes;
	NTSTATUS                            status;
	WDFDEVICE                           device;
	WDF_DEVICE_PNP_CAPABILITIES         pnpCaps;
	WDF_IO_QUEUE_CONFIG                 ioQueueConfig;
	PDEVICE_CONTEXT                     pDevContext;
	WDFQUEUE                            queue;
	GUID                                activity;
	UNICODE_STRING                      symbolicLinkName;
	WDFSTRING                           symbolicLinkString = NULL;
	DEVPROP_BOOLEAN                     isRestricted;

	UNREFERENCED_PARAMETER(Driver);

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> KmdfUsbEvtDeviceAdd routine\n");

	// ��ʼ��pnpPowerCallbacks�ṹ��
	// PNP�͵�Դ�ص�����ͨ���ýṹ������
	// ��������ã���ܻ�ʹ��Ĭ�ϵĻص�������Ĭ�ϵĻص���������DeviceInit����ʼ��ΪFDO��PDO��Filter����ͬ
	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);

	// ����USB�豸����Ҫ��EvtDevicePrepareHardware�ص�������ѡ��ӿڡ������豸
	pnpPowerCallbacks.EvtDevicePrepareHardware = KmdfUsbEvtDevicePrepareHardware;

	// These two callbacks start and stop the wdfusb pipe continuous reader as we go in and out of the D0-working state.
	pnpPowerCallbacks.EvtDeviceD0Entry = KmdfUsbEvtDeviceD0Entry;
	pnpPowerCallbacks.EvtDeviceD0Exit = KmdfUsbEvtDeviceD0Exit;
	pnpPowerCallbacks.EvtDeviceSelfManagedIoFlush = KmdfUsbEvtDeviceSelfManagedIoFlush;

	WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

	// ������������I/O��д��������ݻ������ķ�ʽ��Ĭ��ΪBuffered��ʽ
	WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoBuffered);

	// ��ʼ���豸��������Ժͻ�������
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);

	status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfDeviceCreate failed with Status code %!STATUS!\n", status);
		return status;
	}

	// Setup the activity ID so that we can log events using it.
	activity = DeviceToActivityId(device);

	// ��ȡ�豸����Ļ�������
	pDevContext = GetDeviceContext(device);

	// Get the device's friendly name and location so that we can use it in
	// error logging.  If this fails then it will setup dummy strings.
	GetDeviceEventLoggingNames(device);

	// ��ʼ��PNP���ԡ������豸�쳣�γ���ϵͳ���ᵯ�������
	WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
	pnpCaps.SurpriseRemovalOK = WdfTrue;
	WdfDeviceSetPnpCapabilities(device, &pnpCaps);


	// �������У����ĸ�����
	// a) Ĭ�ϲ��ж��У��ַ�IO����
	// b) ��Ĭ�ϴ��ж��У�����IO������
	// c) ��Ĭ�ϴ��ж��У�����IOд����
	// d) ��Ĭ���ֹ����У������ж���Ϣ��������Ҫ�ȴ��жϵĳ��ֲ�����ɣ�

	// Create a parallel default queue and register an event callback to
	// receive ioctl requests. We will create separate queues for
	// handling read and write requests. All other requests will be
	// completed with error status automatically by the framework.
	// ����Ĭ�϶��У�����ģʽ��ע�ᴦ��ioctl����Ļص�����
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig, WdfIoQueueDispatchParallel);
	ioQueueConfig.EvtIoDeviceControl = KmdfUsbEvtIoDeviceControl;

	// By default, Static Driver Verifier (SDV) displays a warning if it
	// doesn't find the EvtIoStop callback on a power-managed queue.
	// The 'assume' below causes SDV to suppress this warning. If the driver
	// has not explicitly set PowerManaged to WdfFalse, the framework creates
	// power-managed queues when the device is not a filter driver.  Normally
	// the EvtIoStop is required for power-managed queues, but for this driver
	// it is not needed b/c the driver doesn't hold on to the requests for
	// long time or forward them to other drivers.
	// If the EvtIoStop callback is not implemented, the framework waits for
	// all driver-owned requests to be done before moving in the Dx/sleep
	// states or before removing the device, which is the correct behavior
	// for this type of driver. If the requests were taking an indeterminate
	// amount of time to complete, or if the driver forwarded the requests
	// to a lower driver/another stack, the queue should have an
	// EvtIoStop/EvtIoResume.
	__analysis_assume(ioQueueConfig.EvtIoStop != 0);
	status = WdfIoQueueCreate(device,
		&ioQueueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&queue);// pointer to default queue
	__analysis_assume(ioQueueConfig.EvtIoStop == 0);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfIoQueueCreate failed  %!STATUS!\n", status);
		goto Error;
	}

	//
	// We will create a separate sequential queue and configure it
	// to receive read requests.  We also need to register a EvtIoStop
	// handler so that we can acknowledge requests that are pending
	// at the target driver.
	// ������Ĭ�϶��У����д���ע�ᴦ��read����Ļص�������ע��EvtIoStop�ص�����
	WDF_IO_QUEUE_CONFIG_INIT(&ioQueueConfig, WdfIoQueueDispatchSequential);

	// ioQueueConfig.EvtIoRead = OsrFxEvtIoRead;
	// ioQueueConfig.EvtIoStop = OsrFxEvtIoStop;

	status = WdfIoQueueCreate(
		device,
		&ioQueueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&queue // queue handle
	);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfIoQueueCreate failed 0x%x\n", status);
		goto Error;
	}

	status = WdfDeviceConfigureRequestDispatching(
		device,
		queue,
		WdfRequestTypeRead);

	if (!NT_SUCCESS(status)) {
		NT_ASSERT(NT_SUCCESS(status));
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfDeviceConfigureRequestDispatching failed 0x%x\n", status);
		goto Error;
	}

	// ������Ĭ�϶��У����д�������write����
	WDF_IO_QUEUE_CONFIG_INIT(&ioQueueConfig, WdfIoQueueDispatchSequential);

	// ioQueueConfig.EvtIoWrite = OsrFxEvtIoWrite;
	// ioQueueConfig.EvtIoStop = OsrFxEvtIoStop;

	status = WdfIoQueueCreate(
		device,
		&ioQueueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&queue // queue handle
	);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfIoQueueCreate failed 0x%x\n", status);
		goto Error;
	}

	status = WdfDeviceConfigureRequestDispatching(
		device,
		queue,
		WdfRequestTypeWrite);

	if (!NT_SUCCESS(status)) {
		NT_ASSERT(NT_SUCCESS(status));
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfDeviceConfigureRequestDispatching failed 0x%x\n", status);
		goto Error;
	}

	//
	// Register a manual I/O queue for handling Interrupt Message Read Requests.
	// This queue will be used for storing Requests that need to wait for an
	// interrupt to occur before they can be completed.
	WDF_IO_QUEUE_CONFIG_INIT(&ioQueueConfig, WdfIoQueueDispatchManual);

	// This queue is used for requests that dont directly access the device. The
	// requests in this queue are serviced only when the device is in a fully
	// powered state and sends an interrupt. So we can use a non-power managed
	// queue to park the requests since we dont care whether the device is idle
	// or fully powered up.
	ioQueueConfig.PowerManaged = WdfFalse;

	status = WdfIoQueueCreate(device,
		&ioQueueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&pDevContext->InterruptMsgQueue
	);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfIoQueueCreate failed 0x%x\n", status);
		goto Error;
	}

	// ע���豸�ӿ���
	status = WdfDeviceCreateDeviceInterface(device,
		(LPGUID)&GUID_DEVINTERFACE_KMDFUSB,
		NULL); // Reference String

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfDeviceCreateDeviceInterface failed  %!STATUS!\n", status);
		goto Error;
	}

	// ����wait-lock������ͬ��ResetDevice�Ĵ���
	// ��Ҫͬ������ʱ������WdfWaitLockAcquire��WdfWaitLockRelease����ȡ���ͷ���
	// ��device��Ϊwait-lock����ĸ��������豸���Ƴ���device��ɾ����ʱ�����Ҳ���Զ�ɾ��wait-lock����
	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = device;

	status = WdfWaitLockCreate(&attributes, &pDevContext->ResetDeviceWaitLock);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfWaitLockCreate failed  %!STATUS!\n", status);
		goto Error;
	}

	// Get the string for the device interface and set the restricted
	// property on it to allow applications bound with device metadata
	// to access the interface.
	if (g_pIoSetDeviceInterfacePropertyData != NULL) {

		status = WdfStringCreate(NULL, WDF_NO_OBJECT_ATTRIBUTES, &symbolicLinkString);

		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfStringCreate failed  %!STATUS!\n", status);
			goto Error;
		}

		// ��ȡ�豸�ӿ�����Ӧ��WDFSTRING����
		status = WdfDeviceRetrieveDeviceInterfaceString(device,
			(LPGUID)&GUID_DEVINTERFACE_KMDFUSB,
			NULL,
			symbolicLinkString);

		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfDeviceRetrieveDeviceInterfaceString failed  %!STATUS!\n", status);
			goto Error;
		}

		// ����WDFSTRING�����Ӧ��UNICOND_STRING����
		WdfStringGetUnicodeString(symbolicLinkString, &symbolicLinkName);

		isRestricted = DEVPROP_TRUE;

		status = g_pIoSetDeviceInterfacePropertyData(&symbolicLinkName,
			&DEVPKEY_DeviceInterface_Restricted,
			0,
			0,
			DEVPROP_TYPE_BOOLEAN,
			sizeof(isRestricted),
			&isRestricted);

		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "IoSetDeviceInterfacePropertyData failed to set restricted property  %!STATUS!\n", status);
			goto Error;
		}

		WdfObjectDelete(symbolicLinkString);
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- KmdfUsbEvtDeviceAdd\n");

	return status;

Error:

	if (symbolicLinkString != NULL) {
		WdfObjectDelete(symbolicLinkString);
	}

	// Log fail to add device to the event log
	/*
		EventWriteFailAddDevice(&activity,
		pDevContext->DeviceName,
		pDevContext->Location,
		status);
	*/
	return status;

}

NTSTATUS
KmdfUsbEvtDevicePrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourceList,
    _In_ WDFCMRESLIST ResourceListTranslated
    )
/*++

Routine Description:

    In this callback, the driver does whatever is necessary to make the
    hardware ready to use.  In the case of a USB device, this involves
    reading and selecting descriptors.

Arguments:

	Device - handle to a device

	ResourceList - handle to a resource-list object that identifies the
				   raw hardware resources that the PnP manager assigned
				   to the device

	ResourceListTranslated - handle to a resource-list object that
							 identifies the translated hardware resources
							 that the PnP manager assigned to the device

Return Value:

    NT status value

--*/
{
	NTSTATUS                            status;
	PDEVICE_CONTEXT                     pDeviceContext;
	WDF_USB_DEVICE_INFORMATION          deviceInfo;
	ULONG                               waitWakeEnable;

    UNREFERENCED_PARAMETER(ResourceList);
    UNREFERENCED_PARAMETER(ResourceListTranslated);

    PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> KmdfUsbEvtDevicePrepareHardware\n");

	waitWakeEnable = FALSE;
	pDeviceContext = GetDeviceContext(Device);

    // Create a USB device handle so that we can communicate with the
    // underlying USB stack. The WDFUSBDEVICE handle is used to query,
    // configure, and manage all aspects of the USB device.
    // These aspects include device properties, bus properties,
    // and I/O creation and synchronization. We only create the device the first time
    // PrepareHardware is called. If the device is restarted by pnp manager
    // for resource rebalance, we will use the same device handle but then select
    // the interfaces again because the USB stack could reconfigure the device on
    // restart.
	// ����USB�豸�����ѡ���豸�ӿڣ����ֻ�ڵ�һ�ε���ʱ�������ӿ�����������������
    if (pDeviceContext->UsbDevice == NULL) {
        // Specifying a client contract version of 602 enables us to query for
        // and use the new capabilities of the USB driver stack for Windows 8.
        // It also implies that we conform to rules mentioned in MSDN
        // documentation for WdfUsbTargetDeviceCreateWithParameters.
		WDF_USB_DEVICE_CREATE_CONFIG config;
		WDF_USB_DEVICE_CREATE_CONFIG_INIT(&config, USBD_CLIENT_CONTRACT_VERSION_602);

		// ��������USB�豸����
        status = WdfUsbTargetDeviceCreateWithParameters(Device,
                                                    &config,
                                                    WDF_NO_OBJECT_ATTRIBUTES,
                                                    &pDeviceContext->UsbDevice
                                                    );

        if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfUsbTargetDeviceCreateWithParameters failed with Status code %!STATUS!\n", status);
            return status;
        }

		//
		// TODO: If you are fetching configuration descriptor from device for
		// selecting a configuration or to parse other descriptors, call OsrFxValidateConfigurationDescriptor
		// to do basic validation on the descriptors before you access them .
		//
    }

	// Retrieve USBD version information, port driver capabilites and device capabilites such as speed, power, etc.
	// ��ȡUSB�豸�İ汾��Ϣ���˿������������豸�ٶȡ���Դ������
	WDF_USB_DEVICE_INFORMATION_INIT(&deviceInfo);
	status = WdfUsbTargetDeviceRetrieveInformation(pDeviceContext->UsbDevice, &deviceInfo);

	if (NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "IsDeviceHighSpeed: %s\n",
			(deviceInfo.Traits & WDF_USB_DEVICE_TRAIT_AT_HIGH_SPEED) ? "TRUE" : "FALSE");
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "IsDeviceSelfPowered: %s\n",
			(deviceInfo.Traits & WDF_USB_DEVICE_TRAIT_SELF_POWERED) ? "TRUE" : "FALSE");

		waitWakeEnable = deviceInfo.Traits & WDF_USB_DEVICE_TRAIT_REMOTE_WAKE_CAPABLE;

		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "IsDeviceRemoteWakeable: %s\n", waitWakeEnable ? "TRUE" : "FALSE");

		// Save these for use later.
		pDeviceContext->UsbDeviceTraits = deviceInfo.Traits;
	}
	else {
		pDeviceContext->UsbDeviceTraits = 0;
	}

	// ѡ�������USB�ӿڡ��ܵ�
	status = SelectInterfaces(Device);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "SelectInterfaces failed 0x%x\n", status);
		return status;
	}

	// Enable wait-wake and idle timeout if the device supports it
	if (waitWakeEnable) {
		status = KmdfUsbSetPowerPolicy(Device);
		if (!NT_SUCCESS(status)) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "KmdfUsbSetPowerPolicy failed  %!STATUS!\n", status);
			return status;
		}
	}

	status = KmdfUsbConfigContReaderForInterruptEndPoint(pDeviceContext);

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- KmdfUsbEvtDevicePrepareHardware\n");

	return status;

}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
KmdfUsbSetPowerPolicy(
	_In_ WDFDEVICE Device
)
{
	WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS idleSettings;
	WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS wakeSettings;
	NTSTATUS    status = STATUS_SUCCESS;

	PAGED_CODE();

	// �����豸Ϊ��ʱ���ߣ���ʱ����10S���Զ���������״̬
	WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(&idleSettings, IdleUsbSelectiveSuspend);
	idleSettings.IdleTimeout = 10000; // 10-sec

	status = WdfDeviceAssignS0IdleSettings(Device, &idleSettings);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfDeviceSetPowerPolicyS0IdlePolicy failed %x\n", status);
		return status;
	}

	// ����Ϊ��Զ�̻��ѣ������������棺1) �豸��������  2) ��PCϵͳ�Ѿ��������ߺ��豸���Խ�ϵͳ����
	WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS_INIT(&wakeSettings);

	status = WdfDeviceAssignSxWakeSettings(Device, &wakeSettings);
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfDeviceAssignSxWakeSettings failed %x\n", status);
		return status;
	}

	return status;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS
SelectInterfaces(
	_In_ WDFDEVICE Device
)
/*++

Routine Description:

	This helper routine selects the configuration, interface and
	creates a context for every pipe (end point) in that interface.

Arguments:

	Device - Handle to a framework device

Return Value:

	NT status value

--*/
{
	WDF_USB_DEVICE_SELECT_CONFIG_PARAMS configParams;
	NTSTATUS                            status = STATUS_SUCCESS;
	PDEVICE_CONTEXT                     pDeviceContext;
	WDFUSBPIPE                          pipe;
	WDF_USB_PIPE_INFORMATION            pipeInfo;
	UCHAR                               index;
	UCHAR                               numberConfiguredPipes;

	PAGED_CODE();

	pDeviceContext = GetDeviceContext(Device);

	// ��ʼ��configParams�������ӿ�
	WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(&configParams);

	// ����USB�豸����ʹ�õ����ӿ�
	status = WdfUsbTargetDeviceSelectConfig(pDeviceContext->UsbDevice, 
		WDF_NO_OBJECT_ATTRIBUTES,
		&configParams);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfUsbTargetDeviceSelectConfig failed %!STATUS! \n", status);

		// Since the Osr USB fx2 device is capable of working at high speed, the only reason
		// the device would not be working at high speed is if the port doesn't
		// support it. If the port doesn't support high speed it is a 1.1 port
		if ((pDeviceContext->UsbDeviceTraits & WDF_USB_DEVICE_TRAIT_AT_HIGH_SPEED) == 0) {
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP,
				" On a 1.1 USB port on Windows Vista"
				" this is expected as the OSR USB Fx2 board's Interrupt EndPoint descriptor"
				" doesn't conform to the USB specification. Windows Vista detects this and"
				" returns an error. \n"
			);

			/*
			GUID activity = DeviceToActivityId(Device);
			EventWriteSelectConfigFailure(
				&activity,
				pDeviceContext->DeviceName,
				pDeviceContext->Location,
				status
			);
			*/
		}

		return status;
	}

	// �洢����USB�ӿڶ����豸����������
	pDeviceContext->UsbInterface = configParams.Types.SingleInterface.ConfiguredUsbInterface;

	// ��ȡ�õ����ӿڶ����а����Ĺܵ����˵㣩����
	numberConfiguredPipes = configParams.Types.SingleInterface.NumberConfiguredPipes;

	// ��ȡ���洢�ܵ����
	for (index = 0; index < numberConfiguredPipes; index++) {

		WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);

		// ���ؽӿ�ָ���Ĺܵ�������ܵ���Ϣ
		pipe = WdfUsbInterfaceGetConfiguredPipe(
			pDeviceContext->UsbInterface,
			index,			//PipeIndex,
			&pipeInfo
		);

		// Tell the framework that it's okay to read less than MaximumPacketSize
		WdfUsbTargetPipeSetNoMaximumPacketSizeCheck(pipe);

		if (WdfUsbPipeTypeInterrupt == pipeInfo.PipeType) {
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "Interrupt Pipe is 0x%p\n", pipe);
			pDeviceContext->InterruptPipe = pipe;
		}

		// The WdfUsbTargetPipeIsInEndpoint method determines whether a specified USB pipe is connected to an input endpoint.
		if (WdfUsbPipeTypeBulk == pipeInfo.PipeType &&
			WdfUsbTargetPipeIsInEndpoint(pipe)) {
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "BulkInput Pipe is 0x%p\n", pipe);
			pDeviceContext->BulkReadPipe = pipe;
		}

		if (WdfUsbPipeTypeBulk == pipeInfo.PipeType &&
			WdfUsbTargetPipeIsOutEndpoint(pipe)) {
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "BulkOutput Pipe is 0x%p\n", pipe);
			pDeviceContext->BulkWritePipe = pipe;
		}

	}

	// If we didn't find all the 3 pipes, fail the start.
	if (!(pDeviceContext->BulkWritePipe
		&& pDeviceContext->BulkReadPipe && pDeviceContext->InterruptPipe)) {
		status = STATUS_INVALID_DEVICE_STATE;
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "Device is not configured properly %!STATUS!\n", status);
		return status;
	}

	return status;
}

NTSTATUS
KmdfUsbEvtDeviceD0Entry(
	WDFDEVICE Device,
	WDF_POWER_DEVICE_STATE PreviousState
)
/*++

Routine Description:

	EvtDeviceD0Entry event callback must perform any operations that are
	necessary before the specified device is used.  It will be called every
	time the hardware needs to be (re-)initialized.

	This function is not marked pageable because this function is in the
	device power up path. When a function is marked pagable and the code
	section is paged out, it will generate a page fault which could impact
	the fast resume behavior because the client driver will have to wait
	until the system drivers can service this page fault.

	This function runs at PASSIVE_LEVEL, even though it is not paged.  A
	driver can optionally make this function pageable if DO_POWER_PAGABLE
	is set.  Even if DO_POWER_PAGABLE isn't set, this function still runs
	at PASSIVE_LEVEL.  In this case, though, the function absolutely must
	not do anything that will cause a page fault.

Arguments:

	Device - Handle to a framework device object.

	PreviousState - Device power state which the device was in most recently.
		If the device is being newly started, this will be
		PowerDeviceUnspecified.

Return Value:

	NTSTATUS

--*/
{
	PDEVICE_CONTEXT         pDeviceContext;
	NTSTATUS                status;
	BOOLEAN                 isTargetStarted;

	pDeviceContext = GetDeviceContext(Device);
	isTargetStarted = FALSE;

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER, "-->KmdfUsbEvtDeviceD0Entry - coming from %s\n", DbgDevicePowerString(PreviousState));

	// Since continuous reader is configured for this interrupt-pipe, we must explicitly start
	// the I/O target to get the framework to post read requests.
	status = WdfIoTargetStart(WdfUsbTargetPipeGetIoTarget(pDeviceContext->InterruptPipe));
	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, DBG_POWER, "Failed to start interrupt pipe %!STATUS!\n", status);
		goto End;
	}

	isTargetStarted = TRUE;

End:

	if (!NT_SUCCESS(status)) {
		// Failure in D0Entry will lead to device being removed. So let us stop the continuous reader in preparation for the ensuing remove.
		if (isTargetStarted) {
			WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(pDeviceContext->InterruptPipe), WdfIoTargetCancelSentIo);
		}
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER, "<--KmdfUsbEvtDeviceD0Entry\n");

	return status;
}


NTSTATUS
KmdfUsbEvtDeviceD0Exit(
	WDFDEVICE Device,
	WDF_POWER_DEVICE_STATE TargetState
)
/*++

Routine Description:

	This routine undoes anything done in EvtDeviceD0Entry.  It is called
	whenever the device leaves the D0 state, which happens when the device is
	stopped, when it is removed, and when it is powered off.

	The device is still in D0 when this callback is invoked, which means that
	the driver can still touch hardware in this routine.


	EvtDeviceD0Exit event callback must perform any operations that are
	necessary before the specified device is moved out of the D0 state.  If the
	driver needs to save hardware state before the device is powered down, then
	that should be done here.

	This function runs at PASSIVE_LEVEL, though it is generally not paged.  A
	driver can optionally make this function pageable if DO_POWER_PAGABLE is set.

	Even if DO_POWER_PAGABLE isn't set, this function still runs at
	PASSIVE_LEVEL.  In this case, though, the function absolutely must not do
	anything that will cause a page fault.

Arguments:

	Device - Handle to a framework device object.

	TargetState - Device power state which the device will be put in once this
		callback is complete.

Return Value:

	Success implies that the device can be used.  Failure will result in the
	device stack being torn down.

--*/
{
	PDEVICE_CONTEXT         pDeviceContext;

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER,
		"-->KmdfUsbEvtDeviceD0Exit - moving to %s\n",
		DbgDevicePowerString(TargetState));

	pDeviceContext = GetDeviceContext(Device);

	WdfIoTargetStop(WdfUsbTargetPipeGetIoTarget(pDeviceContext->InterruptPipe), WdfIoTargetCancelSentIo);

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER, "<--KmdfUsbEvtDeviceD0Exit\n");

	return STATUS_SUCCESS;
}

VOID
KmdfUsbEvtDeviceSelfManagedIoFlush(
	_In_ WDFDEVICE Device
)
/*++

Routine Description:

	�����̴����豸�����ҹ���I/O������ˢ�»

Arguments:

	Device - Handle to a framework device object.

Return Value:

	None

--*/
{
	// Service the interrupt message queue to drain any outstanding requests
	KmdfUsbIoctlGetInterruptMessage(Device, STATUS_DEVICE_REMOVED);
}

_IRQL_requires_(PASSIVE_LEVEL)
PCHAR
DbgDevicePowerString(
	_In_ WDF_POWER_DEVICE_STATE Type
)
{
	switch (Type)
	{
	case WdfPowerDeviceInvalid:
		return "WdfPowerDeviceInvalid";
	case WdfPowerDeviceD0:
		return "WdfPowerDeviceD0";
	case WdfPowerDeviceD1:
		return "WdfPowerDeviceD1";
	case WdfPowerDeviceD2:
		return "WdfPowerDeviceD2";
	case WdfPowerDeviceD3:
		return "WdfPowerDeviceD3";
	case WdfPowerDeviceD3Final:
		return "WdfPowerDeviceD3Final";
	case WdfPowerDevicePrepareForHibernation:
		return "WdfPowerDevicePrepareForHibernation";
	case WdfPowerDeviceMaximum:
		return "WdfPowerDeviceMaximum";
	default:
		return "UnKnown Device Power State";
	}
}

_IRQL_requires_(PASSIVE_LEVEL)
VOID
GetDeviceEventLoggingNames(
	_In_ WDFDEVICE Device
)
/*++

Routine Description:

	Retrieve the friendly name and the location string into WDFMEMORY objects
	and store them in the device context.

Arguments:

Return Value:

	None

--*/
{
	PDEVICE_CONTEXT pDevContext = GetDeviceContext(Device);

	WDF_OBJECT_ATTRIBUTES objectAttributes;

	WDFMEMORY deviceNameMemory = NULL;
	WDFMEMORY locationMemory = NULL;

	NTSTATUS status;

	PAGED_CODE();

	//
	// We want both memory objects to be children of the device so they will
	// be deleted automatically when the device is removed.
	//
	WDF_OBJECT_ATTRIBUTES_INIT(&objectAttributes);
	objectAttributes.ParentObject = Device;

	//
	// First get the length of the string. If the FriendlyName
	// is not there then get the lenght of device description.
	//
	status = WdfDeviceAllocAndQueryProperty(Device,
		DevicePropertyFriendlyName,
		NonPagedPoolNx,
		&objectAttributes,
		&deviceNameMemory);

	if (!NT_SUCCESS(status))
	{
		status = WdfDeviceAllocAndQueryProperty(Device,
			DevicePropertyDeviceDescription,
			NonPagedPoolNx,
			&objectAttributes,
			&deviceNameMemory);
	}

	if (NT_SUCCESS(status))
	{
		pDevContext->DeviceNameMemory = deviceNameMemory;
		pDevContext->DeviceName = WdfMemoryGetBuffer(deviceNameMemory, NULL);
	}
	else
	{
		pDevContext->DeviceNameMemory = NULL;
		pDevContext->DeviceName = L"(error retrieving name)";
	}

	//
	// Retrieve the device location string.
	//
	status = WdfDeviceAllocAndQueryProperty(Device,
		DevicePropertyLocationInformation,
		NonPagedPoolNx,
		WDF_NO_OBJECT_ATTRIBUTES,
		&locationMemory);

	if (NT_SUCCESS(status))
	{
		pDevContext->LocationMemory = locationMemory;
		pDevContext->Location = WdfMemoryGetBuffer(locationMemory, NULL);
	}
	else
	{
		pDevContext->LocationMemory = NULL;
		pDevContext->Location = L"(error retrieving location)";
	}

	return;
}
