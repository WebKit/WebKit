//
//  DynamicScrollBarsView.m
//  WebBrowser
//
//  Created by John Sullivan on Tue Jan 22 2002.
//  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/IFDynamicScrollBarsView.h>

#import <WebKit/IFWebView.h>

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

@end
