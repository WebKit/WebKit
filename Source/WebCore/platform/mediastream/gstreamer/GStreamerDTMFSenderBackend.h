/*
 *  Copyright (C) 2019-2025 Igalia S.L.
 *  Copyright (C) 2025 Comcast Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#if ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)

#include "GRefPtrGStreamer.h"
#include "RTCDTMFSenderBackend.h"
#include "RealtimeOutgoingAudioSourceGStreamer.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class GStreamerDTMFSenderPrivate : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<GStreamerDTMFSenderPrivate, WTF::DestructionThread::Main> {
public:
    static RefPtr<GStreamerDTMFSenderPrivate> create();
    ~GStreamerDTMFSenderPrivate();

    using OnTonePlayedCallback = Function<void()>;
    void setOnTonePlayedCallback(OnTonePlayedCallback&& callback) { m_onTonePlayed = WTFMove(callback); }
    void playTone(const RefPtr<RealtimeOutgoingAudioSourceGStreamer>&, const char tone, size_t duration);

private:
    explicit GStreamerDTMFSenderPrivate(GRefPtr<GstElement>&&);

    void sendEvent(const GRefPtr<GstPad>&, int number, int volume, bool start);
    void stopTone(const GRefPtr<GstPad>&, int);

    OnTonePlayedCallback m_onTonePlayed;
    GRefPtr<GstElement> m_pipeline;
    GRefPtr<GstElement> m_dtmfSrc;
};

class GStreamerDTMFSenderBackend final : public RTCDTMFSenderBackend {
    WTF_MAKE_TZONE_ALLOCATED(GStreamerDTMFSenderBackend);
    WTF_MAKE_NONCOPYABLE(GStreamerDTMFSenderBackend);
public:
    explicit GStreamerDTMFSenderBackend(ThreadSafeWeakPtr<RealtimeOutgoingAudioSourceGStreamer>&&);
    ~GStreamerDTMFSenderBackend() = default;

private:
    // RTCDTMFSenderBackend
    bool canInsertDTMF() final;
    void playTone(const char tone, size_t duration, size_t interToneGap) final;
    void onTonePlayed(Function<void()>&&) final;
    String tones() const final;
    size_t duration() const final { return m_duration; }
    size_t interToneGap() const final { return m_interToneGap; }

    ThreadSafeWeakPtr<RealtimeOutgoingAudioSourceGStreamer> m_source;
    RefPtr<GStreamerDTMFSenderPrivate> m_senderPrivate;
    bool m_isFirstTone { true };
    bool m_canInsertDTMF { false };
    StringBuilder m_tones;
    size_t m_duration;
    size_t m_interToneGap;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC) && USE(GSTREAMER_WEBRTC)
