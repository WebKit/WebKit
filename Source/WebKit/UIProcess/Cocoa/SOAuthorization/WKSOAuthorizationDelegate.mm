/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "SOAuthorizationSession.h"
#import <wtf/RunLoop.h>

@implementation WKSOAuthorizationDelegate

- (void)authorization:(SOAuthorization *)authorization presentViewController:(SOAuthorizationViewController)viewController withCompletion:(void (^)(BOOL success, NSError *error))completion
{
    ASSERT(RunLoop::isMain() && completion);
    if (!_session) {
        ASSERT_NOT_REACHED();
        completion(NO, nil);
        return;
    }
    _session->presentViewController(viewController, completion);
}

- (void)authorizationDidNotHandle:(SOAuthorization *)authorization
{
    ASSERT(RunLoop::isMain());
    LOG_ERROR("Could not handle AppSSO.");
    if (!_session) {
        ASSERT_NOT_REACHED();
        return;
    }
    _session->fallBackToWebPath();
}

- (void)authorizationDidCancel:(SOAuthorization *)authorization
{
    ASSERT(RunLoop::isMain());
    if (!_session) {
        ASSERT_NOT_REACHED();
        return;
    }
    _session->abort();
}

- (void)authorizationDidComplete:(SOAuthorization *)authorization
{
    ASSERT(RunLoop::isMain());
    LOG_ERROR("Complete AppSSO without any data.");
    if (!_session) {
        ASSERT_NOT_REACHED();
        return;
    }
    _session->fallBackToWebPath();
}

- (void)authorization:(SOAuthorization *)authorization didCompleteWithHTTPAuthorizationHeaders:(NSDictionary *)httpAuthorizationHeaders
{
    ASSERT(RunLoop::isMain());
    LOG_ERROR("Complete AppSSO with unexpected callback.");
    if (!_session) {
        ASSERT_NOT_REACHED();
        return;
    }
    _session->fallBackToWebPath();
}

- (void)authorization:(SOAuthorization *)authorization didCompleteWithHTTPResponse:(NSHTTPURLResponse *)httpResponse httpBody:(NSData *)httpBody
{
    ASSERT(RunLoop::isMain());
    if (!_session) {
        ASSERT_NOT_REACHED();
        return;
    }
    _session->complete(httpResponse, httpBody);
}

- (void)authorization:(SOAuthorization *)authorization didCompleteWithError:(NSError *)error
{
    ASSERT(RunLoop::isMain());
    LOG_ERROR("Could not complete AppSSO: %d", error.code);
    if (!_session) {
        ASSERT_NOT_REACHED();
        return;
    }
    _session->fallBackToWebPath();
}

- (void)setSession:(RefPtr<WebKit::SOAuthorizationSession>&&)session
{
    _session = WTFMove(session);
    if (_session)
        _session->shouldStart();
}

@end

#endif
