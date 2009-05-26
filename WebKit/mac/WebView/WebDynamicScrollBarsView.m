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

static const unsigned cMaxUpdateScrollbarsPass = 2;

- (void)updateScrollers
{
    BOOL hasVerticalScroller = [self hasVerticalScroller];
    BOOL hasHorizontalScroller = [self hasHorizontalScroller];
    
    BOOL newHasHorizontalScroller = hasHorizontalScroller;
    BOOL newHasVerticalScroller = hasVerticalScroller;
    
    BOOL needsLayout = NO;

    NSView *documentView = [self documentView];
    if (!documentView) {
        newHasHorizontalScroller = NO;
        newHasVerticalScroller = NO;
    } 

    if (hScroll != ScrollbarAuto)
        newHasHorizontalScroller = (hScroll == ScrollbarAlwaysOn);
    if (vScroll != ScrollbarAuto)
        newHasVerticalScroller = (vScroll == ScrollbarAlwaysOn);
    
    if (!documentView || suppressLayout || suppressScrollers || (hScroll != ScrollbarAuto && vScroll != ScrollbarAuto)) {
        inUpdateScrollers = YES;
        if (hasHorizontalScroller != newHasHorizontalScroller)
            [self setHasHorizontalScroller:newHasHorizontalScroller];
        if (hasVerticalScroller != newHasVerticalScroller)
            [self setHasVerticalScroller:newHasVerticalScroller];
        if (suppressScrollers) {
            [[self verticalScroller] setNeedsDisplay:NO];
            [[self horizontalScroller] setNeedsDisplay:NO];
        }
        inUpdateScrollers = NO;
        return;
    }

    needsLayout = NO;

    // If we came in here with the view already needing a layout, then go ahead and do that
    // first.  (This will be the common case, e.g., when the page changes due to window resizing for example).
    // This layout will not re-enter updateScrollers and does not count towards our max layout pass total.
    if ([documentView isKindOfClass:[WebHTMLView class]]) {
        WebHTMLView* htmlView = (WebHTMLView*)documentView;
        if ([htmlView _needsLayout]) {
            inUpdateScrollers = YES;
            [(id <WebDocumentView>)documentView layout];
            inUpdateScrollers = NO;
        }
    }

    NSSize documentSize = [documentView frame].size;
    NSSize visibleSize = [self documentVisibleRect].size;
    NSSize frameSize = [self frame].size;

    if (hScroll == ScrollbarAuto) {
        newHasHorizontalScroller = documentSize.width > visibleSize.width;
        if (newHasHorizontalScroller && !inUpdateScrollersLayoutPass && documentSize.height <= frameSize.height && documentSize.width <= frameSize.width)
            newHasHorizontalScroller = NO;
    }
    
    if (vScroll == ScrollbarAuto) {
        newHasVerticalScroller = documentSize.height > visibleSize.height;
        if (newHasVerticalScroller && !inUpdateScrollersLayoutPass && documentSize.height <= frameSize.height && documentSize.width <= frameSize.width)
            newHasVerticalScroller = NO;
    }

    // Unless in ScrollbarsAlwaysOn mode, if we ever turn one scrollbar off, always turn the other one off too.
    // Never ever try to both gain/lose a scrollbar in the same pass.
    if (!newHasHorizontalScroller && hasHorizontalScroller && vScroll != ScrollbarAlwaysOn)
        newHasVerticalScroller = NO;
    if (!newHasVerticalScroller && hasVerticalScroller && hScroll != ScrollbarAlwaysOn)
        newHasHorizontalScroller = NO;

    if (hasHorizontalScroller != newHasHorizontalScroller) {
        inUpdateScrollers = YES;
        [self setHasHorizontalScroller:newHasHorizontalScroller];
        inUpdateScrollers = NO;
        needsLayout = YES;
    }

    if (hasVerticalScroller != newHasVerticalScroller) {
        inUpdateScrollers = YES;
        [self setHasVerticalScroller:newHasVerticalScroller];
        inUpdateScrollers = NO;
        needsLayout = YES;
    }

    if (needsLayout && inUpdateScrollersLayoutPass < cMaxUpdateScrollbarsPass && 
        [documentView conformsToProtocol:@protocol(WebDocumentView)]) {
        inUpdateScrollersLayoutPass++;
        [(id <WebDocumentView>)documentView setNeedsLayout:YES];
        [(id <WebDocumentView>)documentView layout];
        NSSize newDocumentSize = [documentView frame].size;
        if (NSEqualSizes(documentSize, newDocumentSize)) {
            // The layout with the new scroll state had no impact on
            // the document's overall size, so updateScrollers didn't get called.
            // Recur manually.
            [self updateScrollers];
        }
        inUpdateScrollersLayoutPass--;
    }
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
