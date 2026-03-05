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

#include "VideoFrameGStreamer.h"
#include <gst/app/gstappsink.h>

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

GStreamerVideoCapturer::GStreamerVideoCapturer(GStreamerCaptureDevice&& device)
    : GStreamerCapturer(WTF::move(device), adoptGRef(gst_caps_new_empty_simple("video/x-raw")))
{
    initializeVideoCapturerDebugCategory();
}

GStreamerVideoCapturer::GStreamerVideoCapturer(const PipeWireCaptureDevice& device)
    : GStreamerCapturer(device)
{
    initializeVideoCapturerDebugCategory();
}

void GStreamerVideoCapturer::handleSample(GRefPtr<GstSample>&& sample)
{
    VideoFrameTimeMetadata metadata;
    metadata.captureTime = MonotonicTime::now().secondsSinceEpoch();

    auto buffer = gst_sample_get_buffer(sample.get());
    MediaTime presentationTime = MediaTime::invalidTime();
    if (GST_BUFFER_PTS_IS_VALID(buffer))
        presentationTime = fromGstClockTime(GST_BUFFER_PTS(buffer));

    auto rotationFromMeta = webkitGstBufferGetVideoRotation(buffer);
    auto size = this->size();
    VideoFrameGStreamer::CreateOptions options(WTF::move(size));
    options.presentationTime = presentationTime;
    options.rotation = rotationFromMeta.first;
    options.isMirrored = rotationFromMeta.second;
    options.timeMetadata = WTF::move(metadata);
    m_sinkVideoFrameCallback.second(VideoFrameGStreamer::create(WTF::move(sample), options));
}

void GStreamerVideoCapturer::setSinkVideoFrameCallback(SinkVideoFrameCallback&& callback)
{
    if (m_sinkVideoFrameCallback.first.newSampleSignalId) {
        g_signal_handler_disconnect(sink(), m_sinkVideoFrameCallback.first.newSampleSignalId);
        g_signal_handler_disconnect(sink(), m_sinkVideoFrameCallback.first.prerollSignalId);
    }
    m_sinkVideoFrameCallback.second = WTF::move(callback);
    m_sinkVideoFrameCallback.first.newSampleSignalId = g_signal_connect_swapped(sink(), "new-sample", G_CALLBACK(+[](GStreamerVideoCapturer* capturer, GstElement* sink) -> GstFlowReturn {
        auto sample = adoptGRef(gst_app_sink_pull_sample(GST_APP_SINK(sink)));
        capturer->handleSample(WTF::move(sample));
        return GST_FLOW_OK;
    }), this);

    m_sinkVideoFrameCallback.first.prerollSignalId = g_signal_connect_swapped(sink(), "new-preroll", G_CALLBACK(+[](GStreamerVideoCapturer* capturer, GstElement* sink) -> GstFlowReturn {
        auto sample = adoptGRef(gst_app_sink_pull_preroll(GST_APP_SINK(sink)));
        capturer->handleSample(WTF::move(sample));
        return GST_FLOW_OK;
    }), this);
}

bool GStreamerVideoCapturer::isCapturingDisplay() const
{
    auto deviceType = this->deviceType();
    return deviceType == CaptureDevice::DeviceType::Screen || deviceType == CaptureDevice::DeviceType::Window;
}

void GStreamerVideoCapturer::tearDown(bool disconnectSignals)
{
    GStreamerCapturer::tearDown(disconnectSignals);
    if (disconnectSignals)
        m_videoSrcMIMETypeFilter = nullptr;
}

void GStreamerVideoCapturer::setupPipeline()
{
    GStreamerCapturer::setupPipeline();
    auto pad = adoptGRef(gst_element_get_static_pad(m_sink.get(), "sink"));
    gst_pad_add_probe(pad.get(), GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM, reinterpret_cast<GstPadProbeCallback>(+[](GstPad*, GstPadProbeInfo* info, gpointer) -> GstPadProbeReturn {
        if (GST_QUERY_TYPE(GST_PAD_PROBE_INFO_QUERY(info)) == GST_QUERY_ALLOCATION)
            gst_query_add_allocation_meta(GST_PAD_PROBE_INFO_QUERY(info), GST_VIDEO_META_API_TYPE, nullptr);
        return GST_PAD_PROBE_OK;
    }), nullptr, nullptr);
}

GstElement* GStreamerVideoCapturer::createConverter()
{
    if (isCapturingDisplay())
        return nullptr;

    auto videoConvert = createVideoConvertScaleElement();
    if (!videoConvert) [[unlikely]]
        return nullptr;

    auto* bin = gst_bin_new(nullptr);
    auto* videorate = makeGStreamerElement("videorate"_s, "videorate"_s);

    // https://gitlab.freedesktop.org/gstreamer/gst-plugins-base/issues/97#note_56575
    g_object_set(videorate, "drop-only", TRUE, "average-period", UINT64_C(1), nullptr);

    gst_bin_add_many(GST_BIN_CAST(bin), videoConvert.get(), videorate, nullptr);

    m_videoSrcMIMETypeFilter = gst_element_factory_make("capsfilter", "mimetype-filter");

    auto caps = adoptGRef(gst_caps_new_empty_simple("video/x-raw"));
    g_object_set(m_videoSrcMIMETypeFilter.get(), "caps", caps.get(), nullptr);

    auto* decodebin = makeGStreamerElement("decodebin3"_s);
    gst_bin_add_many(GST_BIN_CAST(bin), m_videoSrcMIMETypeFilter.get(), decodebin, nullptr);
    gst_element_link(m_videoSrcMIMETypeFilter.get(), decodebin);

    auto sinkPad = adoptGRef(gst_element_get_static_pad(videoConvert.get(), "sink"));

    g_signal_connect_data(decodebin, "pad-added", G_CALLBACK(+[](GstElement*, GstPad* srcPad, GstPad* sinkPad) {
        RELEASE_ASSERT(!gst_pad_is_linked(sinkPad));
        gst_pad_link(srcPad, sinkPad);
    }), sinkPad.leakRef(), reinterpret_cast<GClosureNotify>(+[](gpointer data, GClosure*) {
        gst_object_unref(GST_PAD_CAST(data));
    }), static_cast<GConnectFlags>(0));

    gst_element_link(videoConvert.get(), videorate);

    sinkPad = adoptGRef(gst_element_get_static_pad(m_videoSrcMIMETypeFilter.get(), "sink"));
    gst_element_add_pad(bin, gst_ghost_pad_new("sink", sinkPad.get()));

    auto srcPad = adoptGRef(gst_element_get_static_pad(videorate, "src"));
    gst_element_add_pad(bin, gst_ghost_pad_new("src", srcPad.get()));

    return bin;
}

bool GStreamerVideoCapturer::setSize(const IntSize& size)
{
    if (isCapturingDisplay()) {
        // Pipewiresrc doesn't seem to support caps re-negotiation and framerate configuration properly.
        GST_FIXME_OBJECT(m_pipeline.get(), "Resizing disabled on display capture source");
        return true;
    }

    int width = size.width();
    int height = size.height();
    GST_INFO_OBJECT(m_pipeline.get(), "Setting size to %dx%d", width, height);
    if (!width || !height)
        return false;

    auto videoResolution = getVideoResolutionFromCaps(m_caps.get());
    if (videoResolution && videoResolution->width() == width && videoResolution->height() == height) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Size has not changed");
        return true;
    }

    if (!m_capsfilter) [[unlikely]]
        return false;

    m_size = size;
    auto modifiedCaps = adoptGRef(gst_caps_make_writable(m_caps.leakRef()));
    gst_caps_set_simple(modifiedCaps.get(), "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, nullptr);
    gst_caps_take(&m_caps.outPtr(), modifiedCaps.leakRef());

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

    if (!m_capsfilter) [[unlikely]]
        return false;

    auto modifiedCaps = adoptGRef(gst_caps_make_writable(m_caps.leakRef()));
    gst_caps_set_simple(modifiedCaps.get(), "framerate", GST_TYPE_FRACTION, numerator, denominator, nullptr);
    gst_caps_take(&m_caps.outPtr(), modifiedCaps.leakRef());

    GST_INFO_OBJECT(m_pipeline.get(), "Setting framerate to %f fps", frameRate);
    g_object_set(m_capsfilter.get(), "caps", m_caps.get(), nullptr);
    return true;
}

static std::optional<int> getMaxIntValueFromStructure(const GstStructure* structure, ASCIILiteral fieldName)
{
    const GValue* value = gst_structure_get_value(structure, fieldName.characters());
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

static std::optional<double> getMaxFractionValueFromStructure(const GstStructure* structure, ASCIILiteral fieldName)
{
    const GValue* value = gst_structure_get_value(structure, fieldName.characters());
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

void GStreamerVideoCapturer::reconfigure()
{
    if (isCapturingDisplay()) {
        // Pipewiresrc doesn't seem to support caps re-negotiation and framerate configuration properly.
        GST_FIXME_OBJECT(m_pipeline.get(), "Caps re-negotiation disabled on display capture source");
        return;
    }

    if (!m_videoSrcMIMETypeFilter)
        return;

    auto deviceCaps = caps();
    if (!deviceCaps)
        return;

    struct MimeTypeSelector {
        String mimeType = "video/x-raw"_s;
        String format;
        int maxWidth = 0;
        int maxHeight = 0;
        double maxFrameRate = 0;

        struct {
            int width = 0;
            int height = 0;
            double frameRate = 0;
        } stopCondition;
    } selector;

    // If nothing has been specified by the user, we target at least an arbitrary resolution of 1920x1080@24fps.
    const GstStructure* capsStruct = gst_caps_get_structure(m_caps.get(), 0);
    selector.stopCondition.width = gstStructureGet<int>(capsStruct, "width"_s).value_or(1920);
    selector.stopCondition.height = gstStructureGet<int>(capsStruct, "height"_s).value_or(1080);

    int numerator = 0;
    int denominator = 1;
    if (gst_structure_get_fraction(capsStruct, "framerate", &numerator, &denominator))
        gst_util_fraction_to_double(numerator, denominator, &selector.stopCondition.frameRate);
    else
        selector.stopCondition.frameRate = 24;

    GST_DEBUG_OBJECT(m_pipeline.get(), "Searching best video capture device mime type for resolution %dx%d@%.3f",
        selector.stopCondition.width, selector.stopCondition.height, selector.stopCondition.frameRate);

    gst_caps_foreach(deviceCaps.get(),
        reinterpret_cast<GstCapsForeachFunc>(+[](GstCapsFeatures*, GstStructure* structure, MimeTypeSelector* selector) -> gboolean {
            auto width = getMaxIntValueFromStructure(structure, "width"_s);
            if (!width.has_value())
                return TRUE;

            auto height = getMaxIntValueFromStructure(structure, "height"_s);
            if (!height.has_value())
                return TRUE;

            auto frameRate = getMaxFractionValueFromStructure(structure, "framerate"_s);
            if (!frameRate.has_value())
                return TRUE;

            if (*width >= selector->stopCondition.width && *height >= selector->stopCondition.height
                && *frameRate >= selector->stopCondition.frameRate) {
                selector->maxWidth = *width;
                selector->maxHeight = *height;
                selector->maxFrameRate = *frameRate;
                selector->mimeType = gstStructureGetName(structure).span();
                if (gst_structure_has_name(structure, "video/x-raw")) {
                    if (gst_structure_has_field(structure, "format"))
                        selector->format = gstStructureGetString(structure, "format"_s).span();
                    else
                        return TRUE;
                }
                return FALSE;
            }

            if (*width >= selector->maxWidth && *height >= selector->maxHeight && *frameRate >= selector->maxFrameRate) {
                selector->maxWidth = *width;
                selector->maxHeight = *height;
                selector->maxFrameRate = *frameRate;
                selector->mimeType = gstStructureGetName(structure).span();
                if (gst_structure_has_name(structure, "video/x-raw")) {
                    if (gst_structure_has_field(structure, "format"))
                        selector->format = gstStructureGetString(structure, "format"_s).span();
                    else
                        return TRUE;
                }
            }

            return TRUE;
        }), &selector);

    auto caps = adoptGRef(gst_caps_new_simple(selector.mimeType.ascii().data(), "width", G_TYPE_INT, selector.maxWidth,
        "height", G_TYPE_INT, selector.maxHeight, nullptr));

    // Workaround for https://gitlab.freedesktop.org/pipewire/pipewire/-/issues/1793.
    if (!selector.format.isEmpty())
        gst_caps_set_simple(caps.get(), "format", G_TYPE_STRING, selector.format.ascii().data(), nullptr);

    GST_INFO_OBJECT(m_pipeline.get(), "Setting video capture device caps to %" GST_PTR_FORMAT, caps.get());
    g_object_set(m_videoSrcMIMETypeFilter.get(), "caps", caps.get(), nullptr);
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
