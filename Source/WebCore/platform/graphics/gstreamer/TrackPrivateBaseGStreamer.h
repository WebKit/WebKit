/*
 * Copyright (C) 2013 Cable Television Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
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

#pragma once

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "AbortableTaskQueue.h"
#include "GStreamerCommon.h"
#include "MainThreadNotifier.h"
#include "MediaPlayerPrivateGStreamer.h"
#include <gst/gst.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class TrackPrivateBase;
using TrackID = uint64_t;
enum GStreamerTrackType {
    Audio,
    Video,
    Text,
    Unknown
};

class TrackPrivateBaseGStreamer;
class TrackDataHolder : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<TrackDataHolder> {
    WTF_MAKE_TZONE_ALLOCATED(TrackDataHolder);
    WTF_MAKE_NONCOPYABLE(TrackDataHolder);

public:
    static Ref<TrackDataHolder> create(TrackPrivateBaseGStreamer& track)
    {
        return adoptRef(*new TrackDataHolder(track));
    }
    virtual ~TrackDataHolder();

    void setPad(GRefPtr<GstPad>&&);
    void setStream(GRefPtr<GstStream>&&);

    void disconnect();

    void tagsChanged();
    void notifyTrackOfTagsChanged();

    void streamIdChanged();
    void streamChanged();
    void installUpdateConfigurationHandlers();
    bool updateTrackIDFromTags(const GRefPtr<GstTagList>&);

    GstObject* objectForLogging() const;

    enum MainThreadNotification {
        TagsChanged = 1 << 1,
        NewSample = 1 << 2,
        StreamChanged = 1 << 3
    };

    Ref<MainThreadNotifier<MainThreadNotification>> m_notifier;
    // FIXME: this should be optional...
    unsigned m_index { 0 };
    String m_label;
    String m_language;
    String m_gstStreamId;
    // Track ID parsed from stream-id.
    TrackID m_id;
    GRefPtr<GstPad> m_pad;
    GRefPtr<GstPad> m_bestUpstreamPad;
    GRefPtr<GstStream> m_stream;
    unsigned long m_eventProbe { 0 };
    GRefPtr<GstCaps> m_initialCaps;
    AbortableTaskQueue m_taskQueue;

    // Track ID inferred from container-specific-track-id tag.
    std::optional<TrackID> m_trackID;

    GStreamerTrackType m_type;
    ThreadSafeWeakPtr<TrackPrivateBase> m_owner;
    Lock m_tagMutex;
    GRefPtr<GstTagList> m_tags WTF_GUARDED_BY_LOCK(m_tagMutex);
    bool m_shouldUsePadStreamId { true };
    bool m_shouldHandleStreamStartEvent { true };

    ThreadSafeWeakPtr<MediaPlayerPrivateGStreamer> m_player;

    // The owning track, its lifetime is the same as its TrackDataHolder reference.
    TrackPrivateBaseGStreamer& m_track;
private:
    TrackDataHolder(TrackPrivateBaseGStreamer&);

    gulong m_streamTagsNotifyHandlerId { 0 };
    gulong m_streamCapsNotifyHandlerId { 0 };
    gulong m_padTagsNotifyHandlerId { 0 };
    gulong m_padCapsNotifyHandlerId { 0 };
};

class TrackPrivateBaseGStreamer {
public:
    friend class AudioTrackPrivateGStreamer;
    friend class InbandTextTrackPrivateGStreamer;
    friend class VideoTrackPrivateGStreamer;

    void disconnect();
    void setPad(GRefPtr<GstPad>&&);
    GRefPtr<GstPad> pad() const;

    virtual void setActive(bool) { }

    unsigned index();
    void setIndex(unsigned);

    GRefPtr<GstStream> stream() const;

    // Used for MSE, where the initial caps of the pad are relevant for initializing the matching pad in the
    // playback pipeline.
    void setInitialCaps(GRefPtr<GstCaps>&&);
    GRefPtr<GstCaps> initialCaps();

    TrackID streamId() const;
    String gstStreamId() const;

    virtual void updateConfigurationFromCaps(GRefPtr<GstCaps>&&) { }
    virtual void tagsChanged(GRefPtr<GstTagList>&&) { }
    virtual void capsChanged(TrackID, GRefPtr<GstCaps>&&) { }
    virtual void updateConfigurationFromTags(GRefPtr<GstTagList>&&) { }

private:
    TrackPrivateBaseGStreamer(ThreadSafeWeakPtr<MediaPlayerPrivateGStreamer>&&, GStreamerTrackType, TrackPrivateBase*, unsigned index, GRefPtr<GstPad>&&, bool shouldHandleStreamStartEvent);
    TrackPrivateBaseGStreamer(ThreadSafeWeakPtr<MediaPlayerPrivateGStreamer>&&, GStreamerTrackType, TrackPrivateBase*, unsigned index, GRefPtr<GstPad>&&, TrackID);
    TrackPrivateBaseGStreamer(ThreadSafeWeakPtr<MediaPlayerPrivateGStreamer>&&, GStreamerTrackType, TrackPrivateBase*, unsigned index, GstStream*);

    const Ref<TrackDataHolder> m_data;
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
