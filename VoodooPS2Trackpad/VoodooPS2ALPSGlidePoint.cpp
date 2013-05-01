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

#include <IOKit/IOLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <string.h>
#include "VoodooPS2Controller.h"
#include "VoodooPS2ALPSGlidePoint.h"

enum {
    kTapEnabled = 0x01
};

#define ARRAY_SIZE(x)    (sizeof(x)/sizeof(x[0]))
#define MAX(X,Y)         ((X) > (Y) ? (X) : (Y))

// =============================================================================
// ApplePS2ALPSGlidePoint Class Implementation
//

OSDefineMetaClassAndStructors(ApplePS2ALPSGlidePoint, IOHIPointing
);

UInt32 ApplePS2ALPSGlidePoint::deviceType() {
    return NX_EVS_DEVICE_TYPE_MOUSE;
}

UInt32 ApplePS2ALPSGlidePoint::interfaceID() {
    return NX_EVS_DEVICE_INTERFACE_BUS_ACE;
}

IOItemCount ApplePS2ALPSGlidePoint::buttonCount() {
    return 2;
};

IOFixed     ApplePS2ALPSGlidePoint::resolution() {
    return _resolution;
};

bool IsItALPS(ALPSStatus_t *E6, ALPSStatus_t *E7);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool ApplePS2ALPSGlidePoint::init(OSDictionary *dict) {
    //
    // Initialize this object's minimal state. This is invoked right after this
    // object is instantiated.
    //

    if (!super::init(dict))
        return false;

    // find config specific to Platform Profile
    OSDictionary *list = OSDynamicCast(OSDictionary, dict->getObject(kPlatformProfile));
    OSDictionary *config = ApplePS2Controller::makeConfigurationNode(list);
    if (config) {
        // if DisableDevice is Yes, then do not load at all...
        OSBoolean *disable = OSDynamicCast(OSBoolean, config->getObject(kDisableDevice));
        if (disable && disable->isTrue()) {
            config->release();
            return false;
        }
#ifdef DEBUG
        // save configuration for later/diagnostics...
        setProperty(kMergedConfiguration, config);
#endif
    }

    // initialize state...
    _device = 0;
    _interruptHandlerInstalled = false;
    _packetByteCount = 0;
    _resolution = (100) << 16; // (100 dpi, 4 counts/mm)
    _touchPadModeByte = kTapEnabled;
    _scrolling = SCROLL_NONE;
    _zscrollpos = 0;


    OSSafeRelease(config);

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

ApplePS2ALPSGlidePoint *ApplePS2ALPSGlidePoint::probe(IOService *provider, SInt32 *score) {
    DEBUG_LOG("ApplePS2ALPSGlidePoint::probe entered...\n");

    ALPSStatus_t E6, E7;
    //
    // The driver has been instructed to verify the presence of the actual
    // hardware we represent. We are guaranteed by the controller that the
    // mouse clock is enabled and the mouse itself is disabled (thus it
    // won't send any asynchronous mouse data that may mess up the
    // responses expected by the commands we send it).
    //

    bool success = false;

    if (!super::probe(provider, score))
        return 0;

    _device = (ApplePS2MouseDevice *) provider;
    _bounds.maxx = ALPS_V3_X_MAX;
    _bounds.maxy = ALPS_V3_Y_MAX;

    getModel(&E6, &E7);

    DEBUG_LOG("E7: { 0x%02x, 0x%02x, 0x%02x } E6: { 0x%02x, 0x%02x, 0x%02x }\n",
    E7.byte0, E7.byte1, E7.byte2, E6.byte0, E6.byte1, E6.byte2);

    success = IsItALPS(&E6, &E7);
    DEBUG_LOG("ALPS Device? %s\n", (success ? "yes" : "no"));

    // override
    //success = true;
    _touchPadVersion = (E7.byte2 & 0x0f) << 8 | E7.byte0;

    _device = 0;

    DEBUG_LOG("ApplePS2ALPSGlidePoint::probe leaving.\n");

    return (success) ? this : 0;
}

bool IsItALPS(ALPSStatus_t *E6, ALPSStatus_t *E7) {
    bool success = false;
    int version;
    static const unsigned char rates[] = {0, 10, 20, 40, 60, 80, 100, 200};
    short i;

    if ((E6->byte0 & 0xf8) != 0 || E6->byte1 != 0 ||
            (E6->byte2 != 10 && E6->byte2 != 100)) {
        return false;
    }

    UInt8 byte0, byte1, byte2;
    byte0 = E7->byte0;
    byte1 = E7->byte1;
    byte2 = E7->byte2;

    for (i = 0; i < ARRAY_SIZE(rates) && E7->byte2 != rates[i]; i++);// empty
    version = (E7->byte0 << 8) | (E7->byte1 << 4) | i;
    DEBUG_LOG("Discovered touchpad version: %d\n", version);

#define NUM_SINGLES 13
    static int singles[NUM_SINGLES * 3] = {
            0x33, 0x02, 0x0a,
            0x53, 0x02, 0x0a,
            0x53, 0x02, 0x14,
            0x63, 0x02, 0x0a,
            0x63, 0x02, 0x14,
            0x63, 0x02, 0x28,
            0x63, 0x02, 0x3c,
            0x63, 0x02, 0x50,
            0x63, 0x02, 0x64,
            0x73, 0x02, 0x0a,
            0x73, 0x02, 0x50,
            0x73, 0x02, 0x64}; // Dell E2 & HP Mini 311 multitouch
#define NUM_DUALS 5 // Mean it has also a track stick
    static int duals[NUM_DUALS * 3] = {
            0x20, 0x02, 0x0e,
            0x22, 0x02, 0x0a,
            0x22, 0x02, 0x14,
            0x42, 0x02, 0x14,
            0x73, 0x02, 0x64};

    for (i = 0; i < NUM_SINGLES; i++) {
        if ((byte0 == singles[i * 3]) && (byte1 == singles[i * 3 + 1]) &&
                (byte2 == singles[i * 3 + 2])) {
            success = true;
            break;
        }
    }

    if (!success) {
        for (i = 0; i < NUM_DUALS; i++) {
            if ((byte0 == duals[i * 3]) && (byte1 == duals[i * 3 + 1]) &&
                    (byte2 == duals[i * 3 + 2])) {
                success = true;
                break;
            }
        }
    }
    return success;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool ApplePS2ALPSGlidePoint::start(IOService *provider) {
    UInt64 enabledProperty;

    //
    // Must add this property to let our superclass know that it should handle
    // trackpad acceleration settings from user space.  Without this, tracking
    // speed adjustments from the mouse prefs panel have no effect.
    //

    setProperty(kIOHIDPointerAccelerationTypeKey, kIOHIDTrackpadAccelerationType);

    //
    // The driver has been instructed to start. This is called after a
    // successful probe and match.
    //

    if (!super::start(provider))
        return false;

    //
    // Maintain a pointer to and retain the provider object.
    //

    _device = (ApplePS2MouseDevice *) provider;
    _device->retain();

    //
    // Announce hardware properties.
    //

    IOLog("ApplePS2Trackpad: ALPS GlidePoint v%d.%d\n",
            (UInt8) (_touchPadVersion >> 8), (UInt8) (_touchPadVersion));

    //
    // Advertise some supported features (tapping, edge scrolling).
    //

    enabledProperty = 1;

    setProperty("Clicking", enabledProperty,
            sizeof(enabledProperty) * 8);
    setProperty("TrackpadScroll", enabledProperty,
            sizeof(enabledProperty) * 8);
    setProperty("TrackpadHorizScroll", enabledProperty,
            sizeof(enabledProperty) * 8);

    //
    // Lock the controller during initialization
    //

    _device->lock();

    if (!hwInitV3()) {
        DEBUG_LOG("Init failed...things will probably not work\n");
    }

    // Enable tapping
    // Not used in v3
    // TODO: put in specific v1/v2 init
//    DEBUG_LOG("enable tapping...\n");
//    setTapEnable(true);

    // Enable Absolute Mode
    // already done in hwInitV3
    // TODO: put in specific v1/v2 init
//    DEBUG_LOG("enable absolute mode orig...");
//    setAbsoluteMode();

    //
    // Finally, we enable the trackpad itself, so that it may start reporting
    // asynchronous events.
    //
    // Not needed in v3(?)
    // TODO: put in specific v1/v2 init
//    DEBUG_LOG("touchpad enable\n");
//    setTouchPadEnable(true);

    //
    // Enable the mouse clock (should already be so) and the mouse IRQ line.
    //

    // Should be taken care of in controller (at least that' what the logs indicate)
    //setCommandByte(kCB_EnableMouseIRQ, kCB_DisableMouseClock);

    //
    // Install our driver's interrupt handler, for asynchronous data delivery.
    //

    _device->installInterruptAction(this,
            OSMemberFunctionCast(PS2InterruptAction, this, &ApplePS2ALPSGlidePoint::interruptOccurred),
            OSMemberFunctionCast(PS2PacketAction, this, &ApplePS2ALPSGlidePoint::packetReady));
    _interruptHandlerInstalled = true;

    // now safe to allow other threads
    _device->unlock();

    //
    // Install our power control handler.
    //

    _device->installPowerControlAction(this, OSMemberFunctionCast(PS2PowerControlAction, this,
            &ApplePS2ALPSGlidePoint::setDevicePowerState));
    _powerControlHandlerInstalled = true;

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::stop(IOService *provider) {
    //
    // The driver has been instructed to stop.  Note that we must break all
    // connections to other service objects now (ie. no registered actions,
    // no pointers and retains to objects, etc), if any.
    //

    assert(_device == provider);

    //
    // Disable the mouse itself, so that it may stop reporting mouse events.
    //

    // should not need to do this...just reset the mouse(?)
//    setTouchPadEnable(false);
    resetMouse();

    //
    // Uninstall the interrupt handler.
    //

    if (_interruptHandlerInstalled) _device->uninstallInterruptAction();
    _interruptHandlerInstalled = false;

    //
    // Uninstall the power control handler.
    //

    if (_powerControlHandlerInstalled) _device->uninstallPowerControlAction();
    _powerControlHandlerInstalled = false;

    //
    // Release the pointer to the provider object.
    //

    OSSafeReleaseNULL(_device);

    super::stop(provider);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool ApplePS2ALPSGlidePoint::resetMouse() {
    TPS2Request<3> request;

    // Reset mouse
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut32 = 0x02ff;
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

    // TODO: add version specific first byte check
    // Right now this checks if the packet is either a PS/2 packet (data & 0xc8)
    // or if the first packet matches the specific trackpad first packet
    // in this case, right now it is for an ALPS v3
    if (0 == _packetByteCount && (data & 0xc8) != 0x08 && (data & ALPS_V3_MASK0) != ALPS_V3_BYTE0) {
        DEBUG_LOG("%s: Unexpected byte0 data (%02x) from PS/2 controller\n", getName(), data);
        return kPS2IR_packetBuffering;
    }

    /* Bytes 2 - packet size should have 0 in highest bit */
    if (_packetByteCount >= 1 && data == 0x80) {
        DEBUG_LOG("%s: Unexpected byte%d data (%02x) from PS/2 controller\n", getName(), _packetByteCount, data);
        _packetByteCount = 0;
        return kPS2IR_packetBuffering;
    }

    UInt8 *packet = _ringBuffer.head();
    packet[_packetByteCount++] = data;

    if (kPacketLengthLarge == _packetByteCount ||
            (kPacketLengthSmall == _packetByteCount && (packet[0] & 0xc8) == 0x08)) {
        // complete 6 or 3-byte packet received...
        // 3-byte packet is bare PS/2 packet instead of ALPS specific packet
        _ringBuffer.advanceHead(kPacketLengthMax);
        _packetByteCount = 0;
        return kPS2IR_packetReady;
    }
    return kPS2IR_packetBuffering;
}

void ApplePS2ALPSGlidePoint::packetReady() {
    // empty the ring buffer, dispatching each packet...
    while (_ringBuffer.count() >= kPacketLengthMax) {
        UInt8 *packet = _ringBuffer.tail();
        // now we have complete packet, either 6-byte or 3-byte
        if ((packet[0] & ALPS_V3_MASK0) == ALPS_V3_BYTE0) {
            processPacketV3(packet);
            // dispatchAbsolutePointerEventWithPacket(packet, kPacketLengthLarge);
        } else {
            // Ignore bare PS/2 packet for now...
//            dispatchRelativePointerEventWithPacket(packet, kPacketLengthSmall);
        }
        _ringBuffer.advanceTail(kPacketLengthSmall);
    }
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
    UInt32 buttons = 0;

    if (!(packet[0] & 0x40)) {
        DEBUG_LOG("Bad trackstick packet, disregarding...\n");
        return;
    }

    /* There is a special packet that seems to indicate the end
     * of a stream of trackstick data. Filter these out
     */
    if (packet[1] == 0x7f && packet[2] == 0x7f && packet[3] == 0x7f) {
        DEBUG_LOG("Ignoring trackstick packet that indicates end of stream\n");
        return;
    }

    x = (SInt8) (((packet[0] & 0x20) << 2) | (packet[1] & 0x7f));
    y = (SInt8) (((packet[0] & 0x10) << 3) | (packet[2] & 0x7f));
    z = (packet[4] & 0x7c) >> 2;

    /*
     * The x and y values tend to be quite large, and when used
     * alone the trackstick is difficult to use. Scale them down
     * to compensate.
     */
    x /= 8;
    y /= 8;

    clock_get_uptime(&now_abs);

    left = packet[3] & 0x01;
    right = packet[3] & 0x02;
    middle = packet[3] & 0x04;

    buttons |= left ? 0x01 : 0;
    buttons |= right ? 0x02 : 0;
    buttons |= middle ? 0x04 : 0;

    DEBUG_LOG("Dispatch relative pointer with x=%d, y=%d, left=%d, right=%d, middle=%d\n", x, y, left, right, middle);
    dispatchRelativePointerEventX(x, y, buttons, now_abs);

}

void ApplePS2ALPSGlidePoint::processTouchpadPacketV3(UInt8 *packet) {
    SInt16 x, y, z;
    int left, right, middle;
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    int fingers = 0, bmapFingers;
    unsigned int xBitmap, yBitmap;
    UInt32 buttons = 0;
    uint64_t now_abs;

    clock_get_uptime(&now_abs);

    /*
	 * There's no single feature of touchpad position and bitmap packets
	 * that can be used to distinguish between them. We rely on the fact
	 * that a bitmap packet should always follow a position packet with
	 * bit 6 of packet[4] set.
	 */
    if (_multiPacket) {
        DEBUG_LOG("Handling multi-packet\n");
        /*
         * Sometimes a position packet will indicate a multi-packet
         * sequence, but then what follows is another position
         * packet. Check for this, and when it happens process the
         * position packet as usual.
         */
        if (packet[0] & 0x40) {
            fingers = (packet[5] & 0x3) + 1;
            xBitmap = ((packet[4] & 0x7e) << 8) |
                    ((packet[1] & 0x7f) << 2) |
                    ((packet[0] & 0x30) >> 4);
            yBitmap = ((packet[3] & 0x70) << 4) |
                    ((packet[2] & 0x7f) << 1) |
                    (packet[4] & 0x01);

            bmapFingers = processBitmap(xBitmap, yBitmap,
                    &x1, &y1, &x2, &y2);
            DEBUG_LOG("Discovered bitmap fingers=%d\n", bmapFingers);

            /*
             * We shouldn't report more than one finger if
             * we don't have two coordinates.
             */
            if (fingers > 1 && bmapFingers < 2) {
                fingers = bmapFingers;
            }

            /* Now process position packet */
            packet = _multiData;
        } else {
            _multiPacket = 0;
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
    if (packet[0] & 0x40)
        return;

    if (!_multiPacket && (packet[4] & 0x40)) {
        DEBUG_LOG("Detected multi-packet first packet, waiting to handle\n");
        _multiPacket = 1;
        memcpy(_multiData, packet, sizeof(_multiData));
        return;
    }

    _multiPacket = 0;

    left = packet[3] & 0x01;
    right = packet[3] & 0x02;
    middle = packet[3] & 0x04;

    x = ((packet[1] & 0x7f) << 4) | ((packet[4] & 0x30) >> 2) |
            ((packet[0] & 0x30) >> 4);
    y = ((packet[2] & 0x7f) << 4) | (packet[4] & 0x0f);
    z = packet[5] & 0x7f;

    /*
     * Sometimes the hardware sends a single packet with z = 0
     * in the middle of a stream. Real releases generate packets
     * with x, y, and z all zero, so these seem to be flukes.
     * Ignore them.
     */
    if (x && y && !z) {
        return;
    }

    /*
     * If we don't have MT data or the bitmaps were empty, we have
     * to rely on ST data.
     */
    if (!fingers) {
        x1 = x;
        y1 = y;
        fingers = z > 0 ? 1 : 0;
    }

    if (z >= 64) {
        DEBUG_LOG("TODO: Figure out how to report BTN_TOUCH=1 here...\n");
    } else {
        DEBUG_LOG("TODO: Figure out how to report BTN_TOUCH=0 here...\n");
    }

    reportSemiMTData(fingers, x1, y1, x2, y2);

    // TODO: translate the following
    // input_mt_report_finger_count(dev, fingers);

    buttons |= left ? 0x01 : 0;
    buttons |= right ? 0x02 : 0;
    buttons |= middle ? 0x04 : 0;

    // Make sure we are still relative
    if (z == 0 || (_zpos >= 1 && z != 0)) {
        _xpos = x;
        _ypos = y;
    }

    if (z >= 64) {
        // NOTE: Tried the following absolute reporting but resulted in erratic mouse movement and clicking/selecting
        /*Point newLoc;
        newLoc.x = x;
        newLoc.y = y;*/

        // absolute mouse movement
        /*DEBUG_LOG("Sending absolute pointer event x=%d, y=%d, z=%d, left=%d, right=%d, middle=%d\n", x, y, z, left, right, middle);
        dispatchAbsolutePointerEvent(&newLoc,
                &_bounds,
                buttons,
                1, // inRange (always assume in range...)
                z, // pressure
                0, // pressure min,
                255, // pressure max (UInt8 max)
                90, // Stylus angle
                *(AbsoluteTime *) &now_abs);*/

        // So, try just a relative mouse movement based on previous absolute position
        int xdiff = x - _xpos;
        int ydiff = y - _ypos;

        DEBUG_LOG("Dispatch relative pointer event: x=%d, y=%d, dx=%d, dy=%d, left=%d, right=%d, middle=%d\n", x, y, xdiff, ydiff, left, right, middle);
        dispatchRelativePointerEventX(xdiff, ydiff, buttons, now_abs);
        _xpos = x;
        _ypos = y;
    } else {
        DEBUG_LOG("reporting pressure but no movement. z=%d, left=%d, right=%d, middle=%d\n", z, left, right, middle);
        // Only report pressure/buttons, but not new location (possibly a tapclick that we should signal with buttons=1 then 0 right after)
        dispatchRelativePointerEventX(0, 0, buttons, now_abs);
    }
    _zpos = z == 0 ? _zpos + 1 : 0;
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

    if (!xMap || !yMap)
        return 0;

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
    yMap = yMap << (sizeof(yMap) * BITS_PER_BYTE - ALPS_BITMAP_Y_BITS);
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
            if (prev_bit)
                point = &y_high;
            else
                point->num_bits = 0;
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

    *x1 = (ALPS_V3_X_MAX * (2 * xLow.start_bit + xLow.num_bits - 1)) /
            (2 * (ALPS_BITMAP_X_BITS - 1));
    *y1 = (ALPS_V3_Y_MAX * (2 * yLow.start_bit + yLow.num_bits - 1)) /
            (2 * (ALPS_BITMAP_Y_BITS - 1));

    if (fingers > 1) {
        *x2 = (ALPS_V3_X_MAX * (2 * x_high.start_bit + x_high.num_bits - 1)) /
                (2 * (ALPS_BITMAP_X_BITS - 1));
        *y2 = (ALPS_V3_Y_MAX * (2 * y_high.start_bit + y_high.num_bits - 1)) /
                (2 * (ALPS_BITMAP_Y_BITS - 1));
    }

    DEBUG_LOG("Process bitmap, fingers=%d\n", fingers);

    return fingers;
}

void ApplePS2ALPSGlidePoint::reportSemiMTData(int fingers, int x1, int y1, int x2, int y2) {
    DEBUG_LOG("TODO: process multi-touch data\n");
    //alps_set_slot(0, num_fingers != 0, x1, y1);
    //alps_set_slot(1, num_fingers == 2, x2, y2);
    // inside alps_set_slot:
    //input_mt_slot(dev, slot);
    //input_mt_report_slot_state(dev, MT_TOOL_FINGER, active);
    //if (active) {
    //    input_report_abs(dev, ABS_MT_POSITION_X, x);
    //    input_report_abs(dev, ABS_MT_POSITION_Y, y);
    //}
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::dispatchAbsolutePointerEventWithPacket(UInt8 *packet, UInt32 packetSize) {
    DEBUG_LOG("Got pointer event with packet = { %02x, %02x, %02x, %02x, %02x, %02x }\n", packet[0], packet[1], packet[2], packet[3], packet[4], packet[5]);
    UInt32 buttons = 0;
    int left = 0, right = 0, middle = 0;
    int xdiff, ydiff;
    short scroll;
    uint64_t now_abs;
    bool wasNotScrolling, willScroll;

    if (packet[5] == 0x3f) {
        DEBUG_LOG("%s:: TODO: process ALPS V3 trackstick packet...\n", getName());
    }

    int x = ((packet[1] & 0x7f) << 4) | ((packet[4] & 0x30) >> 2) |
            ((packet[0] & 0x30) >> 4);
    int y = ((packet[2] & 0x7f) << 4) | (packet[4] & 0x0f);
    int z = packet[5] & 0x7f;
//    int x = (packet[1] & 0x7f) | ((packet[2] & 0x78) << (7 - 3));
//    int y = (packet[4] & 0x7f) | ((packet[3] & 0x70) << (7 - 4));
//    int z = packet[5]; // touch pression

    clock_get_uptime(&now_abs);

    left = packet[3] & 0x01;
    right = packet[3] & 0x02;
    middle = packet[3] & 0x04;
//    left |= (packet[2]) & 1;
//    left |= (packet[3]) & 1;
//    right |= (packet[3] >> 1) & 1;

//    if (packet[0] != 0xff) {
//        left |= (packet[0]) & 1;
//        right |= (packet[0] >> 1) & 1;
//        middle |= (packet[0] >> 2) & 1;
//        middle |= (packet[3] >> 2) & 1;
//    }

    buttons |= left ? 0x01 : 0;
    buttons |= right ? 0x02 : 0;
    buttons |= middle ? 0x04 : 0;

    /*
     * Sometimes the hardware sends a single packet with z = 0
     * in the middle of a stream. Real releases generate packets
     * with x, y, and z all zero, so these seem to be flukes.
     * Ignore them.
     */
    if (x && y && !z) {
        DEBUG_LOG("%s: x and y were set but not z...not going to process\n", getName());
        return;
    }

    /*DEBUG_LOG("Absolute packet: x: %d, y: %d, xpos: %d, ypos: %d, buttons: %x, "
              "z: %d, zpos: %d\n", x, y, (int)_xpos, (int)_ypos, (int)buttons,
              (int)z, (int)_zpos);*/

    wasNotScrolling = _scrolling == SCROLL_NONE;
    scroll = insideScrollArea(x, y);

    willScroll = ((scroll & SCROLL_VERT) && _edgevscroll) ||
            ((scroll & SCROLL_HORIZ) && _edgehscroll);

    // Make sure we are still relative
    if (z == 0 || (_zpos >= 1 && z != 0 && !willScroll)) {
        _xpos = x;
        _ypos = y;
    }

    // Are we scrolling?
    if (willScroll) {
        if (_zscrollpos <= 0 || wasNotScrolling) {
            _xscrollpos = x;
            _yscrollpos = y;
        }

        xdiff = x - _xscrollpos;
        ydiff = y - _yscrollpos;

        ydiff = (scroll == SCROLL_VERT) ? -((int) ((double) ydiff * _edgeaccellvalue)) : 0;
        xdiff = (scroll == SCROLL_HORIZ) ? -((int) ((double) xdiff * _edgeaccellvalue)) : 0;

        // Those "if" should provide angle tapping (simulate click on up/down
        // buttons of a scrollbar), but i have to investigate more on the values,
        // since currently they don't work...
        if (ydiff == 0 && scroll == SCROLL_HORIZ)
            ydiff = ((x >= 950 ? 25 : (x <= 100 ? -25 : 0)) / max(_edgeaccellvalue, 1));

        if (xdiff == 0 && scroll == SCROLL_VERT)
            xdiff = ((y >= 950 ? 25 : (y <= 100 ? -25 : 0)) / max(_edgeaccellvalue, 1));

        dispatchScrollWheelEventX(ydiff, xdiff, 0, now_abs);
        _zscrollpos = z;
        return;
    }

    _zpos = z == 0 ? _zpos + 1 : 0;
    _scrolling = SCROLL_NONE;

    xdiff = x - _xpos;
    ydiff = y - _ypos;

    _xpos = x;
    _ypos = y;

    DEBUG_LOG("Sending event: %d,%d,%d\n", xdiff, ydiff, (int) buttons);
    dispatchRelativePointerEventX(xdiff, ydiff, buttons, now_abs);
}

short ApplePS2ALPSGlidePoint::insideScrollArea(int x, int y) {
    DEBUG_LOG("Checking scroll [x, y]=[%d, %d]\n", x, y);
    short scroll = 0;
    if (x > 900) scroll |= SCROLL_VERT;
    if (y > 650) scroll |= SCROLL_HORIZ;

    if (x > 900 && y > 650) {
        if (_scrolling == SCROLL_VERT)
            scroll = SCROLL_VERT;
        else
            scroll = SCROLL_HORIZ;
    }

    _scrolling = scroll;
    return scroll;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::
dispatchRelativePointerEventWithPacket(UInt8 *packet,
                UInt32 packetSize) {
    //
    // Process the three byte relative format packet that was retreived from the
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

void ApplePS2ALPSGlidePoint::setTapEnable(bool enable) {
    DEBUG_LOG("setTapEnable enter\n");
    //
    // Instructs the trackpad to honor or ignore tapping
    //

    ALPSStatus_t Status;
    getStatus(&Status);
    if (Status.byte0 & 0x04) {
        DEBUG_LOG("Tapping can only be toggled.\n");
        enable = false;
    }

    UInt8 cmd = enable ? kDP_SetMouseSampleRate : kDP_SetMouseResolution;
    UInt8 arg = enable ? 0x0A : 0x00;

    TPS2Request<10> request;
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_GetMouseInformation; //sync..
    request.commands[1].command = kPS2C_ReadDataPort;
    request.commands[1].inOrOut = 0;
    request.commands[2].command = kPS2C_ReadDataPort;
    request.commands[2].inOrOut = 0;
    request.commands[3].command = kPS2C_ReadDataPort;
    request.commands[3].inOrOut = 0;
    request.commands[4].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[4].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[5].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[5].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[6].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[6].inOrOut = cmd;
    request.commands[7].command = kPS2C_WriteCommandPort;
    request.commands[7].inOrOut = kCP_TransmitToMouse;
    request.commands[8].command = kPS2C_WriteDataPort;
    request.commands[8].inOrOut = arg;
    request.commands[9].command = kPS2C_ReadDataPortAndCompare;
    request.commands[9].inOrOut = kSC_Acknowledge;
    request.commandsCount = 10;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);

    getStatus(&Status);
    DEBUG_LOG("setTapEnable exit\n");
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::setTouchPadEnable(bool enable) {
    DEBUG_LOG("setTouchpadEnable enter\n");
    //
    // Instructs the trackpad to start or stop the reporting of data packets.
    // It is safe to issue this request from the interrupt/completion context.
    //

    // (mouse enable/disable command)
    TPS2Request<5> request;
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[1].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[2].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[3].inOrOut = kDP_SetDefaultsAndDisable;

    // (mouse or pad enable/disable command)
    request.commands[4].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[4].inOrOut = enable ? kDP_Enable : kDP_SetDefaultsAndDisable;
    request.commandsCount = 5;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
    DEBUG_LOG("setTouchpadEnable exit\n");
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOReturn ApplePS2ALPSGlidePoint::setParamProperties(OSDictionary *dict) {
    OSNumber *clicking = OSDynamicCast(OSNumber, dict->getObject("Clicking"));
    OSNumber *dragging = OSDynamicCast(OSNumber, dict->getObject("Dragging"));
    OSNumber *draglock = OSDynamicCast(OSNumber, dict->getObject("DragLock"));
    OSNumber *hscroll = OSDynamicCast(OSNumber, dict->getObject("TrackpadHorizScroll"));
    OSNumber *vscroll = OSDynamicCast(OSNumber, dict->getObject("TrackpadScroll"));
    OSNumber *eaccell = OSDynamicCast(OSNumber, dict->getObject("HIDTrackpadScrollAcceleration"));

    OSCollectionIterator *iter = OSCollectionIterator::withCollection(dict);
    OSObject *obj;

    iter->reset();
    while ((obj = iter->getNextObject()) != NULL) {
        OSString *str = OSDynamicCast(OSString, obj);
        OSNumber *val = OSDynamicCast(OSNumber, dict->getObject(str));

        if (val)
        DEBUG_LOG("%s: Dictionary Object: %s Value: %d\n", getName(),
        str->getCStringNoCopy(), val->unsigned32BitValue());
        else
                DEBUG_LOG("%s: Dictionary Object: %s Value: ??\n", getName(),
                str->getCStringNoCopy());
    }
    if (clicking) {
        UInt8 newModeByteValue = clicking->unsigned32BitValue() & 0x1 ?
                kTapEnabled :
                0;

        if (_touchPadModeByte != newModeByteValue) {
            _touchPadModeByte = newModeByteValue;
            // Only for v1/v2
//            setTapEnable(_touchPadModeByte);
            setProperty("Clicking", clicking);
        }
    }

    if (dragging) {
        _dragging = dragging->unsigned32BitValue() & 0x1 ? true : false;
        setProperty("Dragging", dragging);
    }

    if (draglock) {
        _draglock = draglock->unsigned32BitValue() & 0x1 ? true : false;
        setProperty("DragLock", draglock);
    }

    if (hscroll) {
        _edgehscroll = hscroll->unsigned32BitValue() & 0x1 ? true : false;
        setProperty("TrackpadHorizScroll", hscroll);
    }

    if (vscroll) {
        _edgevscroll = vscroll->unsigned32BitValue() & 0x1 ? true : false;
        setProperty("TrackpadScroll", vscroll);
    }
    if (eaccell) {
        _edgeaccell = eaccell->unsigned32BitValue();
        _edgeaccellvalue = (((double) (_edgeaccell / 1966.08)) / 75.0);
        _edgeaccellvalue = _edgeaccellvalue == 0 ? 0.01 : _edgeaccellvalue;
        setProperty("HIDTrackpadScrollAcceleration", eaccell);
    }

    return super::setParamProperties(dict);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::setDevicePowerState(UInt32 whatToDo) {
    // TODO: not sure what to do here to enable or disable for v3
    switch (whatToDo) {
        case kPS2C_DisableDevice:
            DEBUG_LOG("setDevicePowerState - disable\n");

            //
            // Disable touchpad.
            //

            //setTouchPadEnable(false);
            break;

        case kPS2C_EnableDevice:
            DEBUG_LOG("setDevicePowerState - enable\n");

//            setTapEnable(_touchPadModeByte);

            //
            // Finally, we enable the trackpad itself, so that it may
            // start reporting asynchronous events.
            //
            absoluteModeV3();

            _ringBuffer.reset();
            _packetByteCount = 0;

            //setTouchPadEnable(true);
            break;
    }
}

void ApplePS2ALPSGlidePoint::getStatus(ALPSStatus_t *status) {
    // (read command byte)
    TPS2Request<7> request;
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[1].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[2].inOrOut = kDP_SetDefaultsAndDisable;
    request.commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[3].inOrOut = kDP_GetMouseInformation;
    request.commands[4].command = kPS2C_ReadDataPort;
    request.commands[4].inOrOut = 0;
    request.commands[5].command = kPS2C_ReadDataPort;
    request.commands[5].inOrOut = 0;
    request.commands[6].command = kPS2C_ReadDataPort;
    request.commands[6].inOrOut = 0;
    request.commandsCount = 7;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);

    status->byte0 = request.commands[4].inOrOut;
    status->byte1 = request.commands[5].inOrOut;
    status->byte2 = request.commands[6].inOrOut;

    DEBUG_LOG("getStatus(): [%02x %02x %02x]\n", status->byte0, status->byte1, status->byte2);
}

void ApplePS2ALPSGlidePoint::getModel(ALPSStatus_t *E6, ALPSStatus_t *E7) {
    TPS2Request<9> request;

    resetMouse();

    // "E6 report"
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_SetMouseResolution;
    request.commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[1].inOrOut = 0;

    // 3X set mouse scaling 1 to 1
    request.commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[2].inOrOut = kDP_SetMouseScaling1To1;
    request.commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[3].inOrOut = kDP_SetMouseScaling1To1;
    request.commands[4].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[4].inOrOut = kDP_SetMouseScaling1To1;
    request.commands[5].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[5].inOrOut = kDP_GetMouseInformation;
    request.commands[6].command = kPS2C_ReadDataPort;
    request.commands[6].inOrOut = 0;
    request.commands[7].command = kPS2C_ReadDataPort;
    request.commands[7].inOrOut = 0;
    request.commands[8].command = kPS2C_ReadDataPort;
    request.commands[8].inOrOut = 0;
    request.commandsCount = 9;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);

    DEBUG_LOG("E6 Report: [%02x %02x %02x]\n", request.commands[6].inOrOut, request.commands[7].inOrOut, request.commands[8].inOrOut);

    // result is "E6 report"
    E6->byte0 = request.commands[6].inOrOut;
    E6->byte1 = request.commands[7].inOrOut;
    E6->byte2 = request.commands[8].inOrOut;

    // Now fetch "E7 report"
    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_SetMouseResolution;
    request.commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[1].inOrOut = 0;

    // 3X set mouse scaling 2 to 1
    request.commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[2].inOrOut = kDP_SetMouseScaling2To1;
    request.commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[3].inOrOut = kDP_SetMouseScaling2To1;
    request.commands[4].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[4].inOrOut = kDP_SetMouseScaling2To1;
    request.commands[5].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[5].inOrOut = kDP_GetMouseInformation;
    request.commands[6].command = kPS2C_ReadDataPort;
    request.commands[6].inOrOut = 0;
    request.commands[7].command = kPS2C_ReadDataPort;
    request.commands[7].inOrOut = 0;
    request.commands[8].command = kPS2C_ReadDataPort;
    request.commands[8].inOrOut = 0;
    request.commandsCount = 9;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);

    // result is "E7 report"
    E7->byte0 = request.commands[6].inOrOut;
    E7->byte1 = request.commands[7].inOrOut;
    E7->byte2 = request.commands[8].inOrOut;

    DEBUG_LOG("E7 report: [%02x %02x %02x]\n", request.commands[6].inOrOut, request.commands[7].inOrOut, request.commands[8].inOrOut);
}

void ApplePS2ALPSGlidePoint::setAbsoluteMode() {
    // (read command byte)
    TPS2Request<6> request;
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
    request.commands[5].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[5].inOrOut = 0xF0; //Set poll ??!
    request.commandsCount = 6;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);
}

bool ApplePS2ALPSGlidePoint::enterCommandMode() {
    DEBUG_LOG("enter command mode start\n");
    TPS2Request<8> request;
    ALPSStatus_t status;

    request.commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[1].inOrOut = kDP_MouseResetWrap;                  // 0xEC
    request.commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[2].inOrOut = kDP_MouseResetWrap;                  // 0xEC
    request.commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[3].inOrOut = kDP_MouseResetWrap;                  // 0xEC
    request.commands[4].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[4].inOrOut = kDP_GetMouseInformation;             // 0xE9
    request.commands[5].command = kPS2C_ReadDataPort;
    request.commands[5].inOrOut = 0;
    request.commands[6].command = kPS2C_ReadDataPort;
    request.commands[6].inOrOut = 0;
    request.commands[7].command = kPS2C_ReadDataPort;
    request.commands[7].inOrOut = 0;
    request.commandsCount = 8;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);

    // Result is "EC Report"
    status.byte0 = request.commands[5].inOrOut;
    status.byte1 = request.commands[6].inOrOut;
    status.byte2 = request.commands[7].inOrOut;
    DEBUG_LOG("ApplePS2ALPSGlidePoint EC Report: { 0x%02x, 0x%02x, 0x%02x }\n", status.byte0, status.byte1, status.byte2);

    if (status.byte0 != 0x88 && status.byte1 != 0x07) {
        DEBUG_LOG("ApplePS2ALPSGlidePoint: Failed to enter command mode!\n");
        return false;
    }

    DEBUG_LOG("enter command mode exit\n");
    return true;
}

bool ApplePS2ALPSGlidePoint::exitCommandMode() {
    DEBUG_LOG("exit command mode start\n");
    TPS2Request<1> request;

    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = kDP_SetMouseStreamMode;
    request.commandsCount = 1;
    assert(request.commandsCount <= countof(request.commands));
    _device->submitRequestAndBlock(&request);

    DEBUG_LOG("exit command mode exit\n");
    return true;
}

bool ApplePS2ALPSGlidePoint::hwInitV3() {
    // Following linux alps.c alps_hw_init_v3
    DEBUG_LOG("init v3 enter\n");

    int regVal;
    TPS2Request<10> request;

    if (!enterCommandMode()) {
        goto error;
    }

    /* check for trackstick */
    regVal = commandModeReadReg(0x0008);
    if (regVal == -1) {
        goto error;
    }
    if (regVal & 0x80) {
        DEBUG_LOG("Looks like there is a trackstick\n");
        if (!passthroughModeV3(true)) {
            goto error;
        }

        if (!exitCommandMode()) {
            goto error;
        }

        /* E7 report for trackstick */
        ALPSStatus_t status;
        request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
        request.commands[0].inOrOut = kDP_SetMouseScaling2To1;
        request.commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
        request.commands[1].inOrOut = kDP_SetMouseScaling2To1;
        request.commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
        request.commands[2].inOrOut = kDP_SetMouseScaling2To1;

        request.commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
        request.commands[3].inOrOut = kDP_GetMouseInformation;
        request.commands[4].command = kPS2C_WriteCommandPort;
        request.commands[4].inOrOut = kCP_TransmitToMouse;
        request.commands[5].command = kPS2C_WriteDataPort;
        request.commands[5].inOrOut = 0x64;
        request.commands[6].command = kPS2C_ReadDataPortAndCompare;
        request.commands[6].inOrOut = kSC_Acknowledge;
        request.commands[7].command = kPS2C_ReadDataPort;
        request.commands[7].inOrOut = 0;
        request.commands[8].command = kPS2C_ReadDataPort;
        request.commands[8].inOrOut = 0;
        request.commands[9].command = kPS2C_ReadDataPort;
        request.commands[9].inOrOut = 0;
        request.commandsCount = 10;
        assert(request.commandsCount <= countof(request.commands));
        _device->submitRequestAndBlock(&request);

        status.byte0 = request.commands[5].inOrOut;
        status.byte1 = request.commands[6].inOrOut;
        status.byte2 = request.commands[7].inOrOut;

        DEBUG_LOG("ApplePS2ALPSGlidePoint trackstick E7 report: [0x%02x, 0x%02x, 0x%02x]\n", status.byte0, status.byte1, status.byte2);

        // haha, nice comment from the linux driver
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
        commandModeSendNibble(0x9);
        commandModeSendNibble(0x4);
        DEBUG_LOG("Sent magic E6 sequence\n");

        if (!enterCommandMode()) {
            goto error_passthrough;
        }
        if (!passthroughModeV3(false)) {
            goto error;
        }
    }

    DEBUG_LOG("start absolute mode v3\n");
    if (!absoluteModeV3()) {
        DEBUG_LOG("Failed to enter absolute mode\n");
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

    /* Ensures trackstick packets are in the correct format */
    if (!commandModeWriteReg(0x0008, 0x82)) {
        goto error;
    }

    exitCommandMode();

    /* Set rate and enable data reporting */
    DEBUG_LOG("set sample rate\n");
    setSampleRateAndResolution(0x64, 0x02);

    DEBUG_LOG("hw init complete\n");
    return true;

    error_passthrough:
    if (!enterCommandMode()) {
        passthroughModeV3(false);
    }

    error:
    exitCommandMode();
    return false;
}


void ApplePS2ALPSGlidePoint::setSampleRateAndResolution(UInt8 rate, UInt8 res) {
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
}


int ApplePS2ALPSGlidePoint::commandModeReadReg(int addr) {
    TPS2Request<4> request;
    ALPSStatus_t status;

    if (!commandModeSetAddr(addr)) {
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

    status.byte0 = request.commands[1].inOrOut;
    status.byte1 = request.commands[2].inOrOut;
    status.byte2 = request.commands[3].inOrOut;

    DEBUG_LOG("ApplePS2ALPSGlidePoint read reg result: { 0x%02x, 0x%02x, 0x%02x }\n", status.byte0, status.byte1, status.byte2);

    /* The address being read is returned in the first 2 bytes
     * of the result. Check that the address matches the expected
     * address.
     */
    if (addr != ((status.byte0 << 8) | status.byte1)) {
        DEBUG_LOG("ApplePS2ALPSGlidePoint ERROR: read wrong registry value, expected: %x\n", addr);
        return -1;
    }

    return status.byte2;
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
    UInt8 command;
    TPS2Request<2> request;

    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    // TODO: make generic based on mouse version
    command = alps_v3_nibble_commands[nibble].command;
    request.commands[0].inOrOut = command;

    if (command & 0x0f00) {
        DEBUG_LOG("sendNibble: only sending command: 0x%02x\n", command);
        // linux -> param = unsigned char[4] -- dummy read 3 times?..other AppleLife version does not read
        // TODO: should we read the status here?
        request.commandsCount = 1;
    } else {
        DEBUG_LOG("sendNibble: sending command and param: [0x%02x, 0x%02x]\n", command, alps_v3_nibble_commands[nibble].data);
        request.commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
        request.commands[1].inOrOut = alps_v3_nibble_commands[nibble].data;
        request.commandsCount = 2;
    }
    assert(request.commandsCount <= countof(request.commands));

    _device->submitRequestAndBlock(&request);

    return true;
}

bool ApplePS2ALPSGlidePoint::commandModeSetAddr(int addr) {

    TPS2Request<1> request;
    int i, nibble;

    request.commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request.commands[0].inOrOut = ALPS_V3_ADDR_COMMAND;
    request.commandsCount = 1;
    _device->submitRequestAndBlock(&request);

    for (i = 12; i >= 0; i -= 4) {
        nibble = (addr >> i) & 0xf;
        commandModeSendNibble(nibble);
    }

    return true;
}

bool ApplePS2ALPSGlidePoint::passthroughModeV3(bool enable) {
    int regVal;

    regVal = commandModeReadReg(0x0008);
    if (regVal == -1) {
        return false;
    }

    if (enable) {
        regVal |= 0x01;
    } else {
        regVal &= ~0x01;
    }

    if (!commandModeWriteReg(regVal)) {
        return false;
    }

    return true;
};

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
