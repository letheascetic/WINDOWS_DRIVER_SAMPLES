// Test_CharSample.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <windows.h>
#include <setupapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <winioctl.h>

#include "public.h"

PCHAR
GetDevicePath(
    IN  LPGUID InterfaceGuid
    );

int main(int argc, char* argv[])
{
	PCHAR  DevicePath;
    HANDLE hDevice = INVALID_HANDLE_VALUE;

	printf("Application Test_CharSample starting...\n");

    DevicePath = GetDevicePath((LPGUID)&CharSample_DEVINTERFACE_GUID);

    hDevice = CreateFile(DevicePath,
                         GENERIC_READ|GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,
                         OPEN_EXISTING,
                         0,
                         NULL );

    if (hDevice == INVALID_HANDLE_VALUE) {
		printf("ERROR opening device: (%0x) returned from CreateFile\n", GetLastError());
        return 0;
    }

	printf("OK.\n");

	CHAR	bufInput[1];	// Input to device
	CHAR	bufOutput[2];	// Output from device
	ULONG	nOutput;	// Count written to bufOutput

	printf("«Î ‰»Î ˝◊÷(0-9)\n"); 
l0:	bufInput[0] = _getch();
	if ((bufInput[0]<'0') || (bufInput[0]>'9')) goto l0;
	_putch(bufInput[0]);
   
	// Call device IO Control interface (CharSample_IOCTL_800) in driver
	if (!DeviceIoControl(hDevice,
						 CharSample_IOCTL_800,
						 bufInput,
						 1,
						 bufOutput,
						 2,
						 &nOutput,
						 NULL)
	   )
	{
		printf("ERROR: DeviceIoControl returns %0x.", GetLastError());
        goto exit;
	}
	printf(":"); 
	_putch(bufOutput[0]); 
	_putch(bufOutput[1]); 
	printf("\n");
	
exit:

    if (hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hDevice);
    }
	return 0;
}

PCHAR
GetDevicePath(
    IN  LPGUID InterfaceGuid
    )
{
    HDEVINFO HardwareDeviceInfo;
    SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA DeviceInterfaceDetailData = NULL;
    ULONG Length, RequiredLength = 0;
    BOOL bResult;

    HardwareDeviceInfo = SetupDiGetClassDevs(
                             InterfaceGuid,
                             NULL,
                             NULL,
                             (DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));

    if (HardwareDeviceInfo == INVALID_HANDLE_VALUE) {
        printf("SetupDiGetClassDevs failed!\n");
        exit(1);
    }

    DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    bResult = SetupDiEnumDeviceInterfaces(HardwareDeviceInfo,
                                              0,
                                              InterfaceGuid,
                                              0,
                                              &DeviceInterfaceData);

    if (bResult == FALSE) {
/*
        LPVOID lpMsgBuf;

        if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                          FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_IGNORE_INSERTS,
                          NULL,
                          GetLastError(),
                          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                          (LPSTR) &lpMsgBuf,
                          0,
                          NULL
                          )) {

            printf("Error: %s", (LPSTR)lpMsgBuf);
            LocalFree(lpMsgBuf);
        }
*/
        printf("SetupDiEnumDeviceInterfaces failed.\n");

        SetupDiDestroyDeviceInfoList(HardwareDeviceInfo);
        exit(1);
    }

    SetupDiGetDeviceInterfaceDetail(
        HardwareDeviceInfo,
        &DeviceInterfaceData,
        NULL,
        0,
        &RequiredLength,
        NULL
        );

    DeviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA) LocalAlloc(LMEM_FIXED, RequiredLength);

    if (DeviceInterfaceDetailData == NULL) {
        SetupDiDestroyDeviceInfoList(HardwareDeviceInfo);
        printf("Failed to allocate memory.\n");
        exit(1);
    }

    DeviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    Length = RequiredLength;

    bResult = SetupDiGetDeviceInterfaceDetail(
                  HardwareDeviceInfo,
                  &DeviceInterfaceData,
                  DeviceInterfaceDetailData,
                  Length,
                  &RequiredLength,
                  NULL);

    if (bResult == FALSE) {
/*
        LPVOID lpMsgBuf;

        if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL,
                      GetLastError(),
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPSTR) &lpMsgBuf,
                      0,
                      NULL
                      )) {

            MessageBox(NULL, (LPCTSTR) lpMsgBuf, "Error", MB_OK);
            LocalFree(lpMsgBuf);
        }
*/
        printf("Error in SetupDiGetDeviceInterfaceDetail\n");

        SetupDiDestroyDeviceInfoList(HardwareDeviceInfo);
        LocalFree(DeviceInterfaceDetailData);
        exit(1);
    }

    return DeviceInterfaceDetailData->DevicePath;

}


