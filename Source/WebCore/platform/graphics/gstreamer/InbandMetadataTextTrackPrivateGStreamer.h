/*
 * Copyright (C) 2013 Cable Television Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InbandMetadataTextTrackPrivateGStreamer_h
#define InbandMetadataTextTrackPrivateGStreamer_h

#if ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(VIDEO_TRACK)

#include "GRefPtrGStreamer.h"
#include "InbandTextTrackPrivate.h"

namespace WebCore {

class InbandMetadataTextTrackPrivateGStreamer : public InbandTextTrackPrivate {
public:
    static PassRefPtr<InbandMetadataTextTrackPrivateGStreamer> create(Kind kind, CueFormat cueFormat, const AtomicString& id = emptyAtom)
    {
        return adoptRef(new InbandMetadataTextTrackPrivateGStreamer(kind, cueFormat, id));
    }

    ~InbandMetadataTextTrackPrivateGStreamer() { }

    virtual Kind kind() const override { return m_kind; }
    virtual AtomicString id() const override { return m_id; }
    virtual AtomicString inBandMetadataTrackDispatchType() const override { return m_inBandMetadataTrackDispatchType; }
    void setInBandMetadataTrackDispatchType(const AtomicString& value) { m_inBandMetadataTrackDispatchType = value; }

    void addDataCue(const MediaTime& start, const MediaTime& end, const void* data, unsigned length)
    {
        ASSERT(cueFormat() == Data);
        client()->addDataCue(this, start, end, data, length);
    }

    void addGenericCue(PassRefPtr<GenericCueData> data)
    {
        ASSERT(cueFormat() == Generic);
        client()->addGenericCue(this, data);
    }

private:
    InbandMetadataTextTrackPrivateGStreamer(Kind kind, CueFormat cueFormat, const AtomicString& id)
        : InbandTextTrackPrivate(cueFormat)
        , m_kind(kind)
        , m_id(id)
    {

    }

    Kind m_kind;
    AtomicString m_id;
    AtomicString m_inBandMetadataTrackDispatchType;
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(VIDEO_TRACK)

#endif // InbandMetadataTextTrackPrivateGStreamer_h
