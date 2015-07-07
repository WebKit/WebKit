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

#ifndef RTCOfferAnswerOptions_h
#define RTCOfferAnswerOptions_h

#if ENABLE(MEDIA_STREAM)

#include "Dictionary.h"
#include "ExceptionCode.h"
#include "RTCOfferAnswerOptionsPrivate.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Dictionary;

class RTCOfferAnswerOptions : public RefCounted<RTCOfferAnswerOptions> {
public:
    static PassRefPtr<RTCOfferAnswerOptions> create(const Dictionary&, ExceptionCode&);

    const String& requestIdentity() const { return m_private->requestIdentity(); }
    RTCOfferAnswerOptionsPrivate* privateOfferAnswerOptions() const { return m_private.get(); }

    virtual ~RTCOfferAnswerOptions() { }

protected:
    virtual bool initialize(const Dictionary&);
    RTCOfferAnswerOptions() { }

    RefPtr<RTCOfferAnswerOptionsPrivate> m_private;
};

class RTCOfferOptions : public RTCOfferAnswerOptions {
public:
    static PassRefPtr<RTCOfferOptions> create(const Dictionary&, ExceptionCode&);

    int64_t offerToReceiveVideo() const { return privateOfferOptions()->offerToReceiveVideo(); }
    int64_t offerToReceiveAudio() const { return privateOfferOptions()->offerToReceiveAudio(); }
    bool voiceActivityDetection() const { return privateOfferOptions()->voiceActivityDetection(); }
    bool iceRestart() const { return privateOfferOptions()->iceRestart(); }
    RTCOfferOptionsPrivate* privateOfferOptions() const { return static_cast<RTCOfferOptionsPrivate*>(m_private.get()); }

    virtual ~RTCOfferOptions() { }

private:
    virtual bool initialize(const Dictionary&) override;
    RTCOfferOptions() { }
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // RTCOfferAnswerOptions_h
