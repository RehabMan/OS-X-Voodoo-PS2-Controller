/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _APPLEPS2SYNAPTICSTOUCHPAD_H
#define _APPLEPS2SYNAPTICSTOUCHPAD_H

#include "ApplePS2MouseDevice.h"
#include "ApplePS2Device.h"
#include "VoodooPS2TouchPadBase.h"

#define ALPS_PROTO_V1	1
#define ALPS_PROTO_V2	2
#define ALPS_PROTO_V3	3
#define ALPS_PROTO_V4	4
#define ALPS_PROTO_V5	5

/**
 * struct alps_model_info - touchpad ID table
 * @signature: E7 response string to match.
 * @command_mode_resp: For V3/V4 touchpads, the final byte of the EC response
 *   (aka command mode response) identifies the firmware minor version.  This
 *   can be used to distinguish different hardware models which are not
 *   uniquely identifiable through their E7 responses.
 * @proto_version: Indicates V1/V2/V3/...
 * @byte0: Helps figure out whether a position report packet matches the
 *   known format for this model.  The first byte of the report, ANDed with
 *   mask0, should match byte0.
 * @mask0: The mask used to check the first byte of the report.
 * @flags: Additional device capabilities (passthrough port, trackstick, etc.).
 *
 * Many (but not all) ALPS touchpads can be identified by looking at the
 * values returned in the "E7 report" and/or the "EC report."  This table
 * lists a number of such touchpads.
 */
struct alps_model_info {
	UInt8 signature[3];
	UInt8 command_mode_resp;
	UInt8 proto_version;
	UInt8 byte0, mask0;
	UInt8 flags;
};

/**
 * struct alps_nibble_commands - encodings for register accesses
 * @command: PS/2 command used for the nibble
 * @data: Data supplied as an argument to the PS/2 command, if applicable
 *
 * The ALPS protocol uses magic sequences to transmit binary data to the
 * touchpad, as it is generally not OK to send arbitrary bytes out the
 * PS/2 port.  Each of the sequences in this table sends one nibble of the
 * register address or (write) data.  Different versions of the ALPS protocol
 * use slightly different encodings.
 */
struct alps_nibble_commands {
	SInt32 command;
	UInt8 data;
};

/**
 * struct alps_fields - decoded version of the report packet
 * @x_map: Bitmap of active X positions for MT.
 * @y_map: Bitmap of active Y positions for MT.
 * @fingers: Number of fingers for MT.
 * @x: X position for ST.
 * @y: Y position for ST.
 * @z: Z position for ST.
 * @first_mp: Packet is the first of a multi-packet report.
 * @is_mp: Packet is part of a multi-packet report.
 * @left: Left touchpad button is active.
 * @right: Right touchpad button is active.
 * @middle: Middle touchpad button is active.
 * @ts_left: Left trackstick button is active.
 * @ts_right: Right trackstick button is active.
 * @ts_middle: Middle trackstick button is active.
 */
struct alps_fields {
	UInt32 x_map;
	UInt32 y_map;
	UInt32 fingers;
	UInt32 x;
	UInt32 y;
	UInt32 z;
	UInt32 first_mp:1;
	UInt32 is_mp:1;
    
	UInt32 left:1;
	UInt32 right:1;
	UInt32 middle:1;
    
	UInt32 ts_left:1;
	UInt32 ts_right:1;
	UInt32 ts_middle:1;
};

class ApplePS2ALPSGlidePoint;

/**
 * struct alps_data - private data structure for the ALPS driver
 * @dev2: "Relative" device used to report trackstick or mouse activity.
 * @phys: Physical path for the relative device.
 * @nibble_commands: Command mapping used for touchpad register accesses.
 * @addr_command: Command used to tell the touchpad that a register address
 *   follows.
 * @proto_version: Indicates V1/V2/V3/...
 * @byte0: Helps figure out whether a position report packet matches the
 *   known format for this model.  The first byte of the report, ANDed with
 *   mask0, should match byte0.
 * @mask0: The mask used to check the first byte of the report.
 * @flags: Additional device capabilities (passthrough port, trackstick, etc.).
 * @x_max: Largest possible X position value.
 * @y_max: Largest possible Y position value.
 * @x_bits: Number of X bits in the MT bitmap.
 * @y_bits: Number of Y bits in the MT bitmap.
 * @hw_init: Protocol-specific hardware init function.
 * @process_packet: Protocol-specific function to process a report packet.
 * @decode_fields: Protocol-specific function to read packet bitfields.
 * @set_abs_params: Protocol-specific function to configure the input_dev.
 * @prev_fin: Finger bit from previous packet.
 * @multi_packet: Multi-packet data in progress.
 * @multi_data: Saved multi-packet data.
 * @x1: First X coordinate from last MT report.
 * @x2: Second X coordinate from last MT report.
 * @y1: First Y coordinate from last MT report.
 * @y2: Second Y coordinate from last MT report.
 * @fingers: Number of fingers from last MT report.
 * @quirks: Bitmap of ALPS_QUIRK_*.
 * @timer: Timer for flushing out the final report packet in the stream.
 */
struct alps_data {
	/* these are autodetected when the device is identified */
	const struct alps_nibble_commands *nibble_commands;
	SInt32 addr_command;
	UInt8 proto_version;
	UInt8 byte0, mask0;
	UInt8 flags;
    // sensible defaults required during initialization
	SInt32 x_max = 2000;
	SInt32 y_max = 1400;
	SInt32 x_bits;
	SInt32 y_bits;
    
	SInt32 prev_fin;
	SInt32 multi_packet;
	UInt8 multi_data[6];
	SInt32 x1, x2, y1, y2;
	SInt32 fingers;
	UInt8 quirks;
    
    int pktsize = 6;
};

// Pulled out of alps_data, now saved as vars on class
// makes invoking a little easier
typedef bool (ApplePS2ALPSGlidePoint::*hw_init)();
typedef void (ApplePS2ALPSGlidePoint::*decode_fields)(struct alps_fields *f, UInt8 *p);
typedef void (ApplePS2ALPSGlidePoint::*process_packet)(UInt8 *packet);
//typedef void (ApplePS2ALPSGlidePoint::*set_abs_params)();

#define ALPS_QUIRK_TRACKSTICK_BUTTONS	1 /* trakcstick buttons in trackstick packet */

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// ApplePS2ALPSGlidePoint Class Declaration
//

typedef struct ALPSStatus {
    UInt8 bytes[3];
} ALPSStatus_t;

#define kPacketLengthSmall  3
#define kPacketLengthLarge  6
#define kPacketLengthMax    6

#define kDP_CommandNibble10 0xf2

//#define ALPS_V3_ADDR_COMMAND kDP_MouseResetWrap
//#define ALPS_V3_BYTE0 0x8f
//#define ALPS_V3_MASK0 0x8f
//#define ALPS_V3_X_MAX	2000
//#define ALPS_V3_Y_MAX	1400
//#define ALPS_BITMAP_X_BITS	15
//#define ALPS_BITMAP_Y_BITS	11

#define BITS_PER_BYTE 8

// predeclure stuff
struct alps_data;

class EXPORT ApplePS2ALPSGlidePoint : public VoodooPS2TouchPadBase {
    typedef VoodooPS2TouchPadBase super;
    OSDeclareDefaultStructors( ApplePS2ALPSGlidePoint );

private:
    alps_data modelData;
    
    hw_init hw_init;
    decode_fields decode_fields;
    process_packet process_packet;
    UInt32 cur_button;
//    set_abs_params set_abs_params;
    
protected:
    int _multiPacket;
    UInt8 _multiData[6];
    IOGBounds _bounds;

    virtual void dispatchRelativePointerEventWithPacket(UInt8 *packet,
            UInt32 packetSize);

    bool getStatus(ALPSStatus_t *status);

    virtual bool deviceSpecificInit();

    bool enterCommandMode();

    bool exitCommandMode();

    bool hwInitV3();

    int commandModeReadReg(int addr);

    bool commandModeWriteReg(int addr, UInt8 value);

    bool commandModeWriteReg(UInt8 value);

    bool commandModeSendNibble(int value);

    bool commandModeSetAddr(int addr);

    bool passthroughModeV3(int regBase, bool enable);
    
    bool passthroughModeV2(bool enable);

    bool absoluteModeV3();
    
    bool absoluteModeV1V2();
    
    bool absoluteModeV4();

    bool resetMouse();

    bool setSampleRateAndResolution(UInt8 rate, UInt8 res);

    void processPacketV3(UInt8 *packet);

    void processTrackstickPacketV3(UInt8 * packet);

    void processTouchpadPacketV3(UInt8 * packet);

    int processBitmap(UInt32 xMap, UInt32 yMap, int *x1, int *y1, int *x2, int *y2);

    PS2InterruptResult interruptOccurred(UInt8 data);

    void packetReady();

    void setTouchPadEnable(bool enable);
    
    void dispatchEventsWithInfo(int xraw, int yraw, int z, int fingers, UInt32 buttonsraw);

    void calculateMovement(int x, int y, int z, int fingers, int & dx, int & dy);
    
    void processPacketV1V2(UInt8 *packet);
    
    void decodeButtonsV3(struct alps_fields *f, UInt8 *p);
    
    void decodePinnacle(struct alps_fields *f, UInt8 *p);
    
    void decodeRushmore(struct alps_fields *f, UInt8 *p);
    
    void decodeDolphin(struct alps_fields *f, UInt8 *p);
    
    void processPacketV4(UInt8 *packet);
    
    bool repeatCmd(SInt32 init_command, SInt32 init_arg, SInt32 repeated_command, ALPSStatus_t *report);
    
    bool tapMode(bool enable);
    
    bool hwInitV1V2();
    
    bool hwInitV4();
    
    IOReturn probeTrackstickV3(int regBase);
    
    IOReturn setupTrackstickV3(int regBase);
    
    bool hwInitRushmoreV3();
    
    bool hwInitDolphinV1();
    
    void setDefaults();
    
    bool matchTable(ALPSStatus_t *e7, ALPSStatus_t *ec);
    
    IOReturn identify();
    
    void setupMaxes();
    
    bool v1v2MagicEnable();

public:
    virtual ApplePS2ALPSGlidePoint * probe(IOService *provider,
            SInt32 *score);

    void stop(IOService *provider);

    bool init(OSDictionary * dict);
};

#endif /* _APPLEPS2SYNAPTICSTOUCHPAD_H */
