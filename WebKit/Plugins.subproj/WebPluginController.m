//
//  WebPluginController.m
//  WebKit
//
//  Created by Chris Blumenberg on Wed Oct 23 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebPluginController.h>

#import <WebKit/WebBridge.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPlugin.h>
#import <WebKit/WebPluginContainer.h>
#import <WebKit/WebPluginContainerCheck.h>
#import <WebKit/WebPluginPackage.h>
#import <WebKit/WebPluginViewFactory.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebUIDelegate.h>

#import <WebCore/WebCoreBridge.h>

#import <Foundation/NSURLRequest.h>

@interface NSView (PluginSecrets)
- (void)setContainingWindow:(NSWindow *)w;
@end

// For compatibility only.
@interface NSObject (OldPluginAPI)
+ (NSView *)pluginViewWithArguments:(NSDictionary *)arguments;
@end

@interface NSView (OldPluginAPI)
- (void)pluginInitialize;
- (void)pluginStart;
- (void)pluginStop;
- (void)pluginDestroy;
@end

static NSMutableSet *pluginViews = nil;

@implementation WebPluginController

+ (NSView *)plugInViewWithArguments:(NSDictionary *)arguments fromPluginPackage:(WebPluginPackage *)pluginPackage
{
    [pluginPackage load];
    Class viewFactory = [pluginPackage viewFactory];
    
    NSView *view = nil;
    if ([viewFactory respondsToSelector:@selector(plugInViewWithArguments:)]) {
        view = [viewFactory plugInViewWithArguments:arguments];
    } else if ([viewFactory respondsToSelector:@selector(pluginViewWithArguments:)]) {
        view = [viewFactory pluginViewWithArguments:arguments];
    }
    
    if (view == nil) {
        return nil;
    }
    
    if (pluginViews == nil) {
        pluginViews = [[NSMutableSet alloc] init];
    }
    [pluginViews addObject:view];
    
    return view;
}

+ (BOOL)isPlugInView:(NSView *)view
{
    return [pluginViews containsObject:view];
}

- (id)initWithDocumentView:(NSView *)view
{
    [super init];
    _documentView = view;
    _views = [[NSMutableArray alloc] init];
    _checksInProgress = (NSMutableSet *)CFSetCreateMutable(NULL, 0, NULL);

    return self;
}

- (void)startAllPlugins
{
    if (_started) {
        return;
    }
    
    if ([_views count] > 0) {
        LOG(Plugins, "starting WebKit plugins : %@", [_views description]);
    }
    
    int i, count = [_views count];
    for (i = 0; i < count; i++) {
        id aView = [_views objectAtIndex:i];
        if ([aView respondsToSelector:@selector(webPlugInStart)])
            [aView webPlugInStart];
        else if ([aView respondsToSelector:@selector(pluginStart)])
            [aView pluginStart];
    }
    _started = YES;
}

- (void)stopAllPlugins
{
    if (!_started) {
        return;
    }

    if ([_views count] > 0) {
        LOG(Plugins, "stopping WebKit plugins: %@", [_views description]);
    }
    
    int i, count = [_views count];
    for (i = 0; i < count; i++) {
        id aView = [_views objectAtIndex:i];
        if ([aView respondsToSelector:@selector(webPlugInStop)])
            [aView webPlugInStop];
        else if ([aView respondsToSelector:@selector(pluginStop)])
            [aView pluginStop];
    }
    _started = NO;
}

- (void)addPlugin:(NSView *)view
{
    if (!_documentView) {
        ERROR("can't add a plug-in to a defunct WebPluginController");
        return;
    }
    
    if (![_views containsObject:view]) {
        [_views addObject:view];
        
        LOG(Plugins, "initializing plug-in %@", view);
        if ([view respondsToSelector:@selector(webPlugInInitialize)])
            [view webPlugInInitialize];
        else if ([view respondsToSelector:@selector(pluginInitialize)])
            [view pluginInitialize];

        if (_started) {
            LOG(Plugins, "starting plug-in %@", view);
            if ([view respondsToSelector:@selector(webPlugInStart)])
                [view webPlugInStart];
            else if ([view respondsToSelector:@selector(pluginStart)])
                [view pluginStart];
            
            if ([view respondsToSelector:@selector(setContainingWindow:)])
                [view setContainingWindow:[_documentView window]];
        }
    }
}

- (void)_webPluginContainerCancelCheckIfAllowedToLoadRequest:(id)checkIdentifier
{
    [checkIdentifier cancel];
    [_checksInProgress removeObject:checkIdentifier];
}

- (void)_cancelOutstandingChecks
{
    NSEnumerator *e = [_checksInProgress objectEnumerator];
    id check;
    while ((check = [e nextObject])) {
        [check cancel];
    }
    [_checksInProgress release];
    _checksInProgress = nil;
}

- (void)destroyAllPlugins
{    
    [self stopAllPlugins];

    if ([_views count] > 0) {
        LOG(Plugins, "destroying WebKit plugins: %@", [_views description]);
    }

    [self _cancelOutstandingChecks];
    
    int i, count = [_views count];
    for (i = 0; i < count; i++) {
        id aView = [_views objectAtIndex:i];
        if ([aView respondsToSelector:@selector(webPlugInDestroy)]) {
            [aView webPlugInDestroy];
        } else if ([aView respondsToSelector:@selector(pluginDestroy)]) {
            [aView pluginDestroy];
        }
        [pluginViews removeObject:aView];
    }
    [_views makeObjectsPerformSelector:@selector(removeFromSuperviewWithoutNeedingDisplay)];
    [_views release];
    _views = nil;

    _documentView = nil;
}

- (id)_webPluginContainerCheckIfAllowedToLoadRequest:(NSURLRequest *)request inFrame:(NSString *)target resultObject:(id)obj selector:(SEL)selector
{
    WebPluginContainerCheck *check = [WebPluginContainerCheck checkWithRequest:request target:target resultObject:obj selector:selector controller:self];
    [_checksInProgress addObject:check];
    [check start];

    return check;
}

- (void)webPlugInContainerLoadRequest:(NSURLRequest *)request inFrame:(NSString *)target
{
    if (!request) {
        ERROR("nil URL passed");
        return;
    }
    if (!_documentView) {
        ERROR("could not load URL %@ because plug-in has already been destroyed", request);
        return;
    }
    WebFrame *frame = [_documentView _frame];
    if (!frame) {
        ERROR("could not load URL %@ because plug-in has already been stopped", request);
        return;
    }
    if (!target) {
        target = @"_top";
    }
    NSString *JSString = [[request URL] _webkit_scriptIfJavaScriptURL];
    if (JSString) {
        if ([frame findFrameNamed:target] != frame) {
            ERROR("JavaScript requests can only be made on the frame that contains the plug-in");
            return;
        }
        [[frame _bridge] stringByEvaluatingJavaScriptFromString:JSString];
    } else {
        if (!request) {
            ERROR("could not load URL %@", [request URL]);
            return;
        }
        [frame _loadRequest:request inFrameNamed:target];
    }
}

// For compatibility only.
- (void)showURL:(NSURL *)URL inFrame:(NSString *)target
{
    [self webPlugInContainerLoadRequest:[NSURLRequest requestWithURL:URL] inFrame:target];
}

- (void)webPlugInContainerShowStatus:(NSString *)message
{
    if (!message) {
        message = @"";
    }
    if (!_documentView) {
        ERROR("could not show status message (%@) because plug-in has already been destroyed", message);
        return;
    }
    WebView *v = [_documentView _webView];
    [[v _UIDelegateForwarder] webView:v setStatusText:message];
}

// For compatibility only.
- (void)showStatus:(NSString *)message
{
    [self webPlugInContainerShowStatus:message];
}

- (NSColor *)webPlugInContainerSelectionColor
{
    return [[_documentView _bridge] selectionColor];
}

// For compatibility only.
- (NSColor *)selectionColor
{
    return [self webPlugInContainerSelectionColor];
}

- (WebFrame *)webFrame
{
    return [_documentView _frame];
}

- (WebBridge *)bridge
{
    return [[self webFrame] _bridge];
}

- (WebView *)webView
{
    return [[self webFrame] webView];
}

@end
