/*
 * Copyright (C) 2016, 2017 Metrological Group B.V.
 * Copyright (C) 2016, 2017 Igalia S.L
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

#include "config.h"
#include "AppendPipeline.h"
#include "AbortableTaskQueue.h"
#include "MediaSourcePrivateGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(MEDIA_SOURCE)

#include "AudioTrackPrivateGStreamer.h"
#include "GStreamerCommon.h"
#include "GStreamerEMEUtilities.h"
#include "GStreamerMediaDescription.h"
#include "GStreamerRegistryScannerMSE.h"
#include "InbandTextTrackPrivateGStreamer.h"
#include "MediaDescription.h"
#include "MediaSampleGStreamer.h"
#include "SourceBufferPrivateGStreamer.h"
#include "VideoTrackPrivateGStreamer.h"
#include <functional>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/video/video.h>
#include <wtf/Condition.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/text/StringConcatenateNumbers.h>

GST_DEBUG_CATEGORY_EXTERN(webkit_mse_debug);
#define GST_CAT_DEFAULT webkit_mse_debug

namespace WebCore {

GType AppendPipeline::s_endOfAppendMetaType = 0;
const GstMetaInfo* AppendPipeline::s_webKitEndOfAppendMetaInfo = nullptr;
std::once_flag AppendPipeline::s_staticInitializationFlag;

struct EndOfAppendMeta {
    GstMeta base;
    static gboolean init(GstMeta*, void*, GstBuffer*) { return TRUE; }
    static gboolean transform(GstBuffer*, GstMeta*, GstBuffer*, GQuark, void*) { g_return_val_if_reached(FALSE); }
    static void free(GstMeta*, GstBuffer*) { }
};

void AppendPipeline::staticInitialization()
{
    ASSERT(isMainThread());

    const char* tags[] = { nullptr };
    s_endOfAppendMetaType = gst_meta_api_type_register("WebKitEndOfAppendMetaAPI", tags);
    s_webKitEndOfAppendMetaInfo = gst_meta_register(s_endOfAppendMetaType, "WebKitEndOfAppendMeta", sizeof(EndOfAppendMeta), EndOfAppendMeta::init, EndOfAppendMeta::free, EndOfAppendMeta::transform);
}

#if !LOG_DISABLED
static GstPadProbeReturn appendPipelinePadProbeDebugInformation(GstPad*, GstPadProbeInfo*, struct PadProbeInformation*);
#endif

#if ENABLE(ENCRYPTED_MEDIA)
static GstPadProbeReturn appendPipelineAppsinkPadEventProbe(GstPad*, GstPadProbeInfo*, struct PadProbeInformation*);
#endif
static GstPadProbeReturn appendPipelineDemuxerBlackHolePadProbe(GstPad*, GstPadProbeInfo*, gpointer);
static GstPadProbeReturn matroskademuxForceSegmentStartToEqualZero(GstPad*, GstPadProbeInfo*, void*);
static GRefPtr<GstElement> createOptionalParserForFormat(const AtomString&, const GstCaps*);

// Wrapper for gst_element_set_state() that emits a critical if the state change fails or is not synchronous.
static void assertedElementSetState(GstElement* element, GstState desiredState)
{
    GstState oldState;
    gst_element_get_state(element, &oldState, nullptr, 0);

    GstStateChangeReturn result = gst_element_set_state(element, desiredState);

    GstState newState;
    gst_element_get_state(element, &newState, nullptr, 0);

    if (desiredState != newState || result != GST_STATE_CHANGE_SUCCESS) {
        GST_ERROR("AppendPipeline state change failed (returned %d): %" GST_PTR_FORMAT " %d -> %d (expected %d)",
            static_cast<int>(result), element, static_cast<int>(oldState), static_cast<int>(newState), static_cast<int>(desiredState));
        ASSERT_NOT_REACHED();
    }
}

AppendPipeline::AppendPipeline(SourceBufferPrivateGStreamer& sourceBufferPrivate, MediaPlayerPrivateGStreamerMSE& playerPrivate)
    : m_sourceBufferPrivate(sourceBufferPrivate)
    , m_playerPrivate(&playerPrivate)
    , m_wasBusAlreadyNotifiedOfAvailableSamples(false)
{
    ASSERT(isMainThread());
    std::call_once(s_staticInitializationFlag, AppendPipeline::staticInitialization);

    GST_TRACE("Creating AppendPipeline (%p)", this);

    // FIXME: give a name to the pipeline, maybe related with the track it's managing.
    // The track name is still unknown at this time, though.
    static size_t appendPipelineCount = 0;
    String pipelineName = makeString("append-pipeline-",
        m_sourceBufferPrivate.type().containerType().replace("/", "-"), '-', appendPipelineCount++);
    m_pipeline = gst_pipeline_new(pipelineName.utf8().data());

    m_bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
    gst_bus_add_signal_watch_full(m_bus.get(), RunLoopSourcePriority::RunLoopDispatcher);
    gst_bus_enable_sync_message_emission(m_bus.get());

    g_signal_connect(m_bus.get(), "sync-message::error", G_CALLBACK(+[](GstBus*, GstMessage* message, AppendPipeline* appendPipeline) {
        appendPipeline->handleErrorSyncMessage(message);
    }), this);
    g_signal_connect(m_bus.get(), "sync-message::need-context", G_CALLBACK(+[](GstBus*, GstMessage* message, AppendPipeline* appendPipeline) {
        appendPipeline->handleNeedContextSyncMessage(message);
    }), this);
    g_signal_connect(m_bus.get(), "message::state-changed", G_CALLBACK(+[](GstBus*, GstMessage* message, AppendPipeline* appendPipeline) {
        appendPipeline->handleStateChangeMessage(message);
    }), this);

    // We assign the created instances here instead of adoptRef() because gst_bin_add_many()
    // below will already take the initial reference and we need an additional one for us.
    m_appsrc = makeGStreamerElement("appsrc", nullptr);

    GRefPtr<GstPad> appsrcPad = adoptGRef(gst_element_get_static_pad(m_appsrc.get(), "src"));
    gst_pad_add_probe(appsrcPad.get(), GST_PAD_PROBE_TYPE_BUFFER, [](GstPad*, GstPadProbeInfo* padProbeInfo, void* userData) {
        return static_cast<AppendPipeline*>(userData)->appsrcEndOfAppendCheckerProbe(padProbeInfo);
    }, this, nullptr);

    const String& type = m_sourceBufferPrivate.type().containerType();
    GST_DEBUG("SourceBuffer containerType: %s", type.utf8().data());
    bool hasDemuxer = true;
    if (type.endsWith("mp4") || type.endsWith("aac")) {
        m_demux = makeGStreamerElement("qtdemux", nullptr);
        m_typefind = makeGStreamerElement("identity", nullptr);
    } else if (type.endsWith("webm")) {
        m_demux = makeGStreamerElement("matroskademux", nullptr);
        m_typefind = makeGStreamerElement("identity", nullptr);
    } else if (type == "audio/mpeg") {
        m_demux = makeGStreamerElement("identity", nullptr);
        m_typefind = makeGStreamerElement("typefind", nullptr);
        hasDemuxer = false;
    } else
        ASSERT_NOT_REACHED();

#if !LOG_DISABLED
    GRefPtr<GstPad> demuxerPad = adoptGRef(gst_element_get_static_pad(m_demux.get(), "sink"));
    m_demuxerDataEnteringPadProbeInformation.appendPipeline = this;
    m_demuxerDataEnteringPadProbeInformation.description = "demuxer data entering";
    m_demuxerDataEnteringPadProbeInformation.probeId = gst_pad_add_probe(demuxerPad.get(), GST_PAD_PROBE_TYPE_BUFFER, reinterpret_cast<GstPadProbeCallback>(appendPipelinePadProbeDebugInformation), &m_demuxerDataEnteringPadProbeInformation, nullptr);
#endif

    if (hasDemuxer) {
        // These signals won't outlive the lifetime of `this`.
        g_signal_connect(m_demux.get(), "no-more-pads", G_CALLBACK(+[](GstElement*, AppendPipeline* appendPipeline) {
            ASSERT(!isMainThread());
            GST_DEBUG("Posting no-more-pads task to main thread");
            appendPipeline->m_taskQueue.enqueueTaskAndWait<AbortableTaskQueue::Void>([appendPipeline]() {
                appendPipeline->didReceiveInitializationSegment();
                return AbortableTaskQueue::Void();
            });
        }), this);
    } else {
        GRefPtr<GstPad> identitySrcPad = adoptGRef(gst_element_get_static_pad(m_demux.get(), "src"));
        gst_pad_add_probe(identitySrcPad.get(), GST_PAD_PROBE_TYPE_BUFFER, reinterpret_cast<GstPadProbeCallback>(
            +[](GstPad *pad, GstPadProbeInfo*, AppendPipeline* appendPipeline) {
                GRefPtr<GstCaps> caps = gst_pad_get_current_caps(pad);
                if (!caps)
                    return GST_PAD_PROBE_DROP;
                appendPipeline->m_taskQueue.enqueueTaskAndWait<AbortableTaskQueue::Void>([appendPipeline]() {
                    appendPipeline->didReceiveInitializationSegment();
                    return AbortableTaskQueue::Void();
                });
                return GST_PAD_PROBE_REMOVE;
            }
        ), this, nullptr);
    }

    // Add_many will take ownership of a reference. That's why we used an assignment before.
    gst_bin_add_many(GST_BIN(m_pipeline.get()), m_appsrc.get(), m_typefind.get(), m_demux.get(), nullptr);
    gst_element_link_many(m_appsrc.get(), m_typefind.get(), m_demux.get(), nullptr);

    assertedElementSetState(m_pipeline.get(), GST_STATE_PLAYING);
}

AppendPipeline::~AppendPipeline()
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Destructing AppendPipeline (%p)", this);
    ASSERT(isMainThread());

    // Forget all pending tasks and unblock the streaming thread if it was blocked.
    m_taskQueue.startAborting();

    // Disconnect all synchronous event handlers and probes susceptible of firing from the main thread
    // when changing the pipeline state.

    if (m_pipeline) {
        ASSERT(m_bus);
        g_signal_handlers_disconnect_by_data(m_bus.get(), this);
        gst_bus_disable_sync_message_emission(m_bus.get());
        gst_bus_remove_signal_watch(m_bus.get());
    }

    if (m_demux) {
#if !LOG_DISABLED
        GRefPtr<GstPad> demuxerPad = adoptGRef(gst_element_get_static_pad(m_demux.get(), "sink"));
        gst_pad_remove_probe(demuxerPad.get(), m_demuxerDataEnteringPadProbeInformation.probeId);
#endif

        g_signal_handlers_disconnect_by_data(m_demux.get(), this);
    }

    for (std::unique_ptr<Track>& track : m_tracks) {
        GRefPtr<GstPad> appsinkPad = adoptGRef(gst_element_get_static_pad(track->appsink.get(), "sink"));
        g_signal_handlers_disconnect_by_data(appsinkPad.get(), this);
        g_signal_handlers_disconnect_by_data(track->appsink.get(), this);
#if !LOG_DISABLED
        gst_pad_remove_probe(appsinkPad.get(), track->appsinkDataEnteringPadProbeInformation.probeId);
#endif
#if ENABLE(ENCRYPTED_MEDIA)
        gst_pad_remove_probe(appsinkPad.get(), track->appsinkPadEventProbeInformation.probeId);
#endif
    }

    // We can tear down the pipeline safely now.
    if (m_pipeline)
        gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
}

void AppendPipeline::handleErrorConditionFromStreamingThread()
{
    ASSERT(!isMainThread());
    // Notify the main thread that the append has a decode error.
    auto response = m_taskQueue.enqueueTaskAndWait<AbortableTaskQueue::Void>([this]() {
        m_errorReceived = true;
        // appendParsingFailed() will cause resetParserState() to be called.
        m_sourceBufferPrivate.appendParsingFailed();
        return AbortableTaskQueue::Void();
    });
#ifdef NDEBUG
    UNUSED_VARIABLE(response);
#endif
    // The streaming thread has now been unblocked because we are aborting in the main thread.
    ASSERT(!response);
}

void AppendPipeline::handleErrorSyncMessage(GstMessage* message)
{
    ASSERT(!isMainThread());
    GST_WARNING_OBJECT(m_pipeline.get(), "Demuxing error: %" GST_PTR_FORMAT, message);
    handleErrorConditionFromStreamingThread();
}

GstPadProbeReturn AppendPipeline::appsrcEndOfAppendCheckerProbe(GstPadProbeInfo* padProbeInfo)
{
    ASSERT(!isMainThread());
    m_streamingThread = &Thread::current();

    GstBuffer* buffer = GST_BUFFER(padProbeInfo->data);
    ASSERT(GST_IS_BUFFER(buffer));

    GST_TRACE_OBJECT(m_pipeline.get(), "Buffer entered appsrcEndOfAppendCheckerProbe: %" GST_PTR_FORMAT, buffer);

    EndOfAppendMeta* endOfAppendMeta = reinterpret_cast<EndOfAppendMeta*>(gst_buffer_get_meta(buffer, s_endOfAppendMetaType));
    if (!endOfAppendMeta) {
        // Normal buffer, nothing to do.
        return GST_PAD_PROBE_OK;
    }

    GST_TRACE_OBJECT(m_pipeline.get(), "Posting end-of-append task to the main thread");
    m_taskQueue.enqueueTask([this]() {
        handleEndOfAppend();
    });
    return GST_PAD_PROBE_DROP;
}

void AppendPipeline::handleNeedContextSyncMessage(GstMessage* message)
{
    // MediaPlayerPrivateGStreamerBase will take care of setting up encryption.
    m_playerPrivate->handleNeedContextMessage(message);
}

void AppendPipeline::handleStateChangeMessage(GstMessage* message)
{
    ASSERT(isMainThread());

    if (GST_MESSAGE_SRC(message) == reinterpret_cast<GstObject*>(m_pipeline.get())) {
        GstState currentState, newState;
        gst_message_parse_state_changed(message, &currentState, &newState, nullptr);
        CString sourceBufferType = String(m_sourceBufferPrivate.type().raw())
            .replace("/", "_").replace(" ", "_")
            .replace("\"", "").replace("\'", "").utf8();
        CString dotFileName = makeString("webkit-append-",
            sourceBufferType.data(), '-',
            gst_element_state_get_name(currentState), '_',
            gst_element_state_get_name(newState)).utf8();
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.data());
    }
}

std::tuple<GRefPtr<GstCaps>, AppendPipeline::StreamType, FloatSize> AppendPipeline::parseDemuxerSrcPadCaps(GstCaps* demuxerSrcPadCaps)
{
    ASSERT(isMainThread());

    GRefPtr<GstCaps> parsedCaps = demuxerSrcPadCaps;
    StreamType streamType = StreamType::Unknown;
    FloatSize presentationSize;

    const char* originalMediaType = capsMediaType(demuxerSrcPadCaps);
    auto& gstRegistryScanner = GStreamerRegistryScannerMSE::singleton();
    if (!gstRegistryScanner.isCodecSupported(GStreamerRegistryScanner::Configuration::Decoding, originalMediaType)) {
        streamType = StreamType::Invalid;
    } else if (doCapsHaveType(demuxerSrcPadCaps, GST_VIDEO_CAPS_TYPE_PREFIX)) {
        presentationSize = getVideoResolutionFromCaps(demuxerSrcPadCaps).value_or(FloatSize());
        streamType = StreamType::Video;
    } else {
        if (doCapsHaveType(demuxerSrcPadCaps, GST_AUDIO_CAPS_TYPE_PREFIX))
            streamType = StreamType::Audio;
        else if (doCapsHaveType(demuxerSrcPadCaps, GST_TEXT_CAPS_TYPE_PREFIX))
            streamType = StreamType::Text;
    }

    return { WTFMove(parsedCaps), streamType, WTFMove(presentationSize) };
}

void AppendPipeline::appsinkCapsChanged(Track& track)
{
    ASSERT(isMainThread());

    // Consume any pending samples with the previous caps.
    consumeAppsinksAvailableSamples();

    GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(track.appsink.get(), "sink"));
    GRefPtr<GstCaps> caps = adoptGRef(gst_pad_get_current_caps(pad.get()));

    if (!caps)
        return;

    // If this is not the first time we're parsing an initialization segment, fail if the track
    // has a different codec or type (e.g. if we were previously demuxing an audio stream and
    // someone appends a video stream).
    if (track.caps && g_strcmp0(capsMediaType(caps.get()), capsMediaType(track.caps.get()))) {
        GST_WARNING_OBJECT(m_pipeline.get(), "Track received incompatible caps, received '%s' for a track previously handling '%s'. Erroring out.", capsMediaType(caps.get()), capsMediaType(track.caps.get()));
        m_sourceBufferPrivate.appendParsingFailed();
        return;
    }

    if (doCapsHaveType(caps.get(), GST_VIDEO_CAPS_TYPE_PREFIX)) {
        if (auto size = getVideoResolutionFromCaps(caps.get()))
            track.presentationSize = *size;
    }

    if (track.caps != caps)
        track.caps = WTFMove(caps);
}

void AppendPipeline::handleEndOfAppend()
{
    ASSERT(isMainThread());
    consumeAppsinksAvailableSamples();
    GST_TRACE_OBJECT(m_pipeline.get(), "Notifying SourceBufferPrivate the append is complete");
    sourceBufferPrivate().didReceiveAllPendingSamples();
}

static MediaTime bufferTimeToStreamTime(const GstSegment* segment, GstClockTime bufferTime)
{
    if (bufferTime == GST_CLOCK_TIME_NONE)
        return MediaTime::invalidTime();

    guint64 streamTime;
    int sign = gst_segment_to_stream_time_full(segment, GST_FORMAT_TIME, bufferTime, &streamTime);
    if (!sign) {
        GST_ERROR("Couldn't map buffer time %" GST_TIME_FORMAT " to segment %" GST_PTR_FORMAT, GST_TIME_ARGS(bufferTime), segment);
        return MediaTime::invalidTime();
    }
    return MediaTime(sign * streamTime, GST_SECOND);
}

void AppendPipeline::appsinkNewSample(const Track& track, GRefPtr<GstSample>&& sample)
{
    ASSERT(isMainThread());

    GstBuffer* buffer = gst_sample_get_buffer(sample.get());
    if (UNLIKELY(!buffer)) {
        GST_WARNING("Received sample without buffer from appsink.");
        return;
    }

    if (!GST_BUFFER_PTS_IS_VALID(buffer)) {
        // When demuxing Vorbis, matroskademux creates several PTS-less frames with header information. We don't need those.
        GST_DEBUG("Ignoring sample without PTS: %" GST_PTR_FORMAT, gst_sample_get_buffer(sample.get()));
        return;
    }

    GstSegment* segment = gst_sample_get_segment(sample.get());
    auto mediaSample = MediaSampleGStreamer::create(WTFMove(sample), track.presentationSize, track.trackId);

    if (segment && (segment->time || segment->start)) {
        // MP4 has the concept of edit lists, where some buffer time needs to be offsetted, often very slightly,
        // to get exact timestamps.
        MediaTime pts = bufferTimeToStreamTime(segment, GST_BUFFER_PTS(buffer));
        MediaTime dts = bufferTimeToStreamTime(segment, GST_BUFFER_DTS(buffer));
        GST_TRACE_OBJECT(track.appsinkPad.get(), "Mapped buffer to segment, PTS %" GST_TIME_FORMAT " -> %s DTS %" GST_TIME_FORMAT " -> %s",
            GST_TIME_ARGS(GST_BUFFER_PTS(buffer)), pts.toString().utf8().data(), GST_TIME_ARGS(GST_BUFFER_DTS(buffer)), dts.toString().utf8().data());
        mediaSample->setTimestamps(pts, dts);
    } else if (!GST_BUFFER_DTS(buffer) && GST_BUFFER_PTS(buffer) > 0 && GST_BUFFER_PTS(buffer) <= 100'000'000) {
        // Because a track presentation time starting at some close to zero, but not exactly zero time can cause unexpected
        // results for applications, we extend the duration of this first sample to the left so that it starts at zero.
        // This is relevant for files that should have an edit list but don't, or when using GStreamer < 1.16, where
        // edit lists are not parsed in push-mode.

        GST_DEBUG("Extending first sample of track '%s' to make it start at PTS=0 %" GST_PTR_FORMAT, track.trackId.string().utf8().data(), buffer);
        mediaSample->extendToTheBeginning();
    }

    GST_TRACE("append: trackId=%s PTS=%s DTS=%s DUR=%s presentationSize=%.0fx%.0f",
        mediaSample->trackID().string().utf8().data(),
        mediaSample->presentationTime().toString().utf8().data(),
        mediaSample->decodeTime().toString().utf8().data(),
        mediaSample->duration().toString().utf8().data(),
        mediaSample->presentationSize().width(), mediaSample->presentationSize().height());

    m_sourceBufferPrivate.didReceiveSample(mediaSample.get());
}

void AppendPipeline::didReceiveInitializationSegment()
{
    ASSERT(isMainThread());

    bool isFirstInitializationSegment = !m_hasReceivedFirstInitializationSegment;

    SourceBufferPrivateClient::InitializationSegment initializationSegment;

    gint64 timeLength = 0;
    if (gst_element_query_duration(m_demux.get(), GST_FORMAT_TIME, &timeLength)
        && static_cast<guint64>(timeLength) != GST_CLOCK_TIME_NONE)
        initializationSegment.duration = MediaTime(GST_TIME_AS_USECONDS(timeLength), G_USEC_PER_SEC);
    else
        initializationSegment.duration = MediaTime::positiveInfiniteTime();

    if (isFirstInitializationSegment) {
        // Create a Track object per pad.
        int trackIndex = 0;
        for (GstPad* pad : GstIteratorAdaptor<GstPad>(GUniquePtr<GstIterator>(gst_element_iterate_src_pads(m_demux.get())))) {
            auto [createTrackResult, track] = tryCreateTrackFromPad(pad, trackIndex);
            if (createTrackResult == CreateTrackResult::AppendParsingFailed) {
                // appendParsingFailed() will immediately cause a resetParserState() which will stop demuxing, then the
                // AppendPipeline will be destroyed.
                m_sourceBufferPrivate.appendParsingFailed();
                return;
            }
            if (track)
                linkPadWithTrack(pad, *track);
            trackIndex++;
        }
    } else {
        // Link pads to existing Track objects that don't have a linked pad yet.
        unsigned countPads = 0;
        for (GstPad* pad : GstIteratorAdaptor<GstPad>(GUniquePtr<GstIterator>(gst_element_iterate_src_pads(m_demux.get())))) {
            countPads++;
            Track* track = tryMatchPadToExistingTrack(pad);
            if (!track) {
                GST_WARNING_OBJECT(pipeline(), "Can't match pad to existing tracks in the AppendPipeline: %" GST_PTR_FORMAT, pad);
                m_sourceBufferPrivate.appendParsingFailed();
                return;
            }
            linkPadWithTrack(pad, *track);
        }
        if (countPads != m_tracks.size()) {
            GST_WARNING_OBJECT(pipeline(), "Number of pads (%u) doesn't match number of tracks (%zu).", countPads, m_tracks.size());
            m_sourceBufferPrivate.appendParsingFailed();
            return;
        }
    }

    for (std::unique_ptr<Track>& track : m_tracks) {
        GST_DEBUG_OBJECT(pipeline(), "Adding track to initialization with segment type %s, id %s.", streamTypeToString(track->streamType), track->trackId.string().utf8().data());
        switch (track->streamType) {
        case Audio: {
            ASSERT(track->webKitTrack);
            SourceBufferPrivateClient::InitializationSegment::AudioTrackInformation info;
            info.track = static_cast<AudioTrackPrivateGStreamer*>(track->webKitTrack.get());
            info.description = GStreamerMediaDescription::create(track->caps.get());
            initializationSegment.audioTracks.append(info);
            break;
        }
        case Video: {
            ASSERT(track->webKitTrack);
            SourceBufferPrivateClient::InitializationSegment::VideoTrackInformation info;
            info.track = static_cast<VideoTrackPrivateGStreamer*>(track->webKitTrack.get());
            info.description = GStreamerMediaDescription::create(track->caps.get());
            initializationSegment.videoTracks.append(info);
            break;
        }
        default:
            GST_ERROR("Unsupported stream type or codec");
            break;
        }
    }

    if (isFirstInitializationSegment) {
        for (std::unique_ptr<Track>& track : m_tracks) {
            if (track->streamType == StreamType::Video) {
                GST_DEBUG_OBJECT(pipeline(), "Setting initial video size to that of track with id '%s', %gx%g.",
                    track->trackId.string().utf8().data(), static_cast<double>(track->presentationSize.width()), static_cast<double>(track->presentationSize.height()));
                m_playerPrivate->setInitialVideoSize(track->presentationSize);
                break;
            }
        }
    }

    m_hasReceivedFirstInitializationSegment = true;
    GST_DEBUG("Notifying SourceBuffer of initialization segment.");
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, "append-pipeline-received-init-segment");
    m_sourceBufferPrivate.didReceiveInitializationSegment(WTFMove(initializationSegment), []() { });
}

void AppendPipeline::consumeAppsinksAvailableSamples()
{
    ASSERT(isMainThread());

    GRefPtr<GstSample> sample;
    int batchedSampleCount = 0;
    // In some cases each frame increases the duration of the movie.
    // Batch duration changes so that if we pick 100 of such samples we don't have to run 100 times
    // layout for the video controls, but only once.
    m_playerPrivate->blockDurationChanges();
    for (std::unique_ptr<Track>& track : m_tracks) {
        while ((sample = adoptGRef(gst_app_sink_try_pull_sample(GST_APP_SINK(track->appsink.get()), 0)))) {
            appsinkNewSample(*track, WTFMove(sample));
            batchedSampleCount++;
        }
    }
    m_playerPrivate->unblockDurationChanges();

    GST_TRACE_OBJECT(m_pipeline.get(), "batchedSampleCount = %d", batchedSampleCount);
}

void AppendPipeline::resetParserState()
{
    ASSERT(isMainThread());
    GST_DEBUG_OBJECT(m_pipeline.get(), "Handling resetParserState() in AppendPipeline by resetting the pipeline");

    // FIXME: Implement a flush event-based resetParserState() implementation would allow the initialization segment to
    // survive, in accordance with the spec.

    // This function restores the GStreamer pipeline to the same state it was when the AppendPipeline constructor
    // finished. All previously enqueued data is lost and the demuxer is reset, losing all pads and track data.

    // Unlock the streaming thread.
    m_taskQueue.startAborting();

    // Reset the state of all elements in the pipeline.
    assertedElementSetState(m_pipeline.get(), GST_STATE_READY);

    // Set the pipeline to PLAYING so that it can be used again.
    assertedElementSetState(m_pipeline.get(), GST_STATE_PLAYING);

    // All processing related to the previous append has been aborted and the pipeline is idle.
    // We can listen again to new requests coming from the streaming thread.
    m_taskQueue.finishAborting();

#if (!(LOG_DISABLED || defined(GST_DISABLE_GST_DEBUG)))
    {
        static unsigned i = 0;
        // This is here for debugging purposes. It does not make sense to have it as class member.
        String dotFileName = makeString("reset-pipeline-", ++i);
        gst_debug_bin_to_dot_file(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.utf8().data());
    }
#endif
}

void AppendPipeline::pushNewBuffer(GRefPtr<GstBuffer>&& buffer)
{
    GST_TRACE_OBJECT(m_pipeline.get(), "pushing data buffer %" GST_PTR_FORMAT, buffer.get());
    GstFlowReturn pushDataBufferRet = gst_app_src_push_buffer(GST_APP_SRC(m_appsrc.get()), buffer.leakRef());
    // Pushing buffers to appsrc can only fail if the appsrc is flushing, in EOS or stopped. Neither of these should
    // be true at this point.
    if (pushDataBufferRet != GST_FLOW_OK) {
        GST_ERROR_OBJECT(m_pipeline.get(), "Failed to push data buffer into appsrc.");
        ASSERT_NOT_REACHED();
    }

    // Push an additional empty buffer that marks the end of the append.
    // This buffer is detected and consumed by appsrcEndOfAppendCheckerProbe(), which uses it to signal the successful
    // completion of the append.
    //
    // This works based on how push mode scheduling works in GStreamer. Note there is a single streaming thread for the
    // AppendPipeline, and within a stream (the portion of a pipeline covered by the same streaming thread, in this case
    // the whole pipeline) a buffer is guaranteed not to be processed by downstream until processing of the previous
    // buffer has completed.

    GstBuffer* endOfAppendBuffer = gst_buffer_new();
    gst_buffer_add_meta(endOfAppendBuffer, s_webKitEndOfAppendMetaInfo, nullptr);

    GST_TRACE_OBJECT(m_pipeline.get(), "pushing end-of-append buffer %" GST_PTR_FORMAT, endOfAppendBuffer);
    GstFlowReturn pushEndOfAppendBufferRet = gst_app_src_push_buffer(GST_APP_SRC(m_appsrc.get()), endOfAppendBuffer);
    if (pushEndOfAppendBufferRet != GST_FLOW_OK) {
        GST_ERROR_OBJECT(m_pipeline.get(), "Failed to push end-of-append buffer into appsrc.");
        ASSERT_NOT_REACHED();
    }
}

void AppendPipeline::handleAppsinkNewSampleFromStreamingThread(GstElement*)
{
    ASSERT(!isMainThread());
    if (&Thread::current() != m_streamingThread) {
        // m_streamingThreadId has been initialized in appsrcEndOfAppendCheckerProbe().
        // For a buffer to reach the appsink, a buffer must have passed through appsrcEndOfAppendCheckerProbe() first.
        // This error will only raise if someone modifies the pipeline to include more than one streaming thread or
        // removes the appsrcEndOfAppendCheckerProbe(). Either way, the end-of-append detection would be broken.
        // AppendPipeline should have only one streaming thread. Otherwise we can't detect reliably when an appends has
        // been demuxed completely.;
        GST_ERROR_OBJECT(m_pipeline.get(), "Appsink received a sample in a different thread than appsrcEndOfAppendCheckerProbe run.");
        ASSERT_NOT_REACHED();
    }

    if (!m_wasBusAlreadyNotifiedOfAvailableSamples.test_and_set()) {
        GST_TRACE("Posting appsink-new-sample task to the main thread");
        m_taskQueue.enqueueTask([this]() {
            m_wasBusAlreadyNotifiedOfAvailableSamples.clear();
            consumeAppsinksAvailableSamples();
        });
    }
}

static GRefPtr<GstElement>
createOptionalParserForFormat(const AtomString& trackId, const GstCaps* caps)
{
    GstStructure* structure = gst_caps_get_structure(caps, 0);
    const char* mediaType = gst_structure_get_name(structure);
    GUniquePtr<char> parserName(g_strdup_printf("%s_parser", trackId.string().utf8().data()));
    const gchar* elementClass = "identity";

    if (!g_strcmp0(mediaType, "audio/x-opus"))
        elementClass = "opusparse";
    else if (!g_strcmp0(mediaType, "video/x-h264"))
        elementClass = "h264parse";
    else if (!g_strcmp0(mediaType, "audio/mpeg")) {
        int mpegversion = 0;
        gst_structure_get_int(structure, "mpegversion", &mpegversion);
        switch (mpegversion) {
        case 1:
            elementClass = "mpegaudioparse";
            break;
        case 2:
        case 4:
            elementClass = "aacparse";
            break;
        default: {
            GUniquePtr<char> capsString(gst_caps_to_string(caps));
            GST_WARNING("Unsupported audio mpeg caps: %s", capsString.get());
        }
        }
    }
    return GRefPtr<GstElement>(makeGStreamerElement(elementClass, parserName.get()));
}

AtomString AppendPipeline::generateTrackId(StreamType streamType, int padIndex)
{
    switch (streamType) {
    case Audio:
        return makeString("A", padIndex);
    case Video:
        return makeString("V", padIndex);
    case Text:
        return makeString("T", padIndex);
    default:
        return makeString("O", padIndex);
    }
}

std::pair<AppendPipeline::CreateTrackResult, AppendPipeline::Track*> AppendPipeline::tryCreateTrackFromPad(GstPad* demuxerSrcPad, int trackIndex)
{
    ASSERT(isMainThread());
    ASSERT(!m_hasReceivedFirstInitializationSegment);
    GST_DEBUG_OBJECT(pipeline(), "Creating Track object for pad %" GST_PTR_FORMAT, demuxerSrcPad);

    const String& type = m_sourceBufferPrivate.type().containerType();
    if (type.endsWith("webm"))
        gst_pad_add_probe(demuxerSrcPad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, matroskademuxForceSegmentStartToEqualZero, nullptr, nullptr);

    auto [parsedCaps, streamType, presentationSize] = parseDemuxerSrcPadCaps(adoptGRef(gst_pad_get_current_caps(demuxerSrcPad)).get());
#ifndef GST_DISABLE_GST_DEBUG
    {
        GUniquePtr<gchar> strcaps(gst_caps_to_string(parsedCaps.get()));
        GST_DEBUG("%s", strcaps.get());
    }
#endif

    if (streamType == StreamType::Invalid) {
        GST_WARNING_OBJECT(m_pipeline.get(), "Unsupported track codec: %" GST_PTR_FORMAT, parsedCaps.get());
        // 3.5.7 Initialization Segment Received
        // 5.1. If the initialization segment contains tracks with codecs the user agent does not support, then run the
        // append error algorithm and abort these steps.
        return { CreateTrackResult::AppendParsingFailed, nullptr };
    }
    if (streamType == StreamType::Unknown) {
        GST_WARNING_OBJECT(pipeline(), "Pad '%s' with parsed caps %" GST_PTR_FORMAT " has an unknown type, will be connected to a black hole probe.", GST_PAD_NAME(demuxerSrcPad), parsedCaps.get());
        gst_pad_add_probe(demuxerSrcPad, GST_PAD_PROBE_TYPE_BUFFER, reinterpret_cast<GstPadProbeCallback>(appendPipelineDemuxerBlackHolePadProbe), nullptr, nullptr);
        return { CreateTrackResult::TrackIgnored, nullptr };
    }
    AtomString trackId = generateTrackId(streamType, trackIndex);

    GST_DEBUG_OBJECT(pipeline(), "Creating new AppendPipeline::Track with id '%s'", trackId.string().utf8().data());
    size_t newTrackIndex = m_tracks.size();
    m_tracks.append(makeUnique<Track>(trackId, streamType, parsedCaps, presentationSize));
    Track& track = *m_tracks.at(newTrackIndex);
    track.initializeElements(this, GST_BIN(m_pipeline.get()));
    track.webKitTrack = makeWebKitTrack(newTrackIndex);
    hookTrackEvents(track);
    return { CreateTrackResult::TrackCreated, &track };
}

AppendPipeline::Track* AppendPipeline::tryMatchPadToExistingTrack(GstPad *demuxerSrcPad)
{
    ASSERT(isMainThread());
    ASSERT(m_hasReceivedFirstInitializationSegment);
    AtomString trackId = GST_PAD_NAME(demuxerSrcPad);
    auto [parsedCaps, streamType, presentationSize] = parseDemuxerSrcPadCaps(adoptGRef(gst_pad_get_current_caps(demuxerSrcPad)).get());

    // Try to find a matching pre-existing track. Ideally, tracks should be matched by track ID, but matching by type
    // is provided as a fallback -- which will be used, since we don't have a way to fetch those from GStreamer at the moment.
    Track* matchingTrack = nullptr;
    for (std::unique_ptr<Track>& track : m_tracks) {
        if (track->streamType != streamType || gst_pad_is_linked(track->entryPad.get()))
            continue;
        matchingTrack = &*track;
        if (track->trackId == trackId)
            break;
    }

    if (!matchingTrack) {
        // Invalid configuration.
        GST_WARNING_OBJECT(pipeline(), "Couldn't find a matching pre-existing track for pad '%s' with parsed caps %" GST_PTR_FORMAT
            " on non-first initialization segment, will be connected to a black hole probe.", GST_PAD_NAME(demuxerSrcPad), parsedCaps.get());
        gst_pad_add_probe(demuxerSrcPad, GST_PAD_PROBE_TYPE_BUFFER, reinterpret_cast<GstPadProbeCallback>(appendPipelineDemuxerBlackHolePadProbe), nullptr, nullptr);
    }
    return matchingTrack;
}

void AppendPipeline::linkPadWithTrack(GstPad* demuxerSrcPad, Track& track)
{
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, "append-pipeline-before-link");
    ASSERT(!GST_PAD_IS_LINKED(track.entryPad.get()));
    gst_pad_link(demuxerSrcPad, track.entryPad.get());
    ASSERT(GST_PAD_IS_LINKED(track.entryPad.get()));
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, "append-pipeline-after-link");
}

Ref<WebCore::TrackPrivateBase> AppendPipeline::makeWebKitTrack(int trackIndex)
{
    Track& appendPipelineTrack = *m_tracks.at(trackIndex);

    RefPtr<WebCore::TrackPrivateBase> track;
    TrackPrivateBaseGStreamer* gstreamerTrack = nullptr;
    // FIXME: AudioTrackPrivateGStreamer etc. should probably use pads of the playback pipeline rather than the append pipeline.
    GRefPtr<GstPad> pad(appendPipelineTrack.appsinkPad);
    switch (appendPipelineTrack.streamType) {
    case StreamType::Audio: {
        auto specificTrack = AudioTrackPrivateGStreamer::create(m_playerPrivate, trackIndex, WTFMove(pad), false);
        gstreamerTrack = specificTrack.ptr();
        track = static_cast<TrackPrivateBase*>(specificTrack.ptr());
        break;
    }
    case StreamType::Video: {
        auto specificTrack = VideoTrackPrivateGStreamer::create(m_playerPrivate, trackIndex, WTFMove(pad), false);
        gstreamerTrack = specificTrack.ptr();
        track = static_cast<TrackPrivateBase*>(specificTrack.ptr());
        break;
    }
    case StreamType::Text: {
        auto specificTrack = InbandTextTrackPrivateGStreamer::create(trackIndex, WTFMove(pad), false);
        gstreamerTrack = specificTrack.ptr();
        track = static_cast<TrackPrivateBase*>(specificTrack.ptr());
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
    ASSERT(appendPipelineTrack.caps.get());
    gstreamerTrack->setInitialCaps(appendPipelineTrack.caps.get());
    return track.releaseNonNull();
}

void AppendPipeline::Track::initializeElements(AppendPipeline* appendPipeline, GstBin* bin)
{
    appsink = makeGStreamerElement("appsink", nullptr);
    gst_app_sink_set_emit_signals(GST_APP_SINK(appsink.get()), TRUE);
    gst_base_sink_set_sync(GST_BASE_SINK(appsink.get()), FALSE);
    gst_base_sink_set_async_enabled(GST_BASE_SINK(appsink.get()), FALSE); // No prerolls, no async state changes.
    gst_base_sink_set_drop_out_of_segment(GST_BASE_SINK(appsink.get()), FALSE);
    gst_base_sink_set_last_sample_enabled(GST_BASE_SINK(appsink.get()), FALSE);

    gst_bin_add(GST_BIN(appendPipeline->pipeline()), appsink.get());
    gst_element_sync_state_with_parent(appsink.get());
    appsinkPad = adoptGRef(gst_element_get_static_pad(appsink.get(), "sink"));

#if !LOG_DISABLED
    appsinkDataEnteringPadProbeInformation.appendPipeline = appendPipeline;
    appsinkDataEnteringPadProbeInformation.description = "appsink data entering";
    appsinkDataEnteringPadProbeInformation.probeId = gst_pad_add_probe(appsinkPad.get(), GST_PAD_PROBE_TYPE_BUFFER, reinterpret_cast<GstPadProbeCallback>(appendPipelinePadProbeDebugInformation), &appsinkDataEnteringPadProbeInformation, nullptr);
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    appsinkPadEventProbeInformation.appendPipeline = appendPipeline;
    appsinkPadEventProbeInformation.description = "appsink event probe";
    appsinkPadEventProbeInformation.probeId = gst_pad_add_probe(appsinkPad.get(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, reinterpret_cast<GstPadProbeCallback>(appendPipelineAppsinkPadEventProbe), &appsinkPadEventProbeInformation, nullptr);
#endif

    // Some audio files unhelpfully omit the duration of frames in the container. We need to parse
    // the contained audio streams in order to know the duration of the frames.
    // This is known to be an issue with YouTube WebM files containing Opus audio as of YTTV2018.
    // If no parser is needed, a GstIdentity element will be created instead.
    parser = createOptionalParserForFormat(trackId, caps.get());
    gst_bin_add(bin, parser.get());
    gst_element_sync_state_with_parent(parser.get());
    gst_element_link(parser.get(), appsink.get());
    ASSERT(GST_PAD_IS_LINKED(appsinkPad.get()));
    entryPad = adoptGRef(gst_element_get_static_pad(parser.get(), "sink"));
}

void AppendPipeline::hookTrackEvents(Track& track)
{
    g_signal_connect(track.appsink.get(), "new-sample", G_CALLBACK(+[](GstElement* appsink, AppendPipeline* appendPipeline) -> GstFlowReturn {
        appendPipeline->handleAppsinkNewSampleFromStreamingThread(appsink);
        return GST_FLOW_OK;
    }), this);

    struct Closure {
    public:

        Closure(AppendPipeline& appendPipeline, Track& track)
            : appendPipeline(appendPipeline)
            , track(track)
        { }
        static void destruct(void* closure, GClosure*) { delete static_cast<Closure*>(closure); }

        AppendPipeline& appendPipeline;
        Track& track;
    };

    g_signal_connect_data(track.appsinkPad.get(), "notify::caps", G_CALLBACK(+[](GObject*, GParamSpec*, Closure* closure) {
        AppendPipeline& appendPipeline = closure->appendPipeline;
        Track& track = closure->track;
        if (isMainThread()) {
            // When changing the pipeline state down to READY the demuxer is unlinked and this triggers a caps notification
            // because the appsink loses its previously negotiated caps. We are not interested in these unnegotiated caps.
#ifndef NDEBUG
            GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(track.appsink.get(), "sink"));
            GRefPtr<GstCaps> caps = adoptGRef(gst_pad_get_current_caps(pad.get()));
            ASSERT(!caps);
#endif
            return;
        }

        // The streaming thread has just received a new caps and is about to let samples using the
        // new caps flow. Let's block it until the main thread has consumed the samples with the old
        // caps and has processed the caps change.
        appendPipeline.m_taskQueue.enqueueTaskAndWait<AbortableTaskQueue::Void>([&appendPipeline, &track]() {
            appendPipeline.appsinkCapsChanged(track);
            return AbortableTaskQueue::Void();
        });
    }), new Closure { *this, track }, Closure::destruct, static_cast<GConnectFlags>(0));
}

#ifndef GST_DISABLE_GST_DEBUG
const char* AppendPipeline::streamTypeToString(StreamType streamType)
{
    switch (streamType) {
    case StreamType::Audio:
        return "Audio";
    case StreamType::Video:
        return "Video";
    case StreamType::Text:
        return "Text";
    case StreamType::Invalid:
        return "Invalid";
    case StreamType::Unknown:
        return "Unknown";
    default:
        return "(Unsupported stream type)";
    }
}
#endif

#if !LOG_DISABLED
static GstPadProbeReturn appendPipelinePadProbeDebugInformation(GstPad*, GstPadProbeInfo* info, struct PadProbeInformation* padProbeInformation)
{
    ASSERT(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER);
    GstBuffer* buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    GST_TRACE("%s: buffer of size %" G_GSIZE_FORMAT " going thru", padProbeInformation->description, gst_buffer_get_size(buffer));
    return GST_PAD_PROBE_OK;
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
static GstPadProbeReturn appendPipelineAppsinkPadEventProbe(GstPad*, GstPadProbeInfo* info, struct PadProbeInformation *padProbeInformation)
{
    ASSERT(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM);
    GstEvent* event = gst_pad_probe_info_get_event(info);
    GST_DEBUG("Handling event %s on append pipeline appsinkPad", GST_EVENT_TYPE_NAME(event));
    AppendPipeline* appendPipeline = padProbeInformation->appendPipeline;

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_PROTECTION:
        if (appendPipeline && appendPipeline->playerPrivate())
            appendPipeline->playerPrivate()->handleProtectionEvent(event);
        return GST_PAD_PROBE_DROP;
    default:
        break;
    }

    return GST_PAD_PROBE_OK;
}
#endif

static GstPadProbeReturn appendPipelineDemuxerBlackHolePadProbe(GstPad*, GstPadProbeInfo* info, gpointer)
{
    ASSERT(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER);
    GstBuffer* buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    GST_TRACE("buffer of size %" G_GSIZE_FORMAT " ignored", gst_buffer_get_size(buffer));
    return GST_PAD_PROBE_DROP;
}

static GstPadProbeReturn matroskademuxForceSegmentStartToEqualZero(GstPad*, GstPadProbeInfo* info, void*)
{
    // matroskademux sets GstSegment.start to the PTS of the first frame.
    //
    // This way in the unlikely case a user made a .mkv or .webm file where a certain portion of the movie is skipped
    // (e.g. by concatenating a MSE initialization segment with any MSE media segment other than the first) and opened
    // it with a regular player, playback would start immediately. GstSegment.duration is not modified in any case.
    //
    // Leaving the usefulness of that feature aside, the fact that it uses GstSegment.start is problematic for MSE.
    // In MSE is not unusual to process unordered MSE media segments. In this case, a frame may have
    // PTS <<< GstSegment.start and be discarded by downstream. This happens for instance in elements derived from
    // audiobasefilter, such as opusparse.
    //
    // This probe remedies the problem by setting GstSegment.start to 0 in all cases, not only when the PTS of the first
    // frame is zero.

    ASSERT(info->type & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM);
    GstEvent* event = static_cast<GstEvent*>(info->data);
    if (event->type == GST_EVENT_SEGMENT) {
        GstSegment segment;
        gst_event_copy_segment(event, &segment);

        segment.start = 0;

        GRefPtr<GstEvent> newEvent = adoptGRef(gst_event_new_segment(&segment));
        gst_event_replace(reinterpret_cast<GstEvent**>(&info->data), newEvent.get());
    }
    return GST_PAD_PROBE_OK;
}

} // namespace WebCore.

#endif // USE(GSTREAMER)
