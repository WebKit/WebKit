//
//  WebDynamicScrollBarsView.m
//  WebKit
//
//  Created by John Sullivan on Tue Jan 22 2002.
//  Copyright (c) 2001, 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebDynamicScrollBarsView.h>

#import <WebKit/WebDocument.h>
#import <WebKit/WebView.h>

@implementation WebDynamicScrollBarsView

- (void)setSuppressLayout: (BOOL)flag;
{
    suppressLayout = flag;
}

- (void)updateScrollers
{
    // We need to do the work below twice in the case where a scroll bar disappears,
    // making the second layout have a wider width than the first. Doing it more than
    // twice would indicate some kind of infinite loop, so we do it at most twice.
    // It's quite efficient to do this work twice in the normal case, so we don't bother
    // trying to figure out of the second pass is needed or not.
    
    int pass;
    BOOL hasVerticalScroller = [self hasVerticalScroller];
    BOOL hasHorizontalScroller = [self hasHorizontalScroller];
    BOOL oldHasVertical = hasVerticalScroller;
    BOOL oldHasHorizontal = hasHorizontalScroller;
    
    if (suppressLayout)
        return; 
        
    for (pass = 0; pass < 2; pass++) {
        BOOL scrollsVertically;
        BOOL scrollsHorizontally;
    
        if (disallowsScrolling) {
            scrollsVertically = NO;
            scrollsHorizontally = NO;
        } else {
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
            
            scrollsVertically = documentSize.height > frameSize.height;
            if (scrollsVertically)
                scrollsHorizontally = documentSize.width + [NSScroller scrollerWidth] > frameSize.width;
            else {
                scrollsHorizontally = documentSize.width > frameSize.width;
                if (scrollsHorizontally)
                    scrollsVertically = documentSize.height + [NSScroller scrollerWidth] > frameSize.height;
            }
        }
    
        [self setHasVerticalScroller:scrollsVertically];
        [self setHasHorizontalScroller:scrollsHorizontally];
        hasVerticalScroller = scrollsVertically;
        hasHorizontalScroller = scrollsHorizontally;
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
        static BOOL inUpdateScrollers;
        if (!inUpdateScrollers) {
            inUpdateScrollers = YES;
            [self updateScrollers];
            inUpdateScrollers = NO;
        }
    }
    [super reflectScrolledClipView:clipView];
}

- (void)setAllowsScrolling:(BOOL)flag
{
    disallowsScrolling = !flag;
    [self updateScrollers];
}

- (BOOL)allowsScrolling
{
    return !disallowsScrolling;
}

- (void)dealloc
{
    [cursor release];
    [super dealloc];
}

@end
