/*
 * Copyright (C) 2013 Cable Television Laboratories, Inc.
 * Copyright (C) 2021 Igalia S.L.
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
#include "TextCombinerPadGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "TextCombinerGStreamer.h"
#include <wtf/glib/WTFGType.h>

struct _WebKitTextCombinerPadPrivate {
    GRefPtr<GstTagList> tags;
    GRefPtr<GstPad> innerCombinerPad;
    bool shouldProcessStickyEvents { true };
};

enum {
    PROP_PAD_0,
    PROP_PAD_TAGS,
    PROP_INNER_COMBINER_PAD,
    N_PROPERTIES,
};

static GParamSpec* sObjProperties[N_PROPERTIES] = { nullptr, };

#define webkit_text_combiner_pad_parent_class parent_class
WEBKIT_DEFINE_TYPE(WebKitTextCombinerPad, webkit_text_combiner_pad, GST_TYPE_GHOST_PAD);

static gboolean webkitTextCombinerPadEvent(GstPad* pad, GstObject* parent, GstEvent* event)
{
    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_TAG: {
        auto* combinerPad = WEBKIT_TEXT_COMBINER_PAD(pad);
        GstTagList* tags;
        gst_event_parse_tag(event, &tags);
        ASSERT(tags);

        {
            auto locker = GstObjectLocker(pad);
            if (!combinerPad->priv->tags)
                combinerPad->priv->tags = adoptGRef(gst_tag_list_copy(tags));
            else
                gst_tag_list_insert(combinerPad->priv->tags.get(), tags, GST_TAG_MERGE_REPLACE);
        }

        g_object_notify_by_pspec(G_OBJECT(pad), sObjProperties[PROP_PAD_TAGS]);
        break;
    }
    default:
        break;
    }
    return gst_pad_event_default(pad, parent, event);
}

static GstFlowReturn webkitTextCombinerPadChain(GstPad* pad, GstObject* parent, GstBuffer* buffer)
{
    auto* combinerPad = WEBKIT_TEXT_COMBINER_PAD(pad);

    if (combinerPad->priv->shouldProcessStickyEvents) {
        gst_pad_sticky_events_foreach(pad, [](GstPad* pad, GstEvent** event, gpointer) -> gboolean {
            if (GST_EVENT_TYPE(*event) != GST_EVENT_CAPS)
                return TRUE;

            auto* combinerPad = WEBKIT_TEXT_COMBINER_PAD(pad);
            auto parent = adoptGRef(gst_pad_get_parent(pad));
            GstCaps* caps;
            gst_event_parse_caps(*event, &caps);
            combinerPad->priv->shouldProcessStickyEvents = false;
            webKitTextCombinerHandleCaps(WEBKIT_TEXT_COMBINER(parent.get()), pad, caps);
            return FALSE;
        }, nullptr);
    }

    return gst_proxy_pad_chain_default(pad, parent, buffer);
}

static void webkitTextCombinerPadGetProperty(GObject* object, unsigned propertyId, GValue* value, GParamSpec* pspec)
{
    auto* pad = WEBKIT_TEXT_COMBINER_PAD(object);
    switch (propertyId) {
    case PROP_PAD_TAGS: {
        auto locker = GstObjectLocker(object);
        if (pad->priv->tags)
            g_value_take_boxed(value, gst_tag_list_copy(pad->priv->tags.get()));
        break;
    }
    case PROP_INNER_COMBINER_PAD: {
        auto locker = GstObjectLocker(object);
        g_value_set_object(value, pad->priv->innerCombinerPad.get());
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkitTextCombinerPadSetProperty(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    auto* pad = WEBKIT_TEXT_COMBINER_PAD(object);
    switch (propertyId) {
    case PROP_INNER_COMBINER_PAD: {
        auto locker = GstObjectLocker(object);
        pad->priv->innerCombinerPad = adoptGRef(GST_PAD_CAST(g_value_get_object(value)));
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkitTextCombinerPadConstructed(GObject* object)
{
    GST_CALL_PARENT(G_OBJECT_CLASS, constructed, (object));
    gst_ghost_pad_construct(GST_GHOST_PAD(object));
    gst_pad_set_event_function(GST_PAD_CAST(object), webkitTextCombinerPadEvent);
    gst_pad_set_chain_function(GST_PAD_CAST(object), webkitTextCombinerPadChain);
}

static void webkit_text_combiner_pad_class_init(WebKitTextCombinerPadClass* klass)
{
    auto* gobjectClass = G_OBJECT_CLASS(klass);

    gobjectClass->constructed = webkitTextCombinerPadConstructed;
    gobjectClass->get_property = GST_DEBUG_FUNCPTR(webkitTextCombinerPadGetProperty);
    gobjectClass->set_property = GST_DEBUG_FUNCPTR(webkitTextCombinerPadSetProperty);

    sObjProperties[PROP_PAD_TAGS] =
        g_param_spec_boxed("tags", "Tags", "The currently active tags on the pad", GST_TYPE_TAG_LIST,
            static_cast<GParamFlags>(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    sObjProperties[PROP_INNER_COMBINER_PAD] =
        g_param_spec_object("inner-combiner-pad", "Internal Combiner Pad", "The internal funnel (or concat) pad associated with this pad", GST_TYPE_PAD,
            static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_properties(gobjectClass, N_PROPERTIES, sObjProperties);
}

GstPad* webKitTextCombinerPadLeakInternalPadRef(WebKitTextCombinerPad* pad)
{
    return pad->priv->innerCombinerPad.leakRef();
}

#endif
