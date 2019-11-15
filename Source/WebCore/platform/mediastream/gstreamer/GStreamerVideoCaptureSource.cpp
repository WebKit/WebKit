/*
 * Copyright (C) 2018 Metrological Group B.V.
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

#if ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
#include "GStreamerVideoCaptureSource.h"

#include "GStreamerCaptureDeviceManager.h"
#include "MediaSampleGStreamer.h"

#include <gst/app/gstappsink.h>
#include <webrtc/api/media_stream_interface.h>
#include <webrtc/api/peer_connection_interface.h>
#include <webrtc/media/base/video_common.h>
// #include <webrtc/media/engine/video_capturer.h>
// #include <webrtc/media/engine/video_capturer_factory.h>
#include <webrtc/modules/video_capture/video_capture_factory.h>
#include <webrtc/modules/video_capture/video_capture_defines.h>

namespace WebCore {

const static int defaultWidth = 640;
const static int defaultHeight = 480;

GST_DEBUG_CATEGORY(webkit_video_capture_source_debug);
#define GST_CAT_DEFAULT webkit_video_capture_source_debug

static void initializeGStreamerDebug()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_video_capture_source_debug, "webkitvideocapturesource", 0,
            "WebKit Video Capture Source.");
    });
}

class GStreamerVideoPreset : public VideoPreset {
public:
    static Ref<GStreamerVideoPreset> create(IntSize size, Vector<FrameRateRange>&& framerates)
    {
        return adoptRef(*new GStreamerVideoPreset(size, WTFMove(framerates)));
    }

    GStreamerVideoPreset(IntSize size, Vector<FrameRateRange>&& frameRateRanges)
        : VideoPreset(size, WTFMove(frameRateRanges), GStreamer)
    {
    }
};

class GStreamerVideoCaptureSourceFactory final : public VideoCaptureFactory {
public:
    CaptureSourceOrError createVideoCaptureSource(const CaptureDevice& device, String&& hashSalt, const MediaConstraints* constraints) final
    {
        return GStreamerVideoCaptureSource::create(String { device.persistentId() }, WTFMove(hashSalt), constraints);
    }
private:
    CaptureDeviceManager& videoCaptureDeviceManager() final { return GStreamerVideoCaptureDeviceManager::singleton(); }
};

VideoCaptureFactory& libWebRTCVideoCaptureSourceFactory()
{
    static NeverDestroyed<GStreamerVideoCaptureSourceFactory> factory;
    return factory.get();
}

class GStreamerDisplayCaptureSourceFactory final : public DisplayCaptureFactory {
public:
    CaptureSourceOrError createDisplayCaptureSource(const CaptureDevice&, const MediaConstraints*) final
    {
        // FIXME: Implement this.
        return { };
    }
private:
    CaptureDeviceManager& displayCaptureDeviceManager() final { return GStreamerDisplayCaptureDeviceManager::singleton(); }
};

DisplayCaptureFactory& libWebRTCDisplayCaptureSourceFactory()
{
    static NeverDestroyed<GStreamerDisplayCaptureSourceFactory> factory;
    return factory.get();
}

CaptureSourceOrError GStreamerVideoCaptureSource::create(String&& deviceID, String&& hashSalt, const MediaConstraints* constraints)
{
    auto device = GStreamerVideoCaptureDeviceManager::singleton().gstreamerDeviceWithUID(deviceID);
    if (!device) {
        auto errorMessage = makeString("GStreamerVideoCaptureSource::create(): GStreamer did not find the device: ", deviceID, '.');
        return CaptureSourceOrError(WTFMove(errorMessage));
    }

    auto source = adoptRef(*new GStreamerVideoCaptureSource(device.value(), WTFMove(hashSalt)));

    if (constraints) {
        if (auto result = source->applyConstraints(*constraints))
            return WTFMove(result->badConstraint);
    }
    return CaptureSourceOrError(WTFMove(source));
}

VideoCaptureFactory& GStreamerVideoCaptureSource::factory()
{
    return libWebRTCVideoCaptureSourceFactory();
}

DisplayCaptureFactory& GStreamerVideoCaptureSource::displayFactory()
{
    return libWebRTCDisplayCaptureSourceFactory();
}

GStreamerVideoCaptureSource::GStreamerVideoCaptureSource(String&& deviceID, String&& name, String&& hashSalt, const gchar *source_factory)
    : RealtimeVideoCaptureSource(WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt))
    , m_capturer(makeUnique<GStreamerVideoCapturer>(source_factory))
{
    initializeGStreamerDebug();
}

GStreamerVideoCaptureSource::GStreamerVideoCaptureSource(GStreamerCaptureDevice device, String&& hashSalt)
    : RealtimeVideoCaptureSource(String { device.persistentId() }, String { device.label() }, WTFMove(hashSalt))
    , m_capturer(makeUnique<GStreamerVideoCapturer>(device))
{
    initializeGStreamerDebug();
}

GStreamerVideoCaptureSource::~GStreamerVideoCaptureSource()
{
}

void GStreamerVideoCaptureSource::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
{
    if (settings.containsAny({ RealtimeMediaSourceSettings::Flag::Width, RealtimeMediaSourceSettings::Flag::Height }))
        m_capturer->setSize(size().width(), size().height());
    if (settings.contains(RealtimeMediaSourceSettings::Flag::FrameRate))
        m_capturer->setFrameRate(frameRate());
}

void GStreamerVideoCaptureSource::startProducingData()
{
    m_capturer->setupPipeline();
    m_capturer->setSize(size().width(), size().height());
    m_capturer->setFrameRate(frameRate());
    g_signal_connect(m_capturer->sink(), "new-sample", G_CALLBACK(newSampleCallback), this);
    m_capturer->play();
}

void GStreamerVideoCaptureSource::processNewFrame(Ref<MediaSample>&& sample)
{
    if (!isProducingData() || muted())
        return;

    dispatchMediaSampleToObservers(WTFMove(sample));
}

GstFlowReturn GStreamerVideoCaptureSource::newSampleCallback(GstElement* sink, GStreamerVideoCaptureSource* source)
{
    auto gstSample = adoptGRef(gst_app_sink_pull_sample(GST_APP_SINK(sink)));
    auto mediaSample = MediaSampleGStreamer::create(WTFMove(gstSample), WebCore::FloatSize(), String());

    source->scheduleDeferredTask([source, sample = WTFMove(mediaSample)] () mutable {
        source->processNewFrame(WTFMove(sample));
    });

    return GST_FLOW_OK;
}

void GStreamerVideoCaptureSource::stopProducingData()
{
    g_signal_handlers_disconnect_by_func(m_capturer->sink(), reinterpret_cast<gpointer>(newSampleCallback), this);
    m_capturer->stop();

    GST_INFO("Reset height and width after stopping source");
    setSize({ 0, 0 });
}

const RealtimeMediaSourceCapabilities& GStreamerVideoCaptureSource::capabilities()
{
    RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());

    capabilities.setDeviceId(hashedId());
    updateCapabilities(capabilities);

    capabilities.addFacingMode(RealtimeMediaSourceSettings::Unknown);

    m_capabilities = WTFMove(capabilities);

    return m_capabilities.value();
}

const RealtimeMediaSourceSettings& GStreamerVideoCaptureSource::settings()
{
    if (!m_currentSettings) {
        RealtimeMediaSourceSettings settings;
        settings.setDeviceId(hashedId());

        RealtimeMediaSourceSupportedConstraints supportedConstraints;
        supportedConstraints.setSupportsDeviceId(true);
        supportedConstraints.setSupportsFacingMode(true);
        supportedConstraints.setSupportsWidth(true);
        supportedConstraints.setSupportsHeight(true);
        supportedConstraints.setSupportsAspectRatio(true);
        supportedConstraints.setSupportsFrameRate(true);
        settings.setSupportedConstraints(supportedConstraints);

        m_currentSettings = WTFMove(settings);
    }

    m_currentSettings->setWidth(size().width());
    m_currentSettings->setHeight(size().height());
    m_currentSettings->setFrameRate(frameRate());
    m_currentSettings->setAspectRatio(aspectRatio());
    m_currentSettings->setFacingMode(facingMode());
    return m_currentSettings.value();
}

void GStreamerVideoCaptureSource::generatePresets()
{
    Vector<Ref<VideoPreset>> presets;
    GRefPtr<GstCaps> caps = adoptGRef(m_capturer->caps());
    for (unsigned i = 0; i < gst_caps_get_size(caps.get()); i++) {
        GstStructure* str = gst_caps_get_structure(caps.get(), i);

        // Only accept raw video for now.
        if (!gst_structure_has_name(str, "video/x-raw"))
            continue;

        int32_t width, height;
        if (!gst_structure_get(str, "width", G_TYPE_INT, &width, "height", G_TYPE_INT, &height, nullptr)) {
            GST_INFO("Could not find discret height and width values in %" GST_PTR_FORMAT, str);
            continue;
        }

        IntSize size = { width, height };
        double framerate;
        Vector<FrameRateRange> frameRates;
        int32_t minFrameRateNumerator, minFrameRateDenominator, maxFrameRateNumerator, maxFrameRateDenominator, framerateNumerator, framerateDenominator;
        if (gst_structure_get(str, "framerate", GST_TYPE_FRACTION_RANGE, &minFrameRateNumerator, &minFrameRateDenominator, &maxFrameRateNumerator, &maxFrameRateDenominator, nullptr)) {
            FrameRateRange range;

            gst_util_fraction_to_double(minFrameRateNumerator, minFrameRateDenominator, &range.minimum);
            gst_util_fraction_to_double(maxFrameRateNumerator, maxFrameRateDenominator, &range.maximum);

            frameRates.append(range);
        } else if (gst_structure_get(str, "framerate", GST_TYPE_FRACTION, &framerateNumerator, &framerateDenominator, nullptr)) {
            gst_util_fraction_to_double(framerateNumerator, framerateDenominator, &framerate);
            frameRates.append({ framerate, framerate});
        } else {
            const GValue* frameRateValues(gst_structure_get_value(str, "framerate"));
            unsigned frameRatesLength = static_cast<unsigned>(gst_value_list_get_size(frameRateValues));

            for (unsigned j = 0; j < frameRatesLength; j++) {
                const GValue* val = gst_value_list_get_value(frameRateValues, j);

                ASSERT(val && G_VALUE_TYPE(val) == GST_TYPE_FRACTION);
                gst_util_fraction_to_double(gst_value_get_fraction_numerator(val),
                    gst_value_get_fraction_denominator(val), &framerate);

                frameRates.append({ framerate, framerate});
            }
        }

        presets.append(GStreamerVideoPreset::create(size, WTFMove(frameRates)));
    }

    if (presets.isEmpty()) {
        GST_INFO("Could not find any presets for caps: %" GST_PTR_FORMAT " just let anything go out.", caps.get());

        for (auto& size : standardVideoSizes()) {
            Vector<FrameRateRange> frameRates;

            frameRates.append({ 0, G_MAXDOUBLE});
            presets.append(GStreamerVideoPreset::create(size, WTFMove(frameRates)));
        }
    }

    setSupportedPresets(WTFMove(presets));
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC)
