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
    AppendPipeline(SourceBufferPrivateGStreamer&, MediaPlayerPrivateGStreamerMSE&);
    virtual ~AppendPipeline();

    void pushNewBuffer(GRefPtr<GstBuffer>&&);
    void resetParserState();
    SourceBufferPrivateGStreamer& sourceBufferPrivate() { return m_sourceBufferPrivate; }
    MediaPlayerPrivateGStreamerMSE* playerPrivate() { return m_playerPrivate; }

private:
    // Similar to TrackPrivateBaseGStreamer::TrackType, but with a new value (Invalid) for when the codec is
    // not supported on this system, which should result in ParsingFailed error being thrown in SourceBuffer.
    enum StreamType { Audio, Video, Text, Unknown, Invalid };
#ifndef GST_DISABLE_GST_DEBUG
    static const char * streamTypeToString(StreamType);
#endif

    struct Track {
        // Track objects are created on pad-added for the first initialization segment, and destroyed after
        // the pipeline state has been set to GST_STATE_NULL.
        WTF_MAKE_NONCOPYABLE(Track);
        WTF_MAKE_FAST_ALLOCATED;
    public:

        Track(const AtomString& trackId, StreamType streamType, const GRefPtr<GstCaps>& caps, const FloatSize& presentationSize)
            : trackId(trackId)
            , streamType(streamType)
            , caps(caps)
            , presentationSize(presentationSize)
        { }

        AtomString trackId;
        StreamType streamType;
        GRefPtr<GstCaps> caps;
        FloatSize presentationSize;

        // Needed by some formats. To simplify the code, parser can be a GstIdentity when not needed.
        GRefPtr<GstElement> parser;
        GRefPtr<GstElement> appsink;
        GRefPtr<GstPad> entryPad; // Sink pad of the parser/GstIdentity.
        GRefPtr<GstPad> appsinkPad;

        RefPtr<WebCore::TrackPrivateBase> webKitTrack;

#if !LOG_DISABLED
        struct PadProbeInformation appsinkDataEnteringPadProbeInformation;
#endif
#if ENABLE(ENCRYPTED_MEDIA)
        struct PadProbeInformation appsinkPadEventProbeInformation;
#endif

        void initializeElements(AppendPipeline*, GstBin*);
    };

    void handleErrorSyncMessage(GstMessage*);
    void handleNeedContextSyncMessage(GstMessage*);
    // For debug purposes only:
    void handleStateChangeMessage(GstMessage*);

    void handleAppsinkNewSampleFromStreamingThread(GstElement*);
    void handleErrorCondition();
    void handleErrorConditionFromStreamingThread();

    void hookTrackEvents(Track&);
    static std::tuple<GRefPtr<GstCaps>, AppendPipeline::StreamType, FloatSize> parseDemuxerSrcPadCaps(GstCaps*);
    Ref<WebCore::TrackPrivateBase> makeWebKitTrack(int trackIndex);
    void appsinkCapsChanged(Track&);
    void appsinkNewSample(const Track&, GRefPtr<GstSample>&&);
    void handleEndOfAppend();
    void didReceiveInitializationSegment();

    GstBus* bus() { return m_bus.get(); }
    GstElement* pipeline() { return m_pipeline.get(); }
    GstElement* appsrc() { return m_appsrc.get(); }

    static AtomString generateTrackId(StreamType, int padIndex);
    enum class CreateTrackResult { TrackCreated, TrackIgnored, AppendParsingFailed };
    std::pair<CreateTrackResult, AppendPipeline::Track*> tryCreateTrackFromPad(GstPad* demuxerSrcPad, int padIndex);
    AppendPipeline::Track* tryMatchPadToExistingTrack(GstPad* demuxerSrcPad);
    void linkPadWithTrack(GstPad* demuxerSrcPad, Track&);

    void resetPipeline();

    void consumeAppsinksAvailableSamples();

    GstPadProbeReturn appsrcEndOfAppendCheckerProbe(GstPadProbeInfo*);

    static void staticInitialization();

    static std::once_flag s_staticInitializationFlag;
    static GType s_endOfAppendMetaType;
    static const GstMetaInfo* s_webKitEndOfAppendMetaInfo;

    // Used only for asserting that there is only one streaming thread.
    // Only the pointers are compared.
    WTF::Thread* m_streamingThread;

    bool m_hasReceivedFirstInitializationSegment { false };
    // Used only for asserting EOS events are only caused by demuxing errors.
    bool m_errorReceived { false };

    SourceBufferPrivateGStreamer& m_sourceBufferPrivate;
    MediaPlayerPrivateGStreamerMSE* m_playerPrivate;

    MediaTime m_initialDuration;
    GRefPtr<GstElement> m_pipeline;
    GRefPtr<GstBus> m_bus;
    GRefPtr<GstElement> m_appsrc;
    // To simplify the code, mtypefind and m_demux can be a GstIdentity when not needed.
    GRefPtr<GstElement> m_typefind;
    GRefPtr<GstElement> m_demux;

    Vector<std::unique_ptr<Track>> m_tracks;

    // Used to avoid unnecessary notifications per sample.
    // It is read and written from the streaming thread and written from the main thread.
    // The main thread must set it to false before actually pulling samples.
    // This strategy ensures that at any time, there are at most two notifications in the bus
    // queue, instead of it growing unbounded.
    std::atomic_flag m_wasBusAlreadyNotifiedOfAvailableSamples;

#if !LOG_DISABLED
    struct PadProbeInformation m_demuxerDataEnteringPadProbeInformation;
#endif

    AbortableTaskQueue m_taskQueue;
};

} // namespace WebCore.

#endif // USE(GSTREAMER)
