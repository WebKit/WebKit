/*	WebHTMLViewPrivate.mm
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        NSWebPageView.
*/

#import <WebKit/WebHTMLViewPrivate.h>

#import <WebKit/WebKitDebug.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPluginView.h>
#import <WebKit/WebController.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebViewPrivate.h>

@interface NSView (WebHTMLViewPrivate)
- (void)_web_stopIfPluginView;
@end

@implementation NSView (WebHTMLViewPrivate)
- (void)_web_stopIfPluginView
{
    if ([self isKindOfClass:[WebPluginView class]]) {
	WebPluginView *pluginView = (WebPluginView *)self;
        [pluginView stop];
    }
}
@end

@implementation WebHTMLViewPrivate

- (void)dealloc
{
    [cursor release];

    [super dealloc];
}

@end

@implementation WebHTMLView (WebPrivate)

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
    [subviews makeObjectsPerformSelector:@selector(_web_stopIfPluginView)];
    [subviews release];

    [WebImageRenderer stopAnimationsInView:self];
}

- (void)_setController:(WebController *)controller
{
    // Not retained; the controller owns the view.
    _private->controller = controller;    
}

// Required so view can access the part's selection.
- (WebBridge *)_bridge
{
    WebView *webView = [self _web_parentWebView];
    WebFrame *webFrame = [[webView _controller] frameForView:webView];
    return [[webFrame dataSource] _bridge];
}

@end
