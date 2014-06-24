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

#ifndef ISOVTTCue_h
#define ISOVTTCue_h

#include <wtf/MediaTime.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class ArrayBuffer;
}

namespace WebCore {

// An ISOBox represents a ISO-BMFF box. Data in the structure is big-endian. The layout of the data structure as follows:
// 4 bytes : 4CC : identifier
// 4 bytes : unsigned : length
class ISOBox {
public:
    static String peekType(JSC::ArrayBuffer*);
    static size_t peekLength(JSC::ArrayBuffer*);
    static String peekString(JSC::ArrayBuffer*, unsigned offset, unsigned length);
    static unsigned boxHeaderSize() { return 2 * sizeof(uint32_t); }

    size_t length() const { return m_length; }
    const AtomicString& type() const { return m_type; }

protected:
    ISOBox(JSC::ArrayBuffer*);
    
private:
    size_t m_length;
    AtomicString m_type;
};

// 4 bytes : 4CC : identifier = 'vttc'
// 4 bytes : unsigned : length
// N bytes : CueSourceIDBox : box : optional
// N bytes : CueIDBox : box : optional
// N bytes : CueTimeBox : box : optional
// N bytes : CueSettingsBox : box : optional
// N bytes : CuePayloadBox : box : required
class ISOWebVTTCue : public ISOBox {
public:
    ISOWebVTTCue(const MediaTime& presentationTime, const MediaTime& duration, JSC::ArrayBuffer*);

    static const AtomicString& boxType();

    const MediaTime& presentationTime() const { return m_presentationTime; }
    const MediaTime& duration() const { return m_duration; }

    const String& sourceID() const { return m_sourceID; }
    const String& id() const { return m_identifer; }
    const String& originalStartTime() const { return m_originalStartTime; }
    const String& settings() const { return m_settings; }
    const String& cueText() const { return m_cueText; }

private:
    MediaTime m_presentationTime;
    MediaTime m_duration;

    String m_sourceID;
    String m_identifer;
    String m_originalStartTime;
    String m_settings;
    String m_cueText;
};

}

#endif // ISOVTTCue_h
