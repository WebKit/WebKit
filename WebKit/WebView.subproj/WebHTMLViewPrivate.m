/*	WebHTMLViewPrivate.mm
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        NSWebPageView.
*/

#import <WebKit/WebHTMLViewPrivate.h>

#import <AppKit/NSResponder_Private.h>
#import <WebKit/WebKitDebug.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPluginView.h>
#import <WebKit/WebController.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebDataSourcePrivate.h>

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

//FIXME: WebHTMLView doesn't seem to use _private->controller so is _setController needed?
- (void)_setController:(WebController *)controller
{
    // Not retained; the controller owns the view.
    _private->controller = controller;    
}

- (WebController *)_controller
{
    return [[self _web_parentWebView] _controller];
}

// Required so view can access the part's selection.
- (WebBridge *)_bridge
{
    WebView *webView = [self _web_parentWebView];
    WebFrame *webFrame = [[webView _controller] frameForView:webView];
    return [[webFrame dataSource] _bridge];
}

BOOL _modifierTrackingEnabled = FALSE;

+ (void)_setModifierTrackingEnabled:(BOOL)enabled
{
    _modifierTrackingEnabled = enabled;
}

+ (BOOL)_modifierTrackingEnabled
{
    return _modifierTrackingEnabled;
}

+ (void)_postFlagsChangedEvent:(NSEvent *)flagsChangedEvent
{
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved location:[[flagsChangedEvent window] convertScreenToBase:[NSEvent mouseLocation]] modifierFlags:[flagsChangedEvent modifierFlags] timestamp:[flagsChangedEvent timestamp] windowNumber:[flagsChangedEvent windowNumber] context:[flagsChangedEvent context] eventNumber:0 clickCount:0 pressure:0];

    // pretend it's a mouse move
    [[NSNotificationCenter defaultCenter] postNotificationName:NSMouseMovedNotification object:self userInfo:[NSDictionary dictionaryWithObject:fakeEvent forKey:@"NSEvent"]];
}

- (NSDictionary *)_elementAtPoint:(NSPoint)point
{
    NSDictionary *elementInfoWC = [[self _bridge] elementAtPoint:point];
    NSMutableDictionary *elementInfo = [NSMutableDictionary dictionary];

    NSURL *linkURL =   [elementInfoWC objectForKey:WebCoreContextLinkURL];
    NSURL *imageURL =  [elementInfoWC objectForKey:WebCoreContextImageURL];
    NSString *string = [elementInfoWC objectForKey:WebCoreContextString];
    NSImage *image =   [elementInfoWC objectForKey:WebCoreContextImage];

    if(linkURL)  [elementInfo setObject:linkURL  forKey:WebContextLinkURL];
    if(imageURL) [elementInfo setObject:imageURL forKey:WebContextImageURL];
    if(string)   [elementInfo setObject:string   forKey:WebContextString];
    if(image)    [elementInfo setObject:image    forKey:WebContextImage];

    WebView *webView = [self _web_parentWebView];
    WebFrame *webFrame = [[webView _controller] frameForView:webView];
    [elementInfo setObject:webFrame forKey:WebContextFrame];
       
    return elementInfo;
}

@end
