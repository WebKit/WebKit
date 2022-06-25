/*
 * Copyright (C) 2013 Cable Television Laboratories, Inc.
 * Copyright (C) 2021 Igalia S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include "config.h"
#include "TextSinkGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "MediaPlayerPrivateGStreamer.h"
#include <gst/app/gstappsink.h>
#include <wtf/glib/WTFGType.h>

GST_DEBUG_CATEGORY_STATIC(webkitTextSinkDebug);
#define GST_CAT_DEFAULT webkitTextSinkDebug

using namespace WebCore;

struct _WebKitTextSinkPrivate {
    GRefPtr<GstElement> appSink;
    WeakPtr<MediaPlayerPrivateGStreamer> mediaPlayerPrivate;
    const char* streamId { nullptr };
};

#define webkit_text_sink_parent_class parent_class
WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitTextSink, webkit_text_sink, GST_TYPE_BIN,
    GST_DEBUG_CATEGORY_INIT(webkitTextSinkDebug, "webkittextsink", 0, "webkit text sink"))

static void webkitTextSinkHandleSample(WebKitTextSink* self, GRefPtr<GstSample>&& sample)
{
    auto* priv = self->priv;
    if (!priv->streamId) {
        auto pad = adoptGRef(gst_element_get_static_pad(priv->appSink.get(), "sink"));
        auto streamStartEvent = adoptGRef(gst_pad_get_sticky_event(pad.get(), GST_EVENT_STREAM_START, 0));

        if (streamStartEvent)
            gst_event_parse_stream_start(streamStartEvent.get(), &priv->streamId);
    }

    if (priv->streamId) {
        // Player private methods that interact with WebCore must run from the main thread. Things can be destroyed before that
        // code runs, including the text sink and priv, so pass everything in a safe way.
        callOnMainThread([mediaPlayerPrivate = WeakPtr<MediaPlayerPrivateGStreamer>(priv->mediaPlayerPrivate),
            streamId = priv->streamId, sample = WTFMove(sample)] {
            if (!mediaPlayerPrivate)
                return;
            mediaPlayerPrivate->handleTextSample(sample.get(), streamId);
        });
        return;
    }
    GST_WARNING_OBJECT(self, "Unable to handle sample with no stream start event.");
}

static void webkitTextSinkConstructed(GObject* object)
{
    GST_CALL_PARENT(G_OBJECT_CLASS, constructed, (object));

    auto* sink = WEBKIT_TEXT_SINK(object);
    auto* priv = sink->priv;

    priv->appSink = makeGStreamerElement("appsink", nullptr);
    gst_bin_add(GST_BIN_CAST(sink), priv->appSink.get());

    auto pad = adoptGRef(gst_element_get_static_pad(priv->appSink.get(), "sink"));
    gst_element_add_pad(GST_ELEMENT_CAST(sink), gst_ghost_pad_new("sink", pad.get()));

    auto textCaps = adoptGRef(gst_caps_new_empty_simple("application/x-subtitle-vtt"));
    g_object_set(priv->appSink.get(), "emit-signals", TRUE, "enable-last-sample", FALSE, "caps", textCaps.get(), nullptr);

    g_signal_connect(priv->appSink.get(), "new-sample", G_CALLBACK(+[](GstElement* appSink, WebKitTextSink* sink) -> GstFlowReturn {
        webkitTextSinkHandleSample(sink, adoptGRef(gst_app_sink_pull_sample(GST_APP_SINK(appSink))));
        return GST_FLOW_OK;
    }), sink);

    g_signal_connect(priv->appSink.get(), "new-preroll", G_CALLBACK(+[](GstElement* appSink, WebKitTextSink* sink) -> GstFlowReturn {
        webkitTextSinkHandleSample(sink, adoptGRef(gst_app_sink_pull_preroll(GST_APP_SINK(appSink))));
        return GST_FLOW_OK;
    }), sink);

    // We want to get cues as quickly as possible so WebKit has time to handle them,
    // and we don't want cues to block when they come in the wrong order.
    gst_base_sink_set_sync(GST_BASE_SINK_CAST(sink->priv->appSink.get()), false);
}

static gboolean webkitTextSinkQuery(GstElement* element, GstQuery* query)
{
    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_DURATION:
    case GST_QUERY_POSITION:
        // Ignore duration and position because we don't want the seek bar to be based on where the cues are.
        return false;
    default:
        return GST_CALL_PARENT_WITH_DEFAULT(GST_ELEMENT_CLASS, query, (element, query), FALSE);
    }
}

static void webkit_text_sink_class_init(WebKitTextSinkClass* klass)
{
    auto* gobjectClass = G_OBJECT_CLASS(klass);
    auto* elementClass = GST_ELEMENT_CLASS(klass);

    gst_element_class_set_metadata(elementClass, "WebKit text sink", GST_ELEMENT_FACTORY_KLASS_SINK,
        "WebKit's text sink collecting cues encoded in WebVTT by the WebKit text-combiner",
        "Brendan Long <b.long@cablelabs.com>");

    gobjectClass->constructed = GST_DEBUG_FUNCPTR(webkitTextSinkConstructed);
    elementClass->query = GST_DEBUG_FUNCPTR(webkitTextSinkQuery);
}

GstElement* webkitTextSinkNew(WeakPtr<MediaPlayerPrivateGStreamer>&& player)
{
    auto* element = GST_ELEMENT_CAST(g_object_new(WEBKIT_TYPE_TEXT_SINK, nullptr));
    auto* sink = WEBKIT_TEXT_SINK(element);
    ASSERT(isMainThread());
    sink->priv->mediaPlayerPrivate = WTFMove(player);
    return element;
}

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
