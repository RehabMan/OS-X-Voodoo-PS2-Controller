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

// =============================================================================
// ApplePS2ALPSGlidePoint Class Implementation
//

OSDefineMetaClassAndStructors(ApplePS2ALPSGlidePoint, VoodooPS2TouchPadBase
);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

ApplePS2ALPSGlidePoint *ApplePS2ALPSGlidePoint::probe(IOService *provider, SInt32 *score) {
    DEBUG_LOG("ApplePS2ALPSGlidePoint::probe entered...\n");


    //
    // The driver has been instructed to verify the presence of the actual
    // hardware we represent. We are guaranteed by the controller that the
    // mouse clock is enabled and the mouse itself is disabled (thus it
    // won't send any asynchronous mouse data that may mess up the
    // responses expected by the commands we send it).
    //

    ALPSStatus_t E6, E7;
    bool success;

    _device = (ApplePS2MouseDevice *) provider;
    _bounds.maxx = ALPS_V3_X_MAX;
    _bounds.maxy = ALPS_V3_Y_MAX;

    getModel(&E6, &E7);

    DEBUG_LOG("E7: { 0x%02x, 0x%02x, 0x%02x } E6: { 0x%02x, 0x%02x, 0x%02x }\n",
    E7.byte0, E7.byte1, E7.byte2, E6.byte0, E6.byte1, E6.byte2);

    success = isItALPS(&E6, &E7);
    DEBUG_LOG("ALPS Device? %s\n", (success ? "yes" : "no"));

    if (success) {
        IOLog("%s: ALPS model 0x%02x,0x%02x,0x%02x\n", getName(), E7.byte0, E7.byte1, E7.byte2);
    }

    // override
    //success = true;
    _touchPadVersion = (E7.byte2 & 0x0f) << 8 | E7.byte0;

    _device = 0;

    DEBUG_LOG("ApplePS2ALPSGlidePoint::probe leaving.\n");

    return (success) ? this : 0;
}

bool ApplePS2ALPSGlidePoint::isItALPS(ALPSStatus_t *E6, ALPSStatus_t *E7) {
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
    DEBUG_LOG("Discovered touchpad version: 0x%x\n", version);

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

void ApplePS2ALPSGlidePoint::deviceSpecificInit() {
    // TODO: add version dependant init
    if (!hwInitV3()) {
        IOLog("%s: Device initialization failed. Touchpad probably won't work", getName());
    }
}

bool ApplePS2ALPSGlidePoint::init(OSDictionary *dict) {
    if (!super::init(dict)) {
        return false;
    }

    // Set defaults for this mouse model
    zlimit = 255;
    centerx = 1000;
    centery = 700;
    ledge = 0;
    // Right edge, must allow for vertical scrolling
    redge = ALPS_V3_X_MAX - 250;
    tedge = 0;
    bedge = ALPS_V3_Y_MAX;
    vscrolldivisor = 1;
    hscrolldivisor = 1;
    divisorx = 40;
    divisory = 40;
    hscrolldivisor = 50;
    vscrolldivisor = 50;
    _buttonCount = 3;

    return true;
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
            DEBUG_LOG("Got pointer event with packet = { %02x, %02x, %02x, %02x, %02x, %02x }\n", packet[0], packet[1], packet[2], packet[3], packet[4], packet[5]);
            processPacketV3(packet);
        } else {
            // Ignore bare PS/2 packet for now...messes with the actual full 6-byte ALPS packet above
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

    // TODO: separate divisor for trackstick
//    x /= divisorx;
//    y /= divisory;

    clock_get_uptime(&now_abs);

    left = packet[3] & 0x01;
    right = packet[3] & 0x02;
    middle = packet[3] & 0x04;

    buttons |= left ? 0x01 : 0;
    buttons |= right ? 0x02 : 0;
    buttons |= middle ? 0x04 : 0;

    // Reverse y value to get proper movement direction
    y = -y;

    DEBUG_LOG("Dispatch relative pointer with x=%d, y=%d, left=%d, right=%d, middle=%d, (z=%d, not reported)\n",
    x, y, left, right, middle, z);
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

    // TODO: check version of trackpad to see if it has the
    // ALPS_QUIRK_TRACKSTICK_BUTTONS thing
    // This is for checking if the trackstick mouse buttons were pressed
    left |= packet[3] & 0x10;
    right |= packet[3] & 0x20;
    middle |= packet[3] & 0x40;

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

    buttons |= left ? 0x01 : 0;
    buttons |= right ? 0x02 : 0;
    buttons |= middle ? 0x04 : 0;

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

    // double tap in "disable zone" (upper left) for trackpad enable/disable
    //    diszctrl = 0  means automatic enable this feature if trackpad has LED
    //    diszctrl = 1  means always enable this feature
    //    diszctrl = -1 means always disable this feature
    if ((0 == diszctrl && ledpresent) || 1 == diszctrl) {
        DEBUG_LOG("checking disable zone touch. Touchmode=%d\n", touchmode);
        // deal with taps in the disable zone
        // look for a double tap inside the disable zone to enable/disable touchpad
        switch (touchmode) {
            case MODE_NOTOUCH:
                if (isFingerTouch(z) && isInDisableZone(x, y)) {
                    touchtime = now_ns;
                    touchmode = MODE_WAIT1RELEASE;
                    DEBUG_LOG("ps2: detected touch1 in disable zone\n");
                }
                break;
            case MODE_WAIT1RELEASE:
                if (z < z_finger) {
                    DEBUG_LOG("ps2: detected untouch1 in disable zone...\n");
                    if (now_ns - touchtime < maxtaptime) {
                        DEBUG_LOG("ps2: setting MODE_WAIT2TAP.\n");
                        untouchtime = now_ns;
                        touchmode = MODE_WAIT2TAP;
                    }
                    else {
                        DEBUG_LOG("ps2: setting MODE_NOTOUCH.\n");
                        touchmode = MODE_NOTOUCH;
                    }
                }
                else {
                    if (!isInDisableZone(x, y)) {
                        DEBUG_LOG("ps2: moved outside of disable zone in MODE_WAIT1RELEASE\n");
                        touchmode = MODE_NOTOUCH;
                    }
                }
                break;
            case MODE_WAIT2TAP:
                if (isFingerTouch(z)) {
                    if (isInDisableZone(x, y)) {
                        DEBUG_LOG("ps2: detected touch2 in disable zone...\n");
                        if (now_ns - untouchtime < maxdragtime) {
                            DEBUG_LOG("ps2: setting MODE_WAIT2RELEASE.\n");
                            touchtime = now_ns;
                            touchmode = MODE_WAIT2RELEASE;
                        }
                        else {
                            DEBUG_LOG("ps2: setting MODE_NOTOUCH.\n");
                            touchmode = MODE_NOTOUCH;
                        }
                    }
                    else {
                        DEBUG_LOG("ps2: bad input detected in MODE_WAIT2TAP x=%d, y=%d, z=%d\n", x, y, z);
                        touchmode = MODE_NOTOUCH;
                    }
                }
                break;
            case MODE_WAIT2RELEASE:
                if (z < z_finger) {
                    DEBUG_LOG("ps2: detected untouch2 in disable zone...\n");
                    if (now_ns - touchtime < maxtaptime) {
                        DEBUG_LOG("ps2: %s trackpad.\n", ignoreall ? "enabling" : "disabling");
                        // enable/disable trackpad here
                        ignoreall = !ignoreall;
                        touchpadToggled();
                        touchmode = MODE_NOTOUCH;
                    }
                    else {
                        DEBUG_LOG("ps2: not in time, ignoring... setting MODE_NOTOUCH\n");
                        touchmode = MODE_NOTOUCH;
                    }
                }
                else {
                    if (!isInDisableZone(x, y)) {
                        DEBUG_LOG("ps2: moved outside of disable zone in MODE_WAIT2RELEASE\n");
                        touchmode = MODE_NOTOUCH;
                    }
                }
                break;
            default:; // nothing...
        }
        if (touchmode >= MODE_WAIT1RELEASE) {
            DEBUG_LOG("Touchmode is WAIT1RELEASE, returning\n");
            return;
        }
    }

    // if trackpad input is supposed to be ignored, then don't do anything
    if (ignoreall) {
        DEBUG_LOG("ignoreall is set, returning\n");
        return;
    }

    int tm1 = touchmode;

    if (z < z_finger && isTouchMode()) {
        // Finger has been lifted
        DEBUG_LOG("finger lifted after touch\n");
        xrest = yrest = scrollrest = 0;
        inSwipeLeft = inSwipeRight = inSwipeUp = inSwipeDown = 0;
        xmoved = ymoved = 0;
        untouchtime = now_ns;
        tracksecondary = false;

        if (dy_history.count()) {
            DEBUG_LOG("ps2: newest=%llu, oldest=%llu, diff=%llu, avg: %d/%d=%d\n", time_history.newest(), time_history.oldest(), time_history.newest() - time_history.oldest(), dy_history.sum(), dy_history.count(), dy_history.average());
        }
        else {
            DEBUG_LOG("ps2: no time/dy history\n");
        }

        // check for scroll momentum start
        if (MODE_MTOUCH == touchmode && momentumscroll && momentumscrolltimer) {
            // releasing when we were in touchmode -- check for momentum scroll
            if (dy_history.count() > momentumscrollsamplesmin &&
                    (momentumscrollinterval = time_history.newest() - time_history.oldest())) {
                momentumscrollsum = dy_history.sum();
                momentumscrollcurrent = momentumscrolltimer * momentumscrollsum;
                momentumscrollrest1 = 0;
                momentumscrollrest2 = 0;
                setTimerTimeout(scrollTimer, momentumscrolltimer);
            }
        }
        time_history.reset();
        dy_history.reset();
        DEBUG_LOG("ps2: now_ns-touchtime=%lld (%s). touchmode=%d\n", (uint64_t) (now_ns - touchtime) / 1000, now_ns - touchtime < maxtaptime ? "true" : "false", touchmode);
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
                    break;
            }
        } else {
            if ((touchmode == MODE_DRAG || touchmode == MODE_DRAGLOCK) && (draglock || draglocktemp))
                touchmode = MODE_DRAGNOTOUCH;
            else {
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

    int tm2 = touchmode;
    int dx = 0, dy = 0;

    DEBUG_LOG("touchmode=%d\n", touchmode);
    switch (touchmode) {
        case MODE_DRAG:
        case MODE_DRAGLOCK:
            if (MODE_DRAGLOCK == touchmode || (!immediateclick || now_ns - touchtime > maxdbltaptime)) {
                buttons |= 0x1;
            }
            // fall through
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
                    if ((dy < 0) != (dy_history.newest() < 0) || dy == 0) {
                        // stopped or changed direction, clear history
                        dy_history.reset();
                        time_history.reset();
                    }
                    // put movement and time in history for later
                    dy_history.filter(dy);
                    time_history.filter(now_ns);
                    if (0 != dy || 0 != dx) {
                        if (!hscroll) {
                            dx = 0;
                        }
                        // reverse dy to get correct movement
                        dy = -dy;
                        DEBUG_LOG("%s::dispatchScrollWheelEventX: dv=%d, dh=%d\n", getName(), dy, dx);
                        dispatchScrollWheelEventX(dy, dx, 0, now_abs);
                        dx = dy = 0;
                    }
                    break;

                case 3: // three finger
                    xmoved += lastx - x;
                    ymoved += y - lasty;
                    // dispatching 3 finger movement
                    if (ymoved > swipedy && !inSwipeUp) {
                        inSwipeUp = 1;
                        inSwipeDown = 0;
                        ymoved = 0;
                        DEBUG_LOG("swipe up\n");
                        _device->dispatchKeyboardMessage(kPS2M_swipeUp, &now_abs);
                        break;
                    }
                    if (ymoved < -swipedy && !inSwipeDown) {
                        inSwipeDown = 1;
                        inSwipeUp = 0;
                        ymoved = 0;
                        DEBUG_LOG("swipe down\n");
                        _device->dispatchKeyboardMessage(kPS2M_swipeDown, &now_abs);
                        break;
                    }
                    if (xmoved < -swipedx && !inSwipeRight) {
                        inSwipeRight = 1;
                        inSwipeLeft = 0;
                        xmoved = 0;
                        DEBUG_LOG("swipe right\n");
                        _device->dispatchKeyboardMessage(kPS2M_swipeRight, &now_abs);
                        break;
                    }
                    if (xmoved > swipedx && !inSwipeLeft) {
                        inSwipeLeft = 1;
                        inSwipeRight = 0;
                        xmoved = 0;
                        DEBUG_LOG("swipe left\n");
                        _device->dispatchKeyboardMessage(kPS2M_swipeLeft, &now_abs);
                        break;
                    }
            }
            break;

        case MODE_VSCROLL:
            if (!vsticky && (x < redge || fingers > 1 || z > zlimit)) {
                DEBUG_LOG("Switch back to notoch. redge=%d, vsticky=%d, zlimit=%d\n", redge, vsticky, zlimit);
                touchmode = MODE_NOTOUCH;
                break;
            }
            if (palm_wt && now_ns - keytime < maxaftertyping) {
                DEBUG_LOG("Ignore vscroll after typing\n");
                break;
            }
            dy = ((lasty - y) / (vscrolldivisor / 100.0));
            DEBUG_LOG("VScroll: dy=%d\n", dy);
            dispatchScrollWheelEventX(dy, 0, 0, now_abs);
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
            DEBUG_LOG("HScroll: dx=%d\n", dx);
            dispatchScrollWheelEventX(0, dx, 0, now_abs);
            dx = 0;
            break;

        case MODE_CSCROLL:
            if (palm_wt && now_ns - keytime < maxaftertyping) {
                break;
            }
            
            if (y < centery) {
                dx = x - lastx;
            }
            else {
                dx = lastx - x;
            }
            
            if (x < centerx) {
                dx += lasty - y;
            }
            else {
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
        if ((!palm_wt || now_ns - keytime >= maxaftertyping) && !momentumscrollcurrent) {
            if (!isTouchMode()) {
                DEBUG_LOG("Set touchtime to now=%llu, x=%d, y=%d, fingers=%d", now_ns, x, y, fingers);
                touchtime = now_ns;
                touchx = x;
                touchy = y;
            }
            ////if (w>wlimit || w<3)
            if (fingers == 2) {
                wasdouble = true;
            } else if (fingers == 3) {
                wastriple = true;
            }
        }
        // any touch cancels momentum scroll
        momentumscrollcurrent = 0;
    }

    // switch modes, depending on input
    if (touchmode == MODE_PREDRAG && isFingerTouch(z)) {
        DEBUG_LOG("Switch from pre-drag to drag\n");
        touchmode = MODE_DRAG;
        draglocktemp = _modifierdown & draglocktempmask;
    }
    if (touchmode == MODE_DRAGNOTOUCH && isFingerTouch(z)) {
        DEBUG_LOG("switch from dragnotouch to drag lock\n");
        touchmode = MODE_DRAGLOCK;
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
            z > z_finger && x > redge && vscrolldivisor && scroll) {
        DEBUG_LOG("switch to vscroll touchmode\n");
        touchmode = MODE_VSCROLL;
        scrollrest = 0;
    }
    if ((MODE_NOTOUCH == touchmode || (MODE_VSCROLL == touchmode && x <= redge)) &&
            z > z_finger && y > bedge && hscrolldivisor && hscroll && scroll) {
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

    DEBUG_LOG("ps2: dx=%d, dy=%d (%d,%d) z=%d mode=(%d,%d,%d) buttons=%d wasdouble=%d\n", dx, dy, x, y, z, tm1, tm2, touchmode, buttons, wasdouble);
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

void ApplePS2ALPSGlidePoint::setTouchPadEnable(bool enable) {
    DEBUG_LOG("setTouchpadEnable enter\n");
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

void ApplePS2ALPSGlidePoint::initTouchPad() {
    deviceSpecificInit();
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
    IOLog("Initializing TouchPad hardware...this may take a second.\n");

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

    IOLog("TouchPad initialization complete\n");
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
