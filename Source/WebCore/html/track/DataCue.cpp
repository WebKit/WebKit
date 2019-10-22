/*
 * Copyright (C) 2014 Cable Television Labs Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(VIDEO_TRACK)
#include "DataCue.h"

#include "Logging.h"
#include "TextTrack.h"
#include "TextTrackCueList.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/StrongInlines.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
using namespace JSC;

WTF_MAKE_ISO_ALLOCATED_IMPL(DataCue);

DataCue::DataCue(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, ArrayBuffer& data, const String& type)
    : TextTrackCue(context, start, end)
    , m_type(type)
{
    setData(data);
}

DataCue::DataCue(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, const void* data, unsigned length)
    : TextTrackCue(context, start, end)
    , m_data(ArrayBuffer::create(data, length))
{
}

DataCue::DataCue(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, RefPtr<SerializedPlatformRepresentation>&& platformValue, const String& type)
    : TextTrackCue(context, start, end)
    , m_type(type)
    , m_platformValue(WTFMove(platformValue))
{
}

DataCue::DataCue(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, JSC::JSValue value, const String& type)
    : TextTrackCue(context, start, end)
    , m_type(type)
    , m_value(context.vm(), value)
{
}

DataCue::~DataCue()
{
}

RefPtr<ArrayBuffer> DataCue::data() const
{
    if (m_platformValue)
        return m_platformValue->data();

    if (!m_data)
        return nullptr;

    return ArrayBuffer::create(*m_data);
}

void DataCue::setData(ArrayBuffer& data)
{
    m_platformValue = nullptr;
    m_value.clear();
    m_data = ArrayBuffer::create(data);
}

DataCue* toDataCue(TextTrackCue* cue)
{
    ASSERT_WITH_SECURITY_IMPLICATION(cue->cueType() == TextTrackCue::Data);
    return static_cast<DataCue*>(cue);
}

const DataCue* toDataCue(const TextTrackCue* cue)
{
    ASSERT_WITH_SECURITY_IMPLICATION(cue->cueType() == TextTrackCue::Data);
    return static_cast<const DataCue*>(cue);
}

bool DataCue::cueContentsMatch(const TextTrackCue& cue) const
{
    if (cue.cueType() != TextTrackCue::Data)
        return false;

    const DataCue* dataCue = toDataCue(&cue);
    RefPtr<ArrayBuffer> otherData = dataCue->data();
    if ((otherData && !m_data) || (!otherData && m_data))
        return false;
    if (m_data && m_data->byteLength() != otherData->byteLength())
        return false;
    if (m_data && m_data->data() && memcmp(m_data->data(), otherData->data(), m_data->byteLength()))
        return false;

    const SerializedPlatformRepresentation* otherPlatformValue = dataCue->platformValue();
    if ((otherPlatformValue && !m_platformValue) || (!otherPlatformValue && m_platformValue))
        return false;
    if (m_platformValue && !m_platformValue->isEqual(*otherPlatformValue))
        return false;

    JSC::JSValue thisValue = valueOrNull();
    JSC::JSValue otherValue = dataCue->valueOrNull();
    if ((otherValue && !thisValue) || (!otherValue && thisValue))
        return false;
    if (!JSC::JSValue::strictEqual(nullptr, thisValue, otherValue))
        return false;

    return true;
}

bool DataCue::isEqual(const TextTrackCue& cue, TextTrackCue::CueMatchRules match) const
{
    if (!TextTrackCue::isEqual(cue, match))
        return false;

    if (cue.cueType() != TextTrackCue::Data)
        return false;

    return cueContentsMatch(cue);
}

bool DataCue::doesExtendCue(const TextTrackCue& cue) const
{
    if (!cueContentsMatch(cue))
        return false;

    return TextTrackCue::doesExtendCue(cue);
}

JSC::JSValue DataCue::value(JSC::JSGlobalObject& state) const
{
    if (m_platformValue)
        return m_platformValue->deserialize(&state);

    if (m_value)
        return m_value.get();

    return JSC::jsNull();
}

void DataCue::setValue(JSC::JSGlobalObject& state, JSC::JSValue value)
{
    // FIXME: this should use a SerializedScriptValue.
    m_value.set(state.vm(), value);
    m_platformValue = nullptr;
    m_data = nullptr;
}

JSValue DataCue::valueOrNull() const
{
    if (m_value)
        return m_value.get();

    return jsNull();
}

String DataCue::toJSONString() const
{
    auto object = JSON::Object::create();

    TextTrackCue::toJSON(object.get());

    if (!m_type.isEmpty())
        object->setString("type"_s, m_type);

    return object->toJSONString();
}

} // namespace WebCore

#endif
