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

#include "SpeechRecognitionConnectionClientIdentifier.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

struct SpeechRecognitionError;
struct SpeechRecognitionResultData;

class SpeechRecognitionConnectionClient : public CanMakeWeakPtr<SpeechRecognitionConnectionClient> {
public:
    SpeechRecognitionConnectionClient()
        : m_identifier(SpeechRecognitionConnectionClientIdentifier::generate())
    {
    }

    virtual ~SpeechRecognitionConnectionClient() = default;

    virtual void didStart() = 0;
    virtual void didStartCapturingAudio() = 0;
    virtual void didStartCapturingSound() = 0;
    virtual void didStartCapturingSpeech() = 0;
    virtual void didStopCapturingSpeech() = 0;
    virtual void didStopCapturingSound() = 0;
    virtual void didStopCapturingAudio() = 0;
    virtual void didFindNoMatch() = 0;
    virtual void didReceiveResult(Vector<SpeechRecognitionResultData>&& resultDatas) = 0;
    virtual void didError(const SpeechRecognitionError&) = 0;
    virtual void didEnd() = 0;

    SpeechRecognitionConnectionClientIdentifier identifier() const { return m_identifier; };

private:
    SpeechRecognitionConnectionClientIdentifier m_identifier;
};

} // namespace WebCore
