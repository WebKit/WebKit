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

#include <WebCore/SpeechRecognitionConnectionClientIdentifier.h>
#include <WebCore/SpeechRecognitionRequestInfo.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class SpeechRecognitionRequest : public RefCountedAndCanMakeWeakPtr<SpeechRecognitionRequest> {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(SpeechRecognitionRequest, WEBCORE_EXPORT);
public:
    WEBCORE_EXPORT static Ref<SpeechRecognitionRequest> create(SpeechRecognitionRequestInfo&&);
    WEBCORE_EXPORT ~SpeechRecognitionRequest();

    SpeechRecognitionConnectionClientIdentifier clientIdentifier() const { return m_info.clientIdentifier; }
    const String& lang() const LIFETIME_BOUND { return m_info.lang; }
    bool continuous() const { return m_info.continuous;; }
    bool interimResults() const { return m_info.interimResults; }
    uint64_t maxAlternatives() const { return m_info.maxAlternatives; }
    const ClientOrigin& clientOrigin() const LIFETIME_BOUND { return m_info.clientOrigin; }
    FrameIdentifier mainFrameIdentifier() const { return m_info.mainFrameIdentifier; }

private:
    explicit SpeechRecognitionRequest(SpeechRecognitionRequestInfo&&);

    const SpeechRecognitionRequestInfo m_info;
};

} // namespace WebCore
