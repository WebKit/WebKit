/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC)

#include "Dictionary.h"
#include "ExceptionCode.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Dictionary;

class RTCOfferAnswerOptions {
public:
    bool voiceActivityDetection() const { return m_voiceActivityDetection; }

protected:
    RTCOfferAnswerOptions() = default;
    bool initialize(const Dictionary&);

private:
    bool m_voiceActivityDetection { true };
};

// FIXME: Why is this reference counted?
class RTCOfferOptions : public RefCounted<RTCOfferOptions>, public RTCOfferAnswerOptions {
public:
    static ExceptionOr<Ref<RTCOfferOptions>> create(const Dictionary&);

    int64_t offerToReceiveVideo() const { return m_offerToReceiveVideo; }
    int64_t offerToReceiveAudio() const { return m_offerToReceiveAudio; }
    bool iceRestart() const { return m_iceRestart; }

private:
    RTCOfferOptions() = default;
    bool initialize(const Dictionary&);

    int64_t m_offerToReceiveVideo { 0 };
    int64_t m_offerToReceiveAudio { 0 };
    bool m_iceRestart { false };
};

// FIXME: Why is this reference counted?
class RTCAnswerOptions : public RefCounted<RTCAnswerOptions>, public RTCOfferAnswerOptions {
public:
    static ExceptionOr<Ref<RTCAnswerOptions>> create(const Dictionary&);

private:
    RTCAnswerOptions() = default;
    bool initialize(const Dictionary&);
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
