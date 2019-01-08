
#include "private.h"
#include "ioctl.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, DeviceIoControlParallel)
#pragma alloc_text (PAGE, DeviceIoControlSerial)
#endif

#define CY001_LOAD_REQUEST    0xA0
#define MCU_RESET_REG  0xE600

// ���д���
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

	Device = WdfIoQueueGetDevice(Queue);			// ȡ���豸���
	pContext = GetDeviceContext(Device);			// ȡ��WDF�豸����Ļ�����ָ��

	// ȡ�����뻺�������ж�����Ч��
	if (InputBufferLength) {
		status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &pBufferInput, &size);
		if (status != STATUS_SUCCESS || pBufferInput == NULL || size < InputBufferLength) {
			WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
			return;
		}
	}

	// ȡ��������������ж�����Ч��
	if (OutputBufferLength) {
		status = WdfRequestRetrieveOutputBuffer(Request, OutputBufferLength, &pBufferOutput, &size);
		if (status != STATUS_SUCCESS || pBufferOutput == NULL || size < OutputBufferLength) {
			WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
			return;
		}
	}

	//
	// ��������������̡�
	//
	switch (IoControlCode)
	{
	// ȡ�������İ汾��Ϣ
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
		ulRetLen = sizeof(DRIVER_VERSION);		// ��ʾ���س���

		// ����String���������ж�Firmware�����Ƿ��Ѿ������ء�
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
				pVersion->FirmwareType = FW_CY001;	// ��ȫ�����˵���°�Firmware�Ѿ����ص������塣
		}
		break;
	}

	// �յ�App���͹�����һ��ͬ��Request������Ӧ�ð������浽ͬ��Queue�У��ȵ���ͬ���¼�������ʱ���ٴ�Queue��ȡ������ɡ�
	case IOCTL_USB_SYNC:
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_SYNC");
		status = WdfRequestForwardToIoQueue(Request, pContext->AppSyncManualQueue);

		// ֱ�ӷ��أ�������WdfRequestComplete������
		// �����߽�����Ϊ�˶��ȴ������������ڽ�����ĳ��ʱ�̡�
		// �������ν���첽����֮Ҫ���ˡ�
		if (NT_SUCCESS(status))
			return;
		break;

		// ���ͬ�������е���������
	case IOCTL_USB_SYNC_RELEASE:
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_SYNC_RELEASE");
		ClearSyncQueue(Device);
		break;

		// Ӧ�ó����˳���ȡ�����б�����������
	case IOCTL_APP_EXIT_CANCEL:
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_APP_EXIT_CANCEL");
		// ȡ��USB�豸������IO��������������ȡ������Pipe��IO������
		//WdfIoTargetStop(WdfUsbTargetDeviceGetIoTarget(pContext->UsbDevice), WdfIoTargetCancelSentIo);
		break;

		// ȡ�õ�ǰ�����ú�.��������Ϊ0,��Ϊ��WDF�����,0����������ǲ���֧�ֵġ�
	case IOCTL_USB_GET_CURRENT_CONFIG:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_GET_CURRENT_CONFIG");
		if (InputBufferLength < 4) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		*(PULONG)pBufferInput = 0;// ֱ�Ӹ�ֵ0��������ѡ��0�����á�Ҳ���Է���URB�����߻�ȡ��ǰ����ѡ�
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

	// ��ȡPipe��Ϣ
	case IOCTL_USB_GET_PIPE_INFO:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_GET_PIPE_INFO");
		// ������ȡPipe��Ϣ�����Ƶ����������
		BYTE byCurSettingIndex = 0;
		BYTE byPipeNum = 0;
		BYTE index;
		USB_INTERFACE_DESCRIPTOR  interfaceDescriptor;
		WDF_USB_PIPE_INFORMATION  pipeInfor;

		WDFUSBINTERFACE Interface = pContext->UsbInterface;// �ӿھ��

		// ȡ��Pipe��������Pipe�����㻺��������
		byCurSettingIndex = WdfUsbInterfaceGetConfiguredSettingIndex(Interface);
		WdfUsbInterfaceGetDescriptor(Interface, byCurSettingIndex, &interfaceDescriptor);
		byPipeNum = WdfUsbInterfaceGetNumConfiguredPipes(Interface);

		if (OutputBufferLength < byPipeNum * sizeof(pipeInfor)) {
			status = STATUS_BUFFER_TOO_SMALL; // ����������
		}
		else {

			ulRetLen = byPipeNum * sizeof(pipeInfor);

			// ������ȡȫ���ܵ���Ϣ����������������С�
			// Ӧ�ó���õ���������ʱ��ҲӦ��ʹ��WDF_USB_PIPE_INFORMATION�ṹ�������������
			for (index = 0; index < byPipeNum; index++)
			{
				WDF_USB_PIPE_INFORMATION_INIT(&pipeInfor);
				WdfUsbInterfaceGetEndpointInformation(Interface, byCurSettingIndex, index, &pipeInfor);
				RtlCopyMemory((PUCHAR)pBufferOutput + index * pipeInfor.Size, &pipeInfor, sizeof(pipeInfor));
			}
		}
	}

	break;

	// ��ȡ�豸������
	case IOCTL_USB_GET_DEVICE_DESCRIPTOR:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_GET_DEVICE_DESCRIPTOR");
		USB_DEVICE_DESCRIPTOR  UsbDeviceDescriptor;
		WdfUsbTargetDeviceGetDeviceDescriptor(pContext->UsbDevice, &UsbDeviceDescriptor);

		// �ж����뻺�����ĳ����Ƿ��㹻��
		if (OutputBufferLength < UsbDeviceDescriptor.bLength)
			status = STATUS_BUFFER_TOO_SMALL;
		else {
			RtlCopyMemory(pBufferOutput, &UsbDeviceDescriptor, UsbDeviceDescriptor.bLength);
			ulRetLen = UsbDeviceDescriptor.bLength;
		}

		break;
	}

	// ��ȡ�ַ���������
	case IOCTL_USB_GET_STRING_DESCRIPTOR:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_GET_STRING_DESCRIPTOR");
		PGET_STRING_DESCRIPTOR Input = (PGET_STRING_DESCRIPTOR)pBufferInput;
		
		status = GetStringDes(Input->Index, Input->LanguageId, pBufferOutput, OutputBufferLength, &ulRetLen, pContext);

		// ���ַ����ȵ���Ϊ�ֽڳ���
		if (NT_SUCCESS(status) && ulRetLen > 0)
			ulRetLen *= (sizeof(WCHAR) / sizeof(char));
		break;
	}

	// ��ȡ����������Ϣ��
	case IOCTL_USB_GET_CONFIGURATION_DESCRIPTOR:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_GET_CONFIGURATION_DESCRIPTOR");

		// ���Ȼ�������������ĳ��ȡ�
		status = WdfUsbTargetDeviceRetrieveConfigDescriptor(pContext->UsbDevice, NULL, &size);
		if (!NT_SUCCESS(status) && status != STATUS_BUFFER_TOO_SMALL)
			break;

		// ���������������
		if (OutputBufferLength < size)
			break;

		// ��ʽȡ��������������
		status = WdfUsbTargetDeviceRetrieveConfigDescriptor(pContext->UsbDevice, pBufferOutput, &size);
		if (!NT_SUCCESS(status))
			break;

		ulRetLen = size;
		break;
	}

	// ���ݿ�ѡֵ���ýӿ�
	case IOCTL_USB_SET_INTERFACE:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_SETINTERFACE");
		BYTE byAlterSetting = *(BYTE*)pBufferInput;
		WDFUSBINTERFACE usbInterface = pContext->UsbInterface;
		BYTE byCurSetting = WdfUsbInterfaceGetConfiguredSettingIndex(usbInterface); // ��ǰAlternateֵ

		if (InputBufferLength < 1 || OutputBufferLength < 1)
		{
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		// �������Ŀ�ѡֵ�뵱ǰ�Ĳ�ͬ�����������ýӿڣ�
		// ����ֱ�ӷ��ء�
		if (byCurSetting != byAlterSetting)
		{
			WDF_USB_INTERFACE_SELECT_SETTING_PARAMS par;
			WDF_USB_INTERFACE_SELECT_SETTING_PARAMS_INIT_SETTING(&par, byAlterSetting);
			status = WdfUsbInterfaceSelectSetting(usbInterface, NULL, &par);
		}

		*(BYTE*)pBufferOutput = byCurSetting;
		break;
	}

	// �̼�Rest���Զ��������Port Rest�������¡�
	case IOCTL_USB_FIRMWRAE_RESET:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_FIRMWRAE_RESET");
		if (InputBufferLength < 1 || pBufferInput == NULL)
			status = STATUS_INVALID_PARAMETER;
		else
			status = FirmwareReset(Device, *(char*)pBufferInput);

		break;
	}

	// ����USB���߶˿�
	case IOCTL_USB_PORT_RESET:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_PORT_RESET");
		WdfUsbTargetDeviceResetPortSynchronously(pContext->UsbDevice);
		break;
	}

	// �ܵ�����
	case IOCTL_USB_PIPE_RESET:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_PIPE_RESET");
		UCHAR uchPipe;
		WDFUSBPIPE pipe = NULL;

		if (InputBufferLength < 1) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		// ����ID�ҵ���Ӧ��Pipe
		uchPipe = *(UCHAR*)pBufferInput;
		pipe = WdfUsbInterfaceGetConfiguredPipe(pContext->UsbInterface, uchPipe, NULL);
		if (pipe == NULL) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		status = WdfUsbTargetPipeResetSynchronously(pipe, NULL, NULL);
		break;
	}

	// �жϹܵ��������ܵ���ǰ���ڽ��еĲ���
	case IOCTL_USB_PIPE_ABORT:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_PIPE_ABORT");
		UCHAR uchPipe;
		WDFUSBPIPE pipe = NULL;

		if (InputBufferLength < 1) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		// ����ID�ҵ���Ӧ��Pipe
		uchPipe = *(UCHAR*)pBufferInput;
		pipe = WdfUsbInterfaceGetConfiguredPipe(pContext->UsbInterface, uchPipe, NULL);
		if (pipe == NULL) {
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		status = WdfUsbTargetPipeAbortSynchronously(pipe, NULL, NULL);
		break;
	}

	// ȡ������������Ϣ���������ǰ����һ�η��ֵĴ��󱣴����豸����Ļ������С�
	// ����߼���Ȼʵ���ˣ���Ŀǰ�İ汾�У�Ӧ�ó���û����������ӿڡ�
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

	// Clear feature����
	case IOCTL_USB_SET_CLEAR_FEATURE:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_USB_SET_CLEAR_FEATURE");
		status = UsbSetOrClearFeature(Device, Request);
		break;
	}

	// ΪUSB�豸���ع̼����򡣴���ƫ�����������������֧������ƫ������������һ����֧��
	// ��ƫ����������£��̼�������һ��һ�εؼ��أ�
	// ����ƫ������������̼�������Ϊһ����һ���Ա����ء�
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

	// ΪUSB�豸���ع̼�����
	case IOCTL_FIRMWARE_UPLOAD:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_FIRMWARE_UPLOAD");
		void* pData = pBufferOutput;
		status = FirmwareUpload(WdfIoQueueGetDevice(Queue), pData, InputBufferLength, 0);
		break;
	}

	// ��ȡ�������豸��RAM���ݡ�RAMҲ�����ڴ档
	// ÿ�δ�ͬһ��ַ��ȡ�����ݿ��ܲ�����ͬ���������й̼������ڲ������У�RAM�����������ݣ�������ʱ���ݣ���
	case IOCTL_FIRMWARE_READ_RAM:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_FIRMWARE_READ_RAM");
		status = ReadRAM(WdfIoQueueGetDevice(Queue), Request, &ulRetLen);// inforVal�б����ȡ�ĳ���
		break;
	}

	// ����������
	default:
	{
		TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTL, "IOCTL_DEFAULT");
		// һ��ת����SerialQueue��ȥ��			
		WdfRequestForwardToIoQueue(Request, pContext->IoControlSerialQueue);

		// ����ת��֮���������ֱ�ӷ��أ�ǧ�򲻿ɵ���WdfRequestComplete������
		// ����ᵼ��һ��Request��������εĴ���
		return;
	}
	}

	// �������
	WdfRequestCompleteWithInformation(Request, status, ulRetLen);
}

// �������IO�����Ǿ������л��ġ�һ������һ�������Ծ����ᷢ���������⡣
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
	WDFDEVICE Device = WdfIoQueueGetDevice(Queue);// ȡ���豸���
	PDEVICE_CONTEXT pContext = GetDeviceContext(Device); // ȡ��WDF�豸����Ļ�����ָ��

	//KDBG(DPFLTR_INFO_LEVEL, "[DeviceIoControlSerial]");

	// ȡ������/���������
	if (InputBufferLength)WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &pBufferInput, &size);
	if (OutputBufferLength)WdfRequestRetrieveOutputBuffer(Request, OutputBufferLength, &pBufferOutput, &size);

	/*
	switch (IoControlCode)
	{
	// ���������
	case IOCTL_USB_SET_DIGITRON:
	{
		CHAR ch = *(CHAR*)pBufferInput;
		KDBG(DPFLTR_INFO_LEVEL, "IOCTL_USB_SET_DIGITRON");
		SetDigitron(Device, ch);
		break;
	}

	// �������
	case IOCTL_USB_GET_DIGITRON:
	{
		UCHAR* pCh = (UCHAR*)pBufferOutput;
		KDBG(DPFLTR_INFO_LEVEL, "IOCTL_USB_GET_DIGITRON");
		GetDigitron(Device, pCh);
		ulRetLen = 1;
		break;
	}

	// ����LED�ƣ���4յ��
	case IOCTL_USB_SET_LEDs:
	{
		CHAR ch = *(CHAR*)pBufferInput;
		KDBG(DPFLTR_INFO_LEVEL, "IOCTL_USB_SET_LEDs");
		SetLEDs(Device, ch);
		break;
	}

	// ��ȡLED�ƣ���4յ���ĵ�ǰ״̬
	case IOCTL_USB_GET_LEDs:
	{
		UCHAR* pCh = (UCHAR*)pBufferOutput;
		KDBG(DPFLTR_INFO_LEVEL, "IOCTL_USB_GET_LEDs");
		GetLEDs(Device, pCh);
		ulRetLen = 1;
		break;
	}

	// �������
	// ��Ϊ��USBЭ��Ԥ�������Vendor�Զ������������(class)���
	case IOCTL_USB_CTL_REQUEST:
	{
		KDBG(DPFLTR_INFO_LEVEL, "IOCTL_USB_CTL_REQUEST");
		ntStatus = UsbControlRequest(Device, Request);
		if (NT_SUCCESS(ntStatus))return;
		break;
	}

	// �����ж϶�
	case IOCTL_START_INT_READ:
		KDBG(DPFLTR_INFO_LEVEL, "IOCTL_START_INT_READ");
		ntStatus = InterruptReadStart(Device);
		break;

		// ���Ƴ����Ͷ����������Ǳ������ģ�����Queue���Ŷӣ����Բ�Ҫ����������ǡ�
	case IOCTL_INT_READ_KEYs:
		KDBG(DPFLTR_INFO_LEVEL, "IOCTL_INT_READ_KEYs");
		ntStatus = WdfRequestForwardToIoQueue(Request, pContext->InterruptManualQueue);

		if (NT_SUCCESS(ntStatus))
			return;// �ɹ���ֱ�ӷ���;�첽��ɡ�
		break;

		// ��ֹ�ж϶�
	case IOCTL_STOP_INT_READ:
		KDBG(DPFLTR_INFO_LEVEL, "IOCTL_STOP_INT_READ");
		InterruptReadStop(Device);
		ntStatus = STATUS_SUCCESS;
		break;

	default:
		// ��Ӧ�õ����
		// ���ڲ���ʶ���IO�������������������
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

	// ����String��������һ���䳤�ַ����飬������ȡ���䳤��
	status = WdfUsbTargetDeviceQueryString(
		pContext->UsbDevice,
		NULL,
		NULL,
		NULL, // ������ַ���
		&numCharacters,
		shIndex,
		shLanID
	);
	if (!NT_SUCCESS(status))
		return status;

	// �ж��������ĳ���
	if (OutputBufferLength < numCharacters) {
		status = STATUS_BUFFER_TOO_SMALL;
		return status;
	}

	// �ٴ���ʽ��ȡ��String������
	status = WdfUsbTargetDeviceQueryString(pContext->UsbDevice,
		NULL,
		NULL,
		(PUSHORT)pBufferOutput,// Unicode�ַ���
		&numCharacters,
		shIndex,
		shLanID
	);

	// ��ɲ���
	if (NT_SUCCESS(status)) {
		((PUSHORT)pBufferOutput)[numCharacters] = L'\0';// �ֶ����ַ���ĩβ���NULL
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

// ��һ�ζ����ƵĹ̼�����д�뿪����ָ����ַ����
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

	// Ϊ��ȫ��������ع����У�������ݱ��ָ����64�ֽ�Ϊ��λ��С����з��͡�
	// ����Դ����д��ݣ����ܻᷢ�����ݶ�ʧ�������
	//
	for (i = 0; i < chunkCount; i++)
	{
		// �����ڴ�������
		WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memDescriptor, pData, (i < chunkCount - 1) ?
			CHUNK_SIZE :
			(ulLen - (chunkCount - 1) * CHUNK_SIZE));// ����������һ���飬��CHUNK_SIZE�ֽڣ�����Ҫ����β�ͳ��ȡ�

		// ��ʼ����������
		WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(
			&controlPacket,
			BmRequestHostToDevice,
			BmRequestToDevice,
			CY001_LOAD_REQUEST,			// Vendor ����A3
			offset + i * CHUNK_SIZE,    // д����ʼ��ַ
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

	// д��ַMCU_RESET_REG
	WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(
		&controlPacket,
		BmRequestHostToDevice,
		BmRequestToDevice,
		CY001_LOAD_REQUEST,// Vendor����
		MCU_RESET_REG,	   // ָ����ַ
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

// ͨ���ܵ�Item�����Եõ��ܵ����������Abort��������ֹPipe�ϵ���������
NTSTATUS AbortPipe(IN WDFDEVICE Device, IN ULONG nPipeNum)
{
	NTSTATUS status;
	PDEVICE_CONTEXT Context = GetDeviceContext(Device);
	WDFUSBINTERFACE Interface = Context->UsbInterface;
	WDFUSBPIPE pipe = WdfUsbInterfaceGetConfiguredPipe(Interface, nPipeNum, NULL);

	//KDBG(DPFLTR_INFO_LEVEL, "[AbortPipe]");

	if (pipe == NULL)
		return STATUS_INVALID_PARAMETER;// ����nPipeNum̫����

	status = WdfUsbTargetPipeAbortSynchronously(pipe, NULL, NULL);
	if (!NT_SUCCESS(status))
		TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTL, "AbortPipe Failed: 0x%0.8X", status);

	return status;
}


// �ӿ������ڴ�ĵ�ָ����ַ����ȡ��ǰ����
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

	// �����ڴ�������
	ntStatus = WdfMemoryCreatePreallocated(WDF_NO_OBJECT_ATTRIBUTES, pData, min(size, pUpLoad->len), &hMem);
	if (!NT_SUCCESS(ntStatus))
		return ntStatus;

	// ��ʼ����������
	WDF_USB_CONTROL_SETUP_PACKET_INIT_VENDOR(
		&controlPacket,
		BmRequestDeviceToHost,// input����
		BmRequestToDevice,
		CY001_LOAD_REQUEST,// Vendor ����A0
		pUpLoad->addr,// ��ַ
		0);

	// ��������ʼ��WDF REQUEST����
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

			// ȡ�ö�ȡ�����ַ����ȡ�
			*pLen = par.Parameters.Usb.Completion->Parameters.DeviceControlTransfer.Length;
		}
	}

	// ͨ��WdfXxxCreate�����Ķ��󣬱���ɾ��
	WdfObjectDelete(newRequest);

	return ntStatus;
}