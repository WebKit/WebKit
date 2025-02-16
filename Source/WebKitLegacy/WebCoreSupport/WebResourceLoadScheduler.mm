/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "WebResourceLoadScheduler.h"

#include "WebKitErrorsPrivate.h"
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>

WebCore::ResourceError WebResourceLoadScheduler::pluginWillHandleLoadError(const WebCore::ResourceResponse& response) const
{
    return WebResourceLoadScheduler::pluginWillHandleLoadErrorFromResponse(response);
}

WebCore::ResourceError WebResourceLoadScheduler::pluginWillHandleLoadErrorFromResponse(const WebCore::ResourceResponse& response)
{
    return [[[NSError alloc] _initWithPluginErrorCode:WebKitErrorPlugInWillHandleLoad contentURL:response.url() pluginPageURL:nil pluginName:nil MIMEType:response.mimeType()] autorelease];
}

WebCore::ResourceError WebResourceLoadScheduler::cancelledError(const WebCore::ResourceRequest& request) const
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain code:NSURLErrorCancelled URL:request.url()];
}

WebCore::ResourceError WebResourceLoadScheduler::blockedError(const WebCore::ResourceRequest& request) const
{
    return WebResourceLoadScheduler::blockedErrorFromRequest(request);
}

WebCore::ResourceError WebResourceLoadScheduler::blockedErrorFromRequest(const WebCore::ResourceRequest& request)
{
    return [NSError _webKitErrorWithDomain:WebKitErrorDomain code:WebKitErrorCannotUseRestrictedPort URL:request.url()];
}

WebCore::ResourceError WebResourceLoadScheduler::blockedByContentBlockerError(const WebCore::ResourceRequest& request) const
{
    RELEASE_ASSERT_NOT_REACHED(); // Content blockers are not enabled in WebKit1.
}

WebCore::ResourceError WebResourceLoadScheduler::cannotShowURLError(const WebCore::ResourceRequest& request) const
{
    return [NSError _webKitErrorWithDomain:WebKitErrorDomain code:WebKitErrorCannotShowURL URL:request.url()];
}

WebCore::ResourceError WebResourceLoadScheduler::interruptedForPolicyChangeError(const WebCore::ResourceRequest& request) const
{
    return [NSError _webKitErrorWithDomain:WebKitErrorDomain code:WebKitErrorFrameLoadInterruptedByPolicyChange URL:request.url()];
}

#if ENABLE(CONTENT_FILTERING)
WebCore::ResourceError WebResourceLoadScheduler::blockedByContentFilterError(const WebCore::ResourceRequest& request) const
{
    return [NSError _webKitErrorWithDomain:WebKitErrorDomain code:WebKitErrorFrameLoadBlockedByContentFilter URL:request.url()];
}
#endif

WebCore::ResourceError WebResourceLoadScheduler::cannotShowMIMETypeError(const WebCore::ResourceResponse& response) const
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain code:WebKitErrorCannotShowMIMEType URL:response.url()];
}

WebCore::ResourceError WebResourceLoadScheduler::fileDoesNotExistError(const WebCore::ResourceResponse& response) const
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain code:NSURLErrorFileDoesNotExist URL:response.url()];
}

WebCore::ResourceError WebResourceLoadScheduler::httpsUpgradeRedirectLoopError(const WebCore::ResourceRequest&) const
{
    RELEASE_ASSERT_NOT_REACHED(); // This error should never be created in WebKit1 because HTTPSOnly/First aren't available.
}

WebCore::ResourceError WebResourceLoadScheduler::httpNavigationWithHTTPSOnlyError(const WebCore::ResourceRequest&) const
{
    RELEASE_ASSERT_NOT_REACHED(); // This error should never be created in WebKit1 because HTTPSOnly/First aren't available.
}
