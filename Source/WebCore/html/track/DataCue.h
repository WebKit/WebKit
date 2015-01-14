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

#ifndef DataCue_h
#define DataCue_h

#if ENABLE(VIDEO_TRACK)

#include "TextTrackCue.h"
#include <runtime/ArrayBuffer.h>
#include <runtime/JSCInlines.h>
#include <wtf/MediaTime.h>
#include <wtf/RefCounted.h>

#if ENABLE(DATACUE_VALUE)
#include "SerializedPlatformRepresentation.h"
#endif

namespace WebCore {

class ScriptExecutionContext;

class DataCue : public TextTrackCue {
public:
    static PassRefPtr<DataCue> create(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, ArrayBuffer* data, ExceptionCode& ec)
    {
        return adoptRef(new DataCue(context, start, end, data, emptyString(), ec));
    }

    static PassRefPtr<DataCue> create(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, const void* data, unsigned length)
    {
        return adoptRef(new DataCue(context, start, end, data, length));
    }

    static PassRefPtr<DataCue> create(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, ArrayBuffer* data, const String& type, ExceptionCode& ec)
    {
        return adoptRef(new DataCue(context, start, end, data, type, ec));
    }

#if ENABLE(DATACUE_VALUE)
    static PassRefPtr<DataCue> create(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, PassRefPtr<SerializedPlatformRepresentation> platformValue, const String& type)
    {
        return adoptRef(new DataCue(context, start, end, platformValue, type));
    }

    static PassRefPtr<DataCue> create(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, JSC::JSValue value, const String& type)
    {
        return adoptRef(new DataCue(context, start, end, value, type));
    }
#endif

    virtual ~DataCue();
    virtual CueType cueType() const { return Data; }

    PassRefPtr<ArrayBuffer> data() const;
    void setData(ArrayBuffer*, ExceptionCode&);

#if ENABLE(DATACUE_VALUE)
    const PassRefPtr<SerializedPlatformRepresentation> platformValue() const { return m_platformValue; }

    JSC::JSValue value(JSC::ExecState*) const;
    void setValue(JSC::ExecState*, JSC::JSValue);

    String type() const { return m_type; }
    void setType(const String& type) { m_type = type; }
#else
    String text(bool&) const;
#endif

    virtual bool isEqual(const TextTrackCue&, CueMatchRules) const override;
    virtual bool cueContentsMatch(const TextTrackCue&) const override;
    virtual bool doesExtendCue(const TextTrackCue&) const override;

protected:
    DataCue(ScriptExecutionContext&, const MediaTime& start, const MediaTime& end, ArrayBuffer*, const String&, ExceptionCode&);
    DataCue(ScriptExecutionContext&, const MediaTime& start, const MediaTime& end, const void*, unsigned);
#if ENABLE(DATACUE_VALUE)
    DataCue(ScriptExecutionContext&, const MediaTime& start, const MediaTime& end, PassRefPtr<SerializedPlatformRepresentation>, const String&);
    DataCue(ScriptExecutionContext&, const MediaTime& start, const MediaTime& end, JSC::JSValue, const String&);
#endif

private:
    RefPtr<ArrayBuffer> m_data;
    String m_type;
#if ENABLE(DATACUE_VALUE)
    RefPtr<SerializedPlatformRepresentation> m_platformValue;
    JSC::JSValue m_value;
#endif
};

DataCue* toDataCue(TextTrackCue*);
const DataCue* toDataCue(const TextTrackCue*);

} // namespace WebCore

#endif
#endif
