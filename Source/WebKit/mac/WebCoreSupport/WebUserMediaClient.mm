/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebUserMediaClient.h"

#if ENABLE(MEDIA_STREAM)

#import "WebDelegateImplementationCaching.h"
#import "WebSecurityOriginInternal.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <WebCore/BlockExceptions.h>
#import <WebCore/Page.h>
#import <WebCore/ScriptExecutionContext.h>
#import <WebCore/UserMediaRequest.h>
#import <wtf/HashMap.h>
#import <wtf/PassOwnPtr.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>

using namespace WebCore;

@interface WebUserMediaPolicyListener : NSObject <WebAllowDenyPolicyListener>
{
    RefPtr<UserMediaRequest> _request;
}
- (id)initWithUserMediaRequest:(PassRefPtr<UserMediaRequest>)request;
- (void)cancelRequest;
- (void)allow;
- (void)deny;
@end

typedef HashMap<RefPtr<UserMediaRequest>, RetainPtr<WebUserMediaPolicyListener>> UserMediaRequestsMap;

static UserMediaRequestsMap& userMediaRequestsMap()
{
    DEFINE_STATIC_LOCAL(UserMediaRequestsMap, requests, ());
    return requests;
}

static void AddRequestToMap(UserMediaRequest* request, RetainPtr<WebUserMediaPolicyListener> listener)
{
    userMediaRequestsMap().set(request, listener);
}

static void RemoveRequestFromMap(UserMediaRequest* request)
{
    userMediaRequestsMap().remove(request);
}

WebUserMediaClient::WebUserMediaClient(WebView* webView)
    : m_webView(webView)
{
}

WebUserMediaClient::~WebUserMediaClient()
{
}

void WebUserMediaClient::pageDestroyed()
{
    UserMediaRequestsMap& requestsMap = userMediaRequestsMap();
    for (UserMediaRequestsMap::iterator it = requestsMap.begin(); it != requestsMap.end(); ++it) {
        [it->value cancelRequest];
        requestsMap.remove(it);
    }

    delete this;
}

void WebUserMediaClient::requestPermission(PassRefPtr<UserMediaRequest> prpRequest)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    UserMediaRequest* request = prpRequest.get();
    SEL selector = @selector(webView:decidePolicyForUserMediaRequestFromOrigin:listener:);
    if (![[m_webView UIDelegate] respondsToSelector:selector]) {
        request->userMediaAccessDenied();
        return;
    }

    WebUserMediaPolicyListener *listener = [[WebUserMediaPolicyListener alloc] initWithUserMediaRequest:request];
    WebSecurityOrigin *webOrigin = [[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:request->securityOrigin()];

    CallUIDelegate(m_webView, selector, webOrigin, listener);
    AddRequestToMap(request, listener);

    [webOrigin release];
    [listener release];

    END_BLOCK_OBJC_EXCEPTIONS;
}

void WebUserMediaClient::cancelRequest(UserMediaRequest* request)
{
    UserMediaRequestsMap::iterator it = userMediaRequestsMap().find(request);
    if (it == userMediaRequestsMap().end())
        return;

    [it->value cancelRequest];
    userMediaRequestsMap().remove(it);
}


@implementation WebUserMediaPolicyListener

- (id)initWithUserMediaRequest:(PassRefPtr<UserMediaRequest>)request
{
#if ENABLE(MEDIA_STREAM)
    if (!(self = [super init]))
        return nil;

    _request = request;
    return self;
#endif
}

- (void)cancelRequest
{
#if ENABLE(MEDIA_STREAM)
    if (!_request)
        return;

    _request = 0;
#endif
    
}

- (void)allow
{
#if ENABLE(MEDIA_STREAM)
    if (!_request)
        return;

    _request->userMediaAccessGranted();
    RemoveRequestFromMap(_request.get());
#endif
}

- (void)deny
{
#if ENABLE(MEDIA_STREAM)
    if (!_request)
        return;
    
    _request->userMediaAccessDenied();
    RemoveRequestFromMap(_request.get());
#endif
}

@end
#endif
