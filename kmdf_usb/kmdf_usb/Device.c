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


#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, KmdfUsbCreateDevice)
#pragma alloc_text (PAGE, KmdfUsbEvtDevicePrepareHardware)
#pragma alloc_text (PAGE, KmdfUsbEvtDeviceReleaseHardware)
#pragma alloc_text (PAGE, KmdfUsbEvtDeviceSurpriseRemoval)
#pragma alloc_text (PAGE, KmdfUsbEvtDeviceD0Entry)
#pragma alloc_text (PAGE, KmdfUsbEvtDeviceD0Exit)
#pragma alloc_text (PAGE, ConfigureUsbDevice)
#pragma alloc_text (PAGE, GetUsbPipes)
#pragma alloc_text (PAGE, InitPowerManagement)
#pragma alloc_text (PAGE, InitSettingPairs)
#pragma alloc_text (PAGE, PowerName)
#endif

// ��ӿ��豸���ó�ʼ��Alterֵ
UCHAR MultiInterfaceSettings[MAX_INTERFACES] = { 1, 1, 1, 1, 1, 1, 1, 1 };

NTSTATUS
KmdfUsbCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
/*++

Routine Description:

    Worker routine called to create a device and its software resources.

Arguments:

    DeviceInit - Pointer to an opaque init structure. Memory for this
                    structure will be freed by the framework when the WdfDeviceCreate
                    succeeds. So don't access the structure after that point.

Return Value:

    NTSTATUS

--*/
{
	NTSTATUS status = STATUS_OBJECT_NAME_COLLISION;
	WDFDEVICE device = NULL;
	UCHAR nInstance = 0;

	UNICODE_STRING DeviceName;
	UNICODE_STRING DosDeviceName;
	UNICODE_STRING RefString;
	// WDFSTRING	   SymbolName;

	WDF_OBJECT_ATTRIBUTES attributes;					// �豸���������
	PDEVICE_CONTEXT devCtx = NULL;						// �豸����Ļ�������

	WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;		// pnp�ص�����
	WDF_DEVICE_PNP_CAPABILITIES pnpCapabilities;

	WCHAR wcsDeviceName[] = L"\\Device\\CY001-0";			// �豸��
	WCHAR wcsDosDeviceName[] = L"\\DosDevices\\CY001-0";	// ����������
	WCHAR wcsRefString[] = L"CY001-0";						// ����������
	size_t nLen = wcslen(wcsDeviceName);

    PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> KmdfUsbCreateDevice routine\n");

	RtlInitUnicodeString(&DeviceName, wcsDeviceName);
	RtlInitUnicodeString(&DosDeviceName, wcsDosDeviceName);
	RtlInitUnicodeString(&RefString, wcsRefString);

	// ע��PNP��Power�ص�����
	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
	pnpPowerCallbacks.EvtDevicePrepareHardware = KmdfUsbEvtDevicePrepareHardware;	// ��ʼ��
	pnpPowerCallbacks.EvtDeviceReleaseHardware = KmdfUsbEvtDeviceReleaseHardware;	// ֹͣ
	pnpPowerCallbacks.EvtDeviceSurpriseRemoval = KmdfUsbEvtDeviceSurpriseRemoval;	// �쳣�Ƴ�
	pnpPowerCallbacks.EvtDeviceD0Entry = KmdfUsbEvtDeviceD0Entry;					// ����D0��Դ״̬������״̬����������β��롢���߻���
	pnpPowerCallbacks.EvtDeviceD0Exit = KmdfUsbEvtDeviceD0Exit;						// �뿪D0��Դ״̬������״̬�����������߻��豸�Ƴ�
	WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

	// ����д����Ļ��巽ʽ
	// Ĭ��ΪBuffered��ʽ���������ַ�ʽ��Direct��Neither��
	WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoBuffered);

	// ��ʼ���豸�������Ժͻ�������
	// ���ڲ������sizeof(DEVICE_CONTEXT)��ṹ�峤��
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);

	// ����֧�����8��ʵ���������������������PC��ʱ�����������Ǹ��貢��֧�֡�
	// ��ͬ���豸���󣬸��������Ƶ�β����0-7����𣬲�����β�������豸ID��
	// ����Ĵ����߼�Ϊ��ǰ�豸����Ѱ��һ������ID��
	for (nInstance = 0; nInstance < MAX_INSTANCE_NUMBER; nInstance++)
	{
		// �޸��豸ID
		wcsDeviceName[nLen - 1] += nInstance;

		// ������������������ʧ�ܣ�ʧ�ܵ�ԭ�������������Ч�������Ѵ��ڵ�
		WdfDeviceInitAssignName(DeviceInit, &DeviceName);

		// ����WDF�豸
		// ���ܳɹ�����������ɹ���
		status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
		if (!NT_SUCCESS(status))
		{
			if (status == STATUS_OBJECT_NAME_COLLISION)	// ���ֳ�ͻ
			{
				TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "Already exist: %wZ", &DeviceName);
			}
			else
			{
				TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfDeviceCreate failed with status 0x%08x!!!", status);
				return status;
			}
		}
		else
		{
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Device name: %wZ", &DeviceName);
			break;
		}
	}

	// ��ʧ�ܣ����������ӵĿ���������̫�ࣻ
	// ����ʹ��WinOBJ�鿴ϵͳ�е��豸����
	if (!NT_SUCCESS(status))
		return status;

	// �����������ӣ�Ӧ�ó�����ݷ������Ӳ鿴��ʹ���ں��豸��
	// ���˴������������⣬���ڸ��õķ�����ʹ��WdfDeviceCreateDeviceInterface�����豸�ӿڡ�
	// �豸�ӿ��ܱ�֤���ֲ����ͻ���������пɶ��ԣ����������Բ��÷���������ʽ��
	nLen = wcslen(wcsDosDeviceName);
	wcsDosDeviceName[nLen - 1] += nInstance;

	status = WdfDeviceCreateSymbolicLink(device, &DosDeviceName);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "Failed to create symbol link: 0x%08X", status);
		return status;
	}

#if 0
	// WdfDeviceCreateDeviceInterface������һ�������ַ�����������㣻
	// ������ͬһ���ӿ����еĶ���豸�ӿ���������
	nLen = wcslen(wcsRefString);
	wcsRefString[nLen - 1] += nInstance;

	status = WdfDeviceCreateDeviceInterface(
		device,
		&GUID_DEVINTERFACE_USB,	// �ӿ�GUID
		&RefString);			// Ӧ���ַ���

	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "Failed to create the device interface: 0x%08X", status);
		return status;
	}

	status = WdfStringCreate(NULL, WDF_NO_OBJECT_ATTRIBUTES, &SymbolName);
	if (NT_SUCCESS(status))
	{
		status = WdfDeviceRetrieveDeviceInterfaceString(device, &GUID_DEVINTERFACE_USB, &RefString, SymbolName);
		if (status = STATUS_SUCCESS)
		{
			UNICODE_STRING name;
			WdfStringGetUnicodeString(SymbolName, &name);
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Interface Name:%wZ", &name);
		}
	}
#endif

	// PNP���ԡ������豸�쳣�γ���ϵͳ���ᵯ�������
	WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCapabilities);
	pnpCapabilities.Removable = WdfTrue;
	pnpCapabilities.SurpriseRemovalOK = WdfTrue;
	WdfDeviceSetPnpCapabilities(device, &pnpCapabilities);

	// ��ʼ��������
	// GetDeviceContext��һ������ָ�룬�ɺ�WDF_DECLARE_CONTEXT_TYPE_WITH_NAME���塣
	// �ο�pulibc.h�е�˵����
	devCtx = GetDeviceContext(device);
	RtlZeroMemory(devCtx, sizeof(DEVICE_CONTEXT));

	status = KmdfUsbQueueInitialize(device);
	if (!NT_SUCCESS(status))
		return status;

	// ���汻ע�͵Ĵ��룬��ʾ��һ����WDM����Ա��WDF������Ƕ��WDM����ķ�����
	// ͨ����ȡFunction Device Object���豸ջ����һ���Device Object��
	// ���ǿ����ڵõ�Request�󣬴�Request����ȡ��IRP�ṹ���ƹ�WDK��ܶ����д���
	// ��Request����ȡIRP�ĺ����ǣ�WdfRequestWdmGetIrp
#if 0
	PDEVICE_OBJECT pFunctionDevice = WdfDeviceWdmGetDeviceObject(device);			// �����豸����
	WDFDEVICE DeviceStack = WdfDeviceGetIoTarget(device);
	PDEVICE_OBJECT pDeviceNext = WdfIoTargetWdmGetTargetDeviceObject(DeviceStack);	// ��Attached���²�����֮�豸����
#endif

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- KmdfUsbCreateDevice\n");
	return status;
}

// �˺���������WDM�е�PNP_MN_START_DEVICE������������PnpAdd֮�󱻵���
// ��ʱPNP�������������֮���Ѿ���������Щϵͳ��Դ�������ǰ�豸
// ����ResourceList��ResourceListTranslated��������Щϵͳ��Դ
// ��������������ʱ���豸�Ѿ�������D0��Դ״̬��������ɺ��豸����ʽ���빤��״̬��
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

Return Value:

    NT status value

--*/
{
	NTSTATUS status;
	PDEVICE_CONTEXT devCtx = NULL;
	ULONG ulNumRes = 0;
	ULONG ulIndex;
	CM_PARTIAL_RESOURCE_DESCRIPTOR*  pResDes = NULL;

    UNREFERENCED_PARAMETER(ResourceListTranslated);

    PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> KmdfUsbEvtDevicePrepareHardware\n");

    status = STATUS_SUCCESS;
	devCtx = GetDeviceContext(Device);

	// �����豸
	status = ConfigureUsbDevice(Device, devCtx);
	if (!NT_SUCCESS(status))
		return status;

	// ��ȡPipe���
	status = GetUsbPipes(devCtx);
	if (!NT_SUCCESS(status))
		return status;

	// ��ʼ��Դ���ԣ�
	status = InitPowerManagement(Device);
	//if(!NT_SUCCESS(status))
	//	return status;

	// ��ȡUSB���������ӿڡ����߽ӿ��а������������ṩ�Ļص�������������Ϣ��
	// ���߽ӿ���ϵͳ����GUID��ʶ��
	status = WdfFdoQueryForInterface(
		Device,
		&USB_BUS_INTERFACE_USBDI_GUID1,		// ���߽ӿ�ID
		(PINTERFACE)&devCtx->busInterface,	// �������豸��������
		sizeof(USB_BUS_INTERFACE_USBDI_V1),
		1, NULL);

	// ���ýӿں������ж�USB�汾��
	if (NT_SUCCESS(status) && devCtx->busInterface.IsDeviceHighSpeed) {
		devCtx->bIsDeviceHighSpeed = devCtx->busInterface.IsDeviceHighSpeed(devCtx->busInterface.BusContext);
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "USB 2.0");
	}
	else
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "USB 1.1");

	// ��ϵͳ��Դ�б����ǲ���ʵ���ԵĲ�����������ӡһЩ��Ϣ��
	// ������ȫ���԰��������Щ���붼ע�͵���
	ulNumRes = WdfCmResourceListGetCount(ResourceList);
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "ResourceListCount:%d\n", ulNumRes);

	// ���������������µ�ö����Щϵͳ��Դ������ӡ�����ǵ����������Ϣ��
	for (ulIndex = 0; ulIndex < ulNumRes; ulIndex++)
	{
		pResDes = WdfCmResourceListGetDescriptor(ResourceList, ulIndex);
		if (!pResDes)continue; // ȡ��ʧ�ܣ���������һ��

		switch (pResDes->Type)
		{
		case CmResourceTypeMemory:
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "System Resource��CmResourceTypeMemory\n");
			break;

		case CmResourceTypePort:
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "System Resource��CmResourceTypePort\n");
			break;

		case CmResourceTypeInterrupt:
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "System Resource��CmResourceTypeInterrupt\n");
			break;

		default:
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "System Resource��Others %d\n", pResDes->Type);
			break;
		}
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- KmdfUsbEvtDevicePrepareHardware\n");
	return STATUS_SUCCESS;

}

// �˺���������WDM�е�PNP_MN_STOP_DEVICE���������豸�Ƴ�ʱ�����á�
// ��������������ʱ���豸�Դ��ڹ���״̬��
NTSTATUS KmdfUsbEvtDeviceReleaseHardware(IN WDFDEVICE Device, IN WDFCMRESLIST ResourceListTranslated)
{
	NTSTATUS                             status;
	WDF_USB_DEVICE_SELECT_CONFIG_PARAMS  configParams;
	PDEVICE_CONTEXT                      pDeviceContext;

	UNREFERENCED_PARAMETER(ResourceListTranslated);

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> KmdfUsbEvtDeviceReleaseHardware\n");

	pDeviceContext = GetDeviceContext(Device);

	// ���PnpPrepareHardware����ʧ��,UsbDeviceΪ�գ�
	// ��ʱ��ֱ�ӷ��ؼ��ɡ�
	if (pDeviceContext->UsbDevice == NULL)
		return STATUS_SUCCESS;

	// ȡ��USB�豸������IO��������������ȡ������Pipe��IO������
	WdfIoTargetStop(WdfUsbTargetDeviceGetIoTarget(pDeviceContext->UsbDevice), WdfIoTargetCancelSentIo);

	// Deconfiguration���ߡ������á�
	WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_DECONFIG(&configParams);
	status = WdfUsbTargetDeviceSelectConfig(
		pDeviceContext->UsbDevice,
		WDF_NO_OBJECT_ATTRIBUTES,
		&configParams);

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- KmdfUsbEvtDeviceReleaseHardware\n");
	return STATUS_SUCCESS;
}

VOID KmdfUsbEvtDeviceSurpriseRemoval(IN WDFDEVICE  Device)
{
	PAGED_CODE();
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> KmdfUsbEvtDeviceSurpriseRemoval\n");
	CompleteSyncRequest(Device, SURPRISE_REMOVE, 0);
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- KmdfUsbEvtDeviceSurpriseRemoval\n");
}

// ��������Power�ص�����WDM�е�PnpSetPower���ơ�
NTSTATUS KmdfUsbEvtDeviceD0Entry(IN WDFDEVICE  Device, IN WDF_POWER_DEVICE_STATE  PreviousState)
{
	// PAGED_CODE();

	PDEVICE_CONTEXT pContext = GetDeviceContext(Device);

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER, "--> KmdfUsbEvtDeviceD0Entry from %s\n", PowerName(PreviousState));

	if (PreviousState == PowerDeviceD2)
	{
		SetDigitron(Device, pContext->byPre7Seg);
		SetLEDs(Device, pContext->byPreLEDs);
	}

	CompleteSyncRequest(Device, ENTERD0, PreviousState);

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER, "<-- KmdfUsbEvtDeviceD0Entry\n");

	return STATUS_SUCCESS;
}

// �뿪D0״̬
NTSTATUS KmdfUsbEvtDeviceD0Exit(IN WDFDEVICE  Device, IN WDF_POWER_DEVICE_STATE  TargetState)
{
	PDEVICE_CONTEXT pContext;

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> KmdfUsbEvtDeviceD0Exit to %s\n", PowerName(TargetState));

	pContext = GetDeviceContext(Device);

	if (TargetState == PowerDeviceD2)
	{
		GetDigitron(Device, &pContext->byPre7Seg);
		GetLEDs(Device, &pContext->byPreLEDs);
	}

	CompleteSyncRequest(Device, EXITD0, TargetState);

	// ֹͣ�ж϶�����
	InterruptReadStop(Device);
	ClearSyncQueue(Device);

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER, "<-- KmdfUsbEvtDeviceD0Exit\n");

	return STATUS_SUCCESS;
}

// �豸����
// ����WDF��ܣ��豸����ѡ��Ĭ��Ϊ1������ڶ������ѡ���Ҫ�л�ѡ��Ļ�����Ƚ��鷳��
// һ�ְ취�ǣ�ʹ�ó�ʼ���꣺WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_INTERFACES_DESCRIPTORS 
// ʹ������꣬��Ҫ���Ȼ�ȡ����������������ؽӿ���������
// ��һ�ְ취�ǣ�ʹ��WDM�������ȹ���һ������ѡ���URB��Ȼ��Ҫô�Լ�����IRP���͵�����������
// Ҫôʹ��WDF��������WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_URB��ʼ���ꡣ
NTSTATUS ConfigureUsbDevice(WDFDEVICE Device, PDEVICE_CONTEXT DeviceContext)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_USB_DEVICE_SELECT_CONFIG_PARAMS usbConfig;
	PWDF_USB_INTERFACE_SETTING_PAIR settingPairs;
	UCHAR numInterfaces;
	WDF_USB_INTERFACE_SELECT_SETTING_PARAMS  interfaceSelectSetting;

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> ConfigureUsbDevice\n");

	// ����Usb�豸����
	// USB�豸���������ǽ���USB��������㡣�󲿷ֵ�USB�ӿں�����������������еġ�
	// USB�豸���󱻴������������Լ�ά������ܱ�����������Ҳ����������
	status = WdfUsbTargetDeviceCreate(Device, WDF_NO_OBJECT_ATTRIBUTES, &DeviceContext->UsbDevice);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfUsbTargetDeviceCreate failed with status 0x%08x\n", status);
		return status;
	}

	// �ӿ�����
	// WDF�ṩ�˶��ֽӿ����õĳ�ʼ���꣬�ֱ���Ե�һ�ӿڡ���ӿڵ�USB�豸��
	// ��ʼ���껹�ṩ���ڶ�����ü�����л���;�������������������ġ�
	// ��ѡ��Ĭ�����õ�����£��豸���ý��ޱȼ򵥣��򵥵��������ĥ���ں˳���Ա����۾���
	// ��ΪWDM�ϰ��еĴ����߼�������ֻҪ�����о͹��ˡ�
	numInterfaces = WdfUsbTargetDeviceGetNumInterfaces(DeviceContext->UsbDevice);
	if (1 == numInterfaces)
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "There is one interface.");
		WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(&usbConfig);
	}
	else
	{
		// ��ӿ�
		UCHAR nNum;
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "There are %d interfaces.", numInterfaces);
		settingPairs = ExAllocatePoolWithTag(PagedPool,
			sizeof(WDF_USB_INTERFACE_SETTING_PAIR) * numInterfaces, POOLTAG);

		if (settingPairs == NULL)
			return STATUS_INSUFFICIENT_RESOURCES;

		nNum = (UCHAR)InitSettingPairs(DeviceContext->UsbDevice, settingPairs, numInterfaces);
		WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_MULTIPLE_INTERFACES(&usbConfig, nNum, settingPairs);
	}

	status = WdfUsbTargetDeviceSelectConfig(DeviceContext->UsbDevice, WDF_NO_OBJECT_ATTRIBUTES, &usbConfig);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfUsbTargetDeviceSelectConfig failed with status 0x%08x\n", status);
		return status;
	}

	// ����ӿ�
	if (1 == numInterfaces)
	{
		DeviceContext->UsbInterface = usbConfig.Types.SingleInterface.ConfiguredUsbInterface;

		// ʹ��SINGLE_INTERFACE�ӿ����ú꣬�ӿڵ�AltSettingֵĬ��Ϊ0��
		// �������д�����ʾ������ֶ��޸�ĳ�ӿڵ�AltSettingֵ���˴�Ϊ1��.
		WDF_USB_INTERFACE_SELECT_SETTING_PARAMS_INIT_SETTING(&interfaceSelectSetting, 1);
		status = WdfUsbInterfaceSelectSetting(DeviceContext->UsbInterface, WDF_NO_OBJECT_ATTRIBUTES, &interfaceSelectSetting);
	}
	else
	{
		int i;
		DeviceContext->UsbInterface = usbConfig.Types.MultiInterface.Pairs[0].UsbInterface;
		for (i = 0; i < numInterfaces; i++)
			DeviceContext->MulInterfaces[i] = usbConfig.Types.MultiInterface.Pairs[i].UsbInterface;
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- ConfigureUsbDevice\n");
	return status;
}

// �豸���úú󣬽ӿڡ��ܵ����Ѵ����ˡ�
NTSTATUS GetUsbPipes(PDEVICE_CONTEXT DeviceContext)
{
	BYTE index = 0;
	WDF_USB_PIPE_INFORMATION pipeInfo;
	WDFUSBPIPE pipe = NULL;

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> GetUsbPipes\n");

	DeviceContext->UsbIntOutPipe = NULL;
	DeviceContext->UsbIntInPipe = NULL;
	DeviceContext->UsbBulkInPipe = NULL;
	DeviceContext->UsbBulkOutPipe = NULL;

	WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);

	while (TRUE)
	{
		pipe = WdfUsbInterfaceGetConfiguredPipe(DeviceContext->UsbInterface, index, &pipeInfo);
		if (NULL == pipe)break;

		// Dump �ܵ���Ϣ
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Type:%s\r\nEndpointAddress:0x%x\r\nMaxPacketSize:%d\r\nAlternateValue:%d",
			pipeInfo.PipeType == WdfUsbPipeTypeInterrupt ? "Interrupt" :
			pipeInfo.PipeType == WdfUsbPipeTypeBulk ? "Bulk" :
			pipeInfo.PipeType == WdfUsbPipeTypeControl ? "Control" :
			pipeInfo.PipeType == WdfUsbPipeTypeIsochronous ? "Isochronous" : "Invalid!!",
			pipeInfo.EndpointAddress,
			pipeInfo.MaximumPacketSize,
			pipeInfo.SettingIndex);

		// ���ùܵ����ԣ����԰����ȼ��
		// ��������ã���ôÿ�ζԹܵ�����д������ʱ�����뻺�����ĳ��ȱ�����
		// pipeInfo.MaximumPacketSize������������������ʧ�ܡ�
		// ����ṩ����������飬�ɱ������������߻�ȡ�����벻����������Ϣ�������Ǵ˴����ԡ�
		WdfUsbTargetPipeSetNoMaximumPacketSizeCheck(pipe);

		if (WdfUsbPipeTypeControl == pipeInfo.PipeType)
		{
			DeviceContext->UsbCtlPipe = pipe;
		}
		else if (WdfUsbPipeTypeInterrupt == pipeInfo.PipeType)
		{
			if (TRUE == WdfUsbTargetPipeIsInEndpoint(pipe))
				DeviceContext->UsbIntInPipe = pipe;
			else
				DeviceContext->UsbIntOutPipe = pipe;
		}
		else if (WdfUsbPipeTypeBulk == pipeInfo.PipeType)
		{
			if (TRUE == WdfUsbTargetPipeIsInEndpoint(pipe))
			{
				DeviceContext->UsbBulkInPipe = pipe;
			}
			else if (TRUE == WdfUsbTargetPipeIsOutEndpoint(pipe))
			{
				DeviceContext->UsbBulkOutPipe = pipe;
			}
		}

		index++;
	}

	// ͨ���ܵ��жϹ̼��汾
	if ((NULL == DeviceContext->UsbIntOutPipe) ||
		(NULL == DeviceContext->UsbIntInPipe) ||
		(NULL == DeviceContext->UsbBulkInPipe) ||
		(NULL == DeviceContext->UsbBulkOutPipe))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "Not our CY001 firmware!!!\n");
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- GetUsbPipes\n");
	return STATUS_SUCCESS;
}

// �����豸�����ĵ�Դ������
NTSTATUS InitPowerManagement(IN WDFDEVICE  Device)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_USB_DEVICE_INFORMATION usbInfo;
	PDEVICE_CONTEXT pContext;

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> InitPowerManagement\n");

	pContext = GetDeviceContext(Device);

	// ��ȡ�豸��Ϣ
	WDF_USB_DEVICE_INFORMATION_INIT(&usbInfo);
	WdfUsbTargetDeviceRetrieveInformation(pContext->UsbDevice, &usbInfo);

	// USB�豸��Ϣ��������ʽ��������Traits�С�
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Device self powered: %s",
		usbInfo.Traits & WDF_USB_DEVICE_TRAIT_SELF_POWERED ? "TRUE" : "FALSE");
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Device remote wake capable: %s",
		usbInfo.Traits & WDF_USB_DEVICE_TRAIT_REMOTE_WAKE_CAPABLE ? "TRUE" : "FALSE");
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Device high speed: %s",
		usbInfo.Traits & WDF_USB_DEVICE_TRAIT_AT_HIGH_SPEED ? "TRUE" : "FALSE");


	// �����豸�����ߺ�Զ�̻��ѹ���
	if (usbInfo.Traits & WDF_USB_DEVICE_TRAIT_REMOTE_WAKE_CAPABLE)
	{
		WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS idleSettings;
		WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS wakeSettings;

		// �����豸Ϊ��ʱ���ߡ���ʱ����10S���Զ���������״̬��
		WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(&idleSettings, IdleUsbSelectiveSuspend);
		idleSettings.IdleTimeout = 10000;
		status = WdfDeviceAssignS0IdleSettings(Device, &idleSettings);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfDeviceAssignS0IdleSettings failed with status 0x%0.8x!!!", status);
			return status;
		}

		// ����Ϊ��Զ�̻��ѡ������豸�����������Ѿ���PCϵͳ�������ߺ��豸���Խ�ϵͳ���ѣ��������档
		WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS_INIT(&wakeSettings);
		status = WdfDeviceAssignSxWakeSettings(Device, &wakeSettings);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfDeviceAssignSxWakeSettings failed with status 0x%0.8x!!!", status);
			return status;
		}
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- InitPowerManagement\n");
	return status;
}

// ��ʼ���ṹ��WDF_USB_INTERFACE_SETTING_PAIR��
// �������ö�ӿ��豸��
int  InitSettingPairs(
	IN WDFUSBDEVICE UsbDevice,					// �豸����
	OUT PWDF_USB_INTERFACE_SETTING_PAIR Pairs,  // �ṹ��ָ�롣
	IN ULONG NumSettings						// �ӿڸ���
)
{
	UCHAR i;

	PAGED_CODE();

	// ���֧��8���ӿڣ��Ѷ�����е���
	if (NumSettings > MAX_INTERFACES)
		NumSettings = MAX_INTERFACES;

	// ���ýӿ�
	for (i = 0; i < NumSettings; i++) {
		Pairs[i].UsbInterface = WdfUsbTargetDeviceGetInterface(UsbDevice, i);// ���ýӿھ��
		Pairs[i].SettingIndex = MultiInterfaceSettings[i];					 // ���ýӿڿ�ѡֵ(Alternate Setting)
	}

	return NumSettings;
}

char* PowerName(WDF_POWER_DEVICE_STATE PowerState)
{
	char *name;

	switch (PowerState)
	{
	case WdfPowerDeviceInvalid:
		name = "PowerDeviceUnspecified";
		break;
	case WdfPowerDeviceD0:
		name = "WdfPowerDeviceD0";
		break;
	case WdfPowerDeviceD1:
		name = "WdfPowerDeviceD1";
		break;
	case WdfPowerDeviceD2:
		name = "WdfPowerDeviceD2";
		break;
	case WdfPowerDeviceD3:
		name = "WdfPowerDeviceD3";
		break;
	case WdfPowerDeviceD3Final:
		name = "WdfPowerDeviceD3Final";
		break;
	case WdfPowerDevicePrepareForHibernation:
		name = "WdfPowerDevicePrepareForHibernation";
		break;
	case WdfPowerDeviceMaximum:
		name = "WdfPowerDeviceMaximum";
		break;
	default:
		name = "Unknown Power State";
		break;
	}

	return name;
}