/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#pragma once

#include "AuthenticationChallengeDisposition.h"
#include "AuthenticationChallengeProxy.h"
#include "AuthenticationDecisionListener.h"
#include "DownloadID.h"
#include "DownloadProxy.h"
#include <wtf/CompletionHandler.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
}

namespace WebKit {
class AuthenticationChallengeProxy;
class WebsiteDataStore;
class WebProtectionSpace;

enum class AllowOverwrite : bool;
}

namespace API {

class Data;

class DownloadClient : public RefCounted<DownloadClient> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(DownloadClient);
public:
    virtual ~DownloadClient() { }

    virtual void legacyDidStart(WebKit::DownloadProxy&) { }
    virtual void didReceiveAuthenticationChallenge(WebKit::DownloadProxy&, WebKit::AuthenticationChallengeProxy& challenge) { challenge.listener().completeChallenge(WebKit::AuthenticationChallengeDisposition::Cancel); }
    virtual void didReceiveData(WebKit::DownloadProxy&, uint64_t, uint64_t, uint64_t) { }
    virtual void decidePlaceholderPolicy(WebKit::DownloadProxy&, CompletionHandler<void(WebKit::UseDownloadPlaceholder, const WTF::URL&)>&& completionHandler) { completionHandler(WebKit::UseDownloadPlaceholder::No, { }); }
    virtual void decideDestinationWithSuggestedFilename(WebKit::DownloadProxy&, const WebCore::ResourceResponse&, const WTF::String&, CompletionHandler<void(WebKit::AllowOverwrite, WTF::String)>&& completionHandler) { completionHandler(WebKit::AllowOverwrite::No, { }); }
    virtual void didCreateDestination(WebKit::DownloadProxy&, const WTF::String&) { }
    virtual void didFinish(WebKit::DownloadProxy&) { }
    virtual void didFail(WebKit::DownloadProxy&, const WebCore::ResourceError&, API::Data* resumeData) { }
#if HAVE(MODERN_DOWNLOADPROGRESS)
    virtual void didReceivePlaceholderURL(WebKit::DownloadProxy&, const WTF::URL&, std::span<const uint8_t>, CompletionHandler<void()>&& completionHandler) { completionHandler(); }
    virtual void didReceiveFinalURL(WebKit::DownloadProxy&, const WTF::URL&, std::span<const uint8_t>) { }
#endif
    virtual void legacyDidCancel(WebKit::DownloadProxy&) { }
    virtual void processDidCrash(WebKit::DownloadProxy&) { }
    virtual void willSendRequest(WebKit::DownloadProxy&, WebCore::ResourceRequest&& request, const WebCore::ResourceResponse&, CompletionHandler<void(WebCore::ResourceRequest&&)>&& completionHandler) { completionHandler(WTFMove(request)); }
};

} // namespace API
