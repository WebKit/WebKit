//
//  DynamicScrollBarsView.m
//  WebBrowser
//
//  Created by John Sullivan on Tue Jan 22 2002.
//  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/IFDynamicScrollBarsView.h>
#import <WebKit/WebKitDebug.h>

@implementation IFDynamicScrollBarsView

- initWithFrame: (NSRect)frame
{
    [super initWithFrame: frame];
    allowsScrolling = YES;
    return self;
}

// make the horizontal and vertical scroll bars come and go as needed
- (void) reflectScrolledClipView: (NSClipView*)clipView
{
    if (allowsScrolling){    
        if( clipView == [self contentView] && breakRecursionCycle == NO ) {
            breakRecursionCycle = YES;
            
            [self updateScrollers];
            
            breakRecursionCycle = NO;
        }
    }
    [super reflectScrolledClipView: clipView];
}


- (void)updateScrollers
{
    if (allowsScrolling){    
        BOOL scrollsVertically;
        BOOL scrollsHorizontally;
        id dview = [self documentView];
        
        scrollsVertically = [dview bounds].size.height > [self frame].size.height;
        
        if (scrollsVertically)
            scrollsHorizontally = ([dview bounds].size.width + [NSScroller scrollerWidth]) > [self frame].size.width;
        else
            scrollsHorizontally = [dview bounds].size.width > [self frame].size.width;
        
        if (scrollsHorizontally && !scrollsVertically)
            scrollsVertically = ([dview bounds].size.height + [NSScroller scrollerWidth]) > [self frame].size.height;
        
        [self setHasVerticalScroller: scrollsVertically];
        [self setHasHorizontalScroller: scrollsHorizontally];
    }
}


- (void)setCursor:(NSCursor *)cur
{
    [cur retain];
    [cursor release];
    cursor = cur;

    // We have to make both of these calls, because:
    // - Just setting a cursor rect will have no effect, if the mouse cursor is already
    //   inside the area of the rect.
    // - Just calling invalidateCursorRectsForView will not call resetCursorRects if
    //   there is no cursor rect set currently and the view has no subviews.
    // Therefore we have to call resetCursorRects to ensure that a cursor rect is set
    // at all, if we are going to want one, and then invalidateCursorRectsForView: to
    // call resetCursorRects from the proper context that will
    // actually result in updating the cursor.
    [self resetCursorRects];
    [[self window] invalidateCursorRectsForView:self];
}

- (void)resetCursorRects
{
    if (cursor != nil && cursor != [NSCursor arrowCursor]) {
        [self addCursorRect:[self visibleRect] cursor:cursor];
    }
}

- (void)setAllowsScrolling: (BOOL)flag
{
    allowsScrolling = flag;
    if (allowsScrolling == NO){
        [self setHasVerticalScroller: NO];
        [self setHasHorizontalScroller: NO];
    }
}

- (BOOL)allowsScrolling
{
    return allowsScrolling;
}



@end
