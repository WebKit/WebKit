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
#import <WebCore/UserMediaPermissionCheck.h>
#import <WebCore/UserMediaRequest.h>
#import <wtf/HashMap.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>

using namespace WebCore;

@interface WebUserMediaPolicyListener : NSObject <WebAllowDenyPolicyListener>
{
    RefPtr<UserMediaRequest> _request;
}
- (id)initWithUserMediaRequest:(PassRefPtr<UserMediaRequest>)request;
- (void)cancelUserMediaAccessRequest;
- (void)deny;
@end

@interface WebUserMediaPolicyCheckerListener : NSObject <WebAllowDenyPolicyListener> {
    RefPtr<UserMediaPermissionCheck> _request;
}
- (id)initWithUserMediaPermissionCheck:(PassRefPtr<UserMediaPermissionCheck>)request;
- (void)cancelUserMediaPermissionCheck;
- (void)deny;
@end

typedef HashMap<RefPtr<UserMediaRequest>, RetainPtr<WebUserMediaPolicyListener>> UserMediaRequestsMap;

static UserMediaRequestsMap& userMediaRequestsMap()
{
    static NeverDestroyed<UserMediaRequestsMap> requests;
    return requests;
}

static void AddRequestToRequestMap(UserMediaRequest* request, RetainPtr<WebUserMediaPolicyListener> listener)
{
    userMediaRequestsMap().set(request, listener);
}

static void RemoveRequestFromRequestMap(UserMediaRequest* request)
{
    userMediaRequestsMap().remove(request);
}

typedef HashMap<RefPtr<UserMediaPermissionCheck>, RetainPtr<WebUserMediaPolicyCheckerListener>> UserMediaCheckMap;

static UserMediaCheckMap& userMediaCheckMap()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(UserMediaCheckMap, requests, ());
    return requests;
}

static void AddPermissionCheckToMap(UserMediaPermissionCheck* request, RetainPtr<WebUserMediaPolicyCheckerListener> listener)
{
    userMediaCheckMap().set(request, listener);
}

static void RemovePermissionCheckFromMap(UserMediaPermissionCheck* request)
{
    userMediaCheckMap().remove(request);
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
        [it->value cancelUserMediaAccessRequest];
        requestsMap.remove(it);
    }

    delete this;
}

void WebUserMediaClient::requestUserMediaAccess(UserMediaRequest& request)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    SEL selector = @selector(webView:decidePolicyForUserMediaRequestFromOrigin:listener:);
    if (![[m_webView UIDelegate] respondsToSelector:selector]) {
        request.userMediaAccessDenied();
        return;
    }

    WebUserMediaPolicyListener *listener = [[WebUserMediaPolicyListener alloc] initWithUserMediaRequest:&request];
    WebSecurityOrigin *webOrigin = [[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:request.securityOrigin()];

    CallUIDelegate(m_webView, selector, webOrigin, listener);
    AddRequestToRequestMap(&request, listener);

    [webOrigin release];
    [listener release];

    END_BLOCK_OBJC_EXCEPTIONS;
}

void WebUserMediaClient::cancelUserMediaAccessRequest(UserMediaRequest& request)
{
    UserMediaRequestsMap& requestsMap = userMediaRequestsMap();
    UserMediaRequestsMap::iterator it = requestsMap.find(&request);
    if (it == requestsMap.end())
        return;

    [it->value cancelUserMediaAccessRequest];
    requestsMap.remove(it);
}

void WebUserMediaClient::checkUserMediaPermission(UserMediaPermissionCheck& request)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    SEL selector = @selector(webView:checkPolicyForUserMediaRequestFromOrigin:listener:);
    if (![[m_webView UIDelegate] respondsToSelector:selector]) {
        request.setHasPersistentPermission(false);
        return;
    }

    WebUserMediaPolicyCheckerListener *listener = [[WebUserMediaPolicyCheckerListener alloc] initWithUserMediaPermissionCheck:&request];
    WebSecurityOrigin *webOrigin = [[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:request.securityOrigin()];

    CallUIDelegate(m_webView, selector, webOrigin, listener);
    AddPermissionCheckToMap(&request, listener);

    [webOrigin release];
    [listener release];

    END_BLOCK_OBJC_EXCEPTIONS;
}


void WebUserMediaClient::cancelUserMediaPermissionCheck(WebCore::UserMediaPermissionCheck& request)
{
    UserMediaCheckMap& requestsMap = userMediaCheckMap();
    UserMediaCheckMap::iterator it = requestsMap.find(&request);
    if (it == requestsMap.end())
        return;

    [it->value cancelUserMediaPermissionCheck];
    requestsMap.remove(it);
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

- (void)cancelUserMediaAccessRequest
{
#if ENABLE(MEDIA_STREAM)
    if (!_request)
        return;

    _request = nullptr;
#endif
    
}

- (void)allow
{
#if ENABLE(MEDIA_STREAM)
    if (!_request)
        return;
    
    _request->userMediaAccessGranted(_request->allowedAudioDeviceUID(), _request->allowedVideoDeviceUID());

    RemoveRequestFromRequestMap(_request.get());
#endif
}

- (void)deny
{
#if ENABLE(MEDIA_STREAM)
    if (!_request)
        return;
    
    _request->userMediaAccessDenied();
    RemoveRequestFromRequestMap(_request.get());
#endif
}

#if PLATFORM(IOS)
- (void)denyOnlyThisRequest
{
}

- (BOOL)shouldClearCache
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=146245
    ASSERT_NOT_REACHED();
    return true;
}
#endif

@end


@implementation WebUserMediaPolicyCheckerListener

- (id)initWithUserMediaPermissionCheck:(PassRefPtr<UserMediaPermissionCheck>)request
{
#if ENABLE(MEDIA_STREAM)
    if (!(self = [super init]))
        return nil;

    _request = request;
    return self;
#endif
}

- (void)cancelUserMediaPermissionCheck
{
#if ENABLE(MEDIA_STREAM)
    if (!_request)
        return;

    _request = nullptr;
#endif

}

- (void)allow
{
#if ENABLE(MEDIA_STREAM)
    if (!_request)
        return;

    _request->setHasPersistentPermission(true);
    RemovePermissionCheckFromMap(_request.get());
#endif
}

- (void)deny
{
#if ENABLE(MEDIA_STREAM)
    if (!_request)
        return;

    _request->setHasPersistentPermission(true);
    RemovePermissionCheckFromMap(_request.get());
#endif
}

#if PLATFORM(IOS)
- (void)denyOnlyThisRequest
{
}

- (BOOL)shouldClearCache
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=146245
    ASSERT_NOT_REACHED();
    return true;
}
#endif

@end

#endif // ENABLE(MEDIA_STREAM)
