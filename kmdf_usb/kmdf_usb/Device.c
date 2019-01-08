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

// 多接口设备配置初始化Alter值
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

	WDF_OBJECT_ATTRIBUTES attributes;					// 设备对象的属性
	PDEVICE_CONTEXT devCtx = NULL;						// 设备对象的环境变量

	WDF_PNPPOWER_EVENT_CALLBACKS pnpPowerCallbacks;		// pnp回调函数
	WDF_DEVICE_PNP_CAPABILITIES pnpCapabilities;

	WCHAR wcsDeviceName[] = L"\\Device\\CY001-0";			// 设备名
	WCHAR wcsDosDeviceName[] = L"\\DosDevices\\CY001-0";	// 符号链接名
	WCHAR wcsRefString[] = L"CY001-0";						// 符号链接名
	size_t nLen = wcslen(wcsDeviceName);

    PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> KmdfUsbCreateDevice routine\n");

	RtlInitUnicodeString(&DeviceName, wcsDeviceName);
	RtlInitUnicodeString(&DosDeviceName, wcsDosDeviceName);
	RtlInitUnicodeString(&RefString, wcsRefString);

	// 注册PNP与Power回调函数
	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);
	pnpPowerCallbacks.EvtDevicePrepareHardware = KmdfUsbEvtDevicePrepareHardware;	// 初始化
	pnpPowerCallbacks.EvtDeviceReleaseHardware = KmdfUsbEvtDeviceReleaseHardware;	// 停止
	pnpPowerCallbacks.EvtDeviceSurpriseRemoval = KmdfUsbEvtDeviceSurpriseRemoval;	// 异常移除
	pnpPowerCallbacks.EvtDeviceD0Entry = KmdfUsbEvtDeviceD0Entry;					// 进入D0电源状态（工作状态），比如初次插入、或者唤醒
	pnpPowerCallbacks.EvtDeviceD0Exit = KmdfUsbEvtDeviceD0Exit;						// 离开D0电源状态（工作状态），比如休眠或设备移除
	WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

	// 读、写请求的缓冲方式
	// 默认为Buffered方式，另外两种方式是Direct和Neither。
	WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoBuffered);

	// 初始化设备对象属性和环境变量
	// 宏内部会调用sizeof(DEVICE_CONTEXT)求结构体长度
	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);

	// 驱动支持最多8个实例，即当多个开发板连到PC上时，驱动对它们给予并行支持。
	// 不同的设备对象，各以其名称的尾数（0-7）相别，并将此尾数称作设备ID。
	// 下面的代码逻辑为当前设备对象寻找一个可用ID。
	for (nInstance = 0; nInstance < MAX_INSTANCE_NUMBER; nInstance++)
	{
		// 修改设备ID
		wcsDeviceName[nLen - 1] += nInstance;

		// 尝试命名；命名可能失败，失败的原因包括：名称无效、名称已存在等
		WdfDeviceInitAssignName(DeviceInit, &DeviceName);

		// 创建WDF设备
		// 如能成功，则命名亦成功。
		status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
		if (!NT_SUCCESS(status))
		{
			if (status == STATUS_OBJECT_NAME_COLLISION)	// 名字冲突
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

	// 如失败，可能是连接的开发板数量太多；
	// 建议使用WinOBJ查看系统中的设备名称
	if (!NT_SUCCESS(status))
		return status;

	// 创建符号链接，应用程序根据符号链接查看并使用内核设备。
	// 除了创建符号链接外，现在更好的方法是使用WdfDeviceCreateDeviceInterface创建设备接口。
	// 设备接口能保证名字不会冲突，但不具有可读性，所以我们仍采用符号链接形式。
	nLen = wcslen(wcsDosDeviceName);
	wcsDosDeviceName[nLen - 1] += nInstance;

	status = WdfDeviceCreateSymbolicLink(device, &DosDeviceName);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "Failed to create symbol link: 0x%08X", status);
		return status;
	}

#if 0
	// WdfDeviceCreateDeviceInterface需设置一个引用字符串，内容随便；
	// 用来将同一个接口类中的多个设备接口区别开来。
	nLen = wcslen(wcsRefString);
	wcsRefString[nLen - 1] += nInstance;

	status = WdfDeviceCreateDeviceInterface(
		device,
		&GUID_DEVINTERFACE_USB,	// 接口GUID
		&RefString);			// 应用字符串

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

	// PNP属性。允许设备异常拔除，系统不会弹出错误框。
	WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCapabilities);
	pnpCapabilities.Removable = WdfTrue;
	pnpCapabilities.SurpriseRemovalOK = WdfTrue;
	WdfDeviceSetPnpCapabilities(device, &pnpCapabilities);

	// 初始化环境块
	// GetDeviceContext是一个函数指针，由宏WDF_DECLARE_CONTEXT_TYPE_WITH_NAME定义。
	// 参看pulibc.h中的说明。
	devCtx = GetDeviceContext(device);
	RtlZeroMemory(devCtx, sizeof(DEVICE_CONTEXT));

	status = KmdfUsbQueueInitialize(device);
	if (!NT_SUCCESS(status))
		return status;

	// 下面被注释的代码，演示了一种让WDM程序员在WDF程序中嵌入WDM代码的方法。
	// 通过获取Function Device Object和设备栈中下一层的Device Object，
	// 我们可以在得到Request后，从Request中析取出IRP结构，绕过WDK框架而自行处理。
	// 从Request中析取IRP的函数是：WdfRequestWdmGetIrp
#if 0
	PDEVICE_OBJECT pFunctionDevice = WdfDeviceWdmGetDeviceObject(device);			// 功能设备对象
	WDFDEVICE DeviceStack = WdfDeviceGetIoTarget(device);
	PDEVICE_OBJECT pDeviceNext = WdfIoTargetWdmGetTargetDeviceObject(DeviceStack);	// 被Attached的下层驱动之设备对象。
#endif

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- KmdfUsbCreateDevice\n");
	return status;
}

// 此函数类似于WDM中的PNP_MN_START_DEVICE函数，紧接着PnpAdd之后被调用
// 此时PNP管理器经过甄别之后，已经决定将那些系统资源分配给当前设备
// 参数ResourceList和ResourceListTranslated代表了这些系统资源
// 当个函数被调用时候，设备已经进入了D0电源状态；函数完成后，设备即正式进入工作状态。
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

	// 配置设备
	status = ConfigureUsbDevice(Device, devCtx);
	if (!NT_SUCCESS(status))
		return status;

	// 获取Pipe句柄
	status = GetUsbPipes(devCtx);
	if (!NT_SUCCESS(status))
		return status;

	// 初始电源策略，
	status = InitPowerManagement(Device);
	//if(!NT_SUCCESS(status))
	//	return status;

	// 获取USB总线驱动接口。总线接口中包含总线驱动提供的回调函数和其他信息。
	// 总线接口由系统共用GUID标识。
	status = WdfFdoQueryForInterface(
		Device,
		&USB_BUS_INTERFACE_USBDI_GUID1,		// 总线接口ID
		(PINTERFACE)&devCtx->busInterface,	// 保存在设备环境块中
		sizeof(USB_BUS_INTERFACE_USBDI_V1),
		1, NULL);

	// 调用接口函数，判断USB版本。
	if (NT_SUCCESS(status) && devCtx->busInterface.IsDeviceHighSpeed) {
		devCtx->bIsDeviceHighSpeed = devCtx->busInterface.IsDeviceHighSpeed(devCtx->busInterface.BusContext);
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "USB 2.0");
	}
	else
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "USB 1.1");

	// 对系统资源列表，我们不做实质性的操作，仅仅打印一些信息。
	// 读者完全可以把下面的这些代码都注释掉。
	ulNumRes = WdfCmResourceListGetCount(ResourceList);
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "ResourceListCount:%d\n", ulNumRes);

	// 下面我们饶有兴致地枚举这些系统资源，并打印出它们的相关名称信息。
	for (ulIndex = 0; ulIndex < ulNumRes; ulIndex++)
	{
		pResDes = WdfCmResourceListGetDescriptor(ResourceList, ulIndex);
		if (!pResDes)continue; // 取得失败，则跳到下一个

		switch (pResDes->Type)
		{
		case CmResourceTypeMemory:
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "System Resource：CmResourceTypeMemory\n");
			break;

		case CmResourceTypePort:
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "System Resource：CmResourceTypePort\n");
			break;

		case CmResourceTypeInterrupt:
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "System Resource：CmResourceTypeInterrupt\n");
			break;

		default:
			TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "System Resource：Others %d\n", pResDes->Type);
			break;
		}
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "<-- KmdfUsbEvtDevicePrepareHardware\n");
	return STATUS_SUCCESS;

}

// 此函数类似于WDM中的PNP_MN_STOP_DEVICE函数，在设备移除时被调用。
// 当个函数被调用时候，设备仍处于工作状态。
NTSTATUS KmdfUsbEvtDeviceReleaseHardware(IN WDFDEVICE Device, IN WDFCMRESLIST ResourceListTranslated)
{
	NTSTATUS                             status;
	WDF_USB_DEVICE_SELECT_CONFIG_PARAMS  configParams;
	PDEVICE_CONTEXT                      pDeviceContext;

	UNREFERENCED_PARAMETER(ResourceListTranslated);

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> KmdfUsbEvtDeviceReleaseHardware\n");

	pDeviceContext = GetDeviceContext(Device);

	// 如果PnpPrepareHardware调用失败,UsbDevice为空；
	// 这时候直接返回即可。
	if (pDeviceContext->UsbDevice == NULL)
		return STATUS_SUCCESS;

	// 取消USB设备的所有IO操作。它将连带取消所有Pipe的IO操作。
	WdfIoTargetStop(WdfUsbTargetDeviceGetIoTarget(pDeviceContext->UsbDevice), WdfIoTargetCancelSentIo);

	// Deconfiguration或者“反配置”
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

// 下面两个Power回调，和WDM中的PnpSetPower类似。
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

// 离开D0状态
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

	// 停止中断读操作
	InterruptReadStop(Device);
	ClearSyncQueue(Device);

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_POWER, "<-- KmdfUsbEvtDeviceD0Exit\n");

	return STATUS_SUCCESS;
}

// 设备配置
// 按照WDF框架，设备配置选项默认为1；如存在多个配置选项，需要切换选择的话，会比较麻烦。
// 一种办法是：使用初始化宏：WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_INTERFACES_DESCRIPTORS 
// 使用这个宏，需要首先获取配置描述符，和相关接口描述符。
// 另一种办法是：使用WDM方法，先构建一个配置选择的URB，然后要么自己调用IRP发送到总线驱动，
// 要么使用WDF方法调用WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_URB初始化宏。
NTSTATUS ConfigureUsbDevice(WDFDEVICE Device, PDEVICE_CONTEXT DeviceContext)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_USB_DEVICE_SELECT_CONFIG_PARAMS usbConfig;
	PWDF_USB_INTERFACE_SETTING_PAIR settingPairs;
	UCHAR numInterfaces;
	WDF_USB_INTERFACE_SELECT_SETTING_PARAMS  interfaceSelectSetting;

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> ConfigureUsbDevice\n");

	// 创建Usb设备对象。
	// USB设备对象是我们进行USB操作的起点。大部分的USB接口函数，都是针对它进行的。
	// USB设备对象被创建后，由驱动自己维护；框架本身不处理它，也不保持它。
	status = WdfUsbTargetDeviceCreate(Device, WDF_NO_OBJECT_ATTRIBUTES, &DeviceContext->UsbDevice);
	if (!NT_SUCCESS(status))
	{
		TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfUsbTargetDeviceCreate failed with status 0x%08x\n", status);
		return status;
	}

	// 接口配置
	// WDF提供了多种接口配置的初始化宏，分别针对单一接口、多接口的USB设备，
	// 初始化宏还提供了在多个配置间进行切换的途径，正如上面所讲过的。
	// 在选择默认配置的情况下，设备配置将无比简单，简单到令长期受折磨的内核程序员大跌眼镜；
	// 因为WDM上百行的代码逻辑，这里只要两三行就够了。
	numInterfaces = WdfUsbTargetDeviceGetNumInterfaces(DeviceContext->UsbDevice);
	if (1 == numInterfaces)
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "There is one interface.");
		WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(&usbConfig);
	}
	else
	{
		// 多接口
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

	// 保存接口
	if (1 == numInterfaces)
	{
		DeviceContext->UsbInterface = usbConfig.Types.SingleInterface.ConfiguredUsbInterface;

		// 使用SINGLE_INTERFACE接口配置宏，接口的AltSetting值默认为0。
		// 下面两行代码演示了如何手动修改某接口的AltSetting值（此处为1）.
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

// 设备配置好后，接口、管道就已存在了。
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

		// Dump 管道信息
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Type:%s\r\nEndpointAddress:0x%x\r\nMaxPacketSize:%d\r\nAlternateValue:%d",
			pipeInfo.PipeType == WdfUsbPipeTypeInterrupt ? "Interrupt" :
			pipeInfo.PipeType == WdfUsbPipeTypeBulk ? "Bulk" :
			pipeInfo.PipeType == WdfUsbPipeTypeControl ? "Control" :
			pipeInfo.PipeType == WdfUsbPipeTypeIsochronous ? "Isochronous" : "Invalid!!",
			pipeInfo.EndpointAddress,
			pipeInfo.MaximumPacketSize,
			pipeInfo.SettingIndex);

		// 设置管道属性：忽略包长度检查
		// 如果不设置，那么每次对管道进行写操作的时候，输入缓冲区的长度必须是
		// pipeInfo.MaximumPacketSize的整数倍，否则会操作失败。
		// 框架提供的这个额外检查，可避免驱动从总线获取到意想不到的杂乱信息。但我们此处忽略。
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

	// 通过管道判断固件版本
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

// 配置设备驱动的电源管理功能
NTSTATUS InitPowerManagement(IN WDFDEVICE  Device)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_USB_DEVICE_INFORMATION usbInfo;
	PDEVICE_CONTEXT pContext;

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "--> InitPowerManagement\n");

	pContext = GetDeviceContext(Device);

	// 获取设备信息
	WDF_USB_DEVICE_INFORMATION_INIT(&usbInfo);
	WdfUsbTargetDeviceRetrieveInformation(pContext->UsbDevice, &usbInfo);

	// USB设备信息以掩码形式被保存在Traits中。
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Device self powered: %s",
		usbInfo.Traits & WDF_USB_DEVICE_TRAIT_SELF_POWERED ? "TRUE" : "FALSE");
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Device remote wake capable: %s",
		usbInfo.Traits & WDF_USB_DEVICE_TRAIT_REMOTE_WAKE_CAPABLE ? "TRUE" : "FALSE");
	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_PNP, "Device high speed: %s",
		usbInfo.Traits & WDF_USB_DEVICE_TRAIT_AT_HIGH_SPEED ? "TRUE" : "FALSE");


	// 设置设备的休眠和远程唤醒功能
	if (usbInfo.Traits & WDF_USB_DEVICE_TRAIT_REMOTE_WAKE_CAPABLE)
	{
		WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS idleSettings;
		WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS wakeSettings;

		// 设置设备为闲时休眠。闲时超过10S，自动进入休眠状态。
		WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(&idleSettings, IdleUsbSelectiveSuspend);
		idleSettings.IdleTimeout = 10000;
		status = WdfDeviceAssignS0IdleSettings(Device, &idleSettings);
		if (!NT_SUCCESS(status))
		{
			TraceEvents(TRACE_LEVEL_ERROR, DBG_PNP, "WdfDeviceAssignS0IdleSettings failed with status 0x%0.8x!!!", status);
			return status;
		}

		// 设置为可远程唤醒。包含设备自身醒来，已经当PC系统进入休眠后，设备可以将系统唤醒，两个方面。
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

// 初始化结构体WDF_USB_INTERFACE_SETTING_PAIR。
// 用来配置多接口设备。
int  InitSettingPairs(
	IN WDFUSBDEVICE UsbDevice,					// 设备对象
	OUT PWDF_USB_INTERFACE_SETTING_PAIR Pairs,  // 结构体指针。
	IN ULONG NumSettings						// 接口个数
)
{
	UCHAR i;

	PAGED_CODE();

	// 最多支持8个接口，把多余的切掉。
	if (NumSettings > MAX_INTERFACES)
		NumSettings = MAX_INTERFACES;

	// 配置接口
	for (i = 0; i < NumSettings; i++) {
		Pairs[i].UsbInterface = WdfUsbTargetDeviceGetInterface(UsbDevice, i);// 设置接口句柄
		Pairs[i].SettingIndex = MultiInterfaceSettings[i];					 // 设置接口可选值(Alternate Setting)
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