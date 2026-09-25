/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebPluginContainerCheck.h"

#import "WebFrameInternal.h"
#import "WebPluginContainerPrivate.h"
#import "WebPluginController.h"
#import "WebPolicyDelegatePrivate.h"
#import "WebView.h"
#import "WebViewInternal.h"
#import <Foundation/NSDictionary.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSURLRequest.h>
#import <WebCore/Document.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameLoaderTypes.h>
#import <WebCore/LocalFrameInlines.h>
#import <WebCore/OriginAccessPatterns.h>
#import <WebCore/SecurityOrigin.h>
#import <wtf/Assertions.h>
#import <wtf/ObjCRuntimeExtras.h>

#if PLATFORM(IOS_FAMILY)
@interface WebPluginController (SecretsIKnow)
- (WebFrame *)webFrame; // FIXME: This file calls -[WebPluginController webFrame], which is not declared in WebPluginController.h.  Merge issue?  Are the plug-in files out of date?
@end
#endif

@implementation WebPluginContainerCheck

- (id)initWithRequest:(NSURLRequest *)request target:(NSString *)target resultObject:(id)obj selector:(SEL)selector controller:(id <WebPluginContainerCheckController>)controller contextInfo:(id)contextInfo /*optional*/
{
    if (!(self = [super init]))
        return nil;
    
    _request = [request copy];
    _target = [target copy];
    _resultObject = [obj retain];
    _resultSelector = selector;
    _contextInfo = [contextInfo retain];
    
    // controller owns us so don't retain, to avoid cycle
    _controller = controller;
    
    return self;
}

+ (id)checkWithRequest:(NSURLRequest *)request target:(NSString *)target resultObject:(id)obj selector:(SEL)selector controller:(id <WebPluginContainerCheckController>)controller contextInfo:(id)contextInfo /*optional*/
{
    return adoptNS([[self alloc] initWithRequest:request target:target resultObject:obj selector:selector controller:controller contextInfo:contextInfo]).autorelease();
}

- (void)dealloc
{
    // mandatory to complete or cancel before releasing this object
    ASSERT(_done);
    [super dealloc];
}

- (void)_continueWithPolicy:(WebCore::PolicyAction)policy
{
    RetainPtr resultObject = _resultObject;
    if (_contextInfo)
        wtfObjCMsgSend<void>(resultObject, _resultSelector, (policy == WebCore::PolicyAction::Use), protect(_contextInfo));
    else     
        wtfObjCMsgSend<void>(resultObject, _resultSelector, (policy == WebCore::PolicyAction::Use));

    // this will call indirectly call cancel
    [protect(_controller) _webPluginContainerCancelCheckIfAllowedToLoadRequest:self];
}

- (BOOL)_isForbiddenFileLoad
{
    RefPtr coreFrame = core([protect(_controller) webFrame]);
    ASSERT(coreFrame);
    if (!protect(protect(coreFrame->document())->securityOrigin())->canDisplay([protect(_request) URL], WebCore::OriginAccessPatternsForWebProcess::singleton())) {
        [self _continueWithPolicy:WebCore::PolicyAction::Ignore];
        return YES;
    }

    return NO;
}

- (NSDictionary *)_actionInformationWithURL:(NSURL *)URL
{
    return @{
        WebActionNavigationTypeKey: @(WebNavigationTypePlugInRequest),
        WebActionModifierFlagsKey: @(0),
        WebActionOriginalURLKey: URL,
    };
}

- (void)_askPolicyDelegate
{
    RetainPtr target = _target;
    RetainPtr request = _request;
    RetainPtr listener = _listener;
    RetainPtr controller = _controller;
    WebView *webView = [controller webView];

    WebFrame *targetFrame;
    if ([target length] > 0) {
        targetFrame = [[controller webFrame] findFrameNamed:target];
    } else {
        targetFrame = [controller webFrame];
    }

    NSDictionary *action = [self _actionInformationWithURL:[request URL]];

    _listener = [[WebPolicyDecisionListener alloc] _initWithTarget:self action:@selector(_continueWithPolicy:)];

    if (targetFrame == nil) {
        // would open new window
        [[webView _policyDelegateForwarder] webView:webView
                     decidePolicyForNewWindowAction:action
                                            request:request
                                       newFrameName:target
                                   decisionListener:listener];
    } else {
        // would target existing frame
        [[webView _policyDelegateForwarder] webView:webView
                    decidePolicyForNavigationAction:action
                                            request:request
                                              frame:targetFrame
                                   decisionListener:listener];
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

    // Retaining the member just to release it would be pointless.
    SUPPRESS_UNRETAINED_ARG [_request release];
    _request = nil;
    
    SUPPRESS_UNRETAINED_ARG [_target release];
    _target = nil;

    RetainPtr listener = _listener;
    [listener _invalidate];
    [listener release];
    _listener = nil;

    SUPPRESS_UNRETAINED_ARG [_resultObject autorelease];
    _resultObject = nil;

    _controller = nil;
    
    SUPPRESS_UNRETAINED_ARG [_contextInfo release];
    _contextInfo = nil;

    _done = YES;
}

- (id)contextInfo
{
    return _contextInfo;   
}

@end
