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
#include "pic32mx220f032b.h"

//
// These are the commands sent from the PC to the PIC. Commands in the range
// 0x00 - 0x0f are used in Prog18FUsb and not reused here to enable a merge
// of the two at some future date.
//
#define COMMAND_CHECK_DEVICE                 0x10
#define COMMAND_ERASE						 0x11
#define COMMAND_ENTER_SERIAL_EXECUTION_MODE  0x12
#define COMMAND_EXIT_PROGRAMMING_MODE        0x13
#define COMMAND_READ_WORDS                   0x15
#define COMMAND_SEND_WORDS                   0x16
#define COMMAND_PROGRAM_WORDS                0x17

unsigned char bfm[BFM_SIZE];
unsigned char pfm[PFM_SIZE];

bool g_verbose = true;

//===========================================================================
//
// Name    : ReceiveAll
//
// Desc    : This function receives a number of bytes from the USB pipe. If
//           received data consist of more than four bytes and start with the
//           four byte word "DEBU" or "TEXT" the bytes are printed and the
//           function reads again.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool ReceiveAll(unsigned char *response, int *bytes_received)
{
	do
	{
		//
		// Since we're doing USB bulk transfers at 64 bytes maximum packet
		// length.
		//
		*bytes_received = 64;

		if (!Receive(response, bytes_received))
		{
			if (g_verbose)
			{
				printf("*** Waiting for a result, but the read failed.\n");
			}
			return false;
		}
		if (*bytes_received == 0)
		{
			if (g_verbose)
			{
				printf("*** Got 0 bytes, reading again.\n");
			}
			continue;
		}
		if (*bytes_received > 4
			&& response[0] == 'D'
			&& response[1] == 'E'
			&& response[2] == 'B'
			&& response[3] == 'U')
		{
			printf("DEBU: ");
			if ((*bytes_received % 4) == 0)
			{
				//
				// Treat the bytes as an array of words (32 bit).
				//
				unsigned int *long_response = (unsigned int *)&response[4];
				int words = (*bytes_received - 4) / 4;

				for (int i = 0; i < words; i++)
				{
					printf("%08x ", long_response[i]);
				}
			}
			else
			{
				for (int i = 0; i < *bytes_received - 4; i++)
				{
					printf("%02x ", response[i + 4]);
				}
			}
			printf("\n");

			continue;
		}
		if (*bytes_received > 4
			&& response[0] == 'T'
			&& response[1] == 'E'
			&& response[2] == 'X'
			&& response[3] == 'T')
		{
			char string[1024];
			int i;
			for (i = 0; i < *bytes_received - 4; i++)
			{
				string[i] = response[4 + i];
			}
			string[i] = 0;
			printf("TEXT: %s\n", string);

			continue;
		}

		if (*bytes_received > 0)
		{
			return true;
		}
	}
	while(1);
}

//===========================================================================
//
// Name    : ReceiveResult
//
// Desc    : This function reads the USB inpipe until a single bytes has
//           been received. This is the result byte.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool ReceiveResult(unsigned char *result)
{
	unsigned char response[64];

	do
	{
		int bytes_received;

		if (!ReceiveAll(response, &bytes_received))
		{
			return false;
		}
		if (bytes_received == 1)
		{
			*result = response[0];
			return true;
		}

		if (g_verbose)
		{
			printf("*** Waiting for an OK, but got %i bytes rather than 2.\n", bytes_received);
			for (int i = 0; i < bytes_received; i++)
			{
				printf("%02x ", response[i]);
			}
			printf("\n");
		}
	}
	while(1);
}

//===========================================================================
//
// Name    : ReceiveOk
//
// Desc    : This function reads the USB in pipe until the two bytes "OK" or
//           the four bytes "FAIL" has been received.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool ReceiveOk()
{
	unsigned char response[64];

	do
	{
		int bytes_received = 64;

		if (!ReceiveAll(response, &bytes_received))
		{
			return false;
		}
		if (bytes_received == 2
			&& response[0] == 'O'
			&& response[1] == 'K')
		{
			return true;
		}
		if (bytes_received == 4
			&& response[0] == 'F'
			&& response[1] == 'A'
			&& response[2] == 'I'
			&& response[3] == 'L')
		{
			if (g_verbose)
			{
				printf("*** Waiting for an OK, but got %c%c%c%c.\n", response[0], response[1], response[2], response[3]);
			}
			return false;
		}
		if (bytes_received != 2)
		{
			if (g_verbose)
			{
				printf("*** Waiting for an OK, but got %i bytes rather than 2.\n", bytes_received);
				for (int i = 0; i < bytes_received; i++)
				{
					printf("%02x ", response[i]);
				}
				printf("\n");
			}
		}
	}
	while(1);
}

//===========================================================================
//
// Name    : ReadWords
//
// Desc    : Reads "length" words from the target PIC into "buffer" starting
//           at address "address".
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool ReadWords (unsigned int address, unsigned int *buffer, int number_of_words)
{
	unsigned char data[64];

	unsigned char command[] = {
		COMMAND_READ_WORDS,
		static_cast<unsigned char>((address & 0x000000ff) >> 0),
		static_cast<unsigned char>((address & 0x0000ff00) >> 8),
		static_cast<unsigned char>((address & 0x00ff0000) >> 16),
		static_cast<unsigned char>((address & 0xff000000) >> 24),
		static_cast<unsigned char>(number_of_words)
	};
	
	if (!Send(command, 6))
	{
		if (g_verbose)
		{
			printf("*** Failed to send the COMMAND_READ_WORDS message.\n");
		}
		return false;
	}

	do
	{
		int bytes_received;
		if (!ReceiveAll(data, &bytes_received))
		{
			return false;
		}
		if (number_of_words * 4 != bytes_received)
		{
			printf("+++ Expected %i bytes but got %i: (", number_of_words * 4, bytes_received);
			for (int i = 0; i < bytes_received; i++)
			{
				printf("%02x ", data[i]);
			}
			printf(")\n");
			continue;
		}
		memcpy(buffer, data, number_of_words * 4);
		break;
	}
	while(1);

	return true;
}

//===========================================================================
//
// Name    : SendWords
//
// Desc    : Sends 32 bytes to be programmed. Doesn't  actually program them,
//           that is done by the ProgramWords function.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool SendWords (unsigned char offset, unsigned char *bytes)
{
	unsigned char command[1024] = {
		COMMAND_SEND_WORDS,
		offset
	};
	memcpy(&command[2], bytes, 32);

	return Send(command, 2 + 32) && ReceiveOk();
}

//===========================================================================
//
// Name    : ProgramWords
//
// Desc    : Program bytes previously sent using the SendBytes function.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool ProgramWords (unsigned int address)
{
	unsigned char command[1024] = {
		COMMAND_PROGRAM_WORDS,
		static_cast<unsigned char>((address & 0x000000ff) >> 0),
		static_cast<unsigned char>((address & 0x0000ff00) >> 8),
		static_cast<unsigned char>((address & 0x00ff0000) >> 16),
		static_cast<unsigned char>((address & 0xff000000) >> 24)
	};

	return Send(command, 5) && ReceiveOk();
}

//===========================================================================
//===========================================================================

//===========================================================================
//
// Name    : CheckDevice
//
// Desc    : .
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool CheckDevice ()
{
	unsigned char mchp_status;

	if (!Send(COMMAND_CHECK_DEVICE))
	{
		return false;
	}
	
	if (!ReceiveResult(&mchp_status))
	{
		return false;
	}

	if (!(mchp_status & MCHP_STATUS_FCBUSY)
		&& (mchp_status & MCHP_STATUS_CFGRDY))
	{
		return true;
	}

	if (mchp_status == 0xff)
	{
		printf("*** (CheckDevice) MCHP_STATUS was 0xff. That usually means the PIC32MX is not powered.\n");
	}
	else
	{
		if (mchp_status & MCHP_STATUS_NVMERR)
		{
			printf("*** (CheckDevice): NVMERR was asserted in MCHP_STATUS (%02x).\n", mchp_status);
		}
		if (mchp_status & MCHP_STATUS_FCBUSY)
		{
			printf("*** (CheckDevice): FCBUSY was asserted in MCHP_STATUS (%02x).\n", mchp_status);
		}
		if (!(mchp_status & MCHP_STATUS_CFGRDY))
		{
			printf("*** (CheckDevice): CFGRDY was NOT asserted in MCHP_STATUS (%02x).\n", mchp_status);
		}
	}

	return false;
}

//===========================================================================
//
// Name    : Erase
//
// Desc    : .
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool Erase ()
{
	unsigned char mchp_status;

	if (!Send(COMMAND_ERASE))
	{
		return false;
	}
	
	if (!ReceiveResult(&mchp_status))
	{
		return false;
	}

	if (!(mchp_status & MCHP_STATUS_NVMERR)
		&& !(mchp_status & MCHP_STATUS_FCBUSY)
		&& (mchp_status & MCHP_STATUS_CFGRDY))
	{
		return true;
	}

	if (mchp_status & MCHP_STATUS_NVMERR)
	{
		printf("*** The erase failed. NVMERR was asserted in MCHP_STATUS (%02x).\n", mchp_status);
	}
	if (mchp_status & MCHP_STATUS_FCBUSY)
	{
		printf("*** The erase failed. FCBUSY was asserted in MCHP_STATUS (%02x).\n", mchp_status);
	}
	if (!(mchp_status & MCHP_STATUS_CFGRDY))
	{
		printf("*** The erase failed. CFGRDY was NOT asserted in MCHP_STATUS (%02x).\n", mchp_status);
	}

	return false;
}

//===========================================================================
//
// Name    : EnterProgrammingMode
//
// Desc    : .
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool EnterProgrammingMode ()
{
	return Send(COMMAND_ENTER_SERIAL_EXECUTION_MODE) && ReceiveOk();
}

//===========================================================================
//
// Name    : ExitProgrammingMode
//
// Desc    : .
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool ExitProgrammingMode ()
{
	return Send(COMMAND_EXIT_PROGRAMMING_MODE) && ReceiveOk();
}

//===========================================================================
//
// Name    : ProgramFlashMemory
//
// Desc    : This function will program the content of a memory area "fm"
//           (short for flash memory) consisting of "size" bytes into the
//           target PIC starting at physical address "address".
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool ProgramFlashMemory(unsigned char *fm, int size, unsigned int address)
{
	//
	// Loop over each "row", as specified by the PIC32 flash programming
	// specification. A row is the number of bytes programmed into the
	// target PIC in one go. The size of the row depends on the target PIC;
	// for the PIC32MX220F032B a row is 128 bytes, or 32 words.
	//
	for (int row_index = 0; row_index < size / ROW_SIZE; row_index++)
	{
		//
		// Does this row need programming? It does if at least one byte is not
		// 0xff.
		bool needs_programming = false;
		for (int i = 0; i < ROW_SIZE && !needs_programming; i++)
		{
			needs_programming = fm[row_index * ROW_SIZE + i] != 0xff;
		}
		if (!needs_programming)
		{
			//
			// This row is all 0xff, so does not need to be programmed. On to
			// the next row.
			//
			continue;
		}

		//
		// This row does need to be programmed. We do so by sending the bytes to
		// the target PIC using the SendWords function, which makes sure the words
		// are stored in target PIC SRAM, then issue a ProgramWords call to have
		// them programmed to the flash memory.
		//
		// For the PIC32MX220F032B, we send 32 bytes in each call to SendWords. With
		// a row equal to 128 bytes, we will call SendWords four times per row.
		//
		for (int offset = 0; offset < ROW_SIZE; offset += 32)
		{
			if (!SendWords(offset, &fm[row_index * ROW_SIZE + offset]))
			{
				return false;
			}
		}
		
		if (!ProgramWords(address + row_index * ROW_SIZE))
		{
			return false;
		}
	}
	return true;
}

//===========================================================================
//
// Name    : Program
//
// Desc    : This function will program the content of the bfm and pgm areas
//           into the target PIC.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool Program ()
{
	return ProgramFlashMemory(pfm, PFM_SIZE, PFM_START)
		&& ProgramFlashMemory(bfm, BFM_SIZE, BFM_START);
}

//===========================================================================
//
// Name    : Verify
//
// Desc    : Verifies that the contents of the "memory_segments" array matches
//           the contents of the target PIC.
//
// Returns : True if successfull, false otherwise.
//
//===========================================================================
bool Verify ()
{
	//
	// Loop over each segment as read from the hex file and verify than the
	// words read from the PIC match.
	//
	for (int seg = 0; seg < g_number_of_segments; seg++)
	{
		unsigned char buffer[MAX_SEGMENT_LENGTH];

		//
		// Read the bytes from the PIC.
		//
		if (!ReadWords (g_memory_segment[seg].address, (unsigned int *)buffer, g_memory_segment[seg].length / 4))
		{
			return false;
		}

		//
		// Loop over each bytes and verify.
		//
		for (int i = 0; i < g_memory_segment[seg].length; i++)
		{
			//
			// SPECIAL CASE: The DEVCFG0 word is always read with it's most signficant
			// bit set to zero (this is according to documentation and not unexpected)
			// and for some reason the JTAGEN bit cannot be programmed to 0, which is
			// not expected.
			//
			// In any event we ignore a mismatch in the the DEVCFG0 word.
			//
			if (buffer[i] != g_memory_segment[seg].bytes[i]
				&& (((g_memory_segment[seg].address + i) & 0xfffffffc) != DEVICE_CONFIG_ADDRESS_0))
			{
				printf ("\n\n*** Verification Error: Byte %08x should be %02x, but reads as %02x.\n\n",
						g_memory_segment[seg].address + i,
						g_memory_segment[seg].bytes[i],
						buffer[i]);

				return false;
			}
		}
	}

	return true;
}

//===========================================================================
//
// Name    : DumpDevice32
//
// Desc    : Write the contents of "length" bytes from address "address".
//           "Length" should be a multiple of 16.
//
// Returns : True if successfull, false otherwise.
//
//===========================================================================
bool DumpDevice32(int address, int length)
{
	unsigned int buffer[1024];

	for (int i = 0; i < length; i += 16)
	{
		if (!ReadWords  (address + i, buffer, 4))
		{
			return false;
		}

		printf ("%08x : %08x %08x %08x %08x\n",
				address + i,
				buffer[0],
				buffer[1],
				buffer[2],
				buffer[3]);
	}
	
	return true;
}

//===========================================================================
//
// Name    : Erase32
//
// Desc    : 
//
// Returns : Nothing.
//
//===========================================================================
void Erase32()
{
	if (CheckDevice()
		&& Erase()
		&& EnterProgrammingMode()
		&& ExitProgrammingMode())
	{
		printf ("Erased!\n");
	}
}

//===========================================================================
//
// Name    : Program32
//
// Desc    : 
//
// Returns : Nothing.
//
//===========================================================================
void Program32()
{
	//
	// The bfm (boot flash memory) and pgm (program flash memory) area areas
	// that will hold the bytes to be programmed. Bytes that do not need to be
	// programmed should be 0xff.
	//
	memset(bfm, 0xff, BFM_SIZE);
	memset(pfm, 0xff, PFM_SIZE);

	//
	// Now loop over each of the segments loaded and store them in the BFM or
	// PGM as appropriate.
	//
	for (int seg = 0; seg < g_number_of_segments; seg++)
	{
		unsigned char *fm = (g_memory_segment[seg].address < BFM_START) ? pfm : bfm;
		int offset = g_memory_segment[seg].address & 0x000fffff;

		memcpy(&fm[offset], g_memory_segment[seg].bytes, g_memory_segment[seg].length);
	}

	if (CheckDevice()
		&& Erase()
		&& EnterProgrammingMode()
		&& Program()
		&& Verify()
		&& ExitProgrammingMode())
	{
		printf ("Programmed!\n");
	}
}

//===========================================================================
//
// Name    : ReadDeviceId32
//
// Desc    : 
//
// Returns : Nothing.
//
//===========================================================================
void ReadDeviceId32()
{
	unsigned int dev_id;
	if (CheckDevice() && EnterProgrammingMode() && ReadWords(DEVICE_ID_ADDRESS, &dev_id, 1) && ExitProgrammingMode())
	{
		printf ("Dev_ID : 0x%08x", dev_id);

		switch (dev_id & 0x0fffffff)
		{
		case 0x04A07053: printf(", a PIC32MX110F016B rev %i", dev_id >> 28); break;
		case 0x04A06053: printf(", a PIC32MX120F032B rev %i", dev_id >> 28); break;
		case 0x04D07053: printf(", a PIC32MX130F064B rev %i", dev_id >> 28); break;
		case 0x04D06053: printf(", a PIC32MX150F128B rev %i", dev_id >> 28); break;
		case 0x04A01053: printf(", a PIC32MX210F016B rev %i", dev_id >> 28); break;
		case 0x04A00053: printf(", a PIC32MX220F032B rev %i", dev_id >> 28); break;
		case 0x04D00053: printf(", a PIC32MX250F128B rev %i", dev_id >> 28); break;
		case 0x04D01053: printf(", a PIC32MX230F063B rev %i", dev_id >> 28); break;
		default: printf(", an unknown part"); break;
		}
		printf(".\n");
	}
	else
	{
		printf ("Failed to read the device ID.\n");
	}
}

//===========================================================================
//
// Name    : DumpDevice32
//
// Desc    : 
//
// Returns : Nothing.
//
//===========================================================================
void DumpDevice32()
{
	if (!CheckDevice() || !EnterProgrammingMode())
	{
		return;
	}

	printf ("\nProgram Flash Memory:\n");
	DumpDevice32(0x1d000000, 256);
	printf ("\nBoot Flash Memory:\n");
	DumpDevice32(0x1fc00000, 256);
	printf ("\nConfiguration words:\n");
	DumpDevice32(DEVICE_CONFIG_ADDRESS_3, 0x10);   // Configuration words.
	printf ("\nDevice ID bytes:\n");
	DumpDevice32(DEVICE_ID_ADDRESS, 0x01);   // Device ID bytes.

	ExitProgrammingMode();
}
