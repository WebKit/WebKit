/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ISOVTTCue.h"

#include "Logging.h"
#include <JavaScriptCore/DataView.h>
#include <wtf/JSONValues.h>
#include <wtf/URL.h>

using JSC::DataView;

namespace WebCore {

class ISOStringBox final : public ISOBox {
public:
    const String& contents() { return m_contents; }

private:
    bool parse(JSC::DataView& view, unsigned& offset) override
    {
        unsigned localOffset = offset;
        if (!ISOBox::parse(view, localOffset))
            return false;

        auto characterCount = m_size - (localOffset - offset);
        if (!characterCount) {
            m_contents = emptyString();
            return true;
        }

        Vector<LChar> characters;
        characters.reserveInitialCapacity(static_cast<size_t>(characterCount));
        while (characterCount--) {
            int8_t character = 0;
            if (!checkedRead<int8_t>(character, view, localOffset, BigEndian))
                return false;
            characters.uncheckedAppend(character);
        }

        m_contents = String::fromUTF8(characters);
        offset = localOffset;
        return true;
    }
    String m_contents;
};

static FourCC vttIdBoxType() { return "iden"; }
static FourCC vttSettingsBoxType() { return "sttg"; }
static FourCC vttPayloadBoxType() { return "payl"; }
static FourCC vttCurrentTimeBoxType() { return "ctim"; }
static FourCC vttCueSourceIDBoxType() { return "vsid"; }

ISOWebVTTCue::ISOWebVTTCue(const MediaTime& presentationTime, const MediaTime& duration)
    : m_presentationTime(presentationTime)
    , m_duration(duration)
{
}

ISOWebVTTCue::ISOWebVTTCue(MediaTime&& presentationTime, MediaTime&& duration, String&& sourceID, String&& id, String&& originalStartTime, String&& settings, String&& cueText)
    : m_presentationTime(WTFMove(presentationTime))
    , m_duration(WTFMove(duration))
    , m_sourceID(WTFMove(sourceID))
    , m_identifier(WTFMove(id))
    , m_originalStartTime(WTFMove(originalStartTime))
    , m_settings(WTFMove(settings))
    , m_cueText(WTFMove(cueText))
{
}

bool ISOWebVTTCue::parse(DataView& view, unsigned& offset)
{
    if (!ISOBox::parse(view, offset))
        return false;

    ISOStringBox stringBox;

    while (stringBox.read(view, offset)) {
        if (stringBox.boxType() == vttCueSourceIDBoxType())
            m_sourceID = stringBox.contents();
        else if (stringBox.boxType() == vttIdBoxType())
            m_identifier = stringBox.contents();
        else if (stringBox.boxType() == vttCurrentTimeBoxType())
            m_originalStartTime = stringBox.contents();
        else if (stringBox.boxType() == vttSettingsBoxType())
            m_settings = stringBox.contents();
        else if (stringBox.boxType() == vttPayloadBoxType())
            m_cueText = stringBox.contents();
        else
            LOG(Media, "ISOWebVTTCue::ISOWebVTTCue - skipping box id = \"%s\", size = %zu", stringBox.boxType().toString().utf8().data(), (size_t)stringBox.size());
    }
    return true;
}

String ISOWebVTTCue::toJSONString() const
{
    auto object = JSON::Object::create();

#if !LOG_DISABLED
    object->setString("text"_s, m_cueText);
#endif
    object->setString("sourceId"_s, encodeWithURLEscapeSequences(m_sourceID));
    object->setString("id"_s, encodeWithURLEscapeSequences(m_identifier));

    object->setString("originalStartTime"_s, encodeWithURLEscapeSequences(m_originalStartTime));
    object->setString("settings"_s, encodeWithURLEscapeSequences(m_settings));

    object->setDouble("presentationTime"_s, m_presentationTime.toDouble());
    object->setDouble("duration"_s, m_duration.toDouble());

    return object->toJSONString();
}

} // namespace WebCore
