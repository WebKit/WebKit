/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
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

#import "WebDynamicScrollBarsViewInternal.h"

#import "WebDocument.h"
#import "WebFrameInternal.h"
#import "WebFrameView.h"
#import "WebHTMLViewInternal.h"
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebKitSystemInterface.h>

using namespace WebCore;

// FIXME: <rdar://problem/5898985> Mail expects a constant of this name to exist.
const int WebCoreScrollbarAlwaysOn = ScrollbarAlwaysOn;

@implementation WebDynamicScrollBarsView

- (void)setAllowsHorizontalScrolling:(BOOL)flag
{
    if (hScrollModeLocked)
        return;
    if (flag && hScroll == ScrollbarAlwaysOff)
        hScroll = ScrollbarAuto;
    else if (!flag && hScroll != ScrollbarAlwaysOff)
        hScroll = ScrollbarAlwaysOff;
    [self updateScrollers];
}

@end

@implementation WebDynamicScrollBarsView (WebInternal)

- (void)setSuppressLayout:(BOOL)flag;
{
    suppressLayout = flag;
}

- (void)setScrollBarsSuppressed:(BOOL)suppressed repaintOnUnsuppress:(BOOL)repaint
{
    suppressScrollers = suppressed;

    // This code was originally changes for a Leopard performance imporvement. We decided to 
    // ifdef it to fix correctness issues on Tiger documented in <rdar://problem/5441823>.
#ifndef BUILDING_ON_TIGER
    if (suppressed) {
        [[self verticalScroller] setNeedsDisplay:NO];
        [[self horizontalScroller] setNeedsDisplay:NO];
    }
        
    if (!suppressed && repaint)
        [super reflectScrolledClipView:[self contentView]];
#else
    if (suppressed || repaint) { 
        [[self verticalScroller] setNeedsDisplay: !suppressed]; 
        [[self horizontalScroller] setNeedsDisplay: !suppressed]; 
    }
#endif
}

- (void)updateScrollers
{
    // We need to do the work below twice in the case where a scroll bar disappears,
    // making the second layout have a wider width than the first. Doing it more than
    // twice would indicate some kind of infinite loop, so we do it at most twice.
    // It's quite efficient to do this work twice in the normal case, so we don't bother
    // trying to figure out of the second pass is needed or not.
    if (inUpdateScrollers)
        return;
    
    inUpdateScrollers = true;

    int pass;
    BOOL hasVerticalScroller = [self hasVerticalScroller];
    BOOL hasHorizontalScroller = [self hasHorizontalScroller];
    BOOL oldHasVertical = hasVerticalScroller;
    BOOL oldHasHorizontal = hasHorizontalScroller;

    for (pass = 0; pass < 2; pass++) {
        BOOL scrollsVertically;
        BOOL scrollsHorizontally;

        if (!suppressLayout && !suppressScrollers && (hScroll == ScrollbarAuto || vScroll == ScrollbarAuto)) {
            // Do a layout if pending, before checking if scrollbars are needed.
            // This fixes 2969367, although may introduce a slowdown in live resize performance.
            NSView *documentView = [self documentView];
            if (!documentView) {
                scrollsHorizontally = NO;
                scrollsVertically = NO;
            } else {
                if ((hasVerticalScroller != oldHasVertical ||
                    hasHorizontalScroller != oldHasHorizontal || [documentView inLiveResize]) && [documentView conformsToProtocol:@protocol(WebDocumentView)]) {
                    [(id <WebDocumentView>)documentView setNeedsLayout: YES];
                    [(id <WebDocumentView>)documentView layout];
                }

                NSSize documentSize = [documentView frame].size;
                if ([documentView isKindOfClass:[WebHTMLView class]]) {
                    WebHTMLView *htmlView = (WebHTMLView*)documentView;
                    if (Frame* coreFrame = core([htmlView _frame])) {
                        if (FrameView* coreView = coreFrame->view())
                            documentSize = coreView->minimumContentsSize();
                    }
                }

                NSSize frameSize = [self frame].size;

                scrollsVertically = (vScroll == ScrollbarAlwaysOn) ||
                    (vScroll == ScrollbarAuto && documentSize.height > frameSize.height);
                if (scrollsVertically)
                    scrollsHorizontally = (hScroll == ScrollbarAlwaysOn) ||
                        (hScroll == ScrollbarAuto && documentSize.width + [NSScroller scrollerWidth] > frameSize.width);
                else {
                    scrollsHorizontally = (hScroll == ScrollbarAlwaysOn) ||
                        (hScroll == ScrollbarAuto && documentSize.width > frameSize.width);
                    if (scrollsHorizontally)
                        scrollsVertically = (vScroll == ScrollbarAlwaysOn) ||
                            (vScroll == ScrollbarAuto && documentSize.height + [NSScroller scrollerWidth] > frameSize.height);
                }
            }
        } else {
            scrollsHorizontally = (hScroll == ScrollbarAuto) ? hasHorizontalScroller : (hScroll == ScrollbarAlwaysOn);
            scrollsVertically = (vScroll == ScrollbarAuto) ? hasVerticalScroller : (vScroll == ScrollbarAlwaysOn);
        }

        if (hasVerticalScroller != scrollsVertically) {
            [self setHasVerticalScroller:scrollsVertically];
            hasVerticalScroller = scrollsVertically;
        }

        if (hasHorizontalScroller != scrollsHorizontally) {
            [self setHasHorizontalScroller:scrollsHorizontally];
            hasHorizontalScroller = scrollsHorizontally;
        }
    }

    if (suppressScrollers) {
        [[self verticalScroller] setNeedsDisplay: NO];
        [[self horizontalScroller] setNeedsDisplay: NO];
    }

    inUpdateScrollers = false;
}

// Make the horizontal and vertical scroll bars come and go as needed.
- (void)reflectScrolledClipView:(NSClipView *)clipView
{
    if (clipView == [self contentView]) {
        // FIXME: This hack here prevents infinite recursion that takes place when we
        // gyrate between having a vertical scroller and not having one. A reproducible
        // case is clicking on the "the Policy Routing text" link at
        // http://www.linuxpowered.com/archive/howto/Net-HOWTO-8.html.
        // The underlying cause is some problem in the NSText machinery, but I was not
        // able to pin it down.
        if (!inUpdateScrollers && [[NSGraphicsContext currentContext] isDrawingToScreen])
            [self updateScrollers];
    }

    // This code was originally changed for a Leopard performance imporvement. We decided to 
    // ifdef it to fix correctness issues on Tiger documented in <rdar://problem/5441823>.
#ifndef BUILDING_ON_TIGER
    // Update the scrollers if they're not being suppressed.
    if (!suppressScrollers)
        [super reflectScrolledClipView:clipView];
#else
    [super reflectScrolledClipView:clipView]; 
  
    // Validate the scrollers if they're being suppressed. 
    if (suppressScrollers) { 
        [[self verticalScroller] setNeedsDisplay: NO]; 
        [[self horizontalScroller] setNeedsDisplay: NO]; 
    }
#endif
}

- (BOOL)allowsHorizontalScrolling
{
    return hScroll != ScrollbarAlwaysOff;
}

- (BOOL)allowsVerticalScrolling
{
    return vScroll != ScrollbarAlwaysOff;
}

- (void)scrollingModes:(WebCore::ScrollbarMode*)hMode vertical:(WebCore::ScrollbarMode*)vMode
{
    *hMode = static_cast<ScrollbarMode>(hScroll);
    *vMode = static_cast<ScrollbarMode>(vScroll);
}

- (ScrollbarMode)horizontalScrollingMode
{
    return static_cast<ScrollbarMode>(hScroll);
}

- (ScrollbarMode)verticalScrollingMode
{
    return static_cast<ScrollbarMode>(vScroll);
}

- (void)setHorizontalScrollingMode:(ScrollbarMode)horizontalMode andLock:(BOOL)lock
{
    [self setScrollingModes:horizontalMode vertical:[self verticalScrollingMode] andLock:lock];
}

- (void)setVerticalScrollingMode:(ScrollbarMode)verticalMode andLock:(BOOL)lock
{
    [self setScrollingModes:[self horizontalScrollingMode] vertical:verticalMode andLock:lock];
}

// Mail uses this method, so we cannot remove it. 
- (void)setVerticalScrollingMode:(ScrollbarMode)verticalMode 
{ 
    [self setScrollingModes:[self horizontalScrollingMode] vertical:verticalMode andLock:NO]; 
} 

- (void)setScrollingModes:(ScrollbarMode)horizontalMode vertical:(ScrollbarMode)verticalMode andLock:(BOOL)lock
{
    BOOL update = NO;
    if (verticalMode != vScroll && !vScrollModeLocked) {
        vScroll = verticalMode;
        update = YES;
    }

    if (horizontalMode != hScroll && !hScrollModeLocked) {
        hScroll = horizontalMode;
        update = YES;
    }

    if (lock)
        [self setScrollingModesLocked:YES];

    if (update)
        [self updateScrollers];
}

- (void)setHorizontalScrollingModeLocked:(BOOL)locked
{
    hScrollModeLocked = locked;
}

- (void)setVerticalScrollingModeLocked:(BOOL)locked
{
    vScrollModeLocked = locked;
}

- (void)setScrollingModesLocked:(BOOL)locked
{
    hScrollModeLocked = vScrollModeLocked = locked;
}

- (BOOL)horizontalScrollingModeLocked
{
    return hScrollModeLocked;
}

- (BOOL)verticalScrollingModeLocked
{
    return vScrollModeLocked;
}

- (BOOL)autoforwardsScrollWheelEvents
{
    return YES;
}

- (void)scrollWheel:(NSEvent *)event
{
    float deltaX;
    float deltaY;
    BOOL isContinuous;
    WKGetWheelEventDeltas(event, &deltaX, &deltaY, &isContinuous);

    if (fabsf(deltaY) > fabsf(deltaX)) {
        if (![self allowsVerticalScrolling]) {
            [[self nextResponder] scrollWheel:event];
            return;
        }
    } else if (![self allowsHorizontalScrolling]) {
        [[self nextResponder] scrollWheel:event];
        return;
    }

    [super scrollWheel:event];
}

- (BOOL)accessibilityIsIgnored 
{
    id docView = [self documentView];
    if ([docView isKindOfClass:[WebFrameView class]] && ![(WebFrameView *)docView allowsScrolling])
        return YES;
    
    return [super accessibilityIsIgnored];
}

@end
