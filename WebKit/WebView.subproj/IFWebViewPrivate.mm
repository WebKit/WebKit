/*	IFWebViewPrivate.mm
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _viewPrivate in 
        NSWebPageView.
*/
#import <IFWebViewPrivate.h>

// Includes from KDE
#include <khtmlview.h>

@implementation WKWebViewPrivate

- init
{
    [super init];
    
    controller = nil;
    widget = 0;
    
    return self;
}


- (void)dealloc
{
    // controller is not retained!  WKWebControllers maintain
    // a reference to their view and main data source.

    [frameScrollView release];

    if (widget)
        delete widget;
}



@end


@implementation WKWebView  (WKPrivate)

- (void)_resetView 
{
    NSArray *views = [self subviews];
    int count;
    
    count = [views count];
    while (count--){
        //WebKitDebugAtLevel(0x200, "Removing %p %s\n", [views objectAtIndex: 0], DEBUG_OBJECT([[[views objectAtIndex: 0] class] className]));
        [[views objectAtIndex: count] removeFromSuperviewWithoutNeedingDisplay]; 
    }
    [self setFrameSize: NSMakeSize (0,0)];
}


- (void)_setController: (id <WKWebController>)controller
{
    // Not retained.
    ((WKWebViewPrivate *)_viewPrivate)->controller = controller;    
}

- (KHTMLView *)_widget
{
    return ((WKWebViewPrivate *)_viewPrivate)->widget;    
}

- (void)_setFrameScrollView: (WKDynamicScrollBarsView *)sv
{
    ((WKWebViewPrivate *)_viewPrivate)->frameScrollView = [sv retain];    
    [self setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [sv setDocumentView: self];
}

- (WKDynamicScrollBarsView *)_frameScrollView
{
    return ((WKWebViewPrivate *)_viewPrivate)->frameScrollView;    
}

@end
