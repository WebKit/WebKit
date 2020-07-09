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

#if ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(MEDIA_SOURCE)

#include "AudioTrackPrivateGStreamer.h"
#include "GStreamerCommon.h"
#include "GStreamerEMEUtilities.h"
#include "GStreamerMediaDescription.h"
#include "GStreamerRegistryScannerMSE.h"
#include "MediaSampleGStreamer.h"
#include "InbandTextTrackPrivateGStreamer.h"
#include "MediaDescription.h"
#include "SourceBufferPrivateGStreamer.h"
#include "VideoTrackPrivateGStreamer.h"
#include <functional>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/video/video.h>
#include <wtf/Condition.h>
#include <wtf/glib/GLibUtilities.h>
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

AppendPipeline::AppendPipeline(Ref<MediaSourceClientGStreamerMSE> mediaSourceClient, Ref<SourceBufferPrivateGStreamer> sourceBufferPrivate, MediaPlayerPrivateGStreamerMSE& playerPrivate)
    : m_mediaSourceClient(mediaSourceClient.get())
    , m_sourceBufferPrivate(sourceBufferPrivate.get())
    , m_playerPrivate(&playerPrivate)
    , m_id(0)
    , m_wasBusAlreadyNotifiedOfAvailableSamples(false)
    , m_streamType(Unknown)
{
    ASSERT(isMainThread());
    std::call_once(s_staticInitializationFlag, AppendPipeline::staticInitialization);

    GST_TRACE("Creating AppendPipeline (%p)", this);

    // FIXME: give a name to the pipeline, maybe related with the track it's managing.
    // The track name is still unknown at this time, though.
    static size_t appendPipelineCount = 0;
    String pipelineName = makeString("append-pipeline-",
        m_sourceBufferPrivate->type().containerType().replace("/", "-"), '-', appendPipelineCount++);
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
    m_appsrc = gst_element_factory_make("appsrc", nullptr);

    GRefPtr<GstPad> appsrcPad = adoptGRef(gst_element_get_static_pad(m_appsrc.get(), "src"));
    gst_pad_add_probe(appsrcPad.get(), GST_PAD_PROBE_TYPE_BUFFER, [](GstPad*, GstPadProbeInfo* padProbeInfo, void* userData) {
        return static_cast<AppendPipeline*>(userData)->appsrcEndOfAppendCheckerProbe(padProbeInfo);
    }, this, nullptr);

    const String& type = m_sourceBufferPrivate->type().containerType();
    GST_DEBUG("SourceBuffer containerType: %s", type.utf8().data());
    if (type.endsWith("mp4") || type.endsWith("aac"))
        m_demux = gst_element_factory_make("qtdemux", nullptr);
    else if (type.endsWith("webm"))
        m_demux = gst_element_factory_make("matroskademux", nullptr);
    else
        ASSERT_NOT_REACHED();

    m_appsink = gst_element_factory_make("appsink", nullptr);

    gst_app_sink_set_emit_signals(GST_APP_SINK(m_appsink.get()), TRUE);
    gst_base_sink_set_sync(GST_BASE_SINK(m_appsink.get()), FALSE);
    gst_base_sink_set_async_enabled(GST_BASE_SINK(m_appsink.get()), FALSE); // No prerolls, no async state changes.
    gst_base_sink_set_drop_out_of_segment(GST_BASE_SINK(m_appsink.get()), FALSE);
    gst_base_sink_set_last_sample_enabled(GST_BASE_SINK(m_appsink.get()), FALSE);

    GRefPtr<GstPad> appsinkPad = adoptGRef(gst_element_get_static_pad(m_appsink.get(), "sink"));
    g_signal_connect(appsinkPad.get(), "notify::caps", G_CALLBACK(+[](GObject*, GParamSpec*, AppendPipeline* appendPipeline) {
        if (isMainThread()) {
            // When changing the pipeline state down to READY the demuxer is unlinked and this triggers a caps notification
            // because the appsink loses its previously negotiated caps. We are not interested in these unnegotiated caps.
#ifndef NDEBUG
            GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(appendPipeline->m_appsink.get(), "sink"));
            GRefPtr<GstCaps> caps = adoptGRef(gst_pad_get_current_caps(pad.get()));
            ASSERT(!caps);
#endif
            return;
        }

        // The streaming thread has just received a new caps and is about to let samples using the
        // new caps flow. Let's block it until the main thread has consumed the samples with the old
        // caps and has processed the caps change.
        appendPipeline->m_taskQueue.enqueueTaskAndWait<AbortableTaskQueue::Void>([appendPipeline]() {
            appendPipeline->appsinkCapsChanged();
            return AbortableTaskQueue::Void();
        });
    }), this);

#if !LOG_DISABLED
    GRefPtr<GstPad> demuxerPad = adoptGRef(gst_element_get_static_pad(m_demux.get(), "sink"));
    m_demuxerDataEnteringPadProbeInformation.appendPipeline = this;
    m_demuxerDataEnteringPadProbeInformation.description = "demuxer data entering";
    m_demuxerDataEnteringPadProbeInformation.probeId = gst_pad_add_probe(demuxerPad.get(), GST_PAD_PROBE_TYPE_BUFFER, reinterpret_cast<GstPadProbeCallback>(appendPipelinePadProbeDebugInformation), &m_demuxerDataEnteringPadProbeInformation, nullptr);
    m_appsinkDataEnteringPadProbeInformation.appendPipeline = this;
    m_appsinkDataEnteringPadProbeInformation.description = "appsink data entering";
    m_appsinkDataEnteringPadProbeInformation.probeId = gst_pad_add_probe(appsinkPad.get(), GST_PAD_PROBE_TYPE_BUFFER, reinterpret_cast<GstPadProbeCallback>(appendPipelinePadProbeDebugInformation), &m_appsinkDataEnteringPadProbeInformation, nullptr);
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    m_appsinkPadEventProbeInformation.appendPipeline = this;
    m_appsinkPadEventProbeInformation.description = "appsink event probe";
    m_appsinkPadEventProbeInformation.probeId = gst_pad_add_probe(appsinkPad.get(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, reinterpret_cast<GstPadProbeCallback>(appendPipelineAppsinkPadEventProbe), &m_appsinkPadEventProbeInformation, nullptr);
#endif

    // These signals won't be connected outside of the lifetime of "this".
    g_signal_connect(m_demux.get(), "pad-added", G_CALLBACK(+[](GstElement*, GstPad* demuxerSrcPad, AppendPipeline* appendPipeline) {
        appendPipeline->connectDemuxerSrcPadToAppsinkFromStreamingThread(demuxerSrcPad);
    }), this);
    g_signal_connect(m_demux.get(), "pad-removed", G_CALLBACK(+[](GstElement*, GstPad* demuxerSrcPad, AppendPipeline* appendPipeline) {
        appendPipeline->disconnectDemuxerSrcPadFromAppsinkFromAnyThread(demuxerSrcPad);
    }), this);
    g_signal_connect(m_demux.get(), "no-more-pads", G_CALLBACK(+[](GstElement*, AppendPipeline* appendPipeline) {
        ASSERT(!isMainThread());
        GST_DEBUG("Posting no-more-pads task to main thread");
        appendPipeline->m_taskQueue.enqueueTask([appendPipeline]() {
            appendPipeline->didReceiveInitializationSegment();
        });
    }), this);
    g_signal_connect(m_appsink.get(), "new-sample", G_CALLBACK(+[](GstElement* appsink, AppendPipeline* appendPipeline) -> GstFlowReturn {
        appendPipeline->handleAppsinkNewSampleFromStreamingThread(appsink);
        return GST_FLOW_OK;
    }), this);
    g_signal_connect(m_appsink.get(), "eos", G_CALLBACK(+[](GstElement*, AppendPipeline* appendPipeline) {
        if (appendPipeline->m_errorReceived)
            return;

        GST_ERROR("AppendPipeline's appsink received EOS. This is usually caused by an invalid initialization segment.");
        appendPipeline->handleErrorConditionFromStreamingThread();
    }), this);

    // Add_many will take ownership of a reference. That's why we used an assignment before.
    gst_bin_add_many(GST_BIN(m_pipeline.get()), m_appsrc.get(), m_demux.get(), nullptr);
    gst_element_link(m_appsrc.get(), m_demux.get());

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

    if (m_appsrc)
        g_signal_handlers_disconnect_by_data(m_appsrc.get(), this);

    if (m_demux) {
#if !LOG_DISABLED
        GRefPtr<GstPad> demuxerPad = adoptGRef(gst_element_get_static_pad(m_demux.get(), "sink"));
        gst_pad_remove_probe(demuxerPad.get(), m_demuxerDataEnteringPadProbeInformation.probeId);
#endif

        g_signal_handlers_disconnect_by_data(m_demux.get(), this);
    }

    if (m_appsink) {
        GRefPtr<GstPad> appsinkPad = adoptGRef(gst_element_get_static_pad(m_appsink.get(), "sink"));
        g_signal_handlers_disconnect_by_data(appsinkPad.get(), this);
        g_signal_handlers_disconnect_by_data(m_appsink.get(), this);

#if !LOG_DISABLED
        gst_pad_remove_probe(appsinkPad.get(), m_appsinkDataEnteringPadProbeInformation.probeId);
#endif

#if ENABLE(ENCRYPTED_MEDIA)
        gst_pad_remove_probe(appsinkPad.get(), m_appsinkPadEventProbeInformation.probeId);
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
        m_sourceBufferPrivate->appendParsingFailed();
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
    m_streamingThread = &WTF::Thread::current();

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
    const gchar* contextType = nullptr;
    gst_message_parse_context_type(message, &contextType);
    GST_TRACE("context type: %s", contextType);

    // MediaPlayerPrivateGStreamerBase will take care of setting up encryption.
    m_playerPrivate->handleSyncMessage(message);
}

void AppendPipeline::handleStateChangeMessage(GstMessage* message)
{
    ASSERT(isMainThread());

    if (GST_MESSAGE_SRC(message) == reinterpret_cast<GstObject*>(m_pipeline.get())) {
        GstState currentState, newState;
        gst_message_parse_state_changed(message, &currentState, &newState, nullptr);
        CString sourceBufferType = String(m_sourceBufferPrivate->type().raw())
            .replace("/", "_").replace(" ", "_")
            .replace("\"", "").replace("\'", "").utf8();
        CString dotFileName = makeString("webkit-append-",
            sourceBufferType.data(), '-',
            gst_element_state_get_name(currentState), '_',
            gst_element_state_get_name(newState)).utf8();
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.data());
    }
}

gint AppendPipeline::id()
{
    ASSERT(isMainThread());

    if (m_id)
        return m_id;

    static gint s_totalAudio = 0;
    static gint s_totalVideo = 0;
    static gint s_totalText = 0;

    switch (m_streamType) {
    case Audio:
        m_id = ++s_totalAudio;
        break;
    case Video:
        m_id = ++s_totalVideo;
        break;
    case Text:
        m_id = ++s_totalText;
        break;
    case Unknown:
    case Invalid:
        GST_ERROR("Trying to get id for a pipeline of Unknown/Invalid type");
        ASSERT_NOT_REACHED();
        break;
    }

    GST_DEBUG("streamType=%d, id=%d", static_cast<int>(m_streamType), m_id);

    return m_id;
}

void AppendPipeline::parseDemuxerSrcPadCaps(GstCaps* demuxerSrcPadCaps)
{
    ASSERT(isMainThread());

    m_demuxerSrcPadCaps = adoptGRef(demuxerSrcPadCaps);
    m_streamType = WebCore::MediaSourceStreamTypeGStreamer::Unknown;

    const char* originalMediaType = capsMediaType(m_demuxerSrcPadCaps.get());
    auto& gstRegistryScanner = GStreamerRegistryScannerMSE::singleton();
    if (!gstRegistryScanner.isCodecSupported(originalMediaType)) {
            m_presentationSize = WebCore::FloatSize();
            m_streamType = WebCore::MediaSourceStreamTypeGStreamer::Invalid;
    } else if (doCapsHaveType(m_demuxerSrcPadCaps.get(), GST_VIDEO_CAPS_TYPE_PREFIX)) {
        Optional<FloatSize> size = getVideoResolutionFromCaps(m_demuxerSrcPadCaps.get());
        if (size.hasValue())
            m_presentationSize = size.value();
        else
            m_presentationSize = WebCore::FloatSize();

        m_streamType = WebCore::MediaSourceStreamTypeGStreamer::Video;
    } else {
        m_presentationSize = WebCore::FloatSize();
        if (doCapsHaveType(m_demuxerSrcPadCaps.get(), GST_AUDIO_CAPS_TYPE_PREFIX))
            m_streamType = WebCore::MediaSourceStreamTypeGStreamer::Audio;
        else if (doCapsHaveType(m_demuxerSrcPadCaps.get(), GST_TEXT_CAPS_TYPE_PREFIX))
            m_streamType = WebCore::MediaSourceStreamTypeGStreamer::Text;
    }
}

void AppendPipeline::appsinkCapsChanged()
{
    ASSERT(isMainThread());

    // Consume any pending samples with the previous caps.
    consumeAppsinkAvailableSamples();

    GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(m_appsink.get(), "sink"));
    GRefPtr<GstCaps> caps = adoptGRef(gst_pad_get_current_caps(pad.get()));

    if (!caps)
        return;

    if (doCapsHaveType(caps.get(), GST_VIDEO_CAPS_TYPE_PREFIX)) {
        Optional<FloatSize> size = getVideoResolutionFromCaps(caps.get());
        if (size.hasValue())
            m_presentationSize = size.value();
    }

    // This means that we're right after a new track has appeared. Otherwise, it's a caps change inside the same track.
    bool previousCapsWereNull = !m_appsinkCaps;

    if (m_appsinkCaps != caps) {
        m_appsinkCaps = WTFMove(caps);
        m_playerPrivate->trackDetected(*this, m_track, previousCapsWereNull);
    }
}

void AppendPipeline::handleEndOfAppend()
{
    ASSERT(isMainThread());
    consumeAppsinkAvailableSamples();
    GST_TRACE_OBJECT(m_pipeline.get(), "Notifying SourceBufferPrivate the append is complete");
    sourceBufferPrivate()->didReceiveAllPendingSamples();
}

void AppendPipeline::appsinkNewSample(GRefPtr<GstSample>&& sample)
{
    ASSERT(isMainThread());

    if (UNLIKELY(!gst_sample_get_buffer(sample.get()))) {
        GST_WARNING("Received sample without buffer from appsink.");
        return;
    }

    auto mediaSample = WebCore::MediaSampleGStreamer::create(WTFMove(sample), m_presentationSize, trackId());

    GST_TRACE("append: trackId=%s PTS=%s DTS=%s DUR=%s presentationSize=%.0fx%.0f",
        mediaSample->trackID().string().utf8().data(),
        mediaSample->presentationTime().toString().utf8().data(),
        mediaSample->decodeTime().toString().utf8().data(),
        mediaSample->duration().toString().utf8().data(),
        mediaSample->presentationSize().width(), mediaSample->presentationSize().height());

    // Hack, rework when GStreamer >= 1.16 becomes a requirement:
    // We're not applying edit lists. GStreamer < 1.16 doesn't emit the correct segments to do so.
    // GStreamer fix in https://gitlab.freedesktop.org/gstreamer/gst-plugins-good/-/commit/c2a0da8096009f0f99943f78dc18066965be60f9
    // Also, in order to apply them we would need to convert the timestamps to stream time, which we're not currently
    // doing for consistency between GStreamer versions.
    //
    // In consequence, the timestamps we're handling here are unedited track time. In track time, the first sample is
    // guaranteed to have DTS == 0, but in the case of streams with B-frames, often PTS > 0. Edit lists fix this by
    // offsetting all timestamps by that amount in movie time, but we can't do that if we don't have access to them.
    // (We could assume the track PTS of the sample with track DTS = 0 is the offset, but we don't have any guarantee
    // we will get appended that sample first, or ever).
    //
    // Because a track presentation time starting at some close to zero, but not exactly zero time can cause unexpected
    // results for applications, we extend the duration of this first sample to the left so that it starts at zero.
    if (mediaSample->decodeTime() == MediaTime::zeroTime() && mediaSample->presentationTime() > MediaTime::zeroTime() && mediaSample->presentationTime() <= MediaTime(1, 10)) {
        GST_DEBUG("Extending first sample to make it start at PTS=0");
        mediaSample->extendToTheBeginning();
    }

    m_sourceBufferPrivate->didReceiveSample(mediaSample.get());
}

void AppendPipeline::didReceiveInitializationSegment()
{
    ASSERT(isMainThread());

    WebCore::SourceBufferPrivateClient::InitializationSegment initializationSegment;

    GST_DEBUG("Notifying SourceBuffer for track %s", (m_track) ? m_track->id().string().utf8().data() : nullptr);
    initializationSegment.duration = m_initialDuration;

    switch (m_streamType) {
    case Audio: {
        WebCore::SourceBufferPrivateClient::InitializationSegment::AudioTrackInformation info;
        info.track = static_cast<AudioTrackPrivateGStreamer*>(m_track.get());
        info.description = WebCore::GStreamerMediaDescription::create(m_demuxerSrcPadCaps.get());
        initializationSegment.audioTracks.append(info);
        break;
    }
    case Video: {
        WebCore::SourceBufferPrivateClient::InitializationSegment::VideoTrackInformation info;
        info.track = static_cast<VideoTrackPrivateGStreamer*>(m_track.get());
        info.description = WebCore::GStreamerMediaDescription::create(m_demuxerSrcPadCaps.get());
        initializationSegment.videoTracks.append(info);
        break;
    }
    default:
        GST_ERROR("Unsupported stream type or codec");
        break;
    }

    m_sourceBufferPrivate->didReceiveInitializationSegment(initializationSegment);
}

AtomString AppendPipeline::trackId()
{
    ASSERT(isMainThread());

    if (!m_track)
        return AtomString();

    return m_track->id();
}

void AppendPipeline::consumeAppsinkAvailableSamples()
{
    ASSERT(isMainThread());

    GRefPtr<GstSample> sample;
    int batchedSampleCount = 0;
    // In some cases each frame increases the duration of the movie.
    // Batch duration changes so that if we pick 100 of such samples we don't have to run 100 times
    // layout for the video controls, but only once.
    m_playerPrivate->blockDurationChanges();
    while ((sample = adoptGRef(gst_app_sink_try_pull_sample(GST_APP_SINK(m_appsink.get()), 0)))) {
        appsinkNewSample(WTFMove(sample));
        batchedSampleCount++;
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

    // The parser is tear down automatically when the demuxer is reset (see disconnectDemuxerSrcPadFromAppsinkFromAnyThread()).
    ASSERT(!m_parser);

    // Set the pipeline to PLAYING so that it can be used again.
    assertedElementSetState(m_pipeline.get(), GST_STATE_PLAYING);

    // All processing related to the previous append has been aborted and the pipeline is idle.
    // We can listen again to new requests coming from the streaming thread.
    m_taskQueue.finishAborting();

#if (!(LOG_DISABLED || defined(GST_DISABLE_GST_DEBUG)))
    {
        static unsigned i = 0;
        // This is here for debugging purposes. It does not make sense to have it as class member.
        WTF::String dotFileName = makeString("reset-pipeline-", ++i);
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
    if (&WTF::Thread::current() != m_streamingThread) {
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
            consumeAppsinkAvailableSamples();
        });
    }
}

static GRefPtr<GstElement>
createOptionalParserForFormat(GstPad* demuxerSrcPad)
{
    GRefPtr<GstCaps> padCaps = adoptGRef(gst_pad_get_current_caps(demuxerSrcPad));
    GstStructure* structure = gst_caps_get_structure(padCaps.get(), 0);
    const char* mediaType = gst_structure_get_name(structure);

    GUniquePtr<char> demuxerPadName(gst_pad_get_name(demuxerSrcPad));
    GUniquePtr<char> parserName(g_strdup_printf("%s_parser", demuxerPadName.get()));

    if (!g_strcmp0(mediaType, "audio/x-opus")) {
        GstElement* opusparse = gst_element_factory_make("opusparse", parserName.get());
        ASSERT(opusparse);
        g_return_val_if_fail(opusparse, nullptr);
        return GRefPtr<GstElement>(opusparse);
    }
    if (!g_strcmp0(mediaType, "video/x-h264")) {
        GstElement* h264parse = gst_element_factory_make("h264parse", parserName.get());
        ASSERT(h264parse);
        g_return_val_if_fail(h264parse, nullptr);
        return GRefPtr<GstElement>(h264parse);
    }

    return nullptr;
}

void AppendPipeline::connectDemuxerSrcPadToAppsinkFromStreamingThread(GstPad* demuxerSrcPad)
{
    ASSERT(!isMainThread());

    GST_DEBUG("connecting to appsink");

    if (m_demux->numsrcpads > 1) {
        GST_WARNING("Only one stream per SourceBuffer is allowed! Ignoring stream %d by adding a black hole probe.", m_demux->numsrcpads);
        gulong probeId = gst_pad_add_probe(demuxerSrcPad, GST_PAD_PROBE_TYPE_BUFFER, reinterpret_cast<GstPadProbeCallback>(appendPipelineDemuxerBlackHolePadProbe), nullptr, nullptr);
        g_object_set_data(G_OBJECT(demuxerSrcPad), "blackHoleProbeId", GULONG_TO_POINTER(probeId));
        return;
    }

    GRefPtr<GstPad> appsinkSinkPad = adoptGRef(gst_element_get_static_pad(m_appsink.get(), "sink"));

    // Only one stream per demuxer is supported.
    ASSERT(!gst_pad_is_linked(appsinkSinkPad.get()));

    gint64 timeLength = 0;
    if (gst_element_query_duration(m_demux.get(), GST_FORMAT_TIME, &timeLength)
        && static_cast<guint64>(timeLength) != GST_CLOCK_TIME_NONE)
        m_initialDuration = MediaTime(GST_TIME_AS_USECONDS(timeLength), G_USEC_PER_SEC);
    else
        m_initialDuration = MediaTime::positiveInfiniteTime();

    GST_DEBUG("Requesting demuxer-connect-to-appsink to main thread");
    auto response = m_taskQueue.enqueueTaskAndWait<AbortableTaskQueue::Void>([this, demuxerSrcPad]() {
        connectDemuxerSrcPadToAppsink(demuxerSrcPad);
        return AbortableTaskQueue::Void();
    });
    if (!response) {
        // The AppendPipeline has been destroyed or aborted before we received a response.
        return;
    }

    // Must be done in the thread we were called from (usually streaming thread).
    bool isData = (m_streamType == WebCore::MediaSourceStreamTypeGStreamer::Audio)
        || (m_streamType == WebCore::MediaSourceStreamTypeGStreamer::Video)
        || (m_streamType == WebCore::MediaSourceStreamTypeGStreamer::Text);

    if (isData) {
        GRefPtr<GstObject> parent = adoptGRef(gst_element_get_parent(m_appsink.get()));
        if (!parent)
            gst_bin_add(GST_BIN(m_pipeline.get()), m_appsink.get());

        // Current head of the pipeline being built.
        GRefPtr<GstPad> currentSrcPad = demuxerSrcPad;

        // Some audio files unhelpfully omit the duration of frames in the container. We need to parse
        // the contained audio streams in order to know the duration of the frames.
        // This is known to be an issue with YouTube WebM files containing Opus audio as of YTTV2018.
        m_parser = createOptionalParserForFormat(currentSrcPad.get());
        if (m_parser) {
            gst_bin_add(GST_BIN(m_pipeline.get()), m_parser.get());
            gst_element_sync_state_with_parent(m_parser.get());

            GRefPtr<GstPad> parserSinkPad = adoptGRef(gst_element_get_static_pad(m_parser.get(), "sink"));
            GRefPtr<GstPad> parserSrcPad = adoptGRef(gst_element_get_static_pad(m_parser.get(), "src"));

            gst_pad_link(currentSrcPad.get(), parserSinkPad.get());
            currentSrcPad = parserSrcPad;
        }

        gst_pad_link(currentSrcPad.get(), appsinkSinkPad.get());

        gst_element_sync_state_with_parent(m_appsink.get());

        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, "webkit-after-link");
    }
}

void AppendPipeline::connectDemuxerSrcPadToAppsink(GstPad* demuxerSrcPad)
{
    ASSERT(isMainThread());
    GST_DEBUG("Connecting to appsink");

    const String& type = m_sourceBufferPrivate->type().containerType();
    if (type.endsWith("webm"))
        gst_pad_add_probe(demuxerSrcPad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, matroskademuxForceSegmentStartToEqualZero, nullptr, nullptr);

    GRefPtr<GstPad> sinkSinkPad = adoptGRef(gst_element_get_static_pad(m_appsink.get(), "sink"));

    // Only one stream per demuxer is supported.
    ASSERT(!gst_pad_is_linked(sinkSinkPad.get()));

    GRefPtr<GstCaps> caps = adoptGRef(gst_pad_get_current_caps(GST_PAD(demuxerSrcPad)));

#ifndef GST_DISABLE_GST_DEBUG
    {
        GUniquePtr<gchar> strcaps(gst_caps_to_string(caps.get()));
        GST_DEBUG("%s", strcaps.get());
    }
#endif

    parseDemuxerSrcPadCaps(gst_caps_ref(caps.get()));

    switch (m_streamType) {
    case WebCore::MediaSourceStreamTypeGStreamer::Audio:
        m_track = WebCore::AudioTrackPrivateGStreamer::create(makeWeakPtr(*m_playerPrivate), id(), sinkSinkPad.get());
        break;
    case WebCore::MediaSourceStreamTypeGStreamer::Video:
        m_track = WebCore::VideoTrackPrivateGStreamer::create(makeWeakPtr(*m_playerPrivate), id(), sinkSinkPad.get());
        break;
    case WebCore::MediaSourceStreamTypeGStreamer::Text:
        m_track = WebCore::InbandTextTrackPrivateGStreamer::create(id(), sinkSinkPad.get());
        break;
    case WebCore::MediaSourceStreamTypeGStreamer::Invalid:
        GST_WARNING_OBJECT(m_pipeline.get(), "Unsupported track codec: %" GST_PTR_FORMAT, caps.get());
        // 3.5.7 Initialization Segment Received
        // 5.1. If the initialization segment contains tracks with codecs the user agent does not support, then run the
        // append error algorithm and abort these steps.

        // appendParsingFailed() will immediately cause a resetParserState() which will stop demuxing, then the
        // AppendPipeline will be destroyed.
        m_sourceBufferPrivate->appendParsingFailed();
        return;
    default:
        GST_WARNING_OBJECT(m_pipeline.get(), "Pad has unknown track type, ignoring: %" GST_PTR_FORMAT, caps.get());
        break;
    }

    m_appsinkCaps = WTFMove(caps);
    m_playerPrivate->trackDetected(*this, m_track, true);
}

void AppendPipeline::disconnectDemuxerSrcPadFromAppsinkFromAnyThread(GstPad*)
{
    // Note: This function can be called either from the streaming thread (e.g. if a strange initialization segment with
    // incompatible tracks is appended and the srcpad disconnected) or -- more usually -- from the main thread, when
    // a state change is made to bring the demuxer down. (State change operations run in the main thread.)
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, "pad-removed-before");

    GST_DEBUG("Disconnecting appsink");

    if (m_parser) {
        assertedElementSetState(m_parser.get(), GST_STATE_NULL);
        gst_bin_remove(GST_BIN(m_pipeline.get()), m_parser.get());
        m_parser = nullptr;
    }

    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, "pad-removed-after");
}

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
    WebCore::AppendPipeline* appendPipeline = padProbeInformation->appendPipeline;

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
