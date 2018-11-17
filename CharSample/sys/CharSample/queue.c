
#include "private.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, CharSample_EvtIoDeviceControl)
#endif

VOID
CharSample_EvtIoDeviceControl(
    IN WDFQUEUE   Queue,
    IN WDFREQUEST Request,
    IN size_t     OutputBufferLength,
    IN size_t     InputBufferLength,
    IN ULONG      IoControlCode
    )
{
    NTSTATUS  status;
    PVOID	  buffer;
	CHAR	  n,c[]="零一二三四五六七八九";

    PAGED_CODE();

    switch(IoControlCode) {

    case CharSample_IOCTL_800:
		if (InputBufferLength  == 0 || OutputBufferLength < 2)
		{	//检查输入、输出参数有效性
			WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
		}
		else
		{
			//输入缓冲区地址可通过调用WdfRequestRetrieveInputBuffer函数获得
			//输出缓冲区地址可通过调用WdfRequestRetrieveOutputBuffer函数获得

			//获取输入缓冲区地址buffer
			//要求1字节空间
			status = WdfRequestRetrieveInputBuffer(Request, 1, &buffer, NULL);
			if (!NT_SUCCESS(status)) {
				WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
		        break;
			}

			//这里buffer表示输入缓冲区地址
			//输入n=应用程序传给驱动程序的数字ASCII码
			n = *(CHAR *)buffer;
			if ((n>='0') && (n<='9'))
			{	//若为数字，则处理
				n-='0';	//n=数字(0-9)

				//获取输出缓冲区地址buffer
				status = WdfRequestRetrieveOutputBuffer(Request, 2, &buffer, NULL);
				if (!NT_SUCCESS(status)) {
					WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
					break;
				}

				//这里buffer表示输出缓冲区地址
				//输出：从中文数组c[]中取出对应的数字的中文码，拷贝到输出缓冲区
				strncpy((PCHAR)buffer,&c[n*2],2);

				//完成I/O请求，驱动程序传给应用程序的数据长度为2字节（一个中文）
				WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 2);
			}
			else //否则返回无效参数
				WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
		}
        break;

    default :
        status = STATUS_INVALID_DEVICE_REQUEST;
		WdfRequestCompleteWithInformation(Request, status, 0);
        break;
    }

    return;
}

