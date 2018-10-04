/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __LP64__

#import "HIWebView.h"

#import "CarbonWindowAdapter.h"
#import "HIViewAdapter.h"
#import "WebHTMLViewInternal.h"
#import "WebKit.h"
#import <pal/spi/mac/NSEventSPI.h>
#import <pal/spi/mac/NSGraphicsSPI.h>
#import <pal/spi/mac/QuickDrawSPI.h>
#import <wtf/Assertions.h>
#import <wtf/ObjcRuntimeExtras.h>
#import <wtf/cf/TypeCastsCF.h>

WTF_DECLARE_CF_TYPE_TRAIT(CFRunLoop);

@interface NSWindow (AppKitSecretsHIWebViewKnows)
- (void)_removeWindowRef;
@end

@interface NSView (AppKitSecretsHIWebViewKnows)
- (void)_clearDirtyRectsForTree;
@end

extern "C" void HIWebViewRegisterClass();

@interface MenuItemProxy : NSObject <NSValidatedUserInterfaceItem>
{
    int     _tag;
    SEL _action;
}

- (id)initWithAction:(SEL)action;
- (SEL)action;
- (int)tag;

@end

@implementation MenuItemProxy

- (id)initWithAction:(SEL)action
{
    [super init];
    if (self == nil) return nil;

    _action = action;

    return self;
}

- (SEL)action
{
       return _action;
}

- (int)tag 
{
    return 0;
}

@end

@interface NSWindowGraphicsContext (HIWebView)
- (void)_web_setGraphicsPort:(CGContextRef)context;
@end

@implementation NSWindowGraphicsContext (HIWebView)

- (void)_web_setGraphicsPort:(CGContextRef)context
{
    CGContextRetain(context);
    CGContextRelease(_cgsContext);
    _cgsContext = context;
}

@end

struct HIWebView
{
    HIViewRef fViewRef;

    WebView* fWebView;
    NSView* fFirstResponder;
    CarbonWindowAdapter* fKitWindow;
    bool fIsComposited;
    CFRunLoopObserverRef fUpdateObserver;
};
typedef struct HIWebView HIWebView;

static const OSType NSAppKitPropertyCreator = 'akit';
/*
These constants are not used. Commented out to make the compiler happy.
static const OSType NSViewCarbonControlViewPropertyTag = 'view';
static const OSType NSViewCarbonControlAutodisplayPropertyTag = 'autd';
static const OSType NSViewCarbonControlFirstResponderViewPropertyTag = 'frvw';
*/
static const OSType NSCarbonWindowPropertyTag = 'win ';


static SEL _NSSelectorForHICommand(const HICommand*);

static const EventTypeSpec kEvents[] = { 
    { kEventClassHIObject, kEventHIObjectConstruct },
    { kEventClassHIObject, kEventHIObjectDestruct },
    
    { kEventClassMouse, kEventMouseUp },
    { kEventClassMouse, kEventMouseMoved },
    { kEventClassMouse, kEventMouseDragged },
    { kEventClassMouse, kEventMouseWheelMoved },

    { kEventClassKeyboard, kEventRawKeyDown },
    { kEventClassKeyboard, kEventRawKeyRepeat },

    { kEventClassCommand, kEventCommandProcess },
    { kEventClassCommand, kEventCommandUpdateStatus },

    { kEventClassControl, kEventControlInitialize },
    { kEventClassControl, kEventControlDraw },
    { kEventClassControl, kEventControlHitTest },
    { kEventClassControl, kEventControlGetPartRegion },
    { kEventClassControl, kEventControlGetData },
    { kEventClassControl, kEventControlBoundsChanged },
    { kEventClassControl, kEventControlActivate },
    { kEventClassControl, kEventControlDeactivate },
    { kEventClassControl, kEventControlOwningWindowChanged },
    { kEventClassControl, kEventControlClick },
    { kEventClassControl, kEventControlContextualMenuClick },
    { kEventClassControl, kEventControlSetFocusPart }
};

#define kHIViewBaseClassID CFSTR("com.apple.hiview")
#define kHIWebViewClassID CFSTR("com.apple.HIWebView")

static HIWebView* HIWebViewConstructor(HIViewRef inView);
static void HIWebViewDestructor(HIWebView*);

static OSStatus HIWebViewEventHandler(EventHandlerCallRef inCallRef, EventRef inEvent, void *inUserData);

static UInt32 GetBehaviors();
static ControlKind GetKind();
static void Draw(HIWebView* inView, RgnHandle limitRgn, CGContextRef inContext);
static ControlPartCode HitTest(HIWebView*, const HIPoint* where);
static OSStatus GetRegion(HIWebView*, ControlPartCode inPart, RgnHandle outRgn);
static void BoundsChanged(HIWebView* inView, UInt32 inOptions, const HIRect* inOriginalBounds, const HIRect* inCurrentBounds);
static void OwningWindowChanged(HIWebView*, WindowRef oldWindow, WindowRef newWindow);
static void ActiveStateChanged(HIWebView*);

static OSStatus Click(HIWebView* inView, EventRef inEvent);
static OSStatus ContextMenuClick(HIWebView* inView, EventRef inEvent);
static OSStatus MouseUp(HIWebView* inView, EventRef inEvent);
static OSStatus MouseMoved(HIWebView* inView, EventRef inEvent);
static OSStatus MouseDragged(HIWebView* inView, EventRef inEvent);
static OSStatus MouseWheelMoved(HIWebView* inView, EventRef inEvent);

static OSStatus ProcessCommand(HIWebView* inView, const HICommand* inCommand);
static OSStatus UpdateCommandStatus(HIWebView* inView, const HICommand* inCommand);

static OSStatus SetFocusPart(HIWebView*, ControlPartCode desiredFocus, RgnHandle invalidRgn, Boolean focusEverything, ControlPartCode* actualFocus);
static NSView* AdvanceFocus(HIWebView*, bool forward);
static void RelinquishFocus(HIWebView*, bool inAutodisplay);

static WindowRef GetWindowRef(HIWebView* inView);
static void SyncFrame(HIWebView* inView);

static OSStatus WindowHandler(EventHandlerCallRef inCallRef, EventRef inEvent, void* inUserData);

static void StartUpdateObserver(HIWebView*);
static void StopUpdateObserver(HIWebView*);

//----------------------------------------------------------------------------------
// HIWebViewCreate
//----------------------------------------------------------------------------------
//
OSStatus HIWebViewCreate(HIViewRef* outControl)
{
    HIWebViewRegisterClass();
    return HIObjectCreate(kHIWebViewClassID, nullptr, (HIObjectRef*)outControl);
}

//----------------------------------------------------------------------------------
// HIWebViewGetWebView
//----------------------------------------------------------------------------------
//
WebView* HIWebViewGetWebView(HIViewRef inView)
{
    HIWebView* view = (HIWebView*)HIObjectDynamicCast((HIObjectRef)inView, kHIWebViewClassID);
    WebView* result = nullptr;

    if (view)
        result = view->fWebView;

    return result;
}

//----------------------------------------------------------------------------------
// HIWebViewConstructor
//----------------------------------------------------------------------------------
//

static HIWebView* HIWebViewConstructor(HIViewRef inView)
{
    HIWebView* view = (HIWebView*)malloc(sizeof(HIWebView));

    if (view) {
        NSRect frame = { { 0, 0 }, { 400, 400  } };

        view->fViewRef = inView;

        WebView *webView = [[WebView alloc] initWithFrame: frame];
        CFRetain(webView);
        [webView release];
        view->fWebView = webView;
        [HIViewAdapter bindHIViewToNSView:inView nsView:view->fWebView];

        view->fFirstResponder = nullptr;
        view->fKitWindow = nullptr;
        view->fIsComposited = false;
        view->fUpdateObserver = nullptr;
    }

    return view;
}

//----------------------------------------------------------------------------------
// HIWebViewDestructor
//----------------------------------------------------------------------------------
//
static void HIWebViewDestructor(HIWebView* inView)
{
    [HIViewAdapter unbindNSView:inView->fWebView];
    CFRelease(inView->fWebView);

    free(inView);
}

//----------------------------------------------------------------------------------
// HIWebViewRegisterClass
//----------------------------------------------------------------------------------
//
void HIWebViewRegisterClass()
{
    static bool sRegistered;

    if (!sRegistered) {
        HIObjectRegisterSubclass(kHIWebViewClassID, kHIViewBaseClassID, 0, HIWebViewEventHandler,
            GetEventTypeCount(kEvents), kEvents, 0, nullptr);
        sRegistered = true;
    }
}

//----------------------------------------------------------------------------------
// GetBehaviors
//----------------------------------------------------------------------------------
//
static UInt32 GetBehaviors()
{
    return kControlSupportsDataAccess | kControlSupportsGetRegion | kControlGetsFocusOnClick;
}

static CGContextRef overrideCGContext(NSWindow *window, CGContextRef context)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSWindowGraphicsContext *graphicsContext = (NSWindowGraphicsContext *)window.graphicsContext;
    CGContextRef savedContext = (CGContextRef)graphicsContext.graphicsPort;
    ALLOW_DEPRECATED_DECLARATIONS_END
    CGContextRetain(savedContext);
    [graphicsContext _web_setGraphicsPort:context];
    return savedContext;
}

static void restoreCGContext(NSWindow *window, CGContextRef savedContext)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSWindowGraphicsContext *graphicsContext = (NSWindowGraphicsContext *)window.graphicsContext;
    ALLOW_DEPRECATED_DECLARATIONS_END
    [graphicsContext _web_setGraphicsPort:savedContext];
    CGContextRelease(savedContext);
}

//----------------------------------------------------------------------------------
// Draw
//----------------------------------------------------------------------------------
//
static void Draw(HIWebView* inView, RgnHandle limitRgn, CGContextRef inContext)
{
    HIRect bounds;
    Rect drawRect;
    HIRect hiRect;
    bool createdContext = false;

    if (!inView->fIsComposited) {
        GrafPtr port;
        Rect portRect;

        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        GetPort(&port);
        GetPortBounds(port, &portRect);
        CreateCGContextForPort(port, &inContext);
        SyncCGContextOriginWithPort(inContext, port);
        CGContextTranslateCTM(inContext, 0, (portRect.bottom - portRect.top));
        CGContextScaleCTM(inContext, 1, -1);
        createdContext = true;
    }

    HIViewGetBounds(inView->fViewRef, &bounds);

    CGContextRef savedContext = overrideCGContext(inView->fKitWindow, inContext);
    [NSGraphicsContext setCurrentContext:[inView->fKitWindow graphicsContext]];

    GetRegionBounds(limitRgn, &drawRect);

    if (!inView->fIsComposited)
        OffsetRect(&drawRect, (SInt16)-bounds.origin.x, (SInt16)-bounds.origin.y);
    
    hiRect.origin.x = drawRect.left;
    hiRect.origin.y = bounds.size.height - drawRect.bottom; // flip y
    hiRect.size.width = drawRect.right - drawRect.left;
    hiRect.size.height = drawRect.bottom - drawRect.top;

//    printf("Drawing: drawRect is (%g %g) (%g %g)\n", hiRect.origin.x, hiRect.origin.y,
//            hiRect.size.width, hiRect.size.height);

    // FIXME: We need to do layout before Carbon has decided what region needs drawn.
    // In Cocoa we make sure to do layout and invalidate any new regions before draw, so everything
    // can be drawn in one pass. Doing a layout here will cause new regions to be invalidated, but they
    // will not all be drawn in this pass since we already have a fixed rect we are going to display.

    NSView <WebDocumentView> *documentView = [[[inView->fWebView mainFrame] frameView] documentView];
    if ([documentView isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView *)documentView _web_updateLayoutAndStyleIfNeededRecursive];

    if (inView->fIsComposited)
        [inView->fWebView displayIfNeededInRect: *(NSRect*)&hiRect];
    else
        [inView->fWebView displayRect:*(NSRect*)&hiRect];

    restoreCGContext(inView->fKitWindow, savedContext);

    if (!inView->fIsComposited)
    {
        HIViewRef      view;
        HIViewFindByID(HIViewGetRoot(GetControlOwner(inView->fViewRef)), kHIViewWindowGrowBoxID, &view);
        if (view)
        {
            HIRect frame;

            HIViewGetBounds(view, &frame);
            HIViewConvertRect(&frame, view, nullptr);

            hiRect.origin.x = drawRect.left;
            hiRect.origin.y = drawRect.top;
            hiRect.size.width = drawRect.right - drawRect.left;
            hiRect.size.height = drawRect.bottom - drawRect.top;

            HIViewConvertRect(&hiRect, inView->fViewRef, nullptr);

            if (CGRectIntersectsRect(frame, hiRect))
                HIViewSetNeedsDisplay(view, true);
        }
    }

    if (createdContext)
    {
        CGContextSynchronize(inContext);
        CGContextRelease(inContext);
    }
}

//----------------------------------------------------------------------------------
// HitTest
//----------------------------------------------------------------------------------
//
static ControlPartCode HitTest(HIWebView* view, const HIPoint* where)
{
    HIRect bounds;

    HIViewGetBounds(view->fViewRef, &bounds);

    if (CGRectContainsPoint(bounds, *where))
        return 1;
    return kControlNoPart;
}

//----------------------------------------------------------------------------------
// GetRegion
//----------------------------------------------------------------------------------
//
static OSStatus GetRegion(HIWebView* inView, ControlPartCode inPart, RgnHandle outRgn)
{
    OSStatus err = eventNotHandledErr;

    if (inPart == -3 && [inView->fWebView isOpaque]) {
        // kControlOpaqueMetaPart:
        HIRect bounds;
        Rect temp;

        HIViewGetBounds(inView->fViewRef, &bounds);

        temp.top = (SInt16)bounds.origin.y;
        temp.left = (SInt16)bounds.origin.x;
        temp.bottom = (SInt16)CGRectGetMaxY(bounds);
        temp.right = (SInt16)CGRectGetMaxX(bounds);

        RectRgn(outRgn, &temp);
        err = noErr;
    }

    return err;
}

static WindowRef GetWindowRef(HIWebView* inView)
{
    return GetControlOwner(inView->fViewRef);
}

static NSEvent *CreateNSEventAdoptingCGEvent(CGEventRef cgEvent, EventRef eventRef)
{
    NSEvent *result = [[NSEvent alloc] _initWithCGEvent:cgEvent eventRef:eventRef];
    CFRelease(cgEvent);
    return result;
}

static Boolean
CopyEventCGEvent(EventRef event, CGEventRef* cgEvent)
{
    if ((*cgEvent = CopyEventCGEvent(event)))
        return true;

    // This event might not have been created directly from a CGS event, and might not
    // have a CGEventRef associated with it. In that case, try using the event most
    // recently dispatched by the event dispatcher, which is likely to be the original
    // user-input event containing a CGEventRef.
    event = GetCurrentEvent();
    if (event && (*cgEvent = CopyEventCGEvent(event)))
        return true;
    return false;
}

static NSEvent *CreateNSEventWithCarbonClickEvent(EventRef inEvent, WindowRef windowRef)
{
    EventRef newEvent;
    Point where;
    OSStatus err;
    UInt32 modifiers;
    Rect windRect;

    CGEventRef cgEvent = nullptr;
    if (!CopyEventCGEvent(inEvent, &cgEvent))
        return nil;

    // We need to make the event be a kEventMouseDown event, or the webkit might trip up when
    // we click on a Netscape plugin. It calls ConvertEventRefToEventRecord, assuming
    // that mouseDown was passed an event with a real mouse down eventRef. We just need a
    // minimal one here.

    err = CreateEvent(nullptr, kEventClassMouse, kEventMouseDown, GetEventTime(inEvent), 0, &newEvent);
    if (err != noErr)
        return nil;

    GetEventParameter(inEvent, kEventParamWindowMouseLocation, typeQDPoint, nullptr, sizeof(Point), nullptr, &where);
    GetWindowBounds(windowRef, kWindowStructureRgn, &windRect);
    where.h += windRect.left;
    where.v += windRect.top;

    GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, nullptr, sizeof(UInt32), nullptr, &modifiers);
    SetEventParameter(newEvent, kEventParamMouseLocation, typeQDPoint, sizeof(Point), &where);
    SetEventParameter(newEvent, kEventParamKeyModifiers, typeUInt32, sizeof(UInt32), &modifiers);

    return CreateNSEventAdoptingCGEvent(cgEvent, newEvent);
}

//----------------------------------------------------------------------------------
// Click
//----------------------------------------------------------------------------------
//
static OSStatus Click(HIWebView* inView, EventRef inEvent)
{
    NSEvent *kitEvent = CreateNSEventWithCarbonClickEvent(inEvent, GetWindowRef(inView));

    if (!inView->fIsComposited)
        StartUpdateObserver(inView);

    [inView->fKitWindow sendEvent:kitEvent];

    if (!inView->fIsComposited)
        StopUpdateObserver(inView);

    [kitEvent release];

    return noErr;
}

static NSEvent *CreateNSEventWithCarbonEvent(EventRef inEvent)
{
    NSEvent *event = [NSEvent eventWithEventRef:inEvent];
    if (!event)
        event = [NSEvent eventWithEventRef:GetCurrentEvent()];

    return [event retain];
}

//----------------------------------------------------------------------------------
// MouseUp
//----------------------------------------------------------------------------------
//
static OSStatus MouseUp(HIWebView* inView, EventRef inEvent)
{
    NSEvent* kitEvent = CreateNSEventWithCarbonEvent(inEvent);

    [inView->fKitWindow sendEvent:kitEvent];

    [kitEvent release];

    return noErr;
}

static NSEvent *CreateNSEventWithCarbonMouseMoveEvent(EventRef inEvent, NSWindow *window)
{
    NSEvent* kitEvent = [NSEvent eventWithEventRef:inEvent];

    // FIXME: Works around bug 3585644. Can remove it once that bug is fixed.
    // We preflight here and don't do any work when the window is already correct
    // because _eventRelativeToWindow will malfunction if the event's window method
    // has been hijacked by the bug workaround used by Contribute. It's fine to just
    // leave the event alone if the window is already correct.

    if ([kitEvent window] != window)
        kitEvent = [kitEvent _eventRelativeToWindow:window];

    return [kitEvent retain];
}

//----------------------------------------------------------------------------------
// MouseMoved
//----------------------------------------------------------------------------------
//
static OSStatus MouseMoved(HIWebView* inView, EventRef inEvent)
{
    NSEvent *kitEvent = CreateNSEventWithCarbonMouseMoveEvent(inEvent, inView->fKitWindow);
    [inView->fKitWindow sendEvent:kitEvent];
    [kitEvent release];

    return noErr;
}

//----------------------------------------------------------------------------------
// MouseDragged
//----------------------------------------------------------------------------------
//
static OSStatus MouseDragged(HIWebView* inView, EventRef inEvent)
{
    NSEvent* kitEvent = CreateNSEventWithCarbonEvent(inEvent);

    [inView->fKitWindow sendEvent:kitEvent];

    [kitEvent release];

    return noErr;
}

//----------------------------------------------------------------------------------
// MouseDragged
//----------------------------------------------------------------------------------
//
static OSStatus MouseWheelMoved(HIWebView* inView, EventRef inEvent)
{
    NSEvent* kitEvent = CreateNSEventWithCarbonEvent(inEvent);

    [inView->fKitWindow sendEvent:kitEvent];

    [kitEvent release];

    return noErr;
}

//----------------------------------------------------------------------------------
// ContextMenuClick
//----------------------------------------------------------------------------------
//
static OSStatus ContextMenuClick(HIWebView* inView, EventRef inEvent)
{
    NSView *webView = inView->fWebView;
    NSWindow *window = [webView window];

    // Get the point out of the event.
    HIPoint point;
    GetEventParameter(inEvent, kEventParamMouseLocation, typeHIPoint, nullptr, sizeof(point), nullptr, &point);
    HIViewConvertPoint(&point, inView->fViewRef, nullptr);
    
    // Flip the Y coordinate, since Carbon is flipped relative to the AppKit.
    NSPoint location = NSMakePoint(point.x, [window frame].size.height - point.y);
    
    // Make up an event with the point and send it to the window.
    NSEvent *kitEvent = [NSEvent mouseEventWithType:NSEventTypeRightMouseDown
                                           location:location
                                      modifierFlags:0
                                          timestamp:GetEventTime(inEvent)
                                       windowNumber:[window windowNumber]
                                            context:0
                                        eventNumber:0
                                         clickCount:1
                                           pressure:0];
    [inView->fKitWindow sendEvent:kitEvent];
    return noErr;
}

//----------------------------------------------------------------------------------
// GetKind
//----------------------------------------------------------------------------------
//
static ControlKind GetKind()
{
    const ControlKind kMyKind = { 'appl', 'wbvw' };
    
    return kMyKind;
}

//----------------------------------------------------------------------------------
// BoundsChanged
//----------------------------------------------------------------------------------
//
static void BoundsChanged(HIWebView* inView, UInt32 inOptions, const HIRect* inOriginalBounds, const HIRect* inCurrentBounds)
{
    if (inView->fWebView)
        SyncFrame(inView);
}

//----------------------------------------------------------------------------------
// OwningWindowChanged
//----------------------------------------------------------------------------------
//
static void
OwningWindowChanged(HIWebView* view, WindowRef oldWindow, WindowRef newWindow)
{
    if (newWindow) {
        WindowAttributes attrs;
        
        OSStatus err = GetWindowProperty(newWindow, NSAppKitPropertyCreator, NSCarbonWindowPropertyTag, sizeof(NSWindow *), nullptr, &view->fKitWindow);
        if (err != noErr) {
            const EventTypeSpec kWindowEvents[] = {
            { kEventClassWindow, kEventWindowClosed },
            { kEventClassMouse, kEventMouseMoved },
            { kEventClassMouse, kEventMouseUp },
            { kEventClassMouse, kEventMouseDragged },
            { kEventClassMouse, kEventMouseWheelMoved },
            { kEventClassKeyboard, kEventRawKeyDown },
            { kEventClassKeyboard, kEventRawKeyRepeat },
            { kEventClassKeyboard, kEventRawKeyUp },
            { kEventClassControl, kEventControlClick },
            };
            
            view->fKitWindow = [[CarbonWindowAdapter alloc] initWithCarbonWindowRef: newWindow takingOwnership: NO disableOrdering:NO carbon:YES];
            SetWindowProperty(newWindow, NSAppKitPropertyCreator, NSCarbonWindowPropertyTag, sizeof(NSWindow *), &view->fKitWindow);
            
            InstallWindowEventHandler(newWindow, WindowHandler, GetEventTypeCount(kWindowEvents), kWindowEvents, newWindow, nullptr);
        }
        
        [[view->fKitWindow contentView] addSubview:view->fWebView];
        
        GetWindowAttributes(newWindow, &attrs);
        view->fIsComposited = attrs & kWindowCompositingAttribute;
        
        SyncFrame(view);        
    } else {
        // Be sure to detach the cocoa view, too.
        if (view->fWebView)
            [view->fWebView removeFromSuperview];
        
        view->fKitWindow = nullptr; // break the ties that bind
    }
}

//-------------------------------------------------------------------------------------
//    WindowHandler
//-------------------------------------------------------------------------------------
//    Redirect mouse events to the views beneath them. This is required for WebKit to work
//    properly. We install it once per window. We also tap into window close to release
//    the NSWindow that shadows our Carbon window.
//
static OSStatus WindowHandler(EventHandlerCallRef inCallRef, EventRef inEvent, void* inUserData)
{
    WindowRef window = (WindowRef)inUserData;
    OSStatus result = eventNotHandledErr;

    NSWindow* kitWindow;
    OSStatus err = noErr;
    NSEvent* kitEvent;

    switch (GetEventClass(inEvent)) {
    case kEventClassControl:
        switch (GetEventKind(inEvent)) {
        case kEventControlClick:
            CarbonWindowAdapter *kitWindow;

            err = GetWindowProperty(window, NSAppKitPropertyCreator, NSCarbonWindowPropertyTag, sizeof(NSWindow *), nullptr, &kitWindow);
                
            // We must be outside the HIWebView, relinquish focus.
            [kitWindow relinquishFocus];
            break;
        }
        break;
            
    case kEventClassKeyboard:
        // if the first responder in the kit window is something other than the
        // window, we assume a subview of the webview is focused. we must send
        // the event to the window so that it goes through the kit's normal TSM
        // logic, and -- more importantly -- allows any delegates associated
        // with the first responder to have a chance at the event.

        err = GetWindowProperty(window, NSAppKitPropertyCreator, NSCarbonWindowPropertyTag, sizeof(NSWindow *), nullptr, &kitWindow);
        if (err == noErr) {
            NSResponder* responder = [kitWindow firstResponder];
            if (responder != kitWindow) {
                kitEvent = CreateNSEventWithCarbonEvent(inEvent);
                
                [kitWindow sendEvent:kitEvent];
                [kitEvent release];
                
                result = noErr;
            }
        }
        break;

    case kEventClassWindow:
        

        err = GetWindowProperty(window, NSAppKitPropertyCreator, NSCarbonWindowPropertyTag, sizeof(NSWindow *), nullptr, &kitWindow);
        if (err == noErr) {
            [kitWindow _removeWindowRef];
            [kitWindow close];
        }
        result = noErr;
        break;

    case kEventClassMouse:
        switch (GetEventKind(inEvent)) {
        case kEventMouseMoved:
            Point where;
            GetEventParameter(inEvent, kEventParamMouseLocation, typeQDPoint, nullptr, sizeof(Point), nullptr, &where);

            WindowRef temp;
            FindWindow(where, &temp);
            if (temp == window) {
                Rect bounds;
                GetWindowBounds(window, kWindowStructureRgn, &bounds);
                where.h -= bounds.left;
                where.v -= bounds.top;
                SetEventParameter(inEvent, kEventParamWindowRef, typeWindowRef, sizeof(WindowRef), &window);
                SetEventParameter(inEvent, kEventParamWindowMouseLocation, typeQDPoint, sizeof(Point), &where);

                HIViewRef view = nullptr;

                err = HIViewGetViewForMouseEvent(HIViewGetRoot(window), inEvent, &view);
                if (err == noErr && view && HIObjectIsOfClass((HIObjectRef)view, kHIWebViewClassID))
                    result = SendEventToEventTargetWithOptions(inEvent, HIObjectGetEventTarget((HIObjectRef)view), kEventTargetDontPropagate);
            }
            break;
                
        case kEventMouseUp:
        case kEventMouseDragged:
        case kEventMouseWheelMoved:
            HIViewRef view = nullptr;

            err = HIViewGetViewForMouseEvent(HIViewGetRoot(window), inEvent, &view);
            if (err == noErr && view && HIObjectIsOfClass((HIObjectRef)view, kHIWebViewClassID))
                result = SendEventToEventTargetWithOptions(inEvent, HIObjectGetEventTarget((HIObjectRef)view), kEventTargetDontPropagate);
            break;
        }
        break;
    }

    return result;
}


//----------------------------------------------------------------------------------
// SyncFrame
//----------------------------------------------------------------------------------
//
static void SyncFrame(HIWebView* inView)
{
    HIViewRef parent = HIViewGetSuperview(inView->fViewRef);
    
    if (parent) {
        if (inView->fIsComposited) {
            HIRect frame;
            HIRect parentBounds;
            NSPoint origin;

            HIViewGetFrame(inView->fViewRef, &frame);
            HIViewGetBounds(parent, &parentBounds);
            
            origin.x = frame.origin.x;
            origin.y = parentBounds.size.height - CGRectGetMaxY(frame);
//    printf("syncing to (%g %g) (%g %g)\n", origin.x, origin.y,
//            frame.size.width, frame.size.height);
            [inView->fWebView setFrameOrigin: origin];
            [inView->fWebView setFrameSize: *(NSSize*)&frame.size];
        } else {
            GrafPtr port = GetWindowPort(GetControlOwner(inView->fViewRef));
            PixMapHandle portPix = GetPortPixMap(port);
            Rect bounds;
            HIRect rootFrame;
            HIRect frame;

            GetControlBounds(inView->fViewRef, &bounds);
            OffsetRect(&bounds, -(**portPix).bounds.left, -(**portPix).bounds.top);

//            printf("control lives at %d %d %d %d in window-coords\n", bounds.top, bounds.left,
//                bounds.bottom, bounds.right);
  
            HIViewGetFrame(HIViewGetRoot(GetControlOwner(inView->fViewRef)), &rootFrame);

            frame.origin.x = bounds.left;
            frame.origin.y = rootFrame.size.height - bounds.bottom;
            frame.size.width = bounds.right - bounds.left;
            frame.size.height = bounds.bottom - bounds.top;

//            printf("   before frame convert (%g %g) (%g %g)\n", frame.origin.x, frame.origin.y,
//                frame.size.width, frame.size.height);
            
            [inView->fWebView convertRect:*(NSRect*)&frame fromView:nil];

//            printf("   moving web view to (%g %g) (%g %g)\n", frame.origin.x, frame.origin.y,
//                frame.size.width, frame.size.height);

            [inView->fWebView setFrameOrigin: *(NSPoint*)&frame.origin];
            [inView->fWebView setFrameSize: *(NSSize*)&frame.size];
        }
    }
}

//----------------------------------------------------------------------------------
// SetFocusPart
//----------------------------------------------------------------------------------
//
static OSStatus SetFocusPart(HIWebView* view, ControlPartCode desiredFocus, RgnHandle invalidRgn, Boolean focusEverything, ControlPartCode* actualFocus)
{
    NSView *freshlyMadeFirstResponderView;
    SInt32 partCodeToReturn;

    // Do what Carbon is telling us to do.
    if (desiredFocus == kControlFocusNoPart) {
        // Relinquish the keyboard focus.
        RelinquishFocus(view, true); // (autodisplay ? YES : NO));
        freshlyMadeFirstResponderView = nil;
        partCodeToReturn = kControlFocusNoPart;
        // NSLog(@"Relinquished the key focus because we have no choice.");
    } else if (desiredFocus == kControlFocusNextPart || desiredFocus == kControlFocusPrevPart) {
        BOOL goForward = (desiredFocus == kControlFocusNextPart);

        // Advance the keyboard focus, maybe right off of this view.  Maybe a subview of this one already has the keyboard focus, maybe not.
        freshlyMadeFirstResponderView = AdvanceFocus(view, goForward);
        if (freshlyMadeFirstResponderView)
            partCodeToReturn = desiredFocus;
        else
            partCodeToReturn = kControlFocusNoPart;
        // NSLog(freshlyMadeFirstResponderView ? @"Advanced the key focus." : @"Relinquished the key focus.");
    } else {
        // What's this?
        if (desiredFocus != kControlIndicatorPart)
            ASSERT_NOT_REACHED();
        freshlyMadeFirstResponderView = nil;
        partCodeToReturn = desiredFocus;
    }

    view->fFirstResponder = freshlyMadeFirstResponderView;

    *actualFocus = partCodeToReturn;

    // Done.
    return noErr;
}

//----------------------------------------------------------------------------------
// AdvanceFocus
//----------------------------------------------------------------------------------
//
static NSView* AdvanceFocus(HIWebView* view, bool forward)
{
    NSResponder* oldFirstResponder;
    NSView* currentKeyView;
    NSView* viewWeMadeFirstResponder;
    
    // Focus on some part (subview) of this control (view).
    // Maybe a subview of this one already has the keyboard focus, maybe not.
    
    oldFirstResponder = [view->fKitWindow firstResponder];

    // If we tab out of our NSView, it will no longer be the responder
    // when we get here. We'll try this trick for now. We might need to
    // tag the view appropriately.

    if (view->fFirstResponder && ((NSResponder*)view->fFirstResponder != oldFirstResponder))
        return nullptr;
    
    if ([oldFirstResponder isKindOfClass:[NSView class]]) {
        NSView* tentativeNewKeyView;

        // Some view in this window already has the keyboard focus.  It better at least be a subview of this one.
        NSView* oldFirstResponderView = (NSView *)oldFirstResponder;
        ASSERT([oldFirstResponderView isDescendantOf:view->fWebView]);

        if (oldFirstResponderView != view->fFirstResponder && ![oldFirstResponderView isDescendantOf:view->fFirstResponder]) {
            // Despite our efforts to record what view we made the first responder
            // (for use in the next paragraph) we couldn't keep up because the user
            // has clicked in a text field to make it the key focus, instead of using
            // the tab key. Find a control on which it's reasonable to invoke
            // -[NSView nextValidKeyView], taking into account the fact that
            // NSTextFields always pass on first-respondership to a temporarily-
            // contained NSTextView.

            NSView *viewBeingTested;
            currentKeyView = oldFirstResponderView;
            viewBeingTested = currentKeyView;
            while (viewBeingTested != view->fWebView) {
                if ([viewBeingTested isKindOfClass:[NSTextField class]]) {
                    currentKeyView = viewBeingTested;
                    break;
                }
                viewBeingTested = [viewBeingTested superview];
            }
        } else {
            // We recorded which view we made into the first responder the
            // last time the user hit the tab key, and nothing has invalidated
            // our recorded value since.
            
            currentKeyView = view->fFirstResponder;
        }

        // Try to move on to the next or previous key view.  We use the laboriously
        // recorded/figured currentKeyView instead of just oldFirstResponder as the
        // jumping-off-point when searching for the next valid key view. This is so
        // we don't get fooled if we recently made some view the first responder, but
        // it passed on first-responder-ness to some temporary subview.
        //
        // You can't put normal views in a window with Carbon-control-wrapped views.
        // Stuff like this would break. M.P. Notice - 12/2/00

        tentativeNewKeyView = forward ? [currentKeyView nextValidKeyView] : [currentKeyView previousValidKeyView];
        if (tentativeNewKeyView && [tentativeNewKeyView isDescendantOf:view->fWebView]) {
            // The user has tabbed to another subview of this control view.  Change the keyboard focus.
            //NSLog(@"Tabbed to the next or previous key view.");

            [view->fKitWindow makeFirstResponder:tentativeNewKeyView];
            viewWeMadeFirstResponder = tentativeNewKeyView;
        } else {
            // The user has tabbed past the subviews of this control view.  The window is the first responder now.
            //NSLog(@"Tabbed past the first or last key view.");
            [view->fKitWindow makeFirstResponder:view->fKitWindow];
            viewWeMadeFirstResponder = nil;
        }
    } else {
        // No view in this window has the keyboard focus.  This view should
        // try to select one of its key subviews. We're not interested in
        // the subviews of sibling views here.

        // NSLog(@"No keyboard focus in window. Attempting to set...");

        NSView *tentativeNewKeyView;
        ASSERT(oldFirstResponder == view->fKitWindow);
        if ([view->fWebView acceptsFirstResponder])
            tentativeNewKeyView = view->fWebView;
        else
            tentativeNewKeyView = [view->fWebView nextValidKeyView];
        if (tentativeNewKeyView && [tentativeNewKeyView isDescendantOf:view->fWebView]) {
            // This control view has at least one subview that can take the keyboard focus.
            if (!forward) {
                // The user has tabbed into this control view backwards. Find
                // and select the last subview of this one that can take the
                // keyboard focus. Watch out for loops of valid key views.

                NSView *firstTentativeNewKeyView = tentativeNewKeyView;
                NSView *nextTentativeNewKeyView = [tentativeNewKeyView nextValidKeyView];
                while (nextTentativeNewKeyView && [nextTentativeNewKeyView isDescendantOf:view->fWebView] && nextTentativeNewKeyView != firstTentativeNewKeyView) {
                    tentativeNewKeyView = nextTentativeNewKeyView;
                    nextTentativeNewKeyView = [tentativeNewKeyView nextValidKeyView];
                }

            }

            // Set the keyboard focus.
            //NSLog(@"Tabbed into the first or last key view.");
            [view->fKitWindow makeFirstResponder:tentativeNewKeyView];
            viewWeMadeFirstResponder = tentativeNewKeyView;
        } else {
            // This control view has no subviews that can take the keyboard focus.
            //NSLog(@"Can't tab into this view.");
            viewWeMadeFirstResponder = nil;
        }
    }

    // Done.
    return viewWeMadeFirstResponder;
}


//----------------------------------------------------------------------------------
// RelinquishFocus
//----------------------------------------------------------------------------------
//
static void RelinquishFocus(HIWebView* view, bool inAutodisplay)
{
    NSResponder* firstResponder;

    // Apparently Carbon thinks that some subview of this control view has the keyboard focus,
    // or we wouldn't be being asked to relinquish focus.

    firstResponder = [view->fKitWindow firstResponder];
    if ([firstResponder isKindOfClass:[NSView class]]) {
        // Some subview of this control view really is the first responder right now.
        ASSERT([(NSView *)firstResponder isDescendantOf:view->fWebView]);

        // Make the window the first responder, so that no view is the key view.
        [view->fKitWindow makeFirstResponder:view->fKitWindow];

        // If this control is not allowed to do autodisplay, don't let
        // it autodisplay any just-changed focus rings or text on the
        // next go around the event loop. I'm probably clearing more
        // dirty rects than I have to, but it doesn't seem to hurt in
        // the print panel accessory view case, and I don't have time
        // to figure out exactly what -[NSCell _setKeyboardFocusRingNeedsDisplay]
        // is doing when invoked indirectly from -makeFirstResponder up above. M.P. Notice - 12/4/00

        if (!inAutodisplay)
            [[view->fWebView opaqueAncestor] _clearDirtyRectsForTree];
    } else {
        // The Cocoa first responder does not correspond to the Carbon
        // control that has the keyboard focus. This can happen when
        // you've closed a dialog by hitting return in an NSTextView
        // that's a subview of this one; Cocoa closed the window, and
        // now Carbon is telling this control to relinquish the focus
        // as it's being disposed. There's nothing to do.

        ASSERT(firstResponder == view->fKitWindow);
    }
}

//----------------------------------------------------------------------------------
// ActiveStateChanged
//----------------------------------------------------------------------------------
//
static void ActiveStateChanged(HIWebView* view)
{
IGNORE_WARNINGS_BEGIN("undeclared-selector")
    if ([view->fWebView respondsToSelector:@selector(setEnabled)]) {
        [(NSControl*)view->fWebView setEnabled: IsControlEnabled(view->fViewRef)];
        HIViewSetNeedsDisplay(view->fViewRef, true);
    }
IGNORE_WARNINGS_END
}


//----------------------------------------------------------------------------------
// ProcessCommand
//----------------------------------------------------------------------------------
//
static OSStatus ProcessCommand(HIWebView* inView, const HICommand* inCommand)
{
    OSStatus result = eventNotHandledErr;
    NSResponder* resp;
    
    resp = [inView->fKitWindow firstResponder];

    if ([resp isKindOfClass:[NSView class]]) {
        NSView* respView = (NSView*)resp;

        if (respView == inView->fWebView || [respView isDescendantOf: inView->fWebView]) {
            switch (inCommand->commandID) {
            case kHICommandCut:
            case kHICommandCopy:
            case kHICommandPaste:
            case kHICommandClear:
            case kHICommandSelectAll: {
                SEL selector = _NSSelectorForHICommand(inCommand);
                if ([respView respondsToSelector:selector]) {
                    [respView performSelector:selector withObject:nil];
                    result = noErr;
                }
                break;
            }
            }
        }
    }
    
    return result;
}

//----------------------------------------------------------------------------------
// UpdateCommandStatus
//----------------------------------------------------------------------------------
//
static OSStatus
UpdateCommandStatus(HIWebView* inView, const HICommand* inCommand)
{
    OSStatus result = eventNotHandledErr;
    MenuItemProxy* proxy = nullptr;
    NSResponder* resp;
    
    resp = [inView->fKitWindow firstResponder];
    
    if ([resp isKindOfClass:[NSView class]]) {
        NSView* respView = (NSView*)resp;

        if ((respView == inView->fWebView || [respView isDescendantOf: inView->fWebView]) && inCommand->attributes & kHICommandFromMenu) {
            SEL selector = _NSSelectorForHICommand(inCommand);
    
            if (selector && [resp respondsToSelector: selector]) {
                proxy = [[MenuItemProxy alloc] initWithAction: selector];
                    
                // Can't use -performSelector:withObject: here because the method we're calling returns BOOL, while
                // -performSelector:withObject:'s return value is assumed to be an id.
                if (wtfObjcMsgSend<BOOL>(resp, @selector(validateUserInterfaceItem:), proxy))
                    EnableMenuItem(inCommand->menu.menuRef, inCommand->menu.menuItemIndex);
                else
                    DisableMenuItem(inCommand->menu.menuRef, inCommand->menu.menuItemIndex);
                    
                result = noErr;
            }
        }
    }
    
    if (proxy)
        [proxy release];

    return result;
}

// Blatantly stolen from AppKit and cropped a bit

//----------------------------------------------------------------------------------
// _NSSelectorForHICommand
//----------------------------------------------------------------------------------
//
static SEL _NSSelectorForHICommand(const HICommand* inCommand)
{
IGNORE_WARNINGS_BEGIN("undeclared-selector")
    switch (inCommand->commandID) {
    case kHICommandUndo:
        return @selector(undo:);
    case kHICommandRedo:
        return @selector(redo:);
    case kHICommandCut:
        return @selector(cut:);
    case kHICommandCopy:
        return @selector(copy:);
    case kHICommandPaste:
        return @selector(paste:);
    case kHICommandClear:
        return @selector(delete:);
    case kHICommandSelectAll:
        return @selector(selectAll:);
    default:
        return nullptr;
    }
IGNORE_WARNINGS_END

    return nullptr;
}


//-----------------------------------------------------------------------------------
//    HIWebViewEventHandler
//-----------------------------------------------------------------------------------
//    Our object's virtual event handler method. I'm not sure if we need this these days.
//    We used to do various things with it, but those days are long gone...
//
static OSStatus
HIWebViewEventHandler(EventHandlerCallRef inCallRef, EventRef inEvent, void *inUserData)
{
    OSStatus result = eventNotHandledErr;
    HIPoint where;
    OSType tag;
    void *ptr;
    Size size;
    UInt32 features;
    RgnHandle region = nullptr;
    ControlPartCode part;
    HIWebView* view = (HIWebView*)inUserData;

    // [NSApp setWindowsNeedUpdate:YES] must be called before events so that ActivateTSMDocument is called to set an active document. 
    // Without an active document, TSM will use a default document which uses a bottom-line input window which we don't want.
    [NSApp setWindowsNeedUpdate:YES];
        
    switch (GetEventClass(inEvent)) {
    case kEventClassHIObject:
        switch (GetEventKind(inEvent)) {
        case kEventHIObjectConstruct:
            HIObjectRef object;

            result = GetEventParameter(inEvent, kEventParamHIObjectInstance,
            typeHIObjectRef, nullptr, sizeof(HIObjectRef), nullptr, &object);
            ASSERT(!result);
            if (result)
                return result;

            // on entry for our construct event, we're passed the
            // creation proc we registered with for this class.
            // we use it now to create the instance, and then we
            // replace the instance parameter data with said instance
            // as type void.

            view = HIWebViewConstructor((HIViewRef)object);

            if (view)
                SetEventParameter(inEvent, kEventParamHIObjectInstance, typeVoidPtr, sizeof(void *), &view); 
            break;
                
        case kEventHIObjectDestruct:
            HIWebViewDestructor(view);
            // result is unimportant
            break;
        }
        break;

    case kEventClassKeyboard: {
        NSEvent* kitEvent = CreateNSEventWithCarbonEvent(inEvent);
        [view->fKitWindow sendSuperEvent:kitEvent];
        [kitEvent release];
        result = noErr;
        break;
    }

    case kEventClassMouse:
        switch (GetEventKind(inEvent)) {
        case kEventMouseUp:
            result = MouseUp(view, inEvent);
            break;
        
        case kEventMouseWheelMoved:
            result = MouseWheelMoved(view, inEvent);
            break;

        case kEventMouseMoved:
            result = MouseMoved(view, inEvent);
            break;

        case kEventMouseDragged:
            result = MouseDragged(view, inEvent);
            break;
        }
        break;

    case kEventClassCommand:
        HICommand command;
        
        result = GetEventParameter(inEvent, kEventParamDirectObject, typeHICommand, nullptr, sizeof(HICommand), nullptr, &command);
        ASSERT(!result);
        if (result)
            return result;

        switch (GetEventKind(inEvent)) {
        case kEventCommandProcess:
            result = ProcessCommand(view, &command);
            break;
    
        case kEventCommandUpdateStatus:
            result = UpdateCommandStatus(view, &command);
            break;
        }
        break;

    case kEventClassControl:
        switch (GetEventKind(inEvent)) {
        case kEventControlInitialize:
            features = GetBehaviors();
            SetEventParameter(inEvent, kEventParamControlFeatures, typeUInt32, sizeof(UInt32), &features);
            result = noErr;
            break;

        case kEventControlDraw: {
            CGContextRef context = nullptr;

            GetEventParameter(inEvent, kEventParamRgnHandle, typeQDRgnHandle, nullptr, sizeof(RgnHandle), nullptr, &region);
            GetEventParameter(inEvent, kEventParamCGContextRef, typeCGContextRef, nullptr, sizeof(CGContextRef), nullptr, &context);

            Draw(view, region, context);
            result = noErr;
            break;
        }

        case kEventControlHitTest:
            GetEventParameter(inEvent, kEventParamMouseLocation, typeHIPoint, nullptr, sizeof(HIPoint), nullptr, &where);
            part = HitTest(view, &where);
            SetEventParameter(inEvent, kEventParamControlPart, typeControlPartCode, sizeof(ControlPartCode), &part);
            result = noErr;
            break;

        case kEventControlGetPartRegion:
            GetEventParameter(inEvent, kEventParamControlPart, typeControlPartCode, nullptr, sizeof(ControlPartCode), nullptr, &part);
            GetEventParameter(inEvent, kEventParamControlRegion, typeQDRgnHandle, nullptr, sizeof(RgnHandle), nullptr, &region);
            result = GetRegion(view, part, region);
            break;
                
        case kEventControlGetData: {
            GetEventParameter(inEvent, kEventParamControlPart, typeControlPartCode, nullptr, sizeof(ControlPartCode), nullptr, &part);
            GetEventParameter(inEvent, kEventParamControlDataTag, typeEnumeration, nullptr, sizeof(OSType), nullptr, &tag);
            GetEventParameter(inEvent, kEventParamControlDataBuffer, typePtr, nullptr, sizeof(Ptr), nullptr, &ptr);
            GetEventParameter(inEvent, kEventParamControlDataBufferSize, typeByteCount, nullptr, sizeof(Size), nullptr, &size);

            if (tag == kControlKindTag) {
                Size outSize;
                result = noErr;

                if (ptr) {
                    if (size != sizeof(ControlKind))
                        result = errDataSizeMismatch;
                    else
                        (*(ControlKind *)ptr) = GetKind();
                }

                outSize = sizeof(ControlKind);
                SetEventParameter(inEvent, kEventParamControlDataBufferSize, typeByteCount, sizeof(Size), &outSize);
            }
            break;
        }

        case kEventControlBoundsChanged: {
            HIRect prevRect, currRect;
            UInt32 attrs;
        
            GetEventParameter(inEvent, kEventParamAttributes, typeUInt32, nullptr, sizeof(UInt32), nullptr, &attrs);
            GetEventParameter(inEvent, kEventParamOriginalBounds, typeHIRect, nullptr, sizeof(HIRect), nullptr, &prevRect);
            GetEventParameter(inEvent, kEventParamCurrentBounds, typeHIRect, nullptr, sizeof(HIRect), nullptr, &currRect);

            BoundsChanged(view, attrs, &prevRect, &currRect);
            result = noErr;
            break;
        }

        case kEventControlActivate:
            ActiveStateChanged(view);
            result = noErr;
            break;

        case kEventControlDeactivate:
            ActiveStateChanged(view);
            result = noErr;
            break;
        
        case kEventControlOwningWindowChanged: {
            WindowRef fromWindow, toWindow;

            result = GetEventParameter(inEvent, kEventParamControlOriginalOwningWindow, typeWindowRef, nullptr, sizeof(WindowRef), nullptr, &fromWindow);
            ASSERT(!result);
            if (result)
                return result;

            result = GetEventParameter(inEvent, kEventParamControlCurrentOwningWindow, typeWindowRef, nullptr, sizeof(WindowRef), nullptr, &toWindow);
            ASSERT(!result);
            if (result)
                return result;

            OwningWindowChanged(view, fromWindow, toWindow);
            result = noErr;
            break;
        }

        case kEventControlClick:
            result = Click(view, inEvent);
            break;
                                    
        case kEventControlContextualMenuClick:
            result = ContextMenuClick(view, inEvent);
            break;
                                    
        case kEventControlSetFocusPart: {
            ControlPartCode desiredFocus;
            RgnHandle invalidRgn;
            Boolean focusEverything;
            ControlPartCode actualFocus;
            
            result = GetEventParameter(inEvent, kEventParamControlPart, typeControlPartCode, nullptr, sizeof(ControlPartCode), nullptr, &desiredFocus); 
            ASSERT(!result);
            if (result)
                return result;

            GetEventParameter(inEvent, kEventParamControlInvalRgn, typeQDRgnHandle, nullptr, sizeof(RgnHandle), nullptr, &invalidRgn);
            focusEverything = false; // a good default in case the parameter doesn't exist
            GetEventParameter(inEvent, kEventParamControlFocusEverything, typeBoolean, nullptr, sizeof(Boolean), nullptr, &focusEverything);

            result = SetFocusPart(view, desiredFocus, invalidRgn, focusEverything, &actualFocus);
            if (result == noErr)
                ASSERT(!SetEventParameter(inEvent, kEventParamControlPart, typeControlPartCode, sizeof(ControlPartCode), &actualFocus));
            break;
        }

        // some other kind of Control event
        default:
            break;
        }
        break;

    // some other event class
    default:
        break;
    }
    return result;
}


static void UpdateObserver(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info);

static void StartUpdateObserver(HIWebView* view)
{
    CFRunLoopObserverContext context;
    CFRunLoopObserverRef observer;
    CFRunLoopRef mainRunLoop;
    
    ASSERT(!view->fIsComposited);
    ASSERT(!view->fUpdateObserver);

    context.version = 0;
    context.info = view;
    context.retain = nullptr;
    context.release = nullptr;
    context.copyDescription = nullptr;

    mainRunLoop = checked_cf_cast<CFRunLoopRef>(GetCFRunLoopFromEventLoop(GetMainEventLoop()));
    observer = CFRunLoopObserverCreate(nullptr, kCFRunLoopEntry | kCFRunLoopBeforeWaiting, true, 0, UpdateObserver, &context);
    CFRunLoopAddObserver(mainRunLoop, observer, kCFRunLoopCommonModes); 

    view->fUpdateObserver = observer;
    
    // printf("Update observer started\n");
}

static void StopUpdateObserver(HIWebView* view)
{
    ASSERT(!view->fIsComposited);
    ASSERT(view->fUpdateObserver);

    CFRunLoopObserverInvalidate(view->fUpdateObserver);
    CFRelease(view->fUpdateObserver);
    view->fUpdateObserver = nullptr;

    // printf("Update observer removed\n");
}

static void 
UpdateObserver(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info)
{
    HIWebView* view = (HIWebView*)info;
    RgnHandle region = NewRgn();

    // printf("Update observer called\n");

    if (region) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        GetWindowRegion(GetControlOwner(view->fViewRef), kWindowUpdateRgn, region);
        ALLOW_DEPRECATED_DECLARATIONS_END
        
        if (!EmptyRgn(region)) {
            RgnHandle ourRgn = NewRgn();
            Rect rect;
            
            GetWindowBounds(GetControlOwner(view->fViewRef), kWindowStructureRgn, &rect);
            
            // printf("Update region is non-empty\n");
            if (ourRgn) {
                Rect rect;
                GrafPtr savePort, port;
                Point offset = { 0, 0 };
                
                port = GetWindowPort(GetControlOwner(view->fViewRef));
                
                GetPort(&savePort);
                SetPort(port);
                
                GlobalToLocal(&offset);
                OffsetRgn(region, offset.h, offset.v);

                GetControlBounds(view->fViewRef, &rect);
                RectRgn(ourRgn, &rect);
                
                // printf("our control is at %d %d %d %d\n", rect.top, rect.left, rect.bottom, rect.right);
                GetRegionBounds(region, &rect);
                // printf("region is at %d %d %d %d\n", rect.top, rect.left, rect.bottom, rect.right);

                SectRgn(ourRgn, region, ourRgn);
                
                GetRegionBounds(ourRgn, &rect);
                // printf("intersection is  %d %d %d %d\n", rect.top, rect.left, rect.bottom, rect.right);
                if (!EmptyRgn(ourRgn)) {
                    RgnHandle saveVis = NewRgn();
                    
                    // printf("looks like we should draw\n");
                    if (saveVis) {
                        // RGBColor kRedColor = { 0xffff, 0, 0 };
                        GetPortVisibleRegion(GetWindowPort(GetControlOwner(view->fViewRef)), saveVis);
                        SetPortVisibleRegion(GetWindowPort(GetControlOwner(view->fViewRef)), ourRgn);
                        
                        // RGBForeColor(&kRedColor);
                        // PaintRgn(ourRgn);
                        // QDFlushPortBuffer(port, nullptr);
                        // Delay(15, nullptr);
                        Draw1Control(view->fViewRef);
                        ValidWindowRgn(GetControlOwner(view->fViewRef), ourRgn);
                        
                        SetPortVisibleRegion(GetWindowPort(GetControlOwner(view->fViewRef)), saveVis);
                        DisposeRgn(saveVis);
                    }
                }

                SetPort(savePort);
                
                DisposeRgn(ourRgn);
            }
        }
        
        DisposeRgn(region);
    }
}

#endif
