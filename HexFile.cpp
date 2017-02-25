/*
 * Copyright (C) 2017 Johan Bergkvist
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#include "HexFile.h"

/*
 * Global variable holding the data of the last read .hex file.
 */
SEGMENT g_memory_segment[MAX_SEGMENTS];
int g_number_of_segments = 0;

/*
 * A .hex file specifies a base address. The address in subsequent data segments
 * is then realtive the base address. This variable stores the most recently
 * parsed base address.
 */
unsigned int base_address = 0;

//===========================================================================
//
// Name    : LoadHexFile
//
// Desc    : Loads the .hex file with the given name. The name may be a full
//           or relative path.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
bool LoadHexFile (char *name)
{
	FILE *file = NULL;
	if (fopen_s(&file, name, "r") != 0 || file == NULL)
	{
		printf ("ERROR: Cannot open file %s.\n", name);
		return false;
	}

	char line[200];
	while (fgets (line, 200, file) != NULL)
	{
		switch (*(line + 8))
		{
		case '0':
			{
				sscanf_s(line + 1,
						"%02x%04x",
						&g_memory_segment[g_number_of_segments].length,
						&g_memory_segment[g_number_of_segments].address);
				for (int i = 0; i < g_memory_segment[g_number_of_segments].length; i++)
				{
					sscanf_s(line + 9 + i * 2,
							"%02x",
							&g_memory_segment[g_number_of_segments].bytes[i]);
				}

				//
				// Is the number of bytes an odd number bigger than 2? Programmer won't like it, so add
				// another byte. Single bytes are likely to be config words, so we leave them alone.
				//
				if (g_memory_segment[g_number_of_segments].length > 2 && (g_memory_segment[g_number_of_segments].length % 2) == 1)
				{
					printf("+++ Padded bytes starting at %08x with an extra byte at offset %02x.\n", g_memory_segment[g_number_of_segments].address, g_memory_segment[g_number_of_segments].length);
					g_memory_segment[g_number_of_segments].bytes[g_memory_segment[g_number_of_segments].length] = 0xff;
					g_memory_segment[g_number_of_segments].length++;
				}
				g_memory_segment[g_number_of_segments].address += base_address;

				//
				// PIC parts cannot be programmed with a range of bytes that straddle a 32 or 64, depending
				// on the part, byte boundary. So if the just added segment cross a 32 byte boundary then
				// split it into two.
				//
				int start_region = g_memory_segment[g_number_of_segments].address >> 5;
				int end_region = (g_memory_segment[g_number_of_segments].address + g_memory_segment[g_number_of_segments].length - 1) >> 5;
				if (start_region != end_region)
				{
					int boundary_address = end_region << 5;
					int boundary_offset = boundary_address - g_memory_segment[g_number_of_segments].address;

					//
					// Create a new segment with the data on the other side of the boundary.
					//
					g_memory_segment[g_number_of_segments + 1].address = boundary_address;
					g_memory_segment[g_number_of_segments + 1].length = g_memory_segment[g_number_of_segments].length - boundary_offset;
					memcpy(g_memory_segment[g_number_of_segments + 1].bytes, g_memory_segment[g_number_of_segments].bytes + boundary_offset, g_memory_segment[g_number_of_segments + 1].length);

					//
					// Shorten the original set of data.
					//
					g_memory_segment[g_number_of_segments].length = boundary_offset;

					g_number_of_segments++;
					g_number_of_segments++;
				} else {
					g_number_of_segments++;
				}
			}
			break;

		case '1':
			return true;
			break;

		case '4':
			sscanf_s(line + 9, "%04x", &base_address);
			base_address <<= 16;
			break;

		default:
			printf ("Unrecognised line \"%s\".\n", line);
			return false;
			break;
		}
	}

	fclose (file);

	return true;
}

//===========================================================================
//
// Name    : PrintHexFile
//
// Desc    : Dumps the contents of the last read .hex file to stdout.
//
// Returns : True if successful, false otherwise.
//
//===========================================================================
void PrintHexFile ()
{
	for (int seg = 0; seg < g_number_of_segments; seg++)
	{
		printf ("%06x (% 3i bytes) : ", g_memory_segment[seg].address, g_memory_segment[seg].length);
		for (int i = 0; i < g_memory_segment[seg].length; i++)
		{
			printf ("%02x ", g_memory_segment[seg].bytes[i]);
		}
		printf ("\n");
	}
}

