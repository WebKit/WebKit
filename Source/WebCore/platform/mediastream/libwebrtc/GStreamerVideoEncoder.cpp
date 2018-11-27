/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L. All rights reserved.
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

/* NOTE: This file respects GStreamer coding style as we might want to upstream
 * that element in the future */

#include "config.h"

#if ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
#include "GStreamerVideoEncoder.h"

GST_DEBUG_CATEGORY (gst_webrtcenc_debug);
#define GST_CAT_DEFAULT gst_webrtcenc_debug

#define KBIT_TO_BIT 1024

static GstStaticPadTemplate sinkTemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(ANY);"));

static GstStaticPadTemplate srcTemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264;video/x-vp8"));

typedef void (*SetBitrateFunc) (GObject * encoder, const gchar * propname,
    gint bitrate);
typedef void (*SetupEncoder) (GObject * encoder);
typedef struct
{
  gboolean avalaible;
  GstCaps *caps;
  const gchar *name;
  const gchar *parser_name;
  GstCaps *encoded_format;
  SetBitrateFunc setBitrate;
  SetupEncoder setupEncoder;
  const gchar *bitrate_propname;
  const gchar *keyframe_interval_propname;
} EncoderDefinition;

typedef enum
{
  ENCODER_NONE = 0,
  ENCODER_X264,
  ENCODER_OPENH264,
  ENCODER_VP8,
  ENCODER_LAST,
} EncoderId;

EncoderDefinition encoders[ENCODER_LAST] = {
  FALSE,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};

typedef struct
{
  EncoderId encoderId;
  GstElement *encoder;
  GstElement *parser;
  GstElement *capsfilter;
  guint bitrate;
} GstWebrtcVideoEncoderPrivate;

/*  *INDENT-OFF* */
G_DEFINE_TYPE_WITH_PRIVATE (GstWebrtcVideoEncoder, gst_webrtc_video_encoder,
    GST_TYPE_BIN)
#define PRIV(self) ((GstWebrtcVideoEncoderPrivate*)gst_webrtc_video_encoder_get_instance_private(self))
/*  *INDENT-ON* */

enum
{
  PROP_0,
  PROP_FORMAT,
  PROP_ENCODER,
  PROP_BITRATE,
  PROP_KEYFRAME_INTERVAL,
  N_PROPS
};

static void
gst_webrtc_video_encoder_finalize (GObject * object)
{
  G_OBJECT_CLASS (gst_webrtc_video_encoder_parent_class)->finalize (object);
}

static void
gst_webrtc_video_encoder_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstWebrtcVideoEncoder *self = GST_WEBRTC_VIDEO_ENCODER (object);
  GstWebrtcVideoEncoderPrivate *priv = PRIV (self);

  switch (prop_id) {
    case PROP_FORMAT:
      if (priv->encoderId != ENCODER_NONE)
        g_value_set_boxed (value, encoders[priv->encoderId].caps);
      else
        g_value_set_boxed (value, NULL);
      break;
    case PROP_ENCODER:
      g_value_set_object (value, priv->encoder);
      break;
    case PROP_BITRATE:
      g_value_set_uint (value, priv->bitrate);
      break;
    case PROP_KEYFRAME_INTERVAL:
      if (priv->encoder)
        g_object_get_property (G_OBJECT (priv->encoder),
            encoders[priv->encoderId].keyframe_interval_propname, value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gst_webrtc_video_encoder_set_bitrate (GstWebrtcVideoEncoder * self,
    guint bitrate)
{
  GstWebrtcVideoEncoderPrivate *priv = PRIV (self);

  priv->bitrate = bitrate;
  if (priv->encoder) {
    encoders[priv->encoderId].setBitrate (G_OBJECT (priv->encoder),
        encoders[priv->encoderId].bitrate_propname, priv->bitrate);
  }
}

static void
gst_webrtc_video_encoder_set_format (GstWebrtcVideoEncoder * self,
    const GstCaps * caps)
{
  gint i;
  GstWebrtcVideoEncoderPrivate *priv = PRIV (self);
  g_return_if_fail (priv->encoderId == ENCODER_NONE);
  g_return_if_fail (caps);

  for (i = 1; i < ENCODER_LAST; i++) {
    if (encoders[i].avalaible
        && gst_caps_can_intersect (encoders[i].caps, caps)) {
      GstPad *tmppad;
      priv->encoderId = (EncoderId) i;
      priv->encoder = gst_element_factory_make (encoders[i].name, NULL);
      encoders[priv->encoderId].setupEncoder (G_OBJECT (priv->encoder));

      if (encoders[i].parser_name)
        priv->parser = gst_element_factory_make (encoders[i].parser_name, NULL);

      if (encoders[i].encoded_format) {
        priv->capsfilter = gst_element_factory_make ("capsfilter", NULL);
        g_object_set (priv->capsfilter, "caps", encoders[i].encoded_format,
            NULL);
      }

      gst_bin_add (GST_BIN (self), priv->encoder);

      tmppad = gst_element_get_static_pad (priv->encoder, "sink");
      gst_ghost_pad_set_target (GST_GHOST_PAD (GST_ELEMENT (self)->
              sinkpads->data), tmppad);
      gst_object_unref (tmppad);

      tmppad = gst_element_get_static_pad (priv->encoder, "src");
      if (priv->parser) {
        gst_bin_add (GST_BIN (self), priv->parser);
        gst_element_link (priv->encoder, priv->parser);
        gst_object_unref (tmppad);
        tmppad = gst_element_get_static_pad (priv->parser, "src");
      }

      if (priv->capsfilter) {
        GstPad *tmppad2 = gst_element_get_static_pad (priv->capsfilter, "sink");

        gst_bin_add (GST_BIN (self), priv->capsfilter);
        gst_pad_link (tmppad, tmppad2);
        gst_object_unref (tmppad);
        tmppad = gst_element_get_static_pad (priv->capsfilter, "src");
        gst_object_unref (tmppad2);
      }

      g_assert (gst_ghost_pad_set_target (GST_GHOST_PAD (GST_ELEMENT
                  (self)->srcpads->data), tmppad));
      gst_object_unref (tmppad);

      gst_webrtc_video_encoder_set_bitrate (self, priv->bitrate);
      return;
    }
  }

  GST_ERROR ("No encoder found for format %" GST_PTR_FORMAT, caps);
}

static void
gst_webrtc_video_encoder_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstWebrtcVideoEncoder *self = GST_WEBRTC_VIDEO_ENCODER (object);
  GstWebrtcVideoEncoderPrivate *priv = PRIV (self);

  switch (prop_id) {
    case PROP_FORMAT:
      gst_webrtc_video_encoder_set_format (self, gst_value_get_caps (value));
      break;
    case PROP_BITRATE:
      gst_webrtc_video_encoder_set_bitrate (self, g_value_get_uint (value));
      break;
    case PROP_KEYFRAME_INTERVAL:
      if (priv->encoder)
        g_object_set (priv->encoder,
            encoders[priv->encoderId].keyframe_interval_propname,
            g_value_get_uint (value), NULL);

      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
register_known_encoder (EncoderId encId, const gchar * name,
    const gchar * parser_name, const gchar * caps, const gchar * encoded_format,
    SetupEncoder setupEncoder, const gchar * bitrate_propname,
    SetBitrateFunc setBitrate, const gchar * keyframe_interval_propname)
{
  GstPluginFeature *feature =
      gst_registry_lookup_feature (gst_registry_get (), name);
  if (!feature) {
    GST_WARNING ("Could not find %s", name);
    encoders[encId].avalaible = FALSE;

    return;
  }
  gst_object_unref (feature);

  encoders[encId].avalaible = TRUE;
  encoders[encId].name = name;
  encoders[encId].parser_name = parser_name;
  encoders[encId].caps = gst_caps_from_string (caps);
  if (encoded_format)
    encoders[encId].encoded_format = gst_caps_from_string (encoded_format);
  else
    encoders[encId].encoded_format = NULL;
  encoders[encId].setupEncoder = setupEncoder;
  encoders[encId].bitrate_propname = bitrate_propname;
  encoders[encId].setBitrate = setBitrate;
  encoders[encId].keyframe_interval_propname = keyframe_interval_propname;
}

static void
setup_x264enc (GObject * encoder)
{
  gst_util_set_object_arg (encoder, "tune", "zerolatency");
}

static void
setup_openh264enc (GObject *)
{
}

static void
set_bitrate_kbit_per_sec (GObject * encoder, const gchar * prop_name,
    gint bitrate)
{
  g_object_set (encoder, prop_name, bitrate, NULL);
}

static void
gst_webrtc_video_encoder_class_init (GstWebrtcVideoEncoderClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_webrtcenc_debug, "webrtcencoder", 0,
      "Video encoder for WebRTC");

  object_class->finalize = gst_webrtc_video_encoder_finalize;
  object_class->get_property = gst_webrtc_video_encoder_get_property;
  object_class->set_property = gst_webrtc_video_encoder_set_property;

  g_object_class_install_property (object_class, PROP_FORMAT,
      g_param_spec_boxed ("format", "Format as caps",
          "Set the caps of the format to be used.",
          GST_TYPE_CAPS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (object_class, PROP_ENCODER,
      g_param_spec_object ("encoder", "The actual encoder element",
          "The encoder element", GST_TYPE_ELEMENT,
          (GParamFlags) (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (object_class, PROP_BITRATE,
      g_param_spec_uint ("bitrate", "Bitrate",
          "The bitrate in kbit per second", 0, G_MAXINT, 2048,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (object_class, PROP_KEYFRAME_INTERVAL,
      g_param_spec_uint ("keyframe-interval", "Keyframe interval",
          "The interval between keyframes", 0, G_MAXINT, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  register_known_encoder (ENCODER_X264, "x264enc", "h264parse", "video/x-h264",
      "video/x-h264,alignment=au,stream-format=byte-stream,profile=baseline",
      setup_x264enc, "bitrate", set_bitrate_kbit_per_sec, "key-int-max");
  register_known_encoder (ENCODER_OPENH264, "openh264enc", "h264parse",
      "video/x-h264",
      "video/x-h264,alignment=au,stream-format=byte-stream,profile=baseline",
      setup_openh264enc, "bitrate", set_bitrate_kbit_per_sec, "gop-size");
}

static void
gst_webrtc_video_encoder_init (GstWebrtcVideoEncoder * self)
{
  GstWebrtcVideoEncoderPrivate *priv = PRIV (self);

  priv->encoderId = ENCODER_NONE;
  gst_element_add_pad (GST_ELEMENT (self),
      gst_ghost_pad_new_no_target_from_template ("sink",
          gst_static_pad_template_get (&sinkTemplate)));

  gst_element_add_pad (GST_ELEMENT (self),
      gst_ghost_pad_new_no_target_from_template ("src",
          gst_static_pad_template_get (&srcTemplate)));
}

#endif // ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
