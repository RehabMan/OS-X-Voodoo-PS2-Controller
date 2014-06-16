//
// Created by Brandon Pedersen on 5/1/13.
//

#include <IOKit/IOLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include "VoodooPS2Controller.h"
#include "VoodooPS2TouchPadBase.h"

// =============================================================================
// VoodooPS2TouchPadBase Class Implementation
//

OSDefineMetaClassAndAbstractStructors(VoodooPS2TouchPadBase, IOHIPointing);

UInt32 VoodooPS2TouchPadBase::deviceType()
{ return NX_EVS_DEVICE_TYPE_MOUSE; };

UInt32 VoodooPS2TouchPadBase::interfaceID()
{ return NX_EVS_DEVICE_INTERFACE_BUS_ACE; };

IOItemCount VoodooPS2TouchPadBase::buttonCount() { return _buttonCount; };
IOFixed     VoodooPS2TouchPadBase::resolution()  { return _resolution << 16; };

#define abs(x) ((x) < 0 ? -(x) : (x))

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool VoodooPS2TouchPadBase::init(OSDictionary * dict)
{
    //
    // Initialize this object's minimal state. This is invoked right after this
    // object is instantiated.
    //
    
    if (!super::init(dict))
        return false;

    // find config specific to Platform Profile
    OSDictionary* list = OSDynamicCast(OSDictionary, dict->getObject(kPlatformProfile));
    OSDictionary* config = ApplePS2Controller::makeConfigurationNode(list);
    if (config)
    {
        // if DisableDevice is Yes, then do not load at all...
        OSBoolean* disable = OSDynamicCast(OSBoolean, config->getObject(kDisableDevice));
        if (disable && disable->isTrue())
        {
            config->release();
            return false;
        }
#ifdef DEBUG
        // save configuration for later/diagnostics...
        setProperty(kMergedConfiguration, config);
#endif
    }
    
    // initialize state...
    _device = NULL;
    _interruptHandlerInstalled = false;
    _powerControlHandlerInstalled = false;
    _messageHandlerInstalled = false;
    _packetByteCount = 0;
    _lastdata = 0;
    _cmdGate = 0;

    // set defaults for configuration items
    
	z_finger=45;
	divisorx=divisory=1;
	ledge=1700;
	redge=5200;
	tedge=4200;
	bedge=1700;
	vscrolldivisor=30;
	hscrolldivisor=30;
	cscrolldivisor=0;
	ctrigger=0;
	centerx=3000;
	centery=3000;
	maxtaptime=100000000;
	maxdragtime=200000000;
	hsticky=0;
	vsticky=0;
	wsticky=0;
	tapstable=1;
	wlimit=9;
	wvdivisor=30;
	whdivisor=30;
	clicking=true;
	dragging=true;
	draglock=false;
    draglocktemp=0;
	hscroll=vscroll=false;
	scroll=true;
    outzone_wt = palm = palm_wt = false;
    zlimit = 100;
    noled = false;
    maxaftertyping = 250000000;
    mousemultiplierx = 20;
    mousemultipliery = 20;
    mousescrollmultiplierx = 20;
    mousescrollmultipliery = 20;
    mousemiddlescroll = true;
    wakedelay = 1500;
    skippassthru = false;
    tapthreshx = tapthreshy = 50;
    dblthreshx = dblthreshy = 100;
    zonel = 1700;  zoner = 5200;
    zonet = 99999; zoneb = 0;
    diszl = 0; diszr = 1700;
    diszt = 99999; diszb = 4200;
    diszctrl = 0;
    _resolution = 2300;
    _scrollresolution = 2300;
    swipedx = 500;
    swipedy = 500;
    rczl = 3800; rczt = 2000;
    rczr = 99999; rczb = 0;
    _buttonCount = 2;
    swapdoubletriple = false;
    draglocktempmask = 0x0100010; // default is Command key
    clickpadclicktime = 300000000; // 300ms default
    clickpadtrackboth = true;
    
    bogusdxthresh = 700;
    bogusdythresh = 500;
    
    scrolldxthresh = 0;
    scrolldythresh = 0;
    
    immediateclick = true;

    xupmm = yupmm = 50; // 50 is just arbitrary, but same
    
    _extendedwmode=false;
    
    // intialize state
    
	lastx=0;
	lasty=0;
    last_fingers =0;
	xrest=0;
	yrest=0;
    lastbuttons=0;
    
    // intialize state for secondary packets/extendedwmode
    xrest2=0;
    yrest2=0;
    clickedprimary=false;
    lastx2=0;
    lasty2=0;
    tracksecondary=false;
    
    // state for middle button
    _buttonTimer = 0;
    _mbuttonstate = STATE_NOBUTTONS;
    _pendingbuttons = 0;
    _buttontime = 0;
    _maxmiddleclicktime = 100000000;
    _fakemiddlebutton = true;
    
    ignoredeltas=0;
    ignoredeltasstart=0;
	scrollrest=0;
    touchtime=untouchtime=0;
	wastriple=wasdouble=false;
    keytime = 0;
    ignoreall = false;
    passbuttons = 0;
    passthru = false;
    ledpresent = false;
    clickpadtype = 0;
    _clickbuttons = 0;
    _reportsv = false;
    mousecount = 0;
    usb_mouse_stops_trackpad = true;
    _modifierdown = 0;
    scrollzoommask = 0;
    
    inSwipeLeft=inSwipeRight=inSwipeDown=inSwipeUp=0;
    xmoved=ymoved=0;
    
    momentumscroll = true;
    scrollTimer = 0;
    momentumscrolltimer = 10000000;
    momentumscrollthreshy = 7;
    momentumscrollmultiplier = 98;
    momentumscrolldivisor = 100;
    momentumscrollsamplesmin = 3;
    momentumscrollcurrent_y = 0;
    momentumscrollcurrent_x = 0;
    
    dragexitdelay = 100000000;
    dragTimer = 0;
    
	touchmode=MODE_NOTOUCH;
    
	IOLog("VoodooPS2TouchPad Version 1.9.0 loaded...\n");
    
	setProperty("Revision", 24, 32);
    
    //
    // Load settings specific to Platform Profile
    //
    
    setParamPropertiesGated(config);
    OSSafeRelease(config);
    
    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool VoodooPS2TouchPadBase::start( IOService * provider )
{
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
    // Advertise the current state of the tapping feature.
    //
    // Must add this property to let our superclass know that it should handle
    // trackpad acceleration settings from user space.  Without this, tracking
    // speed adjustments from the mouse prefs panel have no effect.
    //

    setProperty(kIOHIDPointerAccelerationTypeKey, kIOHIDTrackpadAccelerationType);
    setProperty(kIOHIDScrollAccelerationTypeKey, kIOHIDTrackpadScrollAccelerationKey);
	setProperty(kIOHIDScrollResolutionKey, _scrollresolution << 16, 32);
    
    //
    // Setup workloop with command gate for thread synchronization...
    //
    IOWorkLoop* pWorkLoop = getWorkLoop();
    _cmdGate = IOCommandGate::commandGate(this);
    if (!pWorkLoop || !_cmdGate)
    {
        _device->release();
        return false;
    }
    
    //
    // Setup button timer event source
    //
    if (_buttonCount >= 3)
    {
        _buttonTimer = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &VoodooPS2TouchPadBase::onButtonTimer));
        if (!_buttonTimer)
        {
            _device->release();
            return false;
        }
        pWorkLoop->addEventSource(_buttonTimer);
    }
    
    //
    // Setup dragTimer event source
    //
    if (dragexitdelay)
    {
        dragTimer = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &VoodooPS2TouchPadBase::onDragTimer));
        if (dragTimer)
            pWorkLoop->addEventSource(dragTimer);
    }
    
    pWorkLoop->addEventSource(_cmdGate);
    
    //
    // Setup scrolltimer event source
    //
    scrollTimer = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &VoodooPS2TouchPadBase::onScrollTimer));
    if (scrollTimer)
        pWorkLoop->addEventSource(scrollTimer);
    
    //
    // Lock the controller during initialization
    //
    
    _device->lock();


    //
    // Perform any implementation specific device initialization
    //
    if (!deviceSpecificInit()) {
        _device->unlock();
        _device->release();
        // TODO: any other cleanup?
        return false;
    }

    //
    // Install our driver's interrupt handler, for asynchronous data delivery.
    //
    
    DEBUG_LOG("touchpadbase : i will install interrupt");

    _device->installInterruptAction(this,
                                    OSMemberFunctionCast(PS2InterruptAction,this,&VoodooPS2TouchPadBase::interruptOccurred),
                                    OSMemberFunctionCast(PS2PacketAction, this, &VoodooPS2TouchPadBase::packetReady));
    _interruptHandlerInstalled = true;
    
        DEBUG_LOG("touchpadbase : call to afterInstall Interrupt");

    afterInstallInterrupt();

    // now safe to allow other threads
    _device->unlock();
    
    afterDeviceUnlock();
    //
	// Install our power control handler.
	//
    
	_device->installPowerControlAction( this,
        OSMemberFunctionCast(PS2PowerControlAction, this, &VoodooPS2TouchPadBase::setDevicePowerState) );
	_powerControlHandlerInstalled = true;
    
    //
    // Install message hook for keyboard to trackpad communication
    //
    
    _device->installMessageAction( this,
        OSMemberFunctionCast(PS2MessageAction, this, &VoodooPS2TouchPadBase::receiveMessage));
    _messageHandlerInstalled = true;

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void VoodooPS2TouchPadBase::stop( IOService * provider )
{
    DEBUG_LOG("%s: stop called\n", getName());
    
    //
    // The driver has been instructed to stop.  Note that we must break all
    // connections to other service objects now (ie. no registered actions,
    // no pointers and retains to objects, etc), if any.
    //

    assert(_device == provider);

    // free up timer for scroll momentum
    IOWorkLoop* pWorkLoop = getWorkLoop();
    if (pWorkLoop)
    {
        if (scrollTimer)
        {
            pWorkLoop->removeEventSource(scrollTimer);
            scrollTimer->release();
            scrollTimer = 0;
        }
        if (_buttonTimer)
        {
            pWorkLoop->removeEventSource(_buttonTimer);
            _buttonTimer->release();
            _buttonTimer = 0;
        }
        if (_cmdGate)
        {
            pWorkLoop->removeEventSource(_cmdGate);
            _cmdGate->release();
            _cmdGate = 0;
        }
    }
    
    //
    // Uninstall the interrupt handler.
    //

    if (_interruptHandlerInstalled)
    {
        _device->uninstallInterruptAction();
        _interruptHandlerInstalled = false;
    }

    //
    // Uninstall the power control handler.
    //

    if (_powerControlHandlerInstalled)
    {
        _device->uninstallPowerControlAction();
        _powerControlHandlerInstalled = false;
    }
    
    //
    // Uinstall message handler.
    //
    if (_messageHandlerInstalled)
    {
        _device->uninstallMessageAction();
        _messageHandlerInstalled = false;
    }

    //
    // Release the pointer to the provider object.
    //
    
    OSSafeReleaseNULL(_device);
    
	super::stop(provider);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void VoodooPS2TouchPadBase::onScrollTimer(void)
{
    //
    // This will be invoked by our workloop timer event source to implement
    // momentum scroll.
    //
    
    if (!momentumscrollcurrent_y && !momentumscrollcurrent_x)
        return;
    
    uint64_t now_abs;
	clock_get_uptime(&now_abs);
    
    int64_t dy64 = momentumscrollcurrent_y / (int64_t)momentumscrollinterval + momentumscrollrest2_y;
    int64_t dx64 = momentumscrollcurrent_x / (int64_t)momentumscrollinterval + momentumscrollrest2_x;
    int dx = (int) dx64;
    int dy = (int)dy64;
    if (abs(dy) > momentumscrollthreshy || abs(dx) > momentumscrollthreshy)
    {
        // dispatch the scroll event
        dispatchScrollWheelEventX(wvdivisor ? dy / wvdivisor : 0, wvdivisor ? dx / wvdivisor : 0, 0, now_abs);
        momentumscrollrest2_y = wvdivisor ? dy % wvdivisor : 0;
        momentumscrollrest2_x = wvdivisor ? dx % wvdivisor : 0;
    
        // adjust momentumscrollcurrent
        momentumscrollcurrent_y = momentumscrollcurrent_y * momentumscrollmultiplier + momentumscrollrest1_y;
        momentumscrollcurrent_x = momentumscrollcurrent_x * momentumscrollmultiplier + momentumscrollrest1_x;
        momentumscrollrest1_y = momentumscrollcurrent_y % momentumscrolldivisor;
        momentumscrollrest1_x = momentumscrollcurrent_x % momentumscrolldivisor;
        momentumscrollcurrent_y /= momentumscrolldivisor;
        momentumscrollcurrent_x /= momentumscrolldivisor;
        
        // start another timer
        setTimerTimeout(scrollTimer, momentumscrolltimer);
    }
    else
    {
        // no more scrolling...
        momentumscrollcurrent_y = 0;
        momentumscrollcurrent_x = 0;
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void VoodooPS2TouchPadBase::onButtonTimer(void)
{
	uint64_t now_abs;
	clock_get_uptime(&now_abs);
    
    middleButton(lastbuttons, now_abs, fromTimer);
}

UInt32 VoodooPS2TouchPadBase::middleButton(UInt32 buttons, uint64_t now_abs, MBComingFrom from)
{
    if (!_fakemiddlebutton || _buttonCount <= 2 || (ignoreall && fromTrackpad == from))
        return buttons;
    
    // cancel timer if we see input before timeout has fired, but after expired
    bool timeout = false;
    uint64_t now_ns;
    absolutetime_to_nanoseconds(now_abs, &now_ns);
    if (fromTimer == from || fromCancel == from || now_ns - _buttontime > _maxmiddleclicktime)
        timeout = true;

    //
    // A state machine to simulate middle buttons with two buttons pressed
    // together.
    //
    switch (_mbuttonstate)
    {
        // no buttons down, waiting for something to happen
        case STATE_NOBUTTONS:
            if (fromCancel != from)
            {
                if (buttons & 0x4)
                    _mbuttonstate = STATE_NOOP;
                else if (0x3 == buttons)
                    _mbuttonstate = STATE_MIDDLE;
                else if (0x0 != buttons)
                {
                    // only single button, so delay this for a bit
                    _pendingbuttons = buttons;
                    _buttontime = now_ns;
                    setTimerTimeout(_buttonTimer, _maxmiddleclicktime);
                    _mbuttonstate = STATE_WAIT4TWO;
                }
            }
            break;
            
        // waiting for second button to come down or timeout
        case STATE_WAIT4TWO:
            if (!timeout && 0x3 == buttons)
            {
                _pendingbuttons = 0;
                cancelTimer(_buttonTimer);
                _mbuttonstate = STATE_MIDDLE;
            }
            else if (timeout || buttons != _pendingbuttons)
            {
                if (fromTimer == from || !(buttons & _pendingbuttons))
                    dispatchRelativePointerEventX(0, 0, buttons|_pendingbuttons, now_abs);
                _pendingbuttons = 0;
                cancelTimer(_buttonTimer);
                if (0x0 == buttons)
                    _mbuttonstate = STATE_NOBUTTONS;
                else
                    _mbuttonstate = STATE_NOOP;
            }
            break;
            
        // both buttons down and delivering middle button
        case STATE_MIDDLE:
            if (0x0 == buttons)
                _mbuttonstate = STATE_NOBUTTONS;
            else if (0x3 != (buttons & 0x3))
            {
                // only single button, so delay to see if we get to none
                _pendingbuttons = buttons;
                _buttontime = now_ns;
                setTimerTimeout(_buttonTimer, _maxmiddleclicktime);
                _mbuttonstate = STATE_WAIT4NONE;
            }
            break;
            
        // was middle button, but one button now up, waiting for second to go up
        case STATE_WAIT4NONE:
            if (!timeout && 0x0 == buttons)
            {
                _pendingbuttons = 0;
                cancelTimer(_buttonTimer);
                _mbuttonstate = STATE_NOBUTTONS;
            }
            else if (timeout || buttons != _pendingbuttons)
            {
                if (fromTimer == from)
                    dispatchRelativePointerEventX(0, 0, buttons|_pendingbuttons, now_abs);
                _pendingbuttons = 0;
                cancelTimer(_buttonTimer);
                if (0x0 == buttons)
                    _mbuttonstate = STATE_NOBUTTONS;
                else
                    _mbuttonstate = STATE_NOOP;
            }
            break;
            
        case STATE_NOOP:
            if (0x0 == buttons)
                _mbuttonstate = STATE_NOBUTTONS;
            break;
    }
    
    // modify buttons after new state set
    switch (_mbuttonstate)
    {
        case STATE_MIDDLE:
            buttons = 0x4;
            break;
            
        case STATE_WAIT4NONE:
        case STATE_WAIT4TWO:
            buttons &= ~0x3;
            break;
            
        case STATE_NOBUTTONS:
        case STATE_NOOP:
            break;
    }
    
    // return modified buttons
    return buttons;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void VoodooPS2TouchPadBase::onDragTimer(void)
{
    if (MODE_DRAGNOTOUCH==touchmode)
    {
        touchmode=MODE_NOTOUCH;
        
        uint64_t now_abs;
        clock_get_uptime(&now_abs);
        UInt32 buttons = middleButton(lastbuttons & ~0x01, now_abs, fromPassthru);
        DEBUG_LOG("ps2: onDragTimer, button = %d\n", buttons);
        dispatchRelativePointerEventX(0, 0, buttons, now_abs);
    }
    else
    {
        //REVIEW: for debugging...
        IOLog("rehab: onDragTimer called with unexpected mode = %d\n", touchmode);
    }
    //TODO: cancel dragnotouch mode, revert to notouch
    //TODO: send lbutton up without modifying other buttons
    //TODO: find other places the timer should be cancelled.
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void VoodooPS2TouchPadBase::initTouchPad()
{
    //
    // Clear packet buffer pointer to avoid issues caused by
    // stale packet fragments.
    //
    
    _packetByteCount = 0;
    _ringBuffer.reset();
    
    // clear passbuttons, just in case buttons were down when system
    // went to sleep (now just assume they are up)
    passbuttons = 0;
    _clickbuttons = 0;
    tracksecondary=false;
    
    // clear state of control key cache
    _modifierdown = 0;
    
    // initialize the touchpad
    deviceSpecificInit();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void VoodooPS2TouchPadBase::setParamPropertiesGated(OSDictionary * config)
{
	if (NULL == config)
		return;
    
	const struct {const char *name; int *var;} int32vars[]={
		{"FingerZ",							&z_finger},
		{"DivisorX",						&divisorx},
		{"DivisorY",						&divisory},
		{"EdgeRight",						&redge},
		{"EdgeLeft",						&ledge},
		{"EdgeTop",							&tedge},
		{"EdgeBottom",						&bedge},
		{"VerticalScrollDivisor",			&vscrolldivisor},
		{"HorizontalScrollDivisor",			&hscrolldivisor},
		{"CircularScrollDivisor",			&cscrolldivisor},
		{"CenterX",							&centerx},
		{"CenterY",							&centery},
		{"CircularScrollTrigger",			&ctrigger},
		{"MultiFingerWLimit",				&wlimit},
		{"MultiFingerVerticalDivisor",		&wvdivisor},
		{"MultiFingerHorizontalDivisor",	&whdivisor},
        {"ZLimit",                          &zlimit},
        {"MouseMultiplierX",                &mousemultiplierx},
        {"MouseMultiplierY",                &mousemultipliery},
        {"MouseScrollMultiplierX",          &mousescrollmultiplierx},
        {"MouseScrollMultiplierY",          &mousescrollmultipliery},
        {"WakeDelay",                       &wakedelay},
        {"TapThresholdX",                   &tapthreshx},
        {"TapThresholdY",                   &tapthreshy},
        {"DoubleTapThresholdX",             &dblthreshx},
        {"DoubleTapThresholdY",             &dblthreshy},
        {"ZoneLeft",                        &zonel},
        {"ZoneRight",                       &zoner},
        {"ZoneTop",                         &zonet},
        {"ZoneBottom",                      &zoneb},
        {"DisableZoneLeft",                 &diszl},
        {"DisableZoneRight",                &diszr},
        {"DisableZoneTop",                  &diszt},
        {"DisableZoneBottom",               &diszb},
        {"DisableZoneControl",              &diszctrl},
        {"Resolution",                      &_resolution},
        {"ScrollResolution",                &_scrollresolution},
        {"SwipeDeltaX",                     &swipedx},
        {"SwipeDeltaY",                     &swipedy},
        {"MouseCount",                      &mousecount},
        {"RightClickZoneLeft",              &rczl},
        {"RightClickZoneRight",             &rczr},
        {"RightClickZoneTop",               &rczt},
        {"RightClickZoneBottom",            &rczb},
        {"HIDScrollZoomModifierMask",       &scrollzoommask},
        {"ButtonCount",                     &_buttonCount},
        {"DragLockTempMask",                &draglocktempmask},
        {"MomentumScrollThreshY",           &momentumscrollthreshy},
        {"MomentumScrollMultiplier",        &momentumscrollmultiplier},
        {"MomentumScrollDivisor",           &momentumscrolldivisor},
        {"MomentumScrollSamplesMin",        &momentumscrollsamplesmin},
        {"FingerChangeIgnoreDeltas",        &ignoredeltasstart},
        {"BogusDeltaThreshX",               &bogusdxthresh},
        {"BogusDeltaThreshY",               &bogusdythresh},
        {"UnitsPerMMX",                     &xupmm},
        {"UnitsPerMMY",                     &yupmm},
        {"ScrollDeltaThreshX",              &scrolldxthresh},
        {"ScrollDeltaThreshY",              &scrolldythresh},
	};
	const struct {const char *name; int *var;} boolvars[]={
		{"StickyHorizontalScrolling",		&hsticky},
		{"StickyVerticalScrolling",			&vsticky},
		{"StickyMultiFingerScrolling",		&wsticky},
		{"StabilizeTapping",				&tapstable},
        {"DisableLEDUpdate",                &noled},
        {"SmoothInput",                     &smoothinput},
        {"UnsmoothInput",                   &unsmoothinput},
        {"SkipPassThrough",                 &skippassthru},
        {"SwapDoubleTriple",                &swapdoubletriple},
        {"ClickPadTrackBoth",               &clickpadtrackboth},
        {"ImmediateClick",                  &immediateclick},
        {"MouseMiddleScroll",               &mousemiddlescroll},
        {"FakeMiddleButton",                &_fakemiddlebutton},
	};
    const struct {const char* name; bool* var;} lowbitvars[]={
        {"TrackpadRightClick",              &rtap},
        {"Clicking",                        &clicking},
        {"Dragging",                        &dragging},
        {"DragLock",                        &draglock},
        {"TrackpadHorizScroll",             &hscroll},
        {"TrackpadVertScroll",              &vscroll},
        {"TrackpadScroll",                  &scroll},
        {"OutsidezoneNoAction When Typing", &outzone_wt},
        {"PalmNoAction Permanent",          &palm},
        {"PalmNoAction When Typing",        &palm_wt},
        {"USBMouseStopsTrackpad",           &usb_mouse_stops_trackpad},
        {"TrackpadMomentumScroll",          &momentumscroll},
    };
    const struct {const char* name; uint64_t* var; } int64vars[]={
        {"MaxDragTime",                     &maxdragtime},
        {"",                      &maxtaptime},
        {"HIDClickTime",                    &maxdbltaptime},
        {"QuietTimeAfterTyping",            &maxaftertyping},
        {"MomentumScrollTimer",             &momentumscrolltimer},
        {"ClickPadClickTime",               &clickpadclicktime},
        {"MiddleClickTime",                 &_maxmiddleclicktime},
        {"DragExitDelayTime",               &dragexitdelay},
    };
    
    int oldmousecount = mousecount;
    bool old_usb_mouse_stops_trackpad = usb_mouse_stops_trackpad;

    OSBoolean *bl;
    OSNumber *num;
    // 64-bit config items
    for (int i = 0; i < countof(int64vars); i++) {
        if ((num=OSDynamicCast(OSNumber, config->getObject(int64vars[i].name))))
        {
            *int64vars[i].var = num->unsigned64BitValue();
            ////DEBUG_LOG("%s::setProperty64(%s, %llu)\n", getName(), int64vars[i].name, *int64vars[i].var);
            setProperty(int64vars[i].name, *int64vars[i].var, 64);
        }
    }
    // boolean config items
	for (int i = 0; i < countof(boolvars); i++) {
		if ((bl=OSDynamicCast (OSBoolean,config->getObject (boolvars[i].name))))
        {
			*boolvars[i].var = bl->isTrue();
            ////DEBUG_LOG("%s::setPropertyBool(%s, %d)\n", getName(), boolvars[i].name, *boolvars[i].var);
            setProperty(boolvars[i].name, *boolvars[i].var ? kOSBooleanTrue : kOSBooleanFalse);
        }
    }
    // 32-bit config items
	for (int i = 0; i < countof(int32vars);i++) {
		if ((num=OSDynamicCast (OSNumber,config->getObject (int32vars[i].name))))
        {
			*int32vars[i].var = num->unsigned32BitValue();
            ////DEBUG_LOG("%s::setProperty32(%s, %d)\n", getName(), int32vars[i].name, *int32vars[i].var);
            setProperty(int32vars[i].name, *int32vars[i].var, 32);
        }
    }
    // lowbit config items
	for (int i = 0; i < countof(lowbitvars); i++) {
		if ((num=OSDynamicCast (OSNumber,config->getObject(lowbitvars[i].name))))
        {
			*lowbitvars[i].var = (num->unsigned32BitValue()&0x1)?true:false;
            ////DEBUG_LOG("%s::setPropertyLowBit(%s, %d)\n", getName(), lowbitvars[i].name, *lowbitvars[i].var);
            setProperty(lowbitvars[i].name, *lowbitvars[i].var ? 1 : 0, 32);
        }
    }
    
    // special case for MaxDragTime (which is really max time for a double-click)
    // we can let it go no more than 230ms because otherwise taps on
    // the menu bar take too long if drag mode is enabled.  The code in that case
    // has to "hold button 1 down" for the duration of maxdragtime because if
    // it didn't then dragging on the caption of a window will not work
    // (some other apps too) because these apps will see a double tap+hold as
    // a single click, then double click and they don't go into drag mode when
    // initiated with a double click.
    //
    // same thing going on with the forward/back buttons in Finder, except the
    // timeout OS X is using is different (shorter)
    //
    // this all happens during MODE_PREDRAG
    //
    // summary:
    //  if the code releases button 1 after a tap, then dragging windows
    //    breaks
    //  if the maxdragtime is too large (200ms is small enough, 500ms is too large)
    //    then clicking on menus breaks because the system sees it as a long
    //    press and hold
    //
    // fyi:
    //  also tried to allow release of button 1 during MODE_PREDRAG, and then when
    //   attempting to initiate the drag (in the case the second touch comes soon
    //   enough), modifying the time such that it is not seen as a double tap.
    //  unfortunately, that destroys double tap as well, probably because the
    //   system is confused seeing input "out of order"
    
    //if (maxdragtime > 230000000)
    //    maxdragtime = 230000000;
    
    // DivisorX and DivisorY cannot be zero, but don't crash if they are...
    if (!divisorx)
        divisorx = 1;
    if (!divisory)
        divisory = 1;

//REVIEW: this should be done maybe only when necessary...
    touchmode=MODE_NOTOUCH;

    // check for special terminating sequence from PS2Daemon
    if (-1 == mousecount)
    {
        DEBUG_LOG("Shutdown touchpad, mousecount=%d\n", mousecount);
        touchpadShutdown();
        mousecount = oldmousecount;
    }

    // disable trackpad when USB mouse is plugged in
    // check for mouse count changing...
    if ((oldmousecount != 0) != (mousecount != 0) || old_usb_mouse_stops_trackpad != usb_mouse_stops_trackpad)
    {
        // either last mouse removed or first mouse added
        ignoreall = (mousecount != 0) && usb_mouse_stops_trackpad;
        touchpadToggled();
    }
}

IOReturn VoodooPS2TouchPadBase::setParamProperties(OSDictionary* dict)
{
    ////IOReturn result = super::IOHIDevice::setParamProperties(dict);
    if (_cmdGate)
    {
        // syncronize through workloop...
        ////_cmdGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &VooodooPS2TouchPadBase::setParamPropertiesGated), dict);
        setParamPropertiesGated(dict);
    }
    
    return super::setParamProperties(dict);
    ////return result;
}

IOReturn VoodooPS2TouchPadBase::setProperties(OSObject *props)
{
	OSDictionary *dict = OSDynamicCast(OSDictionary, props);
    if (dict && _cmdGate)
    {
        // syncronize through workloop...
        _cmdGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &VoodooPS2TouchPadBase::setParamPropertiesGated), dict);
    }
    
	return super::setProperties(props);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void VoodooPS2TouchPadBase::setDevicePowerState( UInt32 whatToDo )
{
    switch ( whatToDo )
    {
        case kPS2C_DisableDevice:
            //
            // Disable touchpad (synchronous).
            //

            setTouchPadEnable( false );
            break;

        case kPS2C_EnableDevice:
            //
            // Must not issue any commands before the device has
            // completed its power-on self-test and calibration.
            //

            IOSleep(wakedelay);
            IOSleep(wakedelay);            

            _device->lock();


              //
              // Perform any implementation specific device initialization
              //
              if (!deviceSpecificInit()) {
                  _device->unlock();
                  _device->release();
                  return ;
              }

              //
              // Install our driver's interrupt handler, for asynchronous data delivery.
              //

              DEBUG_LOG("touchpadbase : i will install interrupt");

              _device->installInterruptAction(this,
                                              OSMemberFunctionCast(PS2InterruptAction,this,&VoodooPS2TouchPadBase::interruptOccurred),
                                              OSMemberFunctionCast(PS2PacketAction, this, &VoodooPS2TouchPadBase::packetReady));
              _interruptHandlerInstalled = true;

              DEBUG_LOG("touchpadbase : call to afterInstall Interrupt");

              afterInstallInterrupt();

              // now safe to allow other threads
              _device->unlock();

              afterDeviceUnlock();



            break;
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void VoodooPS2TouchPadBase::receiveMessage(int message, void* data)
{
    //
    // Here is where we receive messages from the keyboard driver
    //
    // This allows for the keyboard driver to enable/disable the trackpad
    // when a certain keycode is pressed.
    //
    // It also allows the trackpad driver to learn the last time a key
    //  has been pressed, so it can implement various "ignore trackpad
    //  input while typing" options.
    //
    switch (message)
    {
        case kPS2M_getDisableTouchpad:
        {
            bool* pResult = (bool*)data;
            *pResult = !ignoreall;
            break;
        }
            
        case kPS2M_setDisableTouchpad:
        {
            bool enable = *((bool*)data);
            // ignoreall is true when trackpad has been disabled
            if (enable == ignoreall)
            {
                // save state, and update LED
                ignoreall = !enable;
                touchpadToggled();
            }
            break;
        }
            
        case kPS2M_notifyKeyPressed:
        {
            // just remember last time key pressed... this can be used in
            // interrupt handler to detect unintended input while typing
            PS2KeyInfo* pInfo = (PS2KeyInfo*)data;
            static const int masks[] =
            {
                0x10,       // 0x36
                0x100000,   // 0x37
                0,          // 0x38
                0,          // 0x39
                0x080000,   // 0x3a
                0x040000,   // 0x3b
                0,          // 0x3c
                0x08,       // 0x3d
                0x04,       // 0x3e
                0x200000,   // 0x3f
            };
#ifdef SIMULATE_PASSTHRU
            static int buttons = 0;
            int button;
            switch (pInfo->adbKeyCode)
            {
                // make right Alt,Menu,Ctrl into three button passthru
                case 0x36:
                    button = 0x1;
                    goto dispatch_it;
                case 0x3f:
                    button = 0x4;
                    goto dispatch_it;
                case 0x3e:
                    button = 0x2;
                    // fall through...
                dispatch_it:
                    if (pInfo->goingDown)
                        buttons |= button;
                    else
                        buttons &= ~button;
                    UInt8 packet[6];
                    packet[0] = 0x84 | trackbuttons;
                    packet[1] = 0x08 | buttons;
                    packet[2] = 0;
                    packet[3] = 0xC4 | trackbuttons;
                    packet[4] = 0;
                    packet[5] = 0;
                    dispatchEventsWithPacket(packet, 6);
                    pInfo->eatKey = true;
            }
#endif
            switch (pInfo->adbKeyCode)
            {
                // don't store key time for modifier keys going down
                // track modifiers for scrollzoom feature...
                // (note: it turns out we didn't need to do this, but leaving this code in for now in case it is useful)
                case 0x38:  // left shift
                case 0x3c:  // right shift
                case 0x3b:  // left control
                case 0x3e:  // right control
                case 0x3a:  // left windows (option)
                case 0x3d:  // right windows
                case 0x37:  // left alt (command)
                case 0x36:  // right alt
                case 0x3f:  // osx fn (function)
                    if (pInfo->goingDown)
                    {
                        _modifierdown |= masks[pInfo->adbKeyCode-0x36];
                        break;
                    }
                    _modifierdown &= ~masks[pInfo->adbKeyCode-0x36];
                    keytime = pInfo->time;
                    break;
                    
                default:
                    momentumscrollcurrent_y = 0;  // keys cancel momentum scroll
                    momentumscrollcurrent_x = 0; 
                    keytime = pInfo->time;
            }
            break;
        }
    }
}
