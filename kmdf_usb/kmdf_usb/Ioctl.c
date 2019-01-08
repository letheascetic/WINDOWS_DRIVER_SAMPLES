
#include "private.h"
#include "ioctl.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, DeviceIoControlParallel)
#pragma alloc_text (PAGE, DeviceIoControlSerial)
#endif

#define CY001_LOAD_REQUEST    0xA0
#define MCU_RESET_REG  0xE600

// 并行处理
VOID DeviceIoControlParallel(IN WDFQUEUE  Queue,
	IN WDFREQUEST  Request,
	IN size_t  OutputBufferLength,
	IN size_t  InputBufferLength,
	IN ULONG  IoControlCode)
{
	NTSTATUS status = STATUS_SUCCESS;
	ULONG ulRetLen = 0;
	size_t size = 0;
	void* pBufferInput = NULL;
	void* pBufferOutput = NULL;
	WDFDEVICE Device = NULL;			
	PDEVICE_CONTEXT pContext = NULL;

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "--> DeviceIoControlParallel CtlCode:0x%0.8X\n", IoControlCode);

	Device = WdfIoQueueGetDevice(Queue);			// 取得设备句柄
	pContext = GetDeviceContext(Device);			// 取得WDF设备对象的环境块指针

	// 取得输入缓冲区，判断其有效性
	if (InputBufferLength) {
		status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &pBufferInput, &size);
		if (status != STATUS_SUCCESS || pBufferInput == NULL || size < InputBufferLength) {
			WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
			return;
		}
	}

	// 取得输出缓冲区，判断其有效性
	if (OutputBufferLength) {
		status = WdfRequestRetrieveOutputBuffer(Request, OutputBufferLength, &pBufferOutput, &size);
		if (status != STATUS_SUCCESS || pBufferOutput == NULL || size < OutputBufferLength) {
			WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
			return;
		}
	}

	//
	// 下面是主处理过程。
	//
	switch (IoControlCode)
	{
	// 取得驱动的版本信息
	case IOCTL_GET_DRIVER_VERSION:
	{
		PDRIVER_VERSION pVersion = (PDRIVER_VERSION)pBufferOutput;
		ULONG length;
		char tcsBuffer[120];
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_GET_DRIVER_VERSION");

		if (OutputBufferLength < sizeof(DRIVER_VERSION)) {
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		pVersion->DriverType = DR_WDF;
		pVersion->FirmwareType = FW_NOT_CY001;
		ulRetLen = sizeof(DRIVER_VERSION);		// 告示返回长度

		// 根据String描述符，判断Firmware代码是否已经被加载。
		GetStringDes(2, 0, tcsBuffer, 120, &length, pContext);

		if (length) {
			WCHAR* pCyName = L"CY001 V";
			size_t len;
			int nIndex;

			if (length < 8)
				break;

			RtlStringCchLengthW(pCyName, 7, &len);
			for (nIndex = 0; nIndex < len; nIndex++) {
				if (pCyName[nIndex] != ((WCHAR*)tcsBuffer)[nIndex])
					break;
			}

			if (nIndex == len)
				pVersion->FirmwareType = FW_CY001;	// 完全相符，说明新版Firmware已经加载到开发板。
		}
		break;
	}

	// 收到App发送过来的一个同步Request，我们应该把它保存到同步Queue中，等到有同步事件发生的时候再从Queue中取出并完成。
	case IOCTL_USB_SYNC:
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_SYNC");
		status = WdfRequestForwardToIoQueue(Request, pContext->AppSyncManualQueue);

		// 直接返回，不调用WdfRequestComplete函数。
		// 请求者将不会为此而等待；请求的完成在将来的某个时刻。
		// 这就是所谓的异步处理之要义了。
		if (NT_SUCCESS(status))
			return;
		break;

		// 清空同步队列中的所有请求
	case IOCTL_USB_SYNC_RELEASE:
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_SYNC_RELEASE");
		ClearSyncQueue(Device);
		break;

		// 应用程序退出，取消所有被阻塞的请求。
	case IOCTL_APP_EXIT_CANCEL:
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_APP_EXIT_CANCEL");
		// 取消USB设备的所有IO操作。它将连带取消所有Pipe的IO操作。
		//WdfIoTargetStop(WdfUsbTargetDeviceGetIoTarget(pContext->UsbDevice), WdfIoTargetCancelSentIo);
		break;

		// 取得当前的配置号.总是设置为0,因为在WDF框架中,0以外的配置是不被支持的。
	case IOCTL_USB_GET_CURRENT_CONFIG:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_GET_CURRENT_CONFIG");
		if (InputBufferLength < 4) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		*(PULONG)pBufferInput = 0;// 直接赋值0，即总是选择0号配置。也可以发送URB到总线获取当前配置选项。
		ulRetLen = sizeof(ULONG);
		break;
	}

	case IOCTL_USB_ABORTPIPE:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_ABORTPIPE");
		ULONG pipenum = *((PULONG)pBufferOutput);
		status = AbortPipe(Device, pipenum);
	}
	break;

	// 获取Pipe信息
	case IOCTL_USB_GET_PIPE_INFO:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_GET_PIPE_INFO");
		// 遍历获取Pipe信息，复制到输出缓冲中
		BYTE byCurSettingIndex = 0;
		BYTE byPipeNum = 0;
		BYTE index;
		USB_INTERFACE_DESCRIPTOR  interfaceDescriptor;
		WDF_USB_PIPE_INFORMATION  pipeInfor;

		WDFUSBINTERFACE Interface = pContext->UsbInterface;// 接口句柄

		// 取得Pipe数。根据Pipe数计算缓冲区长度
		byCurSettingIndex = WdfUsbInterfaceGetConfiguredSettingIndex(Interface);
		WdfUsbInterfaceGetDescriptor(Interface, byCurSettingIndex, &interfaceDescriptor);
		byPipeNum = WdfUsbInterfaceGetNumConfiguredPipes(Interface);

		if (OutputBufferLength < byPipeNum * sizeof(pipeInfor)) {
			status = STATUS_BUFFER_TOO_SMALL; // 缓冲区不足
		}
		else {

			ulRetLen = byPipeNum * sizeof(pipeInfor);

			// 遍历获取全部管道信息，拷贝到输出缓冲中。
			// 应用程序得到输出缓冲的时候，也应该使用WDF_USB_PIPE_INFORMATION结构体解析缓冲区。
			for (index = 0; index < byPipeNum; index++)
			{
				WDF_USB_PIPE_INFORMATION_INIT(&pipeInfor);
				WdfUsbInterfaceGetEndpointInformation(Interface, byCurSettingIndex, index, &pipeInfor);
				RtlCopyMemory((PUCHAR)pBufferOutput + index * pipeInfor.Size, &pipeInfor, sizeof(pipeInfor));
			}
		}
	}

	break;

	// 获取设备描述符
	case IOCTL_USB_GET_DEVICE_DESCRIPTOR:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_GET_DEVICE_DESCRIPTOR");
		USB_DEVICE_DESCRIPTOR  UsbDeviceDescriptor;
		WdfUsbTargetDeviceGetDeviceDescriptor(pContext->UsbDevice, &UsbDeviceDescriptor);

		// 判断输入缓冲区的长度是否足够长
		if (OutputBufferLength < UsbDeviceDescriptor.bLength)
			status = STATUS_BUFFER_TOO_SMALL;
		else {
			RtlCopyMemory(pBufferOutput, &UsbDeviceDescriptor, UsbDeviceDescriptor.bLength);
			ulRetLen = UsbDeviceDescriptor.bLength;
		}

		break;
	}

	// 获取字符串描述符
	case IOCTL_USB_GET_STRING_DESCRIPTOR:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_GET_STRING_DESCRIPTOR");
		PGET_STRING_DESCRIPTOR Input = (PGET_STRING_DESCRIPTOR)pBufferInput;
		
		status = GetStringDes(Input->Index, Input->LanguageId, pBufferOutput, OutputBufferLength, &ulRetLen, pContext);

		// 由字符长度调整为字节长度
		if (NT_SUCCESS(status) && ulRetLen > 0)
			ulRetLen *= (sizeof(WCHAR) / sizeof(char));
		break;
	}

	// 获取配置描述信息。
	case IOCTL_USB_GET_CONFIGURATION_DESCRIPTOR:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_GET_CONFIGURATION_DESCRIPTOR");

		// 首先获得配置描述符的长度。
		status = WdfUsbTargetDeviceRetrieveConfigDescriptor(pContext->UsbDevice, NULL, &size);
		if (!NT_SUCCESS(status) && status != STATUS_BUFFER_TOO_SMALL)
			break;

		// 输出缓冲区不够长
		if (OutputBufferLength < size)
			break;

		// 正式取得配置描述符。
		status = WdfUsbTargetDeviceRetrieveConfigDescriptor(pContext->UsbDevice, pBufferOutput, &size);
		if (!NT_SUCCESS(status))
			break;

		ulRetLen = size;
		break;
	}

	// 根据可选值配置接口
	case IOCTL_USB_SET_INTERFACE:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_SETINTERFACE");
		BYTE byAlterSetting = *(BYTE*)pBufferInput;
		WDFUSBINTERFACE usbInterface = pContext->UsbInterface;
		BYTE byCurSetting = WdfUsbInterfaceGetConfiguredSettingIndex(usbInterface); // 当前Alternate值

		if (InputBufferLength < 1 || OutputBufferLength < 1)
		{
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		// 如果传入的可选值与当前的不同，则重新配置接口；
		// 否则直接返回。
		if (byCurSetting != byAlterSetting)
		{
			WDF_USB_INTERFACE_SELECT_SETTING_PARAMS par;
			WDF_USB_INTERFACE_SELECT_SETTING_PARAMS_INIT_SETTING(&par, byAlterSetting);
			status = WdfUsbInterfaceSelectSetting(usbInterface, NULL, &par);
		}

		*(BYTE*)pBufferOutput = byCurSetting;
		break;
	}

	// 固件Rest。自定义命令，与Port Rest是两码事。
	case IOCTL_USB_FIRMWRAE_RESET:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_FIRMWRAE_RESET");
		if (InputBufferLength < 1 || pBufferInput == NULL)
			status = STATUS_INVALID_PARAMETER;
		else
			status = FirmwareReset(Device, *(char*)pBufferInput);

		break;
	}

	// 重置USB总线端口
	case IOCTL_USB_PORT_RESET:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_PORT_RESET");
		WdfUsbTargetDeviceResetPortSynchronously(pContext->UsbDevice);
		break;
	}

	// 管道重置
	case IOCTL_USB_PIPE_RESET:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_PIPE_RESET");
		UCHAR uchPipe;
		WDFUSBPIPE pipe = NULL;

		if (InputBufferLength < 1) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		// 根据ID找到对应的Pipe
		uchPipe = *(UCHAR*)pBufferInput;
		pipe = WdfUsbInterfaceGetConfiguredPipe(pContext->UsbInterface, uchPipe, NULL);
		if (pipe == NULL) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		status = WdfUsbTargetPipeResetSynchronously(pipe, NULL, NULL);
		break;
	}

	// 中断管道，放弃管道当前正在进行的操作
	case IOCTL_USB_PIPE_ABORT:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_PIPE_ABORT");
		UCHAR uchPipe;
		WDFUSBPIPE pipe = NULL;

		if (InputBufferLength < 1) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		// 根据ID找到对应的Pipe
		uchPipe = *(UCHAR*)pBufferInput;
		pipe = WdfUsbInterfaceGetConfiguredPipe(pContext->UsbInterface, uchPipe, NULL);
		if (pipe == NULL) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		status = WdfUsbTargetPipeAbortSynchronously(pipe, NULL, NULL);
		break;
	}

	// 取得驱动错误信息，驱动总是把最后一次发现的错误保存在设备对象的环境块中。
	// 这个逻辑虽然实现了，但目前的版本中，应用程序并没有利用这个接口。
	case IOCTL_USB_GET_LAST_ERROR:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_GET_LAST_ERROR");
		if (OutputBufferLength >= sizeof(ULONG))
			*((PULONG)pBufferOutput) = pContext->LastUSBErrorStatusValue;
		else
			status = STATUS_BUFFER_TOO_SMALL;

		ulRetLen = sizeof(ULONG);
		break;
	}

	// Clear feature命令
	case IOCTL_USB_SET_CLEAR_FEATURE:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_SET_CLEAR_FEATURE");
		status = UsbSetOrClearFeature(Device, Request);
		break;
	}

	// 为USB设备加载固件程序。带有偏移量参数，用这个分支；不带偏移量，可用下一个分支。
	// 带偏移量的情况下，固件代码是一段一段地加载；
	// 不带偏移量的情况，固件代码作为一整块一次性被加载。
	case IOCTL_FIRMWARE_UPLOAD_OFFSET:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_FIRMWARE_UPLOAD_OFFSET");
		void* pData = pBufferOutput;
		WORD offset = 0;

		if (InputBufferLength < sizeof(WORD)) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		offset = *(WORD*)pBufferInput;
		status = FirmwareUpload(WdfIoQueueGetDevice(Queue), pData, OutputBufferLength, offset);
		break;
	}

	// 为USB设备加载固件程序。
	case IOCTL_FIRMWARE_UPLOAD:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_FIRMWARE_UPLOAD");
		void* pData = pBufferOutput;
		status = FirmwareUpload(WdfIoQueueGetDevice(Queue), pData, InputBufferLength, 0);
		break;
	}

	// 读取开发板设备的RAM内容。RAM也就是内存。
	// 每次从同一地址读取的内容可能不尽相同，开发板中固件程序在不断运行，RAM被用来储数据（包括临时数据）。
	case IOCTL_FIRMWARE_READ_RAM:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_FIRMWARE_READ_RAM");
		status = ReadRAM(WdfIoQueueGetDevice(Queue), Request, &ulRetLen);// inforVal中保存读取的长度
		break;
	}

	// 其他的请求
	default:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_DEFAULT");
		// 一律转发到SerialQueue中去。			
		WdfRequestForwardToIoQueue(Request, pContext->IoControlSerialQueue);

		// 命令转发之后，这里必须直接返回，千万不可调用WdfRequestComplete函数。
		// 否则会导致一个Request被完成两次的错误。
		return;
	}
	}

	// 完成请求
	WdfRequestCompleteWithInformation(Request, status, ulRetLen);
}

// 这里面的IO操作是经过序列化的。一个挨着一个，所以绝不会发生重入问题。
//
VOID DeviceIoControlSerial(IN WDFQUEUE  Queue,
	IN WDFREQUEST  Request,
	IN size_t  OutputBufferLength,
	IN size_t  InputBufferLength,
	IN ULONG  IoControlCode)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	ULONG ulRetLen = 0;
	SIZE_T size;

	void* pBufferInput = NULL;
	void* pBufferOutput = NULL;
	WDFDEVICE Device = WdfIoQueueGetDevice(Queue);// 取得设备句柄
	PDEVICE_CONTEXT pContext = GetDeviceContext(Device); // 取得WDF设备对象的环境块指针

	//KDBG(DPFLTR_INFO_LEVEL, "[DeviceIoControlSerial]");

	// 取得输入/输出缓冲区
	if (InputBufferLength)WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &pBufferInput, &size);
	if (OutputBufferLength)WdfRequestRetrieveOutputBuffer(Request, OutputBufferLength, &pBufferOutput, &size);

	/*
	switch (IoControlCode)
	{
	// 设置数码管
	case IOCTL_USB_SET_DIGITRON:
	{
		CHAR ch = *(CHAR*)pBufferInput;
		KDBG(DPFLTR_INFO_LEVEL, "IOCTL_USB_SET_DIGITRON");
		SetDigitron(Device, ch);
		break;
	}

	// 读数码管
	case IOCTL_USB_GET_DIGITRON:
	{
		UCHAR* pCh = (UCHAR*)pBufferOutput;
		KDBG(DPFLTR_INFO_LEVEL, "IOCTL_USB_GET_DIGITRON");
		GetDigitron(Device, pCh);
		ulRetLen = 1;
		break;
	}

	// 设置LED灯（共4盏）
	case IOCTL_USB_SET_LEDs:
	{
		CHAR ch = *(CHAR*)pBufferInput;
		KDBG(DPFLTR_INFO_LEVEL, "IOCTL_USB_SET_LEDs");
		SetLEDs(Device, ch);
		break;
	}

	// 读取LED灯（共4盏）的当前状态
	case IOCTL_USB_GET_LEDs:
	{
		UCHAR* pCh = (UCHAR*)pBufferOutput;
		KDBG(DPFLTR_INFO_LEVEL, "IOCTL_USB_GET_LEDs");
		GetLEDs(Device, pCh);
		ulRetLen = 1;
		break;
	}

	// 控制命令。
	// 分为：USB协议预定义命令、Vendor自定义命令、特殊类(class)命令。
	case IOCTL_USB_CTL_REQUEST:
	{
		KDBG(DPFLTR_INFO_LEVEL, "IOCTL_USB_CTL_REQUEST");
		ntStatus = UsbControlRequest(Device, Request);
		if (NT_SUCCESS(ntStatus))return;
		break;
	}

	// 开启中断读
	case IOCTL_START_INT_READ:
		KDBG(DPFLTR_INFO_LEVEL, "IOCTL_START_INT_READ");
		ntStatus = InterruptReadStart(Device);
		break;

		// 控制程序发送读请求。它们是被阻塞的，放至Queue中排队，所以不要即可完成他们。
	case IOCTL_INT_READ_KEYs:
		KDBG(DPFLTR_INFO_LEVEL, "IOCTL_INT_READ_KEYs");
		ntStatus = WdfRequestForwardToIoQueue(Request, pContext->InterruptManualQueue);

		if (NT_SUCCESS(ntStatus))
			return;// 成功，直接返回;异步完成。
		break;

		// 终止中断读
	case IOCTL_STOP_INT_READ:
		KDBG(DPFLTR_INFO_LEVEL, "IOCTL_STOP_INT_READ");
		InterruptReadStop(Device);
		ntStatus = STATUS_SUCCESS;
		break;

	default:
		// 不应该到这里。
		// 对于不能识别的IO控制命令，这里做错误处理。
		KDBG(DPFLTR_INFO_LEVEL, "Unknown Request: %08x(%d)!!!", IoControlCode, IoControlCode);
		ntStatus = STATUS_INVALID_PARAMETER;
		break;
	}
	*/

	WdfRequestCompleteWithInformation(Request, ntStatus, ulRetLen);
	return;
}


NTSTATUS GetStringDes(USHORT shIndex, USHORT shLanID, VOID* pBufferOutput, ULONG OutputBufferLength, ULONG* pulRetLen, PDEVICE_CONTEXT pContext)
{
	NTSTATUS status;
	USHORT  numCharacters;
	PUSHORT  stringBuf;
	WDFMEMORY  memoryHandle;

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "--> GetStringDes index:%d", shIndex);

	ASSERT(pulRetLen);
	ASSERT(pContext);
	*pulRetLen = 0;

	// 由于String描述符是一个变长字符数组，故首先取得其长度
	status = WdfUsbTargetDeviceQueryString(
		pContext->UsbDevice,
		NULL,
		NULL,
		NULL, // 传入空字符串
		&numCharacters,
		shIndex,
		shLanID
	);
	if (!NT_SUCCESS(status))
		return status;

	// 判读缓冲区的长度
	if (OutputBufferLength < numCharacters) {
		status = STATUS_BUFFER_TOO_SMALL;
		return status;
	}

	// 再次正式地取得String描述符
	status = WdfUsbTargetDeviceQueryString(pContext->UsbDevice,
		NULL,
		NULL,
		(PUSHORT)pBufferOutput,// Unicode字符串
		&numCharacters,
		shIndex,
		shLanID
	);

	// 完成操作
	if (NT_SUCCESS(status)) {
		((PUSHORT)pBufferOutput)[numCharacters] = L'\0';// 手动在字符串末尾添加NULL
		*pulRetLen = numCharacters + 1;
	}
	return status;
}


NTSTATUS UsbSetOrClearFeature(WDFDEVICE Device, WDFREQUEST Request)
{
	NTSTATUS status;
	WDFREQUEST Request_New = NULL;
	WDF_USB_CONTROL_SETUP_PACKET controlPacket;
	PSET_FEATURE_CONTROL pFeaturePacket;

	TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTL, "--> UsbSetOrClearFeature");

	status = WdfRequestRetrieveInputBuffer(Request, sizeof(SET_FEATURE_CONTROL), &(void*)pFeaturePacket, NULL);
	if (!NT_SUCCESS(status))return status;

	WDF_USB_CONTROL_SETUP_PACKET_INIT_FEATURE(&controlPacket,
		BmRequestToDevice,
		pFeaturePacket->FeatureSelector,
		pFeaturePacket->Index,
		pFeaturePacket->bSetOrClear
	);

	status = WdfRequestCreate(NULL, NULL, &Request_New);
	if (!NT_SUCCESS(status)) {
		//KDBG(DPFLTR_ERROR_LEVEL, "WdfRequestCreate Failed: 0x%0.8X", status);
		return status;
	}

	WdfUsbTargetDeviceFormatRequestForControlTransfer(
		GetDeviceContext(Device)->UsbDevice,
		Request_New,
		&controlPacket,
		NULL, NULL);

	if (FALSE == WdfRequestSend(Request_New, WdfDeviceGetIoTarget(Device), NULL))
		status = WdfRequestGetStatus(Request_New);
	WdfObjectDelete(Request_New);

	return status;
}

// 把一段二进制的固件代码写入开发板指定地址处。
//
NTSTATUS FirmwareUpload(WDFDEVICE Device, PUCHAR pData, ULONG ulLen, WORD offset)
{
	NTSTATUS ntStatus;
	WDF_USB_CONTROL_SETUP_PACKET controlPacket;
	ULONG chunkCount = 0;
	ULONG ulWritten;
	WDF_MEMORY_DESCRIPTOR memDescriptor;
	WDF_OBJECT_ATTRIBUTES attributes;
	PDEVICE_CONTEXT Context = GetDeviceContext(Device);
	int i;

	chunkCount = ((ulLen + CHUNK_SIZE - 1) / CHUNK_SIZE);

	// 为安全起见，下载过程中，大块数据被分割成以64字节为单位的小块进行发送。
	// 如果以大块进行传递，可能会发生数据丢失的情况。
	//
	for (i = 0; i < chunkCount; i++)
	{
		// 构造内存描述符
		WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memDescriptor, pData, (i < chunkCount - 1) ?
			CHUNK_SIZE :
			(ulLen - (chunkCount - 1) * CHUNK_SIZE));// 如果不是最后一个块，则CHUNK_SIZE字节；否则要计算尾巴长度。

		// 初始化控制命令
		WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(
			&controlPacket,
			BmRequestHostToDevice,
			BmRequestToDevice,
			CY001_LOAD_REQUEST,			// Vendor 命令A3
			offset + i * CHUNK_SIZE,    // 写入起始地址
			0);

		ntStatus = WdfUsbTargetDeviceSendControlTransferSynchronously(
			Context->UsbDevice,
			NULL, NULL,
			&controlPacket,
			&memDescriptor,
			&ulWritten);

		if (!NT_SUCCESS(ntStatus)) {
			//KDBG(DPFLTR_ERROR_LEVEL, "FirmwareUpload Failed :0x%0.8x!!!", ntStatus);
			break;
		}
		else
			//KDBG(DPFLTR_INFO_LEVEL, "%d bytes are written.", ulWritten);

		pData += CHUNK_SIZE;
	}

	return ntStatus;
}


NTSTATUS FirmwareReset(IN WDFDEVICE Device, IN UCHAR resetBit)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_USB_CONTROL_SETUP_PACKET controlPacket;
	WDF_MEMORY_DESCRIPTOR memDescriptor;
	PDEVICE_CONTEXT Context = GetDeviceContext(Device);

	//KDBG(DPFLTR_INFO_LEVEL, "[FirmwareReset]");

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memDescriptor, &resetBit, 1);

	// 写地址MCU_RESET_REG
	WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(
		&controlPacket,
		BmRequestHostToDevice,
		BmRequestToDevice,
		CY001_LOAD_REQUEST,// Vendor命令
		MCU_RESET_REG,	   // 指定地址
		0);

	status = WdfUsbTargetDeviceSendControlTransferSynchronously(
		Context->UsbDevice,
		NULL, NULL,
		&controlPacket,
		&memDescriptor,
		NULL);

	if (!NT_SUCCESS(status))
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "FirmwareReset failed: 0x%X!!!", status);

	return status;
}

// 通过管道Item，可以得到管道句柄。调用Abort方法，中止Pipe上的数据流。
NTSTATUS AbortPipe(IN WDFDEVICE Device, IN ULONG nPipeNum)
{
	NTSTATUS status;
	PDEVICE_CONTEXT Context = GetDeviceContext(Device);
	WDFUSBINTERFACE Interface = Context->UsbInterface;
	WDFUSBPIPE pipe = WdfUsbInterfaceGetConfiguredPipe(Interface, nPipeNum, NULL);

	//KDBG(DPFLTR_INFO_LEVEL, "[AbortPipe]");

	if (pipe == NULL)
		return STATUS_INVALID_PARAMETER;// 可能nPipeNum太大了

	status = WdfUsbTargetPipeAbortSynchronously(pipe, NULL, NULL);
	if (!NT_SUCCESS(status))
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "AbortPipe Failed: 0x%0.8X", status);

	return status;
}


// 从开发板内存的的指定地址处读取当前内容
//
NTSTATUS ReadRAM(WDFDEVICE Device, WDFREQUEST Request, ULONG* pLen)
{
	NTSTATUS ntStatus;
	WDF_USB_CONTROL_SETUP_PACKET controlPacket;
	WDFMEMORY   hMem = NULL;
	PDEVICE_CONTEXT Context = GetDeviceContext(Device);
	PFIRMWARE_UPLOAD pUpLoad = NULL;
	WDFREQUEST newRequest;
	void* pData = NULL;
	size_t size;

	//KDBG(DPFLTR_INFO_LEVEL, "[ReadRAM]");

	ASSERT(pLen);
	*pLen = 0;

	if (!NT_SUCCESS(WdfRequestRetrieveInputBuffer(Request, sizeof(FIRMWARE_UPLOAD), &(void*)pUpLoad, NULL)) ||
		!NT_SUCCESS(WdfRequestRetrieveOutputBuffer(Request, 1, &pData, &size)))
	{
		//KDBG(DPFLTR_ERROR_LEVEL, "Failed to retrieve memory handle\n");
		return STATUS_INVALID_PARAMETER;
	}

	// 构造内存描述符
	ntStatus = WdfMemoryCreatePreallocated(WDF_NO_OBJECT_ATTRIBUTES, pData, min(size, pUpLoad->len), &hMem);
	if (!NT_SUCCESS(ntStatus))
		return ntStatus;

	// 初始化控制命令
	WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(
		&controlPacket,
		BmRequestDeviceToHost,// input命令
		BmRequestToDevice,
		CY001_LOAD_REQUEST,// Vendor 命令A0
		pUpLoad->addr,// 地址
		0);

	// 创建被初始化WDF REQUEST对象。
	ntStatus = WdfRequestCreate(NULL, NULL, &newRequest);
	if (!NT_SUCCESS(ntStatus))
		return ntStatus;

	WdfUsbTargetDeviceFormatRequestForControlTransfer(Context->UsbDevice,
		newRequest, &controlPacket,
		hMem, NULL);

	if (NT_SUCCESS(ntStatus))
	{
		WDF_REQUEST_SEND_OPTIONS opt;
		WDF_REQUEST_SEND_OPTIONS_INIT(&opt, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
		if (WdfRequestSend(newRequest, WdfDeviceGetIoTarget(Device), &opt))
		{
			WDF_REQUEST_COMPLETION_PARAMS par;
			WDF_REQUEST_COMPLETION_PARAMS_INIT(&par);
			WdfRequestGetCompletionParams(newRequest, &par);

			// 取得读取到的字符长度。
			*pLen = par.Parameters.Usb.Completion->Parameters.DeviceControlTransfer.Length;
		}
	}

	// 通过WdfXxxCreate创建的对象，必须删除
	WdfObjectDelete(newRequest);

	return ntStatus;
}