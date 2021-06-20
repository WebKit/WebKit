/*
 *  Copyright (C) 2009, 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *  Copyright (C) 2013 Collabora Ltd.
 *  Copyright (C) 2013 Orange
 *  Copyright (C) 2014, 2015 Sebastian Dröge <sebastian@centricular.com>
 *  Copyright (C) 2015, 2016, 2018, 2019, 2020, 2021 Metrological Group B.V.
 *  Copyright (C) 2015, 2016, 2018, 2019, 2020, 2021 Igalia, S.L
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
#include "MediaSourceTrackGStreamer.h"
#include "VideoTrackPrivateGStreamer.h"
#include <cassert>
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

#define webkit_media_src_parent_class parent_class
#define WEBKIT_MEDIA_SRC_CATEGORY_INIT GST_DEBUG_CATEGORY_INIT(webkit_media_src_debug, "webkitmediasrc", 0, "websrc element");

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
        ASSERT(isMainThread());
        Stream* stream = streams.get(name);
        ASSERT(stream);
        return stream;
    }

    // Used for stream-start events, shared by all streams.
    const unsigned groupId { gst_util_group_id_next() };

    // Set once when the source is started. Not changed after.
    GRefPtr<GstStreamCollection> collection;
    bool isStarted() { return collection; }

    // Changed on seeks.
    GstClockTime startTime { 0 };
    double rate { 1.0 };

    // Only used by URI Handler API implementation.
    GUniquePtr<char> uri;
};

static void webKitMediaSrcUriHandlerInit(gpointer, gpointer);
static void webKitMediaSrcConstructed(GObject*);
static GstStateChangeReturn webKitMediaSrcChangeState(GstElement*, GstStateChange);
static gboolean webKitMediaSrcActivateMode(GstPad*, GstObject*, GstPadMode, gboolean activate);
static void webKitMediaSrcLoop(void*);
static void webKitMediaSrcTearDownStream(WebKitMediaSrc* source, const AtomString& name);
static void webKitMediaSrcGetProperty(GObject*, unsigned propId, GValue*, GParamSpec*);
static void webKitMediaSrcStreamFlush(Stream*, bool isSeekingFlush);
static gboolean webKitMediaSrcSendEvent(GstElement*, GstEvent*);

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

WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitMediaSrc, webkit_media_src, GST_TYPE_ELEMENT,
    G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, webKitMediaSrcUriHandlerInit);
    GST_DEBUG_CATEGORY_INIT(webkit_media_src_debug, "webkitmediasrc", 0, "WebKit MSE source element"));

struct Stream : public ThreadSafeRefCounted<Stream> {
    Stream(WebKitMediaSrc* source, GRefPtr<GstPad>&& pad, Ref<MediaSourceTrackGStreamer>&& track, GRefPtr<GstStream>&& streamInfo)
        : source(source)
        , pad(WTFMove(pad))
        , track(WTFMove(track))
        , streamInfo(WTFMove(streamInfo))
        , streamingMembersDataMutex(GRefPtr(this->track->initialCaps()), source->priv->startTime, source->priv->rate)
    {
        ASSERT(this->track->initialCaps());
    }

    WebKitMediaSrc* const source;
    GRefPtr<GstPad> const pad;
    Ref<MediaSourceTrackGStreamer> track;
    GRefPtr<GstStream> streamInfo;

    struct StreamingMembers {
        StreamingMembers(GRefPtr<GstCaps>&& initialCaps, GstClockTime startTime, double rate)
            : pendingInitialCaps(WTFMove(initialCaps))
        {
            gst_segment_init(&segment, GST_FORMAT_TIME);
            segment.start = segment.time = startTime;
            segment.rate = rate;
            ASSERT(pendingInitialCaps);
        }

        bool hasPushedStreamCollectionEvent { false };
        bool wasStreamStartSent { false };
        bool doesNeedSegmentEvent { true };
        bool hasPushedFirstBuffer { false }; // Used to get a pipeline dump of the pipeline before buffers are flowing.
        GstSegment segment;
        GRefPtr<GstCaps> pendingInitialCaps;
        GRefPtr<GstCaps> previousCaps; // Caps from enqueued samples are compared to these to push CAPS events as needed.
        Condition padLinkedOrFlushedCondition;
        Condition queueChangedOrFlushedCondition;
        bool isFlushing { false };

        // Flushes before any buffer has been popped from the queue and sent downstream can be avoided just
        // by clearing the queue.
        bool hasPoppedFirstObject { false };
    };
    DataMutex<StreamingMembers> streamingMembersDataMutex;
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

static GstStreamType gstStreamType(TrackPrivateBaseGStreamer::TrackType type)
{
    switch (type) {
    case TrackPrivateBaseGStreamer::TrackType::Video:
        return GST_STREAM_TYPE_VIDEO;
    case TrackPrivateBaseGStreamer::TrackType::Audio:
        return GST_STREAM_TYPE_AUDIO;
    case TrackPrivateBaseGStreamer::TrackType::Text:
        return GST_STREAM_TYPE_TEXT;
    default:
        GST_ERROR("Received unexpected stream type");
        return GST_STREAM_TYPE_UNKNOWN;
    }
}

#ifndef GST_DISABLE_GST_DEBUG
static const char* streamTypeToString(TrackPrivateBaseGStreamer::TrackType type)
{
    switch (type) {
    case TrackPrivateBaseGStreamer::TrackType::Audio:
        return "Audio";
    case TrackPrivateBaseGStreamer::TrackType::Video:
        return "Video";
    case TrackPrivateBaseGStreamer::TrackType::Text:
        return "Text";
    default:
    case TrackPrivateBaseGStreamer::TrackType::Unknown:
        return "Unknown";
    }
}
#endif // GST_DISABLE_GST_DEBUG

static void webkit_media_src_class_init(WebKitMediaSrcClass* klass)
{
    GObjectClass* oklass = G_OBJECT_CLASS(klass);
    GstElementClass* eklass = GST_ELEMENT_CLASS(klass);

    oklass->constructed = webKitMediaSrcConstructed;
    oklass->get_property = webKitMediaSrcGetProperty;

    gst_element_class_add_static_pad_template_with_gtype(eklass, &srcTemplate, webkit_media_src_pad_get_type());

    gst_element_class_set_static_metadata(eklass, "WebKit MediaSource source element", "Source/Network", "Feeds samples coming from WebKit MediaSource object", "Igalia <aboya@igalia.com>");

    eklass->change_state = webKitMediaSrcChangeState;
    eklass->send_event = webKitMediaSrcSendEvent;
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

static void webKitMediaSrcConstructed(GObject* object)
{
    GST_CALL_PARENT(G_OBJECT_CLASS, constructed, (object));

    ASSERT(isMainThread());
    GST_OBJECT_FLAG_SET(object, GST_ELEMENT_FLAG_SOURCE);
}

void webKitMediaSrcEmitStreams(WebKitMediaSrc* source, const Vector<RefPtr<MediaSourceTrackGStreamer>>& tracks)
{
    ASSERT(isMainThread());
    ASSERT(!source->priv->isStarted());
    GST_DEBUG_OBJECT(source, "Emitting STREAM_COLLECTION");

    source->priv->collection = adoptGRef(gst_stream_collection_new("WebKitMediaSrc"));
    for (const auto& track : tracks) {
        GST_DEBUG_OBJECT(source, "Adding stream with trackId '%s' of type %s with caps %" GST_PTR_FORMAT, track->trackId().string().utf8().data(), streamTypeToString(track->type()), track->initialCaps().get());

        GRefPtr<WebKitMediaSrcPad> pad = WEBKIT_MEDIA_SRC_PAD(g_object_new(webkit_media_src_pad_get_type(), "name", makeString("src_", track->trackId()).utf8().data(), "direction", GST_PAD_SRC, NULL));
        gst_pad_set_activatemode_function(GST_PAD(pad.get()), webKitMediaSrcActivateMode);

        ASSERT(track->initialCaps());
        RefPtr<Stream> stream = adoptRef(new Stream(source, GRefPtr<GstPad>(GST_PAD(pad.get())), makeRef(*track),
            adoptGRef(gst_stream_new(track->trackId().string().utf8().data(), track->initialCaps().get(), gstStreamType(track->type()), GST_STREAM_FLAG_SELECT))));
        pad->priv->stream = stream;

        gst_stream_collection_add_stream(source->priv->collection.get(), GRefPtr<GstStream>(stream->streamInfo.get()).leakRef());
        source->priv->streams.set(track->trackId(), WTFMove(stream));
    }

    gst_element_post_message(GST_ELEMENT(source), gst_message_new_stream_collection(GST_OBJECT(source), source->priv->collection.get()));

    for (const RefPtr<Stream>& stream: source->priv->streams.values()) {
        // Workaround: gst_element_add_pad() should already call gst_pad_set_active() if the element is PAUSED or
        // PLAYING. Unfortunately, as of GStreamer 1.18.2 it does so with the element lock taken, causing a deadlock
        // in gst_pad_start_task(), who tries to post a `stream-status` message in the element, which also requires
        // the element lock. Activating the pad beforehand avoids that codepath.
        // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/210
        // FIXME: Remove this workaround when the bug gets fixed and versions without the bug are no longer in use.
        GstState state;
        gst_element_get_state(GST_ELEMENT(source), &state, nullptr, 0);
        if (state > GST_STATE_READY)
            gst_pad_set_active(GST_PAD(stream->pad.get()), true);

        GST_DEBUG_OBJECT(source, "Adding pad '%s' for stream with name '%s'", GST_OBJECT_NAME(stream->pad.get()), stream->track->trackId().string().utf8().data());
        gst_element_add_pad(GST_ELEMENT(source), GST_PAD(stream->pad.get()));
    }
    GST_DEBUG_OBJECT(source, "All pads added");
}

static void webKitMediaSrcTearDownStream(WebKitMediaSrc* source, const AtomString& name)
{
    ASSERT(isMainThread());
    Stream* stream = source->priv->streamByName(name);
    GST_DEBUG_OBJECT(source, "Tearing down stream '%s'", name.string().utf8().data());

    // Flush the source element **and** downstream. We want to stop the streaming thread and for that we need all elements downstream to be idle.
    webKitMediaSrcStreamFlush(stream, false);
    // Stop the thread now.
    gst_pad_set_active(stream->pad.get(), false);

    if (source->priv->isStarted())
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
            DataMutexLocker streamingMembers { stream->streamingMembersDataMutex };
            streamingMembers->isFlushing = true;
            streamingMembers->padLinkedOrFlushedCondition.notifyOne();
            streamingMembers->queueChangedOrFlushedCondition.notifyOne();
        }
        // Following gstbasesrc implementation, this code is not flushing downstream.
        // If there is any possibility of the streaming thread being blocked downstream the caller MUST flush before.
        // Otherwise a deadlock would occur as the next function tries to join the thread.
        gst_pad_stop_task(pad);
        {
            DataMutexLocker streamingMembers { stream->streamingMembersDataMutex };
            streamingMembers->isFlushing = false;
        }
    }
    return true;
}

static void webKitMediaSrcPadLinked(GstPad* pad, GstPad*, void*)
{
    RefPtr<Stream>& stream = WEBKIT_MEDIA_SRC_PAD(pad)->priv->stream;
    DataMutexLocker streamingMembers { stream->streamingMembersDataMutex };
    streamingMembers->padLinkedOrFlushedCondition.notifyOne();
}

static void webKitMediaSrcWaitForPadLinkedOrFlush(GstPad* pad, DataMutexLocker<Stream::StreamingMembers>& streamingMembers)
{
    {
        auto locker = GstObjectLocker(pad);
        if (LIKELY(GST_PAD_IS_LINKED(pad)))
            return;

        GST_DEBUG_OBJECT(pad, "Waiting for the pad to be linked...");
        g_signal_connect(pad, "linked", G_CALLBACK(webKitMediaSrcPadLinked), nullptr);
    }

    assertIsHeld(streamingMembers.mutex());
    streamingMembers->padLinkedOrFlushedCondition.wait(streamingMembers.mutex());

    g_signal_handlers_disconnect_by_func(pad, reinterpret_cast<void*>(webKitMediaSrcPadLinked), nullptr);
    GST_DEBUG_OBJECT(pad, "Finished waiting for the pad to be linked.");
}

// Called with STREAM_LOCK.
static void webKitMediaSrcLoop(void* userData)
{
    GstPad* pad = GST_PAD(userData);
    RefPtr<Stream>& stream = WEBKIT_MEDIA_SRC_PAD(pad)->priv->stream;

    DataMutexLocker streamingMembers { stream->streamingMembersDataMutex };
    if (streamingMembers->isFlushing) {
        gst_pad_pause_task(pad);
        return;
    }

    // Since the pad can and will be added when the element is in PLAYING state, this task can start running
    // before the pad is linked. Wait for the pad to be linked to avoid buffers being lost to not-linked errors.
    webKitMediaSrcWaitForPadLinkedOrFlush(pad, streamingMembers);
    if (streamingMembers->isFlushing) {
        gst_pad_pause_task(pad);
        return;
    }
    ASSERT(gst_pad_is_linked(pad));

    // By keeping the lock we are guaranteed that a flush will not happen while we send essential events.
    // These events should never block downstream, so the lock should be released in little time in every
    // case.

    if (!streamingMembers->hasPushedStreamCollectionEvent) {
        GST_DEBUG_OBJECT(pad, "Pushing STREAM_COLLECTION event.");
        bool wasStreamCollectionSent = gst_pad_push_event(stream->pad.get(), gst_event_new_stream_collection(stream->source->priv->collection.get()));
        streamingMembers->hasPushedStreamCollectionEvent = true;
        GST_DEBUG_OBJECT(pad, "STREAM_COLLECTION event has been pushed, %s was returned.", boolForPrinting(wasStreamCollectionSent));
        // Initial events like this must go through, flushes (including tearing down the element) is not allowed until
        // `hasPushedFirstBuffer` has been set to true.
        ASSERT(wasStreamCollectionSent);
    }

    if (!streamingMembers->wasStreamStartSent) {
        GUniquePtr<char> streamId { g_strdup_printf("mse/%s", stream->track->trackId().string().utf8().data()) };
        GRefPtr<GstEvent> event = adoptGRef(gst_event_new_stream_start(streamId.get()));
        gst_event_set_group_id(event.get(), stream->source->priv->groupId);
        gst_event_set_stream(event.get(), stream->streamInfo.get());

        GST_DEBUG_OBJECT(pad, "Pushing STREAM_START event.");
        bool wasStreamStartSent = gst_pad_push_event(pad, event.leakRef());
        streamingMembers->wasStreamStartSent = wasStreamStartSent;
        GST_DEBUG_OBJECT(pad, "STREAM_START event pushed, %s was returned.", boolForPrinting(wasStreamStartSent));
        ASSERT(wasStreamStartSent);
    }

    if (streamingMembers->pendingInitialCaps) {
        GRefPtr<GstEvent> event = adoptGRef(gst_event_new_caps(streamingMembers->pendingInitialCaps.get()));

        GST_DEBUG_OBJECT(pad, "Pushing initial CAPS event: %" GST_PTR_FORMAT, streamingMembers->pendingInitialCaps.get());
        bool wasCapsEventSent = gst_pad_push_event(pad, event.leakRef());
        GST_DEBUG_OBJECT(pad, "Pushed initial CAPS event, %s was returned.", boolForPrinting(wasCapsEventSent));

        streamingMembers->previousCaps = WTFMove(streamingMembers->pendingInitialCaps);
        ASSERT(!streamingMembers->pendingInitialCaps);
    }

    GRefPtr<GstMiniObject> object;
    {
        DataMutexLocker queue { stream->track->queueDataMutex() };
        if (!queue->isEmpty()) {
            object = queue->pop();
            streamingMembers->hasPoppedFirstObject = true;
            GST_TRACE_OBJECT(pad, "Queue not empty, popped %" GST_PTR_FORMAT, object.get());
        } else {
            queue->notifyWhenNotEmpty([&object, stream](GRefPtr<GstMiniObject>&& receivedObject) {
                ASSERT(isMainThread());
                DataMutexLocker streamingMembers { stream->streamingMembersDataMutex };
                ASSERT(!streamingMembers->isFlushing);

                object = WTFMove(receivedObject);
                streamingMembers->hasPoppedFirstObject = true;
                streamingMembers->queueChangedOrFlushedCondition.notifyAll();
            });
            GST_TRACE_OBJECT(pad, "Waiting for objects to be pushed to the track queue.");
        }
    }

    // Wait to receive an object from the queue (if we didn't get one already) or flush.
    streamingMembers->queueChangedOrFlushedCondition.wait(streamingMembers.mutex(), [&]() {
        return streamingMembers->isFlushing || object;
    });
    {
        // Ensure that notifyWhenNotEmpty()'s callback (if any) is cleared after this point.
        DataMutexLocker queue { stream->track->queueDataMutex() };
        queue->resetNotEmptyHandler();
    }
    if (streamingMembers->isFlushing) {
        gst_pad_pause_task(pad);
        return;
    }

    // We wait to get a sample before emitting the first segment. This way, if we get a seek before any
    // enqueue, we're sending only one segment. This also ensures that when such a seek is made, where we also
    // omit the flush (see webKitMediaSrcFlush) we actually emit the updated, correct segment.
    if (streamingMembers->doesNeedSegmentEvent) {
        GST_DEBUG_OBJECT(pad, "Need new SEGMENT event, pushing it: %" GST_SEGMENT_FORMAT, &streamingMembers->segment);
        bool result = gst_pad_push_event(pad, gst_event_new_segment(&streamingMembers->segment));
        GST_DEBUG_OBJECT(pad, "SEGMENT event pushed, result = %s.", boolForPrinting(result));
        ASSERT(result);
        streamingMembers->doesNeedSegmentEvent = false;
    }

    if (GST_IS_SAMPLE(object.get())) {
        GRefPtr<GstSample> sample = adoptGRef(GST_SAMPLE(object.leakRef()));

        if (!gst_caps_is_equal(gst_sample_get_caps(sample.get()), streamingMembers->previousCaps.get())) {
            // This sample needs new caps (typically because of a quality change).
            GST_DEBUG_OBJECT(pad, "Pushing new CAPS event: %" GST_PTR_FORMAT, gst_sample_get_caps(sample.get()));
            bool result = gst_pad_push_event(stream->pad.get(), gst_event_new_caps(gst_sample_get_caps(sample.get())));
            GST_DEBUG_OBJECT(pad, "CAPS event pushed, result = %s.", boolForPrinting(result));
            ASSERT(result);
            streamingMembers->previousCaps = gst_sample_get_caps(sample.get());
        }

        GRefPtr<GstBuffer> buffer = gst_sample_get_buffer(sample.get());
        sample.clear();

        bool pushingFirstBuffer = !streamingMembers->hasPushedFirstBuffer;
        if (pushingFirstBuffer) {
            GST_DEBUG_OBJECT(pad, "Sending first buffer on this pad.");
            GUniquePtr<char> fileName { g_strdup_printf("playback-pipeline-before-playback-%s", stream->track->trackId().string().utf8().data()) };
            GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(findPipeline(GRefPtr<GstElement>(GST_ELEMENT(stream->source))).get()),
                GST_DEBUG_GRAPH_SHOW_ALL, fileName.get());
            streamingMembers->hasPushedFirstBuffer = true;
        }

        // Push the buffer without the streamingMembers lock so that flushes can happen while it travels downstream.
        streamingMembers.unlockEarly();

        ASSERT(GST_BUFFER_PTS_IS_VALID(buffer.get()));
        GST_TRACE_OBJECT(pad, "Pushing buffer downstream: %" GST_PTR_FORMAT, buffer.get());
        GstFlowReturn result = gst_pad_push(pad, buffer.leakRef());
        if (result != GST_FLOW_OK && result != GST_FLOW_FLUSHING) {
            GST_ERROR_OBJECT(pad, "Pushing buffer returned %s", gst_flow_get_name(result));
            GUniquePtr<char> fileName { g_strdup_printf("playback-pipeline-pushing-buffer-failed-%s", stream->track->trackId().string().utf8().data()) };
            GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(findPipeline(GRefPtr<GstElement>(GST_ELEMENT(stream->source))).get()),
                GST_DEBUG_GRAPH_SHOW_ALL, fileName.get());
            gst_pad_pause_task(pad);
        }
    } else if (GST_IS_EVENT(object.get())) {
        // EOS events and other enqueued events are also sent unlocked so they can react to flushes if necessary.
        GRefPtr<GstEvent> event = GRefPtr<GstEvent>(GST_EVENT(object.leakRef()));

        streamingMembers.unlockEarly();
        GST_DEBUG_OBJECT(pad, "Pushing event downstream: %" GST_PTR_FORMAT, event.get());
        bool eventHandled = gst_pad_push_event(pad, GRefPtr<GstEvent>(event).leakRef());
        if (!eventHandled)
            GST_DEBUG_OBJECT(pad, "Pushed event was not handled: %" GST_PTR_FORMAT, event.get());
    } else
        ASSERT_NOT_REACHED();
}

static void webKitMediaSrcStreamFlush(Stream* stream, bool isSeekingFlush)
{
    ASSERT(isMainThread());
    bool skipFlush = false;

    {
        DataMutexLocker streamingMembers { stream->streamingMembersDataMutex };

        if (!streamingMembers->hasPoppedFirstObject) {
            GST_DEBUG_OBJECT(stream->source, "Flush request for stream '%s' occurred before hasPoppedFirstObject, just clearing the queue and readjusting the segment.", stream->track->trackId().string().utf8().data());
            DataMutexLocker queue { stream->track->queueDataMutex() };
            // We use clear() instead of flush() because the WebKitMediaSrc streaming thread could be waiting
            // for the queue. flush() would cancel the notEmptyCallback therefore leaving the streaming thread
            // stuck waiting forever.
            queue->clear();
            skipFlush = true;
        }
    }

    if (!skipFlush) {
        // Signal the loop() function to stop waiting for any condition variable, pause the task and return,
        // which will keeping the streaming thread idle.
        GST_DEBUG_OBJECT(stream->pad.get(), "Taking the StreamingMembers mutex and setting isFlushing = true.");
        {
            DataMutexLocker streamingMembers { stream->streamingMembersDataMutex };
            DataMutexLocker queue { stream->track->queueDataMutex() };

            streamingMembers->isFlushing = true;
            queue->flush(); // Clear the queue and cancel any waiting callback.

            streamingMembers->queueChangedOrFlushedCondition.notifyAll();
            streamingMembers->padLinkedOrFlushedCondition.notifyAll();
        }

        // Flush downstream. This will stop processing in downstream elements and if the streaming thread was in a
        // downstream chain() function, it will quickly return to the loop() function, which thanks to the
        // previous section will also quickly end.
        GST_DEBUG_OBJECT(stream->pad.get(), "Sending FLUSH_START downstream.");
        gst_pad_push_event(stream->pad.get(), gst_event_new_flush_start());
        GST_DEBUG_OBJECT(stream->pad.get(), "FLUSH_START sent.");
    }

    // Adjust segment. This is different for seeks and non-seeking flushes.
    if (isSeekingFlush) {
        // In the case of seeking flush we are resetting the timeline (see the flush stop later).
        // The resulting segment is brand new, but with a different start time.
        WebKitMediaSrcPrivate* priv = stream->source->priv;
        DataMutexLocker streamingMembers { stream->streamingMembersDataMutex };
        streamingMembers->segment.base = 0;
        streamingMembers->segment.rate = priv->rate;
        streamingMembers->segment.start = streamingMembers->segment.time = priv->startTime;
    } else {
        // In the case of non-seeking flushes we don't reset the timeline, so instead we need to increase the `base` field
        // by however running time we're starting after the flush.

        GstClockTime pipelineStreamTime;
        gst_element_query_position(findPipeline(GRefPtr<GstElement>(GST_ELEMENT(stream->source))).get(), GST_FORMAT_TIME,
            reinterpret_cast<gint64*>(&pipelineStreamTime));
        // GST_CLOCK_TIME_NONE is returned when the pipeline is not yet pre-rolled (e.g. just after a seek). In this case
        // we don't need to adjust the segment though, as running time has not advanced.
        if (GST_CLOCK_TIME_IS_VALID(pipelineStreamTime)) {
            DataMutexLocker streamingMembers { stream->streamingMembersDataMutex };
            // We need to increase the base by the running time accumulated during the previous segment.

            GstClockTime pipelineRunningTime = gst_segment_to_running_time(&streamingMembers->segment, GST_FORMAT_TIME, pipelineStreamTime);
            assert(GST_CLOCK_TIME_IS_VALID(pipelineRunningTime));
            GST_DEBUG_OBJECT(stream->source, "Resetting segment to current pipeline running time (%" GST_TIME_FORMAT") and stream time (%" GST_TIME_FORMAT ")",
                GST_TIME_ARGS(pipelineRunningTime), GST_TIME_ARGS(pipelineStreamTime));
            streamingMembers->segment.base = pipelineRunningTime;

            streamingMembers->segment.start = streamingMembers->segment.time = static_cast<GstClockTime>(pipelineStreamTime);
        }
    }

    if (!skipFlush) {
        // By taking the stream lock we are waiting for the streaming thread task to stop if it hadn't yet.
        GST_DEBUG_OBJECT(stream->pad.get(), "Taking the STREAM_LOCK.");
        auto streamLock = GstPadStreamLocker(stream->pad.get());
        {
            GST_DEBUG_OBJECT(stream->pad.get(), "Taking the StreamingMembers mutex again.");
            DataMutexLocker streamingMembers { stream->streamingMembersDataMutex };
            GST_DEBUG_OBJECT(stream->pad.get(), "StreamingMembers mutex taken, using it to set isFlushing = false.");
            streamingMembers->isFlushing = false;
            streamingMembers->doesNeedSegmentEvent = true;
        }

        GST_DEBUG_OBJECT(stream->pad.get(), "Sending FLUSH_STOP downstream (resetTime = %s).", boolForPrinting(isSeekingFlush));
        // Since FLUSH_STOP is a synchronized event, we send it while we still hold the stream lock of the pad.
        gst_pad_push_event(stream->pad.get(), gst_event_new_flush_stop(isSeekingFlush));
        GST_DEBUG_OBJECT(stream->pad.get(), "FLUSH_STOP sent.");

        GST_DEBUG_OBJECT(stream->pad.get(), "Starting webKitMediaSrcLoop task and releasing the STREAM_LOCK.");
        gst_pad_start_task(stream->pad.get(), webKitMediaSrcLoop, stream->pad.get(), nullptr);
    }
}

void webKitMediaSrcFlush(WebKitMediaSrc* source, const AtomString& streamName)
{
    ASSERT(isMainThread());
    GST_DEBUG_OBJECT(source, "Received non-seek flush request for stream '%s'.", streamName.string().utf8().data());
    Stream* stream = source->priv->streamByName(streamName);

    webKitMediaSrcStreamFlush(stream, false);
}

static void webKitMediaSrcSeek(WebKitMediaSrc* source, uint64_t startTime, double rate)
{
    ASSERT(isMainThread());
    source->priv->startTime = startTime;
    source->priv->rate = rate;
    GST_DEBUG_OBJECT(source, "Seek requested to time %" GST_TIME_FORMAT " with rate %f.", GST_TIME_ARGS(startTime), rate);

    for (const RefPtr<Stream>& stream : source->priv->streams.values())
        webKitMediaSrcStreamFlush(stream.get(), true);
}

static int countStreamsOfType(WebKitMediaSrc* source, WebCore::TrackPrivateBaseGStreamer::TrackType type)
{
    // Barring pipeline dumps someone may add during debugging, WebKit will only read these properties (n-video etc.) from the main thread.
    return std::count_if(source->priv->streams.begin(), source->priv->streams.end(), [type](auto item) {
        return item.value->track->type() == type;
    });
}

static void webKitMediaSrcGetProperty(GObject* object, unsigned propId, GValue* value, GParamSpec* pspec)
{
    WebKitMediaSrc* source = WEBKIT_MEDIA_SRC(object);

    switch (propId) {
    case PROP_N_AUDIO:
        g_value_set_int(value, countStreamsOfType(source, WebCore::TrackPrivateBaseGStreamer::TrackType::Audio));
        break;
    case PROP_N_VIDEO:
        g_value_set_int(value, countStreamsOfType(source, WebCore::TrackPrivateBaseGStreamer::TrackType::Video));
        break;
    case PROP_N_TEXT:
        g_value_set_int(value, countStreamsOfType(source, WebCore::TrackPrivateBaseGStreamer::TrackType::Text));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, pspec);
    }
}

static GstStateChangeReturn webKitMediaSrcChangeState(GstElement* element, GstStateChange transition)
{
    WebKitMediaSrc* source = WEBKIT_MEDIA_SRC(element);
    switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        GST_DEBUG_OBJECT(source, "Downgrading to READY state, tearing down all streams...");
        while (!source->priv->streams.isEmpty())
            webKitMediaSrcTearDownStream(source, source->priv->streams.begin()->key);
        break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
        if (source->priv->isStarted()) {
            GST_FIXME_OBJECT(source, "Resuming state from READY -> PAUSED after a downgrade is not implemented. Expect failure.");
        }
        break;
    default:
        break;
    }
    return GST_ELEMENT_CLASS(webkit_media_src_parent_class)->change_state(element, transition);
}

static gboolean webKitMediaSrcSendEvent(GstElement* element, GstEvent* event)
{
    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_SEEK: {
        double rate;
        GstFormat format;
        GstSeekType startType;
        int64_t start;
        gst_event_parse_seek(event, &rate, &format, nullptr, &startType, &start, nullptr, nullptr);
        if (format != GST_FORMAT_TIME || startType != GST_SEEK_TYPE_SET) {
            GST_ERROR_OBJECT(element, "Rejecting unsupported seek event: %" GST_PTR_FORMAT, event);
            return false;
        }
        GST_DEBUG_OBJECT(element, "Handling seek event: %" GST_PTR_FORMAT, event);
        webKitMediaSrcSeek(WEBKIT_MEDIA_SRC(element), start, rate);
        return true;
    }
    default:
        GST_DEBUG_OBJECT(element, "Rejecting unsupported event: %" GST_PTR_FORMAT, event);
        return false;
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

    auto locker = GstObjectLocker(source);
    return g_strdup(source->priv->uri.get());
}

static gboolean webKitMediaSrcSetUri(GstURIHandler* handler, const gchar* uri, GError**)
{
    WebKitMediaSrc* source = WEBKIT_MEDIA_SRC(handler);

    if (GST_STATE(source) >= GST_STATE_PAUSED) {
        GST_ERROR_OBJECT(source, "URI can only be set in states < PAUSED");
        return false;
    }

    auto locker = GstObjectLocker(source);
    source->priv->uri = GUniquePtr<char>(g_strdup(uri));
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
