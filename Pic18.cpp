/*
 * Copyright (C) 2017 Johan Bergkvist
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#include "stdio.h"
#include "time.h"
#include "windows.h"
#include "Usb.h"
#include "HexFile.h"

#define READBYTES			0x00
#define	PROGRAMBYTES		0x01
#define	PROGRAMCONFIGBYTE	0x02
#define ERASE				0x03
#define VDDON				0x04
#define VPPON				0x05
#define VPPVDDOFF			0x06

//===========================================================================
//
// Name    : ReceiveOk18
//
// Desc    : .
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool ReceiveOk18 ()
{
	unsigned char response[64];
	int bytes_received = 64;

	if (!Receive(response, &bytes_received))
	{
		printf("*** Waiting for an OK, but nothing came.\n");
		return false;
	}
	if (bytes_received != 2)
	{
		printf("*** Waiting for an OK, but got %i bytes rather than 2.\n", bytes_received);
		return false;
	}

	if (response[0] != 'O' || response[1] != 'K')
	{
		printf("*** Waiting for an OK, but got \"%c%c\".\n", response[0], response[1]);
		return false;
	}

	return true;
}

//===========================================================================
//
// Name    : VddOn
//
// Desc    : Turns on Vdd (+5V) on the target PIC.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool VddOn ()
{
	unsigned char command[] = {VDDON};

	return Send(command, 1) && ReceiveOk18();
}

//===========================================================================
//
// Name    : VppOn
//
// Desc    : Turns on Vpp (+12V) on the target PIC.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool VppOn ()
{
	unsigned char command[] = {VPPON};

	return Send(command, 1) && ReceiveOk18();
}

//===========================================================================
//
// Name    : VppVddOff
//
// Desc    : Turns off Vpp and Vdd. Vdd must be turned off after Vpp, but no
//           longer than 100ns after. In reality we have no choice but to turn
//           them off at the same time.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool VppVddOff ()
{
	unsigned char command[] = {VPPVDDOFF};

	return Send(command, 1) && ReceiveOk18();
}

//===========================================================================
//
// Name    : ReadBytes
//
// Desc    : Reads "length" bytes from the target PIC into "buffer" starting
//           at address "address".
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool ReadBytes (unsigned int address, unsigned char *buffer, int length)
{
	unsigned char command[] = {
		READBYTES,
		static_cast<unsigned char>((address & 0x00ff0000) >> 16),
		static_cast<unsigned char>((address & 0x0000ff00) >> 8),
		static_cast<unsigned char>((address & 0x000000ff) >> 0),
		static_cast<unsigned char>(length)
	};
	
	if (!Send(command, 5))
	{
		return false;
	}

	int bytes_received = length;
	if (!Receive(buffer, &bytes_received))
	{
		return false;
	}

	return true;
}

//===========================================================================
//
// Name    : ProgramBytes
//
// Desc    : Writes "length" bytes to the target PIC from "buffer" starting
//           at address "address".
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool ProgramBytes (unsigned int address, unsigned char *buffer, int length)
{
	unsigned char command[20] = {
		PROGRAMBYTES,
		static_cast<unsigned char>((address & 0x00ff0000) >> 16),
		static_cast<unsigned char>((address & 0x0000ff00) >> 8),
		static_cast<unsigned char>((address & 0x000000ff) >> 0)
	};
	memcpy(&command[4], buffer, length);

	return Send(command, length + 4) && ReceiveOk18();
}

//===========================================================================
//
// Name    : ProgramConfigByte
//
// Desc    : Write configuration byte "byte" into address "address". Address
//           must be in the range 0x300000 to 0x30000d inclusive.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool ProgramConfigByte (unsigned int address, unsigned char byte)
{
	unsigned char command[] = {
		PROGRAMCONFIGBYTE,
		static_cast<unsigned char>((address & 0x000000ff) >> 0),
		byte
	};
	
	return Send(command, 3) && ReceiveOk18();
}

//===========================================================================
//===========================================================================

//===========================================================================
//
// Name    : DumpDevice
//
// Desc    : Write the contents of "length" bytes from address "address".
//           "Length" should be a multiple of 16.
//
// Returns : True if successfull, false otherwise.
//
//===========================================================================
bool DumpDevice18(int address, int length)
{
	unsigned char buffer[16];

	for (int i = 0; i < length; i += 16)
	{
		if (!ReadBytes  (address + i, buffer, 16))
		{
			return false;
		}

		printf ("%06x : %02x %02x %02x %02x   %02x %02x %02x %02x   %02x %02x %02x %02x   %02x %02x %02x %02x\n",
				address + i,
				buffer[0],
				buffer[1],
				buffer[2],
				buffer[3],
				buffer[4],
				buffer[5],
				buffer[6],
				buffer[7],
				buffer[8],
				buffer[9],
				buffer[10],
				buffer[11],
				buffer[12],
				buffer[13],
				buffer[14],
				buffer[15]);
	}
	
	return true;
}

//===========================================================================
//
// Name    : Erase18
//
// Desc    : 
//
// Returns : Nothing.
//
//===========================================================================
void Erase18()
{
	VddOn ();
	VppOn ();

	//
	// Wait for VPP to settle. The MAX680 takes a little while.
	//
	Sleep(100);

	unsigned char command[] = {ERASE};

	if (Send(command, 1) && ReceiveOk18())
	{
		printf ("Erased!\n");
	}

	VppVddOff ();
}

//===========================================================================
//
// Name    : Program18
//
// Desc    : Writes the contents of the "memory_segments" array to the device.
//
// Returns : Nothing.
//
//===========================================================================
void Program18()
{
	VddOn ();
	VppOn ();

	//
	// Wait for VPP to settle. The MAX680 takes a little while.
	//
	Sleep(100);

	do
	{
		//
		// Program...
		//
		for (int seg = 0; seg < g_number_of_segments; seg++)
		{
//				printf("\n(%i/%i, %08x, %04x) ", seg, segments, memory_segment[seg].address, memory_segment[seg].length);
			if (g_memory_segment[seg].address < 0x300000)
			{
				//
				// Make sure there are no single bytes as the programmer doesn't like them.
				//
				int length = g_memory_segment[seg].length;
				if (length == 1)
				{
					g_memory_segment[seg].bytes[1] = 0xff;
					length = 2;
				}
				if (!ProgramBytes (g_memory_segment[seg].address,
									g_memory_segment[seg].bytes,
									length))
				{
					break;
				}
			}
			else if (g_memory_segment[seg].address < 0x3ffffe)
			{
				//
				// CONFIG words will be programmed last...
				//
			}
			else
			{
				printf ("ERROR: EEPROM data not supported!\n");
			}
		}

		for (int seg = 0; seg < g_number_of_segments; seg++)
		{
			if (0x300000 <= g_memory_segment[seg].address
				&& g_memory_segment[seg].address < 0x30000d)
			{
				for (int i = 0; i < g_memory_segment[seg].length; i++)
				{
					if (!ProgramConfigByte (g_memory_segment[seg].address + i,
										g_memory_segment[seg].bytes[i]))
					{
						break;
					}
				}
			}
		}

		//
		// Verify...
		//
		bool error_found = false;
		for (int seg = 0; seg < g_number_of_segments && !error_found; seg++)
		{
			unsigned char buffer[MAX_SEGMENT_LENGTH];

//			printf("\n(%i/%i, %08x, %04x) ", seg, segments, memory_segment[seg].address, memory_segment[seg].length);
			if (!ReadBytes  (g_memory_segment[seg].address, buffer, g_memory_segment[seg].length))
			{
				break;
			}
/*			for (int i = 0; i < memory_segment[seg].length; i++)
			{
				printf ("%02x (%02x) ",
						memory_segment[seg].bytes[i],
						buffer[i]);
			}
*/
			for (int i = 0; i < g_memory_segment[seg].length; i++)
			{
				if (buffer[i] != g_memory_segment[seg].bytes[i]
					&& g_memory_segment[seg].address + i != 0x00300004  // Unused configuration byte. Ignore.
					&& g_memory_segment[seg].address + i != 0x00300007) // Unused configuration byte. Ignore.
				{
					printf ("\n*** Verification Error: Byte %06x should be %02x, but reads as %02x.\n",
							g_memory_segment[seg].address + i,
							g_memory_segment[seg].bytes[i],
							buffer[i]);
					//error_found = true;
					break;
				}
			}
		}

		if (!error_found) {
			printf ("Programmed!\n");
		}
	}
	while(0);

	VppVddOff ();
}

//===========================================================================
//
// Name    : ReadDeviceId18
//
// Desc    : 
//
// Returns : Nothing.
//
//===========================================================================
void ReadDeviceId18()
{
	VddOn ();
	VppOn ();

	//
	// Wait for VPP to settle. The MAX680 takes a little while.
	//
	Sleep(100);

	unsigned char buffer[2];

	if (ReadBytes (0x3ffffe, buffer, 2))
	{
		unsigned short int word = buffer[0] | (buffer[1] << 8);

		printf ("Dev_ID : 0x%02x%02x", buffer[1], buffer[0]);
		switch (word & 0xffe0)
		{
		case 0x1200: printf(", a 18F4550 rev %i", word & 0x001f); break;
		case 0x1220: printf(", a 18F4450 rev %i", word & 0x001f); break;
		case 0x1240: printf(", a 18F2550 rev %i", word & 0x001f); break;
		case 0x1260: printf(", a 18F2450 rev %i", word & 0x001f); break;
		case 0x5c00: printf(", a 18F45K50 rev %i", word & 0x001f); break;
		case 0x5c20: printf(", a 18F25K50 rev %i", word & 0x001f); break;
		case 0x5c60: printf(", a 18F24K50 rev %i", word & 0x001f); break;
		case 0x5d20: printf(", a 18F26K50 rev %i", word & 0x001f); break;
		case 0x5d60: printf(", a 18F46K50 rev %i", word & 0x001f); break;
		case 0x1e00: printf(", a 18F1230 rev %i", word & 0x001f); break;
		case 0x1e20: printf(", a 18F1330 rev %i", word & 0x001f); break;
		case 0x1ee0: printf(", a 18F1330-ICD rev %i", word & 0x001f); break;
		default: printf(", an unknown part"); break;
		}
		printf(".\n");
	}

	VppVddOff ();
}

//===========================================================================
//
// Name    : DumpDevice18
//
// Desc    : 
//
// Returns : Nothing.
//
//===========================================================================
void DumpDevice18()
{
	VddOn ();
	VppOn ();

	//
	// Wait for VPP to settle. The MAX680 takes a little while.
	//
	Sleep(100);

	DumpDevice18(0x000000, 0x400);
	DumpDevice18(0x000800, 0x1000);
	printf ("\nUser ID words:\n");
	DumpDevice18(0x200000, 0x10);   // User ID words.
	printf ("\nConfiguration words:\n");
	DumpDevice18(0x300000, 0x10);   // Configuration words.
	printf ("\nDevice ID words:\n");
	DumpDevice18(0x3ffff0, 0x10);   // Device ID words.

	VppVddOff ();
}
