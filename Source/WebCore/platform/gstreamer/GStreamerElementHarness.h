/*
 * Copyright (C) 2023 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if USE(GSTREAMER)

#include "GRefPtrGStreamer.h"

#include <wtf/Deque.h>
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class GStreamerElementHarness : public ThreadSafeRefCounted<GStreamerElementHarness> {
    WTF_MAKE_FAST_ALLOCATED;

public:
    class Stream : public ThreadSafeRefCounted<Stream> {
        WTF_MAKE_FAST_ALLOCATED;

    public:
        static Ref<Stream> create(GRefPtr<GstPad>&& pad, RefPtr<GStreamerElementHarness>&& downstreamHarness)
        {
            return adoptRef(*new Stream(WTFMove(pad), WTFMove(downstreamHarness)));
        }

        ~Stream();

        GRefPtr<GstSample> pullSample();
        GRefPtr<GstEvent> pullEvent();

        bool sendEvent(GstEvent*);

        const GRefPtr<GstPad>& pad() const { return m_pad; }
        const GRefPtr<GstPad>& targetPad() const { return m_targetPad; }
        const GRefPtr<GstCaps>& outputCaps();

        const RefPtr<GStreamerElementHarness> downstreamHarness() const { return m_downstreamHarness; }

    private:
        Stream(GRefPtr<GstPad>&&, RefPtr<GStreamerElementHarness>&&);

        GstFlowReturn chainSample(GRefPtr<GstSample>&&);
        bool sinkEvent(GRefPtr<GstEvent>&&);

        GRefPtr<GstPad> m_pad;
        RefPtr<GStreamerElementHarness> m_downstreamHarness;

        GRefPtr<GstPad> m_targetPad;

        Lock m_sampleQueueLock;
        Deque<GRefPtr<GstSample>> m_sampleQueue WTF_GUARDED_BY_LOCK(m_sampleQueueLock);

        Lock m_sinkEventQueueLock;
        Deque<GRefPtr<GstEvent>> m_sinkEventQueue WTF_GUARDED_BY_LOCK(m_sinkEventQueueLock);

        GRefPtr<GstCaps> m_outputCaps WTF_GUARDED_BY_LOCK(m_sinkEventQueueLock);
    };

    using PadLinkCallback = Function<RefPtr<GStreamerElementHarness>(const GRefPtr<GstPad>&)>;
    using ProcessSampleCallback = Function<void(Stream&, GRefPtr<GstSample>&&)>;
    static Ref<GStreamerElementHarness> create(GRefPtr<GstElement>&& element, ProcessSampleCallback&& processOutputSampleCallback, std::optional<PadLinkCallback> padLinkCallback = std::nullopt)
    {
        return adoptRef(*new GStreamerElementHarness(WTFMove(element), WTFMove(processOutputSampleCallback), WTFMove(padLinkCallback)));
    }
    ~GStreamerElementHarness();

    void start(GRefPtr<GstCaps>&&, std::optional<const GstSegment*>&& = { });
    bool isStarted() const { return m_playing.loadRelaxed(); }
    void reset();

    bool pushSample(GRefPtr<GstSample>&&);
    bool pushBuffer(GRefPtr<GstBuffer>&&);
    bool pushEvent(GRefPtr<GstEvent>&&);

    GstPad* inputPad() const { return m_srcPad.get(); }
    const GRefPtr<GstCaps>& inputCaps() const { return m_inputCaps; }
    const Vector<RefPtr<Stream>>& outputStreams() const { return m_outputStreams; }

    GstElement* element() const { return m_element.get(); }

    void processOutputSamples();
    void flush();
    bool flushBuffers();

    void dumpGraph(const char* filenamePrefix);

private:
    GStreamerElementHarness(GRefPtr<GstElement>&&, ProcessSampleCallback&&, std::optional<PadLinkCallback>&&);

    GstFlowReturn pushBufferFull(GRefPtr<GstBuffer>&&);

    bool srcQuery(GstPad*, GstObject*, GstQuery*);
    bool srcEvent(GRefPtr<GstEvent>&&);

    void pushStickyEvents(GRefPtr<GstCaps>&&, std::optional<const GstSegment*>&& = { });
    void pushSegmentEvent(std::optional<const GstSegment*>&& = { });

    GRefPtr<GstElement> m_element;
    ProcessSampleCallback m_processOutputSampleCallback;
    std::optional<PadLinkCallback> m_padLinkCallback;

    GRefPtr<GstCaps> m_inputCaps;

    GRefPtr<GstPad> m_srcPad;
    Vector<RefPtr<Stream>> m_outputStreams;

    Lock m_srcEventQueueLock;
    Deque<GRefPtr<GstEvent>> m_srcEventQueue WTF_GUARDED_BY_LOCK(m_srcEventQueueLock);

    Atomic<bool> m_playing { false };
    Atomic<bool> m_capsEventSent { false };
    Atomic<bool> m_segmentEventSent { false };
};

} // namespace WebCore

#endif // USE(GSTREAMER)
