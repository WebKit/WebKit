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
    using ProcessBufferCallback = Function<void(const GRefPtr<GstBuffer>&)>;
    static Ref<GStreamerElementHarness> create(GRefPtr<GstElement>&& element, ProcessBufferCallback&& processOutputBufferCallback)
    {
        return adoptRef(*new GStreamerElementHarness(WTFMove(element), WTFMove(processOutputBufferCallback)));
    }
    ~GStreamerElementHarness();

    void start(GRefPtr<GstCaps>&&);

    bool pushSample(GstSample*);
    bool pushBuffer(GstBuffer*);
    bool pushEvent(GstEvent*);

    GRefPtr<GstBuffer> pullBuffer();
    GRefPtr<GstEvent> pullEvent();

    const GRefPtr<GstCaps>& outputCaps();
    GstElement* element() const { return m_element.get(); }

    void processOutputBuffers();
    void flush();

private:
    GStreamerElementHarness(GRefPtr<GstElement>&&, ProcessBufferCallback&&);

    GstFlowReturn chainBuffer(GstBuffer*);
    bool sinkQuery(GstPad*, GstObject*, GstQuery*);
    bool sinkEvent(GstEvent*);
    bool srcQuery(GstPad*, GstObject*, GstQuery*);
    bool srcEvent(GstEvent*);

    void pushStickyEvents(GRefPtr<GstCaps>&&);

    GRefPtr<GstElement> m_element;
    ProcessBufferCallback m_processOutputBufferCallback;

    GRefPtr<GstCaps> m_inputCaps;
    GRefPtr<GstCaps> m_outputCaps;

    GRefPtr<GstPad> m_srcPad;
    GRefPtr<GstPad> m_sinkPad;

    Lock m_bufferQueueLock;
    Deque<GRefPtr<GstBuffer>> m_bufferQueue WTF_GUARDED_BY_LOCK(m_bufferQueueLock);

    Lock m_srcEventQueueLock;
    Deque<GRefPtr<GstEvent>> m_srcEventQueue WTF_GUARDED_BY_LOCK(m_srcEventQueueLock);

    Lock m_sinkEventQueueLock;
    Deque<GRefPtr<GstEvent>> m_sinkEventQueue WTF_GUARDED_BY_LOCK(m_sinkEventQueueLock);

    Atomic<bool> m_playing { false };
    Atomic<bool> m_stickyEventsSent { false };
};

} // namespace WebCore

#endif // USE(GSTREAMER)
