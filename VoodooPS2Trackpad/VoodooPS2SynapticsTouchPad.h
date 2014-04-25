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
#include <IOKit/acpi/IOACPIPlatformDevice.h>

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
    IOACPIPlatformDevice*_provider;

#if 0//MERGE
    IOCommandGate*      _cmdGate;
    
	int z_finger;
	int divisorx, divisory;
	int ledge;
	int redge;
	int tedge;
	int bedge;
	int vscrolldivisor, hscrolldivisor, cscrolldivisor;
	int ctrigger;
	int centerx;
	int centery;
	uint64_t maxtaptime;
	uint64_t maxdragtime;
    uint64_t maxdbltaptime;
	int hsticky,vsticky, wsticky, tapstable;
	int wlimit, wvdivisor, whdivisor;
	bool clicking;
	bool dragging;
	bool draglock;
    int draglocktemp;
	bool hscroll, scroll;
	bool rtap;
    bool outzone_wt, palm, palm_wt;
    int zlimit;
    int noled;
    uint64_t maxaftertyping;
    int mousemultiplierx, mousemultipliery;
    int mousescrollmultiplierx, mousescrollmultipliery;
    int mousemiddlescroll;
    int wakedelay;
    int smoothinput;
    int unsmoothinput;
    int skippassthru;
    int tapthreshx, tapthreshy;
    int dblthreshx, dblthreshy;
    int zonel, zoner, zonet, zoneb;
    int diszl, diszr, diszt, diszb;
    int diszctrl; // 0=automatic (ledpresent), 1=enable always, -1=disable always
    int _resolution, _scrollresolution;
    int swipedx, swipedy;
    int _buttonCount;
    int swapdoubletriple;
    int draglocktempmask;
    uint64_t clickpadclicktime;
    int clickpadtrackboth;
    int ignoredeltasstart;
    int bogusdxthresh, bogusdythresh;
    int scrolldxthresh, scrolldythresh;
    int immediateclick;

    // three finger state
    uint8_t inSwipeLeft, inSwipeRight;
    uint8_t inSwipeUp, inSwipeDown;
    int xmoved, ymoved;
    
    int rczl, rczr, rczb, rczt; // rightclick zone for 1-button ClickPads
    
    // state related to secondary packets/extendedwmode
    int lastx2, lasty2;
    bool tracksecondary;
    int xrest2, yrest2;
    bool clickedprimary;
    bool _extendedwmode;

    // normal state
	int lastx, lasty, lastf;
    UInt32 lastbuttons;
    int ignoredeltas;
	int xrest, yrest, scrollrest;
    int touchx, touchy;
	uint64_t touchtime;
	uint64_t untouchtime;
	bool wasdouble,wastriple;
    uint64_t keytime;
    bool ignoreall;
    UInt32 passbuttons;
#ifdef SIMULATE_PASSTHRU
    UInt32 trackbuttons;
#endif
    bool passthru;
    bool ledpresent;
    bool _reportsv;
    int clickpadtype;   //0=not, 1=1button, 2=2button, 3=reserved
    UInt32 _clickbuttons;  //clickbuttons to merge into buttons
    int mousecount;
    bool usb_mouse_stops_trackpad;
    
    int _modifierdown; // state of left+right control keys
    int scrollzoommask;
    
    // for scaling x/y values
    int xupmm, yupmm;
    
    // for middle button simulation
    enum mbuttonstate
    {
        STATE_NOBUTTONS,
        STATE_MIDDLE,
        STATE_WAIT4TWO,
        STATE_WAIT4NONE,
        STATE_NOOP,
    } _mbuttonstate;
    
    UInt32 _pendingbuttons;
    uint64_t _buttontime;
    IOTimerEventSource* _buttonTimer;
    uint64_t _maxmiddleclicktime;
    int _fakemiddlebutton;

    // momentum scroll state
    bool momentumscroll;
    SimpleAverage<int, 32> dy_history;
    SimpleAverage<int, 32> dx_history;
    SimpleAverage<uint64_t, 32> time_history;
    IOTimerEventSource* scrollTimer;
    uint64_t momentumscrolltimer;
    int momentumscrollthreshy;
    uint64_t momentumscrollinterval;
    int momentumscrollsum_y;
    int momentumscrollsum_x;
    int64_t momentumscrollcurrent_x;
    int64_t momentumscrollcurrent_y;
    int64_t momentumscrollrest1_y;
    int64_t momentumscrollrest1_x;
    int momentumscrollmultiplier;
    int momentumscrolldivisor;
    int momentumscrollrest2_x;
    int momentumscrollrest2_y;
    int momentumscrollsamplesmin;
    
    // timer for drag delay
    uint64_t dragexitdelay;
    IOTimerEventSource* dragTimer;
   
    SimpleAverage<int, 4> x_avg;
    SimpleAverage<int, 4> y_avg;
    //DecayingAverage<int, int64_t, 1, 1, 2> x_avg;
    //DecayingAverage<int, int64_t, 1, 1, 2> y_avg;
    UndecayAverage<int, int64_t, 1, 1, 2> x_undo;
    UndecayAverage<int, int64_t, 1, 1, 2> y_undo;
    
    SimpleAverage<int, 4> x2_avg;
    SimpleAverage<int, 4> y2_avg;
    //DecayingAverage<int, int64_t, 1, 1, 2> x2_avg;
    //DecayingAverage<int, int64_t, 1, 1, 2> y2_avg;
    UndecayAverage<int, int64_t, 1, 1, 2> x2_undo;
    UndecayAverage<int, int64_t, 1, 1, 2> y2_undo;
    
	enum
    {
        // "no touch" modes... must be even (see isTouchMode)
        MODE_NOTOUCH =      0,
		MODE_PREDRAG =      2,
        MODE_DRAGNOTOUCH =  4,

        // "touch" modes... must be odd (see isTouchMode)
        MODE_MOVE =         1,
        MODE_VSCROLL =      3,
        MODE_HSCROLL =      5,
        MODE_CSCROLL =      7,
        MODE_MTOUCH =       9,
        MODE_DRAG =         11,
        MODE_DRAGLOCK =     13,
        
        // special modes for double click in LED area to enable/disable
        // same "touch"/"no touch" odd/even rule (see isTouchMode)
        MODE_WAIT1RELEASE = 101,    // "touch"
        MODE_WAIT2TAP =     102,    // "no touch"
        MODE_WAIT2RELEASE = 103,    // "touch"
    } touchmode;
    
    inline bool isTouchMode() { return touchmode & 1; }
    
#endif //#if 0//MERGE
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

#if 0//MERGE
    void onButtonTimer(void);
    
    void onDragTimer(void);
    
#endif
    enum MBComingFrom { fromPassthru, fromTimer, fromTrackpad, fromCancel };
    UInt32 middleButton(UInt32 butttons, uint64_t now, MBComingFrom from);
    
    virtual void setParamPropertiesGated(OSDictionary* dict);
    
    virtual bool deviceSpecificInit();
    
    virtual void afterInstallInterrupt();
    virtual void afterDeviceUnlock();

    virtual void initTouchPad();

    virtual void touchpadToggled();
    virtual void touchpadShutdown();

public:
    virtual bool init( OSDictionary * properties );
    virtual ApplePS2SynapticsTouchPad * probe( IOService * provider,
                                               SInt32 *    score );
    virtual void stop( IOService * provider );
    
};

#endif /* _APPLEPS2SYNAPTICSTOUCHPAD_H */
