/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaPayload_h
#define MediaPayload_h

#if ENABLE(WEB_RTC)

#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class MediaPayload : public RefCounted<MediaPayload> {
public:
    static RefPtr<MediaPayload> create()
    {
        return adoptRef(new MediaPayload());
    }
    virtual ~MediaPayload() { }

    unsigned type() const { return m_type; }
    void setType(unsigned type) { m_type = type; }

    const String& encodingName() const { return m_encodingName; }
    void setEncodingName(const String & encodingName) { m_encodingName = encodingName; }

    unsigned clockRate() const { return m_clockRate; }
    void setClockRate(unsigned clockRate) { m_clockRate = clockRate; }

    unsigned channels() const { return m_channels; }
    void setChannels(unsigned channels) { m_channels = channels; }

    bool ccmfir() const { return m_ccmfir; }
    void setCcmfir(bool ccmfir) { m_ccmfir = ccmfir; }

    bool nackpli() const { return m_nackpli; }
    void setNackpli(bool nackpli) { m_nackpli = nackpli; }

    bool nack() const { return m_nack; }
    void setNack(bool nack) { m_nack = nack; }

    const HashMap<String, unsigned>& parameters() const { return m_parameters; }
    void addParameter(const String& name, unsigned value) { m_parameters.set(name, value); }

    RefPtr<MediaPayload> clone() const
    {
        RefPtr<MediaPayload> copy = create();

        copy->m_type = m_type;
        copy->m_encodingName = String(m_encodingName);
        copy->m_clockRate = m_clockRate;

        copy->m_channels = m_channels;

        copy->m_ccmfir = m_ccmfir;
        copy->m_nackpli = m_nackpli;
        copy->m_nack = m_nack;

        for (auto& key : m_parameters.keys())
            copy->m_parameters.add(key, m_parameters.get(key));

        return copy;
    }

private:
    MediaPayload() { }

    unsigned m_type { 0 };
    String m_encodingName;
    unsigned m_clockRate { 0 };

    // audio
    unsigned m_channels { 0 };

    // video
    bool m_ccmfir { false };
    bool m_nackpli { false };
    bool m_nack { false };

    HashMap<String, unsigned> m_parameters;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)

#endif // MediaPayload_h
