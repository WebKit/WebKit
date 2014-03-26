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

#ifndef RTCOfferAnswerOptionsPrivate_h
#define RTCOfferAnswerOptionsPrivate_h

#if ENABLE(MEDIA_STREAM)

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static bool validateRequestIdentity(const String& value)
{
    return value == "yes" || value == "no" || value == "ifconfigured";
}

class RTCOfferAnswerOptionsPrivate : public RefCounted<RTCOfferAnswerOptionsPrivate> {
public:
    static PassRefPtr<RTCOfferAnswerOptionsPrivate> create()
    {
        return adoptRef(new RTCOfferAnswerOptionsPrivate());
    }

    const String& requestIdentity() const { return m_requestIdentity; }
    void setRequestIdentity(const String& requestIdentity)
    {
        if (!validateRequestIdentity(requestIdentity))
            return;

        m_requestIdentity = requestIdentity;
    }

    virtual ~RTCOfferAnswerOptionsPrivate() { }

protected:
    RTCOfferAnswerOptionsPrivate()
        : m_requestIdentity("ifconfigured")
    {
    }

private:
    String m_requestIdentity;
};

class RTCOfferOptionsPrivate : public RTCOfferAnswerOptionsPrivate {
public:
    static PassRefPtr<RTCOfferOptionsPrivate> create()
    {
        return adoptRef(new RTCOfferOptionsPrivate());
    }

    int64_t offerToReceiveVideo() const { return m_offerToReceiveVideo; }
    void setOfferToReceiveVideo(int64_t offerToReceiveVideo) { m_offerToReceiveVideo = offerToReceiveVideo; }
    int64_t offerToReceiveAudio() const { return m_offerToReceiveAudio; }
    void setOfferToReceiveAudio(int64_t offerToReceiveAudio) { m_offerToReceiveAudio = offerToReceiveAudio; }
    bool voiceActivityDetection() const { return m_voiceActivityDetection; }
    void setVoiceActivityDetection(bool voiceActivityDetection) { m_voiceActivityDetection = voiceActivityDetection; }
    bool iceRestart() const { return m_iceRestart; }
    void setIceRestart(bool iceRestart) { m_iceRestart = iceRestart; }

    virtual ~RTCOfferOptionsPrivate() { }

private:
    RTCOfferOptionsPrivate()
        : RTCOfferAnswerOptionsPrivate()
        , m_offerToReceiveVideo(0)
        , m_offerToReceiveAudio(0)
        , m_voiceActivityDetection(true)
        , m_iceRestart(false)
    {
    }

    int64_t m_offerToReceiveVideo;
    int64_t m_offerToReceiveAudio;
    bool m_voiceActivityDetection;
    bool m_iceRestart;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // RTCOfferAnswerOptionsPrivate_h
