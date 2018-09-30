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
#include <webrtc/api/mediastreaminterface.h>
#include <webrtc/api/peerconnectioninterface.h>
#include <webrtc/media/base/videocommon.h>
#include <webrtc/media/engine/webrtcvideocapturer.h>
#include <webrtc/media/engine/webrtcvideocapturerfactory.h>
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

class GStreamerVideoCaptureSourceFactory final : public VideoCaptureFactory {
public:
    CaptureSourceOrError createVideoCaptureSource(const CaptureDevice& device, const MediaConstraints* constraints) final
    {
        return GStreamerVideoCaptureSource::create(device.persistentId(), constraints);
    }
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
};

DisplayCaptureFactory& libWebRTCDisplayCaptureSourceFactory()
{
    static NeverDestroyed<GStreamerDisplayCaptureSourceFactory> factory;
    return factory.get();
}

CaptureSourceOrError GStreamerVideoCaptureSource::create(const String& deviceID, const MediaConstraints* constraints)
{
    auto device = GStreamerVideoCaptureDeviceManager::singleton().gstreamerDeviceWithUID(deviceID);
    if (!device) {
        auto errorMessage = String::format("GStreamerVideoCaptureSource::create(): GStreamer did not find the device: %s.", deviceID.utf8().data());
        return CaptureSourceOrError(WTFMove(errorMessage));
    }

    auto source = adoptRef(*new GStreamerVideoCaptureSource(device.value()));

    if (constraints) {
        auto result = source->applyConstraints(*constraints);
        if (result)
            return WTFMove(result.value().first);
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

GStreamerVideoCaptureSource::GStreamerVideoCaptureSource(const String& deviceID, const String& name, const gchar *source_factory)
    : RealtimeMediaSource(deviceID, RealtimeMediaSource::Type::Video, name)
    , m_capturer(std::make_unique<GStreamerVideoCapturer>(source_factory))
{
    initializeGStreamerDebug();
}

GStreamerVideoCaptureSource::GStreamerVideoCaptureSource(GStreamerCaptureDevice device)
    : RealtimeMediaSource(device.persistentId(), RealtimeMediaSource::Type::Video, device.label())
    , m_capturer(std::make_unique<GStreamerVideoCapturer>(device))
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

GstFlowReturn GStreamerVideoCaptureSource::newSampleCallback(GstElement* sink, GStreamerVideoCaptureSource* source)
{
    auto gstSample = adoptGRef(gst_app_sink_pull_sample(GST_APP_SINK(sink)));
    auto mediaSample = MediaSampleGStreamer::create(WTFMove(gstSample), WebCore::FloatSize(), String());

    // FIXME - Check how presentationSize is supposed to be used here.
    callOnMainThread([protectedThis = makeRef(*source), mediaSample = WTFMove(mediaSample)] {
        protectedThis->videoSampleAvailable(mediaSample.get());
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
    if (m_capabilities)
        return m_capabilities.value();

    RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());
    GRefPtr<GstCaps> caps = adoptGRef(m_capturer->caps());
    int32_t minWidth = G_MAXINT32, maxWidth = 0, minHeight = G_MAXINT32, maxHeight = 0;
    double minFramerate = G_MAXDOUBLE, maxFramerate = G_MINDOUBLE;

    for (guint i = 0; i < gst_caps_get_size(caps.get()); i++) {
        GstStructure* str = gst_caps_get_structure(caps.get(), i);

        // Only accept raw video for now.
        if (!gst_structure_has_name(str, "video/x-raw"))
            continue;

        int32_t tmpMinWidth, tmpMinHeight, tmpMinFPSNumerator, tmpMinFPSDenominator;
        int32_t tmpMaxWidth, tmpMaxHeight, tmpMaxFPSNumerator, tmpMaxFPSDenominator;
        double tmpMinFramerate = G_MAXDOUBLE, tmpMaxFramerate = G_MINDOUBLE;

        if (!gst_structure_get(str, "width", GST_TYPE_INT_RANGE, &tmpMinWidth, &tmpMaxWidth, "height", GST_TYPE_INT_RANGE, &tmpMinHeight, &tmpMaxHeight, nullptr)) {
            if (!gst_structure_get(str, "width", G_TYPE_INT, &tmpMinWidth, "height", G_TYPE_INT, &tmpMinHeight, nullptr))
                continue;

            tmpMaxWidth = tmpMinWidth;
            tmpMaxHeight = tmpMinHeight;
        }

        if (gst_structure_get(str, "framerate", GST_TYPE_FRACTION_RANGE, &tmpMinFPSNumerator, &tmpMinFPSDenominator, &tmpMaxFPSNumerator, &tmpMaxFPSDenominator, nullptr)) {
            gst_util_fraction_to_double(tmpMinFPSNumerator, tmpMinFPSDenominator, &tmpMinFramerate);
            gst_util_fraction_to_double(tmpMaxFPSNumerator, tmpMaxFPSDenominator, &tmpMaxFramerate);
        } else if (gst_structure_get(str,
            "framerate", GST_TYPE_FRACTION, &tmpMinFPSNumerator, &tmpMinFPSDenominator, nullptr)) {
            tmpMaxFPSNumerator = tmpMinFPSNumerator;
            tmpMaxFPSDenominator = tmpMinFPSDenominator;
            gst_util_fraction_to_double(tmpMinFPSNumerator, tmpMinFPSDenominator, &tmpMinFramerate);
            gst_util_fraction_to_double(tmpMaxFPSNumerator, tmpMaxFPSDenominator, &tmpMaxFramerate);
        } else {
            const GValue* frameRates(gst_structure_get_value(str, "framerate"));
            tmpMinFramerate = G_MAXDOUBLE;
            tmpMaxFramerate = 0.0;

            guint frameRatesLength = static_cast<guint>(gst_value_list_get_size(frameRates)) - 1;

            for (guint i = 0; i < frameRatesLength; i++) {
                double tmpFrameRate;
                const GValue* val = gst_value_list_get_value(frameRates, i);

                ASSERT(G_VALUE_TYPE(val) == GST_TYPE_FRACTION);
                gst_util_fraction_to_double(gst_value_get_fraction_numerator(val),
                    gst_value_get_fraction_denominator(val), &tmpFrameRate);

                tmpMinFramerate = std::min(tmpMinFramerate, tmpFrameRate);
                tmpMaxFramerate = std::max(tmpMaxFramerate, tmpFrameRate);
            }

            if (i > 0) {
                minWidth = std::min(tmpMinWidth, minWidth);
                minHeight = std::min(tmpMinHeight, minHeight);
                minFramerate = std::min(tmpMinFramerate, minFramerate);

                maxWidth = std::max(tmpMaxWidth, maxWidth);
                maxHeight = std::max(tmpMaxHeight, maxHeight);
                maxFramerate = std::max(tmpMaxFramerate, maxFramerate);
            } else {
                minWidth = tmpMinWidth;
                minHeight = tmpMinHeight;
                minFramerate = tmpMinFramerate;

                maxWidth = tmpMaxWidth;
                maxHeight = tmpMaxHeight;
                maxFramerate = tmpMaxFramerate;
            }
        }

        capabilities.setDeviceId(id());
        capabilities.setWidth(CapabilityValueOrRange(minWidth, maxWidth));
        capabilities.setHeight(CapabilityValueOrRange(minHeight, maxHeight));
        capabilities.setFrameRate(CapabilityValueOrRange(minFramerate, maxFramerate));
        m_capabilities = WTFMove(capabilities);
    }

    return m_capabilities.value();
}

const RealtimeMediaSourceSettings& GStreamerVideoCaptureSource::settings()
{
    if (!m_currentSettings) {
        RealtimeMediaSourceSettings settings;
        settings.setDeviceId(id());

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

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC)
