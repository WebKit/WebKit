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

#include "AbortableTaskQueue.h"
#include "GStreamerCommon.h"
#include "MediaPlayerPrivateGStreamerMSE.h"
#include "MediaSourceClientGStreamerMSE.h"
#include "SourceBufferPrivateGStreamer.h"

#include <atomic>
#include <gst/gst.h>
#include <mutex>
#include <wtf/Condition.h>
#include <wtf/Threading.h>

namespace WebCore {

#if !LOG_DISABLED || ENABLE(ENCRYPTED_MEDIA)
struct PadProbeInformation {
    AppendPipeline* appendPipeline;
    const char* description;
    gulong probeId;
};
#endif

class AppendPipeline {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AppendPipeline(Ref<MediaSourceClientGStreamerMSE>, Ref<SourceBufferPrivateGStreamer>, MediaPlayerPrivateGStreamerMSE&);
    virtual ~AppendPipeline();

    void pushNewBuffer(GRefPtr<GstBuffer>&&);
    void resetParserState();
    Ref<SourceBufferPrivateGStreamer> sourceBufferPrivate() { return m_sourceBufferPrivate.get(); }
    GstCaps* appsinkCaps() { return m_appsinkCaps.get(); }
    RefPtr<WebCore::TrackPrivateBase> track() { return m_track; }
    MediaPlayerPrivateGStreamerMSE* playerPrivate() { return m_playerPrivate; }

private:

    void handleErrorSyncMessage(GstMessage*);
    void handleNeedContextSyncMessage(GstMessage*);
    // For debug purposes only:
    void handleStateChangeMessage(GstMessage*);

    gint id();

    void handleAppsinkNewSampleFromStreamingThread(GstElement*);
    void handleErrorConditionFromStreamingThread();

    // Takes ownership of caps.
    void parseDemuxerSrcPadCaps(GstCaps*);
    void appsinkCapsChanged();
    void appsinkNewSample(GRefPtr<GstSample>&&);
    void handleEndOfAppend();
    void didReceiveInitializationSegment();
    AtomString trackId();

    GstBus* bus() { return m_bus.get(); }
    GstElement* pipeline() { return m_pipeline.get(); }
    GstElement* appsrc() { return m_appsrc.get(); }
    GstElement* appsink() { return m_appsink.get(); }
    GstCaps* demuxerSrcPadCaps() { return m_demuxerSrcPadCaps.get(); }
    WebCore::MediaSourceStreamTypeGStreamer streamType() { return m_streamType; }

    void disconnectDemuxerSrcPadFromAppsinkFromAnyThread(GstPad*);
    void connectDemuxerSrcPadToAppsinkFromStreamingThread(GstPad*);
    void connectDemuxerSrcPadToAppsink(GstPad*);

    void resetPipeline();

    void consumeAppsinkAvailableSamples();

    GstPadProbeReturn appsrcEndOfAppendCheckerProbe(GstPadProbeInfo*);

    static void staticInitialization();

    static std::once_flag s_staticInitializationFlag;
    static GType s_endOfAppendMetaType;
    static const GstMetaInfo* s_webKitEndOfAppendMetaInfo;

    // Used only for asserting that there is only one streaming thread.
    // Only the pointers are compared.
    WTF::Thread* m_streamingThread;

    // Used only for asserting EOS events are only caused by demuxing errors.
    bool m_errorReceived { false };

    Ref<MediaSourceClientGStreamerMSE> m_mediaSourceClient;
    Ref<SourceBufferPrivateGStreamer> m_sourceBufferPrivate;
    MediaPlayerPrivateGStreamerMSE* m_playerPrivate;

    // (m_mediaType, m_id) is unique.
    gint m_id;

    MediaTime m_initialDuration;

    GRefPtr<GstElement> m_pipeline;
    GRefPtr<GstBus> m_bus;
    GRefPtr<GstElement> m_appsrc;
    GRefPtr<GstElement> m_demux;
    GRefPtr<GstElement> m_parser; // Optional.
    // The demuxer has one src stream only, so only one appsink is needed and linked to it.
    GRefPtr<GstElement> m_appsink;

    // Used to avoid unnecessary notifications per sample.
    // It is read and written from the streaming thread and written from the main thread.
    // The main thread must set it to false before actually pulling samples.
    // This strategy ensures that at any time, there are at most two notifications in the bus
    // queue, instead of it growing unbounded.
    std::atomic_flag m_wasBusAlreadyNotifiedOfAvailableSamples;

    GRefPtr<GstCaps> m_appsinkCaps;
    GRefPtr<GstCaps> m_demuxerSrcPadCaps;
    FloatSize m_presentationSize;

#if !LOG_DISABLED
    struct PadProbeInformation m_demuxerDataEnteringPadProbeInformation;
    struct PadProbeInformation m_appsinkDataEnteringPadProbeInformation;
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    struct PadProbeInformation m_appsinkPadEventProbeInformation;
#endif

    WebCore::MediaSourceStreamTypeGStreamer m_streamType;
    RefPtr<WebCore::TrackPrivateBase> m_track;

    AbortableTaskQueue m_taskQueue;

    GRefPtr<GstBuffer> m_pendingBuffer;
};

} // namespace WebCore.

#endif // USE(GSTREAMER)
