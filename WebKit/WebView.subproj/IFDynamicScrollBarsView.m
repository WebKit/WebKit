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

// make the horizontal and vertical scroll bars come and go as needed
- (void) reflectScrolledClipView: (NSClipView*)clipView
{
    id dview = [self documentView];
        
    if( clipView == [self contentView] && breakRecursionCycle == NO ) {
        BOOL scrollsVertically;
        BOOL scrollsHorizontally;
    
        breakRecursionCycle = YES;
        
        scrollsVertically = [dview bounds].size.height > [self frame].size.height;
        scrollsHorizontally = [dview bounds].size.width > [self frame].size.width;
    
        [self setHasVerticalScroller: scrollsVertically];
        [self setHasHorizontalScroller: scrollsHorizontally];
        
        breakRecursionCycle = NO;
    }
    [super reflectScrolledClipView: clipView];
}

- (void)setCursor:(NSCursor *)cur
{
    [cur release];
    cursor = [cur retain];

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

@end
