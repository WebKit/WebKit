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
    
    WEBKITDEBUGLEVEL (0x100, "\n");
    
    // Do nothing if the web view is in the provisional state.
    if ([cview isKindOfClass: NSClassFromString (@"IFWebView")]){
        if ([cview _provisionalWidget] != 0){
            WEBKITDEBUGLEVEL (0x100, "not changing scrollview, content in provisional state.\n");
            return;
        }
    }
    
    if( clipView == [self contentView] ) {
        BOOL scrollsVertically = [[self documentView] bounds].size.height > [self contentSize].height;
        BOOL scrollsHorizontally = [[self documentView] bounds].size.width > [self contentSize].width;

        [self setHasVerticalScroller: scrollsVertically];
        [self setHasHorizontalScroller: scrollsHorizontally];
    }
    [super reflectScrolledClipView: clipView];
}

@end
