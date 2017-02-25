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

#define READBYTES_16			0x20
#define	PROGRAMBYTES_16			0x21
#define	PROGRAMCONFIGWORD_16	0x22
#define ERASE_16				0x23
#define VDDON_16				0x24
#define VPPON_16				0x25
#define VPPVDDOFF_16			0x26

//===========================================================================
//
// Name    : ReceiveOk16
//
// Desc    : .
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool ReceiveOk16 ()
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
// Name    : VddOn16
//
// Desc    : Turns on Vdd (+5V) on the target PIC.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool VddOn16 ()
{
	unsigned char command[] = {VDDON_16};

	return Send(command, 1) && ReceiveOk16();
}

//===========================================================================
//
// Name    : VppOn16
//
// Desc    : Turns on Vpp (+12V) on the target PIC.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool VppOn16 ()
{
	unsigned char command[] = {VPPON_16};

	return Send(command, 1) && ReceiveOk16();
}

//===========================================================================
//
// Name    : VppVddOff16
//
// Desc    : Turns off Vpp and Vdd. Vdd must be turned off after Vpp, but no
//           longer than 100ns after. In reality we have no choice but to turn
//           them off at the same time.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool VppVddOff16 ()
{
	unsigned char command[] = {VPPVDDOFF_16};

	return Send(command, 1) && ReceiveOk16();
}

//===========================================================================
//
// Name    : ReadBytes16
//
// Desc    : Reads "length" bytes from the target PIC into "buffer" starting
//           at address "address".
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool ReadBytes16 (unsigned int address, unsigned char *buffer, int length)
{
	unsigned char command[] = {
		READBYTES_16,
		static_cast<unsigned char>((address & 0x0000ff00) >> 8),
		static_cast<unsigned char>((address & 0x000000ff) >> 0),
		static_cast<unsigned char>(length)
	};
	
	if (!Send(command, 4))
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
// Name    : ProgramBytes16
//
// Desc    : Writes "length" bytes to the target PIC from "buffer" starting
//           at address "address".
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool ProgramBytes16 (unsigned int address, unsigned char *buffer, int length)
{
	unsigned char command[200] = {
		PROGRAMBYTES_16,
		static_cast<unsigned char>((address & 0x0000ff00) >> 8),
		static_cast<unsigned char>((address & 0x000000ff) >> 0)
	};
	memcpy(&command[3], buffer, length);

	return Send(command, length + 3) && ReceiveOk16();
}

//===========================================================================
//
// Name    : ProgramConfigWord16
//
// Desc    : Write configuration word "word".
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool ProgramConfigWord16 (unsigned short int word)
{
	unsigned char command[] = {
		PROGRAMCONFIGWORD_16,
		static_cast<unsigned char>(word & 0x00ff),
		static_cast<unsigned char>(word >> 8)
	};
	
	return Send(command, 3) && ReceiveOk16();
}

//===========================================================================
//===========================================================================

//===========================================================================
//
// Name    : DumpDevice16
//
// Desc    : Write the contents of "length" bytes from address "address".
//           "words" should be a multiple of 8.
//
// Returns : True if successfull, false otherwise.
//
//===========================================================================
bool DumpDevice16(int address, int words)
{
	unsigned char buffer[16];

	for (int word = 0; word < words; word += 8)
	{
		if (!ReadBytes16 (address + word, buffer, 16))
		{
			return false;
		}

		printf ("%06x : %02x%02x %02x%02x %02x%02x %02x%02x   %02x%02x %02x%02x %02x%02x %02x%02x\n",
				address + word,
				buffer[1],
				buffer[0],
				buffer[3],
				buffer[2],
				buffer[5],
				buffer[4],
				buffer[7],
				buffer[6],
				buffer[9],
				buffer[8],
				buffer[11],
				buffer[10],
				buffer[13],
				buffer[12],
				buffer[15],
				buffer[14]);
	}
	
	return true;
}

//===========================================================================
//
// Name    : Erase16
//
// Desc    : 
//
// Returns : Nothing.
//
//===========================================================================
void Erase16()
{
	VddOn16 ();
	VppOn16 ();

	//
	// Wait for VPP to settle. The MAX680 takes a little while.
	//
	Sleep(100);

	unsigned char command[] = {ERASE_16};

	if (Send(command, 1) && ReceiveOk16())
	{
		printf ("Erased!\n");
	}

	VppVddOff16 ();
}

//===========================================================================
//
// Name    : Program16
//
// Desc    : Writes the contents of the "memory_segments" array to the device.
//
// Returns : Nothing.
//
//===========================================================================
void Program16()
{
	VddOn16 ();
	VppOn16 ();

	//
	// Wait for VPP to settle. The MAX680 takes a little while.
	//
	Sleep(100);

	//
	// Due to the 14 bit width of the bus, each word in the device is treated
	// as two bytes in the hex file. As a result, all addresses are doubled
	// in the hex file. So when we use them here, we need to divide by two
	// when writing.
	//

	do
	{
		//
		// Program...
		//
		for (int seg = 0; seg < g_number_of_segments; seg++)
		{
			//		printf("\n(%i/%i, %08x, %04x) ", seg, segments, memory_segment[seg].address, memory_segment[seg].length);

			unsigned short int device_address = g_memory_segment[seg].address / 2;
			if (device_address < 0x2000)
			{
				if (!ProgramBytes16 (device_address,
									g_memory_segment[seg].bytes,
									g_memory_segment[seg].length))
				{
					break;
				}
			}
			else if (device_address == 0x2007)
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
			unsigned short int device_address = g_memory_segment[seg].address / 2;
			if (device_address == 0x2007)
			{
				unsigned short int word = g_memory_segment[seg].bytes[0] | (g_memory_segment[seg].bytes[1] << 8);
				if (!ProgramConfigWord16 (word))
				{
					break;
				}
			}
		}

		//
		// Verify...
		//
		for (int seg = 0; seg < g_number_of_segments; seg++)
		{
			unsigned short int device_address = g_memory_segment[seg].address / 2;
			unsigned char buffer[MAX_SEGMENT_LENGTH];

			if (!ReadBytes16 (device_address, buffer, g_memory_segment[seg].length))
			{
				printf ("\n*** Verification Error: Failed to read words at %06x\n", device_address);
				break;
			}

			for (int i = 0; i < g_memory_segment[seg].length; i++)
			{
				unsigned char byte = g_memory_segment[seg].bytes[i];

				//
				// If this is the high byte (of the two), then ignore the top two bits as
				// this bus is only 14 bits wide. The hex file is little endian, so odd
				// bytes are the top ones.
				//
				if ((g_memory_segment[seg].address + i) % 2)
				{
					byte &= 0x3f;
				}
				if (buffer[i] != byte)
				{
					printf ("\n*** Verification Error: Byte %06x should be %02x, but reads as %02x.\n",
							device_address + i,
							byte,
							buffer[i]);

					break;
				}
			}
		}

		printf ("Programmed!\n");
	}
	while(0);

	VppVddOff16 ();
}

//===========================================================================
//
// Name    : ReadDeviceId16
//
// Desc    : 
//
// Returns : Nothing.
//
//===========================================================================
void ReadDeviceId16()
{
	VddOn16 ();
	VppOn16 ();

	//
	// Wait for VPP to settle. The MAX680 takes a little while.
	//
	Sleep(100);

	unsigned char buffer[16];

	if (ReadBytes16(0x2000, buffer, 16))
	{
		unsigned short int word = buffer[13] << 8 | buffer[12];
		printf ("Dev_ID : 0x%04x", word);
		switch (word & 0x3fe0)
		{
		case 0x1040: printf(", a 16F627A rev %i", word & 0x001f); break;
		case 0x1060: printf(", a 16F628A rev %i", word & 0x001f); break;
		case 0x1100: printf(", a 16F648A rev %i", word & 0x001f); break;
		default    : printf(", an unknown part");                 break;
		}
		printf(".\n");
	}

	VppVddOff16 ();
}

//===========================================================================
//
// Name    : DumpDevice16
//
// Desc    : 
//
// Returns : Nothing.
//
//===========================================================================
void DumpDevice16()
{
	VddOn16 ();
	VppOn16 ();

	//
	// Wait for VPP to settle. The MAX680 takes a little while.
	//
	Sleep(100);

	DumpDevice16(0x0000, 0x40);   // Program space.
	printf ("\n");
	DumpDevice16(0x2000, 0x08);   // Configuration words.

	VppVddOff16 ();
}
