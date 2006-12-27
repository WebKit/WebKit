/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#import "WebBaseNetscapePluginView.h"

#import "WebDataSourceInternal.h"
#import "WebDefaultUIDelegate.h"
#import "WebFrameBridge.h"
#import "WebFrameInternal.h" 
#import "WebFrameView.h"
#import "WebGraphicsExtras.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebNSDataExtras.h"
#import "WebNSDictionaryExtras.h"
#import "WebNSObjectExtras.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebNSViewExtras.h"
#import "WebNetscapePluginPackage.h"
#import "WebNetscapePluginStream.h"
#import "WebNullPluginView.h"
#import "WebPreferences.h"
#import "WebViewInternal.h"
#import <Carbon/Carbon.h>
#import <JavaScriptCore/Assertions.h>
#import <JavaScriptCore/npruntime_impl.h>
#import <WebCore/FrameLoader.h> 
#import <WebCore/FrameMac.h> 
#import <WebCore/FrameTree.h> 
#import <WebCore/Page.h> 
#import <WebKit/DOMPrivate.h>
#import <WebKit/WebUIDelegate.h>
#import <WebKitSystemInterface.h>
#import <objc/objc-runtime.h>

using namespace WebCore;

// Send null events 50 times a second when active, so plug-ins like Flash get high frame rates.
#define NullEventIntervalActive         0.02
#define NullEventIntervalNotActive      0.25

#define LoginWindowDidSwitchFromUserNotification    @"WebLoginWindowDidSwitchFromUserNotification"
#define LoginWindowDidSwitchToUserNotification      @"WebLoginWindowDidSwitchToUserNotification"

@interface WebBaseNetscapePluginView (Internal)
- (void)_viewHasMoved;
- (NPError)_createPlugin;
- (void)_destroyPlugin;
- (NSBitmapImageRep *)_printedPluginBitmap;
- (BOOL)_createAGLContextIfNeeded;
- (BOOL)_createWindowedAGLContext;
- (BOOL)_createWindowlessAGLContext;
- (CGLContextObj)_cglContext;
- (BOOL)_getAGLOffscreenBuffer:(GLvoid **)outBuffer width:(GLsizei *)outWidth height:(GLsizei *)outHeight;
- (void)_destroyAGLContext;
- (void)_reshapeAGLWindow;
- (void)_hideAGLWindow;
- (NSImage *)_aglOffscreenImageForDrawingInRect:(NSRect)drawingInRect;
@end

static WebBaseNetscapePluginView *currentPluginView = nil;

typedef struct OpaquePortState* PortState;

#ifndef NP_NO_QUICKDRAW

// QuickDraw is not available in 64-bit

typedef struct {
    GrafPtr oldPort;
    Point oldOrigin;
    RgnHandle oldClipRegion;
    RgnHandle oldVisibleRegion;
    RgnHandle clipRegion;
    BOOL forUpdate;
} PortState_QD;

#endif /* NP_NO_QUICKDRAW */

typedef struct {
    CGContextRef context;
} PortState_CG;

typedef struct {
    AGLContext oldContext;
} PortState_GL;

@interface WebPluginRequest : NSObject
{
    NSURLRequest *_request;
    NSString *_frameName;
    void *_notifyData;
    BOOL _didStartFromUserGesture;
    BOOL _sendNotification;
}

- (id)initWithRequest:(NSURLRequest *)request frameName:(NSString *)frameName notifyData:(void *)notifyData sendNotification:(BOOL)sendNotification didStartFromUserGesture:(BOOL)currentEventIsUserGesture;

- (NSURLRequest *)request;
- (NSString *)frameName;
- (void *)notifyData;
- (BOOL)isCurrentEventUserGesture;
- (BOOL)sendNotification;

@end

@interface NSData (WebPluginDataExtras)
- (BOOL)_web_startsWithBlankLine;
- (WebNSInteger)_web_locationAfterFirstBlankLine;
@end

static OSStatus TSMEventHandler(EventHandlerCallRef inHandlerRef, EventRef inEvent, void *pluginView);

@interface WebBaseNetscapePluginView (ForwardDeclarations)
- (void)setWindowIfNecessary;
@end

@implementation WebBaseNetscapePluginView

+ (void)initialize
{
    WKSendUserChangeNotifications();
}

#pragma mark EVENTS

+ (void)getCarbonEvent:(EventRecord *)carbonEvent
{
    carbonEvent->what = nullEvent;
    carbonEvent->message = 0;
    carbonEvent->when = TickCount();
    GetGlobalMouse(&carbonEvent->where);
    carbonEvent->where.h = static_cast<short>(carbonEvent->where.h * HIGetScaleFactor());
    carbonEvent->where.v = static_cast<short>(carbonEvent->where.v * HIGetScaleFactor());
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
    if (WKConvertNSEventToCarbonEvent(carbonEvent, cocoaEvent)) {
        carbonEvent->where.h = static_cast<short>(carbonEvent->where.h * HIGetScaleFactor());
        carbonEvent->where.v = static_cast<short>(carbonEvent->where.v * HIGetScaleFactor());
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

- (BOOL)superviewsHaveSuperviews
{
    NSView *contentView = [[self window] contentView];
    NSView *view;
    for (view = self; view != nil; view = [view superview]) { 
        if (view == contentView) {
            return YES;
        }
    }
    return NO;
}

#ifndef NP_NO_QUICKDRAW
// The WindowRef created by -[NSWindow windowRef] has a QuickDraw GrafPort that covers 
// the entire window frame (or structure region to use the Carbon term) rather then just the window content.
// We can remove this when <rdar://problem/4201099> is fixed.
- (void)fixWindowPort
{
    ASSERT(drawingModel == NPDrawingModelQuickDraw);
    
    NSWindow *currentWindow = [self currentWindow];
    if ([currentWindow isKindOfClass:objc_getClass("NSCarbonWindow")])
        return;
    
    float windowHeight = [currentWindow frame].size.height;
    NSView *contentView = [currentWindow contentView];
    NSRect contentRect = [contentView convertRect:[contentView frame] toView:nil]; // convert to window-relative coordinates
    
    CGrafPtr oldPort;
    GetPort(&oldPort);    
    SetPort(GetWindowPort((WindowRef)[currentWindow windowRef]));
    
    MovePortTo(static_cast<short>(contentRect.origin.x), /* Flip Y */ static_cast<short>(windowHeight - NSMaxY(contentRect)));
    PortSize(static_cast<short>(contentRect.size.width), static_cast<short>(contentRect.size.height));
    
    SetPort(oldPort);
}
#endif

- (PortState)saveAndSetNewPortStateForUpdate:(BOOL)forUpdate
{
    ASSERT([self currentWindow] != nil);

#ifndef NP_NO_QUICKDRAW
    // If drawing with QuickDraw, fix the window port so that it has the same bounds as the NSWindow's
    // content view.  This makes it easier to convert between AppKit view and QuickDraw port coordinates.
    if (drawingModel == NPDrawingModelQuickDraw)
        [self fixWindowPort];
#endif
    
    WindowRef windowRef = (WindowRef)[[self currentWindow] windowRef];
    ASSERT(windowRef);
    
    // Use AppKit to convert view coordinates to NSWindow coordinates.
    NSRect boundsInWindow = [self convertRect:[self bounds] toView:nil];
    NSRect visibleRectInWindow = [self convertRect:[self visibleRect] toView:nil];
    
    // Flip Y to convert NSWindow coordinates to top-left-based window coordinates.
    float borderViewHeight = [[self currentWindow] frame].size.height;
    boundsInWindow.origin.y = borderViewHeight - NSMaxY(boundsInWindow);
    visibleRectInWindow.origin.y = borderViewHeight - NSMaxY(visibleRectInWindow);
    
#ifndef NP_NO_QUICKDRAW
    // Look at the Carbon port to convert top-left-based window coordinates into top-left-based content coordinates.
    if (drawingModel == NPDrawingModelQuickDraw) {
        Rect portBounds;
        CGrafPtr port = GetWindowPort(windowRef);
        GetPortBounds(port, &portBounds);

        PixMap *pix = *GetPortPixMap(port);
        boundsInWindow.origin.x += pix->bounds.left - portBounds.left;
        boundsInWindow.origin.y += pix->bounds.top - portBounds.top;
        visibleRectInWindow.origin.x += pix->bounds.left - portBounds.left;
        visibleRectInWindow.origin.y += pix->bounds.top - portBounds.top;
    }
#endif
    
    window.x = (int32)boundsInWindow.origin.x; 
    window.y = (int32)boundsInWindow.origin.y;
    window.width = static_cast<uint32>(NSWidth(boundsInWindow));
    window.height = static_cast<uint32>(NSHeight(boundsInWindow));
    
    // "Clip-out" the plug-in when:
    // 1) it's not really in a window or off-screen or has no height or width.
    // 2) window.x is a "big negative number" which is how WebCore expresses off-screen widgets.
    // 3) the window is miniaturized or the app is hidden
    // 4) we're inside of viewWillMoveToWindow: with a nil window. In this case, superviews may already have nil 
    // superviews and nil windows and results from convertRect:toView: are incorrect.
    NSWindow *realWindow = [self window];
    if (window.width <= 0 || window.height <= 0 || window.x < -100000
            || realWindow == nil || [realWindow isMiniaturized]
            || [NSApp isHidden]
            || ![self superviewsHaveSuperviews]
            || [self isHiddenOrHasHiddenAncestor]) {
        // The following code tries to give plug-ins the same size they will eventually have.
        // The specifiedWidth and specifiedHeight variables are used to predict the size that
        // WebCore will eventually resize us to.

        // The QuickTime plug-in has problems if you give it a width or height of 0.
        // Since other plug-ins also might have the same sort of trouble, we make sure
        // to always give plug-ins a size other than 0,0.

        if (window.width <= 0) {
            window.width = specifiedWidth > 0 ? specifiedWidth : 100;
        }
        if (window.height <= 0) {
            window.height = specifiedHeight > 0 ? specifiedHeight : 100;
        }

        window.clipRect.bottom = window.clipRect.top;
        window.clipRect.left = window.clipRect.right;
    } else {
        window.clipRect.top = (uint16)visibleRectInWindow.origin.y;
        window.clipRect.left = (uint16)visibleRectInWindow.origin.x;
        window.clipRect.bottom = (uint16)(visibleRectInWindow.origin.y + visibleRectInWindow.size.height);
        window.clipRect.right = (uint16)(visibleRectInWindow.origin.x + visibleRectInWindow.size.width);        
    }
    
    // Save the port state, set up the port for entry into the plugin
    PortState portState;
    switch (drawingModel) {
#ifndef NP_NO_QUICKDRAW
        case NPDrawingModelQuickDraw:
        {
            // Set up NS_Port.
            Rect portBounds;
            CGrafPtr port = GetWindowPort(windowRef);
            GetPortBounds(port, &portBounds);
            nPort.qdPort.port = port;
            nPort.qdPort.portx = (int32)-boundsInWindow.origin.x;
            nPort.qdPort.porty = (int32)-boundsInWindow.origin.y;
            window.window = &nPort;

            PortState_QD *qdPortState = (PortState_QD*)malloc(sizeof(PortState_QD));
            portState = (PortState)qdPortState;
            
            GetPort(&qdPortState->oldPort);    

            qdPortState->oldOrigin.h = portBounds.left;
            qdPortState->oldOrigin.v = portBounds.top;

            qdPortState->oldClipRegion = NewRgn();
            GetPortClipRegion(port, qdPortState->oldClipRegion);
            
            qdPortState->oldVisibleRegion = NewRgn();
            GetPortVisibleRegion(port, qdPortState->oldVisibleRegion);
            
            RgnHandle clipRegion = NewRgn();
            qdPortState->clipRegion = clipRegion;
            
            MacSetRectRgn(clipRegion,
                window.clipRect.left + nPort.qdPort.portx, window.clipRect.top + nPort.qdPort.porty,
                window.clipRect.right + nPort.qdPort.portx, window.clipRect.bottom + nPort.qdPort.porty);
            
            // Clip to dirty region so plug-in does not draw over already-drawn regions of the window that are
            // not going to be redrawn this update.  This forces plug-ins to play nice with z-index ordering.
            if (forUpdate) {
                RgnHandle viewClipRegion = NewRgn();
                
                // Get list of dirty rects from the opaque ancestor -- WebKit does some tricks with invalidation and
                // display to enable z-ordering for NSViews; a side-effect of this is that only the WebHTMLView
                // knows about the true set of dirty rects.
                NSView *opaqueAncestor = [self opaqueAncestor];
                const NSRect *dirtyRects;
                int dirtyRectCount, dirtyRectIndex;
                [opaqueAncestor getRectsBeingDrawn:&dirtyRects count:&dirtyRectCount];

                for (dirtyRectIndex = 0; dirtyRectIndex < dirtyRectCount; dirtyRectIndex++) {
                    NSRect dirtyRect = [self convertRect:dirtyRects[dirtyRectIndex] fromView:opaqueAncestor];
                    if (!NSEqualSizes(dirtyRect.size, NSZeroSize)) {
                        // Create a region for this dirty rect
                        RgnHandle dirtyRectRegion = NewRgn();
                        SetRectRgn(dirtyRectRegion, static_cast<short>(NSMinX(dirtyRect)), static_cast<short>(NSMinY(dirtyRect)), static_cast<short>(NSMaxX(dirtyRect)), static_cast<short>(NSMaxY(dirtyRect)));
                        
                        // Union this dirty rect with the rest of the dirty rects
                        UnionRgn(viewClipRegion, dirtyRectRegion, viewClipRegion);
                        DisposeRgn(dirtyRectRegion);
                    }
                }
            
                // Intersect the dirty region with the clip region, so that we only draw over dirty parts
                SectRgn(clipRegion, viewClipRegion, clipRegion);
                DisposeRgn(viewClipRegion);
            }
            
            qdPortState->forUpdate = forUpdate;
            
            // Switch to the port and set it up.
            SetPort(port);

            PenNormal();
            ForeColor(blackColor);
            BackColor(whiteColor);
            
            SetOrigin(nPort.qdPort.portx, nPort.qdPort.porty);

            SetPortClipRegion(nPort.qdPort.port, clipRegion);

            if (forUpdate) {
                // AppKit may have tried to help us by doing a BeginUpdate.
                // But the invalid region at that level didn't include AppKit's notion of what was not valid.
                // We reset the port's visible region to counteract what BeginUpdate did.
                SetPortVisibleRegion(nPort.qdPort.port, clipRegion);

                // Some plugins do their own BeginUpdate/EndUpdate.
                // For those, we must make sure that the update region contains the area we want to draw.
                InvalWindowRgn(windowRef, clipRegion);
            }
        }
        break;
#endif /* NP_NO_QUICKDRAW */
        
        case NPDrawingModelCoreGraphics:
        {            
            // A CoreGraphics plugin's window may only be set while the plugin view is being updated
            ASSERT(forUpdate && [NSView focusView] == self);

            PortState_CG *cgPortState = (PortState_CG *)malloc(sizeof(PortState_CG));
            portState = (PortState)cgPortState;
            cgPortState->context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
            
            // Update the plugin's window/context
            nPort.cgPort.window = windowRef;
            nPort.cgPort.context = cgPortState->context;
            window.window = &nPort.cgPort;

            // Save current graphics context's state; will be restored by -restorePortState:
            CGContextSaveGState(nPort.cgPort.context);
            
            // FIXME (4544971): Clip to dirty region when updating in "windowless" mode (transparent), like in the QD case
        }
        break;
        
        case NPDrawingModelOpenGL:
        {
            // An OpenGL plugin's window may only be set while the plugin view is being updated
            ASSERT(forUpdate && [NSView focusView] == self);

            // Clear the "current" window and context -- they will be assigned below (if all goes well)
            nPort.aglPort.window = NULL;
            nPort.aglPort.context = NULL;
            
            // Create AGL context if needed
            if (![self _createAGLContextIfNeeded]) {
                LOG_ERROR("Could not create AGL context");
                return NULL;
            }
            
            // Update the plugin's window/context
            nPort.aglPort.window = windowRef;
            nPort.aglPort.context = [self _cglContext];
            window.window = &nPort.aglPort;
            
            // Save/set current AGL context
            PortState_GL *glPortState = (PortState_GL *)malloc(sizeof(PortState_GL));
            portState = (PortState)glPortState;
            glPortState->oldContext = aglGetCurrentContext();
            aglSetCurrentContext(aglContext);
            
            // Adjust viewport according to clip
            switch (window.type) {
                case NPWindowTypeWindow:
                    glViewport(static_cast<GLint>(NSMinX(boundsInWindow) - NSMinX(visibleRectInWindow)), static_cast<GLint>(NSMaxY(visibleRectInWindow) - NSMaxY(boundsInWindow)), window.width, window.height);
                break;
                
                case NPWindowTypeDrawable:
                {
                    GLsizei width, height;
                    if ([self _getAGLOffscreenBuffer:NULL width:&width height:&height])
                        glViewport(0, 0, width, height);
                }
                break;
                
                default:
                    ASSERT_NOT_REACHED();
                break;
            }
        }
        break;
        
        default:
            ASSERT_NOT_REACHED();
            portState = NULL;
        break;
    }
    
    return portState;
}

- (PortState)saveAndSetNewPortState
{
    return [self saveAndSetNewPortStateForUpdate:NO];
}

- (void)restorePortState:(PortState)portState
{
    ASSERT([self currentWindow]);
    ASSERT(portState);
    
    switch (drawingModel) {
#ifndef NP_NO_QUICKDRAW
        case NPDrawingModelQuickDraw:
        {
            PortState_QD *qdPortState = (PortState_QD *)portState;
            WindowRef windowRef = (WindowRef)[[self currentWindow] windowRef];
            CGrafPtr port = GetWindowPort(windowRef);
            if (qdPortState->forUpdate)
                ValidWindowRgn(windowRef, qdPortState->clipRegion);
            
            SetOrigin(qdPortState->oldOrigin.h, qdPortState->oldOrigin.v);

            SetPortClipRegion(port, qdPortState->oldClipRegion);
            if (qdPortState->forUpdate)
                SetPortVisibleRegion(port, qdPortState->oldVisibleRegion);

            DisposeRgn(qdPortState->oldClipRegion);
            DisposeRgn(qdPortState->oldVisibleRegion);
            DisposeRgn(qdPortState->clipRegion);

            SetPort(qdPortState->oldPort);
        }
        break;
#endif /* NP_NO_QUICKDRAW */
        
        case NPDrawingModelCoreGraphics:
        {
            ASSERT([NSView focusView] == self);
            ASSERT(((PortState_CG *)portState)->context == nPort.cgPort.context);
            CGContextRestoreGState(nPort.cgPort.context);
        }
        break;
        
        case NPDrawingModelOpenGL:
            aglSetCurrentContext(((PortState_GL *)portState)->oldContext);
        break;
        
        default:
            ASSERT_NOT_REACHED();
        break;
    }
}

- (BOOL)sendEvent:(EventRecord *)event
{
    if (![self window])
        return NO;
    ASSERT(event);
   
    // If at any point the user clicks or presses a key from within a plugin, set the 
    // currentEventIsUserGesture flag to true. This is important to differentiate legitimate 
    // window.open() calls;  we still want to allow those.  See rdar://problem/4010765
    if (event->what == mouseDown || event->what == keyDown || event->what == mouseUp || event->what == autoKey)
        currentEventIsUserGesture = YES;
    
    suspendKeyUpEvents = NO;
    
    if (!isStarted)
        return NO;

    ASSERT(NPP_HandleEvent);
    
    // Make sure we don't call NPP_HandleEvent while we're inside NPP_SetWindow.
    // We probably don't want more general reentrancy protection; we are really
    // protecting only against this one case, which actually comes up when
    // you first install the SVG viewer plug-in.
    if (inSetWindow)
        return NO;

    Frame* frame = core([self webFrame]);
    if (!frame)
        return NO;
    Page* page = frame->page();
    if (!page)
        return NO;

    bool wasDeferring = page->defersLoading();
    if (!wasDeferring)
        page->setDefersLoading(true);

    // Can only send updateEvt to CoreGraphics and OpenGL plugins when actually drawing
    ASSERT((drawingModel != NPDrawingModelCoreGraphics && drawingModel != NPDrawingModelOpenGL) || event->what != updateEvt || [NSView focusView] == self);
    
    BOOL updating = event->what == updateEvt;
    PortState portState;
    if ((drawingModel != NPDrawingModelCoreGraphics && drawingModel != NPDrawingModelOpenGL) || event->what == updateEvt) {
        // In CoreGraphics or OpenGL mode, the port state only needs to be saved/set when redrawing the plug-in view.  The plug-in is not
        // allowed to draw at any other time.
        portState = [self saveAndSetNewPortStateForUpdate:updating];
        
        // We may have changed the window, so inform the plug-in.
        [self setWindowIfNecessary];
    } else
        portState = NULL;
    
#if !defined(NDEBUG) && !defined(NP_NO_QUICKDRAW)
    // Draw green to help debug.
    // If we see any green we know something's wrong.
    // Note that PaintRect() only works for QuickDraw plugins; otherwise the current QD port is undefined.
    if (drawingModel == NPDrawingModelQuickDraw && !isTransparent && event->what == updateEvt) {
        ForeColor(greenColor);
        const Rect bigRect = { -10000, -10000, 10000, 10000 };
        PaintRect(&bigRect);
        ForeColor(blackColor);
    }
#endif
    
    // Temporarily retain self in case the plug-in view is released while sending an event. 
    [[self retain] autorelease];
    
    [self willCallPlugInFunction];
    BOOL acceptedEvent = NPP_HandleEvent(plugin, event);
    [self didCallPlugInFunction];
    
    currentEventIsUserGesture = NO;
    
    if (portState) {
        if ([self currentWindow])
            [self restorePortState:portState];
        free(portState);
    }

    if (!wasDeferring)
        page->setDefersLoading(false);
            
    return acceptedEvent;
}

- (void)sendActivateEvent:(BOOL)activate
{
    EventRecord event;
    
    [self getCarbonEvent:&event];
    event.what = activateEvt;
    WindowRef windowRef = (WindowRef)[[self window] windowRef];
    event.message = (unsigned long)windowRef;
    if (activate) {
        event.modifiers |= activeFlag;
    }
    
    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(PluginEvents, "NPP_HandleEvent(activateEvent): %d  isActive: %d", acceptedEvent, activate);
}

- (BOOL)sendUpdateEvent
{
    EventRecord event;
    
    [self getCarbonEvent:&event];
    event.what = updateEvt;
    WindowRef windowRef = (WindowRef)[[self window] windowRef];
    event.message = (unsigned long)windowRef;

    BOOL acceptedEvent = [self sendEvent:&event];

    LOG(PluginEvents, "NPP_HandleEvent(updateEvt): %d", acceptedEvent);

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
    ASSERT([self window]);
    
    if (nullEventTimer)
        [self stopNullEvents];
    
    if (!isStarted || [[self window] isMiniaturized])
        return;

    NSTimeInterval interval;

    // If the plugin is completely obscured (scrolled out of view, for example), then we will
    // send null events at a reduced rate.
    interval = !isCompletelyObscured ? NullEventIntervalActive : NullEventIntervalNotActive;    
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

- (void)installKeyEventHandler
{
    static const EventTypeSpec sTSMEvents[] =
    {
    { kEventClassTextInput, kEventTextInputUnicodeForKeyEvent }
    };
    
    if (!keyEventHandler) {
        InstallEventHandler(GetWindowEventTarget((WindowRef)[[self window] windowRef]),
                            NewEventHandlerUPP(TSMEventHandler),
                            GetEventTypeCount(sTSMEvents),
                            sTSMEvents,
                            self,
                            &keyEventHandler);
    }
}

- (void)removeKeyEventHandler
{
    if (keyEventHandler) {
        RemoveEventHandler(keyEventHandler);
        keyEventHandler = NULL;
    }
}

- (void)setHasFocus:(BOOL)flag
{
    if (hasFocus != flag) {
        hasFocus = flag;
        EventRecord event;
        [self getCarbonEvent:&event];
        BOOL acceptedEvent;
        if (hasFocus) {
            event.what = getFocusEvent;
            acceptedEvent = [self sendEvent:&event]; 
            LOG(PluginEvents, "NPP_HandleEvent(getFocusEvent): %d", acceptedEvent);
            [self installKeyEventHandler];
        } else {
            event.what = loseFocusEvent;
            acceptedEvent = [self sendEvent:&event]; 
            LOG(PluginEvents, "NPP_HandleEvent(loseFocusEvent): %d", acceptedEvent);
            [self removeKeyEventHandler];
        }
    }
}

- (BOOL)becomeFirstResponder
{
    [self setHasFocus:YES];
    return YES;
}

- (BOOL)resignFirstResponder
{
    [self setHasFocus:NO];    
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
    
    LOG(PluginEvents, "NPP_HandleEvent(mouseDown): %d pt.v=%d, pt.h=%d", acceptedEvent, event.where.v, event.where.h);
}

- (void)mouseUp:(NSEvent *)theEvent
{
    EventRecord event;
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = mouseUp;

    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(PluginEvents, "NPP_HandleEvent(mouseUp): %d pt.v=%d, pt.h=%d", acceptedEvent, event.where.v, event.where.h);
}

- (void)mouseEntered:(NSEvent *)theEvent
{
    EventRecord event;
    
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = adjustCursorEvent;

    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(PluginEvents, "NPP_HandleEvent(mouseEntered): %d", acceptedEvent);
}

- (void)mouseExited:(NSEvent *)theEvent
{
    EventRecord event;
        
    [self getCarbonEvent:&event withEvent:theEvent];
    event.what = adjustCursorEvent;

    BOOL acceptedEvent;
    acceptedEvent = [self sendEvent:&event]; 
    
    LOG(PluginEvents, "NPP_HandleEvent(mouseExited): %d", acceptedEvent);
    
    // Set cursor back to arrow cursor.
    [[NSCursor arrowCursor] set];
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    // Do nothing so that other responders don't respond to the drag that initiated in this view.
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

- (void)keyUp:(NSEvent *)theEvent
{
    WKSendKeyEventToTSM(theEvent);
    
    // TSM won't send keyUp events so we have to send them ourselves.
    // Only send keyUp events after we receive the TSM callback because this is what plug-in expect from OS 9.
    if (!suspendKeyUpEvents) {
        EventRecord event;
        
        [self getCarbonEvent:&event withEvent:theEvent];
        event.what = keyUp;
        
        if (event.message == 0) {
            event.message = [self keyMessageForEvent:theEvent];
        }
        
        [self sendEvent:&event];
    }
}

- (void)keyDown:(NSEvent *)theEvent
{
    suspendKeyUpEvents = YES;
    WKSendKeyEventToTSM(theEvent);
}

static OSStatus TSMEventHandler(EventHandlerCallRef inHandlerRef, EventRef inEvent, void *pluginView)
{    
    EventRef rawKeyEventRef;
    OSStatus status = GetEventParameter(inEvent, kEventParamTextInputSendKeyboardEvent, typeEventRef, NULL, sizeof(EventRef), NULL, &rawKeyEventRef);
    if (status != noErr) {
        LOG_ERROR("GetEventParameter failed with error: %d", status);
        return noErr;
    }
    
    // Two-pass read to allocate/extract Mac charCodes
    ByteCount numBytes;    
    status = GetEventParameter(rawKeyEventRef, kEventParamKeyMacCharCodes, typeChar, NULL, 0, &numBytes, NULL);
    if (status != noErr) {
        LOG_ERROR("GetEventParameter failed with error: %d", status);
        return noErr;
    }
    char *buffer = (char *)malloc(numBytes);
    status = GetEventParameter(rawKeyEventRef, kEventParamKeyMacCharCodes, typeChar, NULL, numBytes, NULL, buffer);
    if (status != noErr) {
        LOG_ERROR("GetEventParameter failed with error: %d", status);
        free(buffer);
        return noErr;
    }
    
    EventRef cloneEvent = CopyEvent(rawKeyEventRef);
    unsigned i;
    for (i = 0; i < numBytes; i++) {
        status = SetEventParameter(cloneEvent, kEventParamKeyMacCharCodes, typeChar, 1 /* one char code */, &buffer[i]);
        if (status != noErr) {
            LOG_ERROR("SetEventParameter failed with error: %d", status);
            free(buffer);
            return noErr;
        }
        
        EventRecord eventRec;
        if (ConvertEventRefToEventRecord(cloneEvent, &eventRec)) {
            BOOL acceptedEvent;
            acceptedEvent = [(WebBaseNetscapePluginView *)pluginView sendEvent:&eventRec];
            
            LOG(PluginEvents, "NPP_HandleEvent(keyDown): %d charCode:%c keyCode:%lu",
                acceptedEvent, (char) (eventRec.message & charCodeMask), (eventRec.message & keyCodeMask));
            
            // We originally thought that if the plug-in didn't accept this event,
            // we should pass it along so that keyboard scrolling, for example, will work.
            // In practice, this is not a good idea, because plug-ins tend to eat the event but return false.
            // MacIE handles each key event twice because of this, but we will emulate the other browsers instead.
        }
    }
    ReleaseEvent(cloneEvent);
    
    free(buffer);
    return noErr;
}

// Fake up command-modified events so cut, copy, paste and select all menus work.
- (void)sendModifierEventWithKeyCode:(int)keyCode character:(char)character
{
    EventRecord event;
    [self getCarbonEvent:&event];
    event.what = keyDown;
    event.modifiers |= cmdKey;
    event.message = keyCode << 8 | character;
    [self sendEvent:&event];
}

- (void)cut:(id)sender
{
    [self sendModifierEventWithKeyCode:7 character:'x'];
}

- (void)copy:(id)sender
{
    [self sendModifierEventWithKeyCode:8 character:'c'];
}

- (void)paste:(id)sender
{
    [self sendModifierEventWithKeyCode:9 character:'v'];
}

- (void)selectAll:(id)sender
{
    [self sendModifierEventWithKeyCode:0 character:'a'];
}

#pragma mark WEB_NETSCAPE_PLUGIN

- (BOOL)isNewWindowEqualToOldWindow
{
    if (window.x != lastSetWindow.x)
        return NO;
    if (window.y != lastSetWindow.y)
        return NO;
    if (window.width != lastSetWindow.width)
        return NO;
    if (window.height != lastSetWindow.height)
        return NO;
    if (window.clipRect.top != lastSetWindow.clipRect.top)
        return NO;
    if (window.clipRect.left != lastSetWindow.clipRect.left)
        return NO;
    if (window.clipRect.bottom  != lastSetWindow.clipRect.bottom)
        return NO;
    if (window.clipRect.right != lastSetWindow.clipRect.right)
        return NO;
    if (window.type != lastSetWindow.type)
        return NO;
    
    switch (drawingModel) {
#ifndef NP_NO_QUICKDRAW
        case NPDrawingModelQuickDraw:
            if (nPort.qdPort.portx != lastSetPort.qdPort.portx)
                return NO;
            if (nPort.qdPort.porty != lastSetPort.qdPort.porty)
                return NO;
            if (nPort.qdPort.port != lastSetPort.qdPort.port)
                return NO;
        break;
#endif /* NP_NO_QUICKDRAW */
            
        case NPDrawingModelCoreGraphics:
            if (nPort.cgPort.window != lastSetPort.cgPort.window)
                return NO;
            if (nPort.cgPort.context != lastSetPort.cgPort.context)
                return NO;
        break;
            
        case NPDrawingModelOpenGL:
            if (nPort.aglPort.window != lastSetPort.aglPort.window)
                return NO;
            if (nPort.aglPort.context != lastSetPort.aglPort.context)
                return NO;
        break;
        
        default:
            ASSERT_NOT_REACHED();
        break;
    }
    
    return YES;
}

- (void)updateAndSetWindow
{
    if (drawingModel == NPDrawingModelCoreGraphics || drawingModel == NPDrawingModelOpenGL) {
        // Can only update CoreGraphics and OpenGL plugins while redrawing the plugin view
        [self setNeedsDisplay:YES];
        return;
    }
    
    // Can't update the plugin if it has not started (or has been stopped)
    if (!isStarted)
        return;
        
    PortState portState = [self saveAndSetNewPortState];
    if (portState) {
        [self setWindowIfNecessary];
        [self restorePortState:portState];
        free(portState);
    }
}

- (void)setWindowIfNecessary
{
    if (!isStarted) {
        return;
    }
    
    if (![self isNewWindowEqualToOldWindow]) {        
        // Make sure we don't call NPP_HandleEvent while we're inside NPP_SetWindow.
        // We probably don't want more general reentrancy protection; we are really
        // protecting only against this one case, which actually comes up when
        // you first install the SVG viewer plug-in.
        NPError npErr;
        ASSERT(!inSetWindow);
        
        inSetWindow = YES;
        
        // A CoreGraphics or OpenGL plugin's window may only be set while the plugin is being updated
        ASSERT((drawingModel != NPDrawingModelCoreGraphics && drawingModel != NPDrawingModelOpenGL) || [NSView focusView] == self);
        
        [self willCallPlugInFunction];
        npErr = NPP_SetWindow(plugin, &window);
        [self didCallPlugInFunction];
        inSetWindow = NO;

#ifndef NDEBUG
        switch (drawingModel) {
#ifndef NP_NO_QUICKDRAW
            case NPDrawingModelQuickDraw:
                LOG(Plugins, "NPP_SetWindow (QuickDraw): %d, port=0x%08x, window.x:%d window.y:%d window.width:%d window.height:%d",
                npErr, (int)nPort.qdPort.port, (int)window.x, (int)window.y, (int)window.width, (int)window.height);
            break;
#endif /* NP_NO_QUICKDRAW */
            
            case NPDrawingModelCoreGraphics:
                LOG(Plugins, "NPP_SetWindow (CoreGraphics): %d, window=%p, context=%p, window.x:%d window.y:%d window.width:%d window.height:%d",
                npErr, nPort.cgPort.window, nPort.cgPort.context, (int)window.x, (int)window.y, (int)window.width, (int)window.height);
            break;

            case NPDrawingModelOpenGL:
                LOG(Plugins, "NPP_SetWindow (CoreGraphics): %d, window=%p, context=%p, window.x:%d window.y:%d window.width:%d window.height:%d",
                npErr, nPort.aglPort.window, nPort.aglPort.context, (int)window.x, (int)window.y, (int)window.width, (int)window.height);
            break;
            
            default:
                ASSERT_NOT_REACHED();
            break;
        }
#endif /* !defined(NDEBUG) */
        
        lastSetWindow = window;
        lastSetPort = nPort;
    }
}

- (void)removeTrackingRect
{
    if (trackingTag) {
        [self removeTrackingRect:trackingTag];
        trackingTag = 0;

        // Do the following after setting trackingTag to 0 so we don't re-enter.

        // Balance the retain in resetTrackingRect. Use autorelease in case we hold 
        // the last reference to the window during tear-down, to avoid crashing AppKit. 
        [[self window] autorelease];
    }
}

- (void)resetTrackingRect
{
    [self removeTrackingRect];
    if (isStarted) {
        // Retain the window so that removeTrackingRect can work after the window is closed.
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
    [notificationCenter addObserver:self selector:@selector(windowBecameKey:)
                               name:NSWindowDidBecomeKeyNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowResignedKey:)
                               name:NSWindowDidResignKeyNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowDidMiniaturize:)
                               name:NSWindowDidMiniaturizeNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowDidDeminiaturize:)
                               name:NSWindowDidDeminiaturizeNotification object:theWindow];
    
    [notificationCenter addObserver:self selector:@selector(loginWindowDidSwitchFromUser:)
                               name:LoginWindowDidSwitchFromUserNotification object:nil];
    [notificationCenter addObserver:self selector:@selector(loginWindowDidSwitchToUser:)
                               name:LoginWindowDidSwitchToUserNotification object:nil];
}

- (void)removeWindowObservers
{
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter removeObserver:self name:NSWindowDidBecomeKeyNotification     object:nil];
    [notificationCenter removeObserver:self name:NSWindowDidResignKeyNotification     object:nil];
    [notificationCenter removeObserver:self name:NSWindowDidMiniaturizeNotification   object:nil];
    [notificationCenter removeObserver:self name:NSWindowDidDeminiaturizeNotification object:nil];
    [notificationCenter removeObserver:self name:LoginWindowDidSwitchFromUserNotification   object:nil];
    [notificationCenter removeObserver:self name:LoginWindowDidSwitchToUserNotification     object:nil];
}

- (BOOL)start
{
    ASSERT([self currentWindow]);
    
    if (isStarted)
        return YES;

    if (![self canStart])
        return NO;
    
    ASSERT([self webView]);
    
    if (![[[self webView] preferences] arePlugInsEnabled])
        return NO;

    // Open the plug-in package so it remains loaded while our plugin uses it
    [pluginPackage open];
    
    // Initialize drawingModel to an invalid value so that we can detect when the plugin does not specify a drawingModel
    drawingModel = (NPDrawingModel)-1;
    
    // Plug-ins are "windowed" by default.  On MacOS, windowed plug-ins share the same window and graphics port as the main
    // browser window.  Windowless plug-ins are rendered off-screen, then copied into the main browser window.
    window.type = NPWindowTypeWindow;
    
    NPError npErr = [self _createPlugin];
    if (npErr != NPERR_NO_ERROR) {
        LOG_ERROR("NPP_New failed with error: %d", npErr);
        [self _destroyPlugin];
        [pluginPackage close];
        return NO;
    }
    
    if (drawingModel == (NPDrawingModel)-1) {
#ifndef NP_NO_QUICKDRAW
        // Default to QuickDraw if the plugin did not specify a drawing model.
        drawingModel = NPDrawingModelQuickDraw;
#else
        // QuickDraw is not available, so we can't default to it.  We could default to CoreGraphics instead, but
        // if the plugin did not specify the CoreGraphics drawing model then it must be one of the old QuickDraw
        // plugins.  Thus, the plugin is unsupported and should not be started.  Destroy it here and bail out.
        LOG(Plugins, "Plugin only supports QuickDraw, but QuickDraw is unavailable: %@", pluginPackage);
        [self _destroyPlugin];
        [pluginPackage close];
        return NO;
#endif
    }

    isStarted = YES;
        
    [self updateAndSetWindow];

    if ([self window]) {
        [self addWindowObservers];
        if ([[self window] isKeyWindow]) {
            [self sendActivateEvent:YES];
        }
        [self restartNullEvents];
    }

    [self resetTrackingRect];
    
    [self didStart];
    
    return YES;
}

- (void)stop
{
    // If we're already calling a plug-in function, do not call NPP_Destroy().  The plug-in function we are calling
    // may assume that its instance->pdata, or other memory freed by NPP_Destroy(), is valid and unchanged until said
    // plugin-function returns.
    // See <rdar://problem/4480737>.
    if (pluginFunctionCallDepth > 0) {
        shouldStopSoon = YES;
        return;
    }
    
    [self removeTrackingRect];

    if (!isStarted)
        return;
    
    isStarted = NO;
    
    // Stop any active streams
    [streams makeObjectsPerformSelector:@selector(stop)];
    
    // Stop the null events
    [self stopNullEvents];

    // Set cursor back to arrow cursor
    [[NSCursor arrowCursor] set];
    
    // Stop notifications and callbacks.
    [self removeWindowObservers];
    [[pendingFrameLoads allKeys] makeObjectsPerformSelector:@selector(_setInternalLoadDelegate:) withObject:nil];
    [NSObject cancelPreviousPerformRequestsWithTarget:self];

    // Setting the window type to 0 ensures that NPP_SetWindow will be called if the plug-in is restarted.
    lastSetWindow.type = (NPWindowType)0;
    
    [self _destroyPlugin];
    [pluginPackage close];
    
    // We usually remove the key event handler in resignFirstResponder but it is possible that resignFirstResponder 
    // may never get called so we can't completely rely on it.
    [self removeKeyEventHandler];
    
    if (drawingModel == NPDrawingModelOpenGL)
        [self _destroyAGLContext];
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

- (NPP)plugin
{
    return plugin;
}

- (WebNetscapePluginPackage *)pluginPackage
{
    return pluginPackage;
}

- (void)setPluginPackage:(WebNetscapePluginPackage *)thePluginPackage;
{
    [thePluginPackage retain];
    [pluginPackage release];
    pluginPackage = thePluginPackage;

    NPP_New =           [pluginPackage NPP_New];
    NPP_Destroy =       [pluginPackage NPP_Destroy];
    NPP_SetWindow =     [pluginPackage NPP_SetWindow];
    NPP_NewStream =     [pluginPackage NPP_NewStream];
    NPP_WriteReady =    [pluginPackage NPP_WriteReady];
    NPP_Write =         [pluginPackage NPP_Write];
    NPP_StreamAsFile =  [pluginPackage NPP_StreamAsFile];
    NPP_DestroyStream = [pluginPackage NPP_DestroyStream];
    NPP_HandleEvent =   [pluginPackage NPP_HandleEvent];
    NPP_URLNotify =     [pluginPackage NPP_URLNotify];
    NPP_GetValue =      [pluginPackage NPP_GetValue];
    NPP_SetValue =      [pluginPackage NPP_SetValue];
    NPP_Print =         [pluginPackage NPP_Print];
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
        cValues[0] = strdup([baseURL _web_URLCString]);
        argsCount++;
    } else {
        cAttributes = (char **)malloc([keys count] * sizeof(char *));
        cValues = (char **)malloc([values count] * sizeof(char *));
    }

    BOOL isWMP = [[[pluginPackage bundle] bundleIdentifier] isEqualToString:@"com.microsoft.WMP.defaultplugin"];
    
    unsigned i;
    unsigned count = [keys count];
    for (i = 0; i < count; i++) {
        NSString *key = [keys objectAtIndex:i];
        NSString *value = [values objectAtIndex:i];
        if ([key _webkit_isCaseInsensitiveEqualToString:@"height"]) {
            specifiedHeight = [value intValue];
        } else if ([key _webkit_isCaseInsensitiveEqualToString:@"width"]) {
            specifiedWidth = [value intValue];
        }
        // Avoid Window Media Player crash when these attributes are present.
        if (isWMP && ([key _webkit_isCaseInsensitiveEqualToString:@"SAMIStyle"] || [key _webkit_isCaseInsensitiveEqualToString:@"SAMILang"])) {
            continue;
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

- (NSView *)initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];

    streams = [[NSMutableArray alloc] init];
    pendingFrameLoads = [[NSMutableDictionary alloc] init];

    return self;
}

- (void)freeAttributeKeysAndValues
{
    unsigned i;
    for (i = 0; i < argsCount; i++) {
        free(cAttributes[i]);
        free(cValues[i]);
    }
    free(cAttributes);
    free(cValues);
}

- (void)dealloc
{
    ASSERT(!isStarted);

    [pluginPackage release];
    [streams release];
    [MIMEType release];
    [baseURL release];
    [pendingFrameLoads release];
    [element release];
    
    ASSERT(!plugin);
    ASSERT(!aglWindow);
    ASSERT(!aglContext);

    [self freeAttributeKeysAndValues];

    [super dealloc];
}

- (void)finalize
{
    ASSERT(!isStarted);

    [self freeAttributeKeysAndValues];

    [super finalize];
}

- (void)drawRect:(NSRect)rect
{
    if (!isStarted) {
        return;
    }
    
    if ([NSGraphicsContext currentContextDrawingToScreen])
        [self sendUpdateEvent];
    else {
        NSBitmapImageRep *printedPluginBitmap = [self _printedPluginBitmap];
        if (printedPluginBitmap) {
            // Flip the bitmap before drawing because the QuickDraw port is flipped relative
            // to this view.
            CGContextRef cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
            CGContextSaveGState(cgContext);
            NSRect bounds = [self bounds];
            CGContextTranslateCTM(cgContext, 0.0f, NSHeight(bounds));
            CGContextScaleCTM(cgContext, 1.0f, -1.0f);
            [printedPluginBitmap drawInRect:bounds];
            CGContextRestoreGState(cgContext);
        }
    }
    
    // If this is a windowless OpenGL plugin, blit its contents back into this view.  The plug-in just drew into the offscreen context.
    if (drawingModel == NPDrawingModelOpenGL && window.type == NPWindowTypeDrawable) {
        NSImage *aglOffscreenImage = [self _aglOffscreenImageForDrawingInRect:rect];
        if (aglOffscreenImage) {
            // Flip the context before drawing because the CGL context is flipped relative to this view.
            CGContextRef cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
            CGContextSaveGState(cgContext);
            NSRect bounds = [self bounds];
            CGContextTranslateCTM(cgContext, 0.0f, NSHeight(bounds));
            CGContextScaleCTM(cgContext, 1.0f, -1.0f);
            
            // Copy 'rect' from the offscreen buffer to this view (the flip above makes this sort of tricky)
            NSRect flippedRect = rect;
            flippedRect.origin.y = NSMaxY(bounds) - NSMaxY(flippedRect);
            [aglOffscreenImage drawInRect:flippedRect fromRect:flippedRect operation:NSCompositeSourceOver fraction:1.0f];
            CGContextRestoreGState(cgContext);
        }
    }
}

- (BOOL)isFlipped
{
    return YES;
}

- (void)renewGState
{
    [super renewGState];
    
    // -renewGState is called whenever the view's geometry changes.  It's a little hacky to override this method, but
    // much safer than walking up the view hierarchy and observing frame/bounds changed notifications, since you don't
    // have to track subsequent changes to the view hierarchy and add/remove notification observers.
    // NSOpenGLView uses the exact same technique to reshape its OpenGL surface.
    [self _viewHasMoved];
}

#ifndef NP_NO_QUICKDRAW
-(void)tellQuickTimeToChill
{
    ASSERT(drawingModel == NPDrawingModelQuickDraw);
    
    // Make a call to the secret QuickDraw API that makes QuickTime calm down.
    WindowRef windowRef = (WindowRef)[[self window] windowRef];
    if (!windowRef) {
        return;
    }
    CGrafPtr port = GetWindowPort(windowRef);
    Rect bounds;
    GetPortBounds(port, &bounds);
    WKCallDrawingNotification(port, &bounds);
}
#endif /* NP_NO_QUICKDRAW */

- (void)viewWillMoveToWindow:(NSWindow *)newWindow
{
#ifndef NP_NO_QUICKDRAW
    if (drawingModel == NPDrawingModelQuickDraw)
        [self tellQuickTimeToChill];
#endif

    // We must remove the tracking rect before we move to the new window.
    // Once we move to the new window, it will be too late.
    [self removeTrackingRect];
    [self removeWindowObservers];
    
    // Workaround for: <rdar://problem/3822871> resignFirstResponder is not sent to first responder view when it is removed from the window
    [self setHasFocus:NO];

    if (!newWindow) {
        // Hide the AGL child window
        if (drawingModel == NPDrawingModelOpenGL)
            [self _hideAGLWindow];
        
        if ([[self webView] hostWindow]) {
            // View will be moved out of the actual window but it still has a host window.
            [self stopNullEvents];
        } else {
            // View will have no associated windows.
            [self stop];

            // Stop observing WebPreferencesChangedNotification -- we only need to observe this when installed in the view hierarchy.
            // When not in the view hierarchy, -viewWillMoveToWindow: and -viewDidMoveToWindow will start/stop the plugin as needed.
            [[NSNotificationCenter defaultCenter] removeObserver:self name:WebPreferencesChangedNotification object:nil];
        }
    }
}

- (void)viewWillMoveToSuperview:(NSView *)newSuperview
{
    if (!newSuperview) {
        // Stop the plug-in when it is removed from its superview.  It is not sufficient to do this in -viewWillMoveToWindow:nil, because
        // the WebView might still has a hostWindow at that point, which prevents the plug-in from being destroyed.
        // There is no need to start the plug-in when moving into a superview.  -viewDidMoveToWindow takes care of that.
        [self stop];
        
        // Stop observing WebPreferencesChangedNotification -- we only need to observe this when installed in the view hierarchy.
        // When not in the view hierarchy, -viewWillMoveToWindow: and -viewDidMoveToWindow will start/stop the plugin as needed.
        [[NSNotificationCenter defaultCenter] removeObserver:self name:WebPreferencesChangedNotification object:nil];
    }
}

- (void)viewDidMoveToWindow
{
    [self resetTrackingRect];
    
    if ([self window]) {
        // While in the view hierarchy, observe WebPreferencesChangedNotification so that we can start/stop depending
        // on whether plugins are enabled.
        [[NSNotificationCenter defaultCenter] addObserver:self
                                              selector:@selector(preferencesHaveChanged:)
                                              name:WebPreferencesChangedNotification
                                              object:nil];

        // View moved to an actual window. Start it if not already started.
        [self start];
        [self restartNullEvents];
        [self addWindowObservers];
    } else if ([[self webView] hostWindow]) {
        // View moved out of an actual window, but still has a host window.
        // Call setWindow to explicitly "clip out" the plug-in from sight.
        // FIXME: It would be nice to do this where we call stopNullEvents in viewWillMoveToWindow.
        [self updateAndSetWindow];
    }
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
    if (!hostWindow && ![self window]) {
        // View will have no associated windows.
        [self stop];

        // Remove WebPreferencesChangedNotification observer -- we will observe once again when we move back into the window
        [[NSNotificationCenter defaultCenter] removeObserver:self name:WebPreferencesChangedNotification object:nil];
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

- (void)windowBecameKey:(NSNotification *)notification
{
    [self sendActivateEvent:YES];
    [self setNeedsDisplay:YES];
    [self restartNullEvents];
    SetUserFocusWindow((WindowRef)[[self window] windowRef]);
}

- (void)windowResignedKey:(NSNotification *)notification
{
    [self sendActivateEvent:NO];
    [self setNeedsDisplay:YES];
    [self restartNullEvents];
}

- (void)windowDidMiniaturize:(NSNotification *)notification
{
    [self stopNullEvents];
}

- (void)windowDidDeminiaturize:(NSNotification *)notification
{
    [self restartNullEvents];
}

- (void)loginWindowDidSwitchFromUser:(NSNotification *)notification
{
    [self stopNullEvents];
}

-(void)loginWindowDidSwitchToUser:(NSNotification *)notification
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

- (NPObject *)createPluginScriptableObject
{
    if (!NPP_GetValue)
        return NULL;
        
    NPObject *value = NULL;
    [self willCallPlugInFunction];
    NPError error = NPP_GetValue(plugin, NPPVpluginScriptableNPObject, &value);
    [self didCallPlugInFunction];
    if (error != NPERR_NO_ERROR)
        return NULL;
    
    return value;
}

- (void)willCallPlugInFunction
{
    ASSERT(plugin);

    // Could try to prevent infinite recursion here, but it's probably not worth the effort.
    pluginFunctionCallDepth++;
}

- (void)didCallPlugInFunction
{
    ASSERT(pluginFunctionCallDepth > 0);
    pluginFunctionCallDepth--;
    
    // If -stop was called while we were calling into a plug-in function, and we're no longer
    // inside a plug-in function, stop now.
    if (pluginFunctionCallDepth == 0 && shouldStopSoon) {
        shouldStopSoon = NO;
        [self stop];
    }
}

@end

@implementation WebBaseNetscapePluginView (WebNPPCallbacks)

- (NSMutableURLRequest *)requestWithURLCString:(const char *)URLCString
{
    if (!URLCString)
        return nil;
    
    CFStringRef string = CFStringCreateWithCString(kCFAllocatorDefault, URLCString, kCFStringEncodingISOLatin1);
    ASSERT(string); // All strings should be representable in ISO Latin 1
    
    NSString *URLString = [(NSString *)string _web_stringByStrippingReturnCharacters];
    NSURL *URL = [NSURL _web_URLWithDataAsString:URLString relativeToURL:baseURL];
    CFRelease(string);
    if (!URL)
        return nil;

    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:URL];
    Frame* frame = core([self webFrame]);
    if (!frame)
        return nil;
    [request _web_setHTTPReferrer:frame->loader()->outgoingReferrer()];
    return request;
}

- (void)evaluateJavaScriptPluginRequest:(WebPluginRequest *)JSPluginRequest
{
    // FIXME: Is this isStarted check needed here? evaluateJavaScriptPluginRequest should not be called
    // if we are stopped since this method is called after a delay and we call 
    // cancelPreviousPerformRequestsWithTarget inside of stop.
    if (!isStarted) {
        return;
    }
    
    NSURL *URL = [[JSPluginRequest request] URL];
    NSString *JSString = [URL _webkit_scriptIfJavaScriptURL];
    ASSERT(JSString);
    
    NSString *result = [[[self webFrame] _bridge] stringByEvaluatingJavaScriptFromString:JSString forceUserGesture:[JSPluginRequest isCurrentEventUserGesture]];
    
    // Don't continue if stringByEvaluatingJavaScriptFromString caused the plug-in to stop.
    if (!isStarted) {
        return;
    }
        
    if ([JSPluginRequest frameName] != nil) {
        // FIXME: If the result is a string, we probably want to put that string into the frame.
        if ([JSPluginRequest sendNotification]) {
            [self willCallPlugInFunction];
            NPP_URLNotify(plugin, [URL _web_URLCString], NPRES_DONE, [JSPluginRequest notifyData]);
            [self didCallPlugInFunction];
        }
    } else if ([result length] > 0) {
        // Don't call NPP_NewStream and other stream methods if there is no JS result to deliver. This is what Mozilla does.
        NSData *JSData = [result dataUsingEncoding:NSUTF8StringEncoding];
        WebBaseNetscapePluginStream *stream = [[WebBaseNetscapePluginStream alloc] initWithRequestURL:URL
                                                                                               plugin:plugin
                                                                                           notifyData:[JSPluginRequest notifyData]
                                                                                     sendNotification:[JSPluginRequest sendNotification]];
        [stream startStreamResponseURL:URL
                 expectedContentLength:[JSData length]
                      lastModifiedDate:nil
                              MIMEType:@"text/plain"];
        [stream receivedData:JSData];
        [stream finishedLoadingWithData:JSData];
        [stream release];
    }
}

- (void)webFrame:(WebFrame *)webFrame didFinishLoadWithReason:(NPReason)reason
{
    ASSERT(isStarted);
    
    WebPluginRequest *pluginRequest = [pendingFrameLoads objectForKey:webFrame];
    ASSERT(pluginRequest != nil);
    ASSERT([pluginRequest sendNotification]);
        
    [self willCallPlugInFunction];
    NPP_URLNotify(plugin, [[[pluginRequest request] URL] _web_URLCString], reason, [pluginRequest notifyData]);
    [self didCallPlugInFunction];
    
    [pendingFrameLoads removeObjectForKey:webFrame];
    [webFrame _setInternalLoadDelegate:nil];
}

- (void)webFrame:(WebFrame *)webFrame didFinishLoadWithError:(NSError *)error
{
    NPReason reason = NPRES_DONE;
    if (error != nil) {
        reason = [WebBaseNetscapePluginStream reasonForError:error];
    }    
    [self webFrame:webFrame didFinishLoadWithReason:reason];
}

- (void)loadPluginRequest:(WebPluginRequest *)pluginRequest
{
    NSURLRequest *request = [pluginRequest request];
    NSString *frameName = [pluginRequest frameName];
    WebFrame *frame = nil;
    
    NSURL *URL = [request URL];
    NSString *JSString = [URL _webkit_scriptIfJavaScriptURL];
    
    ASSERT(frameName || JSString);
    
    if (frameName) {
        // FIXME - need to get rid of this window creation which
        // bypasses normal targeted link handling
        frame = [[self webFrame] findFrameNamed:frameName];
    
        if (frame == nil) {
            WebView *newWebView = nil;
            WebView *currentWebView = [self webView];
            id wd = [currentWebView UIDelegate];
            if ([wd respondsToSelector:@selector(webView:createWebViewWithRequest:)]) {
                newWebView = [wd webView:currentWebView createWebViewWithRequest:nil];
            } else {
                newWebView = [[WebDefaultUIDelegate sharedUIDelegate] webView:currentWebView createWebViewWithRequest:nil];
            }
            frame = [newWebView mainFrame];
            core(frame)->tree()->setName(frameName);
            [[newWebView _UIDelegateForwarder] webViewShow:newWebView];
        }
    }

    if (JSString) {
        ASSERT(frame == nil || [self webFrame] == frame);
        [self evaluateJavaScriptPluginRequest:pluginRequest];
    } else {
        [frame loadRequest:request];
        if ([pluginRequest sendNotification]) {
            // Check if another plug-in view or even this view is waiting for the frame to load.
            // If it is, tell it that the load was cancelled because it will be anyway.
            WebBaseNetscapePluginView *view = [frame _internalLoadDelegate];
            if (view != nil) {
                ASSERT([view isKindOfClass:[WebBaseNetscapePluginView class]]);
                [view webFrame:frame didFinishLoadWithReason:NPRES_USER_BREAK];
            }
            [pendingFrameLoads _webkit_setObject:pluginRequest forUncopiedKey:frame];
            [frame _setInternalLoadDelegate:self];
        }
    }
}

- (NPError)loadRequest:(NSMutableURLRequest *)request inTarget:(const char *)cTarget withNotifyData:(void *)notifyData sendNotification:(BOOL)sendNotification
{
    NSURL *URL = [request URL];

    if (!URL) 
        return NPERR_INVALID_URL;

    // don't let a plugin start any loads if it is no longer part of a document that is being
    // displayed
    if ([[self dataSource] _documentLoader] != [[self webFrame] _frameLoader]->activeDocumentLoader())
        return NPERR_GENERIC_ERROR;
    
    NSString *JSString = [URL _webkit_scriptIfJavaScriptURL];
    if (JSString != nil) {
        if (![[[self webView] preferences] isJavaScriptEnabled]) {
            // Return NPERR_GENERIC_ERROR if JS is disabled. This is what Mozilla does.
            return NPERR_GENERIC_ERROR;
        } else if (cTarget == NULL && mode == NP_FULL) {
            // Don't allow a JavaScript request from a standalone plug-in that is self-targetted
            // because this can cause the user to be redirected to a blank page (3424039).
            return NPERR_INVALID_PARAM;
        }
    }
        
    if (cTarget || JSString) {
        // Make when targetting a frame or evaluating a JS string, perform the request after a delay because we don't
        // want to potentially kill the plug-in inside of its URL request.
        NSString *target = nil;
        if (cTarget) {
            // Find the frame given the target string.
            target = (NSString *)CFStringCreateWithCString(kCFAllocatorDefault, cTarget, kCFStringEncodingWindowsLatin1);
        }
        
        WebFrame *frame = [self webFrame];
        if (JSString != nil && target != nil && [frame findFrameNamed:target] != frame) {
            // For security reasons, only allow JS requests to be made on the frame that contains the plug-in.
            CFRelease(target);
            return NPERR_INVALID_PARAM;
        }
        
        WebPluginRequest *pluginRequest = [[WebPluginRequest alloc] initWithRequest:request frameName:target notifyData:notifyData sendNotification:sendNotification didStartFromUserGesture:currentEventIsUserGesture];
        [self performSelector:@selector(loadPluginRequest:) withObject:pluginRequest afterDelay:0];
        [pluginRequest release];
        if (target)
            CFRelease(target);
    } else {
        WebNetscapePluginStream *stream = [[WebNetscapePluginStream alloc] initWithRequest:request 
                                                                                    plugin:plugin 
                                                                                notifyData:notifyData 
                                                                          sendNotification:sendNotification];
        if (!stream)
            return NPERR_INVALID_URL;

        [streams addObject:stream];
        [stream start];
        [stream release];
    }
    
    return NPERR_NO_ERROR;
}

-(NPError)getURLNotify:(const char *)URLCString target:(const char *)cTarget notifyData:(void *)notifyData
{
    LOG(Plugins, "NPN_GetURLNotify: %s target: %s", URLCString, cTarget);

    NSMutableURLRequest *request = [self requestWithURLCString:URLCString];
    return [self loadRequest:request inTarget:cTarget withNotifyData:notifyData sendNotification:YES];
}

-(NPError)getURL:(const char *)URLCString target:(const char *)cTarget
{
    LOG(Plugins, "NPN_GetURL: %s target: %s", URLCString, cTarget);

    NSMutableURLRequest *request = [self requestWithURLCString:URLCString];
    return [self loadRequest:request inTarget:cTarget withNotifyData:NULL sendNotification:NO];
}

- (NPError)_postURL:(const char *)URLCString
             target:(const char *)target
                len:(UInt32)len
                buf:(const char *)buf
               file:(NPBool)file
         notifyData:(void *)notifyData
   sendNotification:(BOOL)sendNotification
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
        NSURL *fileURL = [NSURL _web_URLWithDataAsString:bufString];
        NSString *path;
        if ([fileURL isFileURL]) {
            path = [fileURL path];
        } else {
            path = bufString;
        }
        postData = [NSData dataWithContentsOfFile:[path _webkit_fixedCarbonPOSIXPath]];
        CFRelease(bufString);
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
            WebNSInteger location = [postData _web_locationAfterFirstBlankLine];
            if (location != NSNotFound) {
                // If the blank line is somewhere in the middle of postData, everything before is the header.
                NSData *headerData = [postData subdataWithRange:NSMakeRange(0, location)];
                NSMutableDictionary *header = [headerData _webkit_parseRFC822HeaderFields];
                unsigned dataLength = [postData length] - location;

                // Sometimes plugins like to set Content-Length themselves when they post,
                // but WebFoundation does not like that. So we will remove the header
                // and instead truncate the data to the requested length.
                NSString *contentLength = [header objectForKey:@"Content-Length"];

                if (contentLength != nil)
                    dataLength = MIN((unsigned)[contentLength intValue], dataLength);
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

    // Plug-ins expect to receive uncached data when doing a POST (3347134).
    [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];
    [request setHTTPBody:postData];
    
    return [self loadRequest:request inTarget:target withNotifyData:notifyData sendNotification:sendNotification];
}

- (NPError)postURLNotify:(const char *)URLCString
                  target:(const char *)target
                     len:(UInt32)len
                     buf:(const char *)buf
                    file:(NPBool)file
              notifyData:(void *)notifyData
{
    LOG(Plugins, "NPN_PostURLNotify: %s", URLCString);
    return [self _postURL:URLCString target:target len:len buf:buf file:file notifyData:notifyData sendNotification:YES allowHeaders:YES];
}

-(NPError)postURL:(const char *)URLCString
           target:(const char *)target
              len:(UInt32)len
              buf:(const char *)buf
             file:(NPBool)file
{
    LOG(Plugins, "NPN_PostURL: %s", URLCString);        
    // As documented, only allow headers to be specified via NPP_PostURL when using a file.
    return [self _postURL:URLCString target:target len:len buf:buf file:file notifyData:NULL sendNotification:NO allowHeaders:file];
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
    WebBaseNetscapePluginStream *browserStream = (WebBaseNetscapePluginStream *)stream->ndata;
    [browserStream cancelLoadAndDestroyStreamWithError:[browserStream errorForReason:reason]];
    return NPERR_NO_ERROR;
}

- (const char *)userAgent
{
    return [[[self webView] userAgentForURL:baseURL] lossyCString];
}

-(void)status:(const char *)message
{    
    if (!message) {
        LOG_ERROR("NPN_Status passed a NULL status message");
        return;
    }

    CFStringRef status = CFStringCreateWithCString(NULL, message, kCFStringEncodingWindowsLatin1);
    LOG(Plugins, "NPN_Status: %@", status);
    WebView *wv = [self webView];
    [[wv _UIDelegateForwarder] webView:wv setStatusText:(NSString *)status];
    CFRelease(status);
}

-(void)invalidateRect:(NPRect *)invalidRect
{
    LOG(Plugins, "NPN_InvalidateRect");
    [self setNeedsDisplayInRect:NSMakeRect(invalidRect->left, invalidRect->top,
        (float)invalidRect->right - invalidRect->left, (float)invalidRect->bottom - invalidRect->top)];
}

- (void)invalidateRegion:(NPRegion)invalidRegion
{
    LOG(Plugins, "NPN_InvalidateRegion");
    NSRect invalidRect = NSZeroRect;
    switch (drawingModel) {
#ifndef NP_NO_QUICKDRAW
        case NPDrawingModelQuickDraw:
        {
            Rect qdRect;
            GetRegionBounds((NPQDRegion)invalidRegion, &qdRect);
            invalidRect = NSMakeRect(qdRect.left, qdRect.top, qdRect.right - qdRect.left, qdRect.bottom - qdRect.top);
        }
        break;
#endif /* NP_NO_QUICKDRAW */
        
        case NPDrawingModelCoreGraphics:
        case NPDrawingModelOpenGL:
        {
            CGRect cgRect = CGPathGetBoundingBox((NPCGRegion)invalidRegion);
            invalidRect = *(NSRect *)&cgRect;
        }
        break;
    
        default:
            ASSERT_NOT_REACHED();
        break;
    }
    
    [self setNeedsDisplayInRect:invalidRect];
}

-(void)forceRedraw
{
    LOG(Plugins, "forceRedraw");
    [self setNeedsDisplay:YES];
    [[self window] displayIfNeeded];
}

- (NPError)getVariable:(NPNVariable)variable value:(void *)value
{
    switch (variable) {
        case NPNVWindowNPObject:
        {
            FrameMac* frame = core([self webFrame]);
            NPObject* windowScriptObject = frame ? frame->windowScriptNPObject() : 0;

            // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugins/npruntime.html#browseraccess>
            if (windowScriptObject)
                _NPN_RetainObject(windowScriptObject);
            
            void **v = (void **)value;
            *v = windowScriptObject;

            return NPERR_NO_ERROR;
        }

        case NPNVPluginElementNPObject:
        {
            if (!element)
                return NPERR_GENERIC_ERROR;
            
            NPObject *plugInScriptObject = (NPObject *)[element _NPObject];

            // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugins/npruntime.html#browseraccess>
            if (plugInScriptObject)
                _NPN_RetainObject(plugInScriptObject);

            void **v = (void **)value;
            *v = plugInScriptObject;

            return NPERR_NO_ERROR;
        }
        
        case NPNVpluginDrawingModel:
        {
            *(NPDrawingModel *)value = drawingModel;
            return NPERR_NO_ERROR;
        }

#ifndef NP_NO_QUICKDRAW
        case NPNVsupportsQuickDrawBool:
        {
            *(NPBool *)value = TRUE;
            return NPERR_NO_ERROR;
        }
#endif /* NP_NO_QUICKDRAW */
        
        case NPNVsupportsCoreGraphicsBool:
        {
            *(NPBool *)value = TRUE;
            return NPERR_NO_ERROR;
        }

        case NPNVsupportsOpenGLBool:
        {
            *(NPBool *)value = TRUE;
            return NPERR_NO_ERROR;
        }
        
        default:
            break;
    }

    return NPERR_GENERIC_ERROR;
}

- (NPError)setVariable:(NPPVariable)variable value:(void *)value
{
    switch (variable) {
        case NPPVpluginWindowBool:
        {
            NPWindowType newWindowType = (value ? NPWindowTypeWindow : NPWindowTypeDrawable);

            // Redisplay if window type is changing (some drawing models can only have their windows set while updating).
            if (newWindowType != window.type)
                [self setNeedsDisplay:YES];
            
            window.type = newWindowType;
        }
        
        case NPPVpluginTransparentBool:
        {
            BOOL newTransparent = (value != 0);
            
            // Redisplay if transparency is changing
            if (isTransparent != newTransparent)
                [self setNeedsDisplay:YES];
            
            isTransparent = newTransparent;
            
            return NPERR_NO_ERROR;
        }
        
        case NPNVpluginDrawingModel:
        {
            // Can only set drawing model inside NPP_New()
            if (self != [[self class] currentPluginView])
                return NPERR_GENERIC_ERROR;
            
            // Check for valid, supported drawing model
            NPDrawingModel newDrawingModel = (NPDrawingModel)(uintptr_t)value;
            switch (newDrawingModel) {
                // Supported drawing models:
#ifndef NP_NO_QUICKDRAW
                case NPDrawingModelQuickDraw:
#endif
                case NPDrawingModelCoreGraphics:
                case NPDrawingModelOpenGL:
                    drawingModel = newDrawingModel;
                    return NPERR_NO_ERROR;
                
                // Unsupported (or unknown) drawing models:
                default:
                    LOG(Plugins, "Plugin %@ uses unsupported drawing model: %d", pluginPackage, drawingModel);
                    return NPERR_GENERIC_ERROR;
            }
        }
        
        default:
            return NPERR_GENERIC_ERROR;
    }
}

@end

@implementation WebPluginRequest

- (id)initWithRequest:(NSURLRequest *)request frameName:(NSString *)frameName notifyData:(void *)notifyData sendNotification:(BOOL)sendNotification didStartFromUserGesture:(BOOL)currentEventIsUserGesture
{
    [super init];
    _didStartFromUserGesture = currentEventIsUserGesture;
    _request = [request retain];
    _frameName = [frameName retain];
    _notifyData = notifyData;
    _sendNotification = sendNotification;
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

- (BOOL)isCurrentEventUserGesture
{
    return _didStartFromUserGesture;
}

- (BOOL)sendNotification
{
    return _sendNotification;
}

- (void *)notifyData
{
    return _notifyData;
}

@end

@implementation WebBaseNetscapePluginView (Internal)

- (NPError)_createPlugin
{
    plugin = (NPP)calloc(1, sizeof(NPP_t));
    plugin->ndata = self;

    ASSERT(NPP_New);

    // NPN_New(), which creates the plug-in instance, should never be called while calling a plug-in function for that instance.
    ASSERT(pluginFunctionCallDepth == 0);

    [[self class] setCurrentPluginView:self];
    NPError npErr = NPP_New((char *)[MIMEType cString], plugin, mode, argsCount, cAttributes, cValues, NULL);
    [[self class] setCurrentPluginView:nil];
    
    LOG(Plugins, "NPP_New: %d", npErr);
    return npErr;
}

- (void)_destroyPlugin
{
    NPError npErr;
    npErr = NPP_Destroy(plugin, NULL);
    LOG(Plugins, "NPP_Destroy: %d", npErr);
    free(plugin);
    plugin = NULL;
}

- (void)_viewHasMoved
{
    // All of the work this method does may safely be skipped if the view is not in a window.  When the view
    // is moved back into a window, everything should be set up correctly.
    if (![self window])
        return;
    
    if (drawingModel == NPDrawingModelOpenGL)
        [self _reshapeAGLWindow];

#ifndef NP_NO_QUICKDRAW
    if (drawingModel == NPDrawingModelQuickDraw)
        [self tellQuickTimeToChill];
#endif
    [self updateAndSetWindow];
    [self resetTrackingRect];
    
    // Check to see if the plugin view is completely obscured (scrolled out of view, for example).
    // For performance reasons, we send null events at a lower rate to plugins which are obscured.
    BOOL oldIsObscured = isCompletelyObscured;
    isCompletelyObscured = NSIsEmptyRect([self visibleRect]);
    if (isCompletelyObscured != oldIsObscured)
        [self restartNullEvents];
}

- (NSBitmapImageRep *)_printedPluginBitmap
{
#ifdef NP_NO_QUICKDRAW
    return nil;
#else
    // Cannot print plugins that do not implement NPP_Print
    if (!NPP_Print)
        return nil;

    // This NSBitmapImageRep will share its bitmap buffer with a GWorld that the plugin will draw into.
    // The bitmap is created in 32-bits-per-pixel ARGB format, which is the default GWorld pixel format.
    NSBitmapImageRep *bitmap = [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                                         pixelsWide:window.width
                                                         pixelsHigh:window.height
                                                         bitsPerSample:8
                                                         samplesPerPixel:4
                                                         hasAlpha:YES
                                                         isPlanar:NO
                                                         colorSpaceName:NSDeviceRGBColorSpace
                                                         bitmapFormat:NSAlphaFirstBitmapFormat
                                                         bytesPerRow:0
                                                         bitsPerPixel:0] autorelease];
    ASSERT(bitmap);
    
    // Create a GWorld with the same underlying buffer into which the plugin can draw
    Rect printGWorldBounds;
    SetRect(&printGWorldBounds, 0, 0, window.width, window.height);
    GWorldPtr printGWorld;
    if (NewGWorldFromPtr(&printGWorld,
                         k32ARGBPixelFormat,
                         &printGWorldBounds,
                         NULL,
                         NULL,
                         0,
                         (Ptr)[bitmap bitmapData],
                         [bitmap bytesPerRow]) != noErr) {
        LOG_ERROR("Could not create GWorld for printing");
        return nil;
    }
    
    /// Create NPWindow for the GWorld
    NPWindow printNPWindow;
    printNPWindow.window = &printGWorld; // Normally this is an NP_Port, but when printing it is the actual CGrafPtr
    printNPWindow.x = 0;
    printNPWindow.y = 0;
    printNPWindow.width = window.width;
    printNPWindow.height = window.height;
    printNPWindow.clipRect.top = 0;
    printNPWindow.clipRect.left = 0;
    printNPWindow.clipRect.right = window.width;
    printNPWindow.clipRect.bottom = window.height;
    printNPWindow.type = NPWindowTypeDrawable; // Offscreen graphics port as opposed to a proper window
    
    // Create embed-mode NPPrint
    NPPrint npPrint;
    npPrint.mode = NP_EMBED;
    npPrint.print.embedPrint.window = printNPWindow;
    npPrint.print.embedPrint.platformPrint = printGWorld;
    
    // Tell the plugin to print into the GWorld
    [self willCallPlugInFunction];
    NPP_Print(plugin, &npPrint);
    [self didCallPlugInFunction];

    // Don't need the GWorld anymore
    DisposeGWorld(printGWorld);
        
    return bitmap;
#endif
}

- (BOOL)_createAGLContextIfNeeded
{
    ASSERT(drawingModel == NPDrawingModelOpenGL);

    // Do nothing (but indicate success) if the AGL context already exists
    if (aglContext)
        return YES;
        
    switch (window.type) {
        case NPWindowTypeWindow:
            return [self _createWindowedAGLContext];
        
        case NPWindowTypeDrawable:
            return [self _createWindowlessAGLContext];
        
        default:
            ASSERT_NOT_REACHED();
            return NO;
    }
}

- (BOOL)_createWindowedAGLContext
{
    ASSERT(drawingModel == NPDrawingModelOpenGL);
    ASSERT(!aglContext);
    ASSERT(!aglWindow);
    ASSERT([self window]);
    
    GLint pixelFormatAttributes[] = {
        AGL_RGBA,
        AGL_RED_SIZE, 8,
        AGL_GREEN_SIZE, 8,
        AGL_BLUE_SIZE, 8,
        AGL_ALPHA_SIZE, 8,
        AGL_DEPTH_SIZE, 32,
        AGL_WINDOW,
        AGL_ACCELERATED,
        0
    };
    
    // Choose AGL pixel format
    AGLPixelFormat pixelFormat = aglChoosePixelFormat(NULL, 0, pixelFormatAttributes);
    if (!pixelFormat) {
        LOG_ERROR("Could not find suitable AGL pixel format: %s", aglErrorString(aglGetError()));
        return NO;
    }
    
    // Create AGL context
    aglContext = aglCreateContext(pixelFormat, NULL);
    aglDestroyPixelFormat(pixelFormat);
    if (!aglContext) {
        LOG_ERROR("Could not create AGL context: %s", aglErrorString(aglGetError()));
        return NO;
    }
    
    // Create AGL window
    aglWindow = [[NSWindow alloc] initWithContentRect:NSZeroRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO];
    if (!aglWindow) {
        LOG_ERROR("Could not create window for AGL drawable.");
        return NO;
    }
    
    // AGL window should allow clicks to go through -- mouse events are tracked by WebCore
    [aglWindow setIgnoresMouseEvents:YES];
    
    // Make sure the window is not opaque -- windowed plug-ins cannot layer with other page elements
    [aglWindow setOpaque:YES];

    // Position and order in the AGL window
    [self _reshapeAGLWindow];

    // Attach the AGL context to its window
    GLboolean success;
#ifdef AGL_VERSION_3_0
    success = aglSetWindowRef(aglContext, (WindowRef)[aglWindow windowRef]);
#else
    success = aglSetDrawable(aglContext, (AGLDrawable)GetWindowPort((WindowRef)[aglWindow windowRef]));
#endif
    if (!success) {
        LOG_ERROR("Could not set AGL drawable: %s", aglErrorString(aglGetError()));
        aglDestroyContext(aglContext);
        aglContext = NULL;
        return NO;
    }
        
    return YES;
}

- (BOOL)_createWindowlessAGLContext
{
    ASSERT(drawingModel == NPDrawingModelOpenGL);
    ASSERT(!aglContext);
    ASSERT(!aglWindow);
    
    GLint pixelFormatAttributes[] = {
        AGL_RGBA,
        AGL_RED_SIZE, 8,
        AGL_GREEN_SIZE, 8,
        AGL_BLUE_SIZE, 8,
        AGL_ALPHA_SIZE, 8,
        AGL_DEPTH_SIZE, 32,
        AGL_OFFSCREEN,
        0
    };

    // Choose AGL pixel format
    AGLPixelFormat pixelFormat = aglChoosePixelFormat(NULL, 0, pixelFormatAttributes);
    if (!pixelFormat) {
        LOG_ERROR("Could not find suitable AGL pixel format: %s", aglErrorString(aglGetError()));
        return NO;
    }
    
    // Create AGL context
    aglContext = aglCreateContext(pixelFormat, NULL);
    aglDestroyPixelFormat(pixelFormat);
    if (!aglContext) {
        LOG_ERROR("Could not create AGL context: %s", aglErrorString(aglGetError()));
        return NO;
    }
    
    // Create offscreen buffer for AGL context
    NSSize boundsSize = [self bounds].size;
    GLvoid *offscreenBuffer = (GLvoid *)malloc(static_cast<size_t>(boundsSize.width * boundsSize.height * 4));
    if (!offscreenBuffer) {
        LOG_ERROR("Could not allocate offscreen buffer for AGL context");
        aglDestroyContext(aglContext);
        aglContext = NULL;
        return NO;
    }
    
    // Attach AGL context to offscreen buffer
    CGLContextObj cglContext = [self _cglContext];
    CGLError error = CGLSetOffScreen(cglContext, static_cast<long>(boundsSize.width), static_cast<long>(boundsSize.height), static_cast<long>(boundsSize.width * 4), offscreenBuffer);
    if (error) {
        LOG_ERROR("Could not set offscreen buffer for AGL context: %d", error);
        aglDestroyContext(aglContext);
        aglContext = NULL;
        return NO;
    }
    
    return YES;
}

- (CGLContextObj)_cglContext
{
    ASSERT(drawingModel == NPDrawingModelOpenGL);

    CGLContextObj cglContext = NULL;
    if (!aglGetCGLContext(aglContext, (void **)&cglContext) || !cglContext)
        LOG_ERROR("Could not get CGL context for AGL context: %s", aglErrorString(aglGetError()));
        
    return cglContext;
}

- (BOOL)_getAGLOffscreenBuffer:(GLvoid **)outBuffer width:(GLsizei *)outWidth height:(GLsizei *)outHeight
{
    ASSERT(drawingModel == NPDrawingModelOpenGL);
    
    if (outBuffer)
        *outBuffer = NULL;
    if (outWidth)
        *outWidth = 0;
    if (outHeight)
        *outHeight = 0;
    
    // Only windowless plug-ins have offscreen buffers
    if (window.type != NPWindowTypeDrawable)
        return NO;
    
    CGLContextObj cglContext = [self _cglContext];
    if (!cglContext)
        return NO;
    
    GLsizei width, height;
    GLint rowBytes;
    void *offscreenBuffer = NULL;
    CGLError error = CGLGetOffScreen(cglContext, &width, &height, &rowBytes, &offscreenBuffer);
    if (error || !offscreenBuffer) {
        LOG_ERROR("Could not get offscreen buffer for AGL context: %d", error);
        return NO;
    }
    
    if (outBuffer)
        *outBuffer = offscreenBuffer;
    if (outWidth)
        *outWidth = width;
    if (outHeight)
        *outHeight = height;
    
    return YES;
}

- (void)_destroyAGLContext
{    
    ASSERT(drawingModel == NPDrawingModelOpenGL);

    if (!aglContext)
        return;

    if (aglContext) {
        // If this is a windowless plug-in, free its offscreen buffer
        GLvoid *offscreenBuffer;
        if ([self _getAGLOffscreenBuffer:&offscreenBuffer width:NULL height:NULL])
            free(offscreenBuffer);
        
        // Detach context from the AGL window
#ifdef AGL_VERSION_3_0
        aglSetWindowRef(aglContext, NULL);
#else
        aglSetDrawable(aglContext, NULL);
#endif
        
        // Destroy the context
        aglDestroyContext(aglContext);
        aglContext = NULL;
    }
    
    // Destroy the AGL window
    if (aglWindow) {
        [self _hideAGLWindow];
        aglWindow = nil;
    }
}

- (void)_reshapeAGLWindow
{
    ASSERT(drawingModel == NPDrawingModelOpenGL);
    
    if (!aglContext)
        return;

    switch (window.type) {
        case NPWindowTypeWindow:
        {
            if (!aglWindow)
                break;
                
            // The AGL window is being reshaped because the plugin view has moved.  Since the view has moved, it will soon redraw.
            // We want the AGL window to update at the same time as its underlying view.  So, we disable screen updates until the
            // plugin view's window flushes.
            NSWindow *browserWindow = [self window];
            ASSERT(browserWindow);
            [browserWindow disableScreenUpdatesUntilFlush];

            // Add the AGL window as a child of the main window if necessary
            if ([aglWindow parentWindow] != browserWindow)
                [browserWindow addChildWindow:aglWindow ordered:NSWindowAbove];
            
            // Update the AGL window frame
            NSRect aglWindowFrame = [self convertRect:[self visibleRect] toView:nil];
            aglWindowFrame.origin = [browserWindow convertBaseToScreen:aglWindowFrame.origin];
            [aglWindow setFrame:aglWindowFrame display:NO];
            
            // Update the AGL context
            aglUpdateContext(aglContext);
        }
        break;
        
        case NPWindowTypeDrawable:
        {
            // Get offscreen buffer; we can skip this step if we don't have one yet
            GLvoid *offscreenBuffer;
            GLsizei width, height;
            if (![self _getAGLOffscreenBuffer:&offscreenBuffer width:&width height:&height] || !offscreenBuffer)
                break;
            
            // Don't resize the offscreen buffer if it's already the same size as the view bounds
            NSSize boundsSize = [self bounds].size;
            if (boundsSize.width == width && boundsSize.height == height)
                break;
            
            // Resize the offscreen buffer
            offscreenBuffer = realloc(offscreenBuffer, static_cast<size_t>(boundsSize.width * boundsSize.height * 4));
            if (!offscreenBuffer) {
                LOG_ERROR("Could not allocate offscreen buffer for AGL context");
                break;
            }

            // Update the offscreen 
            CGLContextObj cglContext = [self _cglContext];
            CGLError error = CGLSetOffScreen(cglContext, static_cast<long>(boundsSize.width), static_cast<long>(boundsSize.height), static_cast<long>(boundsSize.width * 4), offscreenBuffer);
            if (error) {
                LOG_ERROR("Could not set offscreen buffer for AGL context: %d", error);
                break;
            }

            // Update the AGL context
            aglUpdateContext(aglContext);
        }
        break;
        
        default:
            ASSERT_NOT_REACHED();
        break;
    }
}

- (void)_hideAGLWindow
{
    ASSERT(drawingModel == NPDrawingModelOpenGL);
    
    if (!aglWindow)
        return;
    
    // aglWindow should only be set for a windowed OpenGL plug-in
    ASSERT(window.type == NPWindowTypeWindow);
    
    NSWindow *parentWindow = [aglWindow parentWindow];
    if (parentWindow) {
        // Disable screen updates so that this AGL window orders out atomically with other plugins' AGL windows
        [parentWindow disableScreenUpdatesUntilFlush];
        ASSERT(parentWindow == [self window]);
        [parentWindow removeChildWindow:aglWindow];
    }
    [aglWindow orderOut:nil];
}

- (NSImage *)_aglOffscreenImageForDrawingInRect:(NSRect)drawingInRect
{
    ASSERT(drawingModel == NPDrawingModelOpenGL);

    CGLContextObj cglContext = [self _cglContext];
    if (!cglContext)
        return nil;

    // Get the offscreen buffer
    GLvoid *offscreenBuffer;
    GLsizei width, height;
    if (![self _getAGLOffscreenBuffer:&offscreenBuffer width:&width height:&height])
        return nil;

    unsigned char *plane = (unsigned char *)offscreenBuffer;

#if defined(__i386__) || defined(__x86_64__)
    // Make rect inside the offscreen buffer because we're about to directly modify the bits inside drawingInRect
    NSRect rect = NSIntegralRect(NSIntersectionRect(drawingInRect, NSMakeRect(0, 0, width, height)));

    // The offscreen buffer, being an OpenGL framebuffer, is in BGRA format on x86.  We need to swap the blue and red channels before
    // wrapping the buffer in an NSBitmapImageRep, which only supports RGBA and ARGB.
    // On PowerPC, the OpenGL framebuffer is in ARGB format.  Since that is a format that NSBitmapImageRep supports, all that is
    // needed on PowerPC is to pass the NSAlphaFirstBitmapFormat flag when creating the NSBitmapImageRep.  On x86, we need to swap the
    // framebuffer color components such that they are in ARGB order, as they are on PowerPC.
    // If only a small region of the plug-in is being redrawn, then it would be a waste to convert the entire image from BGRA to ARGB.
    // Since we know what region of the image will ultimately be drawn to screen (drawingInRect), we restrict the channel swapping to
    // just that region within the offscreen buffer.
    if (!WebConvertBGRAToARGB(plane, width * 4, (int)rect.origin.x, (int)rect.origin.y, (int)rect.size.width, (int)rect.size.height))
        return nil;
#endif /* defined(__i386__) || defined(__x86_64__) */
    
    NSBitmapImageRep *aglBitmap = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:&plane
                      pixelsWide:width
                      pixelsHigh:height
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                    bitmapFormat:NSAlphaFirstBitmapFormat
                     bytesPerRow:width * 4
                    bitsPerPixel:32];
    if (!aglBitmap) {
        LOG_ERROR("Could not create bitmap for AGL offscreen buffer");
        return nil;
    }

    // Wrap the bitmap in an NSImage.  This allocation isn't very expensive -- the actual image data is already in the bitmap rep
    NSImage *aglImage = [[[NSImage alloc] initWithSize:[aglBitmap size]] autorelease];
    [aglImage addRepresentation:aglBitmap];
    [aglBitmap release];
    
    return aglImage;
}

@end

@implementation NSData (PluginExtras)

- (BOOL)_web_startsWithBlankLine
{
    return [self length] > 0 && ((const char *)[self bytes])[0] == '\n';
}


- (WebNSInteger)_web_locationAfterFirstBlankLine
{
    const char *bytes = (const char *)[self bytes];
    unsigned length = [self length];
    
    unsigned i;
    for (i = 0; i < length - 4; i++) {
        
        //  Support for Acrobat. It sends "\n\n".
        if (bytes[i] == '\n' && bytes[i+1] == '\n') {
            return i+2;
        }
        
        // Returns the position after 2 CRLF's or 1 CRLF if it is the first line.
        if (bytes[i] == '\r' && bytes[i+1] == '\n') {
            i += 2;
            if (i == 2) {
                return i;
            } else if (bytes[i] == '\n') {
                // Support for Director. It sends "\r\n\n" (3880387).
                return i+1;
            } else if (bytes[i] == '\r' && bytes[i+1] == '\n') {
                // Support for Flash. It sends "\r\n\r\n" (3758113).
                return i+2;
            }
        }
    }
    return NSNotFound;
}

@end
