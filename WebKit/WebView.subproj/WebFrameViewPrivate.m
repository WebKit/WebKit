/*	IFWebViewPrivate.mm
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _viewPrivate in 
        NSWebPageView.
*/
#import <IFWebViewPrivate.h>
#import <IFPluginView.h>

// Includes from KDE
#include <khtmlview.h>

@implementation IFWebViewPrivate

- init
{
    [super init];
    
    controller = nil;
    widget = 0;
    
    return self;
}


- (void)dealloc
{
    // controller is not retained!  IFWebControllers maintain
    // a reference to their view and main data source.

    [frameScrollView release];

    if (widget)
        delete widget;

    [super dealloc];
}



@end


@implementation IFWebView  (IFPrivate)

- (void)_resetWidget
{
    if (((IFWebViewPrivate *)_viewPrivate)->widget)
        delete ((IFWebViewPrivate *)_viewPrivate)->widget;
    ((IFWebViewPrivate *)_viewPrivate)->widget = 0;
}

- (void)_resetView 
{
    NSArray *views = [self subviews];
    int count;
    
    count = [views count];
    while (count--){
        //WebKitDebugAtLevel(0x200, "Removing %p %s\n", [views objectAtIndex: 0], DEBUG_OBJECT([[[views objectAtIndex: 0] class] className]));
        id view;
        
        view = [views objectAtIndex: count];
        if ([view isKindOfClass: NSClassFromString (@"IFPluginView")])
            [(IFPluginView *)view stop];
        [view removeFromSuperviewWithoutNeedingDisplay]; 
    }
    [self setFrameSize: NSMakeSize (0,0)];
}


- (void)_setController: (id <IFWebController>)controller
{
    // Not retained.
    ((IFWebViewPrivate *)_viewPrivate)->controller = controller;    
}

- (KHTMLView *)_widget
{
    return ((IFWebViewPrivate *)_viewPrivate)->widget;    
}


- (KHTMLView *)_provisionalWidget
{
    return ((IFWebViewPrivate *)_viewPrivate)->provisionalWidget;    
}


- (void)_setFrameScrollView: (IFDynamicScrollBarsView *)sv
{
    ((IFWebViewPrivate *)_viewPrivate)->frameScrollView = [sv retain];    
    //[sv setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    //[sv setHasVerticalScroller: YES];
    //[sv setHasHorizontalScroller: YES];
    [self setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [sv setDocumentView: self];
}

- (IFDynamicScrollBarsView *)_frameScrollView
{
    return ((IFWebViewPrivate *)_viewPrivate)->frameScrollView;    
}

@end
