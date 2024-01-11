/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "SpeechRecognitionPermissionRequest.h"
#include <wtf/Deque.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

class WebPageProxy;

class SpeechRecognitionPermissionManager : public CanMakeWeakPtr<SpeechRecognitionPermissionManager> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class CheckResult { Denied, Granted, Unknown };
    explicit SpeechRecognitionPermissionManager(WebPageProxy&);
    ~SpeechRecognitionPermissionManager();
    void request(WebCore::SpeechRecognitionRequest&, SpeechRecognitionPermissionRequestCallback&&);

    void decideByDefaultAction(const WebCore::SecurityOriginData&, CompletionHandler<void(bool)>&&);
    WebPageProxy& page() { return m_page; }

private:
    Ref<WebPageProxy> protectedPage() const;

    void startNextRequest();
    void startProcessingRequest();
    void continueProcessingRequest();
    void completeCurrentRequest(std::optional<WebCore::SpeechRecognitionError>&& = std::nullopt);
    void requestMicrophoneAccess();
    void requestSpeechRecognitionServiceAccess();
    void requestUserPermission(WebCore::SpeechRecognitionRequest& request);

    WeakRef<WebPageProxy> m_page;
    Deque<Ref<SpeechRecognitionPermissionRequest>> m_requests;
    CheckResult m_microphoneCheck { CheckResult::Unknown };
    CheckResult m_speechRecognitionServiceCheck { CheckResult::Unknown };
    CheckResult m_userPermissionCheck { CheckResult::Unknown };
};

} // namespace WebKit
