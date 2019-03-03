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

#if ENABLE(NETSCAPE_PLUGIN_API)

#import "WebNetscapePluginView.h"

#import "WebDataSourceInternal.h"
#import "WebDefaultUIDelegate.h"
#import "WebFrameInternal.h"
#import "WebFrameView.h"
#import "WebKitErrorsPrivate.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebNSDataExtras.h"
#import "WebNSDictionaryExtras.h"
#import "WebNSObjectExtras.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebNSViewExtras.h"
#import "WebNetscapePluginEventHandler.h"
#import "WebNetscapePluginPackage.h"
#import "WebNetscapePluginStream.h"
#import "WebPluginRequest.h"
#import "WebPreferences.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <Carbon/Carbon.h>
#import <JavaScriptCore/InitializeThreading.h>
#import <JavaScriptCore/JSLock.h>
#import <WebCore/CommonVM.h>
#import <WebCore/CookieJar.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/Element.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameTree.h>
#import <WebCore/FrameView.h>
#import <WebCore/HTMLPlugInElement.h>
#import <WebCore/NP_jsobject.h>
#import <WebCore/Page.h>
#import <WebCore/ProxyServer.h>
#import <WebCore/ScriptController.h>
#import <WebCore/SecurityOrigin.h>
#import <WebCore/UserGestureIndicator.h>
#import <WebCore/WebCoreURLResponse.h>
#import <WebCore/npruntime_impl.h>
#import <WebCore/runtime_root.h>
#import <WebKitLegacy/DOMPrivate.h>
#import <WebKitLegacy/WebUIDelegate.h>
#import <objc/runtime.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <wtf/Assertions.h>
#import <wtf/MainThread.h>
#import <wtf/RunLoop.h>
#import <wtf/SoftLinking.h>
#import <wtf/text/CString.h>

#define LoginWindowDidSwitchFromUserNotification    @"WebLoginWindowDidSwitchFromUserNotification"
#define LoginWindowDidSwitchToUserNotification      @"WebLoginWindowDidSwitchToUserNotification"
#define WKNVSupportsCompositingCoreAnimationPluginsBool 74656  /* TRUE if the browser supports hardware compositing of Core Animation plug-ins  */

using namespace WebCore;
using namespace WebKit;

@interface WebNetscapePluginView (Internal)
- (NPError)_createPlugin;
- (void)_destroyPlugin;
- (void)_redeliverStream;
- (BOOL)_shouldCancelSrcStream;
@end

static WebNetscapePluginView *currentPluginView = nil;

typedef struct OpaquePortState* PortState;

static const Seconds throttledTimerInterval { 250_ms };

class PluginTimer : public TimerBase {
public:
    typedef void (*TimerFunc)(NPP npp, uint32_t timerID);
    
    PluginTimer(NPP npp, uint32_t timerID, uint32_t interval, NPBool repeat, TimerFunc timerFunc)
        : m_npp(npp)
        , m_timerID(timerID)
        , m_interval(interval)
        , m_repeat(repeat)
        , m_timerFunc(timerFunc)
    {
    }
    
    void start(bool throttle)
    {
        ASSERT(!isActive());

        Seconds timeInterval = 1_ms * m_interval;
        
        if (throttle)
            timeInterval = std::max(timeInterval, throttledTimerInterval);
        
        if (m_repeat)
            startRepeating(Seconds { timeInterval });
        else
            startOneShot(timeInterval);
    }

private:
    virtual void fired() 
    {
        m_timerFunc(m_npp, m_timerID);
        if (!m_repeat)
            delete this;
    }
    
    NPP m_npp;
    uint32_t m_timerID;
    uint32_t m_interval;
    NPBool m_repeat;
    TimerFunc m_timerFunc;
};

typedef struct {
    CGContextRef context;
} PortState_CG;

@class NSTextInputContext;
@interface NSResponder (AppKitDetails)
- (NSTextInputContext *)inputContext;
@end

@interface WebNetscapePluginView (ForwardDeclarations)
- (void)setWindowIfNecessary;
- (NPError)loadRequest:(NSMutableURLRequest *)request inTarget:(const char *)cTarget withNotifyData:(void *)notifyData sendNotification:(BOOL)sendNotification;
@end

@implementation WebNetscapePluginView

+ (void)initialize
{
    JSC::initializeThreading();
    WTF::initializeMainThreadToProcessMainThread();
    RunLoop::initializeMainRunLoop();
    sendUserChangeNotifications();
}

// MARK: EVENTS

static inline void getNPRect(const NSRect& nr, NPRect& npr)
{
    npr.top = static_cast<uint16_t>(nr.origin.y);
    npr.left = static_cast<uint16_t>(nr.origin.x);
    npr.bottom = static_cast<uint16_t>(NSMaxY(nr));
    npr.right = static_cast<uint16_t>(NSMaxX(nr));
}

- (PortState)saveAndSetNewPortStateForUpdate:(BOOL)forUpdate
{
    ASSERT([self currentWindow] != nil);
    
    // The base coordinates of a window and it's contentView happen to be the equal at a userSpaceScaleFactor
    // of 1. For non-1.0 scale factors this assumption is false.
    NSView *windowContentView = [[self window] contentView];
    NSRect boundsInWindow = [self convertRect:[self bounds] toView:windowContentView];
    NSRect visibleRectInWindow = [self actualVisibleRectInWindow];
    
    // Flip Y to convert -[NSWindow contentView] coordinates to top-left-based window coordinates.
    float borderViewHeight = [[self currentWindow] frame].size.height;
    boundsInWindow.origin.y = borderViewHeight - NSMaxY(boundsInWindow);
    visibleRectInWindow.origin.y = borderViewHeight - NSMaxY(visibleRectInWindow);
    
    window.type = NPWindowTypeWindow;
    window.x = (int32_t)boundsInWindow.origin.x; 
    window.y = (int32_t)boundsInWindow.origin.y;
    window.width = static_cast<uint32_t>(NSWidth(boundsInWindow));
    window.height = static_cast<uint32_t>(NSHeight(boundsInWindow));
    
    // "Clip-out" the plug-in when:
    // 1) it's not really in a window or off-screen or has no height or width.
    // 2) window.x is a "big negative number" which is how WebCore expresses off-screen widgets.
    // 3) the window is miniaturized or the app is hidden
    // 4) we're inside of viewWillMoveToWindow: with a nil window. In this case, superviews may already have nil 
    // superviews and nil windows and results from convertRect:toView: are incorrect.
    if (window.width <= 0 || window.height <= 0 || window.x < -100000 || [self shouldClipOutPlugin]) {

        // The following code tries to give plug-ins the same size they will eventually have.
        // The specifiedWidth and specifiedHeight variables are used to predict the size that
        // WebCore will eventually resize us to.

        // The QuickTime plug-in has problems if you give it a width or height of 0.
        // Since other plug-ins also might have the same sort of trouble, we make sure
        // to always give plug-ins a size other than 0,0.

        if (window.width <= 0)
            window.width = specifiedWidth > 0 ? specifiedWidth : 100;
        if (window.height <= 0)
            window.height = specifiedHeight > 0 ? specifiedHeight : 100;

        window.clipRect.bottom = window.clipRect.top;
        window.clipRect.left = window.clipRect.right;
        
        // Core Animation plug-ins need to be updated (with a 0,0,0,0 clipRect) when
        // moved to a background tab. We don't do this for Core Graphics plug-ins as
        // older versions of Flash have historical WebKit-specific code that isn't
        // compatible with this behavior.
        if (drawingModel == NPDrawingModelCoreAnimation)
            getNPRect(NSZeroRect, window.clipRect);
    } else {
        getNPRect(visibleRectInWindow, window.clipRect);
    }
    
    // Save the port state, set up the port for entry into the plugin
    PortState portState;
    switch (drawingModel) {
        case NPDrawingModelCoreGraphics: {            
            if (![self canDraw]) {
                portState = NULL;
                break;
            }
            
            ASSERT([NSView focusView] == self);

            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            CGContextRef context = static_cast<CGContextRef>([[NSGraphicsContext currentContext] graphicsPort]);
            ALLOW_DEPRECATED_DECLARATIONS_END

            PortState_CG *cgPortState = (PortState_CG *)malloc(sizeof(PortState_CG));
            portState = (PortState)cgPortState;
            cgPortState->context = context;

            // Save current graphics context's state; will be restored by -restorePortState:
            CGContextSaveGState(context);

            // Clip to the dirty region if drawing to a window. When drawing to another bitmap context, do not clip.
            if ([NSGraphicsContext currentContext] == [[self currentWindow] graphicsContext]) {
                // Get list of dirty rects from the opaque ancestor -- WebKit does some tricks with invalidation and
                // display to enable z-ordering for NSViews; a side-effect of this is that only the WebHTMLView
                // knows about the true set of dirty rects.
                NSView *opaqueAncestor = [self opaqueAncestor];
                const NSRect *dirtyRects;
                NSInteger count;
                [opaqueAncestor getRectsBeingDrawn:&dirtyRects count:&count];
                Vector<CGRect, 16> convertedDirtyRects;
                convertedDirtyRects.grow(count);
                for (int i = 0; i < count; ++i)
                    reinterpret_cast<NSRect&>(convertedDirtyRects[i]) = [self convertRect:dirtyRects[i] fromView:opaqueAncestor];
                CGContextClipToRects(context, convertedDirtyRects.data(), count);
            }

            break;
        }
          
        case NPDrawingModelCoreAnimation:
            // Just set the port state to a dummy value.
            portState = (PortState)1;
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
        case NPDrawingModelCoreGraphics: {
            ASSERT([NSView focusView] == self);
            
            CGContextRef context = ((PortState_CG *)portState)->context;
            ASSERT(!nPort.cgPort.context || (context == nPort.cgPort.context));
            CGContextRestoreGState(context);
            break;
        }
        
        case NPDrawingModelCoreAnimation:
            ASSERT(portState == (PortState)1);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
    }
}

- (BOOL)sendEvent:(void*)event isDrawRect:(BOOL)eventIsDrawRect
{
    if (![self window])
        return NO;
    ASSERT(event);
       
    if (!_isStarted)
        return NO;

    ASSERT([_pluginPackage.get() pluginFuncs]->event);
    
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

    // Can only send drawRect (updateEvt) to CoreGraphics plugins when actually drawing
    ASSERT((drawingModel != NPDrawingModelCoreGraphics) || !eventIsDrawRect || [NSView focusView] == self);
    
    PortState portState = NULL;
    
    if (drawingModel != NPDrawingModelCoreAnimation && eventIsDrawRect) {
        // In CoreGraphics mode, the port state only needs to be saved/set when redrawing the plug-in view.
        // The plug-in is not allowed to draw at any other time.
        portState = [self saveAndSetNewPortStateForUpdate:eventIsDrawRect];
        // We may have changed the window, so inform the plug-in.
        [self setWindowIfNecessary];
    }

    // Temporarily retain self in case the plug-in view is released while sending an event. 
    [[self retain] autorelease];

    BOOL acceptedEvent;
    [self willCallPlugInFunction];
    // Set the pluginAllowPopup flag.
    ASSERT(_eventHandler);
    {
        JSC::JSLock::DropAllLocks dropAllLocks(commonVM());
        UserGestureIndicator gestureIndicator(_eventHandler->currentEventIsUserGesture() ? Optional<ProcessingUserGestureState>(ProcessingUserGesture) : WTF::nullopt);
        acceptedEvent = [_pluginPackage.get() pluginFuncs]->event(plugin, event);
    }
    [self didCallPlugInFunction];

    if (portState) {
        if ([self currentWindow])
            [self restorePortState:portState];
        if (portState != (PortState)1)
            free(portState);
    }

    return acceptedEvent;
}

- (void)windowFocusChanged:(BOOL)hasFocus
{
    _eventHandler->windowFocusChanged(hasFocus);
}

- (void)sendDrawRectEvent:(NSRect)rect
{
    ASSERT(_eventHandler);
    
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    CGContextRef context = static_cast<CGContextRef>([[NSGraphicsContext currentContext] graphicsPort]);
    ALLOW_DEPRECATED_DECLARATIONS_END
    _eventHandler->drawRect(context, rect);
}

- (void)stopTimers
{
    [super stopTimers];
    
    if (_eventHandler)
        _eventHandler->stopTimers();
    
    if (!timers)
        return;

    for (auto& it: timers->values())
        it->stop();
}

- (void)startTimers
{
    [super startTimers];
    
    // If the plugin is completely obscured (scrolled out of view, for example), then we will
    // send null events at a reduced rate.
    _eventHandler->startTimers(_isCompletelyObscured);
    
    if (!timers)
        return;
    
    for (auto& it: timers->values()) {
        ASSERT(!it->isActive());
        it->start(_isCompletelyObscured);
    }    
}

- (void)focusChanged
{
    // We need to null check the event handler here because
    // the plug-in view can resign focus after it's been stopped
    // and the event handler has been deleted.
    if (_eventHandler)
        _eventHandler->focusChanged(_hasFocus);
}

- (void)mouseDown:(NSEvent *)theEvent
{
    if (!_isStarted)
        return;

    _eventHandler->mouseDown(theEvent);
}

- (void)mouseUp:(NSEvent *)theEvent
{
    if (!_isStarted)
        return;

    _eventHandler->mouseUp(theEvent);
}

- (void)handleMouseEntered:(NSEvent *)theEvent
{
    if (!_isStarted)
        return;

    // Set cursor to arrow. Plugins often handle cursor internally, but those that don't will just get this default one.
    [[NSCursor arrowCursor] set];

    _eventHandler->mouseEntered(theEvent);
}

- (void)handleMouseExited:(NSEvent *)theEvent
{
    if (!_isStarted)
        return;

    _eventHandler->mouseExited(theEvent);
    
    // Set cursor back to arrow cursor.  Because NSCursor doesn't know about changes that the plugin made, we could get confused about what we think the
    // current cursor is otherwise.  Therefore we have no choice but to unconditionally reset the cursor when the mouse exits the plugin.
    [[NSCursor arrowCursor] set];
}

- (void)handleMouseMoved:(NSEvent *)theEvent
{
    if (!_isStarted)
        return;

    _eventHandler->mouseMoved(theEvent);
}
    
- (void)mouseDragged:(NSEvent *)theEvent
{
    if (!_isStarted)
        return;

    _eventHandler->mouseDragged(theEvent);
}

- (void)scrollWheel:(NSEvent *)theEvent
{
    if (!_isStarted) {
        [super scrollWheel:theEvent];
        return;
    }

    if (!_eventHandler->scrollWheel(theEvent))
        [super scrollWheel:theEvent];
}

- (void)keyUp:(NSEvent *)theEvent
{
    if (!_isStarted)
        return;

    _eventHandler->keyUp(theEvent);
}

- (void)keyDown:(NSEvent *)theEvent
{
    if (!_isStarted)
        return;

    _eventHandler->keyDown(theEvent);
}

- (void)flagsChanged:(NSEvent *)theEvent
{
    if (!_isStarted)
        return;

    _eventHandler->flagsChanged(theEvent);
}

- (void)sendModifierEventWithKeyCode:(int)keyCode character:(char)character
{
    if (!_isStarted)
        return;
    
    _eventHandler->syntheticKeyDownWithCommandModifier(keyCode, character);
}

- (void)privateBrowsingModeDidChange
{
    if (!_isStarted)
        return;
    
    NPBool value = _isPrivateBrowsingEnabled;

    [self willCallPlugInFunction];
    {
        JSC::JSLock::DropAllLocks dropAllLocks(commonVM());
        if ([_pluginPackage.get() pluginFuncs]->setvalue)
            [_pluginPackage.get() pluginFuncs]->setvalue(plugin, NPNVprivateModeBool, &value);
    }
    [self didCallPlugInFunction];
}

// MARK: WEB_NETSCAPE_PLUGIN

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
        case NPDrawingModelCoreGraphics:
            if (nPort.cgPort.window != lastSetPort.cgPort.window)
                return NO;
            if (nPort.cgPort.context != lastSetPort.cgPort.context)
                return NO;
        break;
                    
        case NPDrawingModelCoreAnimation:
          if (window.window != lastSetWindow.window)
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
    // A plug-in can only update if it's (1) already been started (2) isn't stopped
    // and (3) is able to draw on-screen. To meet condition (3) the plug-in must not
    // be hidden and be attached to a window. There are two exceptions to this rule:
    //
    // Exception 1: QuickDraw plug-ins must be manually told when to stop writing
    // bits to the window backing store, thus to do so requires a new call to
    // NPP_SetWindow() with an empty NPWindow struct.
    //
    // Exception 2: CoreGraphics plug-ins expect to have their drawable area updated
    // when they are moved to a background tab, via a NPP_SetWindow call. This is
    // accomplished by allowing -saveAndSetNewPortStateForUpdate to "clip-out" the window's
    // clipRect. Flash is curently an exception to this. See 6453738.
    //
    
    if (!_isStarted)
        return;
    
    if (![self canDraw])
        return;

    BOOL didLockFocus = [NSView focusView] != self && [self lockFocusIfCanDraw];

    PortState portState = [self saveAndSetNewPortState];
    if (portState) {
        [self setWindowIfNecessary];
        [self restorePortState:portState];
        if (portState != (PortState)1)
            free(portState);
    } else if (drawingModel == NPDrawingModelCoreGraphics)
        [self setWindowIfNecessary];        

    if (didLockFocus)
        [self unlockFocus];
}

- (void)setWindowIfNecessary
{
    if (!_isStarted) 
        return;
    
    if (![self isNewWindowEqualToOldWindow]) {        
        // Make sure we don't call NPP_HandleEvent while we're inside NPP_SetWindow.
        // We probably don't want more general reentrancy protection; we are really
        // protecting only against this one case, which actually comes up when
        // you first install the SVG viewer plug-in.
        NPError npErr;
        
        BOOL wasInSetWindow = inSetWindow;
        inSetWindow = YES;        
        [self willCallPlugInFunction];
        {
            JSC::JSLock::DropAllLocks dropAllLocks(commonVM());
            npErr = [_pluginPackage.get() pluginFuncs]->setwindow(plugin, &window);
        }
        [self didCallPlugInFunction];
        inSetWindow = wasInSetWindow;

#ifndef NDEBUG
        switch (drawingModel) {
            case NPDrawingModelCoreGraphics:
                LOG(Plugins, "NPP_SetWindow (CoreGraphics): %d, window=%p, context=%p, window.x:%d window.y:%d window.width:%d window.height:%d window.clipRect size:%dx%d",
                npErr, nPort.cgPort.window, nPort.cgPort.context, (int)window.x, (int)window.y, (int)window.width, (int)window.height, 
                    window.clipRect.right - window.clipRect.left, window.clipRect.bottom - window.clipRect.top);
            break;

            case NPDrawingModelCoreAnimation:
                LOG(Plugins, "NPP_SetWindow (CoreAnimation): %d, window=%p window.x:%d window.y:%d window.width:%d window.height:%d",
                npErr, window.window, nPort.cgPort.context, (int)window.x, (int)window.y, (int)window.width, (int)window.height);
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

+ (void)setCurrentPluginView:(WebNetscapePluginView *)view
{
    currentPluginView = view;
}

+ (WebNetscapePluginView *)currentPluginView
{
    return currentPluginView;
}

- (BOOL)createPlugin
{
    // Open the plug-in package so it remains loaded while our plugin uses it
    [_pluginPackage.get() open];
    
    // Initialize drawingModel to an invalid value so that we can detect when the plugin does not specify a drawingModel
    drawingModel = (NPDrawingModel)-1;
    
    // Initialize eventModel to an invalid value so that we can detect when the plugin does not specify an event model.
    eventModel = (NPEventModel)-1;
    
    NPError npErr = [self _createPlugin];
    if (npErr != NPERR_NO_ERROR) {
        LOG_ERROR("NPP_New failed with error: %d", npErr);
        [self _destroyPlugin];
        [_pluginPackage.get() close];
        return NO;
    }
    
    if (drawingModel == (NPDrawingModel)-1)
        drawingModel = NPDrawingModelCoreGraphics;

    if (eventModel == (NPEventModel)-1)
        eventModel = NPEventModelCocoa;

    if (drawingModel == NPDrawingModelCoreAnimation) {
        void *value = 0;
        if ([_pluginPackage.get() pluginFuncs]->getvalue(plugin, NPPVpluginCoreAnimationLayer, &value) == NPERR_NO_ERROR && value) {

            // The plug-in gives us a retained layer.
            _pluginLayer = adoptNS((CALayer *)value);

            BOOL accleratedCompositingEnabled = false;
            accleratedCompositingEnabled = [[[self webView] preferences] acceleratedCompositingEnabled];
            if (accleratedCompositingEnabled) {
                // FIXME: This code can be shared between WebHostedNetscapePluginView and WebNetscapePluginView.
                // Since this layer isn't going to be inserted into a view, we need to create another layer and flip its geometry
                // in order to get the coordinate system right.
                RetainPtr<CALayer> realPluginLayer = WTFMove(_pluginLayer);
                
                _pluginLayer = adoptNS([[CALayer alloc] init]);
                _pluginLayer.get().bounds = realPluginLayer.get().bounds;
                _pluginLayer.get().geometryFlipped = YES;

                realPluginLayer.get().autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
                [_pluginLayer.get() addSublayer:realPluginLayer.get()];

                // Eagerly enter compositing mode, since we know we'll need it. This avoids firing invalidateStyle()
                // for iframes that contain composited plugins at bad times. https://bugs.webkit.org/show_bug.cgi?id=39033
                core([self webFrame])->view()->enterCompositingMode();
                [self element]->invalidateStyleAndLayerComposition();
            } else
                [self setWantsLayer:YES];

            LOG(Plugins, "%@ is using Core Animation drawing model with layer %@", _pluginPackage.get(), _pluginLayer.get());
        }

        ASSERT(_pluginLayer);
    }
    
    // Create the event handler
    _eventHandler = WebNetscapePluginEventHandler::create(self);

    return YES;
}

// FIXME: This method is an ideal candidate to move up to the base class
- (CALayer *)pluginLayer
{
    return _pluginLayer.get();
}

- (void)setLayer:(CALayer *)newLayer
{
    [super setLayer:newLayer];

    if (newLayer && _pluginLayer) {
        _pluginLayer.get().frame = [newLayer frame];
        _pluginLayer.get().autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
        [newLayer addSublayer:_pluginLayer.get()];
    }
}

- (void)loadStream
{
    if ([self _shouldCancelSrcStream])
        return;
    
    if (_loadManually) {
        [self _redeliverStream];
        return;
    }
    
    // If the OBJECT/EMBED tag has no SRC, the URL is passed to us as "".
    // Check for this and don't start a load in this case.
    if (_sourceURL && ![_sourceURL.get() _web_isEmpty]) {
        NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:_sourceURL.get()];
        [request _web_setHTTPReferrer:core([self webFrame])->loader().outgoingReferrer()];
        [self loadRequest:request inTarget:nil withNotifyData:nil sendNotification:NO];
    } 
}

- (BOOL)shouldStop
{
    // If we're already calling a plug-in function, do not call NPP_Destroy().  The plug-in function we are calling
    // may assume that its instance->pdata, or other memory freed by NPP_Destroy(), is valid and unchanged until said
    // plugin-function returns.
    // See <rdar://problem/4480737>.
    if (pluginFunctionCallDepth > 0) {
        shouldStopSoon = YES;
        return NO;
    }

    return YES;
}

- (void)destroyPlugin
{
    // To stop active streams it's necessary to invoke stop() on a copy 
    // of streams. This is because calling WebNetscapePluginStream::stop() also has the side effect
    // of removing a stream from this hash set.
    for (auto& stream : copyToVector(streams))
        stream->stop();

    for (WebFrame *frame in [_pendingFrameLoads keyEnumerator])
        [frame _setInternalLoadDelegate:nil];
    [NSObject cancelPreviousPerformRequestsWithTarget:self];

    // Setting the window type to 0 ensures that NPP_SetWindow will be called if the plug-in is restarted.
    lastSetWindow.type = (NPWindowType)0;
    
    _pluginLayer = nil;
    
    [self _destroyPlugin];
    [_pluginPackage.get() close];
    
    _eventHandler = nullptr;
}

- (NPEventModel)eventModel
{
    return eventModel;
}

- (NPP)plugin
{
    return plugin;
}

- (void)setAttributeKeys:(NSArray *)keys andValues:(NSArray *)values
{
    ASSERT([keys count] == [values count]);
    
    // Convert the attributes to 2 C string arrays.
    // These arrays are passed to NPP_New, but the strings need to be
    // modifiable and live the entire life of the plugin.

    // The Java plug-in requires the first argument to be the base URL
    if ([_MIMEType.get() isEqualToString:@"application/x-java-applet"]) {
        cAttributes = (char **)malloc(([keys count] + 1) * sizeof(char *));
        cValues = (char **)malloc(([values count] + 1) * sizeof(char *));
        cAttributes[0] = strdup("DOCBASE");
        cValues[0] = strdup([_baseURL.get() _web_URLCString]);
        argsCount++;
    } else {
        cAttributes = (char **)malloc([keys count] * sizeof(char *));
        cValues = (char **)malloc([values count] * sizeof(char *));
    }

    BOOL isWMP = [_pluginPackage.get() bundleIdentifier] == "com.microsoft.WMP.defaultplugin";
    
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

// MARK: NSVIEW

- (id)initWithFrame:(NSRect)frame
      pluginPackage:(WebNetscapePluginPackage *)pluginPackage
                URL:(NSURL *)URL
            baseURL:(NSURL *)baseURL
           MIMEType:(NSString *)MIME
      attributeKeys:(NSArray *)keys
    attributeValues:(NSArray *)values
       loadManually:(BOOL)loadManually
            element:(RefPtr<WebCore::HTMLPlugInElement>&&)element
{
    self = [super initWithFrame:frame pluginPackage:pluginPackage URL:URL baseURL:baseURL MIMEType:MIME attributeKeys:keys attributeValues:values loadManually:loadManually element:WTFMove(element)];
    if (!self)
        return nil;

    _pendingFrameLoads = adoptNS([[NSMapTable alloc] initWithKeyOptions:NSPointerFunctionsStrongMemory valueOptions:NSPointerFunctionsStrongMemory capacity:0]);

    // load the plug-in if it is not already loaded
    if (![pluginPackage load]) {
        [self release];
        return nil;
    }

    return self;
}

- (id)initWithFrame:(NSRect)frame
{
    ASSERT_NOT_REACHED();
    return nil;
}

- (void)fini
{
    for (unsigned i = 0; i < argsCount; i++) {
        free(cAttributes[i]);
        free(cValues[i]);
    }
    free(cAttributes);
    free(cValues);
    
    ASSERT(!_eventHandler);
}

- (void)disconnectStream:(WebNetscapePluginStream*)stream
{
    streams.remove(stream);
}

- (void)dealloc
{
    ASSERT(!_isStarted);
    ASSERT(!plugin);

    [self fini];

    [super dealloc];
}

- (void)drawRect:(NSRect)rect
{
    if (_cachedSnapshot) {
        NSRect sourceRect = { NSZeroPoint, [_cachedSnapshot.get() size] };
        [_cachedSnapshot.get() drawInRect:[self bounds] fromRect:sourceRect operation:NSCompositeSourceOver fraction:1];
        return;
    }
    
    if (drawingModel == NPDrawingModelCoreAnimation && (!_snapshotting || ![self supportsSnapshotting]))
        return;

    if (!_isStarted)
        return;
    
    if ([NSGraphicsContext currentContextDrawingToScreen] || _isFlash)
        [self sendDrawRectEvent:rect];
}

- (NPObject *)createPluginScriptableObject
{
    if (![_pluginPackage.get() pluginFuncs]->getvalue || !_isStarted)
        return NULL;
        
    NPObject *value = NULL;
    NPError error;
    [self willCallPlugInFunction];
    {
        JSC::JSLock::DropAllLocks dropAllLocks(commonVM());
        error = [_pluginPackage.get() pluginFuncs]->getvalue(plugin, NPPVpluginScriptableNPObject, &value);
    }
    [self didCallPlugInFunction];
    if (error != NPERR_NO_ERROR)
        return NULL;
    
    return value;
}

- (BOOL)getFormValue:(NSString **)value
{
    if (![_pluginPackage.get() pluginFuncs]->getvalue || !_isStarted)
        return false;
    // Plugins will allocate memory for the buffer by using NPN_MemAlloc().
    char* buffer = NULL;
    NPError error;
    [self willCallPlugInFunction];
    {
        JSC::JSLock::DropAllLocks dropAllLocks(commonVM());
        error = [_pluginPackage.get() pluginFuncs]->getvalue(plugin, NPPVformValue, &buffer);
    }
    [self didCallPlugInFunction];
    if (error != NPERR_NO_ERROR || !buffer)
        return false;
    *value = [[NSString alloc] initWithUTF8String:buffer];
    [_pluginPackage.get() browserFuncs]->memfree(buffer);
    return true;
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

-(void)pluginView:(NSView *)pluginView receivedResponse:(NSURLResponse *)response
{
    ASSERT(_loadManually);
    ASSERT(!_manualStream);

    _manualStream = WebNetscapePluginStream::create(&core([self webFrame])->loader());
}

- (void)pluginView:(NSView *)pluginView receivedData:(NSData *)data
{
    ASSERT(_loadManually);
    ASSERT(_manualStream);
    
    _dataLengthReceived += [data length];
    
    if (!_isStarted)
        return;

    if (!_manualStream->plugin()) {
        // Check if the load should be cancelled
        if ([self _shouldCancelSrcStream]) {
            NSURLResponse *response = [[self dataSource] response];
            
            NSError *error = [[NSError alloc] _initWithPluginErrorCode:WebKitErrorPlugInWillHandleLoad
                                                            contentURL:[response URL]
                                                         pluginPageURL:nil
                                                            pluginName:nil // FIXME: Get this from somewhere
                                                              MIMEType:[response MIMEType]];
            [[self dataSource] _documentLoader]->cancelMainResourceLoad(error);
            [error release];
            return;
        }
        
        _manualStream->setRequestURL([[[self dataSource] request] URL]);
        _manualStream->setPlugin([self plugin]);
        ASSERT(_manualStream->plugin());
        
        _manualStream->startStreamWithResponse([[self dataSource] response]);
    }

    if (_manualStream->plugin())
        _manualStream->didReceiveData(0, static_cast<const char *>([data bytes]), [data length]);
}

- (void)pluginView:(NSView *)pluginView receivedError:(NSError *)error
{
    ASSERT(_loadManually);

    _error = error;
    
    if (!_isStarted) {
        return;
    }

    _manualStream->destroyStreamWithError(error);
}

- (void)pluginViewFinishedLoading:(NSView *)pluginView 
{
    ASSERT(_loadManually);
    ASSERT(_manualStream);
    
    if (_isStarted)
        _manualStream->didFinishLoading(0);
}

- (NSTextInputContext *)inputContext
{
    return nil;
}

@end

@implementation WebNetscapePluginView (WebNPPCallbacks)

- (void)evaluateJavaScriptPluginRequest:(WebPluginRequest *)JSPluginRequest
{
    // FIXME: Is this isStarted check needed here? evaluateJavaScriptPluginRequest should not be called
    // if we are stopped since this method is called after a delay and we call 
    // cancelPreviousPerformRequestsWithTarget inside of stop.
    if (!_isStarted) {
        return;
    }
    
    NSURL *URL = [[JSPluginRequest request] URL];
    NSString *JSString = [URL _webkit_scriptIfJavaScriptURL];
    ASSERT(JSString);
    
    NSString *result = [[self webFrame] _stringByEvaluatingJavaScriptFromString:JSString forceUserGesture:[JSPluginRequest isCurrentEventUserGesture]];
    
    // Don't continue if stringByEvaluatingJavaScriptFromString caused the plug-in to stop.
    if (!_isStarted) {
        return;
    }
        
    if ([JSPluginRequest frameName] != nil) {
        // FIXME: If the result is a string, we probably want to put that string into the frame.
        if ([JSPluginRequest sendNotification]) {
            [self willCallPlugInFunction];
            {
                JSC::JSLock::DropAllLocks dropAllLocks(commonVM());
                [_pluginPackage.get() pluginFuncs]->urlnotify(plugin, [URL _web_URLCString], NPRES_DONE, [JSPluginRequest notifyData]);
            }
            [self didCallPlugInFunction];
        }
    } else if ([result length] > 0) {
        // Don't call NPP_NewStream and other stream methods if there is no JS result to deliver. This is what Mozilla does.
        NSData *JSData = [result dataUsingEncoding:NSUTF8StringEncoding];
        
        auto stream = WebNetscapePluginStream::create([NSURLRequest requestWithURL:URL], plugin, [JSPluginRequest sendNotification], [JSPluginRequest notifyData]);
        
        RetainPtr<NSURLResponse> response = adoptNS([[NSURLResponse alloc] initWithURL:URL 
                                                                             MIMEType:@"text/plain" 
                                                                expectedContentLength:[JSData length]
                                                                     textEncodingName:nil]);
        
        stream->startStreamWithResponse(response.get());
        stream->didReceiveData(0, static_cast<const char*>([JSData bytes]), [JSData length]);
        stream->didFinishLoading(0);
    }
}

- (void)webFrame:(WebFrame *)webFrame didFinishLoadWithReason:(NPReason)reason
{
    ASSERT(_isStarted);
    
    WebPluginRequest *pluginRequest = [_pendingFrameLoads objectForKey:webFrame];
    ASSERT(pluginRequest != nil);
    ASSERT([pluginRequest sendNotification]);
        
    [self willCallPlugInFunction];
    {
        JSC::JSLock::DropAllLocks dropAllLocks(commonVM());
        [_pluginPackage.get() pluginFuncs]->urlnotify(plugin, [[[pluginRequest request] URL] _web_URLCString], reason, [pluginRequest notifyData]);
    }
    [self didCallPlugInFunction];
    
    [_pendingFrameLoads removeObjectForKey:webFrame];
    [webFrame _setInternalLoadDelegate:nil];
}

- (void)webFrame:(WebFrame *)webFrame didFinishLoadWithError:(NSError *)error
{
    NPReason reason = NPRES_DONE;
    if (error != nil)
        reason = WebNetscapePluginStream::reasonForError(error);
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
        frame = kit(core([self webFrame])->loader().findFrameForNavigation(frameName));
        if (frame == nil) {
            WebView *currentWebView = [self webView];
            NSDictionary *features = [[NSDictionary alloc] init];
            WebView *newWebView = [[currentWebView _UIDelegateForwarder] webView:currentWebView
                                                        createWebViewWithRequest:nil
                                                                  windowFeatures:features];
            [features release];

            if (!newWebView) {
                if ([pluginRequest sendNotification]) {
                    [self willCallPlugInFunction];
                    {
                        JSC::JSLock::DropAllLocks dropAllLocks(commonVM());
                        [_pluginPackage.get() pluginFuncs]->urlnotify(plugin, [[[pluginRequest request] URL] _web_URLCString], NPERR_GENERIC_ERROR, [pluginRequest notifyData]);
                    }
                    [self didCallPlugInFunction];
                }
                return;
            }
            
            frame = [newWebView mainFrame];
            core(frame)->tree().setName(frameName);
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
            WebNetscapePluginView *view = [frame _internalLoadDelegate];
            if (view != nil) {
                ASSERT([view isKindOfClass:[WebNetscapePluginView class]]);
                [view webFrame:frame didFinishLoadWithReason:NPRES_USER_BREAK];
            }
            [_pendingFrameLoads setObject:pluginRequest forKey:frame];
            [frame _setInternalLoadDelegate:self];
        }
    }
}

- (NPError)loadRequest:(NSMutableURLRequest *)request inTarget:(const char *)cTarget withNotifyData:(void *)notifyData sendNotification:(BOOL)sendNotification
{
    NSURL *URL = [request URL];

    if (!URL) 
        return NPERR_INVALID_URL;

    // Don't allow requests to be loaded when the document loader is stopping all loaders.
    if ([[self dataSource] _documentLoader]->isStopping())
        return NPERR_GENERIC_ERROR;
    
    NSString *target = nil;
    if (cTarget) {
        // Find the frame given the target string.
        target = [NSString stringWithCString:cTarget encoding:NSISOLatin1StringEncoding];
    }
    WebFrame *frame = [self webFrame];

    // don't let a plugin start any loads if it is no longer part of a document that is being 
    // displayed unless the loads are in the same frame as the plugin.
    if ([[self dataSource] _documentLoader] != core([self webFrame])->loader().activeDocumentLoader() &&
        (!cTarget || [frame findFrameNamed:target] != frame)) {
        return NPERR_GENERIC_ERROR; 
    }
    
    NSString *JSString = [URL _webkit_scriptIfJavaScriptURL];
    if (JSString != nil) {
        if (![[[self webView] preferences] isJavaScriptEnabled]) {
            // Return NPERR_GENERIC_ERROR if JS is disabled. This is what Mozilla does.
            return NPERR_GENERIC_ERROR;
        } else if (cTarget == NULL && _mode == NP_FULL) {
            // Don't allow a JavaScript request from a standalone plug-in that is self-targetted
            // because this can cause the user to be redirected to a blank page (3424039).
            return NPERR_INVALID_PARAM;
        }
    } else {
        if (!core([self webFrame])->document()->securityOrigin().canDisplay(URL))
            return NPERR_GENERIC_ERROR;
    }
        
    if (cTarget || JSString) {
        // Make when targeting a frame or evaluating a JS string, perform the request after a delay because we don't
        // want to potentially kill the plug-in inside of its URL request.
        
        if (JSString && target && [frame findFrameNamed:target] != frame) {
            // For security reasons, only allow JS requests to be made on the frame that contains the plug-in.
            return NPERR_INVALID_PARAM;
        }
        
        bool currentEventIsUserGesture = false;
        if (_eventHandler)
            currentEventIsUserGesture = _eventHandler->currentEventIsUserGesture();
        
        WebPluginRequest *pluginRequest = [[WebPluginRequest alloc] initWithRequest:request 
                                                                          frameName:target
                                                                         notifyData:notifyData 
                                                                   sendNotification:sendNotification
                                                            didStartFromUserGesture:currentEventIsUserGesture];
        [self performSelector:@selector(loadPluginRequest:) withObject:pluginRequest afterDelay:0];
        [pluginRequest release];
    } else {
        auto stream = WebNetscapePluginStream::create(request, plugin, sendNotification, notifyData);

        streams.add(stream.copyRef());
        stream->start();
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
        auto bufString = adoptCF(CFStringCreateWithCString(kCFAllocatorDefault, buf, kCFStringEncodingWindowsLatin1));
        if (!bufString) {
            return NPERR_INVALID_PARAM;
        }
        NSURL *fileURL = [NSURL _web_URLWithDataAsString:(__bridge NSString *)bufString.get()];
        NSString *path;
        if ([fileURL isFileURL]) {
            path = [fileURL path];
        } else {
            path = (__bridge NSString *)bufString.get();
        }
        postData = [NSData dataWithContentsOfFile:path];
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
            NSInteger location = [postData _web_locationAfterFirstBlankLine];
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
                    dataLength = std::min<unsigned>([contentLength intValue], dataLength);
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
    // This function does a sanity check to ensure that the NPStream provided actually
    // belongs to the plug-in that provided it, which fixes a crash in the DivX 
    // plug-in: <rdar://problem/5093862> | http://bugs.webkit.org/show_bug.cgi?id=13203
    if (!stream || WebNetscapePluginStream::ownerForStream(stream) != plugin) {
        LOG(Plugins, "Invalid NPStream passed to NPN_DestroyStream: %p", stream);
        return NPERR_INVALID_INSTANCE_ERROR;
    }
    
    WebNetscapePluginStream* browserStream = static_cast<WebNetscapePluginStream*>(stream->ndata);
    browserStream->cancelLoadAndDestroyStreamWithError(browserStream->errorForReason(reason));
    
    return NPERR_NO_ERROR;
}

- (const char *)userAgent
{
    NSString *userAgent = [[self webView] userAgentForURL:_baseURL.get()];
    
    if (_isSilverlight) {
        // Silverlight has a workaround for a leak in Safari 2. This workaround is 
        // applied when the user agent does not contain "Version/3" so we append it
        // at the end of the user agent.
        userAgent = [userAgent stringByAppendingString:@" Version/3.2.1"];
    }        
        
    return [userAgent UTF8String];
}

-(void)status:(const char *)message
{    
    CFStringRef status = CFStringCreateWithCString(NULL, message ? message : "", kCFStringEncodingUTF8);
    if (!status) {
        LOG_ERROR("NPN_Status: the message was not valid UTF-8");
        return;
    }
    
    LOG(Plugins, "NPN_Status: %@", status);
    WebView *wv = [self webView];
    [[wv _UIDelegateForwarder] webView:wv setStatusText:(__bridge NSString *)status];
    CFRelease(status);
}

-(void)invalidateRect:(NPRect *)invalidRect
{
    LOG(Plugins, "NPN_InvalidateRect");
    [self invalidatePluginContentRect:NSMakeRect(invalidRect->left, invalidRect->top,
        (float)invalidRect->right - invalidRect->left, (float)invalidRect->bottom - invalidRect->top)];
}

- (void)invalidateRegion:(NPRegion)invalidRegion
{
    LOG(Plugins, "NPN_InvalidateRegion");
    NSRect invalidRect = NSZeroRect;
    switch (drawingModel) {
        case NPDrawingModelCoreGraphics:
        {
            CGRect cgRect = CGPathGetBoundingBox((NPCGRegion)invalidRegion);
            invalidRect = *(NSRect*)&cgRect;
            break;
        }
        default:
            ASSERT_NOT_REACHED();
        break;
    }
    
    [self invalidatePluginContentRect:invalidRect];
}

-(void)forceRedraw
{
    LOG(Plugins, "forceRedraw");
    [self invalidatePluginContentRect:[self bounds]];
    [[self window] displayIfNeeded];
}

- (NPError)getVariable:(NPNVariable)variable value:(void *)value
{
    switch (static_cast<unsigned>(variable)) {
        case NPNVWindowNPObject:
        {
            Frame* frame = core([self webFrame]);
            NPObject* windowScriptObject = frame ? frame->script().windowScriptNPObject() : 0;

            // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugins/npruntime.html#browseraccess>
            if (windowScriptObject)
                _NPN_RetainObject(windowScriptObject);
            
            void **v = (void **)value;
            *v = windowScriptObject;

            return NPERR_NO_ERROR;
        }

        case NPNVPluginElementNPObject:
        {
            if (!_elementNPObject) {
                if (!_element)
                    return NPERR_GENERIC_ERROR;

                Frame* frame = core(self.webFrame);
                if (!frame)
                    return NPERR_GENERIC_ERROR;

                JSC::JSObject* object = frame->script().jsObjectForPluginElement(_element.get());
                if (!object)
                    _elementNPObject = _NPN_CreateNoScriptObject();
                else
                    _elementNPObject = _NPN_CreateScriptObject(0, object, frame->script().bindingRootObject());
            }

            // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugins/npruntime.html#browseraccess>
            if (_elementNPObject)
                _NPN_RetainObject(_elementNPObject);

            *(void **)value = _elementNPObject;

            return NPERR_NO_ERROR;
        }
        
        case NPNVpluginDrawingModel:
        {
            *(NPDrawingModel *)value = drawingModel;
            return NPERR_NO_ERROR;
        }

        case NPNVsupportsCoreGraphicsBool:
        {
            *(NPBool *)value = TRUE;
            return NPERR_NO_ERROR;
        }

        case NPNVsupportsOpenGLBool:
        {
            *(NPBool *)value = FALSE;
            return NPERR_NO_ERROR;
        }
        
        case NPNVsupportsCoreAnimationBool:
        {
            *(NPBool *)value = TRUE;
            return NPERR_NO_ERROR;
        }

        case NPNVsupportsCocoaBool:
        {
            *(NPBool *)value = TRUE;
            return NPERR_NO_ERROR;
        }

        case NPNVprivateModeBool:
        {
            *(NPBool *)value = _isPrivateBrowsingEnabled;
            return NPERR_NO_ERROR;
        }

        case WKNVSupportsCompositingCoreAnimationPluginsBool:
        {
            *(NPBool *)value = [[[self webView] preferences] acceleratedCompositingEnabled];
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
        case NPPVpluginDrawingModel:
        {
            // Can only set drawing model inside NPP_New()
            if (self != [[self class] currentPluginView])
                return NPERR_GENERIC_ERROR;
            
            // Check for valid, supported drawing model
            NPDrawingModel newDrawingModel = (NPDrawingModel)(uintptr_t)value;
            switch (newDrawingModel) {
                // Supported drawing models:
                case NPDrawingModelCoreGraphics:
                case NPDrawingModelCoreAnimation:
                    drawingModel = newDrawingModel;
                    return NPERR_NO_ERROR;
                    

                // Unsupported (or unknown) drawing models:
                default:
                    LOG(Plugins, "Plugin %@ uses unsupported drawing model: %d", _eventHandler.get(), drawingModel);
                    return NPERR_GENERIC_ERROR;
            }
        }
        
        case NPPVpluginEventModel:
        {
            // Can only set event model inside NPP_New()
            if (self != [[self class] currentPluginView])
                return NPERR_GENERIC_ERROR;
            
            // Check for valid, supported event model
            NPEventModel newEventModel = (NPEventModel)(uintptr_t)value;
            switch (newEventModel) {
                // Supported event models:
                case NPEventModelCocoa:
                    eventModel = newEventModel;
                    return NPERR_NO_ERROR;
                    
                    // Unsupported (or unknown) event models:
                default:
                    LOG(Plugins, "Plugin %@ uses unsupported event model: %d", _eventHandler.get(), eventModel);
                    return NPERR_GENERIC_ERROR;
            }
        }
            
        default:
            return NPERR_GENERIC_ERROR;
    }
}

- (uint32_t)scheduleTimerWithInterval:(uint32_t)interval repeat:(NPBool)repeat timerFunc:(void (*)(NPP npp, uint32_t timerID))timerFunc
{
    if (!timerFunc)
        return 0;
    
    if (!timers)
        timers = std::make_unique<HashMap<uint32_t, std::unique_ptr<PluginTimer>>>();

    std::unique_ptr<PluginTimer>* slot;
    uint32_t timerID;
    do
        timerID = ++currentTimerID;
    while (!timers->isValidKey(timerID) || *(slot = &timers->add(timerID, nullptr).iterator->value));

    auto timer = std::make_unique<PluginTimer>(plugin, timerID, interval, repeat, timerFunc);

    if (_shouldFireTimers)
        timer->start(_isCompletelyObscured);
    
    *slot = WTFMove(timer);

    return timerID;
}

- (void)unscheduleTimer:(uint32_t)timerID
{
    if (!timers)
        return;
    
    timers->remove(timerID);
}

- (NPError)popUpContextMenu:(NPMenu *)menu
{
    NSEvent *currentEvent = [NSApp currentEvent];
    
    // NPN_PopUpContextMenu must be called from within the plug-in's NPP_HandleEvent.
    if (!currentEvent)
        return NPERR_GENERIC_ERROR;
    
    [NSMenu popUpContextMenu:(__bridge NSMenu *)menu withEvent:currentEvent forView:self];
    return NPERR_NO_ERROR;
}

- (NPError)getVariable:(NPNURLVariable)variable forURL:(const char*)url value:(char**)value length:(uint32_t*)length
{
    switch (variable) {
        case NPNURLVCookie: {
            if (!value)
                break;
            
            NSURL *URL = [self URLWithCString:url];
            if (!URL)
                break;
            
            if (Frame* frame = core([self webFrame])) {
                auto* document = frame->document();
                if (!document)
                    break;
                
                auto* page = document->page();
                if (!page)
                    break;

                String cookieString = page->cookieJar().cookies(*document, URL);
                CString cookieStringUTF8 = cookieString.utf8();
                if (cookieStringUTF8.isNull())
                    return NPERR_GENERIC_ERROR;

                *value = static_cast<char*>(NPN_MemAlloc(cookieStringUTF8.length()));
                memcpy(*value, cookieStringUTF8.data(), cookieStringUTF8.length());
                
                if (length)
                    *length = cookieStringUTF8.length();
                return NPERR_NO_ERROR;
            }
            break;
        }
        case NPNURLVProxy: {
            if (!value)
                break;
            
            NSURL *URL = [self URLWithCString:url];
            if (!URL)
                break;

            Vector<ProxyServer> proxyServers = proxyServersForURL(URL);
            CString proxiesUTF8 = toString(proxyServers).utf8();
            
            *value = static_cast<char*>(NPN_MemAlloc(proxiesUTF8.length()));
            memcpy(*value, proxiesUTF8.data(), proxiesUTF8.length());
            
           if (length)
               *length = proxiesUTF8.length();
            
            return NPERR_NO_ERROR;
        }
    }
    return NPERR_GENERIC_ERROR;
}

- (NPError)setVariable:(NPNURLVariable)variable forURL:(const char*)url value:(const char*)value length:(uint32_t)length
{
    switch (variable) {
        case NPNURLVCookie: {
            NSURL *URL = [self URLWithCString:url];
            if (!URL)
                break;
            
            String cookieString = String::fromUTF8(value, length);
            if (!cookieString)
                break;
            
            if (Frame* frame = core([self webFrame])) {
                if (auto* document = frame->document()) {
                    if (auto* page = document->page())
                        page->cookieJar().setCookies(*document, URL, cookieString);
                }
                return NPERR_NO_ERROR;
            }
            
            break;
        }
        case NPNURLVProxy:
            // Can't set the proxy for a URL.
            break;
    }
    return NPERR_GENERIC_ERROR;
}

- (NPError)getAuthenticationInfoWithProtocol:(const char*)protocolStr host:(const char*)hostStr port:(int32_t)port scheme:(const char*)schemeStr realm:(const char*)realmStr
                                    username:(char**)usernameStr usernameLength:(uint32_t*)usernameLength 
                                    password:(char**)passwordStr passwordLength:(uint32_t*)passwordLength
{
    if (!protocolStr || !hostStr || !schemeStr || !realmStr || !usernameStr || !usernameLength || !passwordStr || !passwordLength)
        return NPERR_GENERIC_ERROR;
  
    CString username;
    CString password;
    if (!getAuthenticationInfo(protocolStr, hostStr, port, schemeStr, realmStr, username, password))
        return NPERR_GENERIC_ERROR;
    
    *usernameLength = username.length();
    *usernameStr = static_cast<char*>(NPN_MemAlloc(username.length()));
    memcpy(*usernameStr, username.data(), username.length());
    
    *passwordLength = password.length();
    *passwordStr = static_cast<char*>(NPN_MemAlloc(password.length()));
    memcpy(*passwordStr, password.data(), password.length());
    
    return NPERR_NO_ERROR;
}

@end

@implementation WebNetscapePluginView (Internal)

- (BOOL)_shouldCancelSrcStream
{
    ASSERT(_isStarted);
    
    // Check if we should cancel the load
    NPBool cancelSrcStream = 0;
    if ([_pluginPackage.get() pluginFuncs]->getvalue &&
        [_pluginPackage.get() pluginFuncs]->getvalue(plugin, NPPVpluginCancelSrcStream, &cancelSrcStream) == NPERR_NO_ERROR && cancelSrcStream)
        return YES;
    
    return NO;
}

- (NPError)_createPlugin
{
    plugin = (NPP)calloc(1, sizeof(NPP_t));
    plugin->ndata = self;

    ASSERT([_pluginPackage.get() pluginFuncs]->newp);

    // NPN_New(), which creates the plug-in instance, should never be called while calling a plug-in function for that instance.
    ASSERT(pluginFunctionCallDepth == 0);

    _isFlash = [_pluginPackage.get() bundleIdentifier] == "com.macromedia.Flash Player.plugin";
    _isSilverlight = [_pluginPackage.get() bundleIdentifier] == "com.microsoft.SilverlightPlugin";

    [[self class] setCurrentPluginView:self];
    NPError npErr = [_pluginPackage.get() pluginFuncs]->newp(const_cast<char*>([_MIMEType.get() cString]), plugin, _mode, argsCount, cAttributes, cValues, NULL);
    [[self class] setCurrentPluginView:nil];
    LOG(Plugins, "NPP_New: %d", npErr);
    return npErr;
}

- (void)_destroyPlugin
{
    NPError npErr;
    npErr = ![_pluginPackage.get() pluginFuncs]->destroy(plugin, NULL);
    LOG(Plugins, "NPP_Destroy: %d", npErr);

    if (_elementNPObject)
        _NPN_ReleaseObject(_elementNPObject);

    if (Frame* frame = core([self webFrame]))
        frame->script().cleanupScriptObjectsForPlugin(self);
        
    free(plugin);
    plugin = NULL;
}

- (void)_redeliverStream
{
    if ([self dataSource] && _isStarted) {
        // Deliver what has not been passed to the plug-in up to this point.
        if (_dataLengthReceived > 0) {
            NSData *data = [[[self dataSource] data] subdataWithRange:NSMakeRange(0, _dataLengthReceived)];
            _dataLengthReceived = 0;
            [self pluginView:self receivedData:data];
            if (![[self dataSource] isLoading]) {
                if (_error)
                    [self pluginView:self receivedError:_error.get()];
                else
                    [self pluginViewFinishedLoading:self];
            }
        }
    }
}

@end

#endif
