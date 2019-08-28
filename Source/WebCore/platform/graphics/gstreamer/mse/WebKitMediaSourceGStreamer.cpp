/*
 *  Copyright (C) 2009, 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *  Copyright (C) 2013 Collabora Ltd.
 *  Copyright (C) 2013 Orange
 *  Copyright (C) 2014, 2015 Sebastian Dröge <sebastian@centricular.com>
 *  Copyright (C) 2015, 2016, 2018, 2019 Metrological Group B.V.
 *  Copyright (C) 2015, 2016, 2018, 2019 Igalia, S.L
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

#include "config.h"
#include "WebKitMediaSourceGStreamer.h"

#if ENABLE(VIDEO) && ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "VideoTrackPrivateGStreamer.h"

#include <gst/gst.h>
#include <wtf/Condition.h>
#include <wtf/DataMutex.h>
#include <wtf/HashMap.h>
#include <wtf/MainThread.h>
#include <wtf/MainThreadData.h>
#include <wtf/RefPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/CString.h>

using namespace WTF;
using namespace WebCore;

GST_DEBUG_CATEGORY_STATIC(webkit_media_src_debug);
#define GST_CAT_DEFAULT webkit_media_src_debug

static GstStaticPadTemplate srcTemplate = GST_STATIC_PAD_TEMPLATE("src_%s", GST_PAD_SRC,
    GST_PAD_SOMETIMES, GST_STATIC_CAPS_ANY);

enum {
    PROP_0,
    PROP_N_AUDIO,
    PROP_N_VIDEO,
    PROP_N_TEXT,
    PROP_LAST
};

struct Stream;

struct WebKitMediaSrcPrivate {
    HashMap<AtomString, RefPtr<Stream>> streams;
    Stream* streamByName(const AtomString& name)
    {
        Stream* stream = streams.get(name);
        ASSERT(stream);
        return stream;
    }

    // Used for stream-start events, shared by all streams.
    const unsigned groupId { gst_util_group_id_next() };

    // Every time a track is added or removed this collection is swapped by an updated one and a STREAM_COLLECTION
    // message is posted in the bus.
    GRefPtr<GstStreamCollection> collection { adoptGRef(gst_stream_collection_new("WebKitMediaSrc")) };

    // Changed on seeks.
    GstClockTime startTime { 0 };
    double rate { 1.0 };

    // Only used by URI Handler API implementation.
    GUniquePtr<char> uri;
};

static void webKitMediaSrcUriHandlerInit(gpointer, gpointer);
static void webKitMediaSrcFinalize(GObject*);
static GstStateChangeReturn webKitMediaSrcChangeState(GstElement*, GstStateChange);
static gboolean webKitMediaSrcActivateMode(GstPad*, GstObject*, GstPadMode, gboolean activate);
static void webKitMediaSrcLoop(void*);
static void webKitMediaSrcStreamFlushStart(const RefPtr<Stream>&);
static void webKitMediaSrcStreamFlushStop(const RefPtr<Stream>&, bool resetTime);
static void webKitMediaSrcGetProperty(GObject*, unsigned propId, GValue*, GParamSpec*);

#define webkit_media_src_parent_class parent_class

struct WebKitMediaSrcPadPrivate {
    RefPtr<Stream> stream;
};

struct WebKitMediaSrcPad {
    GstPad parent;
    WebKitMediaSrcPadPrivate* priv;
};

struct WebKitMediaSrcPadClass {
    GstPadClass parent;
};

namespace WTF {

template<> GRefPtr<WebKitMediaSrcPad> adoptGRef(WebKitMediaSrcPad* ptr)
{
    ASSERT(!ptr || !g_object_is_floating(ptr));
    return GRefPtr<WebKitMediaSrcPad>(ptr, GRefPtrAdopt);
}

template<> WebKitMediaSrcPad* refGPtr<WebKitMediaSrcPad>(WebKitMediaSrcPad* ptr)
{
    if (ptr)
        gst_object_ref_sink(GST_OBJECT(ptr));

    return ptr;
}

template<> void derefGPtr<WebKitMediaSrcPad>(WebKitMediaSrcPad* ptr)
{
    if (ptr)
        gst_object_unref(ptr);
}

} // namespace WTF

static GType webkit_media_src_pad_get_type();
WEBKIT_DEFINE_TYPE(WebKitMediaSrcPad, webkit_media_src_pad, GST_TYPE_PAD);
#define WEBKIT_TYPE_MEDIA_SRC_PAD (webkit_media_src_pad_get_type())
#define WEBKIT_MEDIA_SRC_PAD(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_MEDIA_SRC_PAD, WebKitMediaSrcPad))

static void webkit_media_src_pad_class_init(WebKitMediaSrcPadClass*)
{
}

G_DEFINE_TYPE_WITH_CODE(WebKitMediaSrc, webkit_media_src, GST_TYPE_ELEMENT,
    G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, webKitMediaSrcUriHandlerInit);
    G_ADD_PRIVATE(WebKitMediaSrc);
    GST_DEBUG_CATEGORY_INIT(webkit_media_src_debug, "webkitmediasrc", 0, "WebKit MSE source element"));

struct Stream : public ThreadSafeRefCounted<Stream> {
    Stream(WebKitMediaSrc* source, GRefPtr<GstPad>&& pad, const AtomString& name, WebCore::MediaSourceStreamTypeGStreamer type, GRefPtr<GstCaps>&& initialCaps, GRefPtr<GstStream>&& streamInfo)
        : source(source)
        , pad(WTFMove(pad))
        , name(name)
        , type(type)
        , streamInfo(WTFMove(streamInfo))
        , streamingMembersDataMutex(WTFMove(initialCaps), source->priv->startTime, source->priv->rate, adoptGRef(gst_event_new_stream_collection(source->priv->collection.get())))
    { }

    WebKitMediaSrc* const source;
    GRefPtr<GstPad> const pad;
    AtomString const name;
    WebCore::MediaSourceStreamTypeGStreamer type;
    GRefPtr<GstStream> streamInfo;

    // The point of having a queue in WebKitMediaSource is to limit the number of context switches per second.
    // If we had no queue, the main thread would have to be awaken for every frame. On the other hand, if the
    // queue had unlimited size WebKit would end up requesting flushes more often than necessary when frames
    // in the future are re-appended. As a sweet spot between these extremes we choose to allow enqueueing a
    // few seconds worth of samples.

    // `isReadyForMoreSamples` follows the classical two water levels strategy: initially it's true until the
    // high water level is reached, then it becomes false until the queue drains down to the low water level
    // and the cycle repeats. This way we avoid stalls and minimize context switches.

    static const uint64_t durationEnqueuedHighWaterLevel = 5 * GST_SECOND;
    static const uint64_t durationEnqueuedLowWaterLevel = 2 * GST_SECOND;

    struct StreamingMembers {
        StreamingMembers(GRefPtr<GstCaps>&& initialCaps, GstClockTime startTime, double rate, GRefPtr<GstEvent>&& pendingStreamCollectionEvent)
            : pendingStreamCollectionEvent(WTFMove(pendingStreamCollectionEvent))
            , pendingInitialCaps(WTFMove(initialCaps))
        {
            gst_segment_init(&segment, GST_FORMAT_TIME);
            segment.start = segment.time = startTime;
            segment.rate = rate;

            GstStreamCollection* collection;
            gst_event_parse_stream_collection(this->pendingStreamCollectionEvent.get(), &collection);
            ASSERT(collection);
        }

        bool hasPushedFirstBuffer { false };
        bool wasStreamStartSent { false };
        bool doesNeedSegmentEvent { true };
        GstSegment segment;
        GRefPtr<GstEvent> pendingStreamCollectionEvent;
        GRefPtr<GstCaps> pendingInitialCaps;
        GRefPtr<GstCaps> previousCaps;

        Condition padLinkedOrFlushedCondition;
        Condition queueChangedOrFlushedCondition;
        Deque<GRefPtr<GstMiniObject>> queue;
        bool isFlushing { false };
        bool doesNeedToNotifyOnLowWaterLevel { false };

        uint64_t durationEnqueued() const
        {
            // Find the first and last GstSample in the queue and subtract their DTS.
            auto frontIter = std::find_if(queue.begin(), queue.end(), [](const GRefPtr<GstMiniObject>& object) {
                return GST_IS_SAMPLE(object.get());
            });

            // If there are no samples in the queue, that makes total duration of enqueued frames of zero.
            if (frontIter == queue.end())
                return 0;

            auto backIter = std::find_if(queue.rbegin(), queue.rend(), [](const GRefPtr<GstMiniObject>& object) {
                return GST_IS_SAMPLE(object.get());
            });

            const GstBuffer* front = gst_sample_get_buffer(GST_SAMPLE(frontIter->get()));
            const GstBuffer* back = gst_sample_get_buffer(GST_SAMPLE(backIter->get()));
            return GST_BUFFER_DTS_OR_PTS(back) - GST_BUFFER_DTS_OR_PTS(front);
        }
    };
    DataMutex<StreamingMembers> streamingMembersDataMutex;

    struct ReportedStatus {
        // Set to true when the pad is removed. In the case where a reference to the Stream object is alive because of
        // a posted task to notify isReadyForMoreSamples, the notification must not be delivered if this flag is true.
        bool wasRemoved { false };

        bool isReadyForMoreSamples { true };
        SourceBufferPrivateClient* sourceBufferPrivateToNotify { nullptr };
    };
    MainThreadData<ReportedStatus> reportedStatus;
};

static GRefPtr<GstElement> findPipeline(GRefPtr<GstElement> element)
{
    while (true) {
        GRefPtr<GstElement> parentElement = adoptGRef(GST_ELEMENT(gst_element_get_parent(element.get())));
        if (!parentElement)
            return element;
        element = parentElement;
    }
}

static void webkit_media_src_class_init(WebKitMediaSrcClass* klass)
{
    GObjectClass* oklass = G_OBJECT_CLASS(klass);
    GstElementClass* eklass = GST_ELEMENT_CLASS(klass);

    oklass->finalize = webKitMediaSrcFinalize;
    oklass->get_property = webKitMediaSrcGetProperty;

    gst_element_class_add_static_pad_template_with_gtype(eklass, &srcTemplate, webkit_media_src_pad_get_type());

    gst_element_class_set_static_metadata(eklass, "WebKit MediaSource source element", "Source/Network", "Feeds samples coming from WebKit MediaSource object", "Igalia <aboya@igalia.com>");

    eklass->change_state = webKitMediaSrcChangeState;

    g_object_class_install_property(oklass,
        PROP_N_AUDIO,
        g_param_spec_int("n-audio", "Number Audio", "Total number of audio streams",
        0, G_MAXINT, 0, GParamFlags(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(oklass,
        PROP_N_VIDEO,
        g_param_spec_int("n-video", "Number Video", "Total number of video streams",
        0, G_MAXINT, 0, GParamFlags(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(oklass,
        PROP_N_TEXT,
        g_param_spec_int("n-text", "Number Text", "Total number of text streams",
        0, G_MAXINT, 0, GParamFlags(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
}

static void webkit_media_src_init(WebKitMediaSrc* source)
{
    ASSERT(isMainThread());

    GST_OBJECT_FLAG_SET(source, GST_ELEMENT_FLAG_SOURCE);
    source->priv = G_TYPE_INSTANCE_GET_PRIVATE((source), WEBKIT_TYPE_MEDIA_SRC, WebKitMediaSrcPrivate);
    new (source->priv) WebKitMediaSrcPrivate();
}

static void webKitMediaSrcFinalize(GObject* object)
{
    ASSERT(isMainThread());

    WebKitMediaSrc* source = WEBKIT_MEDIA_SRC(object);
    source->priv->~WebKitMediaSrcPrivate();
    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

static GstPadProbeReturn debugProbe(GstPad* pad, GstPadProbeInfo* info, void*)
{
    RefPtr<Stream>& stream = WEBKIT_MEDIA_SRC_PAD(pad)->priv->stream;
    GST_TRACE_OBJECT(stream->source, "track %s: %" GST_PTR_FORMAT, stream->name.string().utf8().data(), info->data);
    return GST_PAD_PROBE_OK;
}

// GstStreamCollection are immutable objects once posted. THEY MUST NOT BE MODIFIED once they have been posted.
// Instead, when stream changes occur a new collection must be made. The following functions help to create
// such new collections:

static GRefPtr<GstStreamCollection> copyCollectionAndAddStream(GstStreamCollection* collection, GRefPtr<GstStream>&& stream)
{
    GRefPtr<GstStreamCollection> newCollection = adoptGRef(gst_stream_collection_new(collection->upstream_id));

    unsigned n = gst_stream_collection_get_size(collection);
    for (unsigned i = 0; i < n; i++)
        gst_stream_collection_add_stream(newCollection.get(), static_cast<GstStream*>(gst_object_ref(gst_stream_collection_get_stream(collection, i))));
    gst_stream_collection_add_stream(newCollection.get(), stream.leakRef());

    return newCollection;
}

static GRefPtr<GstStreamCollection> copyCollectionWithoutStream(GstStreamCollection* collection, const GstStream* stream)
{
    GRefPtr<GstStreamCollection> newCollection = adoptGRef(gst_stream_collection_new(collection->upstream_id));

    unsigned n = gst_stream_collection_get_size(collection);
    for (unsigned i = 0; i < n; i++) {
        GRefPtr<GstStream> oldStream = gst_stream_collection_get_stream(collection, i);
        if (oldStream.get() != stream)
            gst_stream_collection_add_stream(newCollection.get(), oldStream.leakRef());
    }

    return newCollection;
}

static GstStreamType gstStreamType(WebCore::MediaSourceStreamTypeGStreamer type)
{
    switch (type) {
    case WebCore::MediaSourceStreamTypeGStreamer::Video:
        return GST_STREAM_TYPE_VIDEO;
    case WebCore::MediaSourceStreamTypeGStreamer::Audio:
        return GST_STREAM_TYPE_AUDIO;
    case WebCore::MediaSourceStreamTypeGStreamer::Text:
        return GST_STREAM_TYPE_TEXT;
    default:
        GST_ERROR("Received unexpected stream type");
        return GST_STREAM_TYPE_UNKNOWN;
    }
}

void webKitMediaSrcAddStream(WebKitMediaSrc* source, const AtomString& name, WebCore::MediaSourceStreamTypeGStreamer type, GRefPtr<GstCaps>&& initialCaps)
{
    ASSERT(isMainThread());
    ASSERT(!source->priv->streams.contains(name));

    GRefPtr<GstStream> streamInfo = adoptGRef(gst_stream_new(name.string().utf8().data(), initialCaps.get(), gstStreamType(type), GST_STREAM_FLAG_SELECT));
    source->priv->collection = copyCollectionAndAddStream(source->priv->collection.get(), GRefPtr<GstStream>(streamInfo));
    gst_element_post_message(GST_ELEMENT(source), gst_message_new_stream_collection(GST_OBJECT(source), source->priv->collection.get()));

    GRefPtr<WebKitMediaSrcPad> pad = WEBKIT_MEDIA_SRC_PAD(g_object_new(webkit_media_src_pad_get_type(), "name", makeString("src_", name).utf8().data(), "direction", GST_PAD_SRC, NULL));
    gst_pad_set_activatemode_function(GST_PAD(pad.get()), webKitMediaSrcActivateMode);

    {
        RefPtr<Stream> stream = adoptRef(new Stream(source, GRefPtr<GstPad>(GST_PAD(pad.get())), name, type, WTFMove(initialCaps), WTFMove(streamInfo)));
        pad->priv->stream = stream;
        source->priv->streams.set(name, WTFMove(stream));
    }

    if (gst_debug_category_get_threshold(webkit_media_src_debug) >= GST_LEVEL_TRACE)
        gst_pad_add_probe(GST_PAD(pad.get()), static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM | GST_PAD_PROBE_TYPE_EVENT_FLUSH), debugProbe, nullptr, nullptr);

    // Workaround: gst_element_add_pad() should already call gst_pad_set_active() if the element is PAUSED or
    // PLAYING. Unfortunately, as of GStreamer 1.14.4 it does so with the element lock taken, causing a deadlock
    // in gst_pad_start_task(), who tries to post a `stream-status` message in the element, which also requires
    // the element lock. Activating the pad beforehand avoids that codepath.
    GstState state;
    gst_element_get_state(GST_ELEMENT(source), &state, nullptr, 0);
    if (state > GST_STATE_READY)
        gst_pad_set_active(GST_PAD(pad.get()), true);

    gst_element_add_pad(GST_ELEMENT(source), GST_PAD(pad.get()));
}

void webKitMediaSrcRemoveStream(WebKitMediaSrc* source, const AtomString& name)
{
    ASSERT(isMainThread());
    Stream* stream = source->priv->streamByName(name);

    source->priv->collection = copyCollectionWithoutStream(source->priv->collection.get(), stream->streamInfo.get());
    gst_element_post_message(GST_ELEMENT(source), gst_message_new_stream_collection(GST_OBJECT(source), source->priv->collection.get()));

    // Flush the source element **and** downstream. We want to stop the streaming thread and for that we need all elements downstream to be idle.
    webKitMediaSrcStreamFlushStart(stream);
    webKitMediaSrcStreamFlushStop(stream, false);
    // Stop the thread now.
    gst_pad_set_active(stream->pad.get(), false);

    stream->reportedStatus->wasRemoved = true;
    gst_element_remove_pad(GST_ELEMENT(source), stream->pad.get());
    source->priv->streams.remove(name);
}

static gboolean webKitMediaSrcActivateMode(GstPad* pad, GstObject* source, GstPadMode mode, gboolean active)
{
    if (mode != GST_PAD_MODE_PUSH) {
        GST_ERROR_OBJECT(source, "Unexpected pad mode in WebKitMediaSrc");
        return false;
    }

    if (active)
        gst_pad_start_task(pad, webKitMediaSrcLoop, pad, nullptr);
    else {
        // Unblock the streaming thread.
        RefPtr<Stream>& stream = WEBKIT_MEDIA_SRC_PAD(pad)->priv->stream;
        {
            DataMutex<Stream::StreamingMembers>::LockedWrapper streamingMembers(stream->streamingMembersDataMutex);
            streamingMembers->isFlushing = true;
            streamingMembers->padLinkedOrFlushedCondition.notifyOne();
            streamingMembers->queueChangedOrFlushedCondition.notifyOne();
        }
        // Following gstbasesrc implementation, this code is not flushing downstream.
        // If there is any possibility of the streaming thread being blocked downstream the caller MUST flush before.
        // Otherwise a deadlock would occur as the next function tries to join the thread.
        gst_pad_stop_task(pad);
        {
            DataMutex<Stream::StreamingMembers>::LockedWrapper streamingMembers(stream->streamingMembersDataMutex);
            streamingMembers->isFlushing = false;
        }
    }
    return true;
}

static void webKitMediaSrcPadLinked(GstPad* pad, GstPad*, void*)
{
    RefPtr<Stream>& stream = WEBKIT_MEDIA_SRC_PAD(pad)->priv->stream;
    DataMutex<Stream::StreamingMembers>::LockedWrapper streamingMembers(stream->streamingMembersDataMutex);
    streamingMembers->padLinkedOrFlushedCondition.notifyOne();
}

static void webKitMediaSrcStreamNotifyLowWaterLevel(const RefPtr<Stream>& stream)
{
    RunLoop::main().dispatch([stream]() {
        if (stream->reportedStatus->wasRemoved)
            return;

        stream->reportedStatus->isReadyForMoreSamples = true;
        if (stream->reportedStatus->sourceBufferPrivateToNotify) {
            // We need to set sourceBufferPrivateToNotify BEFORE calling sourceBufferPrivateDidBecomeReadyForMoreSamples(),
            // not after, since otherwise it would destroy a notification request should the callback request one.
            SourceBufferPrivateClient* sourceBuffer = stream->reportedStatus->sourceBufferPrivateToNotify;
            stream->reportedStatus->sourceBufferPrivateToNotify = nullptr;
            sourceBuffer->sourceBufferPrivateDidBecomeReadyForMoreSamples(stream->name);
        }
    });
}

// Called with STREAM_LOCK.
static void webKitMediaSrcLoop(void* userData)
{
    GstPad* pad = GST_PAD(userData);
    RefPtr<Stream>& stream = WEBKIT_MEDIA_SRC_PAD(pad)->priv->stream;

    DataMutex<Stream::StreamingMembers>::LockedWrapper streamingMembers(stream->streamingMembersDataMutex);
    if (streamingMembers->isFlushing) {
        gst_pad_pause_task(pad);
        return;
    }

    // Since the pad can and will be added when the element is in PLAYING state, this task can start running
    // before the pad is linked. Wait for the pad to be linked to avoid buffers being lost to not-linked errors.
    GST_OBJECT_LOCK(pad);
    if (!GST_PAD_IS_LINKED(pad)) {
        g_signal_connect(pad, "linked", G_CALLBACK(webKitMediaSrcPadLinked), nullptr);
        GST_OBJECT_UNLOCK(pad);

        streamingMembers->padLinkedOrFlushedCondition.wait(streamingMembers.mutex());

        g_signal_handlers_disconnect_by_func(pad, reinterpret_cast<void*>(webKitMediaSrcPadLinked), nullptr);
        if (streamingMembers->isFlushing)
            return;
    } else
        GST_OBJECT_UNLOCK(pad);
    ASSERT(gst_pad_is_linked(pad));

    // By keeping the lock we are guaranteed that a flush will not happen while we send essential events.
    // These events should never block downstream, so the lock should be released in little time in every
    // case.

    if (streamingMembers->pendingStreamCollectionEvent)
        gst_pad_push_event(stream->pad.get(), streamingMembers->pendingStreamCollectionEvent.leakRef());

    if (!streamingMembers->wasStreamStartSent) {
        GUniquePtr<char> streamId(g_strdup_printf("mse/%s", stream->name.string().utf8().data()));
        GRefPtr<GstEvent> event = adoptGRef(gst_event_new_stream_start(streamId.get()));
        gst_event_set_group_id(event.get(), stream->source->priv->groupId);
        gst_event_set_stream(event.get(), stream->streamInfo.get());

        bool wasStreamStartSent = gst_pad_push_event(pad, event.leakRef());
        streamingMembers->wasStreamStartSent = wasStreamStartSent;
    }

    if (streamingMembers->pendingInitialCaps) {
        GRefPtr<GstEvent> event = adoptGRef(gst_event_new_caps(streamingMembers->pendingInitialCaps.get()));

        gst_pad_push_event(pad, event.leakRef());

        streamingMembers->previousCaps = WTFMove(streamingMembers->pendingInitialCaps);
        ASSERT(!streamingMembers->pendingInitialCaps);
    }

    streamingMembers->queueChangedOrFlushedCondition.wait(streamingMembers.mutex(), [&]() {
        return !streamingMembers->queue.isEmpty() || streamingMembers->isFlushing;
    });
    if (streamingMembers->isFlushing)
        return;

    // We wait to get a sample before emitting the first segment. This way, if we get a seek before any
    // enqueue, we're sending only one segment. This also ensures that when such a seek is made, where we also
    // omit the flush (see webKitMediaSrcFlush) we actually emit the updated, correct segment.
    if (streamingMembers->doesNeedSegmentEvent) {
        gst_pad_push_event(pad, gst_event_new_segment(&streamingMembers->segment));
        streamingMembers->doesNeedSegmentEvent = false;
    }

    GRefPtr<GstMiniObject> object = streamingMembers->queue.takeFirst();
    if (GST_IS_SAMPLE(object.get())) {
        GRefPtr<GstSample> sample = adoptGRef(GST_SAMPLE(object.leakRef()));

        if (!gst_caps_is_equal(gst_sample_get_caps(sample.get()), streamingMembers->previousCaps.get())) {
            // This sample needs new caps (typically because of a quality change).
            gst_pad_push_event(stream->pad.get(), gst_event_new_caps(gst_sample_get_caps(sample.get())));
            streamingMembers->previousCaps = gst_sample_get_caps(sample.get());
        }

        if (streamingMembers->doesNeedToNotifyOnLowWaterLevel && streamingMembers->durationEnqueued() <= Stream::durationEnqueuedLowWaterLevel) {
            streamingMembers->doesNeedToNotifyOnLowWaterLevel = false;
            webKitMediaSrcStreamNotifyLowWaterLevel(RefPtr<Stream>(stream));
        }

        GRefPtr<GstBuffer> buffer = gst_sample_get_buffer(sample.get());
        sample.clear();

        if (!streamingMembers->hasPushedFirstBuffer) {
            GUniquePtr<char> fileName { g_strdup_printf("playback-pipeline-before-playback-%s", stream->name.string().utf8().data()) };
            GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(findPipeline(GRefPtr<GstElement>(GST_ELEMENT(stream->source))).get()),
                GST_DEBUG_GRAPH_SHOW_ALL, fileName.get());
            streamingMembers->hasPushedFirstBuffer = true;
        }

        // Push the buffer without the streamingMembers lock so that flushes can happen while it travels downstream.
        streamingMembers.lockHolder().unlockEarly();

        ASSERT(GST_BUFFER_PTS_IS_VALID(buffer.get()));
        GstFlowReturn ret = gst_pad_push(pad, buffer.leakRef());
        if (ret != GST_FLOW_OK && ret != GST_FLOW_FLUSHING) {
            GST_ERROR_OBJECT(pad, "Pushing buffer returned %s", gst_flow_get_name(ret));
            gst_pad_pause_task(pad);
        }
    } else if (GST_IS_EVENT(object.get())) {
        // EOS events and other enqueued events are also sent unlocked so they can react to flushes if necessary.
        GRefPtr<GstEvent> event = GRefPtr<GstEvent>(GST_EVENT(object.leakRef()));

        streamingMembers.lockHolder().unlockEarly();
        bool eventHandled = gst_pad_push_event(pad, GRefPtr<GstEvent>(event).leakRef());
        if (!eventHandled)
            GST_DEBUG_OBJECT(pad, "Pushed event was not handled: %" GST_PTR_FORMAT, event.get());
    } else
        ASSERT_NOT_REACHED();
}

static void webKitMediaSrcEnqueueObject(WebKitMediaSrc* source, const AtomString& streamName, GRefPtr<GstMiniObject>&& object)
{
    ASSERT(isMainThread());
    ASSERT(object);

    Stream* stream = source->priv->streamByName(streamName);
    DataMutex<Stream::StreamingMembers>::LockedWrapper streamingMembers(stream->streamingMembersDataMutex);
    streamingMembers->queue.append(WTFMove(object));
    if (stream->reportedStatus->isReadyForMoreSamples && streamingMembers->durationEnqueued() > Stream::durationEnqueuedHighWaterLevel) {
        stream->reportedStatus->isReadyForMoreSamples = false;
        streamingMembers->doesNeedToNotifyOnLowWaterLevel = true;
    }
    streamingMembers->queueChangedOrFlushedCondition.notifyOne();
}

void webKitMediaSrcEnqueueSample(WebKitMediaSrc* source, const AtomString& streamName, GRefPtr<GstSample>&& sample)
{
    ASSERT(GST_BUFFER_PTS_IS_VALID(gst_sample_get_buffer(sample.get())));
    webKitMediaSrcEnqueueObject(source, streamName, adoptGRef(GST_MINI_OBJECT(sample.leakRef())));
}

static void webKitMediaSrcEnqueueEvent(WebKitMediaSrc* source, const AtomString& streamName, GRefPtr<GstEvent>&& event)
{
    webKitMediaSrcEnqueueObject(source, streamName, adoptGRef(GST_MINI_OBJECT(event.leakRef())));
}

void webKitMediaSrcEndOfStream(WebKitMediaSrc* source, const AtomString& streamName)
{
    webKitMediaSrcEnqueueEvent(source, streamName, adoptGRef(gst_event_new_eos()));
}

bool webKitMediaSrcIsReadyForMoreSamples(WebKitMediaSrc* source, const AtomString& streamName)
{
    ASSERT(isMainThread());
    Stream* stream = source->priv->streamByName(streamName);
    return stream->reportedStatus->isReadyForMoreSamples;
}

void webKitMediaSrcNotifyWhenReadyForMoreSamples(WebKitMediaSrc* source, const AtomString& streamName, WebCore::SourceBufferPrivateClient* sourceBufferPrivate)
{
    ASSERT(isMainThread());
    Stream* stream = source->priv->streamByName(streamName);
    ASSERT(!stream->reportedStatus->isReadyForMoreSamples);
    stream->reportedStatus->sourceBufferPrivateToNotify = sourceBufferPrivate;
}

static GstStateChangeReturn webKitMediaSrcChangeState(GstElement* element, GstStateChange transition)
{
    WebKitMediaSrc* source = WEBKIT_MEDIA_SRC(element);
    if (transition == GST_STATE_CHANGE_PAUSED_TO_READY) {
        while (!source->priv->streams.isEmpty())
            webKitMediaSrcRemoveStream(source, source->priv->streams.begin()->key);
    }
    return GST_ELEMENT_CLASS(webkit_media_src_parent_class)->change_state(element, transition);
}

static void webKitMediaSrcStreamFlushStart(const RefPtr<Stream>& stream)
{
    ASSERT(isMainThread());
    {
        DataMutex<Stream::StreamingMembers>::LockedWrapper streamingMembers(stream->streamingMembersDataMutex);

        streamingMembers->isFlushing = true;
        streamingMembers->queueChangedOrFlushedCondition.notifyOne();
        streamingMembers->padLinkedOrFlushedCondition.notifyOne();
    }

    gst_pad_push_event(stream->pad.get(), gst_event_new_flush_start());
}

static void webKitMediaSrcStreamFlushStop(const RefPtr<Stream>& stream, bool resetTime)
{
    ASSERT(isMainThread());

    // By taking the stream lock we are waiting for the streaming thread task to stop if it hadn't yet.
    GST_PAD_STREAM_LOCK(stream->pad.get());
    {
        DataMutex<Stream::StreamingMembers>::LockedWrapper streamingMembers(stream->streamingMembersDataMutex);

        streamingMembers->isFlushing = false;
        streamingMembers->doesNeedSegmentEvent = true;
        streamingMembers->queue.clear();
        if (streamingMembers->doesNeedToNotifyOnLowWaterLevel) {
            streamingMembers->doesNeedToNotifyOnLowWaterLevel = false;
            webKitMediaSrcStreamNotifyLowWaterLevel(stream);
        }
    }

    // Since FLUSH_STOP is a synchronized event, we send it while we still hold the stream lock of the pad.
    gst_pad_push_event(stream->pad.get(), gst_event_new_flush_stop(resetTime));

    gst_pad_start_task(stream->pad.get(), webKitMediaSrcLoop, stream->pad.get(), nullptr);
    GST_PAD_STREAM_UNLOCK(stream->pad.get());
}

void webKitMediaSrcFlush(WebKitMediaSrc* source, const AtomString& streamName)
{
    ASSERT(isMainThread());
    Stream* stream = source->priv->streamByName(streamName);

    bool hasPushedFirstBuffer;
    {
        DataMutex<Stream::StreamingMembers>::LockedWrapper streamingMembers(stream->streamingMembersDataMutex);
        hasPushedFirstBuffer = streamingMembers->hasPushedFirstBuffer;
    }

    if (hasPushedFirstBuffer) {
        // If no buffer has been pushed there is no need for flush... and flushing at that point could
        // expose bugs in downstream which may have not completely initialized (e.g. decodebin3 not
        // having linked the chain so far and forgetting to do it after the flush).
        webKitMediaSrcStreamFlushStart(stream);
    }

    GstClockTime pipelineStreamTime;
    gst_element_query_position(findPipeline(GRefPtr<GstElement>(GST_ELEMENT(source))).get(), GST_FORMAT_TIME,
        reinterpret_cast<gint64*>(&pipelineStreamTime));
    // -1 is returned when the pipeline is not yet pre-rolled (e.g. just after a seek). In this case we don't need to
    // adjust the segment though, as running time has not advanced.
    if (GST_CLOCK_TIME_IS_VALID(pipelineStreamTime)) {
        DataMutex<Stream::StreamingMembers>::LockedWrapper streamingMembers(stream->streamingMembersDataMutex);
        // We need to increase the base by the running time accumulated during the previous segment.

        GstClockTime pipelineRunningTime = gst_segment_to_running_time(&streamingMembers->segment, GST_FORMAT_TIME, pipelineStreamTime);
        assert(GST_CLOCK_TIME_IS_VALID(pipelineRunningTime));
        streamingMembers->segment.base = pipelineRunningTime;

        streamingMembers->segment.start = streamingMembers->segment.time = static_cast<GstClockTime>(pipelineStreamTime);
    }

    if (hasPushedFirstBuffer)
        webKitMediaSrcStreamFlushStop(stream, false);
}

void webKitMediaSrcSeek(WebKitMediaSrc* source, uint64_t startTime, double rate)
{
    ASSERT(isMainThread());
    source->priv->startTime = startTime;
    source->priv->rate = rate;

    for (auto& pair : source->priv->streams) {
        const RefPtr<Stream>& stream = pair.value;
        bool hasPushedFirstBuffer;
        {
            DataMutex<Stream::StreamingMembers>::LockedWrapper streamingMembers(stream->streamingMembersDataMutex);
            hasPushedFirstBuffer = streamingMembers->hasPushedFirstBuffer;
        }

        if (hasPushedFirstBuffer) {
            // If no buffer has been pushed there is no need for flush... and flushing at that point could
            // expose bugs in downstream which may have not completely initialized (e.g. decodebin3 not
            // having linked the chain so far and forgetting to do it after the flush).
            webKitMediaSrcStreamFlushStart(stream);
        }

        {
            DataMutex<Stream::StreamingMembers>::LockedWrapper streamingMembers(stream->streamingMembersDataMutex);
            streamingMembers->segment.base = 0;
            streamingMembers->segment.rate = rate;
            streamingMembers->segment.start = streamingMembers->segment.time = startTime;
        }

        if (hasPushedFirstBuffer)
            webKitMediaSrcStreamFlushStop(stream, true);
    }
}

static int countStreamsOfType(WebKitMediaSrc* source, WebCore::MediaSourceStreamTypeGStreamer type)
{
    // Barring pipeline dumps someone may add during debugging, WebKit will only read these properties (n-video etc.) from the main thread.
    return std::count_if(source->priv->streams.begin(), source->priv->streams.end(), [type](auto item) {
        return item.value->type == type;
    });
}

static void webKitMediaSrcGetProperty(GObject* object, unsigned propId, GValue* value, GParamSpec* pspec)
{
    WebKitMediaSrc* source = WEBKIT_MEDIA_SRC(object);

    switch (propId) {
    case PROP_N_AUDIO:
        g_value_set_int(value, countStreamsOfType(source, WebCore::MediaSourceStreamTypeGStreamer::Audio));
        break;
    case PROP_N_VIDEO:
        g_value_set_int(value, countStreamsOfType(source, WebCore::MediaSourceStreamTypeGStreamer::Video));
        break;
    case PROP_N_TEXT:
        g_value_set_int(value, countStreamsOfType(source, WebCore::MediaSourceStreamTypeGStreamer::Text));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
    }
}

// URI handler interface. It's only purpose is for the element to be instantiated by playbin on "mediasourceblob:"
// URIs. The actual URI does not matter.
static GstURIType webKitMediaSrcUriGetType(GType)
{
    return GST_URI_SRC;
}

static const gchar* const* webKitMediaSrcGetProtocols(GType)
{
    static const char* protocols[] = {"mediasourceblob", nullptr };
    return protocols;
}

static gchar* webKitMediaSrcGetUri(GstURIHandler* handler)
{
    WebKitMediaSrc* source = WEBKIT_MEDIA_SRC(handler);
    gchar* result;

    GST_OBJECT_LOCK(source);
    result = g_strdup(source->priv->uri.get());
    GST_OBJECT_UNLOCK(source);
    return result;
}

static gboolean webKitMediaSrcSetUri(GstURIHandler* handler, const gchar* uri, GError**)
{
    WebKitMediaSrc* source = WEBKIT_MEDIA_SRC(handler);

    if (GST_STATE(source) >= GST_STATE_PAUSED) {
        GST_ERROR_OBJECT(source, "URI can only be set in states < PAUSED");
        return false;
    }

    GST_OBJECT_LOCK(source);
    source->priv->uri = GUniquePtr<char>(g_strdup(uri));
    GST_OBJECT_UNLOCK(source);
    return TRUE;
}

static void webKitMediaSrcUriHandlerInit(void* gIface, void*)
{
    GstURIHandlerInterface* iface = (GstURIHandlerInterface *) gIface;

    iface->get_type = webKitMediaSrcUriGetType;
    iface->get_protocols = webKitMediaSrcGetProtocols;
    iface->get_uri = webKitMediaSrcGetUri;
    iface->set_uri = webKitMediaSrcSetUri;
}

namespace WTF {
template <> GRefPtr<WebKitMediaSrc> adoptGRef(WebKitMediaSrc* ptr)
{
    ASSERT(!ptr || !g_object_is_floating(G_OBJECT(ptr)));
    return GRefPtr<WebKitMediaSrc>(ptr, GRefPtrAdopt);
}

template <> WebKitMediaSrc* refGPtr<WebKitMediaSrc>(WebKitMediaSrc* ptr)
{
    if (ptr)
        gst_object_ref_sink(GST_OBJECT(ptr));

    return ptr;
}

template <> void derefGPtr<WebKitMediaSrc>(WebKitMediaSrc* ptr)
{
    if (ptr)
        gst_object_unref(ptr);
}
} // namespace WTF

#endif // USE(GSTREAMER)

