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

#import "WKFoundation.h"

#if WK_API_ENABLED

#import "APIDownloadClient.h"
#import "ProcessThrottler.h"
#import <wtf/WeakObjCPtr.h>

@protocol _WKDownloadDelegate;

namespace WebCore {
class ResourceError;
class ResourceResponse;
}

namespace WebKit {

class DownloadClient final : public API::DownloadClient {
public:
    explicit DownloadClient(id <_WKDownloadDelegate>);
    
private:
    // From API::DownloadClient
    void didStart(WebProcessPool&, DownloadProxy&) final;
    void didReceiveResponse(WebProcessPool&, DownloadProxy&, const WebCore::ResourceResponse&) final;
    void didReceiveData(WebProcessPool&, DownloadProxy&, uint64_t length) final;
    void decideDestinationWithSuggestedFilename(WebProcessPool&, DownloadProxy&, const String& suggestedFilename, Function<void(AllowOverwrite, String)>&&) final;
    void didFinish(WebProcessPool&, DownloadProxy&) final;
    void didFail(WebProcessPool&, DownloadProxy&, const WebCore::ResourceError&) final;
    void didCancel(WebProcessPool&, DownloadProxy&) final;
    void willSendRequest(WebProcessPool&, DownloadProxy&, WebCore::ResourceRequest&&, const WebCore::ResourceResponse&, CompletionHandler<void(WebCore::ResourceRequest&&)>&&) final;
    void didReceiveAuthenticationChallenge(WebProcessPool&, DownloadProxy&, AuthenticationChallengeProxy&) final;
    void didCreateDestination(WebProcessPool&, DownloadProxy&, const String&) final;
    void processDidCrash(WebProcessPool&, DownloadProxy&) final;

#if USE(SYSTEM_PREVIEW)
    void takeActivityToken(DownloadProxy&);
    void releaseActivityTokenIfNecessary(DownloadProxy&);
#endif

    WeakObjCPtr<id <_WKDownloadDelegate>> m_delegate;

#if PLATFORM(IOS_FAMILY) && USE(SYSTEM_PREVIEW)
    ProcessThrottler::BackgroundActivityToken m_activityToken { nullptr };
#endif

    struct {
        bool downloadDidStart : 1;            
        bool downloadDidReceiveResponse : 1;
        bool downloadDidReceiveData : 1;
        bool downloadDecideDestinationWithSuggestedFilenameAllowOverwrite : 1;
        bool downloadDecideDestinationWithSuggestedFilenameCompletionHandler : 1;
        bool downloadDidFinish : 1;
        bool downloadDidFail : 1;
        bool downloadDidCancel : 1;
        bool downloadDidReceiveServerRedirectToURL : 1;
        bool downloadDidReceiveAuthenticationChallengeCompletionHandler : 1;
        bool downloadShouldDecodeSourceDataOfMIMEType : 1;
        bool downloadDidCreateDestination : 1;
        bool downloadProcessDidCrash : 1;
    } m_delegateMethods;
};

} // namespace WebKit

#endif
