/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016 Igalia S.L
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

#if ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(MEDIA_SOURCE)

#include "GRefPtrGStreamer.h"
#include "MediaPlayerPrivateGStreamerMSE.h"
#include "MediaSourceClientGStreamerMSE.h"
#include "SourceBufferPrivateGStreamer.h"

#include <gst/gst.h>
#include <wtf/Condition.h>

namespace WebCore {

#if !LOG_DISABLED
struct PadProbeInformation {
    AppendPipeline* appendPipeline;
    const char* description;
    gulong probeId;
};
#endif

class AppendPipeline : public ThreadSafeRefCounted<AppendPipeline> {
public:
    enum class AppendState { Invalid, NotStarted, Ongoing, KeyNegotiation, DataStarve, Sampling, LastSample, Aborting };

    AppendPipeline(Ref<MediaSourceClientGStreamerMSE>, Ref<SourceBufferPrivateGStreamer>, MediaPlayerPrivateGStreamerMSE&);
    virtual ~AppendPipeline();

    void handleNeedContextSyncMessage(GstMessage*);
    void handleApplicationMessage(GstMessage*);
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    void handleElementMessage(GstMessage*);
#endif

    gint id();
    AppendState appendState() { return m_appendState; }
    void setAppendState(AppendState);

    GstFlowReturn handleNewAppsinkSample(GstElement*);
    GstFlowReturn pushNewBuffer(GstBuffer*);
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    void dispatchDecryptionKey(GstBuffer*);
#endif

    // Takes ownership of caps.
    void parseDemuxerSrcPadCaps(GstCaps*);
    void appsinkCapsChanged();
    void appsinkNewSample(GstSample*);
    void appsinkEOS();
    void didReceiveInitializationSegment();
    AtomicString trackId();
    void abort();

    void clearPlayerPrivate();
    Ref<SourceBufferPrivateGStreamer> sourceBufferPrivate() { return m_sourceBufferPrivate.get(); }
    GstBus* bus() { return m_bus.get(); }
    GstElement* pipeline() { return m_pipeline.get(); }
    GstElement* appsrc() { return m_appsrc.get(); }
    GstElement* appsink() { return m_appsink.get(); }
    GstCaps* demuxerSrcPadCaps() { return m_demuxerSrcPadCaps.get(); }
    GstCaps* appsinkCaps() { return m_appsinkCaps.get(); }
    RefPtr<WebCore::TrackPrivateBase> track() { return m_track; }
    WebCore::MediaSourceStreamTypeGStreamer streamType() { return m_streamType; }

    void disconnectDemuxerSrcPadFromAppsinkFromAnyThread(GstPad*);
    void connectDemuxerSrcPadToAppsinkFromAnyThread(GstPad*);
    void connectDemuxerSrcPadToAppsink(GstPad*);

    void reportAppsrcAtLeastABufferLeft();
    void reportAppsrcNeedDataReceived();

private:
    void resetPipeline();
    void checkEndOfAppend();
    void handleAppsrcAtLeastABufferLeft();
    void handleAppsrcNeedDataReceived();
    void removeAppsrcDataLeavingProbe();
    void setAppsrcDataLeavingProbe();
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    void dispatchPendingDecryptionKey();
#endif

private:
    Ref<MediaSourceClientGStreamerMSE> m_mediaSourceClient;
    Ref<SourceBufferPrivateGStreamer> m_sourceBufferPrivate;
    MediaPlayerPrivateGStreamerMSE* m_playerPrivate;

    // (m_mediaType, m_id) is unique.
    gint m_id;

    MediaTime m_initialDuration;

    GstFlowReturn m_flowReturn;

    GRefPtr<GstElement> m_pipeline;
    GRefPtr<GstBus> m_bus;
    GRefPtr<GstElement> m_appsrc;
    GRefPtr<GstElement> m_demux;
#if ENABLE(LEGACY_ENCRYPTED_MEDIA) || ENABLE(ENCRYPTED_MEDIA)
    GRefPtr<GstElement> m_decryptor;
#endif
    // The demuxer has one src stream only, so only one appsink is needed and linked to it.
    GRefPtr<GstElement> m_appsink;

    Lock m_newSampleLock;
    Condition m_newSampleCondition;
    Lock m_padAddRemoveLock;
    Condition m_padAddRemoveCondition;

    GRefPtr<GstCaps> m_appsinkCaps;
    GRefPtr<GstCaps> m_demuxerSrcPadCaps;
    FloatSize m_presentationSize;

    bool m_appsrcAtLeastABufferLeft;
    bool m_appsrcNeedDataReceived;

    gulong m_appsrcDataLeavingProbeId;
#if !LOG_DISABLED
    struct PadProbeInformation m_demuxerDataEnteringPadProbeInformation;
    struct PadProbeInformation m_appsinkDataEnteringPadProbeInformation;
#endif

    // Keeps track of the states of append processing, to avoid performing actions inappropriate for the current state
    // (eg: processing more samples when the last one has been detected, etc.). See setAppendState() for valid
    // transitions.
    AppendState m_appendState;

    // Aborts can only be completed when the normal sample detection has finished. Meanwhile, the willing to abort is
    // expressed in this field.
    bool m_abortPending;

    WebCore::MediaSourceStreamTypeGStreamer m_streamType;
    RefPtr<WebCore::TrackPrivateBase> m_oldTrack;
    RefPtr<WebCore::TrackPrivateBase> m_track;

    GRefPtr<GstBuffer> m_pendingBuffer;
#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    GRefPtr<GstBuffer> m_pendingKey;
#endif
};

} // namespace WebCore.

#endif // USE(GSTREAMER)
