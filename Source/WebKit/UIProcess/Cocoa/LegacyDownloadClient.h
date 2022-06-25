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

#import "APIDownloadClient.h"
#import "ProcessThrottler.h"
#import "WKFoundation.h"
#import <wtf/WeakObjCPtr.h>

@protocol _WKDownloadDelegate;

namespace WebCore {
class ResourceError;
class ResourceResponse;
}

namespace WebKit {

class LegacyDownloadClient final : public API::DownloadClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit LegacyDownloadClient(id <_WKDownloadDelegate>);
    
private:
    // From API::DownloadClient
    void legacyDidStart(DownloadProxy&) final;
    void didReceiveResponse(DownloadProxy&, const WebCore::ResourceResponse&);
    void didReceiveData(DownloadProxy&, uint64_t, uint64_t, uint64_t) final;
    void decideDestinationWithSuggestedFilename(DownloadProxy&, const WebCore::ResourceResponse&, const String& suggestedFilename, CompletionHandler<void(AllowOverwrite, String)>&&) final;
    void didFinish(DownloadProxy&) final;
    void didFail(DownloadProxy&, const WebCore::ResourceError&, API::Data*) final;
    void legacyDidCancel(DownloadProxy&) final;
    void willSendRequest(DownloadProxy&, WebCore::ResourceRequest&&, const WebCore::ResourceResponse&, CompletionHandler<void(WebCore::ResourceRequest&&)>&&) final;
    void didReceiveAuthenticationChallenge(DownloadProxy&, AuthenticationChallengeProxy&) final;
    void didCreateDestination(DownloadProxy&, const String&) final;
    void processDidCrash(DownloadProxy&) final;

#if USE(SYSTEM_PREVIEW)
    void takeActivityToken(DownloadProxy&);
    void releaseActivityTokenIfNecessary(DownloadProxy&);
#endif

    WeakObjCPtr<id <_WKDownloadDelegate>> m_delegate;

#if PLATFORM(IOS_FAMILY) && USE(SYSTEM_PREVIEW)
    std::unique_ptr<ProcessThrottler::BackgroundActivity> m_activity;
#endif

    struct {
        bool downloadDidStart : 1;            
        bool downloadDidReceiveResponse : 1;
        bool downloadDidReceiveData : 1;
        bool downloadDidWriteDataTotalBytesWrittenTotalBytesExpectedToWrite : 1;
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
