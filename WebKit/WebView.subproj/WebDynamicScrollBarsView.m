//
//  IFDynamicScrollBarsView.m
//  WebKit
//
//  Created by John Sullivan on Tue Jan 22 2002.
//  Copyright (c) 2001, 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/IFDynamicScrollBarsView.h>

@implementation IFDynamicScrollBarsView

- initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];
    allowsScrolling = YES;
    return self;
}

- (void)updateScrollers
{
    BOOL scrollsVertically;
    BOOL scrollsHorizontally;
    BOOL scrollersChanged;    

    if (!allowsScrolling) {
        scrollsVertically = NO;
        scrollsHorizontally = NO;
    } else {
        NSSize documentSize = [[self documentView] bounds].size;
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

    scrollersChanged = NO;
        
    if ([self hasVerticalScroller] != scrollsVertically) {
        [self setHasVerticalScroller:scrollsVertically];
        scrollersChanged = YES;
    }
        
    if ([self hasHorizontalScroller] != scrollsHorizontally) {
        [self setHasHorizontalScroller:scrollsHorizontally];
        scrollersChanged = YES;
    }
    
    if (scrollersChanged) {
        [self tile];
        [self setNeedsDisplay:YES];
    }
}

// Make the horizontal and vertical scroll bars come and go as needed.
- (void)reflectScrolledClipView:(NSClipView *)clipView
{
    if (clipView == [self contentView]) {
        [self updateScrollers];
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
    allowsScrolling = flag;
    [self updateScrollers];
}

- (BOOL)allowsScrolling
{
    return allowsScrolling;
}

@end
