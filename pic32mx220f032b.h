/*
 * Copyright (C) 2017 Johan Bergkvist
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#ifndef PIC32MX220F032B_H
#define PIC32MX220F032B_H

//
// This is the (physical) address the device ID can be found at.
// Should be 0x4a000053 or similar.
//
#define DEVICE_ID_ADDRESS         0xbf80f220

//
// The four config words locations.
//
#define DEVICE_CONFIG_ADDRESS_0   0x1fc00bfc
#define DEVICE_CONFIG_ADDRESS_1   0x1fc00bf8
#define DEVICE_CONFIG_ADDRESS_2   0x1fc00bf4
#define DEVICE_CONFIG_ADDRESS_3   0x1fc00bf0

//
// The Microchip TAP status register values.
//
#define MCHP_STATUS_DEVRST  0x01
#define MCHP_STATUS_FAEN    0x02
#define MCHP_STATUS_FCBUSY  0x04
#define MCHP_STATUS_CFGRDY  0x08
#define MCHP_STATUS_NVMERR  0x20
#define MCHP_STATUS_CPS     0x80
#define MCHP_STATUS_UNUSED  0x50

//
// Locations and sizes of the boot and program flash memory areas and the
// number of bytes in a "row". A row is used for flash programming.
//
#define BFM_START   0x1fc00000
#define BFM_SIZE        0x0c00  //  3k bytes
#define PFM_START   0x1d000000
#define PFM_SIZE      0x020000  //128k bytes
#define ROW_SIZE        0x0080  // 128 bytes

#endif
