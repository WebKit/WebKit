//
//  WebPluginContainerCheck.m
//  WebKit
//
//  Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebPluginContainerCheck.h>

#import <Foundation/NSDictionary.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSURLRequest.h>
#import <WebKit/WebAssertions.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebPluginContainer.h>
#import <WebKit/WebPluginContainerPrivate.h>
#import <WebKit/WebPluginController.h>
#import <WebKit/WebPolicyDelegate.h>
#import <WebKit/WebPolicyDelegatePrivate.h>
#import <WebKit/WebView.h>
#import <WebKit/WebViewPrivate.h>
#import <objc/objc-runtime.h>

@implementation WebPluginContainerCheck

+ (id)checkWithRequest:(NSURLRequest *)request target:(NSString *)target resultObject:(id)obj selector:(SEL)selector controller:(WebPluginController *)controller
{
    return [[[self alloc] initWithRequest:request target:target resultObject:obj selector:selector controller:controller] autorelease];
}

- (id)initWithRequest:(NSURLRequest *)request target:(NSString *)target resultObject:(id)obj selector:(SEL)selector controller:(WebPluginController *)controller
{
    if (!(self = [super init]))
        return nil;

    _request = [request copy];
    _target = [target copy];
    _resultObject = [obj retain];
    _resultSelector = selector;

    // controller owns us so don't retain, to avoid cycle
    _controller = controller;

    return self;
}

- (void)finalize
{
    // mandatory to complete or cancel before releasing this object
    ASSERT(_done);
}

- (void)dealloc
{
    // mandatory to complete or cancel before releasing this object
    ASSERT(_done);
    [super dealloc];
}

- (void)_continueWithPolicy:(WebPolicyAction)policy
{
    void (*callBack)(id, SEL, BOOL) = (void (*)(id, SEL, BOOL))objc_msgSend;
    (*callBack) (_resultObject, _resultSelector, (policy == WebPolicyUse));

    // this will call indirectly call cancel
    [_controller _webPluginContainerCancelCheckIfAllowedToLoadRequest:self];
}

- (BOOL)_isForbiddenFileLoad
{
   BOOL ignore;
   WebBridge *bridge = [_controller bridge];
   if (![bridge canLoadURL:[_request URL] fromReferrer:[bridge referrer] hideReferrer:&ignore]) {
       [self _continueWithPolicy:WebPolicyIgnore];
       return YES;
   }

   return NO;
}

- (NSDictionary *)_actionInformationWithURL:(NSURL *)URL
{
    return [NSDictionary dictionaryWithObjectsAndKeys:
               [NSNumber numberWithInt:WebNavigationTypePlugInRequest], WebActionNavigationTypeKey,
               [NSNumber numberWithInt:0], WebActionModifierFlagsKey,
               URL, WebActionOriginalURLKey,
               nil];
}

- (void)_askPolicyDelegate
{
    WebView *webView = [_controller webView];

    WebFrame *targetFrame;
    if ([_target length] > 0) {
        targetFrame = [[_controller webFrame] findFrameNamed:_target];
    } else {
        targetFrame = [_controller webFrame];
    }

    NSDictionary *action = [self _actionInformationWithURL:[_request URL]];

    _listener = [[WebPolicyDecisionListener alloc] _initWithTarget:self action:@selector(_continueWithPolicy:)];

    if (targetFrame == nil) {
        // would open new window
        [[webView _policyDelegateForwarder] webView:webView
                     decidePolicyForNewWindowAction:action
                                            request:_request
                                       newFrameName:_target
                                   decisionListener:_listener];
    } else {
        // would target existing frame
        [[webView _policyDelegateForwarder] webView:webView
                    decidePolicyForNavigationAction:action
                                            request:_request
                                              frame:targetFrame
                                   decisionListener:_listener];        
    }
}

- (void)start
{
    ASSERT(!_listener);
    ASSERT(!_done);

    if ([self _isForbiddenFileLoad])
        return;

    [self _askPolicyDelegate];
}

- (void)cancel
{
    if (_done)
        return;

    [_request release];
    _request = nil;
    
    [_target release];
    _target = nil;

    [_listener _invalidate];
    [_listener release];
    _listener = nil;

    [_resultObject autorelease];
    _resultObject = nil;

    _controller = nil;

    _done = YES;
}

@end
