/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebErrors.h"

#include "APIError.h"
#include <WebCore/LocalizedStrings.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>

using namespace WebCore;

namespace WebKit {

ResourceError blockedError(const ResourceRequest& request)
{
    return ResourceError(API::Error::webKitPolicyErrorDomain(), API::Error::Policy::CannotUseRestrictedPort, request.url(), WEB_UI_STRING("Not allowed to use restricted network port", "WebKitErrorCannotUseRestrictedPort description"));
}

ResourceError blockedByContentBlockerError(const ResourceRequest& request)
{
    return ResourceError(API::Error::webKitPolicyErrorDomain(), API::Error::Policy::FrameLoadBlockedByContentBlocker, request.url(), WEB_UI_STRING("The URL was blocked by a content blocker", "WebKitErrorBlockedByContentBlocker description"));
}

ResourceError cannotShowURLError(const ResourceRequest& request)
{
    return ResourceError(API::Error::webKitPolicyErrorDomain(), API::Error::Policy::CannotShowURL, request.url(), WEB_UI_STRING("The URL can’t be shown", "WebKitErrorCannotShowURL description"));
}

ResourceError interruptedForPolicyChangeError(const ResourceRequest& request)
{
    return ResourceError(API::Error::webKitPolicyErrorDomain(), API::Error::Policy::FrameLoadInterruptedByPolicyChange, request.url(), WEB_UI_STRING("Frame load interrupted", "WebKitErrorFrameLoadInterruptedByPolicyChange description"));
}

#if ENABLE(CONTENT_FILTERING)
ResourceError blockedByContentFilterError(const ResourceRequest& request)
{
    return ResourceError(API::Error::webKitPolicyErrorDomain(), API::Error::Policy::FrameLoadBlockedByContentFilter, request.url(), WEB_UI_STRING("The URL was blocked by a content filter", "WebKitErrorFrameLoadBlockedByContentFilter description"));
}
#endif

ResourceError cannotShowMIMETypeError(const ResourceResponse& response)
{
    return ResourceError(API::Error::webKitPolicyErrorDomain(), API::Error::Policy::CannotShowMIMEType, response.url(), WEB_UI_STRING("Content with specified MIME type can’t be shown", "WebKitErrorCannotShowMIMEType description"));
}

ResourceError pluginWillHandleLoadError(const ResourceResponse& response)
{
    return ResourceError(API::Error::webKitPluginErrorDomain(), API::Error::Plugin::PlugInWillHandleLoad, response.url(), WEB_UI_STRING("Plug-in handled load", "WebKitErrorPlugInWillHandleLoad description"));
}

ResourceError internalError(const URL& url)
{
    return ResourceError(API::Error::webKitErrorDomain(), API::Error::General::Internal, url, WEB_UI_STRING("WebKit encountered an internal error", "WebKitErrorInternal description"));
}

} // namespace WebKit
