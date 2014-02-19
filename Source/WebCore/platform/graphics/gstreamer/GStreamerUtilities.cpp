/*
 *  Copyright (C) 2012 Igalia S.L
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

#if USE(GSTREAMER)
#include "GStreamerUtilities.h"

#include "IntSize.h"

#include <gst/audio/audio.h>
#include <gst/gst.h>
#include <wtf/gobject/GUniquePtr.h>

namespace WebCore {

const char* webkitGstMapInfoQuarkString = "webkit-gst-map-info";

GstPad* webkitGstGhostPadFromStaticTemplate(GstStaticPadTemplate* staticPadTemplate, const gchar* name, GstPad* target)
{
    GstPad* pad;
    GstPadTemplate* padTemplate = gst_static_pad_template_get(staticPadTemplate);

    if (target)
        pad = gst_ghost_pad_new_from_template(name, target, padTemplate);
    else
        pad = gst_ghost_pad_new_no_target_from_template(name, padTemplate);

    gst_object_unref(padTemplate);

    return pad;
}

#if ENABLE(VIDEO)
bool getVideoSizeAndFormatFromCaps(GstCaps* caps, WebCore::IntSize& size, GstVideoFormat& format, int& pixelAspectRatioNumerator, int& pixelAspectRatioDenominator, int& stride)
{
    GstVideoInfo info;

    if (!gst_caps_is_fixed(caps) || !gst_video_info_from_caps(&info, caps))
        return false;

    format = GST_VIDEO_INFO_FORMAT(&info);
    size.setWidth(GST_VIDEO_INFO_WIDTH(&info));
    size.setHeight(GST_VIDEO_INFO_HEIGHT(&info));
    pixelAspectRatioNumerator = GST_VIDEO_INFO_PAR_N(&info);
    pixelAspectRatioDenominator = GST_VIDEO_INFO_PAR_D(&info);
    stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);

    return true;
}
#endif

GstBuffer* createGstBuffer(GstBuffer* buffer)
{
    gsize bufferSize = gst_buffer_get_size(buffer);
    GstBuffer* newBuffer = gst_buffer_new_and_alloc(bufferSize);

    if (!newBuffer)
        return 0;

    gst_buffer_copy_into(newBuffer, buffer, static_cast<GstBufferCopyFlags>(GST_BUFFER_COPY_METADATA), 0, bufferSize);
    return newBuffer;
}

GstBuffer* createGstBufferForData(const char* data, int length)
{
    GstBuffer* buffer = gst_buffer_new_and_alloc(length);

    gst_buffer_fill(buffer, 0, data, length);

    return buffer;
}

char* getGstBufferDataPointer(GstBuffer* buffer)
{
    GstMiniObject* miniObject = reinterpret_cast<GstMiniObject*>(buffer);
    GstMapInfo* mapInfo = static_cast<GstMapInfo*>(gst_mini_object_get_qdata(miniObject, g_quark_from_static_string(webkitGstMapInfoQuarkString)));
    return reinterpret_cast<char*>(mapInfo->data);
}

void mapGstBuffer(GstBuffer* buffer)
{
    GstMapInfo* mapInfo = g_slice_new(GstMapInfo);
    if (!gst_buffer_map(buffer, mapInfo, GST_MAP_WRITE)) {
        g_slice_free(GstMapInfo, mapInfo);
        gst_buffer_unref(buffer);
        return;
    }

    GstMiniObject* miniObject = reinterpret_cast<GstMiniObject*>(buffer);
    gst_mini_object_set_qdata(miniObject, g_quark_from_static_string(webkitGstMapInfoQuarkString), mapInfo, 0);
}

void unmapGstBuffer(GstBuffer* buffer)
{
    GstMiniObject* miniObject = reinterpret_cast<GstMiniObject*>(buffer);
    GstMapInfo* mapInfo = static_cast<GstMapInfo*>(gst_mini_object_steal_qdata(miniObject, g_quark_from_static_string(webkitGstMapInfoQuarkString)));

    if (!mapInfo)
        return;

    gst_buffer_unmap(buffer, mapInfo);
    g_slice_free(GstMapInfo, mapInfo);
}

bool initializeGStreamer()
{
#if GST_CHECK_VERSION(0, 10, 31)
    if (gst_is_initialized())
        return true;
#endif

    GUniqueOutPtr<GError> error;
    // FIXME: We should probably pass the arguments from the command line.
    bool gstInitialized = gst_init_check(0, 0, &error.outPtr());
    ASSERT_WITH_MESSAGE(gstInitialized, "GStreamer initialization failed: %s", error ? error->message : "unknown error occurred");
    return gstInitialized;
}

unsigned getGstPlaysFlag(const char* nick)
{
    static GFlagsClass* flagsClass = static_cast<GFlagsClass*>(g_type_class_ref(g_type_from_name("GstPlayFlags")));
    ASSERT(flagsClass);

    GFlagsValue* flag = g_flags_get_value_by_nick(flagsClass, nick);
    if (!flag)
        return 0;

    return flag->value;
}

}

#endif // USE(GSTREAMER)
