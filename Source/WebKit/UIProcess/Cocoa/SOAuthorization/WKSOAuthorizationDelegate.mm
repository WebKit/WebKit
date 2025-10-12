/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKSOAuthorizationDelegate.h"

#if HAVE(APP_SSO)

#import "Logging.h"
#import "SOAuthorizationSession.h"
#import "WebPageProxy.h"
#import <wtf/RunLoop.h>

#define WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG(fmt, ...) RELEASE_LOG(AppSSO, "%p - WKSOAuthorizationDelegate::" fmt, &self, ##__VA_ARGS__)

@implementation WKSOAuthorizationDelegate {
    RefPtr<WebKit::SOAuthorizationSession> _session;
}

- (void)authorization:(SOAuthorization *)authorization presentViewController:(SOAuthorizationViewController)viewController withCompletion:(void (^)(BOOL success, NSError *error))completion
{
    ASSERT(RunLoop::isMain() && completion);
    WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorization (authorization = %p, _session = %p)", authorization, _session.get());
    RefPtr session = _session;
    if (!session) {
        WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorization: No session, so completing with NO as success state.");
        ASSERT_NOT_REACHED();
        completion(NO, nil);
        return;
    }

    if (!viewController) {
        WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorization: No view controller to present, so completing with NO as success state.");
        completion(NO, nil);
        return;
    }

    WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorization: presentingViewController %p", viewController);
    session->presentViewController(viewController, completion);
}

- (void)authorizationDidNotHandle:(SOAuthorization *)authorization
{
    ASSERT(RunLoop::isMain());
    WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorizationDidNotHandle: (authorization = %p, _session = %p)", authorization, _session.get());
    LOG_ERROR("Could not handle AppSSO.");
    RefPtr session = _session;
    if (!session) {
        WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorizationDidNotHandle: No session, so returning early.");
        ASSERT_NOT_REACHED();
        return;
    }
    WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorizationDidNotHandle: Falling back to web path.");
    session->fallBackToWebPath();
}

- (void)authorizationDidCancel:(SOAuthorization *)authorization
{
    ASSERT(RunLoop::isMain());
    WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorizationDidCancel: (authorization = %p, _session = %p)", authorization, _session.get());
    RefPtr session = _session;
    if (!session) {
        WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorizationDidCancel: No session, so returning early.");
        ASSERT_NOT_REACHED();
        return;
    }
    WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorizationDidCancel: Aborting session.");
    session->abort();
}

- (void)authorizationDidComplete:(SOAuthorization *)authorization
{
    ASSERT(RunLoop::isMain());
    WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorizationDidComplete: (authorization = %p, _session = %p)", authorization, _session.get());
    LOG_ERROR("Complete AppSSO without any data.");
    RefPtr session = _session;
    if (!session) {
        WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorizationDidComplete: No session, so returning early.");
        ASSERT_NOT_REACHED();
        return;
    }
    WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorizationDidComplete: Falling back to web path.");
    session->fallBackToWebPath();
}

- (void)authorization:(SOAuthorization *)authorization didCompleteWithHTTPAuthorizationHeaders:(NSDictionary *)httpAuthorizationHeaders
{
    ASSERT(RunLoop::isMain());
    WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorization:didCompleteWithHTTPAuthorizationHeaders: (authorization = %p, _session = %p)", authorization, _session.get());
    LOG_ERROR("Complete AppSSO with unexpected callback.");
    RefPtr session = _session;
    if (!session) {
        WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorization:didCompleteWithHTTPAuthorizationHeaders: No session, so returning early.");
        ASSERT_NOT_REACHED();
        return;
    }
    WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorization:didCompleteWithHTTPAuthorizationHeaders: Falling back to web path.");
    session->fallBackToWebPath();
}

- (void)authorization:(SOAuthorization *)authorization didCompleteWithHTTPResponse:(NSHTTPURLResponse *)httpResponse httpBody:(NSData *)httpBody
{
    ASSERT(RunLoop::isMain());
    WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorization:didCompleteWithHTTPResponse: (authorization = %p, _session = %p)", authorization, _session.get());
    RefPtr session = _session;
    if (!session) {
        WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorization:didCompleteWithHTTPResponse: No session, so returning early.");
        ASSERT_NOT_REACHED();
        return;
    }
    WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorization:didCompleteWithHTTPResponse: Completing.");
    session->complete(httpResponse, httpBody);
}

- (void)authorization:(SOAuthorization *)authorization didCompleteWithError:(NSError *)error
{
    ASSERT(RunLoop::isMain());
    WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorization:didCompleteWithError: (authorization = %p, _session = %p)", authorization, _session.get());
    if (error.code)
        LOG_ERROR("Could not complete AppSSO operation. Error: %zd", error.code);
    RefPtr session = _session;
    if (!session) {
        WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorization:didCompleteWithError: No session, so returning early.");
        ASSERT_NOT_REACHED();
        return;
    }
    WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("authorization:didCompleteWithError: Falling back to web path.");
    session->fallBackToWebPath();
}

- (void)setSession:(RefPtr<WebKit::SOAuthorizationSession>&&)session
{
    RELEASE_ASSERT(RunLoop::isMain());
    WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG("setSession: (existing session = %p, new session = %p)", _session.get(), session.get());
    _session = session.copyRef();

    if (session)
        session->shouldStart();
}
@end

#undef WKSOAUTHORIZATIONDELEGATE_RELEASE_LOG

#endif
