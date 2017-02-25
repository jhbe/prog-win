/*
 * Copyright (C) 2017 Johan Bergkvist
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#include <stdio.h>
#include <windows.h>
#include <WinUsb.h>
#include <Setupapi.h>
#include <Usb100.h>

DEFINE_GUID (GUID_PROG_DEVICE_INTERFACE_CLASS, 0xb35924d6, 0x3e16, 0x4a9e, 0x97, 0x82, 0x55, 0x24, 0xa4, 0xb7, 0x9b, 0xe0);

HANDLE handle;
WINUSB_INTERFACE_HANDLE usb_handle;
UCHAR out_pipe;
UCHAR in_pipe;

extern bool g_verbose;
bool g_print_txrx = false;

//===========================================================================
//
// Name    : Open
//
// Desc    : Opens communication with the PIC programmer over USB.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool Open()
{
	HDEVINFO hardware_device_info = SetupDiGetClassDevs (&GUID_PROG_DEVICE_INTERFACE_CLASS,
														 NULL,
														 NULL,
														 DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (hardware_device_info == INVALID_HANDLE_VALUE)
	{
		if (g_verbose)
		{
			printf ("*** SetupDiGetClassDevs failed\n");
		}
		return false;
	}
	
	SP_DEVICE_INTERFACE_DATA device_interface_data;
	device_interface_data.cbSize = sizeof (SP_DEVICE_INTERFACE_DATA);

	if (!SetupDiEnumDeviceInterfaces (hardware_device_info,
									 NULL,
									 &GUID_PROG_DEVICE_INTERFACE_CLASS,
									 0,
									 &device_interface_data))
	{
		if (g_verbose)
		{
			printf ("*** SetupDiEnumDeviceInterfaces failed\n");
		}
		else
		{
			printf ("*** Can't find the programmer. Is it connected?\n");
		}
		return false;
	}

	DWORD actual_length, length;
	SetupDiGetDeviceInterfaceDetail (hardware_device_info,
										 &device_interface_data,
										 NULL,
										 0,
										 &actual_length,
										 NULL);

	PSP_DEVICE_INTERFACE_DETAIL_DATA device_interface_detail_data = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc (actual_length);
	device_interface_detail_data->cbSize = sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA);

	if (!SetupDiGetDeviceInterfaceDetail (hardware_device_info,
										 &device_interface_data,
										 device_interface_detail_data,
										 actual_length,
										 &length,
										 NULL))
	{
		if (g_verbose)
		{
			printf ("*** SetupDiGetDeviceInterfaceDetail (2) failed\n");
		}
		return false;
	}

	SetupDiDestroyDeviceInfoList (hardware_device_info);

	handle = CreateFile (device_interface_detail_data->DevicePath,
								 GENERIC_READ | GENERIC_WRITE,
								 FILE_SHARE_WRITE | FILE_SHARE_READ,
								 NULL,
								 OPEN_EXISTING,
								 FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
								 NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		if (g_verbose)
		{
			printf ("*** Can't open \"%s\"\n", device_interface_detail_data->DevicePath);
		}
		else
		{
			printf ("*** Can't find the programmer. Is it connected?\n");
		}
		return false;
	}

	if (!WinUsb_Initialize(handle, &usb_handle))
	{
		if (g_verbose)
		{
			printf ("*** WinUsb_Initialize failed\n");
		}
		return false;
	}

	USB_INTERFACE_DESCRIPTOR interface_descriptor;
	if (!WinUsb_QueryInterfaceSettings(usb_handle, 0, &interface_descriptor))
	{
		if (g_verbose)
		{
			printf ("*** WinUsb_QueryInterfaceSettings failed\n");
		}
		return false;
	}

	for (int i = 0; i < interface_descriptor.bNumEndpoints; i++)
	{
		WINUSB_PIPE_INFORMATION pipe_information;
		if (!WinUsb_QueryPipe(usb_handle, 0, (UCHAR)i, &pipe_information))
		{
			if (g_verbose)
			{
				printf ("*** WinUsb_QueryPipe failed\n");
			}
			return false;
		}

		if (pipe_information.PipeType == UsbdPipeTypeBulk
			&& USB_ENDPOINT_DIRECTION_OUT(pipe_information.PipeId))
		{
			out_pipe = pipe_information.PipeId;
		}
		else if (pipe_information.PipeType == UsbdPipeTypeBulk
				 && USB_ENDPOINT_DIRECTION_IN(pipe_information.PipeId))
		{
			in_pipe = pipe_information.PipeId;
		}
		else
		{
			if (g_verbose)
			{
				printf ("+++ Unexpected pipe %i\n", i);
			}
		}
	}

	free(device_interface_detail_data);

	ULONG timeout_in_milliseconds = 1000;
	if (!WinUsb_SetPipePolicy(usb_handle, out_pipe, PIPE_TRANSFER_TIMEOUT, sizeof(ULONG), &timeout_in_milliseconds))
	{
		if (g_verbose)
		{
			printf ("*** WinUsb_SetPipePolicy(..., out_pipe, PIPE_TRANSFER_TIMEOUT, ...) failed\n");
		}
		return false;
	}
	if (!WinUsb_SetPipePolicy(usb_handle, in_pipe, PIPE_TRANSFER_TIMEOUT, sizeof(ULONG), &timeout_in_milliseconds))
	{
		if (g_verbose)
		{
			printf ("*** WinUsb_SetPipePolicy(..., in_pipe, PIPE_TRANSFER_TIMEOUT, ...) failed\n");
		}
		return false;
	}

	return true;
}

//===========================================================================
//
// Name    : Close
//
// Desc    : Closes the previously opened USB handle.
//
// Returns : Nothing.
//
//===========================================================================
void Close()
{
	if (usb_handle != NULL)
	{
		WinUsb_Free(usb_handle);
		usb_handle = NULL;
	}

	CloseHandle (handle);
}

//===========================================================================
//
// Name    : Send
//
// Desc    : Sends "length" bytes in "buffer" to the programmer over USB.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool Send(unsigned char *buffer, int length)
{
	if (g_print_txrx)
	{
		printf("TX: ");
		for (int i = 0; i < length; i++)
		{
			printf("%02x ", buffer[i]);
		}
		printf("\n");
	}

	ULONG bytes_written;
	if (!WinUsb_WritePipe(usb_handle, out_pipe, buffer, length, &bytes_written, NULL))
	{
		DWORD error = GetLastError();
		switch(error)
		{
		case ERROR_SEM_TIMEOUT:
			if (g_verbose)
			{
				printf("*** WinUsb_WritePipe failed with ERROR_SEM_TIMEOUT.\n");
			}
			break;

		case ERROR_INVALID_HANDLE:
			if (g_verbose)
			{
				printf("*** WinUsb_WritePipe failed with ERROR_INVALID_HANDLE.\n");
			}
			break;

		case ERROR_IO_PENDING:
			if (g_verbose)
			{
				printf("*** WinUsb_WritePipe failed with ERROR_IO_PENDING.\n");
			}
			break;

		case ERROR_NOT_ENOUGH_MEMORY:
			if (g_verbose)
			{
				printf("*** WinUsb_WritePipe failed with ERROR_NOT_ENOUGH_MEMORY.\n");
			}
			break;

		default:
			if (g_verbose)
			{
				printf("*** WinUsb_WritePipe failed with error code %d (0x%08x).\n", (int)error, (unsigned int)error);
			}
			break;
		}

		return false;
	}
	else if (bytes_written != (ULONG)length)
	{
		printf("*** WinUsb_WritePipe worked, but managed to send %i bytes rather then the %i expected.\n", (int)bytes_written, length);
		return false;
	}

	return true;
}

//===========================================================================
//
// Name    : Send
//
// Desc    : Send s a single byte to the programmer over USB.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool Send(unsigned char byte)
{
	return Send(&byte, 1);
}

//===========================================================================
//
// Name    : Receive
//
// Desc    : Receives up to "*length" bytes into "buffer" from the programmer
//           over USB. If successful, "*length" denotes the number of bytes
//           actually received.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool Receive(unsigned char *buffer, int *length)
{
	ULONG bytes_read;

	memset(buffer, 0xcd, *length);

	if (!WinUsb_ReadPipe(usb_handle, in_pipe, buffer, *length, &bytes_read, NULL)
		&& GetLastError() != ERROR_SEM_TIMEOUT)
	{
		*length = 0;
		return false;
	}

	*length = bytes_read;

	if (g_print_txrx)
	{
		printf("RX: ");
		for (int i = 0; i < *length; i++)
		{
			printf("%02x ", buffer[i]);
		}
		printf("\n");
	}

	return true;
}

