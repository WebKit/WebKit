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
#include "InbandGenericCue.h"

#if ENABLE(VIDEO)

#include "ColorSerialization.h"


namespace WebCore {


InbandGenericCue::InbandGenericCue()
{
    m_cueData.m_uniqueId = InbandGenericCueIdentifier::generate();
}

String InbandGenericCue::toJSONString() const
{
    auto object = JSON::Object::create();

    object->setString("text"_s, m_cueData.m_content);
    object->setInteger("identifier"_s, m_cueData.m_uniqueId.toUInt64());
    object->setDouble("start"_s, m_cueData.m_startTime.toDouble());
    object->setDouble("end"_s, m_cueData.m_endTime.toDouble());

    ASCIILiteral status = ""_s;
    switch (m_cueData.m_status) {
    case GenericCueData::Status::Uninitialized:
        status = "Uninitialized"_s;
        break;
    case GenericCueData::Status::Partial:
        status = "Partial"_s;
        break;
    case GenericCueData::Status::Complete:
        status = "Complete"_s;
        break;
    }
    object->setString("status"_s, status);

    if (!m_cueData.m_id.isEmpty())
        object->setString("id"_s, m_cueData.m_id);

    if (m_cueData.m_line > 0)
        object->setDouble("line"_s, m_cueData.m_line);

    if (m_cueData.m_size > 0)
        object->setDouble("size"_s, m_cueData.m_size);

    if (m_cueData.m_position > 0)
        object->setDouble("position"_s, m_cueData.m_position);

    if (m_cueData.m_positionAlign != GenericCueData::Alignment::None) {
        ASCIILiteral positionAlign = ""_s;
        switch (m_cueData.m_positionAlign) {
        case GenericCueData::Alignment::Start:
            positionAlign = "Start"_s;
            break;
        case GenericCueData::Alignment::Middle:
            positionAlign = "Middle"_s;
            break;
        case GenericCueData::Alignment::End:
            positionAlign = "End"_s;
            break;
        case GenericCueData::Alignment::None:
            positionAlign = "None"_s;
            break;
        }
        object->setString("positionAlign"_s, positionAlign);
    }

    if (m_cueData.m_align != GenericCueData::Alignment::None) {
        ASCIILiteral align = ""_s;
        switch (m_cueData.m_align) {
        case GenericCueData::Alignment::Start:
            align = "Start"_s;
            break;
        case GenericCueData::Alignment::Middle:
            align = "Middle"_s;
            break;
        case GenericCueData::Alignment::End:
            align = "End"_s;
            break;
        case GenericCueData::Alignment::None:
            align = "None"_s;
            break;
        }
        object->setString("align"_s, align);
    }

    if (m_cueData.m_foregroundColor.isValid())
        object->setString("foregroundColor"_s, serializationForHTML(m_cueData.m_foregroundColor));

    if (m_cueData.m_backgroundColor.isValid())
        object->setString("backgroundColor"_s, serializationForHTML(m_cueData.m_backgroundColor));

    if (m_cueData.m_highlightColor.isValid())
        object->setString("highlightColor"_s, serializationForHTML(m_cueData.m_highlightColor));

    if (m_cueData.m_baseFontSize)
        object->setDouble("baseFontSize"_s, m_cueData.m_baseFontSize);

    if (m_cueData.m_relativeFontSize)
        object->setDouble("relativeFontSize"_s, m_cueData.m_relativeFontSize);

    if (!m_cueData.m_fontName.isEmpty())
        object->setString("font"_s, m_cueData.m_fontName);

    return object->toJSONString();
}

bool GenericCueData::equalNotConsideringTimesOrId(const GenericCueData& other) const
{
    return m_relativeFontSize == other.m_relativeFontSize
        && m_baseFontSize == other.m_baseFontSize
        && m_position == other.m_position
        && m_line == other.m_line
        && m_size == other.m_size
        && m_align == other.m_align
        && m_foregroundColor == other.m_foregroundColor
        && m_backgroundColor == other.m_backgroundColor
        && m_highlightColor == other.m_highlightColor
        && m_fontName == other.m_fontName
        && m_id == other.m_id
        && m_content == other.m_content;
}

}

#endif
