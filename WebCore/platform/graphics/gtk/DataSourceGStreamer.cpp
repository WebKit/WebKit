/*
 *  Copyright (C) 2009 Igalia S.L
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
#include "DataSourceGStreamer.h"

#include <gio/gio.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/pbutils/missing-plugins.h>

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE("src",
                                                                   GST_PAD_SRC,
                                                                   GST_PAD_ALWAYS,
                                                                   GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC(webkit_data_src_debug);
#define GST_CAT_DEFAULT webkit_data_src_debug

static void webkit_data_src_uri_handler_init(gpointer g_iface,
                                             gpointer iface_data);

static void webkit_data_src_finalize(WebkitDataSrc* src);
static GstStateChangeReturn webkit_data_src_change_state(GstElement* element,
                                                         GstStateChange transition);

static const GInterfaceInfo urihandler_info = {
    webkit_data_src_uri_handler_init,
    0, 0
};


static void _do_init(GType datasrc_type)
{
    GST_DEBUG_CATEGORY_INIT(webkit_data_src_debug, "webkit_data_src", 0, "datasrc element");
    g_type_add_interface_static(datasrc_type, GST_TYPE_URI_HANDLER,
                                &urihandler_info);
}

GST_BOILERPLATE_FULL(WebkitDataSrc, webkit_data_src, GstBin, GST_TYPE_BIN, _do_init);

static void webkit_data_src_base_init(gpointer klass)
{
    GstElementClass* element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_pad_template(element_class,
                                       gst_static_pad_template_get(&src_template));
    gst_element_class_set_details_simple(element_class, (gchar*) "WebKit data source element",
                                         (gchar*) "Source",
                                         (gchar*) "Handles data: uris",
                                         (gchar*) "Philippe Normand <pnormand@igalia.com>");

}

static void webkit_data_src_class_init(WebkitDataSrcClass* klass)
{
    GObjectClass* oklass = G_OBJECT_CLASS(klass);
    GstElementClass* eklass = GST_ELEMENT_CLASS(klass);

    oklass->finalize = (GObjectFinalizeFunc) webkit_data_src_finalize;
    eklass->change_state = webkit_data_src_change_state;
}


static gboolean webkit_data_src_reset(WebkitDataSrc* src)
{
    GstPad* targetpad;

    if (src->kid) {
        gst_element_set_state(src->kid, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(src), src->kid);
    }

    src->kid = gst_element_factory_make("giostreamsrc", "streamsrc");
    if (!src->kid) {
        GST_ERROR_OBJECT(src, "Failed to create giostreamsrc");
        return FALSE;
    }

    gst_bin_add(GST_BIN(src), src->kid);

    targetpad = gst_element_get_static_pad(src->kid, "src");
    gst_ghost_pad_set_target(GST_GHOST_PAD(src->pad), targetpad);
    gst_object_unref(targetpad);

    return TRUE;
}

static void webkit_data_src_init(WebkitDataSrc* src,
                                 WebkitDataSrcClass* g_class)
{
    GstPadTemplate* pad_template = gst_static_pad_template_get(&src_template);
    src->pad = gst_ghost_pad_new_no_target_from_template("src",
                                                         pad_template);

    gst_element_add_pad(GST_ELEMENT(src), src->pad);

    webkit_data_src_reset(src);
}

static void webkit_data_src_finalize(WebkitDataSrc* src)
{
    g_free(src->uri);

    if (src->kid) {
        GST_DEBUG_OBJECT(src, "Removing giostreamsrc element");
        gst_element_set_state(src->kid, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(src), src->kid);
        src->kid = 0;
    }

    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, ((GObject* )(src)));
}

static GstStateChangeReturn webkit_data_src_change_state(GstElement* element, GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    WebkitDataSrc* src = WEBKIT_DATA_SRC(element);

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        if (!src->kid) {
            gst_element_post_message(element,
                                     gst_missing_element_message_new(element, "giostreamsrc"));
            GST_ELEMENT_ERROR(src, CORE, MISSING_PLUGIN, (0), ("no giostreamsrc"));
            return GST_STATE_CHANGE_FAILURE;
        }
        break;
    default:
        break;
    }

    ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (G_UNLIKELY(ret == GST_STATE_CHANGE_FAILURE))
        return ret;

    // Downwards state change code should be here, after chaining up
    // to the parent class.

    return ret;
}

/*** GSTURIHANDLER INTERFACE *************************************************/

static GstURIType webkit_data_src_uri_get_type(void)
{
    return GST_URI_SRC;
}

static gchar** webkit_data_src_uri_get_protocols(void)
{
    static gchar* protocols[] = {(gchar*) "data", 0 };

    return protocols;
}

static const gchar* webkit_data_src_uri_get_uri(GstURIHandler* handler)
{
    WebkitDataSrc* src = WEBKIT_DATA_SRC(handler);

    return src->uri;
}

static gboolean webkit_data_src_uri_set_uri(GstURIHandler* handler, const gchar* uri)
{
    WebkitDataSrc* src = WEBKIT_DATA_SRC(handler);

    // URI as defined in RFC2397:
    // "data:" [ mediatype ] [ ";base64" ] "," data
    // we parse URIs like this one:
    // data:audio/3gpp;base64,AA...

    gchar** scheme_and_remains = g_strsplit(uri, ":", 2);
    gchar** mime_type_and_options = g_strsplit(scheme_and_remains[1], ";", 0);
    gint options_size = g_strv_length(mime_type_and_options);
    gchar* data = 0;
    gchar* mime_type = 0;
    gint ret = FALSE;

    // we require uris with a specified mime-type and base64-encoded
    // data. It doesn't make much sense anyway to play plain/text data
    // with very few allowed characters (as per the RFC).

    if (GST_STATE(src) >= GST_STATE_PAUSED) {
        GST_ERROR_OBJECT(src, "Element already configured. Reset it and retry");
    } else if (!options_size)
        GST_ERROR_OBJECT(src, "A mime-type is needed in %s", uri);
    else {
        mime_type = mime_type_and_options[0];
        data = mime_type_and_options[options_size-1];

        guchar* decoded_data = 0;
        gsize decoded_size;

        if (!g_str_has_prefix(data, "base64"))
            GST_ERROR_OBJECT(src, "Data has to be base64-encoded in %s", uri);
        else {
            decoded_data = g_base64_decode(data+7, &decoded_size);
            GInputStream* stream = g_memory_input_stream_new_from_data(decoded_data,
                                                                       decoded_size,
                                                                       g_free);
            g_object_set(src->kid, "stream", stream, 0);
            g_object_unref(stream);

            if (src->uri) {
                g_free(src->uri);
                src->uri = 0;
            }

            src->uri = g_strdup(uri);
            ret = TRUE;
        }
    }

    g_strfreev(scheme_and_remains);
    g_strfreev(mime_type_and_options);
    return ret;
}

static void webkit_data_src_uri_handler_init(gpointer g_iface, gpointer iface_data)
{
    GstURIHandlerInterface* iface = (GstURIHandlerInterface *) g_iface;

    iface->get_type = webkit_data_src_uri_get_type;
    iface->get_protocols = webkit_data_src_uri_get_protocols;
    iface->get_uri = webkit_data_src_uri_get_uri;
    iface->set_uri = webkit_data_src_uri_set_uri;
}
