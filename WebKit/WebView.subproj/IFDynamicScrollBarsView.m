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

@interface IFWebView (_IFPrivate)
- (void *)_provisionalWidget;
@end

@implementation IFDynamicScrollBarsView

// make the horizontal and vertical scroll bars come and go as needed
- (void) reflectScrolledClipView: (NSClipView*)clipView
{
    id cview = [self documentView];
        
    // Do nothing if the web view is in the provisional state.
    if ([cview isKindOfClass: NSClassFromString (@"IFWebView")]){
        if ([cview _provisionalWidget] != 0){
            return;
        }
    }
    
    if( clipView == [self contentView] ) {
        BOOL scrollsVertically;
        BOOL scrollsHorizontally;

        scrollsVertically = [[self documentView] bounds].size.height > [self frame].size.height;
        scrollsHorizontally = [[self documentView] bounds].size.width > [self frame].size.width;

        [self setHasVerticalScroller: scrollsVertically];
        [self setHasHorizontalScroller: scrollsHorizontally];
    }
    [super reflectScrolledClipView: clipView];
}

@end
