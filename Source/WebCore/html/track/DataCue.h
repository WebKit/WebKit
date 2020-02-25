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

#pragma once

#if ENABLE(VIDEO_TRACK)

#include "SerializedPlatformDataCue.h"
#include "TextTrackCue.h"
#include <wtf/MediaTime.h>
#include <wtf/TypeCasts.h>

namespace JSC {
class ArrayBuffer;
class JSValue;
}

namespace WebCore {

class ScriptExecutionContext;

class DataCue final : public TextTrackCue {
    WTF_MAKE_ISO_ALLOCATED(DataCue);
public:
    static Ref<DataCue> create(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, ArrayBuffer& data)
    {
        return adoptRef(*new DataCue(context, start, end, data, emptyString()));
    }

    static Ref<DataCue> create(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, const void* data, unsigned length)
    {
        return adoptRef(*new DataCue(context, start, end, data, length));
    }

    static Ref<DataCue> create(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, ArrayBuffer& data, const String& type)
    {
        return adoptRef(*new DataCue(context, start, end, data, type));
    }

    static Ref<DataCue> create(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, RefPtr<SerializedPlatformDataCue>&& platformValue, const String& type)
    {
        return adoptRef(*new DataCue(context, start, end, WTFMove(platformValue), type));
    }

    static Ref<DataCue> create(ScriptExecutionContext& context, double start, double end, ArrayBuffer& data)
    {
        return adoptRef(*new DataCue(context, MediaTime::createWithDouble(start), MediaTime::createWithDouble(end), data, emptyString()));
    }
    static Ref<DataCue> create(ScriptExecutionContext& context, double start, double end, JSC::JSValue value, const String& type)
    {
        return adoptRef(*new DataCue(context, MediaTime::createWithDouble(start), MediaTime::createWithDouble(end), value, type));
    }

    virtual ~DataCue();

    RefPtr<JSC::ArrayBuffer> data() const;
    void setData(JSC::ArrayBuffer&);

    const SerializedPlatformDataCue* platformValue() const { return m_platformValue.get(); }

    JSC::JSValue value(JSC::JSGlobalObject&) const;
    void setValue(JSC::JSGlobalObject&, JSC::JSValue);

    String type() const { return m_type; }
    void setType(const String& type) { m_type = type; }

private:
    DataCue(ScriptExecutionContext&, const MediaTime& start, const MediaTime& end, ArrayBuffer&, const String&);
    DataCue(ScriptExecutionContext&, const MediaTime& start, const MediaTime& end, const void*, unsigned);
    DataCue(ScriptExecutionContext&, const MediaTime& start, const MediaTime& end, RefPtr<SerializedPlatformDataCue>&&, const String&);
    DataCue(ScriptExecutionContext&, const MediaTime& start, const MediaTime& end, JSC::JSValue, const String&);

    JSC::JSValue valueOrNull() const;
    CueType cueType() const final { return Data; }
    bool cueContentsMatch(const TextTrackCue&) const final;
    void toJSON(JSON::Object&) const final;

    RefPtr<ArrayBuffer> m_data;
    String m_type;
    RefPtr<SerializedPlatformDataCue> m_platformValue;
    // FIXME: The following use of JSC::Strong is incorrect and can lead to storage leaks
    // due to reference cycles; we should use JSValueInWrappedObject instead.
    // https://bugs.webkit.org/show_bug.cgi?id=201173
    JSC::Strong<JSC::Unknown> m_value;
};

} // namespace WebCore

namespace WTF {

template<> struct LogArgument<WebCore::DataCue> : LogArgument<WebCore::TextTrackCue> { };

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::DataCue)
static bool isType(const WebCore::TextTrackCue& cue) { return cue.cueType() == WebCore::TextTrackCue::Data; }
SPECIALIZE_TYPE_TRAITS_END()

#endif
