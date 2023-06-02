/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro  <alex@igalia.com>
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

#include "config.h"

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include "GStreamerVideoCapturer.h"

#include <wtf/glib/WTFGType.h>

GST_DEBUG_CATEGORY(webkit_video_capturer_debug);
#define GST_CAT_DEFAULT webkit_video_capturer_debug

namespace WebCore {

static void initializeVideoCapturerDebugCategory()
{
    ensureGStreamerInitialized();

    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_video_capturer_debug, "webkitvideocapturer", 0, "WebKit Video Capturer");
    });
}

GStreamerVideoCapturer::GStreamerVideoCapturer(GStreamerCaptureDevice device)
    : GStreamerCapturer(device, adoptGRef(gst_caps_new_empty_simple("video/x-raw")))
{
    initializeVideoCapturerDebugCategory();
}

GStreamerVideoCapturer::GStreamerVideoCapturer(const char* sourceFactory, CaptureDevice::DeviceType deviceType)
    : GStreamerCapturer(sourceFactory, adoptGRef(gst_caps_new_empty_simple("video/x-raw")), deviceType)
{
    initializeVideoCapturerDebugCategory();
}

GstElement* GStreamerVideoCapturer::createSource()
{
    auto* src = GStreamerCapturer::createSource();
    if (m_nodeAndFd) {
        auto& [node, fd] = *m_nodeAndFd;
        auto path = AtomString::number(node);
        // FIXME: The path property is deprecated in favor of target-object but the portal doesn't expose this object.
        g_object_set(m_src.get(), "path", path.string().ascii().data(), nullptr);
        g_object_set(m_src.get(), "fd", fd, nullptr);
    }
    return src;
}

GstElement* GStreamerVideoCapturer::createConverter()
{
    auto* bin = gst_bin_new(nullptr);
    auto* videoscale = makeGStreamerElement("videoscale", "videoscale");
    auto* videoconvert = makeGStreamerElement("videoconvert", nullptr);
    auto* videorate = makeGStreamerElement("videorate", "videorate");

    // https://gitlab.freedesktop.org/gstreamer/gst-plugins-base/issues/97#note_56575
    g_object_set(videorate, "drop-only", TRUE, "average-period", UINT64_C(1), nullptr);

    gst_bin_add_many(GST_BIN_CAST(bin), videoscale, videoconvert, videorate, nullptr);

    GstElement* head = videoscale;
    if (!isCapturingDisplay()) {
        m_videoSrcMIMETypeFilter = gst_element_factory_make("capsfilter", "mimetype-filter");
        head = m_videoSrcMIMETypeFilter.get();

        auto caps = adoptGRef(gst_caps_new_empty_simple("video/x-raw"));
        g_object_set(m_videoSrcMIMETypeFilter.get(), "caps", caps.get(), nullptr);

        auto* decodebin = makeGStreamerElement("decodebin3", nullptr);
        gst_bin_add_many(GST_BIN_CAST(bin), m_videoSrcMIMETypeFilter.get(), decodebin, nullptr);
        gst_element_link(m_videoSrcMIMETypeFilter.get(), decodebin);

        auto sinkPad = adoptGRef(gst_element_get_static_pad(videoscale, "sink"));
        g_signal_connect_swapped(decodebin, "pad-added", G_CALLBACK(+[](GstPad* sinkPad, GstPad* srcPad) {
            RELEASE_ASSERT(!gst_pad_is_linked(sinkPad));
            gst_pad_link(srcPad, sinkPad);
        }), sinkPad.get());
    }

    gst_element_link_many(videoscale, videoconvert, videorate, nullptr);

    auto sinkPad = adoptGRef(gst_element_get_static_pad(head, "sink"));
    gst_element_add_pad(bin, gst_ghost_pad_new("sink", sinkPad.get()));

    auto srcPad = adoptGRef(gst_element_get_static_pad(videorate, "src"));
    gst_element_add_pad(bin, gst_ghost_pad_new("src", srcPad.get()));

    return bin;
}

GstVideoInfo GStreamerVideoCapturer::getBestFormat()
{
    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_fixate(gst_device_get_caps(m_device.get())));
    GstVideoInfo info;
    gst_video_info_from_caps(&info, caps.get());

    return info;
}

bool GStreamerVideoCapturer::setSize(int width, int height)
{
    if (isCapturingDisplay()) {
        // Pipewiresrc doesn't seem to support caps re-negotiation and framerate configuration properly.
        GST_FIXME_OBJECT(m_pipeline.get(), "Resizing disabled on display capture source");
        return true;
    }

    if (!width || !height)
        return false;

    auto videoResolution = getVideoResolutionFromCaps(m_caps.get());
    if (videoResolution && videoResolution->width() == width && videoResolution->height() == height) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Size has not changed");
        return true;
    }

    GST_INFO_OBJECT(m_pipeline.get(), "Setting size to %dx%d", width, height);
    m_caps = adoptGRef(gst_caps_copy(m_caps.get()));
    gst_caps_set_simple(m_caps.get(), "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, nullptr);

    if (!m_capsfilter)
        return false;

    g_object_set(m_capsfilter.get(), "caps", m_caps.get(), nullptr);
    return true;
}

bool GStreamerVideoCapturer::setFrameRate(double frameRate)
{
    if (isCapturingDisplay()) {
        // Pipewiresrc doesn't seem to support caps re-negotiation and framerate configuration properly.
        GST_FIXME_OBJECT(m_pipeline.get(), "Framerate override disabled on display capture source");
        return true;
    }

    int numerator, denominator;

    gst_util_double_to_fraction(frameRate, &numerator, &denominator);

    if (numerator < -G_MAXINT) {
        GST_INFO_OBJECT(m_pipeline.get(), "Framerate %f not allowed", frameRate);
        return false;
    }

    if (!numerator) {
        GST_INFO_OBJECT(m_pipeline.get(), "Do not force variable framerate");
        return false;
    }

    m_caps = adoptGRef(gst_caps_copy(m_caps.get()));
    gst_caps_set_simple(m_caps.get(), "framerate", GST_TYPE_FRACTION, numerator, denominator, nullptr);

    if (!m_capsfilter)
        return false;

    GST_INFO_OBJECT(m_pipeline.get(), "Setting framerate to %f fps", frameRate);
    g_object_set(m_capsfilter.get(), "caps", m_caps.get(), nullptr);

    return true;
}

static std::optional<int> getMaxIntValueFromStructure(const GstStructure* structure, const char* fieldName)
{
    const GValue* value = gst_structure_get_value(structure, fieldName);
    if (!value)
        return std::nullopt;

    int maxInt = -G_MAXINT;
    if (G_VALUE_HOLDS_INT(value))
        maxInt = g_value_get_int(value);
    else if (GST_VALUE_HOLDS_INT_RANGE(value))
        maxInt = gst_value_get_int_range_max(value);
    else if (GST_VALUE_HOLDS_ARRAY(value)) {
        const guint size = gst_value_array_get_size(value);
        for (guint i = 0; i < size; ++i) {
            const GValue* item = gst_value_array_get_value(value, i);
            if (G_VALUE_HOLDS_INT(item)) {
                int val = g_value_get_int(item);
                if (val > maxInt)
                    maxInt = val;
            }
        }
    } else if (GST_VALUE_HOLDS_LIST(value)) {
        const guint size = gst_value_list_get_size(value);
        for (guint i = 0; i < size; ++i) {
            const GValue* item = gst_value_list_get_value(value, i);
            if (G_VALUE_HOLDS_INT(item)) {
                int val = g_value_get_int(item);
                if (val > maxInt)
                    maxInt = val;
            }
        }
    }

    return (maxInt > -G_MAXINT) ? std::make_optional<>(maxInt) : std::nullopt;
}

static std::optional<double> getMaxFractionValueFromStructure(const GstStructure* structure, const char* fieldName)
{
    const GValue* value = gst_structure_get_value(structure, fieldName);
    if (!value)
        return std::nullopt;

    double maxFraction = -G_MAXDOUBLE;
    if (GST_VALUE_HOLDS_FRACTION(value)) {
        gst_util_fraction_to_double(gst_value_get_fraction_numerator(value),
            gst_value_get_fraction_denominator(value), &maxFraction);
    } else if (GST_VALUE_HOLDS_FRACTION_RANGE(value)) {
        const GValue* fractionValue = gst_value_get_fraction_range_max(value);
        gst_util_fraction_to_double(gst_value_get_fraction_numerator(fractionValue),
            gst_value_get_fraction_denominator(fractionValue), &maxFraction);
    } else if (GST_VALUE_HOLDS_ARRAY(value)) {
        const guint size = gst_value_array_get_size(value);
        for (guint i = 0; i < size; ++i) {
            const GValue* item = gst_value_array_get_value(value, i);
            if (GST_VALUE_HOLDS_FRACTION(item)) {
                double val = -G_MAXDOUBLE;
                gst_util_fraction_to_double(gst_value_get_fraction_numerator(item),
                    gst_value_get_fraction_denominator(item), &val);
                if (val > maxFraction)
                    maxFraction = val;
            }
        }
    } else if (GST_VALUE_HOLDS_LIST(value)) {
        const guint size = gst_value_list_get_size(value);
        for (guint i = 0; i < size; ++i) {
            const GValue* item = gst_value_list_get_value(value, i);
            if (GST_VALUE_HOLDS_FRACTION(item)) {
                double val = -G_MAXDOUBLE;
                gst_util_fraction_to_double(gst_value_get_fraction_numerator(item),
                    gst_value_get_fraction_denominator(item), &val);
                if (val > maxFraction)
                    maxFraction = val;
            }
        }
    }

    return (maxFraction > -G_MAXDOUBLE) ? std::make_optional<>(maxFraction) : std::nullopt;
}

struct DeviceCapsProcessHolder {
    DeviceCapsProcessHolder()
        : aggregatedCaps(adoptGRef(gst_caps_new_empty()))
    {
    }

    int targetWidth = 0;
    int targetHeight = 0;
    double targetFrameRate = 0;
    Vector<String> mimeTypes;
    GRefPtr<GstCaps> aggregatedCaps;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(DeviceCapsProcessHolder);

void GStreamerVideoCapturer::reconfigure()
{
    if (isCapturingDisplay()) {
        // Pipewiresrc doesn't seem to support caps re-negotiation and framerate configuration properly.
        GST_FIXME_OBJECT(m_pipeline.get(), "Caps re-negotiation disabled on display capture source");
        return;
    }

    if (!m_videoSrcMIMETypeFilter)
        return;

    auto* holder = createDeviceCapsProcessHolder();

    // If nothing has been specified by the user, we target at least an arbitrary resolution of 1920x1080@24fps.
    const GstStructure* capsStruct = gst_caps_get_structure(m_caps.get(), 0);
    if (!gst_structure_get_int(capsStruct, "width", &holder->targetWidth))
        holder->targetWidth = 1920;

    if (!gst_structure_get_int(capsStruct, "height", &holder->targetHeight))
        holder->targetHeight = 1080;

    int numerator = 0;
    int denominator = 1;
    if (gst_structure_get_fraction(capsStruct, "framerate", &numerator, &denominator))
        gst_util_fraction_to_double(numerator, denominator, &holder->targetFrameRate);
    else
        holder->targetFrameRate = 24;

    GST_DEBUG_OBJECT(m_pipeline.get(), "Searching best video capture device mime type for resolution %dx%d@%.3f",
        holder->targetHeight, holder->targetHeight, holder->targetFrameRate);

    auto deviceCaps = adoptGRef(gst_device_get_caps(m_device.get()));

    // Collect all formats fitting with the requested constraints and store them in the holder aggregatedCaps.
    gst_caps_foreach(deviceCaps.get(), reinterpret_cast<GstCapsForeachFunc>(+[](GstCapsFeatures*, GstStructure* structure, DeviceCapsProcessHolder* holder) -> gboolean {
        auto width = getMaxIntValueFromStructure(structure, "width");
        if (!width.has_value())
            return TRUE;

        auto height = getMaxIntValueFromStructure(structure, "height");
        if (!height.has_value())
            return TRUE;

        auto frameRate = getMaxFractionValueFromStructure(structure, "framerate");
        if (!frameRate.has_value())
            return TRUE;

        if (*width < holder->targetWidth || *height < holder->targetHeight || *frameRate < holder->targetFrameRate)
            return TRUE;

        auto mimeType = makeString(gst_structure_get_name(structure));
        if (holder->mimeTypes.contains(mimeType))
            return TRUE;

        holder->mimeTypes.append(mimeType);

        int frameRateNumerator, frameRateDenominator;
        gst_util_double_to_fraction(*frameRate, &frameRateNumerator, &frameRateDenominator);
        GUniquePtr<GstStructure> candidate(gst_structure_new(mimeType.ascii().data(), "width", G_TYPE_INT, *width, "height", G_TYPE_INT, *height, "framerate", GST_TYPE_FRACTION, frameRateNumerator, frameRateDenominator, nullptr));
        // Workaround for https://gitlab.freedesktop.org/pipewire/pipewire/-/issues/1793.
        if (gst_structure_has_name(structure, "video/x-raw")) {
            if (gst_structure_has_field(structure, "format"))
                gst_structure_set(candidate.get(), "format", G_TYPE_STRING, gst_structure_get_string(structure, "format"), nullptr);
        }
        gst_caps_append_structure(holder->aggregatedCaps.get(), candidate.release());
        return TRUE;
    }), holder);

    auto caps = adoptGRef(gst_caps_new_empty());

    // Prefer video/x-raw over other formats.
    GST_DEBUG_OBJECT(m_pipeline.get(), "Candidates: %" GST_PTR_FORMAT, holder->aggregatedCaps.get());
    unsigned candidatesSize = gst_caps_get_size(holder->aggregatedCaps.get());
    for (unsigned i = 0; i < candidatesSize; i++) {
        const auto* structure = gst_caps_get_structure(holder->aggregatedCaps.get(), i);
        if (g_str_equal(gst_structure_get_name(structure), "video/x-raw")) {
            gst_caps_append_structure(caps.get(), gst_structure_copy(structure));
            break;
        }
    }

    // Fallback to first non video/x-raw format.
    if (gst_caps_is_empty(caps.get())) {
        gst_caps_foreach(holder->aggregatedCaps.get(), +[](GstCapsFeatures*, GstStructure* structure, gpointer userData) -> gboolean {
            if (g_str_equal(gst_structure_get_name(structure), "video/x-raw"))
                return TRUE;

            gst_caps_append_structure(GST_CAPS_CAST(userData), gst_structure_copy(structure));
            return FALSE;
        }, caps.get());
    }
    destroyDeviceCapsProcessHolder(holder);

    GST_INFO_OBJECT(m_pipeline.get(), "Setting video capture device caps to %" GST_PTR_FORMAT, caps.get());
    g_object_set(m_videoSrcMIMETypeFilter.get(), "caps", caps.get(), nullptr);
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
