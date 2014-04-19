/*
 * Copyright (C) 2014 Cable Television Labs Inc. All rights reserved.
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
#include <wtf/RefCounted.h>

namespace WebCore {

class ScriptExecutionContext;

class DataCue : public TextTrackCue {
public:
    static PassRefPtr<DataCue> create(ScriptExecutionContext& context, double start, double end, ArrayBuffer* data, ExceptionCode& ec)
    {
        return adoptRef(new DataCue(context, start, end, data, ec));
    }

    static PassRefPtr<DataCue> create(ScriptExecutionContext& context, double start, double end, const void* data, unsigned length)
    {
        return adoptRef(new DataCue(context, start, end, data, length));
    }

    virtual ~DataCue();
    virtual CueType cueType() const { return Data; }

    RefPtr<ArrayBuffer> data() const;
    void setData(ArrayBuffer*, ExceptionCode&);
    String text(bool& isNull) const;

protected:
    DataCue(ScriptExecutionContext&, double start, double end, ArrayBuffer*, ExceptionCode&);
    DataCue(ScriptExecutionContext&, double start, double end, const void* data, unsigned length);

private:
    RefPtr<ArrayBuffer> m_data;
};

DataCue* toDataCue(TextTrackCue*);
const DataCue* toDataCue(const TextTrackCue*);

} // namespace WebCore

#endif
#endif
