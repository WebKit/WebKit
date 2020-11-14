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
#include <wtf/CompletionHandler.h>

namespace WebKit {

enum class SpeechRecognitionPermissionDecision : bool { Deny, Grant };

class SpeechRecognitionPermissionRequest : public RefCounted<SpeechRecognitionPermissionRequest> {
public:
    static Ref<SpeechRecognitionPermissionRequest> create(const WebCore::ClientOrigin& origin, CompletionHandler<void(SpeechRecognitionPermissionDecision)>&& completionHandler)
    {
        return adoptRef(*new SpeechRecognitionPermissionRequest(origin, WTFMove(completionHandler)));
    }

    void complete(SpeechRecognitionPermissionDecision decision)
    {
        auto completionHandler = std::exchange(m_completionHandler, { });
        completionHandler(decision);
    }

    const WebCore::ClientOrigin& origin() const { return m_origin; }

private:
    SpeechRecognitionPermissionRequest(const WebCore::ClientOrigin& origin, CompletionHandler<void(SpeechRecognitionPermissionDecision)>&& completionHandler)
        : m_origin(origin)
        , m_completionHandler(WTFMove(completionHandler))
    { }

    WebCore::ClientOrigin m_origin;
    CompletionHandler<void(SpeechRecognitionPermissionDecision)> m_completionHandler;
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
