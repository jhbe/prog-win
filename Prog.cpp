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
#include "Pic16.h"
#include "Pic18.h"
#include "Pic32.h"

//===========================================================================
//
// Name    : main
//
// Desc    : You know what "main" is, don't you...?  :)
//
// Returns : 0 if no errors were encountered, -1 otherwise.
//
//===========================================================================
int main (int argc, char *argv[])
{
	bool pic16          = false;
	bool pic18          = false;
	bool pic32          = false;
	bool erase          = false;
	bool program        = false;
	bool print_hex_file = false;
	bool read_device_id = false;
	bool dump_device    = false;

	char *hex_file_name = NULL;
	int next_arg = 1;

	//
	// Loop over each command line argument one at a time.
	//
	while (next_arg < argc
		   && argv[next_arg][0] == '-')
	{
		if (strcmp (argv[next_arg], "-p") == 0)
		{
			program = true;
			hex_file_name = argv[++next_arg];
		}
		else if (strcmp (argv[next_arg], "-h") == 0)
		{
			print_hex_file = true;
			hex_file_name = argv[++next_arg];
		}
		else if (strcmp (argv[next_arg], "-id") == 0)
		{
			read_device_id = true;
		}
		else if (strcmp (argv[next_arg], "-e") == 0)
		{
			erase = true;
		}
		else if (strcmp (argv[next_arg], "-16") == 0)
		{
			pic16 = true;
		}
		else if (strcmp (argv[next_arg], "-18") == 0)
		{
			pic18 = true;
		}
		else if (strcmp (argv[next_arg], "-32") == 0)
		{
			pic32 = true;
		}
		else if (strcmp (argv[next_arg], "-d") == 0)
		{
			dump_device = true;
		}
		else if (strcmp (argv[next_arg], "-rxtx") == 0)
		{
			g_print_txrx = true;
		}
		else if (strcmp (argv[next_arg], "-?") == 0)
		{
			printf ("\n");
			printf ("Usage: Prog [-16|-18|-32] [[-e] [-p <hex_file>] [-id] [-d] [-rxtx]| -h <hex_file>]\n");
			printf ("\n");
			printf ("         -16     Target is a PIC16F device.\n");
			printf ("         -18     Target is a PIC18F device.\n");
			printf ("         -32     Target is a PIC32MX device.\n");
			printf ("\n");
			printf ("         -e      Erase the device.\n");
			printf ("         -p      Program and verify the device.\n");
			printf ("         -id     Read the Device ID.\n");
			printf ("         -d      Dump selected memory areas of the device.\n");
			printf ("         -h      Print the content of the hex file.\n");
			printf ("\n");
			printf ("         -rxtx   Prints the USB communication. For debugging purposes.\n");
			printf ("\n");
			return 0;
		}
		else
		{
			printf ("ERROR: \"%s\" is not a valid option. Use \"-?\".\n", argv[next_arg]);
			return -1;
		}
					
		next_arg++;
	}

	int number_of_devices_nonimated = 0;
	number_of_devices_nonimated += pic16 ? 1 : 0;
	number_of_devices_nonimated += pic18 ? 1 : 0;
	number_of_devices_nonimated += pic32 ? 1 : 0;

	if (number_of_devices_nonimated == 0)
	{
		printf ("ERROR: Must specify -16, -18 or -32.\n");
		return -1;
	}
	if (number_of_devices_nonimated > 1)
	{
		printf ("ERROR: Must specify either -16, -18 or -32, but not more than one.\n");
		return -1;
	}

	if (hex_file_name != NULL)
	{
		//
		// Read and load the content of the hex file.
		//
		if (!LoadHexFile(hex_file_name))
		{
			return -1;
		}
	}
	if (print_hex_file)
	{
		PrintHexFile();
		return 0;
	}

	if (!Open())
	{
		printf("*** Failed to open the USB connection to the programmer.\n");
		return 0;
	}

	if (pic16)
	{
		if (erase)
		{
			Erase16();
		}
		if (program)
		{
			Program16();
		}
		if (read_device_id)
		{
			ReadDeviceId16();
		}
		if (dump_device)
		{
			DumpDevice16();
		}
	}
	if (pic18)
	{
		if (erase)
		{
			Erase18();
		}
		if (program)
		{
			Program18();
		}
		if (read_device_id)
		{
			ReadDeviceId18();
		}
		if (dump_device)
		{
			DumpDevice18();
		}
	}
	if (pic32)
	{
		if (erase)
		{
			Erase32();
		}
		if (program)
		{
			Program32();
		}
		if (read_device_id)
		{
			ReadDeviceId32();
		}
		if (dump_device)
		{
			DumpDevice32();
		}
	}

	Close();

	return 0;
}
