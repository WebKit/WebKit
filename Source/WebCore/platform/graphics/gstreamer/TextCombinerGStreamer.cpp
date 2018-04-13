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
#include "TextCombinerGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(VIDEO_TRACK)

#include "GStreamerCommon.h"

static GstStaticPadTemplate sinkTemplate =
    GST_STATIC_PAD_TEMPLATE("sink_%u", GST_PAD_SINK, GST_PAD_REQUEST,
        GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate srcTemplate =
    GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS,
        GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC(webkitTextCombinerDebug);
#define GST_CAT_DEFAULT webkitTextCombinerDebug

#define webkit_text_combiner_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(WebKitTextCombiner, webkit_text_combiner, GST_TYPE_BIN,
    GST_DEBUG_CATEGORY_INIT(webkitTextCombinerDebug, "webkittextcombiner", 0,
        "webkit text combiner"));

enum {
    PROP_PAD_0,
    PROP_PAD_TAGS
};

#define WEBKIT_TYPE_TEXT_COMBINER_PAD webkit_text_combiner_pad_get_type()

#define WEBKIT_TEXT_COMBINER_PAD(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_TEXT_COMBINER_PAD, WebKitTextCombinerPad))
#define WEBKIT_TEXT_COMBINER_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_TEXT_COMBINER_PAD, WebKitTextCombinerPadClass))
#define WEBKIT_IS_TEXT_COMBINER_PAD(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_TEXT_COMBINER_PAD))
#define WEBKIT_IS_TEXT_COMBINER_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_TEXT_COMBINER_PAD))
#define WEBKIT_TEXT_COMBINER_PAD_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), WEBKIT_TYPE_TEXT_COMBINER_PAD, WebKitTextCombinerPadClass))

typedef struct _WebKitTextCombinerPad WebKitTextCombinerPad;
typedef struct _WebKitTextCombinerPadClass WebKitTextCombinerPadClass;

struct _WebKitTextCombinerPad {
    GstGhostPad parent;

    GstTagList* tags;
};

struct _WebKitTextCombinerPadClass {
    GstGhostPadClass parent;
};

G_DEFINE_TYPE(WebKitTextCombinerPad, webkit_text_combiner_pad, GST_TYPE_GHOST_PAD);

static gboolean webkitTextCombinerPadEvent(GstPad*, GstObject* parent, GstEvent*);

static void webkit_text_combiner_init(WebKitTextCombiner* combiner)
{
    combiner->funnel = gst_element_factory_make("funnel", nullptr);
    ASSERT(combiner->funnel);

    gboolean ret = gst_bin_add(GST_BIN(combiner), combiner->funnel);
    UNUSED_PARAM(ret);
    ASSERT(ret);

    GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(combiner->funnel, "src"));
    ASSERT(pad);

    ret = gst_element_add_pad(GST_ELEMENT(combiner), gst_ghost_pad_new("src", pad.get()));
    ASSERT(ret);
}

static void webkit_text_combiner_pad_init(WebKitTextCombinerPad* pad)
{
    gst_pad_set_event_function(GST_PAD(pad), webkitTextCombinerPadEvent);
}

static void webkitTextCombinerPadFinalize(GObject* object)
{
    WebKitTextCombinerPad* pad = WEBKIT_TEXT_COMBINER_PAD(object);
    if (pad->tags)
        gst_tag_list_unref(pad->tags);
    G_OBJECT_CLASS(webkit_text_combiner_pad_parent_class)->finalize(object);
}

static void webkitTextCombinerPadGetProperty(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitTextCombinerPad* pad = WEBKIT_TEXT_COMBINER_PAD(object);
    switch (propertyId) {
    case PROP_PAD_TAGS:
        GST_OBJECT_LOCK(object);
        if (pad->tags)
            g_value_take_boxed(value, gst_tag_list_copy(pad->tags));
        GST_OBJECT_UNLOCK(object);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static gboolean webkitTextCombinerPadEvent(GstPad* pad, GstObject* parent, GstEvent* event)
{
    gboolean ret;
    UNUSED_PARAM(ret);
    WebKitTextCombiner* combiner = WEBKIT_TEXT_COMBINER(parent);
    WebKitTextCombinerPad* combinerPad = WEBKIT_TEXT_COMBINER_PAD(pad);
    ASSERT(combiner);

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_CAPS: {
        GstCaps* caps;
        gst_event_parse_caps(event, &caps);
        ASSERT(caps);

        GRefPtr<GstPad> target = adoptGRef(gst_ghost_pad_get_target(GST_GHOST_PAD(pad)));
        ASSERT(target);

        GRefPtr<GstElement> targetParent = adoptGRef(gst_pad_get_parent_element(target.get()));
        ASSERT(targetParent);

        GRefPtr<GstCaps> textCaps = adoptGRef(gst_caps_new_empty_simple("text/x-raw"));
        if (gst_caps_can_intersect(textCaps.get(), caps)) {
            /* Caps are plain text, put a WebVTT encoder between the ghostpad and
             * the funnel */
            if (targetParent.get() == combiner->funnel) {
                /* Setup a WebVTT encoder */
                GstElement* encoder = gst_element_factory_make("webvttenc", nullptr);
                ASSERT(encoder);

                ret = gst_bin_add(GST_BIN(combiner), encoder);
                ASSERT(ret);

                ret = gst_element_sync_state_with_parent(encoder);
                ASSERT(ret);

                /* Switch the ghostpad to target the WebVTT encoder */
                GRefPtr<GstPad> sinkPad = adoptGRef(gst_element_get_static_pad(encoder, "sink"));
                ASSERT(sinkPad);

                ret = gst_ghost_pad_set_target(GST_GHOST_PAD(pad), sinkPad.get());
                ASSERT(ret);

                /* Connect the WebVTT encoder to the funnel */
                GRefPtr<GstPad> srcPad = adoptGRef(gst_element_get_static_pad(encoder, "src"));
                ASSERT(srcPad);

                ret = GST_PAD_LINK_SUCCESSFUL(gst_pad_link(srcPad.get(), target.get()));
                ASSERT(ret);
            } /* else: pipeline is already correct */
        } else {
            /* Caps are not plain text, remove the WebVTT encoder */
            if (targetParent.get() != combiner->funnel) {
                /* Get the funnel sink pad */
                GRefPtr<GstPad> srcPad = adoptGRef(gst_element_get_static_pad(targetParent.get(), "src"));
                ASSERT(srcPad);

                GRefPtr<GstPad> sinkPad = adoptGRef(gst_pad_get_peer(srcPad.get()));
                ASSERT(sinkPad);

                /* Switch the ghostpad to target the funnel */
                ret = gst_ghost_pad_set_target(GST_GHOST_PAD(pad), sinkPad.get());
                ASSERT(ret);

                /* Remove the WebVTT encoder */
                ret = gst_bin_remove(GST_BIN(combiner), targetParent.get());
                ASSERT(ret);
            } /* else: pipeline is already correct */
        }
        break;
    }
    case GST_EVENT_TAG: {
        GstTagList* tags;
        gst_event_parse_tag(event, &tags);
        ASSERT(tags);

        GST_OBJECT_LOCK(pad);
        if (!combinerPad->tags)
            combinerPad->tags = gst_tag_list_copy(tags);
        else
            gst_tag_list_insert(combinerPad->tags, tags, GST_TAG_MERGE_REPLACE);
        GST_OBJECT_UNLOCK(pad);

        g_object_notify(G_OBJECT(pad), "tags");
        break;
    }
    default:
        break;
    }
    return gst_pad_event_default(pad, parent, event);
}

static GstPad* webkitTextCombinerRequestNewPad(GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
    gboolean ret;
    UNUSED_PARAM(ret);
    ASSERT(templ);

    WebKitTextCombiner* combiner = WEBKIT_TEXT_COMBINER(element);
    ASSERT(combiner);

    GstPad* pad = gst_element_request_pad(combiner->funnel, templ, name, caps);
    ASSERT(pad);

    GstPad* ghostPad = GST_PAD(g_object_new(WEBKIT_TYPE_TEXT_COMBINER_PAD, "direction", gst_pad_get_direction(pad), nullptr));
    ASSERT(ghostPad);

    ret = gst_ghost_pad_construct(GST_GHOST_PAD(ghostPad));
    ASSERT(ret);

    ret = gst_ghost_pad_set_target(GST_GHOST_PAD(ghostPad), pad);
    ASSERT(ret);

    ret = gst_pad_set_active(ghostPad, true);
    ASSERT(ret);

    ret = gst_element_add_pad(GST_ELEMENT(combiner), ghostPad);
    ASSERT(ret);
    return ghostPad;
}

static void webkitTextCombinerReleasePad(GstElement *element, GstPad *pad)
{
    WebKitTextCombiner* combiner = WEBKIT_TEXT_COMBINER(element);
    if (GRefPtr<GstPad> peer = adoptGRef(gst_pad_get_peer(pad))) {
        GRefPtr<GstElement> parent = adoptGRef(gst_pad_get_parent_element(peer.get()));
        ASSERT(parent);
        gst_element_release_request_pad(parent.get(), peer.get());
        if (parent.get() != combiner->funnel)
            gst_bin_remove(GST_BIN(combiner), parent.get());
    }

    gst_element_remove_pad(element, pad);
}

static void webkit_text_combiner_class_init(WebKitTextCombinerClass* klass)
{
    GstElementClass* elementClass = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_pad_template(elementClass, gst_static_pad_template_get(&sinkTemplate));
    gst_element_class_add_pad_template(elementClass, gst_static_pad_template_get(&srcTemplate));

    gst_element_class_set_metadata(elementClass, "WebKit text combiner", "Generic",
        "A funnel that accepts any caps, but converts plain text to WebVTT",
        "Brendan Long <b.long@cablelabs.com>");

    elementClass->request_new_pad =
        GST_DEBUG_FUNCPTR(webkitTextCombinerRequestNewPad);
    elementClass->release_pad =
        GST_DEBUG_FUNCPTR(webkitTextCombinerReleasePad);
}

static void webkit_text_combiner_pad_class_init(WebKitTextCombinerPadClass* klass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(klass);

    gobjectClass->finalize = GST_DEBUG_FUNCPTR(webkitTextCombinerPadFinalize);
    gobjectClass->get_property = GST_DEBUG_FUNCPTR(webkitTextCombinerPadGetProperty);

    g_object_class_install_property(gobjectClass, PROP_PAD_TAGS,
        g_param_spec_boxed("tags", "Tags", "The currently active tags on the pad", GST_TYPE_TAG_LIST,
            static_cast<GParamFlags>(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
}

GstElement* webkitTextCombinerNew()
{
    return GST_ELEMENT(g_object_new(WEBKIT_TYPE_TEXT_COMBINER, nullptr));
}

#endif // ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(VIDEO_TRACK)
