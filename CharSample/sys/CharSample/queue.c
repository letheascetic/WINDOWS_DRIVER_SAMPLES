
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
	CHAR	  n,c[]="��һ�����������߰˾�";

    PAGED_CODE();

    switch(IoControlCode) {

    case CharSample_IOCTL_800:
		if (InputBufferLength  == 0 || OutputBufferLength < 2)
		{	//������롢���������Ч��
			WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
		}
		else
		{
			//���뻺������ַ��ͨ������WdfRequestRetrieveInputBuffer�������
			//�����������ַ��ͨ������WdfRequestRetrieveOutputBuffer�������

			//��ȡ���뻺������ַbuffer
			//Ҫ��1�ֽڿռ�
			status = WdfRequestRetrieveInputBuffer(Request, 1, &buffer, NULL);
			if (!NT_SUCCESS(status)) {
				WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
		        break;
			}

			//����buffer��ʾ���뻺������ַ
			//����n=Ӧ�ó��򴫸��������������ASCII��
			n = *(CHAR *)buffer;
			if ((n>='0') && (n<='9'))
			{	//��Ϊ���֣�����
				n-='0';	//n=����(0-9)

				//��ȡ�����������ַbuffer
				status = WdfRequestRetrieveOutputBuffer(Request, 2, &buffer, NULL);
				if (!NT_SUCCESS(status)) {
					WdfRequestComplete(Request, STATUS_UNSUCCESSFUL);
					break;
				}

				//����buffer��ʾ�����������ַ
				//���������������c[]��ȡ����Ӧ�����ֵ������룬���������������
				strncpy((PCHAR)buffer,&c[n*2],2);

				//���I/O�����������򴫸�Ӧ�ó�������ݳ���Ϊ2�ֽڣ�һ�����ģ�
				WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, 2);
			}
			else //���򷵻���Ч����
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

