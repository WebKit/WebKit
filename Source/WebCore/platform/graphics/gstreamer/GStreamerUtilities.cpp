/*
 *  Copyright (C) 2012, 2015, 2016 Igalia S.L
 *  Copyright (C) 2015, 2016 Metrological Group B.V.
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

#include "GRefPtrGStreamer.h"
#include "GstAllocatorFastMalloc.h"
#include "IntSize.h"

#include <gst/audio/audio-info.h>
#include <gst/gst.h>
#include <wtf/glib/GUniquePtr.h>

#if ENABLE(VIDEO_TRACK) && USE(GSTREAMER_MPEGTS)
#define GST_USE_UNSTABLE_API
#include <gst/mpegts/mpegts.h>
#undef GST_USE_UNSTABLE_API
#endif

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

    gst_video_info_init(&info);
    if (!gst_video_info_from_caps(&info, caps))
        return false;

    format = GST_VIDEO_INFO_FORMAT(&info);
    size.setWidth(GST_VIDEO_INFO_WIDTH(&info));
    size.setHeight(GST_VIDEO_INFO_HEIGHT(&info));
    pixelAspectRatioNumerator = GST_VIDEO_INFO_PAR_N(&info);
    pixelAspectRatioDenominator = GST_VIDEO_INFO_PAR_D(&info);
    stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);

    return true;
}

bool getSampleVideoInfo(GstSample* sample, GstVideoInfo& videoInfo)
{
    if (!GST_IS_SAMPLE(sample))
        return false;

    GstCaps* caps = gst_sample_get_caps(sample);
    if (!caps)
        return false;

    gst_video_info_init(&videoInfo);
    if (!gst_video_info_from_caps(&videoInfo, caps))
        return false;

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

void mapGstBuffer(GstBuffer* buffer, uint32_t flags)
{
    GstMapInfo* mapInfo = static_cast<GstMapInfo*>(fastMalloc(sizeof(GstMapInfo)));
    if (!gst_buffer_map(buffer, mapInfo, static_cast<GstMapFlags>(flags))) {
        fastFree(mapInfo);
        gst_buffer_unref(buffer);
        return;
    }

    GstMiniObject* miniObject = reinterpret_cast<GstMiniObject*>(buffer);
    gst_mini_object_set_qdata(miniObject, g_quark_from_static_string(webkitGstMapInfoQuarkString), mapInfo, nullptr);
}

void unmapGstBuffer(GstBuffer* buffer)
{
    GstMiniObject* miniObject = reinterpret_cast<GstMiniObject*>(buffer);
    GstMapInfo* mapInfo = static_cast<GstMapInfo*>(gst_mini_object_steal_qdata(miniObject, g_quark_from_static_string(webkitGstMapInfoQuarkString)));

    if (!mapInfo)
        return;

    gst_buffer_unmap(buffer, mapInfo);
    fastFree(mapInfo);
}

bool initializeGStreamer()
{
    if (gst_is_initialized())
        return true;

    GUniqueOutPtr<GError> error;
    // FIXME: We should probably pass the arguments from the command line.
    bool gstInitialized = gst_init_check(nullptr, nullptr, &error.outPtr());
    ASSERT_WITH_MESSAGE(gstInitialized, "GStreamer initialization failed: %s", error ? error->message : "unknown error occurred");

    if (isFastMallocEnabled()) {
        const char* disableFastMalloc = getenv("WEBKIT_GST_DISABLE_FAST_MALLOC");
        if (!disableFastMalloc || !strcmp(disableFastMalloc, "0"))
            gst_allocator_set_default(GST_ALLOCATOR(g_object_new(gst_allocator_fast_malloc_get_type(), nullptr)));
    }

#if ENABLE(VIDEO_TRACK) && USE(GSTREAMER_MPEGTS)
    if (gstInitialized)
        gst_mpegts_initialize();
#endif

    return gstInitialized;
}

unsigned getGstPlayFlag(const char* nick)
{
    static GFlagsClass* flagsClass = static_cast<GFlagsClass*>(g_type_class_ref(g_type_from_name("GstPlayFlags")));
    ASSERT(flagsClass);

    GFlagsValue* flag = g_flags_get_value_by_nick(flagsClass, nick);
    if (!flag)
        return 0;

    return flag->value;
}

// Convert a MediaTime in seconds to a GstClockTime. Note that we can get MediaTime objects with a time scale that isn't a GST_SECOND, since they can come to
// us through the internal testing API, the DOM and internally. It would be nice to assert the format of the incoming time, but all the media APIs assume time
// is passed around in fractional seconds, so we'll just have to assume the same.
uint64_t toGstUnsigned64Time(const MediaTime& mediaTime)
{
    MediaTime time = mediaTime.toTimeScale(GST_SECOND);
    if (time.isInvalid())
        return GST_CLOCK_TIME_NONE;
    return time.timeValue();
}

bool gstRegistryHasElementForMediaType(GList* elementFactories, const char* capsString)
{
    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_from_string(capsString));
    GList* candidates = gst_element_factory_list_filter(elementFactories, caps.get(), GST_PAD_SINK, false);
    bool result = candidates;

    gst_plugin_feature_list_free(candidates);
    return result;
}

}

#endif // USE(GSTREAMER)
