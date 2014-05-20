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

#include <IOKit/hidsystem/IOHIDParameter.h>
#include "VoodooPS2Controller.h"
#include "VoodooPS2ALPSGlidePoint.h"

enum {
    kTapEnabled = 0x01
};

#define ARRAY_SIZE(x)    (sizeof(x)/sizeof(x[0]))
#define MAX(X,Y)         ((X) > (Y) ? (X) : (Y))
#define abs(x) ((x) < 0 ? -(x) : (x))


/*
 * Definitions for ALPS version 3 and 4 command mode protocol
 */
#define ALPS_CMD_NIBBLE_10  0x01f2

#define ALPS_REG_BASE_RUSHMORE  0xc2c0
#define ALPS_REG_BASE_PINNACLE  0x0000

/*
 * All these commands are 1 byte. The Linux driver uses 2 bytes for
 * each command where nibble 3 (0x0f00) is used to determine the
 * amount of data to receive and nibble 4 (0xf000) is used to
 * determine the amount of data to send after the initial command.
 *
 */
static const struct alps_nibble_commands alps_v3_nibble_commands[] = { {
kDP_MouseSetPoll, 0x00 }, /* 0 no send/recv */
    { kDP_SetDefaults,                  0x00 }, /* 1 no send/recv */
    { kDP_SetMouseScaling2To1,          0x00 }, /* 2 no send/recv */
    { kDP_SetMouseSampleRate | 0x1000,  0x0a }, /* 3 send=1 recv=0 */
    { kDP_SetMouseSampleRate | 0x1000,  0x14 }, /* 4 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x28 }, /* 5 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x3c }, /* 6 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x50 }, /* 7 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x64 }, /* 8 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0xc8 }, /* 9 ..*/
    { kDP_CommandNibble10    | 0x0100,  0x00 }, /* a send=0 recv=1 */
    { kDP_SetMouseResolution | 0x1000,  0x00 }, /* b send=1 recv=0 */
    { kDP_SetMouseResolution | 0x1000,  0x01 }, /* c ..*/
    { kDP_SetMouseResolution | 0x1000,  0x02 }, /* d ..*/
    { kDP_SetMouseResolution | 0x1000,  0x03 }, /* e ..*/
    { kDP_SetMouseScaling1To1,          0x00 }, /* f no send/recv */
};

static const struct alps_nibble_commands alps_v4_nibble_commands[] = { {
kDP_Enable, 0x00 }, /* 0 no send/recv */
    { kDP_SetDefaults,                  0x00 }, /* 1 no send/recv */
    { kDP_SetMouseScaling2To1,          0x00 }, /* 2 no send/recv */
    { kDP_SetMouseSampleRate | 0x1000,  0x0a }, /* 3 send=1 recv=0 */
    { kDP_SetMouseSampleRate | 0x1000,  0x14 }, /* 4 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x28 }, /* 5 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x3c }, /* 6 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x50 }, /* 7 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0x64 }, /* 8 ..*/
    { kDP_SetMouseSampleRate | 0x1000,  0xc8 }, /* 9 ..*/
    { kDP_CommandNibble10    | 0x0100,  0x00 }, /* a send=0 recv=1 */
    { kDP_SetMouseResolution | 0x1000,  0x00 }, /* b send=1 recv=0 */
    { kDP_SetMouseResolution | 0x1000,  0x01 }, /* c ..*/
    { kDP_SetMouseResolution | 0x1000,  0x02 }, /* d ..*/
    { kDP_SetMouseResolution | 0x1000,  0x03 }, /* e ..*/
    { kDP_SetMouseScaling1To1,          0x00 }, /* f no send/recv */
};

#define PSMOUSE_CMD_SETSCALE11	0x00e6
#define PSMOUSE_CMD_SETSCALE21	0x00e7
#define PSMOUSE_CMD_SETRES	0x10e8
#define PSMOUSE_CMD_GETINFO	0x03e9
#define PSMOUSE_CMD_SETSTREAM	0x00ea
#define PSMOUSE_CMD_SETPOLL	0x00f0
#define PSMOUSE_CMD_POLL	0x00eb	/* caller sets number of bytes to receive */
#define PSMOUSE_CMD_RESET_WRAP	0x00ec
#define PSMOUSE_CMD_GETID	0x02f2
#define PSMOUSE_CMD_SETRATE	0x10f3
#define PSMOUSE_CMD_ENABLE	0x00f4
#define PSMOUSE_CMD_DISABLE	0x00f5
#define PSMOUSE_CMD_RESET_DIS	0x00f6
#define PSMOUSE_CMD_RESET_BAT	0x02ff

#define PSMOUSE_RET_BAT		0xaa
#define PSMOUSE_RET_ID		0x00
#define PSMOUSE_RET_ACK		0xfa
#define PSMOUSE_RET_NAK		0xfe

static const struct alps_nibble_commands alps_v6_nibble_commands[] = { {
PSMOUSE_CMD_ENABLE, 0x00 }, /* 0 */
{ PSMOUSE_CMD_SETRATE, 0x0a }, /* 1 */
{ PSMOUSE_CMD_SETRATE, 0x14 }, /* 2 */
{ PSMOUSE_CMD_SETRATE, 0x28 }, /* 3 */
{ PSMOUSE_CMD_SETRATE, 0x3c }, /* 4 */
{ PSMOUSE_CMD_SETRATE, 0x50 }, /* 5 */
{ PSMOUSE_CMD_SETRATE, 0x64 }, /* 6 */
{ PSMOUSE_CMD_SETRATE, 0xc8 }, /* 7 */
{ PSMOUSE_CMD_GETID, 0x00 }, /* 8 */
{ PSMOUSE_CMD_GETINFO, 0x00 }, /* 9 */
{ PSMOUSE_CMD_SETRES, 0x00 }, /* a */
{ PSMOUSE_CMD_SETRES, 0x01 }, /* b */
{ PSMOUSE_CMD_SETRES, 0x02 }, /* c */
{ PSMOUSE_CMD_SETRES, 0x03 }, /* d */
{ PSMOUSE_CMD_SETSCALE21, 0x00 }, /* e */
{ PSMOUSE_CMD_SETSCALE11, 0x00 }, /* f */
};

#define ALPS_DUALPOINT          0x02    /* touchpad has trackstick */
#define ALPS_PASS               0x04    /* device has a pass-through port */

#define ALPS_WHEEL              0x08    /* hardware wheel present */
#define ALPS_FW_BK_1            0x10    /* front & back buttons present */
#define ALPS_FW_BK_2            0x20    /* front & back buttons present */
#define ALPS_FOUR_BUTTONS       0x40    /* 4 direction button present */
#define ALPS_PS2_INTERLEAVED    0x80    /* 3-byte PS/2 packet interleaved with
6-byte ALPS packet */

static const struct alps_model_info alps_model_data[] = { 
	{ { 0x32, 0x02, 0x14 }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_PASS | ALPS_DUALPOINT }, /* Toshiba Salellite Pro M10 */
    { { 0x33, 0x02, 0x0a }, 0x00, ALPS_PROTO_V1, 0x88, 0xf8, 0 },               /* UMAX-530T */
	{ { 0x53, 0x02, 0x0a }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, 0 }, 
	{ { 0x53, 0x02,	0x14 }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, 0 }, 
	{ { 0x60, 0x03, 0xc8 }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, 0 }, /* HP ze1115 */
	{ { 0x63, 0x02, 0x0a }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, 0 }, 
	{ { 0x63, 0x02,	0x14 }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, 0 }, 
	{ { 0x63, 0x02, 0x28 },	0x00, ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_FW_BK_2 }, /* Fujitsu Siemens S6010 */
    { { 0x63, 0x02, 0x3c }, 0x00, ALPS_PROTO_V2, 0x8f, 0x8f, ALPS_WHEEL },      /* Toshiba Satellite S2400-103 */
    { { 0x63, 0x02, 0x50 }, 0x00, ALPS_PROTO_V2, 0xef, 0xef, ALPS_FW_BK_1 },    /* NEC Versa L320 */
	{ { 0x63, 0x02, 0x64 }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, 0 }, 
	{ { 0x63, 0x03,	0xc8 }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_PASS | ALPS_DUALPOINT }, /* Dell Latitude D800 */
	{ { 0x73, 0x00, 0x0a }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_DUALPOINT }, /* ThinkPad R61 8918-5QG */
	{ { 0x73, 0x02, 0x0a }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, 0 }, 
	{ { 0x73, 0x02,	0x14 }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_FW_BK_2 }, /* Ahtec Laptop */
	{ { 0x20, 0x02, 0x0e }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_PASS | ALPS_DUALPOINT }, /* XXX */
	{ { 0x22, 0x02, 0x0a }, 0x00, ALPS_PROTO_V2, 0xf8, 0xf8, ALPS_PASS | ALPS_DUALPOINT }, 
	{ { 0x22, 0x02, 0x14 }, 0x00, ALPS_PROTO_V2, 0xff, 0xff, ALPS_PASS | ALPS_DUALPOINT }, /* Dell Latitude D600 */
        /* Dell Latitude E5500, E6400, E6500, Precision M4400 */
    { { 0x62, 0x02, 0x14 }, 0x00, ALPS_PROTO_V2, 0xcf, 0xcf, ALPS_PASS | ALPS_DUALPOINT | ALPS_PS2_INTERLEAVED },
    { { 0x73, 0x02, 0x50 }, 0x00, ALPS_PROTO_V2, 0xcf, 0xcf, ALPS_FOUR_BUTTONS },       
        /* Dell Vostro 1400 */
    { { 0x52, 0x01, 0x14 }, 0x00, ALPS_PROTO_V2, 0xff, 0xff, ALPS_PASS | ALPS_DUALPOINT | ALPS_PS2_INTERLEAVED },                
        /* Toshiba Tecra A11-11L */
	{ { 0x52, 0x01, 0x14 }, 0x00, ALPS_PROTO_V2, 0xff, 0xff, ALPS_PASS | ALPS_DUALPOINT | ALPS_PS2_INTERLEAVED }, 
	{ { 0x73, 0x02, 0x64 },	0x9b, ALPS_PROTO_V3, 0x8f, 0x8f, ALPS_DUALPOINT }, 
	{ { 0x73, 0x02, 0x64 }, 0x9d, ALPS_PROTO_V3, 0x8f, 0x8f, ALPS_DUALPOINT }, 
	{ { 0x73, 0x02, 0x64 }, 0x8a, ALPS_PROTO_V4, 0x8f, 0x8f, 0 },
	/* Dell Latitude E6230 (no trackstick), E6430, E6530 */
	{ { 0x73, 0x03, 0x0a }, 0x1d, ALPS_PROTO_V5, 0x8f, 0x8f, ALPS_DUALPOINT },
	/* Dell Inspiron N5110 */
	{ { 0x73, 0x03, 0x50 }, 0x0d, ALPS_PROTO_V6, 0xc8, 0xc8, 0 },
	/* Dell Inspiron 17R 7720 */
	{ { 0x73, 0x03, 0x50 }, 0x02, ALPS_PROTO_V6, 0xc8, 0xc8, 0 }, };

// =============================================================================
// ApplePS2ALPSGlidePoint Class Implementation
//

OSDefineMetaClassAndStructors(ApplePS2ALPSGlidePoint, VoodooPS2TouchPadBase
);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

ApplePS2ALPSGlidePoint *ApplePS2ALPSGlidePoint::probe(IOService *provider, SInt32 *score) {
    DEBUG_LOG("ApplePS2ALPSGlidePoint::probe entered...\n");
    bool success;

    //
    // The driver has been instructed to verify the presence of the actual
    // hardware we represent. We are guaranteed by the controller that the
    // mouse clock is enabled and the mouse itself is disabled (thus it
    // won't send any asynchronous mouse data that may mess up the
    // responses expected by the commands we send it).
    //
    
    _device = (ApplePS2MouseDevice *) provider;
    
    _device->lock();
    resetMouse();
    
    if (identify() != 0) {
        success = false;
    } else {
        success = true;
        _bounds.maxx = modelData.x_max;
        _bounds.maxy = modelData.y_max;
    }
    _device->unlock();
    
    _device = 0;

    DEBUG_LOG("ApplePS2ALPSGlidePoint::probe leaving.\n");

    return success ? this : 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::afterInstallInterrupt() {

	DEBUG_LOG( "afterInterruptInstall  - AlpsGlidepoint\n" );

	enterCommandMode();

	_device-> setCommandByte( kCB_EnableMouseIRQ, kCB_DisableMouseClock );
	exitCommandMode();
		setTouchPadEnable( true );
}
void ApplePS2ALPSGlidePoint::afterDeviceUnlock() {
	DEBUG_LOG( " afterDeviceUnlock  - AlpsGlidepoint\n" );

}

bool ApplePS2ALPSGlidePoint::deviceSpecificInit() {
    
    resetMouse();
    
    if (identify() != 0) {
        goto init_fail;
    }

    // Setup expected packet size
    modelData.pktsize = modelData.proto_version == ALPS_PROTO_V4 ? 8 : 6;

	IOLog("Initializing TouchPad hardware...this may take a second - version : %d.\n",
			modelData.proto_version );

    if (!(this->*hw_init)()) {
        goto init_fail;
    }

    IOLog("Touchpad initialization complete.\n");

    return true;

init_fail:
    IOLog("%s: Device initialization failed. Touchpad probably won't work\n", getName());
    resetMouse();
    return false;
}

bool ApplePS2ALPSGlidePoint::init(OSDictionary *dict) {
    if (!super::init(dict)) {
        return false;
    }

    // Set defaults for this mouse model
	z_finger = 8;
    zlimit = 255;
    ledge = 0;
    setupMaxes();
    tedge = 0;
    hscroll = vscroll = false;
    vscrolldivisor = 0;
    hscrolldivisor = 0;
    divisorx = 40;
    divisory = 40;
    hscrolldivisor = 50;
    vscrolldivisor = 50;
    _buttonCount = 3;
    
	scrolldxthresh = 0;
	scrolldythresh = 0;

    dragexitdelay = 600000000;
    dragTimer = 0;
    
    return true;
}

void ApplePS2ALPSGlidePoint::setupMaxes() {
    centerx = modelData.x_max / 2;
    centery = modelData.y_max / 2;
    // Right edge, must allow for vertical scrolling
    redge = modelData.x_max - 250;
    bedge = modelData.y_max;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::stop(IOService *provider) {
    resetMouse();

    super::stop(provider);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool ApplePS2ALPSGlidePoint::resetMouse() {
    TPS2Request<3> request;

    // Reset mouse
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_Reset;
    request.commands[1].command = kPS2C_ReadDataPort;
    request.commands[1].inOrOut = 0;
    request.commands[2].command = kPS2C_ReadDataPort;
    request.commands[2].inOrOut = 0;
    request.commandsCount = 3;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);

    // Verify the result
    if (request.commands[1].inOrOut != kSC_Reset && request.commands[2].inOrOut != kSC_ID) {
        DEBUG_LOG("Failed to reset mouse, return values did not match. [0x%02x, 0x%02x]\n", request.commands[1].inOrOut, request.commands[2].inOrOut);
        return false;
    }
    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

PS2InterruptResult ApplePS2ALPSGlidePoint::interruptOccurred(UInt8 data) {
    //
    // This will be invoked automatically from our device when asynchronous
    // events need to be delivered. Process the trackpad data. Do NOT issue
    // any BLOCKING commands to our device in this context.
    //
    // Ignore all bytes until we see the start of a packet, otherwise the
    // packets may get out of sequence and things will get very confusing.
    //

    // Right now this checks if the packet is either a PS/2 packet (data & 0xc8)
    // or if the first packet matches the specific trackpad first packet
//    if (0 == _packetByteCount && (data & 0xc8) != 0x08 && (data & modelData.mask0) != modelData.byte0) {
//        DEBUG_LOG("%s: Unexpected byte0 data (%02x) from PS/2 controller\n", getName(), data);
//        return kPS2IR_packetBuffering;
//    }
//
//    /* Bytes 2 - packet size should have 0 in highest bit */
//    if (_packetByteCount >= 1 && data == 0x80) {
//        DEBUG_LOG("%s: Unexpected byte%d data (%02x) from PS/2 controller\n", getName(), _packetByteCount, data);
//        _packetByteCount = 0;
//        return kPS2IR_packetBuffering;
//    }

    UInt8 *packet = _ringBuffer.head();
    packet[_packetByteCount++] = data;

    if (modelData.pktsize == _packetByteCount ||
            (kPacketLengthSmall == _packetByteCount && (packet[0] & 0xc8) == 0x08)) {
        // complete 6/8 or 3-byte packet received...
        // 3-byte packet is bare PS/2 packet instead of ALPS specific packet
        _ringBuffer.advanceHead(modelData.pktsize);
        _packetByteCount = 0;
        return kPS2IR_packetReady;
    }
    return kPS2IR_packetBuffering;
}

void ApplePS2ALPSGlidePoint::packetReady() {
    // empty the ring buffer, dispatching each packet...
    while (_ringBuffer.count() >= modelData.pktsize) {
        UInt8 *packet = _ringBuffer.tail();
        // now we have complete packet, either 6-byte or 3-byte
		DEBUG_LOG( "ps2: packet = { %02x, %02x, %02x, %02x, %02x, %02x }\n",
				packet[0], packet[1], packet[2], packet[3], packet[4],
				packet[5] );

        if ((packet[0] & modelData.mask0) == modelData.byte0) {
            DEBUG_LOG("ps2: Got pointer event with packet = { %02x, %02x, %02x, %02x, %02x, %02x }\n", packet[0], packet[1], packet[2], packet[3], packet[4], packet[5]);
            (this->*process_packet)(packet);
            _ringBuffer.advanceTail(modelData.pktsize);
        } else {
            DEBUG_LOG("ps2: Intercepted bare PS/2 packet..ignoring\n");
            // Ignore bare PS/2 packet for now...messes with the actual full 6-byte ALPS packet above
//            dispatchRelativePointerEventWithPacket(packet, kPacketLengthSmall);
            _ringBuffer.advanceTail(kPacketLengthSmall);
        }
    }
}

void ApplePS2ALPSGlidePoint::processPacketV1V2(UInt8 *packet) {
    int x, y, z, ges, fin, left, right, middle, buttons = 0, fingers = 0;
    int back = 0, forward = 0;
    uint64_t now_abs;

    clock_get_uptime(&now_abs);

    if (modelData.proto_version == ALPS_PROTO_V1) {
        left = packet[2] & 0x10;
        right = packet[2] & 0x08;
        middle = 0;
        x = packet[1] | ((packet[0] & 0x07) << 7);
        y = packet[4] | ((packet[3] & 0x07) << 7);
        z = packet[5];
    } else {
        left = packet[3] & 1;
        right = packet[3] & 2;
        middle = packet[3] & 4;
        x = packet[1] | ((packet[2] & 0x78) << (7 - 3));
        y = packet[4] | ((packet[3] & 0x70) << (7 - 4));
        z = packet[5];
    }
    
    if (modelData.flags & ALPS_FW_BK_1) {
        back = packet[0] & 0x10;
        forward = packet[2] & 4;
    }
    
    if (modelData.flags & ALPS_FW_BK_2) {
        back = packet[3] & 4;
        forward = packet[2] & 4;
        if ((middle = forward && back)) {
            forward = back = 0;
        }
    }

    buttons |= left ? 0x01 : 0;
    buttons |= right ? 0x02 : 0;
    buttons |= middle ? 0x04 : 0;

    ges = packet[2] & 1;
    fin = packet[2] & 2;

    if ((modelData.flags & ALPS_DUALPOINT) && z == 127) {
        int dx, dy;
        dx = x > 383 ? (x - 768) : x;
        dy = -(y > 255 ? (y - 512) : y);
        // I think this means it is a trackstick packet....
        // if so we don't need all the extra logic...only movement
        DEBUG_LOG("dispatch trackstick movement dx=%d, dy=%d\n", dx, dy);
        dispatchRelativePointerEventX(dx, dy, buttons, now_abs);
        return;
    }
    
    /* Convert hardware tap to a reasonable Z value */
    if (ges && !fin) {
        z = z_finger + 1;
    }

    /*
     * A "tap and drag" operation is reported by the hardware as a transition
     * from (!fin && ges) to (fin && ges). This should be translated to the
     * sequence Z>0, Z==0, Z>0, so the Z==0 event has to be generated manually.
     */
    if (ges && fin && !modelData.prev_fin) {
        // TODO: Not quite sure if this is correct but sounds like it should be
        // generating a drag...so mark the touchmode now as dragging??
        DEBUG_LOG("switch to drag mode\n");
        touchmode = MODE_DRAG;
    }
    modelData.prev_fin = fin;
    
    if (z > 0) {
        fingers++;

        /*
         * Arbitrary value. The z value increases as more fingers are
         * on the trackpad, taken from the Slice version where he integrated
         * 2-finger scrolling in this way, also analyzed some z values from
         * someone else testing this out
         */
        if (z >= 98) {
            fingers++;
        }
    }
    

    // TODO: would be nice to have someone verify this behavior, not sure if
    // this is a correct translation for this hardware...much different than
    // the other ones
    dispatchEventsWithInfo(x, y, z, fingers, buttons);
    
    if (modelData.flags & ALPS_WHEEL) {
        // TODO: get verification that this works correctly
        //input_report_rel(dev, REL_WHEEL, ((packet[2] << 1) & 0x08) - ((packet[0] >> 4) & 0x07));
        int scrollAmount = ((packet[2] << 1) & 0x08) - ((packet[0] >> 4) & 0x07);
        if (scrollAmount) {
            DEBUG_LOG("dispatch scroll wheel event, scroll=%d\n", scrollAmount);
            dispatchScrollWheelEventX(scrollAmount, 0, 0, now_abs);
        }
    }

    // TODO: send back and forward events
//  if (priv->flags & (ALPS_FW_BK_1 | ALPS_FW_BK_2)) {
//      input_report_key(dev, BTN_FORWARD, forward);
//      input_report_key(dev, BTN_BACK, back);
//  }

    // TODO: not sure what this is...
//  if (priv->flags & ALPS_FOUR_BUTTONS) {
//      input_report_key(dev, BTN_0, packet[2] & 4);
//      input_report_key(dev, BTN_1, packet[0] & 0x10);
//      input_report_key(dev, BTN_2, packet[3] & 4);
//      input_report_key(dev, BTN_3, packet[0] & 0x20);
//  }

}

void ApplePS2ALPSGlidePoint::processPacketV3(UInt8 *packet) {
    /*
     * v3 protocol packets come in three types, two representing
     * touchpad data and one representing trackstick data.
     * Trackstick packets seem to be distinguished by always
     * having 0x3f in the last byte. This value has never been
     * observed in the last byte of either of the other types
     * of packets.
     */
    if (packet[5] == 0x3f) {
        processTrackstickPacketV3(packet);
        return;
    }

    processTouchpadPacketV3(packet);
}

void ApplePS2ALPSGlidePoint::processTrackstickPacketV3(UInt8 *packet) {
    int x, y, z, left, right, middle;
    uint64_t now_abs;
    UInt32 buttons = 0, raw_buttons = 0;

    if (!(packet[0] & 0x40)) {
        DEBUG_LOG("ps2: bad trackstick packet, disregarding...\n");
        return;
    }

    /* There is a special packet that seems to indicate the end
     * of a stream of trackstick data. Filter these out
     */
    if (packet[1] == 0x7f && packet[2] == 0x7f && packet[3] == 0x7f) {
        DEBUG_LOG("ps2: ignoring trackstick packet that indicates end of stream\n");
        return;
    }

    x = (SInt8) (((packet[0] & 0x20) << 2) | (packet[1] & 0x7f));
    y = (SInt8) (((packet[0] & 0x10) << 3) | (packet[2] & 0x7f));
    z = (packet[4] & 0x7c) >> 2;

    // TODO: separate divisor for trackstick
//    x /= divisorx;
//    y /= divisory;

    clock_get_uptime(&now_abs);

    left = packet[3] & 0x01;
    right = packet[3] & 0x02;
    middle = packet[3] & 0x04;
    
    if (!(modelData.quirks & ALPS_QUIRK_TRACKSTICK_BUTTONS) &&
        (left || middle || right)) {
        modelData.quirks |= ALPS_QUIRK_TRACKSTICK_BUTTONS;
    }
    
    raw_buttons |= left ? 0x01 : 0;
    raw_buttons |= right ? 0x02 : 0;
    raw_buttons |= middle ? 0x04 : 0;

    // Reverse y value to get proper movement direction
    y = -y;
    
    // Sometimes, a big value can spit out, so we must remove it...
    if ((abs(x) >= 0x7f) && (abs(y) >= 0x7f)) {
        x = y = 0;
    }
    // Button status can appear in normal packet...
    if (0 == raw_buttons) {
        buttons = lastbuttons;
    } else {
        buttons = raw_buttons;
        lastbuttons = buttons;
    }
    
    lastx2 = x; lasty2 = y;
    
    ignoreall = FALSE;
    if ((0 != x) || (0 != y)) {
        ignoreall = TRUE;
    }
    
    // normal mode: middle button is not pressed or no movement made
    if ( ((0 == x) && (0 == y)) || (0 == (buttons & 0x04))) {
        y += y >> 1; x += x >> 1;
        DEBUG_LOG("ps2: trackStick: dispatch relative pointer with x=%d, y=%d, tbuttons = %d, buttons=%d, (z=%d, not reported)\n",
                  x, y, raw_buttons, buttons, z);
        dispatchRelativePointerEventX(x, y, buttons, now_abs);
    } else {
        // scroll mode
        y = -y; x = -x;
        DEBUG_LOG("ps2: trackStick: dispatchScrollWheelEventX: dv=%d, dh=%d\n", y, x);
        dispatchScrollWheelEventX(y, x, 0, now_abs);
    }
}

void ApplePS2ALPSGlidePoint::processTouchpadPacketV3(UInt8 *packet) {
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    int fingers = 0, bmapFingers;
    UInt32 buttons = 0;
    uint64_t now_abs;
    struct alps_fields f;

    clock_get_uptime(&now_abs);
    
    (this->*decode_fields)(&f, packet);

    /*
     * There's no single feature of touchpad position and bitmap packets
     * that can be used to distinguish between them. We rely on the fact
     * that a bitmap packet should always follow a position packet with
     * bit 6 of packet[4] set.
     */
    if (modelData.multi_packet) {
        DEBUG_LOG("ps2: trackPad: handling multi-packet\n");
        /*
         * Sometimes a position packet will indicate a multi-packet
         * sequence, but then what follows is another position
         * packet. Check for this, and when it happens process the
         * position packet as usual.
         */
        if (f.is_mp) {
            fingers = f.fingers;
            bmapFingers = processBitmap(f.x_map, f.y_map, &x1, &y1, &x2, &y2);
            
            DEBUG_LOG("ps2: trackPad: fingers=%d, bmapFingers=%d\n", fingers, bmapFingers);
            
            /*
             * We shouldn't report more than one finger if
             * we don't have two coordinates.
             */
            if (fingers > 1 && bmapFingers < 2) {
                fingers = bmapFingers;
            }
            
            (this->*decode_fields)(&f, modelData.multi_data);
        } else {
            modelData.multi_packet = 0;
        }
    }

    /*
     * Bit 6 of byte 0 is not usually set in position packets. The only
     * times it seems to be set is in situations where the data is
     * suspect anyway, e.g. a palm resting flat on the touchpad. Given
     * this combined with the fact that this bit is useful for filtering
     * out misidentified bitmap packets, we reject anything with this
     * bit set.
     */
    if (f.is_mp) {
        return;
    }

    if (!modelData.multi_packet && (f.first_mp)) {
        DEBUG_LOG("ps2: trackPad: detected multi-packet first packet, waiting to handle\n");
        modelData.multi_packet = 1;
        memcpy(modelData.multi_data, packet, sizeof(modelData.multi_data));
        return;
    }

    modelData.multi_packet = 0;

    /*
     * Sometimes the hardware sends a single packet with z = 0
     * in the middle of a stream. Real releases generate packets
     * with x, y, and z all zero, so these seem to be flukes.
     * Ignore them.
     */
    if (f.x && f.y && !f.z) {
        return;
    }

    /*
     * If we don't have MT data or the bitmaps were empty, we have
     * to rely on ST data.
     */
    if (!fingers) {
        x1 = f.x;
        y1 = f.y;
        fingers = f.z > 0 ? 1 : 0;
    }

    buttons |= f.left ? 0x01 : 0;
    buttons |= f.right ? 0x02 : 0;
    buttons |= f.middle ? 0x04 : 0;
    
    if (!(modelData.quirks & ALPS_QUIRK_TRACKSTICK_BUTTONS)) {
        buttons |= f.ts_left ? 0x01 : 0;
        buttons |= f.ts_right ? 0x02 : 0;
        buttons |= f.ts_middle ? 0x04 : 0;
    }
    
    dispatchEventsWithInfo(f.x, f.y, f.z, fingers, buttons);
}

void ApplePS2ALPSGlidePoint::decodeButtonsV3(struct alps_fields *fields, UInt8 *packet) {
    fields->left = !!(packet[3] & 0x01);
    fields->right = !!(packet[3] & 0x02);
    fields->middle = !!(packet[3] & 0x04);
    
    fields->ts_left = !!(packet[3] & 0x10);
    fields->ts_right = !!(packet[3] & 0x20);
    fields->ts_middle = !!(packet[3] & 0x40);
}

void ApplePS2ALPSGlidePoint::decodePinnacle(struct alps_fields *f, UInt8 *p) {
    f->first_mp = !!(p[4] & 0x40);
    f->is_mp = !!(p[0] & 0x40);
    
    f->fingers = (p[5] & 0x3) + 1;
    f->x_map = ((p[4] & 0x7e) << 8) |
    ((p[1] & 0x7f) << 2) |
    ((p[0] & 0x30) >> 4);
    f->y_map = ((p[3] & 0x70) << 4) |
    ((p[2] & 0x7f) << 1) |
    (p[4] & 0x01);
    
    f->x = ((p[1] & 0x7f) << 4) | ((p[4] & 0x30) >> 2) |
    ((p[0] & 0x30) >> 4);
    f->y = ((p[2] & 0x7f) << 4) | (p[4] & 0x0f);
    f->z = p[5] & 0x7f;
    
    decodeButtonsV3(f, p);
}

void ApplePS2ALPSGlidePoint::decodeRushmore(struct alps_fields *f, UInt8 *p) {
    decodePinnacle(f, p);
    
    f->x_map |= (p[5] & 0x10) << 11;
    f->y_map |= (p[5] & 0x20) << 6;
}

void ApplePS2ALPSGlidePoint::decodeDolphin(struct alps_fields *f, UInt8 *p) {
    f->first_mp = !!(p[0] & 0x02);
    f->is_mp = !!(p[0] & 0x20);
    
    f->fingers = ((p[0] & 0x6) >> 1 |
                  (p[0] & 0x10) >> 2);
    f->x_map = ((p[2] & 0x60) >> 5) |
    ((p[4] & 0x7f) << 2) |
    ((p[5] & 0x7f) << 9) |
    ((p[3] & 0x07) << 16) |
    ((p[3] & 0x70) << 15) |
    ((p[0] & 0x01) << 22);
    f->y_map = (p[1] & 0x7f) |
    ((p[2] & 0x1f) << 7);
    
    f->x = ((p[1] & 0x7f) | ((p[4] & 0x0f) << 7));
    f->y = ((p[2] & 0x7f) | ((p[4] & 0xf0) << 3));
    f->z = (p[0] & 4) ? 0 : p[5] & 0x7f;
    
    decodeButtonsV3(f, p);
}

//For single-touch, the 6-byte packet format is:

// byte 0:    1    1    0    0    1    0    0    0
// byte 1:    0   x6   x5   x4   x3   x2   x1   x0
// byte 2:    0   y6   y5   y4   y3   y2   y1   y0
// byte 3:    0    M    R    L    1    m    r    l
// byte 4:   y10  y9   y8   y7  x10   x9   x8   x7
// byte 5:    0   z6   z5   z4   z3   z2   z1   z0
//
//For mt, the format is:
//
// byte 0:    1    1    1    n3   1   n2   n1   x24
// byte 1:    1   y7   y6    y5  y4   y3   y2    y1
// byte 2:    ?   x2   x1   y12 y11  y10   y9    y8
// byte 3:    0  x23  x22   x21 x20  x19  x18   x17
// byte 4:    0   x9   x8    x7  x6   x5   x4    x3
// byte 5:    0  x16  x15   x14 x13  x12  x11   x10

void ApplePS2ALPSGlidePoint::processPacketV6(UInt8 *packet) {
	//single touch byte 0:    1    1    0    0    1    0    0    0
	if ( packet[0] == 0xc8 ) {
		processPacketV6SingleTouch( packet );
	} else if ((packet[0] & 0xC8) == 0xC8) {
		processPacketV6MultiTouch( packet );
	}else {
        DEBUG_LOG("alps mt : filter packet !!!!\n");
    }

}

void ApplePS2ALPSGlidePoint::processPacketV6SingleTouch(UInt8 *packet) {
	int x, y, z, left, right, middle;
	uint64_t now_abs;
	UInt32 buttons = 0, raw_buttons = 0;

	//calculate x,y,z

	x = ( SInt16 )( ( ( packet[4] & 0x0f ) << 7 ) | ( packet[1] & 0x7f ) );
	y = ( SInt16 )( ( ( packet[4] & 0xf0 ) << 3 ) | ( packet[2] & 0x7f ) );
	z = ( SInt16 )( packet[5] & 0x7f );

	clock_get_uptime( &now_abs );
	//calculate buttons

	left = packet[3] & 0x01;
	right = packet[3] & 0x02;
	middle = packet[3] & 0x04;

	raw_buttons |= left ? 0x01 : 0;
	raw_buttons |= right ? 0x02 : 0;
	raw_buttons |= middle ? 0x04 : 0;

	// Sometimes, a big value can spit out, so we must remove it...
	//    if ((abs(x) >= 0x7f) && (abs(y) >= 0x7f)) {
	//        x = y = 0;
	//    }
	// Button status can appear in normal packet...
	if (0 == raw_buttons) {
		buttons = lastbuttons;
	} else {
		buttons = raw_buttons;
		lastbuttons = buttons;
	}

	dispatchEventsWithInfo( x, y, z, 1, raw_buttons );

}

void ApplePS2ALPSGlidePoint::processPacketV6MultiTouch(UInt8 *packet) {
	UInt64 x_map, y_map;
	SInt32 x1, y1, x2, y2,x,y,z;
	int  left, right, middle;
	UInt32 buttons = 0, raw_buttons = 0;

	//SInt32 z = 15;
	UInt32 x_bitmap, y_bitmap;
	int nrOfFingers, fingersFromBitMap;

	DEBUG_LOG( "ProcessPacket v6 - multi-touch Packet" );

	modelData.multi_packet = ( packet[0] & 0x20 ) == 0x20;

	// one packet with 0x20 , second packet match to 0x02;

	if (modelData.multi_packet) {
		nrOfFingers = ( ( ( packet[0] & 0x10 ) >> 1 ) | ( packet[0] & 0x06 ) );
        nrOfFingers = nrOfFingers/2;
		x_map = ( ( packet[2] & 0x60 ) >> 5 ) | ( ( packet[4] & 0x7f ) << 2 )
				| ( ( packet[5] & 0x7f ) << 9 ) | ( ( packet[3] & 0x07 ) << 16 )
				| ( ( packet[3] & 0x70 ) << 15 )
				| ( ( packet[0] & 0x01 ) << 22 );
		y_map = ( packet[1] & 0x7f ) | ( ( packet[2] & 0x1f ) << 7 );
		DEBUG_LOG( "alps mt :  trackpad : number of fingers :%d\n ",
				nrOfFingers );
		fingersFromBitMap = processBitmap( x_map, y_map, &x1, &y1, &x2, &y2 );
		modelData.fingers = nrOfFingers;
		modelData.x1 = x1;
		modelData.x2 = x2;
		modelData.y1 = y1;
		modelData.y2 = y2;
		return;
	}

	x = ( SInt16 )( ( ( packet[4] & 0x0f ) << 7 ) | ( packet[1] & 0x7f ) );
	y = ( SInt16 )( ( ( packet[4] & 0xf0 ) << 3 ) | ( packet[2] & 0x7f ) );
	z = ( SInt16 )( packet[5] & 0x7f );

	left = packet[3] & 0x10;
	right = packet[3] & 0x20;
    middle = packet[3] & 0x40;

	raw_buttons |= left ? 0x01 : 0;
	raw_buttons |= right ? 0x02 : 0;
	raw_buttons |= middle ? 0x04 : 0;

	DEBUG_LOG( "alps mt :  2ndpacket :x=%d,y=%d,z=%d ,buttons = %d\n ", x,y,z,buttons );

//	if (0 == raw_buttons) {
//		buttons = lastbuttons;
//	} else {
//		buttons = raw_buttons;
//		lastbuttons = buttons;
//	}

	dispatchEventsWithInfo( x, y, z, modelData.fingers, raw_buttons );

}

void ApplePS2ALPSGlidePoint::processPacketV4(UInt8 *packet) {
    SInt32 offset;
    SInt32 x, y, z;
    SInt32 left, right;
    SInt32 x1, y1, x2, y2;
    SInt32 fingers = 0;
    UInt32 x_bitmap, y_bitmap;
    UInt32 buttons = 0;
    
    /*
     * v4 has a 6-byte encoding for bitmap data, but this data is
     * broken up between 3 normal packets. Use priv->multi_packet to
     * track our position in the bitmap packet.
     */
    if (packet[6] & 0x40) {
        /* sync, reset position */
        modelData.multi_packet = 0;
    }
    
    if (modelData.multi_packet > 2) {
        IOLog("WARNING: multipacket size > 2\n");
        return;
    }
    
    offset = 2 * modelData.multi_packet;
    modelData.multi_data[offset] = packet[6];
    modelData.multi_data[offset + 1] = packet[7];
    
    if (++modelData.multi_packet > 2) {
        modelData.multi_packet = 0;
        
        x_bitmap = ((modelData.multi_data[2] & 0x1f) << 10) |
        ((modelData.multi_data[3] & 0x60) << 3) |
        ((modelData.multi_data[0] & 0x3f) << 2) |
        ((modelData.multi_data[1] & 0x60) >> 5);
        y_bitmap = ((modelData.multi_data[5] & 0x01) << 10) |
        ((modelData.multi_data[3] & 0x1f) << 5) |
        (modelData.multi_data[1] & 0x1f);
        
        fingers = processBitmap(x_bitmap, y_bitmap, &x1, &y1, &x2, &y2);
        
        /* Store MT data.*/
        modelData.fingers = fingers;
        modelData.x1 = x1;
        modelData.x2 = x2;
        modelData.y1 = y1;
        modelData.y2 = y2;
    }
    
    left = packet[4] & 0x01;
    right = packet[4] & 0x02;
    
    x = ((packet[1] & 0x7f) << 4) | ((packet[3] & 0x30) >> 2) |
    ((packet[0] & 0x30) >> 4);
    y = ((packet[2] & 0x7f) << 4) | (packet[3] & 0x0f);
    z = packet[5] & 0x7f;
    
    /*
     * If there were no contacts in the bitmap, use ST
     * points in MT reports.
     * If there were two contacts or more, report MT data.
     */
    if (modelData.fingers < 2) {
        x1 = x;
        y1 = y;
        fingers = z > 0 ? 1 : 0;
    } else {
        fingers = modelData.fingers;
        x1 = modelData.x1;
        x2 = modelData.x2;
        y1 = modelData.y1;
        y2 = modelData.y2;
    }
    
    buttons |= left ? 0x01 : 0;
    buttons |= right ? 0x02 : 0;
    
    dispatchEventsWithInfo(x, y, z, fingers, buttons);
}

void ApplePS2ALPSGlidePoint::dispatchEventsWithInfo(int xraw, int yraw, int z, int fingers, UInt32 buttonsraw) {
    uint64_t now_abs;
    clock_get_uptime(&now_abs);
    uint64_t now_ns;
    absolutetime_to_nanoseconds(now_abs, &now_ns);

    DEBUG_LOG("%s::dispatchEventsWithInfo: x=%d, y=%d, z=%d, fingers=%d, buttons=%d\n",
    getName(), xraw, yraw, z, fingers, buttonsraw);
    
    // scale x & y to the axis which has the most resolution
    if (xupmm < yupmm) {
        xraw = xraw * yupmm / xupmm;
    } else if (xupmm > yupmm) {
        yraw = yraw * xupmm / yupmm;
    }
    int x = xraw;
    int y = yraw;

    // allow middle click to be simulated the other two physical buttons
    UInt32 buttons = buttonsraw;
    lastbuttons = buttons;

    // allow middle button to be simulated with two buttons down
    if (!clickpadtype || fingers == 3) {
        buttons = middleButton(buttons, now_abs, fingers == 3 ? fromPassthru : fromTrackpad);
        DEBUG_LOG("New buttons value after check for middle click: %d\n", buttons);
    }

    // recalc middle buttons if finger is going down
    if (0 == last_fingers && fingers > 0) {
        buttons = middleButton(buttonsraw | passbuttons, now_abs, fromCancel);
    }

    if (last_fingers > 0 && fingers > 0 && last_fingers != fingers) {
        DEBUG_LOG("Start ignoring delta with finger change\n");
        // ignore deltas for a while after finger change
        ignoredeltas = ignoredeltasstart;
    }

    if (last_fingers != fingers) {
        DEBUG_LOG("Finger change, reset averages\n");
        // reset averages after finger change
        x_undo.reset();
        y_undo.reset();
        x_avg.reset();
        y_avg.reset();
    }

    // unsmooth input (probably just for testing)
    // by default the trackpad itself does a simple decaying average (1/2 each)
    // we can undo it here
    if (unsmoothinput) {
        x = x_undo.filter(x);
        y = y_undo.filter(y);
    }

    // smooth input by unweighted average
    if (smoothinput) {
        x = x_avg.filter(x);
        y = y_avg.filter(y);
    }

    if (ignoredeltas) {
        DEBUG_LOG("Still ignoring deltas. Value=%d\n", ignoredeltas);
        lastx = x;
        lasty = y;
        if (--ignoredeltas == 0) {
            x_undo.reset();
            y_undo.reset();
            x_avg.reset();
            y_avg.reset();
        }
    }

    // deal with "OutsidezoneNoAction When Typing"
    if (outzone_wt && z > z_finger && now_ns - keytime < maxaftertyping &&
            (x < zonel || x > zoner || y < zoneb || y > zonet)) {
        DEBUG_LOG("Ignore touch input after typing\n");
        // touch input was shortly after typing and outside the "zone"
        // ignore it...
        return;
    }

    // if trackpad input is supposed to be ignored, then don't do anything
    if (ignoreall) {
        DEBUG_LOG("ignoreall is set, returning\n");
        return;
    }

#ifdef DEBUG_VERBOSE
    int tm1 = touchmode;
#endif

    if (z < z_finger && isTouchMode()) {
        // Finger has been lifted
        DEBUG_LOG("finger lifted after touch\n");
        xrest = yrest = scrollrest = 0;
        inSwipeLeft = inSwipeRight = inSwipeUp = inSwipeDown = 0;
        inSwipe4Left = inSwipe4Right = inSwipe4Up = inSwipe4Down = 0;
        xmoved = ymoved = 0;
        untouchtime = now_ns;
        tracksecondary = false;

		if (dy_history.count() || dx_history.count()) {
			DEBUG_LOG(
					"ps2: newest=%llu, oldest=%llu, diff=%llu, avg_y: %d/%d=%d , avg_x:%d/%d=%d\n",
					time_history.newest(), time_history.oldest(),
					time_history.newest() - time_history.oldest(),
					dy_history.sum(), dy_history.count(),
					dy_history.average(),
					dx_history.sum(), dx_history.count(),
					dx_history.average()
			);
		} else {
            DEBUG_LOG("ps2: no time/dy history\n");
        }

        // check for scroll momentum start
        if (MODE_MTOUCH == touchmode && momentumscroll && momentumscrolltimer) {
            // releasing when we were in touchmode -- check for momentum scroll
			if ((dy_history.count() > momentumscrollsamplesmin || dx_history.count()> momentumscrollsamplesmin  )
					&& ( momentumscrollinterval = time_history.newest()
							- time_history.oldest() )) {
				momentumscrollsum_y = dy_history.sum();
				momentumscrollsum_x = dx_history.sum();
				momentumscrollcurrent_y = momentumscrolltimer * -momentumscrollsum_y;
				momentumscrollcurrent_x = momentumscrolltimer * -momentumscrollsum_x;
				momentumscrollrest1_x = 0;
				momentumscrollrest1_y = 0;
				momentumscrollrest2_x = 0;
				momentumscrollrest2_y = 0;
                setTimerTimeout(scrollTimer, momentumscrolltimer);
            }
        }
        time_history.reset();
        dy_history.reset();
		dx_history.reset();
		DEBUG_LOG( "ps2: now_ns-touchtime=%lld (%s). touchmode=%d\n",
				(uint64_t) (now_ns - touchtime) / 1000,
				now_ns - touchtime < maxtaptime ? "true" : "false", touchmode );
        if (now_ns - touchtime < maxtaptime && clicking) {
            switch (touchmode) {
                case MODE_DRAG:
                    if (!immediateclick) {
                        buttons &= ~0x7;
                        dispatchRelativePointerEventX(0, 0, buttons | 0x1, now_abs);
                        dispatchRelativePointerEventX(0, 0, buttons, now_abs);
                    }
                    if (wastriple && rtap) {
                        buttons |= !swapdoubletriple ? 0x4 : 0x02;
                    } else if (wasdouble && rtap) {
                        buttons |= !swapdoubletriple ? 0x2 : 0x04;
                    } else {
                        buttons |= 0x1;
                    }
                    touchmode = MODE_NOTOUCH;
                    break;

                case MODE_DRAGLOCK:
                    touchmode = MODE_NOTOUCH;
                    break;

                default:
                    // Don't allow tap click when in the scroll area
                    if (!scroll || x < redge) {
                        if (wastriple && rtap) {
                            buttons |= !swapdoubletriple ? 0x4 : 0x02;
                            touchmode = MODE_NOTOUCH;
                        } else if (wasdouble && rtap) {
                            buttons |= !swapdoubletriple ? 0x2 : 0x04;
                            touchmode = MODE_NOTOUCH;
                        } else {
                            DEBUG_LOG("Detected tap click\n");
                            buttons |= 0x1;
                            touchmode = dragging ? MODE_PREDRAG : MODE_NOTOUCH;
                        }
                    }
                    break;
            }
        } else {
            if ((touchmode==MODE_DRAG || touchmode==MODE_DRAGLOCK)
                && (draglock || draglocktemp || (dragTimer && dragexitdelay)))
            {
                touchmode=MODE_DRAGNOTOUCH;
                if (!draglock && !draglocktemp)
                {
                    cancelTimer(dragTimer);
                    setTimerTimeout(dragTimer, dragexitdelay);
                }
            } else {
                touchmode = MODE_NOTOUCH;
                draglocktemp = 0;
            }
        }
        wasdouble = false;
        wastriple = false;
    }

    // cancel pre-drag mode if second tap takes too long
    if (touchmode == MODE_PREDRAG && now_ns - untouchtime >= maxdragtime) {
        DEBUG_LOG("cancel pre-drag since second tap took too long\n");
        touchmode = MODE_NOTOUCH;
    }

    // Note: This test should probably be done somewhere else, especially if to
    // implement more gestures in the future, because this information we are
    // erasing here (time of touch) might be useful for certain gestures...

    // cancel tap if touch point moves too far
    if (isTouchMode() && isFingerTouch(z)) {
        int dx = xraw > touchx ? xraw - touchx : touchx - xraw;
        int dy = yraw > touchy ? touchy - yraw : yraw - touchy;
        if (!wasdouble && !wastriple && (dx > tapthreshx || dy > tapthreshy)) {
            touchtime = 0;
        }
        else if (dx > dblthreshx || dy > dblthreshy) {
            touchtime = 0;
        }
    }

#ifdef DEBUG_VERBOSE
    int tm2 = touchmode;
#endif
    int dx = 0, dy = 0;

    DEBUG_LOG("ps2: touchmode=%d, buttons = %d\n", touchmode, buttons);
    switch (touchmode) {
        case MODE_DRAG:
        case MODE_DRAGLOCK:
            if (MODE_DRAGLOCK == touchmode || (!immediateclick || now_ns - touchtime > maxdbltaptime)) {
                buttons |= 0x1;
            }
//            calculateMovement(x, y, z, fingers, dx, dy);
//            if (abs(dx)>10 || abs(dy)>10){
//                buttons &= 0xfe; //reset mouse drag if click wasnt in aproximatly same place
//            }
//            break;
            // no fall  through
        case MODE_MOVE:
            calculateMovement(x, y, z, fingers, dx, dy);
            break;

        case MODE_MTOUCH:
            DEBUG_LOG("detected multitouch with fingers=%d\n", fingers);
            switch (fingers) {
                case 1:
                    // transition from multitouch to single touch
                    // continue moving with the primary finger
                    DEBUG_LOG("Transition from multitouch to single touch and move\n");
                    calculateMovement(x, y, z, fingers, dx, dy);
                    break;
                case 2: // two finger
                    if (last_fingers != fingers) {
                        break;
                    }
                    if (palm && z > zlimit) {
                        break;
                    }
                    if (palm_wt && now_ns - keytime < maxaftertyping) {
                        break;
                    }
                    calculateMovement(x, y, z, fingers, dx, dy);
                    // check for stopping or changing direction
					if ((( dy < 0 ) != ( dy_history.newest() < 0 ) || dy == 0) ||
					(( dx < 0 ) != ( dx_history.newest() < 0 ) || dx == 0)) {
                        // stopped or changed direction, clear history
                        dy_history.reset();
						dx_history.reset();
                        time_history.reset();
                    }
                    // put movement and time in history for later
                    dy_history.filter(dy);
					dx_history.filter( dx );
                    time_history.filter(now_ns);
                    if (0 != dy || 0 != dx) {
//						if (!hscroll) {
//							dx = 0;
//						}
                        // reverse dy to get correct movement
                        dy = yrest - dy;
                        dx = xrest - dx;
						if (abs(dx) < scrolldxthresh) {
                            xrest = dx;
                            dx = 0;
                        } else {
                            xrest = 0;
                        }
						if (abs(dy) < scrolldythresh) {
                            yrest = dy;
                            dy = 0;
                        } else {
                            yrest = 0;
                        }
						DEBUG_LOG(
								"%s::dispatchScrollWheelEventX: dv=%d, dh=%d\n",
								getName(), dy, dx );
                        dispatchScrollWheelEventX(dy, dx, 0, now_abs);
                        dx = dy = 0;
                    }
                    break;

                case 3: // three finger
                    // Calculate movement since last interrupt
                    calculateMovement(x, y, z, fingers, dx, dy);
                    // Now calculate total movement since 3 fingers down (add to total)
                    xmoved += dx;
                    ymoved += dy;
                    DEBUG_LOG("xmoved=%d, ymoved=%d, inSwipeUp=%d, inSwipeRight=%d, inSwipeLeft=%d, inSwipeDown=%d\n", xmoved, ymoved, inSwipeUp, inSwipeRight, inSwipeLeft, inSwipeDown);
                    // Reset relative movement so we don't actually report that
                    // there was regular movement
                    dx = 0;
                    dy = 0;
                    // dispatching 3 finger movement
                    if (ymoved < -swipedy && !inSwipeUp && !inSwipe4Up) {
                        inSwipeUp = 1;
                        inSwipeDown = 0;
                        ymoved = 0;
                        DEBUG_LOG("swipe up\n");
                        _device->dispatchKeyboardMessage(kPS2M_swipeUp, &now_abs);
                        break;
                    }
                    if (ymoved > swipedy && !inSwipeDown && !inSwipe4Down) {
                        inSwipeDown = 1;
                        inSwipeUp = 0;
                        ymoved = 0;
                        DEBUG_LOG("swipe down\n");
                        _device->dispatchKeyboardMessage(kPS2M_swipeDown, &now_abs);
                        break;
                    }
                    if (xmoved > swipedx && !inSwipeRight && !inSwipe4Right) {
                        inSwipeRight = 1;
                        inSwipeLeft = 0;
                        xmoved = 0;
                        DEBUG_LOG("swipe right\n");
                        _device->dispatchKeyboardMessage(kPS2M_swipeRight, &now_abs);
                        break;
                    }
                    if (xmoved < -swipedx && !inSwipeLeft && !inSwipe4Left) {
                        inSwipeLeft = 1;
                        inSwipeRight = 0;
                        xmoved = 0;
                        DEBUG_LOG("swipe left\n");
                        _device->dispatchKeyboardMessage(kPS2M_swipeLeft, &now_abs);
                        break;
                    }
                    break;
                    
                case 4: // four fingers
                    // Calculate movement since last interrupt
                    calculateMovement(x, y, z, fingers, dx, dy);
                    // Now calculate total movement since 4 fingers down (add to total)
                    xmoved += dx;
                    ymoved += dy;
                    DEBUG_LOG("xmoved=%d, ymoved=%d, inSwipeUp=%d, inSwipeRight=%d, inSwipeLeft=%d, inSwipeDown=%d\n", xmoved, ymoved, inSwipe4Up, inSwipe4Right, inSwipe4Left, inSwipe4Down);
                    // Reset relative movement so we don't actually report that
                    // there was regular movement
                    dx = 0;
                    dy = 0;
                    // dispatching 4 finger movement
                    if (ymoved < -swipedy && !inSwipe4Up) {
                        inSwipe4Up = 1; inSwipeUp = 0;
                        inSwipe4Down = 0;
                        ymoved = 0;
                        DEBUG_LOG("swipe 4 up\n");
                        _device->dispatchKeyboardMessage(kPS2M_swipe4Up, &now_abs);
                        break;
                    }
                    if (ymoved > swipedy && !inSwipe4Down) {
                        inSwipe4Down = 1; inSwipeDown = 0;
                        inSwipe4Up = 0;
                        ymoved = 0;
                        DEBUG_LOG("swipe 4 down\n");
                        _device->dispatchKeyboardMessage(kPS2M_swipe4Down, &now_abs);
                        break;
                    }
                    if (xmoved > swipedx && !inSwipe4Right) {
                        inSwipe4Right = 1; inSwipeRight = 0;
                        inSwipe4Left = 0;
                        xmoved = 0;
                        DEBUG_LOG("swipe 4 right\n");
                        _device->dispatchKeyboardMessage(kPS2M_swipe4Right, &now_abs);
                        break;
                    }
                    if (xmoved < -swipedx && !inSwipe4Left) {
                        inSwipe4Left = 1; inSwipeLeft = 0;
                        inSwipe4Right = 0;
                        xmoved = 0;
                        DEBUG_LOG("swipe 4 left\n");
                        _device->dispatchKeyboardMessage(kPS2M_swipe4Left, &now_abs);
                        break;
                    }
            }
            break;

        case MODE_VSCROLL:
            if (!vsticky && (x < redge || fingers > 1 || z > zlimit)) {
                DEBUG_LOG("Switch back to notouch. redge=%d, vsticky=%d, zlimit=%d\n", redge, vsticky, zlimit);
                touchmode = MODE_NOTOUCH;
                break;
            }
            if (palm_wt && now_ns - keytime < maxaftertyping) {
                DEBUG_LOG("Ignore vscroll after typing\n");
                break;
            }
            dy = ((lasty - y) / (vscrolldivisor / 100.0));
            dy = yrest + dy;
			if (abs(dy) < scrolldythresh) {
                yrest = dy;
                dy = 0;
            } else {
                yrest = 0;
            }
            DEBUG_LOG("VScroll: dy=%d\n", dy);
			dispatchScrollWheelEventX( dy, dx, 0, now_abs );
            dy = 0;
            break;

        case MODE_HSCROLL:
            if (!hsticky && (y < bedge || fingers > 1 || z > zlimit)) {
                DEBUG_LOG("Switch back to notouch. bedge=%d, hsticky=%d, zlimit=%d\n", bedge, hsticky, zlimit);
                touchmode = MODE_NOTOUCH;
                break;
            }
            if (palm_wt && now_ns - keytime < maxaftertyping) {
                DEBUG_LOG("ignore hscroll after typing\n");
                break;
            }
            dx = ((lastx - x) / (hscrolldivisor / 100.0));
            dx = xrest + dx;
			if (abs(dx) < scrolldxthresh) {
                xrest = dx;
                dx = 0;
            } else {
                xrest = 0;
            }
            DEBUG_LOG("HScroll: dx=%d\n", dx);
			dispatchScrollWheelEventX( dy, dx, 0, now_abs );
            dx = 0;
            break;

        case MODE_CSCROLL:
            if (palm_wt && now_ns - keytime < maxaftertyping) {
                break;
            }
            
            if (y < centery) {
                dx = x - lastx;
			} else {
                dx = lastx - x;
            }
            
            if (x < centerx) {
                dx += lasty - y;
			} else {
                dx += y - lasty;
            }
            DEBUG_LOG("CScroll: %d\n", (dx + scrollrest) / cscrolldivisor);
            dispatchScrollWheelEventX((dx + scrollrest) / cscrolldivisor, 0, 0, now_abs);
            scrollrest = (dx + scrollrest) % cscrolldivisor;
            dx = 0;
            break;

        case MODE_DRAGNOTOUCH:
            buttons |= 0x1;
            DEBUG_LOG("dragnotouch. buttons=%d\n", buttons);
            // fall through
        case MODE_PREDRAG:
            if (!immediateclick && (!palm_wt || now_ns - keytime >= maxaftertyping)) {
                buttons |= 0x1;
                DEBUG_LOG("predrag button change: %d\n", buttons);
            }
        case MODE_NOTOUCH:
            break;

        default:; // nothing
    }

    // capture time of tap, and watch for double/triple tap
    if (isFingerTouch(z)) {
        DEBUG_LOG("isFingerTouch\n");
        // taps don't count if too close to typing or if currently in momentum scroll
		if (( !palm_wt || now_ns - keytime >= maxaftertyping )
				&& (!momentumscrollcurrent_y||!momentumscrollcurrent_x)) {
            if (!isTouchMode()) {
                DEBUG_LOG("Set touchtime to now=%llu, x=%d, y=%d, fingers=%d\n", now_ns, x, y, fingers);
                touchtime = now_ns;
                touchx = x;
                touchy = y;
            }
            if (fingers == 2) {
                wasdouble = true;
            } else if (fingers == 3) {
                wastriple = true;
            }
        }
        // any touch cancels momentum scroll
		momentumscrollcurrent_x = 0;
		momentumscrollcurrent_y = 0;
    }

    // switch modes, depending on input
    if (touchmode == MODE_PREDRAG && isFingerTouch(z)) {
        DEBUG_LOG("Switch from pre-drag to drag\n");
        touchmode = MODE_DRAG;
        draglocktemp = _modifierdown & draglocktempmask;
    }
    if (touchmode == MODE_DRAGNOTOUCH && isFingerTouch(z)) {
        DEBUG_LOG("switch from dragnotouch to drag lock\n");
        if (dragTimer)
            cancelTimer(dragTimer);
        touchmode=MODE_DRAGLOCK;
    }
    ////if ((w>wlimit || w<3) && isFingerTouch(z) && scroll && (wvdivisor || (hscroll && whdivisor)))
    if (MODE_MTOUCH != touchmode && (fingers > 1) && isFingerTouch(z)) {
        DEBUG_LOG("switch to multitouch mode\n");
        touchmode = MODE_MTOUCH;
        tracksecondary = false;
    }

    if (scroll && cscrolldivisor) {
        if (touchmode == MODE_NOTOUCH && z > z_finger && y > tedge && (ctrigger == 1 || ctrigger == 9))
            touchmode = MODE_CSCROLL;
        if (touchmode == MODE_NOTOUCH && z > z_finger && y > tedge && x > redge && (ctrigger == 2))
            touchmode = MODE_CSCROLL;
        if (touchmode == MODE_NOTOUCH && z > z_finger && x > redge && (ctrigger == 3 || ctrigger == 9))
            touchmode = MODE_CSCROLL;
        if (touchmode == MODE_NOTOUCH && z > z_finger && x > redge && y < bedge && (ctrigger == 4))
            touchmode = MODE_CSCROLL;
        if (touchmode == MODE_NOTOUCH && z > z_finger && y < bedge && (ctrigger == 5 || ctrigger == 9))
            touchmode = MODE_CSCROLL;
        if (touchmode == MODE_NOTOUCH && z > z_finger && y < bedge && x < ledge && (ctrigger == 6))
            touchmode = MODE_CSCROLL;
        if (touchmode == MODE_NOTOUCH && z > z_finger && x < ledge && (ctrigger == 7 || ctrigger == 9))
            touchmode = MODE_CSCROLL;
        if (touchmode == MODE_NOTOUCH && z > z_finger && x < ledge && y > tedge && (ctrigger == 8))
            touchmode = MODE_CSCROLL;

        DEBUG_LOG("new touchmode=%d\n", touchmode);
    }
    if ((MODE_NOTOUCH == touchmode || (MODE_HSCROLL == touchmode && y >= bedge)) &&
            z > z_finger && x > redge && vscrolldivisor && vscroll) {
        DEBUG_LOG("switch to vscroll touchmode redge=%d, bedge=%d, vscrolldivisor=%d, scroll=%d\n", redge, bedge, vscrolldivisor, scroll);
        touchmode = MODE_VSCROLL;
        scrollrest = 0;
    }
    if ((MODE_NOTOUCH == touchmode || (MODE_VSCROLL == touchmode && x <= redge)) &&
            z > z_finger && y > bedge && hscrolldivisor && hscroll && vscroll) {
        DEBUG_LOG("switch to hscroll touchmode\n");
        touchmode = MODE_HSCROLL;
        scrollrest = 0;
    }
    if (touchmode == MODE_NOTOUCH && z > z_finger) {
        touchmode = MODE_MOVE;
    }

    // dispatch dx/dy and current button status
    dispatchRelativePointerEventX(dx, dy, buttons, now_abs);

    // always save last seen position for calculating deltas later
    lastx = x;
    lasty = y;
    last_fingers = fingers;

#ifdef DEBUG_VERBOSE
    DEBUG_LOG("ps2: dx=%d, dy=%d (%d,%d) z=%d mode=(%d,%d,%d) buttons=%d wasdouble=%d wastriple=%d\n", dx, dy, x, y, z, tm1, tm2, touchmode, buttons, wasdouble, wastriple);
#endif
}

void ApplePS2ALPSGlidePoint::calculateMovement(int x, int y, int z, int fingers, int &dx, int &dy) {
    if (last_fingers == fingers && (!palm || (z <= zlimit))) {
        dx = x - lastx;
        dy = y - lasty;
        DEBUG_LOG("before: dx=%d, dy=%d\n", dx, dy);
        dx = (dx / (divisorx / 100.0));
        dy = (dy / (divisory / 100.0));
        // Don't worry about rest of divisor value for now...not signigicant enough
//        xrest = dx % divisorx;
//        yrest = dy % divisory;
        // This was in the original version but not sure why...seems OK if
        // the user is moving fast, at least on the ALPS hardware here
		//        if (abs(dx) > bogusdxthresh || abs(dy) > bogusdythresh) {
		//            dx = dy = xrest = yrest = 0;
		//        }
    }
}

int ApplePS2ALPSGlidePoint::processBitmap(unsigned int xMap, unsigned int yMap, int *x1, int *y1, int *x2, int *y2) {
    struct alps_bitmap_point {
        int start_bit;
        int num_bits;
    };

    int fingers_x = 0, fingers_y = 0, fingers;
    int i, bit, prev_bit;
    struct alps_bitmap_point xLow = {0,}, x_high = {0,};
    struct alps_bitmap_point yLow = {0,}, y_high = {0,};
    struct alps_bitmap_point *point;

    if (!xMap || !yMap) {
        return 0;
    }

    *x1 = *y1 = *x2 = *y2 = 0;

    prev_bit = 0;
    point = &xLow;
    for (i = 0; xMap != 0; i++, xMap >>= 1) {
        bit = xMap & 1;
        if (bit) {
            if (!prev_bit) {
                point->start_bit = i;
                fingers_x++;
            }
            point->num_bits++;
        } else {
            if (prev_bit)
                point = &x_high;
            else
                point->num_bits = 0;
        }
        prev_bit = bit;
    }

    /*
     * y bitmap is reversed for what we need (lower positions are in
     * higher bits), so we process from the top end.
     */
    yMap = yMap << (sizeof(yMap) * BITS_PER_BYTE - modelData.y_bits);
    prev_bit = 0;
    point = &yLow;
    for (i = 0; yMap != 0; i++, yMap <<= 1) {
        bit = yMap & (1 << (sizeof(yMap) * BITS_PER_BYTE - 1));
        if (bit) {
            if (!prev_bit) {
                point->start_bit = i;
                fingers_y++;
            }
            point->num_bits++;
        } else {
            if (prev_bit) {
                point = &y_high;
            } else {
                point->num_bits = 0;
            }
        }
        prev_bit = bit;
    }

    /*
     * Fingers can overlap, so we use the maximum count of fingers
     * on either axis as the finger count.
     */
    fingers = MAX(fingers_x, fingers_y);

    /*
     * If total fingers is > 1 but either axis reports only a single
     * contact, we have overlapping or adjacent fingers. For the
     * purposes of creating a bounding box, divide the single contact
     * (roughly) equally between the two points.
     */
    if (fingers > 1) {
        if (fingers_x == 1) {
            i = xLow.num_bits / 2;
            xLow.num_bits = xLow.num_bits - i;
            x_high.start_bit = xLow.start_bit + i;
            x_high.num_bits = MAX(i, 1);
        } else if (fingers_y == 1) {
            i = yLow.num_bits / 2;
            yLow.num_bits = yLow.num_bits - i;
            y_high.start_bit = yLow.start_bit + i;
            y_high.num_bits = MAX(i, 1);
        }
    }

    *x1 = (modelData.x_max * (2 * xLow.start_bit + xLow.num_bits - 1)) /
            (2 * (modelData.x_bits - 1));
    *y1 = (modelData.y_max * (2 * yLow.start_bit + yLow.num_bits - 1)) /
            (2 * (modelData.y_bits - 1));

    if (fingers > 1) {
        *x2 = (modelData.x_max * (2 * x_high.start_bit + x_high.num_bits - 1)) /
                (2 * (modelData.x_bits - 1));
        *y2 = (modelData.y_max * (2 * y_high.start_bit + y_high.num_bits - 1)) /
                (2 * (modelData.y_bits - 1));
    }

    DEBUG_LOG("ps2: Process bitmap, fingers=%d, x1=%d, x2=%d, y1=%d, y2=%d, area = %d\n", fingers, *x1, *x2, *y1, *y2, abs(*x1 - *x2) * abs(*y1 - *y2));

    return fingers;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::
dispatchRelativePointerEventWithPacket(UInt8 *packet,
                UInt32 packetSize) {
    //
    // Process the three byte relative format packet that was retrieved from the
    // trackpad. The format of the bytes is as follows:
    //
    //  7  6  5  4  3  2  1  0
    // -----------------------
    // YO XO YS XS  1  M  R  L
    // X7 X6 X5 X4 X3 X3 X1 X0  (X delta)
    // Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0  (Y delta)
    //

    UInt32 buttons = 0;
    SInt32 dx, dy;

    if ((packet[0] & 0x1)) buttons |= 0x1;  // left button   (bit 0 in packet)
    if ((packet[0] & 0x2)) buttons |= 0x2;  // right button  (bit 1 in packet)
    if ((packet[0] & 0x4)) buttons |= 0x4;  // middle button (bit 2 in packet)

    dx = packet[1];
    if (dx) {
        dx = packet[1] - ((packet[0] << 4) & 0x100);
    }

    dy = packet[2];
    if (dy) {
        dy = ((packet[0] << 3) & 0x100) - packet[2];
    }

    uint64_t now_abs;
    clock_get_uptime(&now_abs);
    DEBUG_LOG("Dispatch relative PS2 packet: dx=%d, dy=%d, buttons=%d\n", dx, dy, buttons);
    dispatchRelativePointerEventX(dx, dy, buttons, now_abs);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void ApplePS2ALPSGlidePoint::setTouchPadV6Enable(bool enable) {
	DEBUG_LOG( "setTouchpadEnableV6 enter,%d\n", enable );
	//
	// Instructs the trackpad to start or stop the reporting of data packets.
	// It is safe to issue this request from the interrupt/completion context.
	//

	PS2Request * request = _device->allocateRequest();
	if (!request)
		return;

	// (mouse enable/disable command)
	request->commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[0].inOrOut = kDP_SetDefaultsAndDisable;
	request->commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[1].inOrOut = kDP_SetDefaultsAndDisable;
	request->commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[2].inOrOut = kDP_SetDefaultsAndDisable;
	request->commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[3].inOrOut = kDP_SetDefaultsAndDisable;

	// (mouse or pad enable/disable command)
	request->commands[4].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[4].inOrOut =
			( enable ) ? kDP_Enable : kDP_SetDefaultsAndDisable;
	request->commandsCount = 5;
	_device->submitRequest( request ); // asynchronous, auto-free'd
}

void ApplePS2ALPSGlidePoint::setTouchPadEnable(bool enable) {
    DEBUG_LOG("setTouchpadEnable enter\n");

	if (modelData.proto_version==ALPS_PROTO_V6){
			setTouchPadV6Enable( enable );
			return;
	}
    //
    // Instructs the trackpad to start or stop the reporting of data packets.
    // It is safe to issue this request from the interrupt/completion context.
    //

    if (enable) {
        initTouchPad();
    } else {
        // to disable just reset the mouse
        resetMouse();
    }
}

bool ApplePS2ALPSGlidePoint::getStatus(ALPSStatus_t *status) {
    return repeatCmd(NULL, NULL, kDP_SetDefaultsAndDisable, status);
}

/*
 * Turn touchpad tapping on or off. The sequences are:
 * 0xE9 0xF5 0xF5 0xF3 0x0A to enable,
 * 0xE9 0xF5 0xF5 0xE8 0x00 to disable.
 * My guess that 0xE9 (GetInfo) is here as a sync point.
 * For models that also have stickpointer (DualPoints) its tapping
 * is controlled separately (0xE6 0xE6 0xE6 0xF3 0x14|0x0A) but
 * we don't fiddle with it.
 */
bool ApplePS2ALPSGlidePoint::tapMode(bool enable) {
	//
	// Instructs the trackpad to honor or ignore tapping
	//
	ALPSStatus_t Status;
	bool success;
    
	PS2Request * request = _device->allocateRequest();
	if (!request)
        return false;

	request->commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[0].inOrOut = kDP_SetMouseScaling2To1;
	request->commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[1].inOrOut = kDP_SetMouseScaling2To1;
	request->commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[2].inOrOut = kDP_SetMouseScaling2To1;
	request->commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[3].inOrOut = kDP_SetDefaultsAndDisable;
	request->commands[4].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[4].inOrOut = kDP_GetMouseInformation;
	request->commands[5].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[5].inOrOut = kDP_SetDefaultsAndDisable;
	request->commands[6].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[6].inOrOut = kDP_SetDefaultsAndDisable;

	getStatus( &Status );
	if (Status.bytes[0] & 0x04) {
		DEBUG_LOG( "Tapping can only be toggled.\n" );
		enable = false;
    }
    
	if (enable) {
		request->commands[7].command = kPS2C_SendMouseCommandAndCompareAck;
		request->commands[7].inOrOut = kDP_SetMouseSampleRate;
		request->commands[8].command = kPS2C_SendMouseCommandAndCompareAck;
		request->commands[8].inOrOut = 0x0A;
	} else {
		request->commands[7].command = kPS2C_SendMouseCommandAndCompareAck;
		request->commands[7].inOrOut = kDP_SetMouseResolution;
		request->commands[8].command = kPS2C_SendMouseCommandAndCompareAck;
		request->commands[8].inOrOut = 0x00;
}

	request->commands[9].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[9].inOrOut = kDP_SetMouseScaling1To1;
	request->commands[10].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[10].inOrOut = kDP_SetMouseScaling1To1;
	request->commands[11].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[11].inOrOut = kDP_SetMouseScaling1To1;
	request->commands[12].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[12].inOrOut = kDP_SetDefaultsAndDisable;
	request->commands[13].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[13].inOrOut = kDP_Enable;
	request->commandsCount = 14;
	_device->submitRequestAndBlock( request );

	getStatus( &Status );

	success = ( request->commandsCount == 14 );
	if (success) {
		setSampleRateAndResolution( 0x64, 0x03 );
	}

	_device->freeRequest( request );
	return true;
}

bool ApplePS2ALPSGlidePoint::enterCommandMode() {
    DEBUG_LOG("enter command mode\n");
    TPS2Request<8> request;
    ALPSStatus_t status;
    
    repeatCmd(NULL, NULL, kDP_MouseResetWrap, &status);

    IOLog("ApplePS2ALPSGlidePoint EC Report: { 0x%02x, 0x%02x, 0x%02x }\n", status.bytes[0], status.bytes[1], status.bytes[2]);

    if ((status.bytes[0] != 0x88 || (status.bytes[1] != 0x07 && status.bytes[1] != 0x08)) && status.bytes[0] != 0x73) {
        DEBUG_LOG("ApplePS2ALPSGlidePoint: Failed to enter command mode!\n");
        return false;
    }

    return true;
}

bool ApplePS2ALPSGlidePoint::exitCommandMode() {
    DEBUG_LOG("exit command mode\n");
    TPS2Request<1> request;

    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_SetMouseStreamMode;
    request.commandsCount = 1;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);

    return true;
}

bool ApplePS2ALPSGlidePoint::hwInitV3() {
    int regVal;

    regVal = probeTrackstickV3(ALPS_REG_BASE_PINNACLE);
    if (regVal == kIOReturnIOError) {
        goto error;
    }
    
    if (regVal == 0 && setupTrackstickV3(ALPS_REG_BASE_PINNACLE) == kIOReturnIOError) {
        IOLog("Failed to setup trackstick\n");
        goto error;
    }
    
    if (!(enterCommandMode() &&
          absoluteModeV3())) {
        IOLog("ERROR: Failed to enter absolute mode\n");
        goto error;
    }

    DEBUG_LOG("now setting a bunch of regs\n");
    regVal = commandModeReadReg(0x0006);
    if (regVal == -1) {
        DEBUG_LOG("Failed to read reg 0x0006\n");
        goto error;
    }
    if (!commandModeWriteReg(regVal | 0x01)) {
        goto error;
    }

    regVal = commandModeReadReg(0x0007);
    if (regVal == -1) {
        DEBUG_LOG("Failed to read reg 0x0007\n");
    }
    if (!commandModeWriteReg(regVal | 0x01)) {
        goto error;
    }

    if (commandModeReadReg(0x0144) == -1) {
        goto error;
    }
    if (!commandModeWriteReg(0x04)) {
        goto error;
    }

    if (commandModeReadReg(0x0159) == -1) {
        goto error;
    }
    if (!commandModeWriteReg(0x03)) {
        goto error;
    }

    if (commandModeReadReg(0x0163) == -1) {
        goto error;
    }
    if (!commandModeWriteReg(0x0163, 0x03)) {
        goto error;
    }

    if (commandModeReadReg(0x0162) == -1) {
        goto error;
    }
    if (!commandModeWriteReg(0x0162, 0x04)) {
        goto error;
    }

    exitCommandMode();

    /* Set rate and enable data reporting */
    DEBUG_LOG("set sample rate\n");
    if (!setSampleRateAndResolution(0x64, 0x02)) {
        return false;
    }

    return true;

error:
    exitCommandMode();
    return false;
}

bool ApplePS2ALPSGlidePoint::hwInitRushmoreV3() {
    
    
    int regVal;
    TPS2Request<1> request;
    
    if (modelData.flags & ALPS_DUALPOINT) {
        regVal = setupTrackstickV3(ALPS_REG_BASE_RUSHMORE);
        if (regVal == kIOReturnIOError) {
            goto error;
        }
        if (regVal == kIOReturnNoDevice) {
            modelData.flags &= ~ALPS_DUALPOINT;
        }
    }
    
    if (!enterCommandMode() ||
        commandModeReadReg(0xc2d9) == -1 ||
        !commandModeWriteReg(0xc2cb, 0x00)) {

        goto error;
    }
    
    regVal = commandModeReadReg(0xc2c6);
    if (regVal == -1) {
        goto error;
    }
    
    if (!commandModeWriteReg(regVal & 0xfd)) {
        goto error;
    }
    
    if (!commandModeWriteReg(0xc2c9, 0x64)) {
        goto error;
    }
    
    /* enter absolute mode */
    regVal = commandModeReadReg(0xc2c4);
    if (regVal == -1) {
        goto error;
    }
    if (!commandModeWriteReg(regVal | 0x02)) {
        goto error;
    }
    
    exitCommandMode();
    
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_Enable;
    request.commandsCount = 1;
    _device->submitRequestAndBlock(&request);

    return request.commandsCount == 1;

error:
    exitCommandMode();
    return false;
}

bool ApplePS2ALPSGlidePoint::setSampleRateAndResolution(UInt8 rate, UInt8 res) {
    TPS2Request<6> request;
    UInt8 commandNum = 0;

    DEBUG_LOG("setSampleRateAndResolution %d %d\n", (int) rate, (int) res);
    // NOTE: Don't do this otherwise the touchpad stops reporting data and
    // may or may not be related but the keyboard was screwed up too...
    //request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    //request.commands[0].inOrOut = kDP_SetDefaultsAndDisable;            // 0xF5, Disable data reporting
    request.commands[commandNum].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[commandNum++].inOrOut = kDP_SetMouseSampleRate;                // 0xF3
    request.commands[commandNum].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[commandNum++].inOrOut = rate;                                // 100
    request.commands[commandNum].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[commandNum++].inOrOut = kDP_SetMouseResolution;                // 0xE8
    request.commands[commandNum].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[commandNum++].inOrOut = res;                                // 0x02 = 4 counts per mm
    request.commands[commandNum].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[commandNum++].inOrOut = kDP_Enable;                            // 0xF4, Enable Data Reporting
    request.commandsCount = commandNum;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
    
    return request.commandsCount == commandNum;
}


int ApplePS2ALPSGlidePoint::commandModeReadReg(int addr) {
    TPS2Request<4> request;
    ALPSStatus_t status;

    if (!commandModeSetAddr(addr)) {
        DEBUG_LOG("Failed to set addr to read register\n");
        return -1;
    }

    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_GetMouseInformation; //sync..
    request.commands[1].command = kPS2C_ReadDataPort;
    request.commands[1].inOrOut = 0;
    request.commands[2].command = kPS2C_ReadDataPort;
    request.commands[2].inOrOut = 0;
    request.commands[3].command = kPS2C_ReadDataPort;
    request.commands[3].inOrOut = 0;
    request.commandsCount = 4;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
    
    if (request.commandsCount != 4) {
        return -1;
    }

    status.bytes[0] = request.commands[1].inOrOut;
    status.bytes[1] = request.commands[2].inOrOut;
    status.bytes[2] = request.commands[3].inOrOut;

    DEBUG_LOG("ApplePS2ALPSGlidePoint read reg result: { 0x%02x, 0x%02x, 0x%02x }\n", status.bytes[0], status.bytes[1], status.bytes[2]);

    /* The address being read is returned in the first 2 bytes
     * of the result. Check that the address matches the expected
     * address.
     */
    if (addr != ((status.bytes[0] << 8) | status.bytes[1])) {
        DEBUG_LOG("ApplePS2ALPSGlidePoint ERROR: read wrong registry value, expected: %x\n", addr);
        return -1;
    }

    return status.bytes[2];
}

bool ApplePS2ALPSGlidePoint::commandModeWriteReg(int addr, UInt8 value) {

    if (!commandModeSetAddr(addr)) {
        return false;
    }

    return commandModeWriteReg(value);
}

bool ApplePS2ALPSGlidePoint::commandModeWriteReg(UInt8 value) {
    if (!commandModeSendNibble((value >> 4) & 0xf)) {
        return false;
    }
    if (!commandModeSendNibble(value & 0xf)) {
        return false;
    }

    return true;
}

bool ApplePS2ALPSGlidePoint::commandModeSendNibble(int nibble) {
    SInt32 command;
    // The largest amount of requests we will have is 2 right now
    // 1 for the initial command, and 1 for sending data OR 1 for receiving data
    // If the nibble commands at the top change then this will need to change as
    // well. For now we will just validate that the request will not overload
    // this object.
    TPS2Request<2> request;
    int cmdCount = 0, send = 0, receive = 0, i;

    if (nibble > 0xf) {
        IOLog("%s::commandModeSendNibble ERROR: nibble value is greater than 0xf, command may fail\n", getName());
    }

    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    command = modelData.nibble_commands[nibble].command;
    request.commands[cmdCount++].inOrOut = command & 0xff;

    send = (command >> 12 & 0xf);
    receive = (command >> 8 & 0xf);

    // Validate that the number of requests will not exceed our buffer as
    // defined above
    // Also, send can never be > 1 since all we have available is the data
    // from the alps_nibble_commands which is 1 byte
    if ((send > 1) || ((send + receive + 1) > 2)) {
        IOLog("%s::commandModeSendNibble: ERROR: Nibble commands have changed. Cannot process nibble that sends or receives more than 1 byte of data.\n", getName());
        return false;
    }

    //DEBUG_LOG("%s: send nibble: nibble=%x command info=%x command=0x%02x send=%d, receive=%d, data=0x%02x\n",
    //          getName(), nibble, command, request.commands[0].inOrOut, send, receive, modelData.nibble_commands[nibble].data);

    if (send > 0) {
        request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
        request.commands[cmdCount++].inOrOut = modelData.nibble_commands[nibble].data;
    }

    // Receive the amount of data for the given command
    // Even though we don't read the data, we should drain the data port to follow protocol
    for (i = 0; i < receive; i++) {
        request.commands[cmdCount].command = kPS2C_ReadDataPort;
        request.commands[cmdCount++].inOrOut = 0;
    }

    request.commandsCount = cmdCount;
    assert(request.commandsCount <= countof(request.commands));

    _device->submitRequestAndBlock(&request);
    
    //DEBUG_LOG("%s: num nibble commands=%d, expected=%d\n", getName(), request.commandsCount, cmdCount);

    return request.commandsCount == cmdCount;
}

bool ApplePS2ALPSGlidePoint::commandModeSetAddr(int addr) {

    TPS2Request<1> request;
    int i, nibble;

//    DEBUG_LOG("command mode set addr with addr command: 0x%02x\n", modelData.addr_command);
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = modelData.addr_command;
    request.commandsCount = 1;
    _device->submitRequestAndBlock(&request);
    
    if (request.commandsCount != 1) {
        return false;
    }

    for (i = 12; i >= 0; i -= 4) {
        nibble = (addr >> i) & 0xf;
        if (!commandModeSendNibble(nibble)) {
            return false;
        }
    }
    
    return true;
}

bool ApplePS2ALPSGlidePoint::passthroughModeV3(int regBase, bool enable) {
    int regVal;
    bool ret = false;
    
    DEBUG_LOG("passthrough mode enable=%d\n", enable);
    
    if (!enterCommandMode()) {
        IOLog("ERROR: Failed to enter command mode while enabling passthrough mode\n");
        return false;
    }

    regVal = commandModeReadReg(regBase + 0x0008);
    if (regVal == -1) {
        IOLog("Failed to read register while setting up passthrough mode\n");
        goto error;
    }

    if (enable) {
        regVal |= 0x01;
    } else {
        regVal &= ~0x01;
    }

    ret = commandModeWriteReg(regVal);
    
error:
    if (!exitCommandMode()) {
        IOLog("ERROR: failed to exit command mode while enabling passthrough mode v3\n");
        return false;
    }

    return ret;
};

bool ApplePS2ALPSGlidePoint::passthroughModeV2(bool enable) {
    int cmd = enable ? kDP_SetMouseScaling2To1 : kDP_SetMouseScaling1To1;
    TPS2Request<4> request;
    
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = cmd;
    request.commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[1].inOrOut = cmd;
    request.commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[2].inOrOut = cmd;
    request.commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[3].inOrOut = kDP_SetDefaultsAndDisable;
    request.commandsCount = 4;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
    
    // TODO: drain a possible 3 extra bytes from the command port
    /* we may get 3 more bytes, just ignore them */
    //ps2_drain(ps2dev, 3, 100);
    
    return request.commandsCount == 4;
}

bool ApplePS2ALPSGlidePoint::absoluteModeV3() {

    int regVal;

    regVal = commandModeReadReg(0x0004);
    if (regVal == -1) {
        return false;
    }

    regVal |= 0x06;
    if (!commandModeWriteReg(regVal)) {
        return false;
    }

    return true;
}

IOReturn ApplePS2ALPSGlidePoint::probeTrackstickV3(int regBase) {
    int ret = kIOReturnIOError;
    int regVal;
    
    if (!enterCommandMode()) {
        goto error;
    }

    regVal = commandModeReadReg(regBase + 0x08);

    if (regVal == -1) {
        // On linux this is reported as an IO error
        // however, here it can also mean that the device
        // doesn't exist. So I lean on the side that it
        // doesn't exist. If there was an IO error here
        // it doesn't matter too much anyway, the trackstick
        // just won't work or there will be another IO error
        // later on that will break out of the init as well
        ret = kIOReturnNoDevice;
        goto error;
    }

    /* bit 7: trackstick is present */
    ret = regVal & 0x80 ? 0 : kIOReturnNoDevice;
    
error:
    exitCommandMode();
    return ret;
}

IOReturn ApplePS2ALPSGlidePoint::setupTrackstickV3(int regBase) {
    IOReturn ret = 0;
    ALPSStatus_t report;
    TPS2Request<3> request;
    
    if (!passthroughModeV3(regBase, true)) {
        return kIOReturnIOError;
    }
    
    if (!repeatCmd(NULL, NULL, kDP_SetMouseScaling2To1, &report)) {
        IOLog("WARN: trackstick E7 report failed\n");
        ret = kIOReturnNoDevice;
    } else {
        /*
         * Not sure what this does, but it is absolutely
         * essential. Without it, the touchpad does not
         * work at all and the trackstick just emits normal
         * PS/2 packets.
         */
        request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
        request.commands[0].inOrOut = kDP_SetMouseScaling1To1;
        request.commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
        request.commands[1].inOrOut = kDP_SetMouseScaling1To1;
        request.commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
        request.commands[2].inOrOut = kDP_SetMouseScaling1To1;
        request.commandsCount = 3;
        assert(request.commandsCount <= countof(request.commands));
        _device->submitRequestAndBlock(&request);
        if (!request.commandsCount == 3) {
            IOLog("ERROR: error sending magic E6 scaling sequence\n");
            ret = kIOReturnIOError;
            goto error;
        }
        if (!(commandModeSendNibble(0x9) && commandModeSendNibble(0x4))) {
            IOLog("ERROR: error sending magic E6 nibble sequence\n");
            ret = kIOReturnIOError;
            goto error;
        }
        DEBUG_LOG("Sent magic E6 sequence\n");
        
        /* Ensures trackstick packets are in the correct format */
        if (!(enterCommandMode() &&
              commandModeWriteReg(regBase + 0x0008, 0x82) &&
              exitCommandMode())) {
            ret = kIOReturnIOError;
            goto error;
        }
    }
error:
    if (!passthroughModeV3(regBase, false)) {
        ret = kIOReturnIOError;
    }
    
    return ret;
}

/*
 * Used during both passthrough mode initialization and touchpad enablement
 */
bool ApplePS2ALPSGlidePoint::v1v2MagicEnable() {
    TPS2Request<5> request;
    
    /* Try ALPS magic knock - 4 disable before enable */
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[1].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[2].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[3].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[4].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[4].inOrOut = kDP_Enable;
    request.commandsCount = 5;
    
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
    
    return request.commandsCount == 5;
}

bool ApplePS2ALPSGlidePoint::absoluteModeV1V2() {
    
    TPS2Request<1> request;
    
    if (!v1v2MagicEnable()) {
        IOLog("Failed to enter absolute mode with magic knock\n");
        return false;
    }
    
    /*
     * Switch mouse to poll (remote) mode so motion data will not
     * get in our way
     */
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_MouseSetPoll;
    request.commandsCount = 1;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);

    return request.commandsCount == 1;
}

bool ApplePS2ALPSGlidePoint::repeatCmd(SInt32 init_command, SInt32 init_arg, SInt32 repeated_command, ALPSStatus_t *report) {
    TPS2Request<9> request;
    int byte0, cmd;
    cmd = byte0 = 0;
    
    if (init_command) {
        request.commands[cmd].command = kPS2C_SendMouseCommandAndCompareAck;
        request.commands[cmd++].inOrOut = kDP_SetMouseResolution;
        request.commands[cmd].command = kPS2C_SendMouseCommandAndCompareAck;
        request.commands[cmd++].inOrOut = init_arg;
    }
    
    
    // 3X run command
    request.commands[cmd].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmd++].inOrOut = repeated_command;
    request.commands[cmd].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmd++].inOrOut = repeated_command;
    request.commands[cmd].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmd++].inOrOut = repeated_command;
    
    // Get info/result
    request.commands[cmd].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmd++].inOrOut = kDP_GetMouseInformation;
    byte0 = cmd;
    request.commands[cmd].command = kPS2C_ReadDataPort;
    request.commands[cmd++].inOrOut = 0;
    request.commands[cmd].command = kPS2C_ReadDataPort;
    request.commands[cmd++].inOrOut = 0;
    request.commands[cmd].command = kPS2C_ReadDataPort;
    request.commands[cmd++].inOrOut = 0;
    request.commandsCount = cmd;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
    
    report->bytes[0] = request.commands[byte0].inOrOut;
    report->bytes[1] = request.commands[byte0+1].inOrOut;
    report->bytes[2] = request.commands[byte0+2].inOrOut;
    
    DEBUG_LOG("%02x report: [0x%02x 0x%02x 0x%02x]\n",
              repeated_command,
              report->bytes[0],
              report->bytes[1],
              report->bytes[2]);
    
    return request.commandsCount == cmd;
}

bool ApplePS2ALPSGlidePoint::hwInitV1V2() {
    TPS2Request<1> request;
    
    if (modelData.flags & ALPS_PASS) {
        if (!passthroughModeV2(true)) {
            IOLog("ERROR: Failed to enter passthrough mode\n");
            return false;
        }
    }
    
    if (!tapMode(true)) {
        IOLog("ERROR: Failed to enable hardware tapping\n");
        return false;
    }
    
    if (!absoluteModeV1V2()) {
        IOLog("ERROR: Failed to enable absolute mode\n");
        return false;
    }
    
    if (modelData.flags & ALPS_PASS) {
        if (!passthroughModeV2(false)) {
            IOLog("ERROR: Failed to exit passthrough mode\n");
            return false;
        }
    }
    
    // Enable data reporting
    v1v2MagicEnable();
    
    // Not sure if this is needed or not...previous version did not have
    // it but the linux driver does, need to figure out why its not reporting
    // data right now
    /* ALPS needs stream mode, otherwise it won't report any data */
    /* request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_SetMouseStreamMode;
    request.commandsCount = 1;
    _device->submitRequestAndBlock(&request);
    
    if (request.commandsCount != 1) {
        IOLog("ERROR: Failed to set stream mode on touchpad\n");
        return false;
    }*/
    
    return true;
}

/* Must be in command mode when calling this function */
bool ApplePS2ALPSGlidePoint::absoluteModeV4() {
    int regVal;
    
    regVal = commandModeReadReg(0x0004);
    if (regVal == -1) {
        return false;
    }
    
    regVal |= 0x02;
    if (!commandModeWriteReg(regVal)) {
        return false;
    }
    
    return true;
}

bool ApplePS2ALPSGlidePoint::hwInitV4() {    
    TPS2Request<7> request;
    
    if (!enterCommandMode()) {
        goto error;
    }
    
    if (!absoluteModeV4()) {
        IOLog("ERROR: Failed to enter absolute mode\n");
        goto error;
    }
    
    DEBUG_LOG("now setting a bunch of regs\n");
    
    if (!commandModeWriteReg(0x0007, 0x8c)) {
        goto error;
    }
    
    if (!commandModeWriteReg(0x0149, 0x03)) {
        goto error;
    }
    
    if (!commandModeWriteReg(0x0160, 0x03)) {
        goto error;
    }
    
    if (!commandModeWriteReg(0x017f, 0x15)) {
        goto error;
    }
    
    if (!commandModeWriteReg(0x0151, 0x01)) {
        goto error;
    }
    
    if (!commandModeWriteReg(0x0168, 0x03)) {
        goto error;
    }
    
    if (!commandModeWriteReg(0x014a, 0x03)) {
        goto error;
    }
    
    if (!commandModeWriteReg(0x0161, 0x03)) {
        goto error;
    }
    
    exitCommandMode();
    
    /*
     * This sequence changes the output from a 9-byte to an
     * 8-byte format. All the same data seems to be present,
     * just in a more compact format.
     */
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_SetMouseSampleRate;
    request.commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[1].inOrOut = 0xc8;
    request.commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[2].inOrOut = kDP_SetMouseSampleRate;
    request.commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[3].inOrOut = 0x64;
    request.commands[4].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[4].inOrOut = kDP_SetMouseSampleRate;
    request.commands[5].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[5].inOrOut = 0x50;
    request.commands[6].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[6].inOrOut = kDP_GetId;
    request.commandsCount = 7;
    _device->submitRequestAndBlock(&request);
    
    if (request.commandsCount != 7) {
        return false;
    }
    
    /* Set rate and enable data reporting */
    DEBUG_LOG("set sample rate\n");
    if (!setSampleRateAndResolution(0x64, 0x02)) {
        return false;
    }

    return true;
    
error:
    exitCommandMode();
    return false;
}

bool ApplePS2ALPSGlidePoint::hwInitDolphinV1() {
    TPS2Request<16> request;
    int cmdCount = 0;
    /*
     		int array[]={0xE8, 0x00, 0xE7, 0xE7,  
                         0xE7, 0xE9, 0xEC, 0xEC, 
                         0xEC, 0xE9, 0xEA, 0xEA, 
                         0xF3, 0x64, 0xF3, 0x28};
     */
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xE8;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0x00;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xE7;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xE7;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xE7;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xE9;
    
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xEC;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xEC;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xEC;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xE9;
    
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0xEA;
    
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = kDP_SetMouseStreamMode;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = kDP_SetMouseSampleRate;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0x64;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = kDP_SetMouseSampleRate;
    request.commands[cmdCount].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[cmdCount++].inOrOut = 0x28;
    request.commandsCount = cmdCount;
    
    _device->submitRequestAndBlock(&request);
    
    if (request.commandsCount != cmdCount) {
        return false;
    }
    
    return true;
}

bool ApplePS2ALPSGlidePoint::setAbsoluteModeNew() {
	PS2Request * request = _device->allocateRequest();
	if (!request)
		return false;

	// Try ALPS magic knock - 4 disable before enable
	request->commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[0].inOrOut = kDP_SetDefaultsAndDisable;           //F5
	request->commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[1].inOrOut = kDP_SetDefaultsAndDisable;           //F5
	request->commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[2].inOrOut = kDP_SetDefaultsAndDisable;           //F5
	request->commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[3].inOrOut = kDP_SetDefaultsAndDisable;           //F5
	request->commands[4].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[4].inOrOut = kDP_Enable;                          //F4

	// Switch mouse to poll (remote) mode so motion data will not get in our way

	request->commands[5].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[5].inOrOut = kDP_SetMousePoll;                    //F0
	request->commandsCount = 6;
	_device->submitRequestAndBlock( request );
	_device->freeRequest( request );
	return true;
}

bool ApplePS2ALPSGlidePoint::absoluteModeV6() {
	PS2Request * request = _device->allocateRequest();
	if (!request)
		return false;
	//           |---------------E7---------------|  |---------EC---------|  |EC|  |---------set abs----------|
	int array[] = { 0xE8, 0x00, 0xE7, 0xE7, 0xE7, 0xE9, 0xEC, 0xEC, 0xEC, 0xE9,
			0xEA, 0xEA, 0xF3, 0x64, 0xF3, 0x28 };
	request->commandsCount = 16;
	for (int i = 0; i < request->commandsCount; i++) {
		request->commands[i].command = kPS2C_SendMouseCommandAndCompareAck;
		request->commands[i].inOrOut = array[i];
	}
	_device->submitRequestAndBlock( request );
	_device->freeRequest( request );
	return true;
}
ALPSStatus_t ApplePS2ALPSGlidePoint::getE6Report() {
	ALPSStatus_t e6;
	repeatCmd( NULL, NULL, kDP_SetMouseScaling1To1, &e6 );
	return e6;
}

ALPSStatus_t ApplePS2ALPSGlidePoint::getE7Report() {
	ALPSStatus_t e7;
	repeatCmd( NULL, NULL, kDP_SetMouseScaling2To1, &e7 );
	return e7;
}
ALPSStatus_t ApplePS2ALPSGlidePoint::getECReport() {
	ALPSStatus_t ec;
	repeatCmd( NULL, NULL, kDP_MouseResetWrap, &ec );
	return ec;
}
bool ApplePS2ALPSGlidePoint::hwInitV6_version2() {
	//0xc8,0x14;
	ALPSStatus_t result;

	repeatCmd( NULL, NULL, kDP_SetMouseScaling1To1, &result );
	repeatCmd( NULL, NULL, kDP_SetMouseScaling1To1, &result );
	repeatCmd( NULL, NULL, kDP_SetMouseScaling1To1, &result );
	repeatCmd( kDP_SetMouseSampleRate, 0xc8, kDP_SetMouseSampleRate, &result );
	repeatCmd( kDP_SetMouseSampleRate, 0x14, kDP_SetMouseSampleRate, &result );

	absoluteModeV6();

	return true;
}

void ApplePS2ALPSGlidePoint::setDefaults() {
    modelData.byte0 = 0x8f;
    modelData.mask0 = 0x8f;
    modelData.flags = ALPS_DUALPOINT;
    
    modelData.x_max = 2000;
    modelData.y_max = 1400;
    modelData.x_bits = 15;
    modelData.y_bits = 11;
    
    switch (modelData.proto_version) {
        case ALPS_PROTO_V1:
        case ALPS_PROTO_V2:
            hw_init = &ApplePS2ALPSGlidePoint::hwInitV1V2;
            process_packet = &ApplePS2ALPSGlidePoint::processPacketV1V2;
            // On linux it appears to use x/y maxes as defined above
            // however in some preliminary testing with this driver it
            // appears the maxes are actually closer to these values:
            modelData.x_max = 1100;
            modelData.y_max = 800;
//            set_abs_params = alps_set_abs_params_st;
            break;
        case ALPS_PROTO_V3:
            hw_init = &ApplePS2ALPSGlidePoint::hwInitV3;
            process_packet = &ApplePS2ALPSGlidePoint::processPacketV3;
//            set_abs_params = alps_set_abs_params_mt;
            decode_fields = &ApplePS2ALPSGlidePoint::decodePinnacle;
            modelData.nibble_commands = alps_v3_nibble_commands;
            modelData.addr_command = kDP_MouseResetWrap;
            break;
        case ALPS_PROTO_V4:
            hw_init = &ApplePS2ALPSGlidePoint::hwInitV4;
            process_packet = &ApplePS2ALPSGlidePoint::processPacketV4;
//            set_abs_params = alps_set_abs_params_mt;
            modelData.nibble_commands = alps_v4_nibble_commands;
            modelData.addr_command = kDP_SetDefaultsAndDisable;
            break;
        case ALPS_PROTO_V5:
            hw_init = &ApplePS2ALPSGlidePoint::hwInitDolphinV1;
            process_packet = &ApplePS2ALPSGlidePoint::processPacketV3;
            decode_fields = &ApplePS2ALPSGlidePoint::decodeDolphin;
//            set_abs_params = alps_set_abs_params_mt;
            modelData.nibble_commands = alps_v3_nibble_commands;
            modelData.addr_command = kDP_MouseResetWrap;
            modelData.byte0 = 0xc8;
            modelData.mask0 = 0xc8;
            modelData.flags = 0;
            modelData.x_max = 1360;
            modelData.y_max = 660;
            modelData.x_bits = 23;
            modelData.y_bits = 12;
            break;
		case ALPS_PROTO_V6:
			hw_init = &ApplePS2ALPSGlidePoint::hwInitV6_version2;
			process_packet = &ApplePS2ALPSGlidePoint::processPacketV6;
			modelData.nibble_commands = alps_v6_nibble_commands;
			modelData.addr_command = kDP_MouseResetWrap;
			modelData.byte0 = 0xc8;
			modelData.mask0 = 0xc8;
			modelData.flags = 0;
			modelData.x_max = 1400;
			modelData.y_max = 700;
			modelData.x_bits = 23;
			modelData.y_bits = 12;
			//decode_fields = &ApplePS2ALPSGlidePoint::decodePacketV6;
			break;
    }

    setupMaxes();
}

bool ApplePS2ALPSGlidePoint::matchTable(ALPSStatus_t *e7, ALPSStatus_t *ec) {
    const struct alps_model_info *model;
    int i;
    
    for (i = 0; i < ARRAY_SIZE(alps_model_data); i++) {
        model = &alps_model_data[i];
        
        if (!memcmp(e7->bytes, model->signature, sizeof(model->signature)) &&
            (!model->command_mode_resp ||
             model->command_mode_resp == ec->bytes[2])) {
                
                modelData.proto_version = model->proto_version;
                setDefaults();
                
                modelData.flags = model->flags;
                modelData.byte0 = model->byte0;
                modelData.mask0 = model->mask0;
                
                return true;
            }
    }
    
    return false;
}

IOReturn ApplePS2ALPSGlidePoint::identify() {
    ALPSStatus_t e6, e7, ec;
    
    /*
     * First try "E6 report".
     * ALPS should return 0,0,10 or 0,0,100 if no buttons are pressed.
     * The bits 0-2 of the first byte will be 1s if some buttons are
     * pressed.
     */
    if (!repeatCmd(kDP_SetMouseResolution, NULL, kDP_SetMouseScaling1To1, &e6)) {
        IOLog("%s::identify: not an ALPS device. Error getting E6 report\n", getName());
        return kIOReturnIOError;
    }
    
    if ((e6.bytes[0] & 0xf8) != 0 || e6.bytes[1] != 0 || (e6.bytes[2] != 10 && e6.bytes[2] != 100)) {
        IOLog("%s::identify: not an ALPS device. Invalid E6 report\n", getName());
        return kIOReturnInvalid;
    }
    
    /*
     * Now get the "E7" and "EC" reports.  These will uniquely identify
     * most ALPS touchpads.
     */
    if (!(repeatCmd(kDP_SetMouseResolution, NULL, kDP_SetMouseScaling2To1, &e7) &&
          repeatCmd(kDP_SetMouseResolution, NULL, kDP_MouseResetWrap, &ec) &&
          exitCommandMode())) {
        IOLog("%s::identify: not an ALPS device. Error getting E7/EC report\n", getName());
        return kIOReturnIOError;
    }
    
    IOLog("%s::identify: Found ALPS Device with ID E7=0x%02x 0x%02x 0x%02x, EC=0x%02x 0x%02x 0x%02x\n",
          getName(), e7.bytes[0], e7.bytes[1], e7.bytes[2], ec.bytes[0], ec.bytes[1], ec.bytes[2]);
    
    if (matchTable(&e7, &ec)) {
        // found a perfect match
        return 0;
    } else if (e7.bytes[0] == 0x73 && e7.bytes[1] == 0x03 && e7.bytes[2] == 0x50 &&
               ec.bytes[0] == 0x73 && ec.bytes[1] == 0x01) {
        modelData.proto_version = ALPS_PROTO_V5;
        setDefaults();
        
        return 0;
    } else if (ec.bytes[0] == 0x88 && ec.bytes[1] == 0x08) {
        modelData.proto_version = ALPS_PROTO_V3;
        setDefaults();
        
        hw_init = &ApplePS2ALPSGlidePoint::hwInitRushmoreV3;
        decode_fields = &ApplePS2ALPSGlidePoint::decodeRushmore;
        modelData.x_bits = 16;
        modelData.y_bits = 12;
        
        if (probeTrackstickV3(ALPS_REG_BASE_RUSHMORE)) {
            modelData.flags &= ~ALPS_DUALPOINT;
        }
        
        return 0;
    } else if (ec.bytes[0] == 0x88 && ec.bytes[1] == 0x07 &&
               ec.bytes[2] >= 0x90 && ec.bytes[2] <= 0x9d) {
        modelData.proto_version = ALPS_PROTO_V3;
        setDefaults();

        return 0;
    }
    
    IOLog("Unknown ALPS touchpad, does not match any known identifiers: E7=0x%02x 0x%02x 0x%02x, EC=0x%02x 0x%02x 0x%02x\n",
          e7.bytes[0], e7.bytes[1], e7.bytes[2], ec.bytes[0], ec.bytes[1], ec.bytes[2]);
    
    return kIOReturnInvalid;
}
