/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import <WebKit/WebDynamicScrollBarsView.h>

#import <WebKit/WebDocument.h>
#import <WebKitSystemInterface.h>

@implementation WebDynamicScrollBarsView

- (void)setSuppressLayout: (BOOL)flag;
{
    suppressLayout = flag;
}

- (void)setScrollBarsSuppressed:(BOOL)suppressed repaintOnUnsuppress:(BOOL)repaint
{
    suppressScrollers = suppressed;
    if (suppressed) {
        [[self verticalScroller] setNeedsDisplay:NO];
        [[self horizontalScroller] setNeedsDisplay:NO];
    }
        
    if (!suppressed && repaint)
        [super reflectScrolledClipView:[self contentView]];
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

        if (!suppressLayout && !suppressScrollers && (hScroll == WebCoreScrollbarAuto || vScroll == WebCoreScrollbarAuto)) {
            // Do a layout if pending, before checking if scrollbars are needed.
            // This fixes 2969367, although may introduce a slowdown in live resize performance.
            NSView *documentView = [self documentView];
            if ((hasVerticalScroller != oldHasVertical ||
                hasHorizontalScroller != oldHasHorizontal || [documentView inLiveResize]) && [documentView conformsToProtocol:@protocol(WebDocumentView)]) {
                [(id <WebDocumentView>)documentView setNeedsLayout: YES];
                [(id <WebDocumentView>)documentView layout];
            }

            NSSize documentSize = [documentView frame].size;
            NSSize frameSize = [self frame].size;

            scrollsVertically = (vScroll == WebCoreScrollbarAlwaysOn) ||
                (vScroll == WebCoreScrollbarAuto && documentSize.height > frameSize.height);
            if (scrollsVertically)
                scrollsHorizontally = (hScroll == WebCoreScrollbarAlwaysOn) ||
                    (hScroll == WebCoreScrollbarAuto && documentSize.width + [NSScroller scrollerWidth] > frameSize.width);
            else {
                scrollsHorizontally = (hScroll == WebCoreScrollbarAlwaysOn) ||
                    (hScroll == WebCoreScrollbarAuto && documentSize.width > frameSize.width);
                if (scrollsHorizontally)
                    scrollsVertically = (vScroll == WebCoreScrollbarAlwaysOn) ||
                        (vScroll == WebCoreScrollbarAuto && documentSize.height + [NSScroller scrollerWidth] > frameSize.height);
            }
        } else {
            scrollsHorizontally = (hScroll == WebCoreScrollbarAuto) ? hasHorizontalScroller : (hScroll == WebCoreScrollbarAlwaysOn);
            scrollsVertically = (vScroll == WebCoreScrollbarAuto) ? hasVerticalScroller : (vScroll == WebCoreScrollbarAlwaysOn);
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

    // Update the scrollers if they're not being suppressed.
    if (!suppressScrollers)
        [super reflectScrolledClipView:clipView];
}

- (void)setAllowsScrolling:(BOOL)flag
{
    if (hScrollModeLocked && vScrollModeLocked)
        return;

    if (flag && vScroll == WebCoreScrollbarAlwaysOff)
        vScroll = WebCoreScrollbarAuto;
    else if (!flag && vScroll != WebCoreScrollbarAlwaysOff)
        vScroll = WebCoreScrollbarAlwaysOff;

    if (flag && hScroll == WebCoreScrollbarAlwaysOff)
        hScroll = WebCoreScrollbarAuto;
    else if (!flag && hScroll != WebCoreScrollbarAlwaysOff)
        hScroll = WebCoreScrollbarAlwaysOff;

    [self updateScrollers];
}

- (BOOL)allowsScrolling
{
    // Returns YES if either horizontal or vertical scrolling is allowed.
    return hScroll != WebCoreScrollbarAlwaysOff || vScroll != WebCoreScrollbarAlwaysOff;
}

- (void)setAllowsHorizontalScrolling:(BOOL)flag
{
    if (hScrollModeLocked)
        return;
    if (flag && hScroll == WebCoreScrollbarAlwaysOff)
        hScroll = WebCoreScrollbarAuto;
    else if (!flag && hScroll != WebCoreScrollbarAlwaysOff)
        hScroll = WebCoreScrollbarAlwaysOff;
    [self updateScrollers];
}

- (void)setAllowsVerticalScrolling:(BOOL)flag
{
    if (vScrollModeLocked)
        return;
    if (flag && vScroll == WebCoreScrollbarAlwaysOff)
        vScroll = WebCoreScrollbarAuto;
    else if (!flag && vScroll != WebCoreScrollbarAlwaysOff)
        vScroll = WebCoreScrollbarAlwaysOff;
    [self updateScrollers];
}

- (BOOL)allowsHorizontalScrolling
{
    return hScroll != WebCoreScrollbarAlwaysOff;
}

- (BOOL)allowsVerticalScrolling
{
    return vScroll != WebCoreScrollbarAlwaysOff;
}

-(WebCoreScrollbarMode)horizontalScrollingMode
{
    return hScroll;
}

-(WebCoreScrollbarMode)verticalScrollingMode
{
    return vScroll;
}

- (void)setHorizontalScrollingMode:(WebCoreScrollbarMode)mode
{
    [self setHorizontalScrollingMode:mode andLock:NO];
}

- (void)setHorizontalScrollingMode:(WebCoreScrollbarMode)mode andLock:(BOOL)lock
{
    if (mode == hScroll || hScrollModeLocked)
        return;

    hScroll = mode;

    if (lock)
        [self setHorizontalScrollingModeLocked:YES];

    [self updateScrollers];
}

- (void)setVerticalScrollingMode:(WebCoreScrollbarMode)mode
{
    [self setVerticalScrollingMode:mode andLock:NO];
}

- (void)setVerticalScrollingMode:(WebCoreScrollbarMode)mode andLock:(BOOL)lock
{
    if (mode == vScroll || vScrollModeLocked)
        return;

    vScroll = mode;

    if (lock)
        [self setVerticalScrollingModeLocked:YES];

    [self updateScrollers];
}

- (void)setScrollingMode:(WebCoreScrollbarMode)mode
{
    [self setScrollingMode:mode andLock:NO];
}

- (void)setScrollingMode:(WebCoreScrollbarMode)mode andLock:(BOOL)lock
{
    if ((mode == vScroll && mode == hScroll) || (vScrollModeLocked && hScrollModeLocked))
        return;

    BOOL update = NO;
    if (mode != vScroll && !vScrollModeLocked) {
        vScroll = mode;
        update = YES;
    }

    if (mode != hScroll && !hScrollModeLocked) {
        hScroll = mode;
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

@end
