//
// Created by Brandon Pedersen on 5/1/13.
//

#ifndef __VoodooPS2TouchPadBase_H_
#define __VoodooPS2TouchPadBase_H_

#include "ApplePS2MouseDevice.h"
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/hidsystem/IOHIPointing.h>
#include <IOKit/IOCommandGate.h>
#include "Decay.h"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// VoodooPS2TouchPadBase Class Declaration
//

#define kPacketLength 6

class EXPORT VoodooPS2TouchPadBase : public IOHIPointing
{
    typedef IOHIPointing super;
    OSDeclareAbstractStructors(VoodooPS2TouchPadBase);

protected:
    ApplePS2MouseDevice * _device;
    bool                _interruptHandlerInstalled;
    bool                _powerControlHandlerInstalled;
    bool                _messageHandlerInstalled;
    RingBuffer<UInt8, kPacketLength*32> _ringBuffer;
    UInt32              _packetByteCount;
    UInt8               _lastdata;
    UInt16              _touchPadVersion;

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
	int lastx, lasty, last_fingers;
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
    SimpleAverage<uint64_t, 32> time_history;
    IOTimerEventSource* scrollTimer;
    uint64_t momentumscrolltimer;
    int momentumscrollthreshy;
    uint64_t momentumscrollinterval;
    int momentumscrollsum;
    int64_t momentumscrollcurrent;
    int64_t momentumscrollrest1;
    int momentumscrollmultiplier;
    int momentumscrolldivisor;
    int momentumscrollrest2;
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

    inline bool isInDisableZone(int x, int y)
        { return x > diszl && x < diszr && y > diszb && y < diszt; }

    // Sony: coordinates captured from single touch event
    // Don't know what is the exact value of x and y on edge of touchpad
    // the best would be { return x > xmax/2 && y < ymax/4; }

    inline bool isInRightClickZone(int x, int y)
        { return x > rczl && x < rczr && y > rczb && y < rczt; }

    virtual void   setTouchPadEnable( bool enable ) = 0;
	virtual PS2InterruptResult interruptOccurred(UInt8 data) = 0;
    virtual void packetReady() = 0;
    virtual void   setDevicePowerState(UInt32 whatToDo);

    virtual void   receiveMessage(int message, void* data);

    virtual void touchpadToggled() {};
    virtual void touchpadShutdown() {};
    virtual void initTouchPad();

    inline bool isFingerTouch(int z) { return z>z_finger && z<zlimit; }

    void onScrollTimer(void);
    void onButtonTimer(void);
    void onDragTimer(void);

    enum MBComingFrom { fromPassthru, fromTimer, fromTrackpad, fromCancel };
    UInt32 middleButton(UInt32 buttons, uint64_t now, MBComingFrom from);

    virtual void setParamPropertiesGated(OSDictionary* dict);

	virtual IOItemCount buttonCount();
	virtual IOFixed     resolution();
    virtual bool deviceSpecificInit() = 0;
    inline void dispatchRelativePointerEventX(int dx, int dy, UInt32 buttonState, uint64_t now)
        { dispatchRelativePointerEvent(dx, dy, buttonState, *(AbsoluteTime*)&now); }
    inline void dispatchScrollWheelEventX(short deltaAxis1, short deltaAxis2, short deltaAxis3, uint64_t now)
        { dispatchScrollWheelEvent(deltaAxis1, deltaAxis2, deltaAxis3, *(AbsoluteTime*)&now); }
    inline void setTimerTimeout(IOTimerEventSource* timer, uint64_t time)
        { timer->setTimeout(*(AbsoluteTime*)&time); }
    inline void cancelTimer(IOTimerEventSource* timer)
        { timer->cancelTimeout(); }

public:
    virtual bool init( OSDictionary * properties );
    virtual VoodooPS2TouchPadBase * probe( IOService * provider,
                                               SInt32 *    score ) = 0;
    virtual bool start( IOService * provider );
    virtual void stop( IOService * provider );

    virtual UInt32 deviceType();
    virtual UInt32 interfaceID();

	virtual IOReturn setParamProperties(OSDictionary * dict);
	virtual IOReturn setProperties(OSObject *props);
};

#endif //__VoodooPS2TouchPadBase_H_
