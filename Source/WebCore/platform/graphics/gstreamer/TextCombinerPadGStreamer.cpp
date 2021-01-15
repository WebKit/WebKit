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
};

enum {
    PROP_PAD_0,
    PROP_PAD_TAGS,
    PROP_INNER_COMBINER_PAD,
};

#define webkit_text_combiner_pad_parent_class parent_class
WEBKIT_DEFINE_TYPE(WebKitTextCombinerPad, webkit_text_combiner_pad, GST_TYPE_GHOST_PAD);

static gboolean webkitTextCombinerPadEvent(GstPad* pad, GstObject* parent, GstEvent* event)
{
    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_CAPS:
        webKitTextCombinerHandleCapsEvent(WEBKIT_TEXT_COMBINER(parent), pad, event);
        break;
    case GST_EVENT_TAG: {
        auto* combinerPad = WEBKIT_TEXT_COMBINER_PAD(pad);
        GstTagList* tags;
        gst_event_parse_tag(event, &tags);
        ASSERT(tags);

        GST_OBJECT_LOCK(pad);
        if (!combinerPad->priv->tags)
            combinerPad->priv->tags = adoptGRef(gst_tag_list_copy(tags));
        else
            gst_tag_list_insert(combinerPad->priv->tags.get(), tags, GST_TAG_MERGE_REPLACE);
        GST_OBJECT_UNLOCK(pad);

        g_object_notify(G_OBJECT(pad), "tags");
        break;
    }
    default:
        break;
    }
    return gst_pad_event_default(pad, parent, event);
}

static void webkitTextCombinerPadGetProperty(GObject* object, unsigned propertyId, GValue* value, GParamSpec* pspec)
{
    auto* pad = WEBKIT_TEXT_COMBINER_PAD(object);
    switch (propertyId) {
    case PROP_PAD_TAGS:
        GST_OBJECT_LOCK(object);
        if (pad->priv->tags)
            g_value_take_boxed(value, gst_tag_list_copy(pad->priv->tags.get()));
        GST_OBJECT_UNLOCK(object);
        break;
    case PROP_INNER_COMBINER_PAD:
        GST_OBJECT_LOCK(object);
        g_value_set_object(value, pad->priv->innerCombinerPad.get());
        GST_OBJECT_UNLOCK(object);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkitTextCombinerPadSetProperty(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    auto* pad = WEBKIT_TEXT_COMBINER_PAD(object);
    switch (propertyId) {
    case PROP_INNER_COMBINER_PAD:
        GST_OBJECT_LOCK(object);
        pad->priv->innerCombinerPad = adoptGRef(GST_PAD_CAST(g_value_get_object(value)));
        GST_OBJECT_UNLOCK(object);
        break;
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
}

static void webkit_text_combiner_pad_class_init(WebKitTextCombinerPadClass* klass)
{
    auto* gobjectClass = G_OBJECT_CLASS(klass);

    gobjectClass->constructed = webkitTextCombinerPadConstructed;
    gobjectClass->get_property = GST_DEBUG_FUNCPTR(webkitTextCombinerPadGetProperty);
    gobjectClass->set_property = GST_DEBUG_FUNCPTR(webkitTextCombinerPadSetProperty);

    g_object_class_install_property(gobjectClass, PROP_PAD_TAGS,
        g_param_spec_boxed("tags", "Tags", "The currently active tags on the pad", GST_TYPE_TAG_LIST,
            static_cast<GParamFlags>(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(gobjectClass, PROP_INNER_COMBINER_PAD,
        g_param_spec_object("inner-combiner-pad", "Internal Combiner Pad", "The internal funnel (or concat) pad associated with this pad", GST_TYPE_PAD,
            static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

GstPad* webKitTextCombinerPadLeakInternalPadRef(WebKitTextCombinerPad* pad)
{
    return pad->priv->innerCombinerPad.leakRef();
}

#endif
