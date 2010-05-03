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
#import "DrawingAreaProxyUpdateChunk.h"
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
    RefPtr<WebPageProxy> _page;

    // For ToolTips.
    NSToolTipTag _lastToolTipTag;
    id _trackingRectOwner;
    void* _trackingRectUserData;
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

    _data->_page = toWK(pageNamespaceRef)->createWebPage();
    _data->_page->setPageClient(new PageClientImpl(self));
    _data->_page->initializeWebPage(IntSize(frame.size), new DrawingAreaProxyUpdateChunk(self));

    return self;
}

- (id)initWithFrame:(NSRect)frame
{
    RefPtr<WebContext> context = WebContext::create(ProcessModelSecondaryProcess);
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

- (void)setFrame:(NSRect)frame
{
    [super setFrame:frame];

    _data->_page->drawingArea()->setSize(IntSize(frame.size));
}

// Events

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

- (void)keyUp:(NSEvent *)theEvent;
{
    WebKeyboardEvent keyboardEvent = WebEventFactory::createWebKeyboardEvent(theEvent);
    _data->_page->keyEvent(keyboardEvent);
}

- (void)keyDown:(NSEvent *)theEvent;
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
    _data->_page->drawingArea()->setPageIsVisible(isViewVisible(self));
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
    [self _updateVisibility];
    [self _updateActiveState];
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
    return [[(NSString *)_data->_page->toolTip() copy] autorelease];
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

@end
