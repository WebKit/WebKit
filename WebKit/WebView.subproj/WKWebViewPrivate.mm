/*	WKWebViewPrivate.mm
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _viewPrivate in 
        NSWebPageView.
*/
#import <WKWebViewPrivate.h>

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

    //if (widget)
    //    delete widget;
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
        [[views objectAtIndex: 0] removeFromSuperviewWithoutNeedingDisplay]; 
    }
    [self setFrameSize: NSMakeSize (0,0)];
}


- (void)_setController: (id <WKWebController>)controller
{
    // Not retained.
    ((WKWebViewPrivate *)_viewPrivate)->controller = controller;    
}

@end
