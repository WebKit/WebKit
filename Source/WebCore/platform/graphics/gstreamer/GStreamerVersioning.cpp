/*
 * Copyright (C) 2012 Igalia, S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "GStreamerVersioning.h"

#include "IntSize.h"
#include <wtf/UnusedParam.h>

void webkitGstObjectRefSink(GstObject* gstObject)
{
#ifdef GST_API_VERSION_1
    gst_object_ref_sink(gstObject);
#else
    gst_object_ref(gstObject);
    gst_object_sink(gstObject);
#endif
}

GstCaps* webkitGstGetPadCaps(GstPad* pad)
{
    if (!pad)
        return 0;

    GstCaps* caps;
#ifdef GST_API_VERSION_1
    caps = gst_pad_get_current_caps(pad);
    if (!caps)
        caps = gst_pad_query_caps(pad, 0);
#else
    caps = GST_PAD_CAPS(pad);
#endif
    return caps;
}

bool getVideoSizeAndFormatFromCaps(GstCaps* caps, WebCore::IntSize& size, GstVideoFormat& format, int& pixelAspectRatioNumerator, int& pixelAspectRatioDenominator, int& stride)
{
#ifdef GST_API_VERSION_1
    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, caps))
        return false;

    format = GST_VIDEO_INFO_FORMAT(&info);
    size.setWidth(GST_VIDEO_INFO_WIDTH(&info));
    size.setHeight(GST_VIDEO_INFO_HEIGHT(&info));
    pixelAspectRatioNumerator = GST_VIDEO_INFO_PAR_N(&info);
    pixelAspectRatioDenominator = GST_VIDEO_INFO_PAR_D(&info);
    stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
#else
    gint width, height;
    if (!GST_IS_CAPS(caps) || !gst_caps_is_fixed(caps)
        || !gst_video_format_parse_caps(caps, &format, &width, &height)
        || !gst_video_parse_caps_pixel_aspect_ratio(caps, &pixelAspectRatioNumerator,
                                                    &pixelAspectRatioDenominator))
        return false;
    size.setWidth(width);
    size.setHeight(height);
    stride = size.width() * 4;
#endif

    return true;
}

GstBuffer* createGstBuffer(GstBuffer* buffer)
{
#ifndef GST_API_VERSION_1
    GstBuffer* newBuffer = gst_buffer_try_new_and_alloc(GST_BUFFER_SIZE(buffer));
#else
    gsize bufferSize = gst_buffer_get_size(buffer);
    GstBuffer* newBuffer = gst_buffer_new_and_alloc(bufferSize);
#endif

    if (!newBuffer)
        return 0;

#ifndef GST_API_VERSION_1
    gst_buffer_copy_metadata(newBuffer, buffer, static_cast<GstBufferCopyFlags>(GST_BUFFER_COPY_ALL));
#else
    gst_buffer_copy_into(newBuffer, buffer, static_cast<GstBufferCopyFlags>(GST_BUFFER_COPY_METADATA), 0, bufferSize);
#endif
    return newBuffer;
}

void setGstElementClassMetadata(GstElementClass* elementClass, const char* name, const char* longName, const char* description, const char* author)
{
#ifdef GST_API_VERSION_1
    gst_element_class_set_metadata(elementClass, name, longName, description, author);
#else
    gst_element_class_set_details_simple(elementClass, name, longName, description, author);
#endif
}

bool gstObjectIsFloating(GstObject* gstObject)
{
#ifdef GST_API_VERSION_1
    return g_object_is_floating(G_OBJECT(gstObject));
#else
    return GST_OBJECT_IS_FLOATING(gstObject);
#endif
}

void notifyGstTagsOnPad(GstElement* element, GstPad* pad, GstTagList* tags)
{
#ifdef GST_API_VERSION_1
    UNUSED_PARAM(element);
    gst_pad_push_event(GST_PAD_CAST(pad), gst_event_new_tag(tags));
#else
    gst_element_found_tags_for_pad(element, pad, tags);
#endif
}
