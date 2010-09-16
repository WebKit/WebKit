/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WKView.h"

// C API
#import "WKAPICast.h"

// Implementation
#import "ChunkedUpdateDrawingAreaProxy.h"
#import "LayerBackedDrawingAreaProxy.h"
#import "PageClientImpl.h"
#import "RunLoop.h"
#import "WebContext.h"
#import "WebEventFactory.h"
#import "WebPage.h"
#import "WebPageNamespace.h"
#import "WebPageProxy.h"
#import "WebProcessManager.h"
#import "WebProcessProxy.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/IntRect.h>
#import <wtf/RefPtr.h>

using namespace WebKit;
using namespace WebCore;

@interface WKViewData : NSObject {
@public
    OwnPtr<PageClientImpl> _pageClient;
    RefPtr<WebPageProxy> _page;

    // For ToolTips.
    NSToolTipTag _lastToolTipTag;
    id _trackingRectOwner;
    void* _trackingRectUserData;

#if USE(ACCELERATED_COMPOSITING)
    NSView *_layerHostingView;
#endif
}
@end

@implementation WKViewData
@end

@implementation WKView

- (id)initWithFrame:(NSRect)frame pageNamespaceRef:(WKPageNamespaceRef)pageNamespaceRef
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;

    RunLoop::initializeMainRunLoop();

    NSTrackingArea *trackingArea = [[NSTrackingArea alloc] initWithRect:frame
                                                                options:(NSTrackingMouseMoved | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect)
                                                                  owner:self
                                                               userInfo:nil];
    [self addTrackingArea:trackingArea];
    [trackingArea release];

    _data = [[WKViewData alloc] init];

    _data->_pageClient = PageClientImpl::create(self);
    _data->_page = toWK(pageNamespaceRef)->createWebPage();
    _data->_page->setPageClient(_data->_pageClient.get());
    _data->_page->initializeWebPage(IntSize(frame.size), ChunkedUpdateDrawingAreaProxy::create(self));
    _data->_page->setIsInWindow([self window]);

    return self;
}

- (id)initWithFrame:(NSRect)frame
{
    WebContext* context = WebContext::sharedProcessContext();
    self = [self initWithFrame:frame pageNamespaceRef:toRef(context->createPageNamespace())];
    if (!self)
        return nil;

    return self;
}

- (void)dealloc
{
    _data->_page->close();

    [_data release];
    [super dealloc];
}

- (WKPageRef)pageRef
{
    return toRef(_data->_page.get());
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    _data->_page->setFocused(true);
    return YES;
}

- (BOOL)resignFirstResponder
{
    _data->_page->setFocused(false);
    return YES;
}

- (BOOL)isFlipped
{
    return YES;
}

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];

    _data->_page->drawingArea()->setSize(IntSize(size));
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    // FIXME: this needs to be implemented
    return YES;
}

// Editing commands

- (void)copy:(id)sender
{ 
    _data->_page->copy();
}

- (void)cut:(id)sender
{ 
    _data->_page->cut();
}

- (void)paste:(id)sender
{ 
    _data->_page->paste();
}

- (void)selectAll:(id)sender
{
    _data->_page->selectAll();
}

// Events

// Override this so that AppKit will send us arrow keys as key down events so we can
// support them via the key bindings mechanism.
- (BOOL)_wantsKeyDownForEvent:(NSEvent *)event
{
    return YES;
}

- (void)mouseDown:(NSEvent *)theEvent
{
    WebMouseEvent mouseEvent = WebEventFactory::createWebMouseEvent(theEvent, self);
    _data->_page->mouseEvent(mouseEvent);    
}

- (void)mouseUp:(NSEvent *)theEvent
{
    if (!_data->_page->isValid()) {
        _data->_page->revive();
        _data->_page->loadURL(_data->_page->urlAtProcessExit());
        return;
    }

    WebMouseEvent mouseEvent = WebEventFactory::createWebMouseEvent(theEvent, self);
    _data->_page->mouseEvent(mouseEvent);
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    WebMouseEvent mouseEvent = WebEventFactory::createWebMouseEvent(theEvent, self);
    _data->_page->mouseEvent(mouseEvent);
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    WebMouseEvent mouseEvent = WebEventFactory::createWebMouseEvent(theEvent, self);
    _data->_page->mouseEvent(mouseEvent);
}

- (void)scrollWheel:(NSEvent *)theEvent
{
    WebWheelEvent wheelEvent = WebEventFactory::createWebWheelEvent(theEvent, self);
    _data->_page->wheelEvent(wheelEvent);
}

- (void)keyUp:(NSEvent *)theEvent
{
    WebKeyboardEvent keyboardEvent = WebEventFactory::createWebKeyboardEvent(theEvent);
    _data->_page->keyEvent(keyboardEvent);
}

- (void)keyDown:(NSEvent *)theEvent
{
    WebKeyboardEvent keyboardEvent = WebEventFactory::createWebKeyboardEvent(theEvent);
    _data->_page->keyEvent(keyboardEvent);
}

- (void)_updateActiveState
{
    _data->_page->setActive([[self window] isKeyWindow]);
}

- (void)addWindowObserversForWindow:(NSWindow *)window
{
    if (window) {
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowDidBecomeKey:)
            name:NSWindowDidBecomeKeyNotification object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_windowDidResignKey:)
            name:NSWindowDidResignKeyNotification object:nil];
    }
}

- (void)removeWindowObservers
{
    NSWindow *window = [self window];
    if (window) {
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowDidBecomeKeyNotification object:nil];
        [[NSNotificationCenter defaultCenter] removeObserver:self
            name:NSWindowDidResignKeyNotification object:nil];
    }
}

static bool isViewVisible(NSView *view)
{
    if (![view window])
        return false;
    
    if ([view isHiddenOrHasHiddenAncestor])
        return false;
    
    return true;
}

- (void)_updateVisibility
{
    _data->_page->setIsInWindow([self window]);
    if (DrawingAreaProxy* area = _data->_page->drawingArea())
        area->setPageIsVisible(isViewVisible(self));
}

- (void)viewWillMoveToWindow:(NSWindow *)window
{
    if (window != [self window]) {
        [self removeWindowObservers];
        [self addWindowObserversForWindow:window];
    }
}

- (void)viewDidMoveToWindow
{
    // We want to make sure to update the active state while hidden, so if the view is about to become visible, we
    // update the active state first and then make it visible. If the view is about to be hidden, we hide it first and then
    // update the active state.
    if ([self window]) {
        [self _updateActiveState];
        [self _updateVisibility];
    } else {
        [self _updateVisibility];
        [self _updateActiveState];
    }
}

- (void)_windowDidBecomeKey:(NSNotification *)notification
{
    NSWindow *keyWindow = [notification object];
    if (keyWindow == [self window] || keyWindow == [[self window] attachedSheet])
        [self _updateActiveState];
}

- (void)_windowDidResignKey:(NSNotification *)notification
{
    NSWindow *formerKeyWindow = [notification object];
    if (formerKeyWindow == [self window] || formerKeyWindow == [[self window] attachedSheet])
        [self _updateActiveState];
}

- (void)drawRect:(NSRect)rect
{    
    [[NSColor whiteColor] set];
    NSRectFill(rect);

    if (_data->_page->isValid() && _data->_page->drawingArea()) {
        CGContextRef context = static_cast<CGContextRef>([[NSGraphicsContext currentContext] graphicsPort]);
        _data->_page->drawingArea()->paint(IntRect(rect), context);
    }
}

- (BOOL)isOpaque 
{
    // FIXME: Return NO if we have a transparent background.
    return YES;
}

- (void)viewDidHide
{
    [self _updateVisibility];
}

- (void)viewDidUnhide
{
    [self _updateVisibility];
}

@end

@implementation WKView (Internal)

- (void)_processDidExit
{
    [self setNeedsDisplay:YES];
}

- (void)_processDidRevive
{
    _data->_page->reinitializeWebPage(IntSize([self visibleRect].size));

    _data->_page->setActive([[self window] isKeyWindow]);
    _data->_page->setFocused([[self window] firstResponder] == self);
    
    [self setNeedsDisplay:YES];
}

- (void)_takeFocus:(BOOL)forward
{
    if (forward)
        [[self window] selectKeyViewFollowingView:self];
    else
        [[self window] selectKeyViewPrecedingView:self];
}

- (void)_setCursor:(NSCursor *)cursor
{
    if ([NSCursor currentCursor] == cursor)
        return;
    [cursor set];
}

// Any non-zero value will do, but using something recognizable might help us debug some day.
#define TRACKING_RECT_TAG 0xBADFACE

- (NSTrackingRectTag)addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside
{
    ASSERT(_data->_trackingRectOwner == nil);
    _data->_trackingRectOwner = owner;
    _data->_trackingRectUserData = data;
    return TRACKING_RECT_TAG;
}

- (NSTrackingRectTag)_addTrackingRect:(NSRect)rect owner:(id)owner userData:(void *)data assumeInside:(BOOL)assumeInside useTrackingNum:(int)tag
{
    ASSERT(tag == 0 || tag == TRACKING_RECT_TAG);
    ASSERT(_data->_trackingRectOwner == nil);
    _data->_trackingRectOwner = owner;
    _data->_trackingRectUserData = data;
    return TRACKING_RECT_TAG;
}

- (void)_addTrackingRects:(NSRect *)rects owner:(id)owner userDataList:(void **)userDataList assumeInsideList:(BOOL *)assumeInsideList trackingNums:(NSTrackingRectTag *)trackingNums count:(int)count
{
    ASSERT(count == 1);
    ASSERT(trackingNums[0] == 0 || trackingNums[0] == TRACKING_RECT_TAG);
    ASSERT(_data->_trackingRectOwner == nil);
    _data->_trackingRectOwner = owner;
    _data->_trackingRectUserData = userDataList[0];
    trackingNums[0] = TRACKING_RECT_TAG;
}

- (void)removeTrackingRect:(NSTrackingRectTag)tag
{
    if (tag == 0)
        return;
    
    if (_data && (tag == TRACKING_RECT_TAG)) {
        _data->_trackingRectOwner = nil;
        return;
    }
    
    if (_data && (tag == _data->_lastToolTipTag)) {
        [super removeTrackingRect:tag];
        _data->_lastToolTipTag = 0;
        return;
    }
    
    // If any other tracking rect is being removed, we don't know how it was created
    // and it's possible there's a leak involved (see 3500217)
    ASSERT_NOT_REACHED();
}

- (void)_removeTrackingRects:(NSTrackingRectTag *)tags count:(int)count
{
    int i;
    for (i = 0; i < count; ++i) {
        int tag = tags[i];
        if (tag == 0)
            continue;
        ASSERT(tag == TRACKING_RECT_TAG);
        if (_data != nil) {
            _data->_trackingRectOwner = nil;
        }
    }
}

- (void)_sendToolTipMouseExited
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseExited
        location:NSMakePoint(0, 0)
        modifierFlags:0
        timestamp:0
        windowNumber:[[self window] windowNumber]
        context:NULL
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:_data->_trackingRectUserData];
    [_data->_trackingRectOwner mouseExited:fakeEvent];
}

- (void)_sendToolTipMouseEntered
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseEntered
        location:NSMakePoint(0, 0)
        modifierFlags:0
        timestamp:0
        windowNumber:[[self window] windowNumber]
        context:NULL
        eventNumber:0
        trackingNumber:TRACKING_RECT_TAG
        userData:_data->_trackingRectUserData];
    [_data->_trackingRectOwner mouseEntered:fakeEvent];
}

- (NSString *)view:(NSView *)view stringForToolTip:(NSToolTipTag)tag point:(NSPoint)point userData:(void *)data
{
    return nsStringFromWebCoreString(_data->_page->toolTip());
}

- (void)_toolTipChangedFrom:(NSString *)oldToolTip to:(NSString *)newToolTip
{
    if (oldToolTip)
        [self _sendToolTipMouseExited];

    if (newToolTip && [newToolTip length] > 0) {
        // See radar 3500217 for why we remove all tooltips rather than just the single one we created.
        [self removeAllToolTips];
        NSRect wideOpenRect = NSMakeRect(-100000, -100000, 200000, 200000);
        _data->_lastToolTipTag = [self addToolTipRect:wideOpenRect owner:self userData:NULL];
        [self _sendToolTipMouseEntered];
    }
}

#if USE(ACCELERATED_COMPOSITING)
- (void)_startAcceleratedCompositing:(CALayer *)rootLayer
{
    if (!_data->_layerHostingView) {
        NSView *hostingView = [[NSView alloc] initWithFrame:[self bounds]];
#if !defined(BUILDING_ON_LEOPARD)
        [hostingView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
#endif
        
        [self addSubview:hostingView];
        [hostingView release];
        _data->_layerHostingView = hostingView;
    }

    // Make a container layer, which will get sized/positioned by AppKit and CA.
    CALayer *viewLayer = [CALayer layer];

#ifndef NDEBUG
    [viewLayer setName:@"hosting layer"];
#endif

#if defined(BUILDING_ON_LEOPARD)
    // Turn off default animations.
    NSNull *nullValue = [NSNull null];
    NSDictionary *actions = [NSDictionary dictionaryWithObjectsAndKeys:
                             nullValue, @"anchorPoint",
                             nullValue, @"bounds",
                             nullValue, @"contents",
                             nullValue, @"contentsRect",
                             nullValue, @"opacity",
                             nullValue, @"position",
                             nullValue, @"sublayerTransform",
                             nullValue, @"sublayers",
                             nullValue, @"transform",
                             nil];
    [viewLayer setStyle:[NSDictionary dictionaryWithObject:actions forKey:@"actions"]];
#endif

#if !defined(BUILDING_ON_LEOPARD)
    // If we aren't in the window yet, we'll use the screen's scale factor now, and reset the scale 
    // via -viewDidMoveToWindow.
    CGFloat scaleFactor = [self window] ? [[self window] userSpaceScaleFactor] : [[NSScreen mainScreen] userSpaceScaleFactor];
    [viewLayer setTransform:CATransform3DMakeScale(scaleFactor, scaleFactor, 1)];
#endif

    [_data->_layerHostingView setLayer:viewLayer];
    [_data->_layerHostingView setWantsLayer:YES];
    
    // Parent our root layer in the container layer
    [viewLayer addSublayer:rootLayer];
}

- (void)_stopAcceleratedCompositing
{
    if (_data->_layerHostingView) {
        [_data->_layerHostingView setLayer:nil];
        [_data->_layerHostingView setWantsLayer:NO];
        [_data->_layerHostingView removeFromSuperview];
        _data->_layerHostingView = nil;
    }
}

- (void)_switchToDrawingAreaTypeIfNecessary:(DrawingAreaProxy::Type)type
{
    DrawingAreaProxy::Type existingDrawingAreaType = _data->_page->drawingArea() ? _data->_page->drawingArea()->type() : DrawingAreaProxy::None;
    if (existingDrawingAreaType == type)
        return;

    OwnPtr<DrawingAreaProxy> newDrawingArea;
    switch (type) {
        case DrawingAreaProxy::None:
            break;
        case DrawingAreaProxy::ChunkedUpdateDrawingAreaType: {
            newDrawingArea = ChunkedUpdateDrawingAreaProxy::create(self);
            break;
        }
        case DrawingAreaProxy::LayerBackedDrawingAreaType: {
            newDrawingArea = LayerBackedDrawingAreaProxy::create(self);
            break;
        }
    }

    newDrawingArea->setSize(IntSize([self frame].size));

    _data->_page->drawingArea()->detachCompositingContext();
    _data->_page->setDrawingArea(newDrawingArea.release());
}

- (void)_pageDidEnterAcceleratedCompositing
{
    [self _switchToDrawingAreaTypeIfNecessary:DrawingAreaProxy::LayerBackedDrawingAreaType];
}

- (void)_pageDidLeaveAcceleratedCompositing
{
    // FIXME: we may want to avoid flipping back to the non-layer-backed drawing area until the next page load, to avoid thrashing.
    [self _switchToDrawingAreaTypeIfNecessary:DrawingAreaProxy::ChunkedUpdateDrawingAreaType];
}
#endif // USE(ACCELERATED_COMPOSITING)

@end
