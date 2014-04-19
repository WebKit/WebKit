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
#include <runtime/Protect.h>

namespace WebCore {

DataCue::DataCue(ScriptExecutionContext& context, double start, double end, ArrayBuffer* data, const String& type, ExceptionCode& ec)
    : TextTrackCue(context, start, end)
    , m_type(type)
{
    setData(data, ec);
}

DataCue::DataCue(ScriptExecutionContext& context, double start, double end, const void* data, unsigned length)
    : TextTrackCue(context, start, end)
{
    m_data = ArrayBuffer::create(data, length);
}

#if ENABLE(DATACUE_VALUE)
DataCue::DataCue(ScriptExecutionContext& context, double start, double end, PassRefPtr<SerializedPlatformRepresentation> platformValue, const String& type)
    : TextTrackCue(context, start, end)
    , m_type(type)
    , m_platformValue(platformValue)
{
}

DataCue::DataCue(ScriptExecutionContext& context, double start, double end, JSC::JSValue value, const String& type)
    : TextTrackCue(context, start, end)
    , m_type(type)
    , m_value(value)
{
}
#endif

DataCue::~DataCue()
{
#if ENABLE(DATACUE_VALUE)
    if (m_value)
        JSC::gcUnprotect(m_value);
#endif
}

PassRefPtr<ArrayBuffer> DataCue::data() const
{
#if ENABLE(DATACUE_VALUE)
    if (m_platformValue)
        return m_platformValue->data();
#endif

    if (!m_data)
        return nullptr;

    return ArrayBuffer::create(m_data.get());
}

void DataCue::setData(ArrayBuffer* data, ExceptionCode& ec)
{
    if (!data) {
        ec = TypeError;
        return;
    }

#if ENABLE(DATACUE_VALUE)
    m_platformValue = nullptr;
    m_value = JSC::JSValue();
#endif

    m_data = ArrayBuffer::create(data);
}

#if !ENABLE(DATACUE_VALUE)
String DataCue::text(bool& isNull) const
{
    isNull = true;
    return String();
}
#endif

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

bool DataCue::isEqual(const TextTrackCue& cue, TextTrackCue::CueMatchRules match) const
{
    if (!TextTrackCue::isEqual(cue, match))
        return false;

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

#if ENABLE(DATACUE_VALUE)
    RefPtr<SerializedPlatformRepresentation> otherPlatformValue = dataCue->platformValue();
    if ((otherPlatformValue && !m_platformValue) || (!otherPlatformValue && m_platformValue))
        return false;
    if (m_platformValue && !m_platformValue->isEqual(*otherPlatformValue.get()))
        return false;

    JSC::JSValue otherValue = dataCue->value(nullptr);
    if ((otherValue && !m_value) || (!otherValue && m_value))
        return false;
    if (!JSC::JSValue::strictEqual(nullptr, m_value ? m_value : JSC::JSValue(), otherValue))
        return false;
#endif

    return true;
}

#if ENABLE(DATACUE_VALUE)
JSC::JSValue DataCue::value(JSC::ExecState* exec) const
{
    if (exec && m_platformValue)
        return m_platformValue->deserialize(exec);

    if (m_value)
        return m_value;

    return JSC::jsNull();
}

void DataCue::setValue(JSC::ExecState*, JSC::JSValue value)
{
    // FIXME: this should use a SerializedScriptValue.
    if (m_value)
        JSC::gcUnprotect(m_value);
    m_value = value;
    if (m_value)
        JSC::gcProtect(m_value);

    m_platformValue = nullptr;
    m_data = nullptr;
}
#endif

} // namespace WebCore

#endif
