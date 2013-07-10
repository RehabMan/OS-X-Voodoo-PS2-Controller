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
#include <IOKit/hidsystem/IOHIPointing.h>
#include <IOKit/IOCommandGate.h>
#include "Decay.h"
#include "VoodooPS2TouchPadBase.h"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// ApplePS2SynapticsTouchPad Class Declaration
//

#define kPacketLength 6

class EXPORT ApplePS2SynapticsTouchPad : public VoodooPS2TouchPadBase
{
    typedef VoodooPS2TouchPadBase super;
	OSDeclareDefaultStructors(ApplePS2SynapticsTouchPad);
    
protected:
    UInt8               _touchPadType; // from identify: either 0x46 or 0x47
    UInt8               _touchPadModeByte;
    
    inline bool isInDisableZone(int x, int y)
        { return x > diszl && x < diszr && y > diszb && y < diszt; }
	
    virtual void   dispatchEventsWithPacket(UInt8* packet, UInt32 packetSize);
    virtual void   dispatchEventsWithPacketEW(UInt8* packet, UInt32 packetSize);
    // virtual void   dispatchSwipeEvent ( IOHIDSwipeMask swipeType, AbsoluteTime now);
    
    virtual void   setTouchPadEnable( bool enable );
    virtual bool   getTouchPadData( UInt8 dataSelector, UInt8 buf3[] );
    virtual bool   getTouchPadStatus(  UInt8 buf3[] );
    virtual bool   setTouchPadModeByte(UInt8 modeByteValue);
	virtual PS2InterruptResult interruptOccurred(UInt8 data);
    virtual void packetReady();
    
    void updateTouchpadLED();
    bool setTouchpadLED(UInt8 touchLED);
    bool setTouchpadModeByte();
    
    void queryCapabilities(void);
    
    enum MBComingFrom { fromPassthru, fromTimer, fromTrackpad, fromCancel };
    UInt32 middleButton(UInt32 butttons, uint64_t now, MBComingFrom from);
    
    void setParamPropertiesGated(OSDictionary* dict);
    
    virtual bool deviceSpecificInit();
    
    virtual void initTouchPad();

public:
    virtual ApplePS2SynapticsTouchPad * probe( IOService * provider,
                                               SInt32 *    score );
    virtual void stop( IOService * provider );
    
};

#endif /* _APPLEPS2SYNAPTICSTOUCHPAD_H */
