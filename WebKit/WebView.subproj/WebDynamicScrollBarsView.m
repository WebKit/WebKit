//
//  DynamicScrollBarsView.m
//  WebBrowser
//
//  Created by John Sullivan on Tue Jan 22 2002.
//  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WKDynamicScrollBarsView.h>

@implementation WKDynamicScrollBarsView

// make the horizontal and vertical scroll bars come and go as needed
- (void) reflectScrolledClipView: (NSClipView*)clipView
{
    if( clipView == [self contentView] ) {
        BOOL scrollsVertically = [[self documentView] bounds].size.height > [self contentSize].height;
        BOOL scrollsHorizontally = [[self documentView] bounds].size.width > [self contentSize].width;

        [self setHasVerticalScroller: scrollsVertically];
        [self setHasHorizontalScroller: scrollsHorizontally];
    }
    [super reflectScrolledClipView: clipView];
}

@end
