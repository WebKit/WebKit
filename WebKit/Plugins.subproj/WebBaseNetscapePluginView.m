/*	
        WebBaseNetscapePluginView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBaseNetscapePluginView.h>

#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDefaultUIDelegate.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebFramePrivate.h> 
#import <WebKit/WebFrameView.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNetscapePluginStream.h>
#import <WebKit/WebNullPluginView.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebUIDelegate.h>

#import <WebKit/WebAssertions.h>
#import <Foundation/NSURLRequestPrivate.h>

#import <Foundation/NSData_NSURLExtras.h>
#import <Foundation/NSString_NSURLExtras.h>
#import <Foundation/NSURL_NSURLExtras.h>

#import <AppKit/NSEvent_Private.h>
#import <Carbon/Carbon.h>
#import <QD/QuickdrawPriv.h>

// This is not yet in QuickdrawPriv.h, although it's supposed to be.
void CallDrawingNotifications(CGrafPtr port, Rect *mayDrawIntoThisRect, int drawingType);

// Send null events 50 times a second when active, so plug-ins like Flash get high frame rates.
#define NullEventIntervalActive 	0.02
#define NullEventIntervalNotActive	0.25

static WebBaseNetscapePluginView *currentPluginView = nil;

typedef struct {
    GrafPtr oldPort;
    Point oldOrigin;
    RgnHandle oldClipRegion;
    RgnHandle oldVisibleRegion;
    RgnHandle clipRegion;
    BOOL forUpdate;
} PortState;

@interface WebPluginRequest : NSObject
{
    NSURLRequest *_request;
    NSString *_frameName;
    void *_notifyData;
}

- (id)initWithRequest:(NSURLRequest *)request frameName:(NSString *)frameName notifyData:(void *)notifyData;

- (NSURLRequest *)request;
- (NSString *)frameName;
- (void *)notifyData;

@end

@interface NSData (WebPluginDataExtras)
- (BOOL)_web_startsWithBlankLine;
- (unsigned)_web_locationAfterFirstBlankLine;
@end


@implementation WebBaseNetscapePluginView

#pragma mark EVENTS

+ (void)getCarbonEvent:(EventRecord *)carbonEvent
{
    carbonEvent->what = nullEvent;
    carbonEvent->message = 0;
    carbonEvent->when = TickCount();
    GetGlobalMouse(&carbonEvent->where);
    carbonEvent->modifiers = GetCurrentKeyModifiers();
    if (!Button())
        carbonEvent->modifiers |= btnState;
}

- (void)getCarbonEvent:(EventRecord *)carbonEvent
{
    [[self class] getCarbonEvent:carbonEvent];
}

- (EventModifiers)modifiersForEvent:(NSEvent *)event
{
    EventModifiers modifiers;
    unsigned int modifierFlags = [event modifierFlags];
    NSEventType eventType = [event type];
    
    modifiers = 0;
    
    if (eventType != NSLeftMouseDown && eventType != NSRightMouseDown)
        modifiers |= btnState;
    
    if (modifierFlags & NSCommandKeyMask)
        modifiers |= cmdKey;
    
    if (modifierFlags & NSShiftKeyMask)
        modifiers |= shiftKey;

    if (modifierFlags & NSAlphaShiftKeyMask)
        modifiers |= alphaLock;

    if (modifierFlags & NSAlternateKeyMask)
        modifiers |= optionKey;

    if (modifierFlags & NSControlKeyMask || eventType == NSRightMouseDown)
        modifiers |= controlKey;
    
    return modifiers;
}

- (void)getCarbonEvent:(EventRecord *)carbonEvent withEvent:(NSEvent *)cocoaEvent
{
    if ([cocoaEvent _eventRef] && ConvertEventRefToEventRecord([cocoaEvent _eventRef], carbonEvent)) {
        return;
    }
    
    NSPoint where = [[cocoaEvent window] convertBaseToScreen:[cocoaEvent locationInWindow]];
        
    carbonEvent->what = nullEvent;
    carbonEvent->message = 0;
    carbonEvent->when = (UInt32)([cocoaEvent timestamp] * 60); // seconds to ticks
    carbonEvent->where.h = (short)where.x;
    carbonEvent->where.v = (short)(NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - where.y);
    carbonEvent->modifiers = [self modifiersForEvent:cocoaEvent];
}

- (UInt32)keyMessageForEvent:(NSEvent *)event
{
    NSData *data = [[event characters] dataUsingEncoding:CFStringConvertEncodingToNSStringEncoding(CFStringGetSystemEncoding())];
    if (!data) {
        return 0;
    }
    UInt8 characterCode;
    [data getBytes:&characterCode length:1];
    UInt16 keyCode = [event keyCode];
    return keyCode << 8 | characterCode;
}

- (PortState)saveAndSetPortStateForUpdate:(BOOL)forUpdate
{
    ASSERT([self currentWindow]);
    
    WindowRef windowRef = [[self currentWindow] windowRef];
    CGrafPtr port = GetWindowPort(windowRef);

    Rect portBounds;
    GetPortBounds(port, &portBounds);

    // Use AppKit to convert view coordinates to NSWindow coordinates.

    NSRect boundsInWindow = [self convertRect:[self bounds] toView:nil];
    NSRect visibleRectInWindow = [self convertRect:[self visibleRect] toView:nil];
    
    // Flip Y to convert NSWindow coordinates to top-left-based window coordinates.

    float borderViewHeight = [[self currentWindow] frame].size.height;
    boundsInWindow.origin.y = borderViewHeight - NSMaxY(boundsInWindow);
    visibleRectInWindow.origin.y = borderViewHeight - NSMaxY(visibleRectInWindow);
    
    // Look at the Carbon port to convert top-left-based window coordinates into top-left-based content coordinates.

    PixMap *pix = *GetPortPixMap(port);
    boundsInWindow.origin.x += pix->bounds.left - portBounds.left;
    boundsInWindow.origin.y += pix->bounds.top - portBounds.top;
    visibleRectInWindow.origin.x += pix->bounds.left - portBounds.left;
    visibleRectInWindow.origin.y += pix->bounds.top - portBounds.top;
    
    // Set up NS_Port.
    
    nPort.port = port;
    nPort.portx = (int32)-boundsInWindow.origin.x;
    nPort.porty = (int32)-boundsInWindow.origin.y;
    
    // Set up NPWindow.
    
    window.window = &nPort;
    
    window.x = (int32)boundsInWindow.origin.x; 
    window.y = (int32)boundsInWindow.origin.y;
    window.width = NSWidth(boundsInWindow) > 0 ? NSWidth(boundsInWindow) : specifiedWidth;
    window.height = NSHeight(boundsInWindow) > 0 ? NSHeight(boundsInWindow) : specifiedHeight;

    window.clipRect.top = (uint16)visibleRectInWindow.origin.y;
    window.clipRect.left = (uint16)visibleRectInWindow.origin.x;

    // Clip out the plug-in when it's not really in a window.
    if ([self window]) {
        window.clipRect.bottom = (uint16)(visibleRectInWindow.origin.y + visibleRectInWindow.size.height);
        window.clipRect.right = (uint16)(visibleRectInWindow.origin.x + visibleRectInWindow.size.width);
    } else {
        window.clipRect.bottom = window.clipRect.top;
        window.clipRect.left = window.clipRect.right;
    }
    
    window.type = NPWindowTypeWindow;
    
    // Save the port state.

    PortState portState;
    
    GetPort(&portState.oldPort);    

    portState.oldOrigin.h = portBounds.left;
    portState.oldOrigin.v = portBounds.top;

    portState.oldClipRegion = NewRgn();
    GetPortClipRegion(port, portState.oldClipRegion);
    
    portState.oldVisibleRegion = NewRgn();
    GetPortVisibleRegion(port, portState.oldVisibleRegion);
    
    RgnHandle clipRegion = NewRgn();
    portState.clipRegion = clipRegion;
    
    MacSetRectRgn(clipRegion,
        window.clipRect.left + nPort.portx, window.clipRect.top + nPort.porty,
        window.clipRect.right + nPort.portx, window.clipRect.bottom + nPort.porty);
    
    portState.forUpdate = forUpdate;
    
    // Switch to the port and set it up.

    SetPort(port);

    PenNormal();
    ForeColor(blackColor);
    BackColor(whiteColor);
    
    SetOrigin(nPort.portx, nPort.porty);

    SetPortClipRegion(nPort.port, clipRegion);

    if (forUpdate) {
        // AppKit may have tried to help us by doing a BeginUpdate.
        // But the invalid region at that level didn't include AppKit's notion of what was not valid.
        // We reset the port's visible region to counteract what BeginUpdate did.
        SetPortVisibleRegion(nPort.port, clipRegion);

        // Some plugins do their own BeginUpdate/EndUpdate.
        // For those, we must make sure that the update region contains the area we want to draw.
        InvalWindowRgn(windowRef, clipRegion);
    }
    
    return portState;
}

- (PortState)saveAndSetPortState
{
    return [self saveAndSetPortStateForUpdate:NO];
}

- (void)restorePortState:(PortState)portState
{
    ASSERT([self currentWindow]);
    
    WindowRef windowRef = [[self currentWindow] windowRef];
    CGrafPtr port = GetWindowPort(windowRef);

    if (portState.forUpdate) {
        ValidWindowRgn(windowRef, portState.clipRegion);
    }
    
    SetOrigin(portState.oldOrigin.h, portState.oldOrigin.v);

    SetPortClipRegion(port, portState.oldClipRegion);
    if (portState.forUpdate) {
        SetPortVisibleRegion(port, portState.oldVisibleRegion);
    }

    DisposeRgn(portState.oldClipRegion);
    DisposeRgn(portState.oldVisibleRegion);
    DisposeRgn(portState.clipRegion);

    SetPort(portState.oldPort);
}

- (BOOL)sendEvent:(EventRecord *)event
{
    ASSERT([self window]);

    if (!isStarted) {
        return NO;
    }

    ASSERT(NPP_HandleEvent);
    
    // Make sure we don't call NPP_HandleEvent while we're inside NPP_SetWindow.
    // We probably don't want more general reentrancy protection; we are really
    // protecting only against this one case, which actually comes up when
    // you first install the SVG viewer plug-in.
    if (inSetWindow) {
        return NO;
    }

    BOOL defers = [[self webView] defersCallbacks];
    if (!defers) {
        [[self webView] setDefersCallbacks:YES];
    }

    PortState portState = [self saveAndSetPortStateForUpdate:event->what == updateEvt];

#ifndef NDEBUG
    // Draw green to help debug.
    // If we see any green we know something's wrong.
    if (event->what == updateEvt) {
        ForeColor(greenColor);
        const Rect bigRect = { -10000, -10000, 10000, 10000 };
        PaintRect(&bigRect);
        ForeColor(blackColor);
    }
#endif

    BOOL acceptedEvent = NPP_HandleEvent(instance, event);

    [self restorePortState:portState];

    if (!defers) {
        [[self webView] setDefersCallbacks:NO];
    }
    
    return acceptedEvent;
}

- (void)sendActivateEvent:(BOOL)activate
{
    EventRecord event;
    
    [self getCarbonEvent:&event];
    event.what = activateEvt;
    WindowRef windowRef = [[self window] windowRef];
    event.message = (UInt32)windowRef;
    if (activate) {
        event.modifiers |= activeFlag;
    }
    
    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(Plugins, "NPP_HandleEvent(activateEvent): %d  isActive: %d", acceptedEvent, activate);
}

- (BOOL)sendUpdateEvent
{
    EventRecord event;
    
    [self getCarbonEvent:&event];
    event.what = updateEvt;
    WindowRef windowRef = [[self window] windowRef];
    event.message = (UInt32)windowRef;

    BOOL acceptedEvent = [self sendEvent:&event];

    LOG(Plugins, "NPP_HandleEvent(updateEvt): %d", acceptedEvent);

    return acceptedEvent;
}

-(void)sendNullEvent
{
    EventRecord event;

    [self getCarbonEvent:&event];

    // Plug-in should not react to cursor position when not active or when a menu is down.
    MenuTrackingData trackingData;
    OSStatus error = GetMenuTrackingData(NULL, &trackingData);

    // Plug-in should not react to cursor position when the actual window is not key.
    if (![[self window] isKeyWindow] || (error == noErr && trackingData.menu)) {
        // FIXME: Does passing a v and h of -1 really prevent it from reacting to the cursor position?
        event.where.v = -1;
        event.where.h = -1;
    }

    [self sendEvent:&event];
}

- (void)stopNullEvents
{
    [nullEventTimer invalidate];
    [nullEventTimer release];
    nullEventTimer = nil;
}

- (void)restartNullEvents
{
    if (nullEventTimer) {
        [self stopNullEvents];
    }

    NSTimeInterval interval;

    // Send null events less frequently when the actual window is not key.
    if ([[self window] isKeyWindow]) {
        interval = NullEventIntervalActive;
    } else {
        interval = NullEventIntervalNotActive;
    }
    
    nullEventTimer = [[NSTimer scheduledTimerWithTimeInterval:interval
                                                       target:self
                                                     selector:@selector(sendNullEvent)
                                                     userInfo:nil
                                                      repeats:YES] retain];
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    EventRecord event;
    
    [self getCarbonEvent:&event];
    event.what = getFocusEvent;
    
    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(Plugins, "NPP_HandleEvent(getFocusEvent): %d", acceptedEvent);
    return YES;
}

- (BOOL)resignFirstResponder
{
    EventRecord event;
    
    [self getCarbonEvent:&event];
    event.what = loseFocusEvent;
    
    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(Plugins, "NPP_HandleEvent(loseFocusEvent): %d", acceptedEvent);
    return YES;
}

// AppKit doesn't call mouseDown or mouseUp on right-click. Simulate control-click
// mouseDown and mouseUp so plug-ins get the right-click event as they do in Carbon (3125743).
- (void)rightMouseDown:(NSEvent *)theEvent
{
    [self mouseDown:theEvent];
}

- (void)rightMouseUp:(NSEvent *)theEvent
{
    [self mouseUp:theEvent];
}

- (void)mouseDown:(NSEvent *)theEvent
{
    EventRecord event;

    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = mouseDown;

    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(Plugins, "NPP_HandleEvent(mouseDown): %d pt.v=%d, pt.h=%d", acceptedEvent, event.where.v, event.where.h);
}

- (void)mouseUp:(NSEvent *)theEvent
{
    EventRecord event;
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = mouseUp;

    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(Plugins, "NPP_HandleEvent(mouseUp): %d pt.v=%d, pt.h=%d", acceptedEvent, event.where.v, event.where.h);
}

- (void)mouseEntered:(NSEvent *)theEvent
{
    EventRecord event;
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = adjustCursorEvent;

    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(Plugins, "NPP_HandleEvent(mouseEntered): %d", acceptedEvent);
}

- (void)mouseExited:(NSEvent *)theEvent
{
    EventRecord event;
        
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = adjustCursorEvent;

    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(Plugins, "NPP_HandleEvent(mouseExited): %d", acceptedEvent);
    
    // Set cursor back to arrow cursor.
    [[NSCursor arrowCursor] set];
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    // Do nothing so that other responders don't respond to the drag that initiated in this view.
}

- (void)keyUp:(NSEvent *)theEvent
{
    EventRecord event;

    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = keyUp;

    if (event.message == 0) {
        event.message = [self keyMessageForEvent:theEvent];
    }
    
    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event];

    LOG(Plugins, "NPP_HandleEvent(keyUp): %d charCode:%c keyCode:%lu",
        acceptedEvent, (char) (event.message & charCodeMask), (event.message & keyCodeMask));
    
    // We originally thought that if the plug-in didn't accept this event,
    // we should pass it along so that keyboard scrolling, for example, will work.
    // In practice, this is not a good idea, because browsers tend to eat the event but return false.
    // MacIE handles each key event twice because of this, but we will emulate the other browsers instead.
}

- (void)keyDown:(NSEvent *)theEvent
{
    EventRecord event;

#if 0
    // Some command keys are sent with both performKeyEquivalent and keyDown.
    // We should send only 1 keyDown to the plug-in, so we'll ignore this one.
    if ([theEvent modifierFlags] & NSCommandKeyMask) {
        return;
    }
#endif
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = keyDown;

    if (event.message == 0) {
        event.message = [self keyMessageForEvent:theEvent];
    }
    
    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event];

    LOG(Plugins, "NPP_HandleEvent(keyDown): %d charCode:%c keyCode:%lu",
        acceptedEvent, (char) (event.message & charCodeMask), (event.message & keyCodeMask));
    
    // We originally thought that if the plug-in didn't accept this event,
    // we should pass it along so that keyboard scrolling, for example, will work.
    // In practice, this is not a good idea, because browsers tend to eat the event but return false.
    // MacIE handles each key event twice because of this, but we will emulate the other browsers instead.
}

- (BOOL)isInResponderChain
{
    NSResponder *responder = [[self window] firstResponder];

    while (responder != nil) {
        if (responder == self) {
            return YES;
        }
        responder = [responder nextResponder];
    }
    return NO;
}

// Stop overriding performKeyEquivalent because the gain is not worth the frustation.
// Need to find a better way to pass command-modified keys to plug-ins. 3080103
#if 0
// Must subclass performKeyEquivalent: for command-modified keys to work.
- (BOOL)performKeyEquivalent:(NSEvent *)theEvent
{
    EventRecord event;

    if (![self isInResponderChain]) {
        return NO;
    }
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = keyDown;

    if (event.message == 0) {
        event.message = [self keyMessageForEvent:theEvent];
    }

    BOOL acceptedEvent = [self sendEvent:&event];

    LOG(Plugins, "NPP_HandleEvent(performKeyEquivalent): %d charCode:%c keyCode:%lu",
        acceptedEvent, (char) (event.message & charCodeMask), (event.message & keyCodeMask));
    
    return acceptedEvent;
}
#endif

#pragma mark WEB_NETSCAPE_PLUGIN

- (BOOL)isNewWindowEqualToOldWindow
{
    if (window.x != lastSetWindow.x) {
        return NO;
    }
    if (window.y != lastSetWindow.y) {
        return NO;
    }
    if (window.width != lastSetWindow.width) {
        return NO;
    }
    if (window.height != lastSetWindow.height) {
        return NO;
    }
    if (window.clipRect.top != lastSetWindow.clipRect.top) {
        return NO;
    }
    if (window.clipRect.left != lastSetWindow.clipRect.left) {
        return NO;
    }
    if (window.clipRect.bottom  != lastSetWindow.clipRect.bottom ) {
        return NO;
    }
    if (window.clipRect.right != lastSetWindow.clipRect.right) {
        return NO;
    }
    if (window.type != lastSetWindow.type) {
        return NO;
    }
    if (nPort.portx != lastSetPort.portx) {
        return NO;
    }
    if (nPort.porty != lastSetPort.porty) {
        return NO;
    }
    if (nPort.port != lastSetPort.port) {
        return NO;
    }
    
    return YES;
}

- (void)setWindow
{
    if (!isStarted) {
        return;
    }
    
    PortState portState = [self saveAndSetPortState];

    if (![self isNewWindowEqualToOldWindow]) {        
        // Make sure we don't call NPP_HandleEvent while we're inside NPP_SetWindow.
        // We probably don't want more general reentrancy protection; we are really
        // protecting only against this one case, which actually comes up when
        // you first install the SVG viewer plug-in.
        NPError npErr;
        ASSERT(!inSetWindow);
        
        inSetWindow = YES;
        npErr = NPP_SetWindow(instance, &window);
        inSetWindow = NO;

        LOG(Plugins, "NPP_SetWindow: %d, port=0x%08x, window.x:%d window.y:%d",
            npErr, (int)nPort.port, (int)window.x, (int)window.y);

        lastSetWindow = window;
        lastSetPort = nPort;
    }

    [self restorePortState:portState];
}

- (void)removeTrackingRect
{
    if (trackingTag) {
        [self removeTrackingRect:trackingTag];
        trackingTag = 0;

        // Must release the window to balance the retain in resetTrackingRect.
        // But must do it after setting trackingTag to 0 so we don't re-enter.
        [[self window] release];
    }
}

- (void)resetTrackingRect
{
    [self removeTrackingRect];
    if (isStarted) {
        // Must retain the window so that removeTrackingRect can work after the window is closed.
        [[self window] retain];
        trackingTag = [self addTrackingRect:[self bounds] owner:self userData:nil assumeInside:NO];
    }
}

+ (void)setCurrentPluginView:(WebBaseNetscapePluginView *)view
{
    currentPluginView = view;
}

+ (WebBaseNetscapePluginView *)currentPluginView
{
    return currentPluginView;
}

- (BOOL)canStart
{
    return YES;
}

- (void)didStart
{
    // Do nothing. Overridden by subclasses.
}

- (void)addWindowObservers
{
    ASSERT([self window]);

    NSWindow *theWindow = [self window];
    
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    NSView *view;
    for (view = self; view; view = [view superview]) {
        [notificationCenter addObserver:self selector:@selector(viewHasMoved:)
                                   name:NSViewFrameDidChangeNotification object:view];
        [notificationCenter addObserver:self selector:@selector(viewHasMoved:)
                                   name:NSViewBoundsDidChangeNotification object:view];
    }
    [notificationCenter addObserver:self selector:@selector(windowWillClose:)
                               name:NSWindowWillCloseNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowBecameKey:)
                               name:NSWindowDidBecomeKeyNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowResignedKey:)
                               name:NSWindowDidResignKeyNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowDidMiniaturize:)
                               name:NSWindowDidMiniaturizeNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowDidDeminiaturize:)
                               name:NSWindowDidDeminiaturizeNotification object:theWindow];
}

- (void)removeWindowObservers
{
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter removeObserver:self name:NSViewFrameDidChangeNotification     object:nil];
    [notificationCenter removeObserver:self name:NSViewBoundsDidChangeNotification    object:nil];
    [notificationCenter removeObserver:self name:NSWindowWillCloseNotification        object:nil];
    [notificationCenter removeObserver:self name:NSWindowDidBecomeKeyNotification     object:nil];
    [notificationCenter removeObserver:self name:NSWindowDidResignKeyNotification     object:nil];
    [notificationCenter removeObserver:self name:NSWindowDidMiniaturizeNotification   object:nil];
    [notificationCenter removeObserver:self name:NSWindowDidDeminiaturizeNotification object:nil];
}

- (BOOL)start
{
    ASSERT([self currentWindow]);
    
    if (isStarted) {
        return YES;
    }

    if (![[WebPreferences standardPreferences] arePlugInsEnabled] || ![self canStart]) {
        return NO;
    }

    ASSERT(NPP_New);

    [[self class] setCurrentPluginView:self];
    NPError npErr = NPP_New((char *)[MIMEType cString], instance, mode, argsCount, cAttributes, cValues, NULL);
    [[self class] setCurrentPluginView:nil];
    
    LOG(Plugins, "NPP_New: %d", npErr);
    if (npErr != NPERR_NO_ERROR) {
        ERROR("NPP_New failed with error: %d", npErr);
        return NO;
    }

    isStarted = YES;
        
    [self setWindow];

    if ([self window]) {
        [self addWindowObservers];
        if ([[self window] isKeyWindow]) {
            [self sendActivateEvent:YES];
        }
        if (![[self window] isMiniaturized]) {
            [self restartNullEvents];
        }
    }

    [self resetTrackingRect];
    
    [self didStart];

    return YES;
}

- (void)stop
{
    [self removeTrackingRect];

    if (!isStarted) {
        return;
    }
    
    isStarted = NO;
    
    // Stop any active streams
    [streams makeObjectsPerformSelector:@selector(stop)];
    
    // Stop the null events
    [self stopNullEvents];

    // Set cursor back to arrow cursor
    [[NSCursor arrowCursor] set];
    
    // Stop notifications and callbacks.
    [self removeWindowObservers];
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    
    NPError npErr;
    npErr = NPP_Destroy(instance, NULL);
    LOG(Plugins, "NPP_Destroy: %d", npErr);

    instance->pdata = NULL;
}

- (BOOL)isStarted
{
    return isStarted;
}

- (WebDataSource *)dataSource
{
    // Do nothing. Overridden by subclasses.
    return nil;
}

- (WebFrame *)webFrame
{
    return [[self dataSource] webFrame];
}

- (WebView *)webView
{
    return [[self webFrame] webView];
}

- (NSWindow *)currentWindow
{
    return [self window] ? [self window] : [[self webView] hostWindow];
}

- (NPP)pluginPointer
{
    return instance;
}

- (WebNetscapePluginPackage *)plugin
{
    return plugin;
}

- (void)setPlugin:(WebNetscapePluginPackage *)thePlugin;
{
    [plugin release];
    plugin = [thePlugin retain];

    NPP_New = 		[plugin NPP_New];
    NPP_Destroy = 	[plugin NPP_Destroy];
    NPP_SetWindow = 	[plugin NPP_SetWindow];
    NPP_NewStream = 	[plugin NPP_NewStream];
    NPP_WriteReady = 	[plugin NPP_WriteReady];
    NPP_Write = 	[plugin NPP_Write];
    NPP_StreamAsFile = 	[plugin NPP_StreamAsFile];
    NPP_DestroyStream = [plugin NPP_DestroyStream];
    NPP_HandleEvent = 	[plugin NPP_HandleEvent];
    NPP_URLNotify = 	[plugin NPP_URLNotify];
    NPP_GetValue = 	[plugin NPP_GetValue];
    NPP_SetValue = 	[plugin NPP_SetValue];
    NPP_Print = 	[plugin NPP_Print];
}

- (void)setMIMEType:(NSString *)theMIMEType
{
    NSString *type = [theMIMEType copy];
    [MIMEType release];
    MIMEType = type;
}

- (void)setBaseURL:(NSURL *)theBaseURL
{
    [theBaseURL retain];
    [baseURL release];
    baseURL = theBaseURL;
}

- (void)setAttributeKeys:(NSArray *)keys andValues:(NSArray *)values;
{
    ASSERT([keys count] == [values count]);
    
    // Convert the attributes to 2 C string arrays.
    // These arrays are passed to NPP_New, but the strings need to be
    // modifiable and live the entire life of the plugin.

    // The Java plug-in requires the first argument to be the base URL
    if ([MIMEType isEqualToString:@"application/x-java-applet"]) {
        cAttributes = (char **)malloc(([keys count] + 1) * sizeof(char *));
        cValues = (char **)malloc(([values count] + 1) * sizeof(char *));
        cAttributes[0] = strdup("DOCBASE");
        cValues[0] = strdup([[baseURL absoluteString] UTF8String]);
        argsCount++;
    } else {
        cAttributes = (char **)malloc([keys count] * sizeof(char *));
        cValues = (char **)malloc([values count] * sizeof(char *));
    }

    unsigned i;
    unsigned count = [keys count];
    for (i = 0; i < count; i++) {
        NSString *key = [keys objectAtIndex:i];
        NSString *value = [values objectAtIndex:i];
        if ([key _web_isCaseInsensitiveEqualToString:@"height"]) {
            specifiedHeight = [value intValue];
        } else if ([key _web_isCaseInsensitiveEqualToString:@"width"]) {
            specifiedWidth = [value intValue];
        }
        cAttributes[argsCount] = strdup([key UTF8String]);
        cValues[argsCount] = strdup([value UTF8String]);
        LOG(Plugins, "%@ = %@", key, value);
        argsCount++;
    }
}

- (void)setMode:(int)theMode
{
    mode = theMode;
}

#pragma mark NSVIEW

- initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];

    instance = &instanceStruct;
    instance->ndata = self;

    streams = [[NSMutableArray alloc] init];
    streamNotifications = [[NSMutableDictionary alloc] init];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(preferencesHaveChanged:)
                                                 name:WebPreferencesChangedNotification
                                               object:nil];

    return self;
}

-(void)dealloc
{
    unsigned i;

    [[NSNotificationCenter defaultCenter] removeObserver:self];
    
    [self stop];

    for (i = 0; i < argsCount; i++) {
        free(cAttributes[i]);
        free(cValues[i]);
    }
    [streams removeAllObjects];
    [streams release];
    [MIMEType release];
    [baseURL release];
    [streamNotifications removeAllObjects];
    [streamNotifications release];
    free(cAttributes);
    free(cValues);
    [super dealloc];
}

- (void)drawRect:(NSRect)rect
{
    if (!isStarted) {
        return;
    }
    
    if ([NSGraphicsContext currentContextDrawingToScreen]) {
        [self sendUpdateEvent];
    } else {
        // Printing 2862383
    }
}

- (BOOL)isFlipped
{
    return YES;
}

-(void)tellQuickTimeToChill
{
    // Make a call to the secret QuickDraw API that makes QuickTime calm down.
    WindowRef windowRef = [[self window] windowRef];
    if (!windowRef) {
        return;
    }
    CGrafPtr port = GetWindowPort(windowRef);
    Rect bounds;
    GetPortBounds(port, &bounds);
    CallDrawingNotifications(port, &bounds, kBitsProc);
}

- (void)viewWillMoveToWindow:(NSWindow *)newWindow
{
    [self tellQuickTimeToChill];

    // We must remove the tracking rect before we move to the new window.
    // Once we move to the new window, it will be too late.
    [self removeTrackingRect];
    [self removeWindowObservers];

    if (!newWindow) {
        if ([[self webView] hostWindow]) {
            // View will be moved out of the actual window but it still has a host window.
            [self stopNullEvents];
        } else {
            // View will have no associated windows.
            [self stop];
        }
    }
}

- (void)viewDidMoveToWindow
{
    [self resetTrackingRect];
    
    if ([self window]) {
        // View moved to an actual window. Start it if not already started.
        [self start];
        [self restartNullEvents];
        [self addWindowObservers];
    } else if ([[self webView] hostWindow]) {
        // View moved out of an actual window, but still has a host window.
        // Call setWindow to explicitly "clip out" the plug-in from sight.
        // FIXME: It would be nice to do this where we call stopNullEvents in viewWillMoveToWindow.
        [self setWindow];
    }
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
    if (!hostWindow && ![self window]) {
        // View will have no associated windows.
        [self stop];
    }
}

- (void)viewDidMoveToHostWindow
{
    if ([[self webView] hostWindow]) {
        // View now has an associated window. Start it if not already started.
        [self start];
    }
}

#pragma mark NOTIFICATIONS

-(void)viewHasMoved:(NSNotification *)notification
{
    [self tellQuickTimeToChill];
    [self setWindow];
    [self resetTrackingRect];
}

-(void)windowWillClose:(NSNotification *)notification
{
    [self stop];
}

-(void)windowBecameKey:(NSNotification *)notification
{
    [self sendActivateEvent:YES];
    [self setNeedsDisplay:YES];
    [self restartNullEvents];
}

-(void)windowResignedKey:(NSNotification *)notification
{
    [self sendActivateEvent:NO];
    [self setNeedsDisplay:YES];
    [self restartNullEvents];
}

-(void)windowDidMiniaturize:(NSNotification *)notification
{
    [self stopNullEvents];
}

-(void)windowDidDeminiaturize:(NSNotification *)notification
{
    [self restartNullEvents];
}

- (void)preferencesHaveChanged:(NSNotification *)notification
{
    WebPreferences *preferences = [[self webView] preferences];
    BOOL arePlugInsEnabled = [preferences arePlugInsEnabled];
    
    if ([notification object] == preferences && isStarted != arePlugInsEnabled) {
        if (arePlugInsEnabled) {
            if ([self currentWindow]) {
                [self start];
            }
        } else {
            [self stop];
            [self setNeedsDisplay:YES];
        }
    }
}

- (void)frameStateChanged:(NSNotification *)notification
{
    WebFrame *frame = [notification object];
    NSURL *URL = [[[frame dataSource] request] URL];
    NSValue *notifyDataValue = [streamNotifications objectForKey:URL];
    if (!notifyDataValue) {
        return;
    }
    
    void *notifyData = [notifyDataValue pointerValue];
    WebFrameState frameState = [[[notification userInfo] objectForKey:WebCurrentFrameState] intValue];
    if (frameState == WebFrameStateComplete) {
        if (isStarted) {
            NPP_URLNotify(instance, [[URL absoluteString] cString], NPRES_DONE, notifyData);
        }
        [streamNotifications removeObjectForKey:URL];
    }

    //FIXME: Need to send other NPReasons
}

@end

@implementation WebBaseNetscapePluginView (WebNPPCallbacks)

- (NSMutableURLRequest *)requestWithURLCString:(const char *)URLCString
{
    if (!URLCString) {
        return nil;
    }
    
    NSString *URLString = (NSString *)CFStringCreateWithCString(kCFAllocatorDefault, URLCString, kCFStringEncodingWindowsLatin1);
    NSURL *URL = [NSURL _web_URLWithString:URLString relativeToURL:baseURL];
    [URLString release];

    if (!URL) {
        return nil;
    }
    
    return [NSMutableURLRequest requestWithURL:URL];
}

- (void)loadPluginRequest:(WebPluginRequest *)pluginRequest
{
    NSURLRequest *request = [pluginRequest request];
    NSString *frameName = [pluginRequest frameName];
    void *notifyData = [pluginRequest notifyData];

    // FIXME - need to get rid of this window creation which
    // bypasses normal targeted link handling
    WebFrame *frame = [[self webFrame] findFrameNamed:frameName];

    if (frame == nil) {
	WebView *newWebView = nil;
	WebView *currentWebView = [self webView];
	id wd = [currentWebView UIDelegate];
	if ([wd respondsToSelector:@selector(webView:createWebViewWithRequest:)])
	    newWebView = [wd webView:currentWebView createWebViewWithRequest:nil];
	else
	    newWebView = [[WebDefaultUIDelegate sharedUIDelegate] webView:currentWebView createWebViewWithRequest:nil];
        
	[newWebView _setTopLevelFrameName:frameName];
	[[newWebView _UIDelegateForwarder] webViewShow:newWebView];
	frame = [newWebView mainFrame];
    }

    NSURL *URL = [request URL];
    NSString *JSString = [URL _web_scriptIfJavaScriptURL];
    if (JSString) {
        [[frame _bridge] stringByEvaluatingJavaScriptFromString:JSString];
        // FIXME: If the result is a string, we probably want to put that string into the frame, just
        // like we do in KHTMLPartBrowserExtension::openURLRequest.
        if (notifyData && isStarted) {
            NPP_URLNotify(instance, [[URL absoluteString] cString], NPRES_DONE, notifyData);
        }
    } else {
        [frame loadRequest:request];
        if (notifyData) {
            // FIXME: How do we notify about failures? It seems this will only notify about success.
        
            // FIXME: This will overwrite any previous notification for the same URL.
            // It might be better to keep track of these per frame.
            [streamNotifications setObject:[NSValue valueWithPointer:notifyData] forKey:URL];
            
            // FIXME: We add this same observer to a frame multiple times. Is that OK?
            // FIXME: This observer doesn't get removed until the plugin stops, so we could
            // end up with lots and lots of these.
            [[NSNotificationCenter defaultCenter] addObserver:self
                                                     selector:@selector(frameStateChanged:)
                                                         name:WebFrameStateChangedNotification
                                                       object:frame];
        }
    }
}

- (NPError)loadRequest:(NSMutableURLRequest *)request inTarget:(const char *)cTarget withNotifyData:(void *)notifyData
{
    if (![request URL]) {
        return NPERR_INVALID_URL;
    }
    
    if (!cTarget) {
        WebNetscapePluginStream *stream = [[WebNetscapePluginStream alloc]
            initWithRequest:request pluginPointer:instance notifyData:notifyData];
        if (!stream) {
            return NPERR_INVALID_URL;
        }
        [streams addObject:stream];
        [stream start];
        [stream release];
    } else {
        // Find the frame given the target string.
        NSString *target = (NSString *)CFStringCreateWithCString(kCFAllocatorDefault, cTarget, kCFStringEncodingWindowsLatin1);

	[request setHTTPReferrer:[[[[[self webFrame] dataSource] request] URL] absoluteString]];
        // Make a request, but don't do it right away because it could make the plugin view go away.
        WebPluginRequest *pluginRequest = [[WebPluginRequest alloc]
            initWithRequest:request frameName:target notifyData:notifyData];
        [self performSelector:@selector(loadPluginRequest:) withObject:pluginRequest afterDelay:0];
        [pluginRequest release];
	[target release];
    }
    
    return NPERR_NO_ERROR;
}

-(NPError)getURLNotify:(const char *)URLCString target:(const char *)cTarget notifyData:(void *)notifyData
{
    LOG(Plugins, "NPN_GetURLNotify: %s target: %s", URLCString, cTarget);

    NSMutableURLRequest *request = [self requestWithURLCString:URLCString];
    return [self loadRequest:request inTarget:cTarget withNotifyData:notifyData];
}

-(NPError)getURL:(const char *)URLCString target:(const char *)cTarget
{
    LOG(Plugins, "NPN_GetURL: %s target: %s", URLCString, cTarget);

    NSMutableURLRequest *request = [self requestWithURLCString:URLCString];
    return [self loadRequest:request inTarget:cTarget withNotifyData:NULL];
}

- (NPError)_postURLNotify:(const char *)URLCString
                   target:(const char *)target
                      len:(UInt32)len
                      buf:(const char *)buf
                     file:(NPBool)file
               notifyData:(void *)notifyData
             allowHeaders:(BOOL)allowHeaders
{
    if (!URLCString || !len || !buf) {
        return NPERR_INVALID_PARAM;
    }
    
    NSData *postData = nil;

    if (file) {
        // If we're posting a file, buf is either a file URL or a path to the file.
        NSString *bufString = (NSString *)CFStringCreateWithCString(kCFAllocatorDefault, buf, kCFStringEncodingWindowsLatin1);
        if (!bufString) {
            return NPERR_INVALID_PARAM;
        }
        NSURL *fileURL = [NSURL _web_URLWithString:bufString];
        NSString *path;
        if ([fileURL isFileURL]) {
            path = [fileURL path];
        } else {
            path = bufString;
        }
        postData = [NSData dataWithContentsOfFile:[path _web_fixedCarbonPOSIXPath]];
        [bufString release];
        if (!postData) {
            return NPERR_FILE_NOT_FOUND;
        }
    } else {
        postData = [NSData dataWithBytes:buf length:len];
    }

    if ([postData length] == 0) {
        return NPERR_INVALID_PARAM;
    }

    NSMutableURLRequest *request = [self requestWithURLCString:URLCString];
    [request setHTTPMethod:@"POST"];
    
    if (allowHeaders) {
        if ([postData _web_startsWithBlankLine]) {
            postData = [postData subdataWithRange:NSMakeRange(1, [postData length] - 1)];
        } else {
            unsigned location = [postData _web_locationAfterFirstBlankLine];
            if (location != NSNotFound) {
                // If the blank line is somewhere in the middle of postData, everything before is the header.
                NSData *headerData = [postData subdataWithRange:NSMakeRange(0, location)];
                NSMutableDictionary *header = [headerData _web_parseRFC822HeaderFields];
		unsigned dataLength = [postData length] - location;

		// Sometimes plugins like to set Content-Length themselves when they post,
		// but WebFoundation does not like that. So we will remove the header
		// and instead truncate the data to the requested length.
		NSString *contentLength = [header objectForKey:@"Content-Length"];

		if (contentLength != nil) {
		    dataLength = MIN((unsigned)[contentLength intValue], dataLength);
		}
		[header removeObjectForKey:@"Content-Length"];

                if ([header count] > 0) {
                    [request setAllHTTPHeaderFields:header];
                }
                // Everything after the blank line is the actual content of the POST.
                postData = [postData subdataWithRange:NSMakeRange(location, dataLength)];

            }
        }
        if ([postData length] == 0) {
            return NPERR_INVALID_PARAM;
        }
    }

    [request setHTTPBody:postData];
    return [self loadRequest:request inTarget:target withNotifyData:notifyData];
}

- (NPError)postURLNotify:(const char *)URLCString
                  target:(const char *)target
                     len:(UInt32)len
                     buf:(const char *)buf
                    file:(NPBool)file
              notifyData:(void *)notifyData
{
    LOG(Plugins, "NPN_PostURLNotify: %s", URLCString);
    return [self _postURLNotify:URLCString target:target len:len buf:buf file:file notifyData:notifyData allowHeaders:YES];
}

-(NPError)postURL:(const char *)URLCString
           target:(const char *)target
              len:(UInt32)len
              buf:(const char *)buf
             file:(NPBool)file
{
    LOG(Plugins, "NPN_PostURL: %s", URLCString);        
    return [self _postURLNotify:URLCString target:target len:len buf:buf file:file notifyData:NULL allowHeaders:NO];
}

-(NPError)newStream:(NPMIMEType)type target:(const char *)target stream:(NPStream**)stream
{
    LOG(Plugins, "NPN_NewStream");
    return NPERR_GENERIC_ERROR;
}

-(NPError)write:(NPStream*)stream len:(SInt32)len buffer:(void *)buffer
{
    LOG(Plugins, "NPN_Write");
    return NPERR_GENERIC_ERROR;
}

-(NPError)destroyStream:(NPStream*)stream reason:(NPReason)reason
{
    LOG(Plugins, "NPN_DestroyStream");
    if (!stream->ndata) {
        return NPERR_INVALID_INSTANCE_ERROR;
    }
    [(WebNetscapePluginStream *)stream->ndata stop];
    return NPERR_NO_ERROR;
}

- (const char *)userAgent
{
    return [[[self webView] userAgentForURL:baseURL] lossyCString];
}

-(void)status:(const char *)message
{    
    if (!message) {
        ERROR("NPN_Status passed a NULL status message");
        return;
    }

    NSString *status = (NSString *)CFStringCreateWithCString(NULL, message, kCFStringEncodingWindowsLatin1);
    LOG(Plugins, "NPN_Status: %@", status);
    WebView *wv = [self webView];
    [[wv _UIDelegateForwarder] webView:wv setStatusText:status];
    [status release];
}

-(void)invalidateRect:(NPRect *)invalidRect
{
    LOG(Plugins, "NPN_InvalidateRect");
    [self setNeedsDisplayInRect:NSMakeRect(invalidRect->left, invalidRect->top,
        (float)invalidRect->right - invalidRect->left, (float)invalidRect->bottom - invalidRect->top)];
}

-(void)invalidateRegion:(NPRegion)invalidRegion
{
    LOG(Plugins, "NPN_InvalidateRegion");
    Rect invalidRect;
    GetRegionBounds(invalidRegion, &invalidRect);
    [self setNeedsDisplayInRect:NSMakeRect(invalidRect.left, invalidRect.top,
        (float)invalidRect.right - invalidRect.left, (float)invalidRect.bottom - invalidRect.top)];
}

-(void)forceRedraw
{
    LOG(Plugins, "forceRedraw");
    [self setNeedsDisplay:YES];
    [[self window] displayIfNeeded];
}

@end

@implementation WebPluginRequest

- (id)initWithRequest:(NSURLRequest *)request frameName:(NSString *)frameName notifyData:(void *)notifyData
{
    [super init];
    _request = [request retain];
    _frameName = [frameName retain];
    _notifyData = notifyData;
    return self;
}

- (void)dealloc
{
    [_request release];
    [_frameName release];
    [super dealloc];
}

- (NSURLRequest *)request
{
    return _request;
}

- (NSString *)frameName
{
    return _frameName;
}

- (void *)notifyData
{
    return _notifyData;
}

@end

@implementation NSData (PluginExtras)

- (BOOL)_web_startsWithBlankLine
{
    return [self length] > 0 && ((const char *)[self bytes])[0] == '\n';
}

- (unsigned)_web_locationAfterFirstBlankLine
{
    const char *bytes = (const char *)[self bytes];
    unsigned length = [self length];

    unsigned i;
    for (i = 0; i < length - 2; i++) {
        if (bytes[i] == '\n' && (i == 0 || bytes[i+1] == '\n')) {
            i++;
            while (i < length - 2 && bytes[i] == '\n') {
                i++;
            }
            return i;
        }
    }
    
    return NSNotFound;
}

@end

