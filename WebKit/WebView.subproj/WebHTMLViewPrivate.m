/*	WebHTMLViewPrivate.mm
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        NSWebPageView.
*/

#import <WebKit/WebHTMLViewPrivate.h>

#import <AppKit/NSResponder_Private.h>
#import <WebFoundation/WebAssertions.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPluginView.h>
#import <WebKit/WebController.h>
#import <WebKit/WebControllerPrivate.h>
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
    if ([self isKindOfClass:[WebNetscapePluginView class]]) {
	WebNetscapePluginView *pluginView = (WebNetscapePluginView *)self;
        [pluginView stop];
    }
}
@end

@implementation WebHTMLViewPrivate

- (void)dealloc
{
    [draggedURL release];
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

- (WebController *)_controller
{
    return [[self _web_parentWebView] _controller];
}

- (WebFrame *)_frame
{
    WebView *webView = [self _web_parentWebView];
    return [[webView _controller] frameForView:webView];
}

// Required so view can access the part's selection.
- (WebBridge *)_bridge
{
    return [[self _frame] _bridge];
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

    [elementInfo addEntriesFromDictionary: elementInfoWC];

    WebView *webView = [self _web_parentWebView];
    WebFrame *webFrame = [[webView _controller] frameForView:webView];
    [elementInfo setObject:webFrame forKey:WebContextMenuElementFrameKey];
       
    return elementInfo;
}

- (BOOL)_continueAfterClickPolicyForEvent: (NSEvent *)event
{
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    WebController *controller = [self _controller];
    WebClickPolicy *clickPolicy;

    clickPolicy = [[controller policyHandler] clickPolicyForElement:[self _elementAtPoint:point]
                                                             button:[event type]
                                                       modifierMask:[event modifierFlags]];

    WebPolicyAction clickAction = [clickPolicy policyAction];
    NSURL *URL = [clickPolicy URL];

    switch (clickAction) {
        case WebClickPolicyShow:
            return YES;
        case WebClickPolicyOpenNewWindow:
            [[controller windowContext] openNewWindowWithURL:URL];
            break;
        case WebClickPolicySave:
        case WebClickPolicySaveAndOpenExternally:
            [controller _downloadURL:URL toPath:[clickPolicy path]];
            break;
        case WebClickPolicyIgnore:
            break;
        default:
            [NSException raise:NSInvalidArgumentException
                        format:@"clickPolicyForElement:button:modifierMask: returned an invalid WebClickPolicy"];
    }
    return NO;
}

@end
