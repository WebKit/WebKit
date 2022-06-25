/*
 * Copyright (C) 2020, 2021 Metrological Group B.V.
 * Copyright (C) 2020, 2021 Igalia, S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include <functional>
#include <wtf/Deque.h>

namespace WebCore {

class TrackQueue {
public:
    TrackQueue(AtomString trackId);

    typedef std::function<void(GRefPtr<GstMiniObject>&&)> NotEmptyHandler;
    typedef std::function<void()> LowLevelHandler;

    // Note: The TrackQueue methods are not thread-safe. TrackQueue must always be wrapped in a DataMutex<>.

    // For producer thread (main-thread):
    void enqueueObject(GRefPtr<GstMiniObject>&&);
    bool isFull() const { return durationEnqueued() >= s_durationEnqueuedHighWaterLevel; }
    void notifyWhenLowLevel(LowLevelHandler&&);
    void clear();
    void flush();

    // For consumer thread:
    bool isEmpty() const { return m_queue.isEmpty(); }
    GRefPtr<GstMiniObject> pop();
    void notifyWhenNotEmpty(NotEmptyHandler&&);
    bool hasNotEmptyHandler() const { return m_notEmptyCallback != nullptr; }
    void resetNotEmptyHandler();

private:
    // The point of having a queue for WebKitMediaSource is to limit the number of context switches per second.
    // If we had no queue, the main thread would have to be awaken for every frame. On the other hand, if the
    // queue had unlimited size WebKit would end up requesting flushes more often than necessary when frames
    // in the future are re-appended. As a sweet spot between these extremes we choose to allow enqueueing a
    // few seconds worth of samples.

    // `isReadyForMoreSamples` follows the classical two water levels strategy: initially it's true until the
    // high water level is reached, then it becomes false until the queue drains down to the low water level
    // and the cycle repeats. This way we avoid stalls and minimize context switches.

    static const GstClockTime s_durationEnqueuedHighWaterLevel = 5 * GST_SECOND;
    static const GstClockTime s_durationEnqueuedLowWaterLevel = 2 * GST_SECOND;

    GstClockTime durationEnqueued() const;
    void checkLowLevel();

    AtomString m_trackId;
    Deque<GRefPtr<GstMiniObject>> m_queue;
    LowLevelHandler m_lowLevelCallback;
    NotEmptyHandler m_notEmptyCallback;
};

}

#endif
