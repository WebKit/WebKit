/*	IFWebViewPrivate.mm
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        NSWebPageView.
*/
#import <WebKit/WebKitDebug.h>

#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFPluginView.h>

// Includes from KDE
#import <khtmlview.h>
#import <khtml_part.h>
#import <html/html_documentimpl.h>
#import "IFWebController.h"

@implementation IFWebViewPrivate

- (void)dealloc
{
    [controller release];
    [frameScrollView release];
    [draggingTypes release];
    [cursor release];

    //if (widget)
    //    delete widget;

    [super dealloc];
}

@end

@implementation IFWebView (IFPrivate)

- (void)_resetWidget
{
    delete _private->widget;
    _private->widget = 0;
}

- (void)_stopPlugins 
{
    NSArray *subviews = [[self subviews] copy];
    int count = [subviews count];
    while (count--) {
        id view = [subviews objectAtIndex:count];
        if ([view isKindOfClass:[IFPluginView class]]) {
            IFPluginView *pluginView = (IFPluginView *)view;
            [pluginView stop];
        }
    }
    [subviews release];
}

- (void)_removeSubviews
{
    // Remove all the views.  They will be be re-added if this is a re-layout. 
    NSArray *subviews = [[self subviews] copy];
    [subviews makeObjectsPerformSelector:@selector(removeFromSuperviewWithoutNeedingDisplay)];
    [subviews release];
}

- (void)_setController: (IFWebController *)controller
{
    // Not retained; the controller owns the view.
    _private->controller = controller;    
}

- (KHTMLView *)_widget
{
    return _private->widget;    
}

- (KHTMLView *)_provisionalWidget
{
    return _private->provisionalWidget;    
}

- (void)_setFrameScrollView: (NSScrollView *)sv
{
    [sv retain];
    [_private->frameScrollView release];
    _private->frameScrollView = sv;    
    //[sv setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    //[sv setHasVerticalScroller: YES];
    //[sv setHasHorizontalScroller: YES];
    [self setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [sv setDocumentView: self];
}

- (NSScrollView *)_frameScrollView
{
    return _private->frameScrollView;    
}

- (void)_setupScrollers
{
    BOOL scrollsVertically;
    BOOL scrollsHorizontally;

    if ([self _frameScrollView]) {
        scrollsVertically = [self bounds].size.height > [[self _frameScrollView] frame].size.height;
        scrollsHorizontally = [self bounds].size.width > [[self _frameScrollView] frame].size.width;
    
        [[self _frameScrollView] setHasVerticalScroller: scrollsVertically];
        [[self _frameScrollView] setHasHorizontalScroller: scrollsHorizontally];
    }
}

@end
