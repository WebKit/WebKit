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

- (void)updateScrollers
{
    BOOL scrollsVertically;
    BOOL scrollsHorizontally;

    if (disallowsScrolling) {
        scrollsVertically = NO;
        scrollsHorizontally = NO;
    } else {
        // Force a layout before checking if scrollbars are needed.
        // This fixes 2969367, although may introduce a slowdown in
        // live resize performance.
        [((id<WebDocumentView>)[self documentView]) layout];
        
        NSSize documentSize = [[self documentView] frame].size;
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

- (void)setCursor:(NSCursor *)cur
{
    // Do nothing for cases where the cursor isn't changing.
    // Also turn arrowCursor into nil.
    if (!cur) {
        if (!cursor) {
            return;
        }
    } else {
        if ([cur isEqual:[NSCursor arrowCursor]]) {
            cur = nil;
        } else if (cursor && [cursor isEqual:cur]) {
            return;
        }
    }
    
    [cursor release];
    cursor = [cur retain];

    // We have to make both of these calls, because:
    // - Just setting a cursor rect will have no effect, if the mouse cursor is already
    //   inside the area of the rect.
    // - Just calling invalidateCursorRectsForView will not call resetCursorRects if
    //   there is no cursor rect set currently and the view has no subviews.
    // Therefore we have to call resetCursorRects to ensure that a cursor rect is set
    // at all, if we are going to want one, and then invalidateCursorRectsForView: to
    // call resetCursorRects from the proper context that will actually result in
    // updating the cursor.
    [self resetCursorRects];
    [[self window] invalidateCursorRectsForView:self];
}

- (void)resetCursorRects
{
    [self discardCursorRects];
    if (cursor) {
        [self addCursorRect:[self visibleRect] cursor:cursor];
    }
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

@end
