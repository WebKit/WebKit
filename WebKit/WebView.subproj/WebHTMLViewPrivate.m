/*	IFHTMLViewPrivate.mm
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        NSWebPageView.
*/

#import <WebKit/IFHTMLViewPrivate.h>

#import <WebKit/WebKitDebug.h>
#import <WebKit/IFImageRenderer.h>
#import <WebKit/IFNSViewExtras.h>
#import <WebKit/IFPluginView.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFWebCoreBridge.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFWebViewPrivate.h>

@interface NSView (IFHTMLViewPrivate)
- (void)_IF_stopIfPluginView;
@end

@implementation NSView (IFHTMLViewPrivate)
- (void)_IF_stopIfPluginView
{
    if ([self isKindOfClass:[IFPluginView class]]) {
	IFPluginView *pluginView = (IFPluginView *)self;
        [pluginView stop];
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

- (void)_adjustFrames
{
    // Ick!  khtml set the frame size during layout and
    // the frame origins during drawing!  So we have to 
    // layout and do a draw with rendering disabled to
    // correclty adjust the frames.
    [[self _bridge] adjustFrames:[self frame]];
}


- (void)_reset
{
    NSArray *subviews = [[self subviews] copy];
    [subviews makeObjectsPerformSelector:@selector(_IF_stopIfPluginView)];
    [subviews release];

    [IFImageRenderer stopAnimationsInView:self];
}

- (void)_setController:(IFWebController *)controller
{
    // Not retained; the controller owns the view.
    _private->controller = controller;    
}

// Required so view can access the part's selection.
- (IFWebCoreBridge *)_bridge
{
    IFWebView *webView = [self _IF_parentWebView];
    IFWebFrame *webFrame = [[webView _controller] frameForView:webView];
    return [[webFrame dataSource] _bridge];
}

@end
