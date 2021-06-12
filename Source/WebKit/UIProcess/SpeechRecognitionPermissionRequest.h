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

#include "APIObject.h"
#include <WebCore/ClientOrigin.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/SpeechRecognitionError.h>
#include <WebCore/SpeechRecognitionRequest.h>
#include <wtf/CompletionHandler.h>

namespace WebKit {

using SpeechRecognitionPermissionRequestCallback = CompletionHandler<void(std::optional<WebCore::SpeechRecognitionError>&&)>;

class SpeechRecognitionPermissionRequest : public RefCounted<SpeechRecognitionPermissionRequest> {
public:
    static Ref<SpeechRecognitionPermissionRequest> create(WebCore::SpeechRecognitionRequest& request, SpeechRecognitionPermissionRequestCallback&& completionHandler)
    {
        return adoptRef(*new SpeechRecognitionPermissionRequest(request, WTFMove(completionHandler)));
    }

    ~SpeechRecognitionPermissionRequest()
    {
        if (m_completionHandler)
            m_completionHandler(WebCore::SpeechRecognitionError { WebCore::SpeechRecognitionErrorType::NotAllowed, "Request is cancelled"_s });
    }

    void complete(std::optional<WebCore::SpeechRecognitionError>&& error)
    {
        auto completionHandler = std::exchange(m_completionHandler, { });
        completionHandler(WTFMove(error));
    }

    WebCore::SpeechRecognitionRequest* request() { return m_request.get(); }

private:
    SpeechRecognitionPermissionRequest(WebCore::SpeechRecognitionRequest& request, SpeechRecognitionPermissionRequestCallback&& completionHandler)
        : m_request(makeWeakPtr(request))
        , m_completionHandler(WTFMove(completionHandler))
    { }


    WeakPtr<WebCore::SpeechRecognitionRequest> m_request;
    SpeechRecognitionPermissionRequestCallback m_completionHandler;
};

class SpeechRecognitionPermissionCallback : public API::ObjectImpl<API::Object::Type::SpeechRecognitionPermissionCallback> {
public:
    static Ref<SpeechRecognitionPermissionCallback> create(CompletionHandler<void(bool)>&& completionHandler)
    {
        return adoptRef(*new SpeechRecognitionPermissionCallback(WTFMove(completionHandler)));
    }

    void complete(bool granted) { m_completionHandler(granted); }

private:
    SpeechRecognitionPermissionCallback(CompletionHandler<void(bool)>&& completionHandler)
        : m_completionHandler(WTFMove(completionHandler))
    { }

    CompletionHandler<void(bool)> m_completionHandler;
};

} // namespace WebKit
