/*
 * Copyright (C) 2017 Igalia S.L
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

#if USE(LIBWEBRTC) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "RealtimeOutgoingAudioSource.h"

#include <gst/audio/audio.h>
#include <wtf/CheckedRef.h>
#include <wtf/Lock.h>
#include <wtf/MediaTime.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

class RealtimeOutgoingAudioSourceLibWebRTC final : public RealtimeOutgoingAudioSource, public CanMakeCheckedPtr<RealtimeOutgoingAudioSourceLibWebRTC> {
    WTF_MAKE_TZONE_ALLOCATED(RealtimeOutgoingAudioSourceLibWebRTC);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RealtimeOutgoingAudioSourceLibWebRTC);
public:
    static Ref<RealtimeOutgoingAudioSourceLibWebRTC> create(Ref<MediaStreamTrackPrivate>&& audioTrackPrivate)
    {
        return adoptRef(*new RealtimeOutgoingAudioSourceLibWebRTC(WTFMove(audioTrackPrivate)));
    }

private:
    explicit RealtimeOutgoingAudioSourceLibWebRTC(Ref<MediaStreamTrackPrivate>&&);
    ~RealtimeOutgoingAudioSourceLibWebRTC();

    // CheckedPtr interface
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    void setDidBeginCheckedPtrDeletion() final { CanMakeCheckedPtr::setDidBeginCheckedPtrDeletion(); }

    void audioSamplesAvailable(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t) final;

    bool isReachingBufferedAudioDataHighLimit() final;
    bool isReachingBufferedAudioDataLowLimit() final;
    bool hasBufferedEnoughData() final;

    void pullAudioData();

    GUniquePtr<GstAudioConverter> m_sampleConverter;
    GstAudioInfo m_inputStreamDescription;
    GstAudioInfo m_outputStreamDescription;

    Lock m_adapterLock;
    GRefPtr<GstAdapter> m_adapter WTF_GUARDED_BY_LOCK(m_adapterLock);
    Vector<uint8_t> m_audioBuffer;
};

} // namespace WebCore

#endif // USE(LIBWEBRTC) && USE(GSTREAMER)
