/*
 * Copyright (C) 2017 Johan Bergkvist
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#ifndef HEXFILE_H
#define HEXFILE_H

#include "stdio.h"
#include "time.h"
#include "windows.h"

/*
 * A segment is a single line from a .hex file, which can hold 16 bytes of data. We cater for a maximum of 64kB in total.
 */
#define MAX_SEGMENT_LENGTH 16
#define MAX_SEGMENTS (65536 / MAX_SEGMENT_LENGTH)

/*
 * Represent a single line from a .hex file. The address is the address in the PIC where this data shall be programmed.
 */
typedef struct {
	unsigned int address;
	int length;
	unsigned char bytes[MAX_SEGMENT_LENGTH];
} SEGMENT, *P_SEGMENT;

/*
 * Global variable holding the data of the last read .hex file.
 */
extern SEGMENT g_memory_segment[MAX_SEGMENTS];
extern int g_number_of_segments;

bool LoadHexFile(char *name);
void PrintHexFile();

#endif
