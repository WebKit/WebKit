/*	IFHTMLViewPrivate.mm
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        NSWebPageView.
*/
#import <WebKit/WebKitDebug.h>

#import <WebKit/IFHTMLViewPrivate.h>
#import <WebKit/IFImageRenderer.h>
#import <WebKit/IFNSViewExtras.h>
#import <WebKit/IFPluginView.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFWebCoreBridge.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFWebViewPrivate.h>

#ifndef WEBKIT_INDEPENDENT_OF_WEBCORE
#import <khtmlview.h>
#endif

@interface NSView (IFHTMLViewPrivate)
- (void)_IF_stopIfPluginView;
@end

@implementation NSView (IFHTMLViewPrivate)
- (void)_IF_stopIfPluginView
{
    if ([self isKindOfClass:[IFPluginView class]]) {
        [(IFPluginView *)self stop];
    }
}
@end

@implementation IFHTMLViewPrivate

- (void)dealloc
{
    [cursor release];

    [super dealloc];
}

@end

@implementation IFHTMLView (IFPrivate)

- (void)_reset
{
    NSArray *subviews = [[self subviews] copy];
    [subviews makeObjectsPerformSelector:@selector(_IF_stopIfPluginView)];
    [subviews release];

    [IFImageRenderer stopAnimationsInView: self];
    
    delete _private->provisionalWidget;
    _private->provisionalWidget = 0;
    if (_private->widgetOwned)
        delete _private->widget;
    _private->widget = 0;
    _private->widgetOwned = NO;
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

// Required so view can access the part's selection.
- (IFWebCoreBridge *)_bridge
{
    IFWebView *webView = [self _IF_parentWebView];
    IFWebFrame *webFrame = [[webView _controller] frameForView: webView];
    return [[webFrame dataSource] _bridge];
}

@end
