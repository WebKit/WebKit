/*
 * Copyright (C) 2013 Cable Television Laboratories, Inc.
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
#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "TextSinkGStreamer.h"

GST_DEBUG_CATEGORY_STATIC(webkitTextSinkDebug);
#define GST_CAT_DEFAULT webkitTextSinkDebug

#define webkit_text_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(WebKitTextSink, webkit_text_sink, GST_TYPE_APP_SINK,
    GST_DEBUG_CATEGORY_INIT(webkitTextSinkDebug, "webkittextsink", 0,
        "webkit text sink"));

enum {
    Prop0,
    PropSync,
    PropLast
};

static void webkit_text_sink_init(WebKitTextSink* sink)
{
    /* We want to get cues as quickly as possible so WebKit has time to handle them,
     * and we don't want cues to block when they come in the wrong order. */
    gst_base_sink_set_sync(GST_BASE_SINK(sink), false);
}

static void webkitTextSinkGetProperty(GObject*, guint /* propertyId */,
    GValue*, GParamSpec*)
{
    /* Do nothing with PropSync */
}

static void webkitTextSinkSetProperty(GObject*, guint /* propertyId */,
    const GValue*, GParamSpec*)
{
    /* Do nothing with PropSync */
}

static gboolean webkitTextSinkQuery(GstElement *element, GstQuery *query)
{
    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_DURATION:
    case GST_QUERY_POSITION:
        /* Ignore duration and position because we don't want the seek bar to be
         * based on where the cues are. */
        return false;
    default:
        WebKitTextSink* sink = WEBKIT_TEXT_SINK(element);
        GstElement* parent = GST_ELEMENT(&sink->parent);
        return GST_ELEMENT_CLASS(parent_class)->query(parent, query);
    }
}

static void webkit_text_sink_class_init(WebKitTextSinkClass* klass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(klass);
    GstElementClass* elementClass = GST_ELEMENT_CLASS(klass);

    gst_element_class_set_metadata(elementClass, "WebKit text sink", "Generic",
        "An appsink that ignores the sync property and position and duration queries",
        "Brendan Long <b.long@cablelabs.com>");

    gobjectClass->get_property = GST_DEBUG_FUNCPTR(webkitTextSinkGetProperty);
    gobjectClass->set_property = GST_DEBUG_FUNCPTR(webkitTextSinkSetProperty);
    elementClass->query = GST_DEBUG_FUNCPTR(webkitTextSinkQuery);

    /* Override "sync" so playsink doesn't mess with our appsink */
    g_object_class_override_property(gobjectClass, PropSync, "sync");
}

GstElement* webkitTextSinkNew()
{
    return GST_ELEMENT(g_object_new(WEBKIT_TYPE_TEXT_SINK, nullptr));
}

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
