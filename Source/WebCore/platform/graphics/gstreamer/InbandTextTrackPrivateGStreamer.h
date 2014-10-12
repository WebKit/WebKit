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

#ifndef InbandTextTrackPrivateGStreamer_h
#define InbandTextTrackPrivateGStreamer_h

#if ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(VIDEO_TRACK)

#include "GRefPtrGStreamer.h"
#include "InbandTextTrackPrivate.h"
#include "TrackPrivateBaseGStreamer.h"
#include <wtf/gobject/GThreadSafeMainLoopSource.h>

namespace WebCore {

class MediaPlayerPrivateGStreamer;
typedef struct _GstSample GstSample;

class InbandTextTrackPrivateGStreamer : public InbandTextTrackPrivate, public TrackPrivateBaseGStreamer {
public:
    static PassRefPtr<InbandTextTrackPrivateGStreamer> create(gint index, GRefPtr<GstPad> pad)
    {
        return adoptRef(new InbandTextTrackPrivateGStreamer(index, pad));
    }

    virtual void disconnect() override;

    virtual AtomicString label() const override { return m_label; }
    virtual AtomicString language() const override { return m_language; }

    virtual int trackIndex() const override { return m_index; }
    String streamId() const { return m_streamId; }

    void handleSample(GRefPtr<GstSample>);
    void streamChanged();

    void notifyTrackOfSample();
    void notifyTrackOfStreamChanged();

private:
    InbandTextTrackPrivateGStreamer(gint index, GRefPtr<GstPad>);

    GThreadSafeMainLoopSource m_sampleTimerHandler;
    GThreadSafeMainLoopSource m_streamTimerHandler;
    gulong m_eventProbe;
    Vector<GRefPtr<GstSample> > m_pendingSamples;
    String m_streamId;
    Mutex m_sampleMutex;
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(VIDEO_TRACK)

#endif // InbandTextTrackPrivateGStreamer_h
