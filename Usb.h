/*
 * Copyright (C) 2017 Johan Bergkvist
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#ifndef USB_H
#define USB_H

/*
 * If true, Send() and Receive() will dump the raw bytes to stdout.
 */
extern bool g_print_txrx;

bool Open();
void Close();

bool Receive(unsigned char *buffer, int *length);
bool Send(unsigned char *buffer, int length);
bool Send(unsigned char byte);

#endif
