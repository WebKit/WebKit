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
#include "GStreamerVideoCaptureSource.h"

#include "DisplayCaptureManager.h"
#include "GStreamerCaptureDeviceManager.h"

#include <gst/app/gstappsink.h>

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_video_capture_source_debug);
#define GST_CAT_DEFAULT webkit_video_capture_source_debug

static void initializeVideoCaptureSourceDebugCategory()
{
    ensureGStreamerInitialized();

    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_video_capture_source_debug, "webkitvideocapturesource", 0,
            "WebKit Video Capture Source.");
    });
}

class GStreamerVideoCaptureSourceFactory final : public VideoCaptureFactory {
public:
    CaptureSourceOrError createVideoCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier) final
    {
        return GStreamerVideoCaptureSource::create(String { device.persistentId() }, WTFMove(hashSalts), constraints);
    }
private:
    CaptureDeviceManager& videoCaptureDeviceManager() final { return GStreamerVideoCaptureDeviceManager::singleton(); }
};

class GStreamerDisplayCaptureSourceFactory final : public DisplayCaptureFactory {
public:
    CaptureSourceOrError createDisplayCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, PageIdentifier) final
    {
        auto& manager = GStreamerDisplayCaptureDeviceManager::singleton();
        return manager.createDisplayCaptureSource(device, WTFMove(hashSalts), constraints);
    }
private:
    DisplayCaptureManager& displayCaptureDeviceManager() final { return GStreamerDisplayCaptureDeviceManager::singleton(); }
};

CaptureSourceOrError GStreamerVideoCaptureSource::create(String&& deviceID, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints)
{
    auto device = GStreamerVideoCaptureDeviceManager::singleton().gstreamerDeviceWithUID(deviceID);
    if (!device) {
        auto errorMessage = makeString("GStreamerVideoCaptureSource::create(): GStreamer did not find the device: ", deviceID, '.');
        return CaptureSourceOrError({ WTFMove(errorMessage), MediaAccessDenialReason::HardwareError });
    }

    auto source = adoptRef(*new GStreamerVideoCaptureSource(WTFMove(*device), WTFMove(hashSalts)));
    if (constraints) {
        if (auto result = source->applyConstraints(*constraints))
            return CaptureSourceOrError(CaptureSourceError { result->invalidConstraint });
    }
    return CaptureSourceOrError(WTFMove(source));
}

CaptureSourceOrError GStreamerVideoCaptureSource::createPipewireSource(String&& deviceID, const NodeAndFD& nodeAndFd, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints, CaptureDevice::DeviceType deviceType)
{
    auto source = adoptRef(*new GStreamerVideoCaptureSource(WTFMove(deviceID), { }, WTFMove(hashSalts), "pipewiresrc", deviceType, nodeAndFd));
    if (constraints) {
        if (auto result = source->applyConstraints(*constraints))
            return CaptureSourceOrError(CaptureSourceError { result->invalidConstraint });
    }
    return CaptureSourceOrError(WTFMove(source));
}

VideoCaptureFactory& GStreamerVideoCaptureSource::factory()
{
    static NeverDestroyed<GStreamerVideoCaptureSourceFactory> factory;
    return factory.get();
}

DisplayCaptureFactory& GStreamerVideoCaptureSource::displayFactory()
{
    static NeverDestroyed<GStreamerDisplayCaptureSourceFactory> factory;
    return factory.get();
}

GStreamerVideoCaptureSource::GStreamerVideoCaptureSource(String&& deviceID, AtomString&& name, MediaDeviceHashSalts&& hashSalts, const gchar* sourceFactory, CaptureDevice::DeviceType deviceType, const NodeAndFD& nodeAndFd)
    : RealtimeVideoCaptureSource(CaptureDevice { WTFMove(deviceID), CaptureDevice::DeviceType::Camera, WTFMove(name) }, WTFMove(hashSalts), { })
    , m_capturer(adoptRef(*new GStreamerVideoCapturer(sourceFactory, deviceType)))
    , m_deviceType(deviceType)
{
    initializeVideoCaptureSourceDebugCategory();
    m_capturer->setPipewireNodeAndFD(nodeAndFd);
    m_capturer->addObserver(*this);

    auto& singleton = GStreamerVideoCaptureDeviceManager::singleton();
    singleton.registerCapturer(m_capturer);
}

GStreamerVideoCaptureSource::GStreamerVideoCaptureSource(GStreamerCaptureDevice&& device, MediaDeviceHashSalts&& hashSalts)
    : RealtimeVideoCaptureSource(device, WTFMove(hashSalts), { })
    , m_capturer(adoptRef(*new GStreamerVideoCapturer(WTFMove(device))))
    , m_deviceType(CaptureDevice::DeviceType::Camera)
{
    initializeVideoCaptureSourceDebugCategory();
    m_capturer->addObserver(*this);

    auto& singleton = GStreamerVideoCaptureDeviceManager::singleton();
    singleton.registerCapturer(m_capturer);
}

GStreamerVideoCaptureSource::~GStreamerVideoCaptureSource()
{
    m_capturer->removeObserver(*this);
    if (!m_capturer->pipeline())
        return;
    m_capturer->stop();

    if (m_capturer->isCapturingDisplay()) {
        auto& manager = GStreamerDisplayCaptureDeviceManager::singleton();
        manager.stopSource(persistentID());
    }

    auto& singleton = GStreamerVideoCaptureDeviceManager::singleton();
    singleton.unregisterCapturer(*m_capturer);
}

void GStreamerVideoCaptureSource::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
{
    bool reconfigure = false;
    if (settings.containsAny({ RealtimeMediaSourceSettings::Flag::Width, RealtimeMediaSourceSettings::Flag::Height })) {
        if (m_deviceType == CaptureDevice::DeviceType::Window || m_deviceType == CaptureDevice::DeviceType::Screen)
            ensureIntrinsicSizeMaintainsAspectRatio();

        if (m_capturer->setSize(size().width(), size().height()))
            reconfigure = true;
    }

    if (settings.contains(RealtimeMediaSourceSettings::Flag::FrameRate))
        if (m_capturer->setFrameRate(frameRate()))
            reconfigure = true;

    if (reconfigure)
        m_capturer->reconfigure();
}

void GStreamerVideoCaptureSource::sourceCapsChanged(const GstCaps* caps)
{
    auto videoResolution = getVideoResolutionFromCaps(caps);
    if (!videoResolution)
        return;

    setIntrinsicSize(IntSize(*videoResolution), false);
    if (m_deviceType == CaptureDevice::DeviceType::Screen)
        ensureIntrinsicSizeMaintainsAspectRatio();
}

void GStreamerVideoCaptureSource::captureEnded()
{
    m_capturer->stop();
}

void GStreamerVideoCaptureSource::startProducingData()
{
    m_capturer->setupPipeline();

    if (m_deviceType == CaptureDevice::DeviceType::Camera)
        m_capturer->setSize(size().width(), size().height());

    m_capturer->setFrameRate(frameRate());
    m_capturer->reconfigure();
    m_capturer->setSinkVideoFrameCallback([this](auto&& videoFrame) {
        if (!isProducingData() || muted())
            return;
        dispatchVideoFrameToObservers(WTFMove(videoFrame), { });
    });

    m_capturer->start();
}

void GStreamerVideoCaptureSource::stopProducingData()
{
    GST_INFO("Reset height and width after stopping source");
    setSize({ 0, 0 });
    m_capturer->stop();
}

const RealtimeMediaSourceCapabilities& GStreamerVideoCaptureSource::capabilities()
{
    RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());

    capabilities.setDeviceId(hashedId());
    updateCapabilities(capabilities);

    capabilities.addFacingMode(VideoFacingMode::Unknown);

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
    m_currentSettings->setFacingMode(facingMode());
    return m_currentSettings.value();
}

void GStreamerVideoCaptureSource::generatePresets()
{
    Vector<VideoPreset> presets;
    auto caps = m_capturer->caps();
    for (unsigned i = 0; i < gst_caps_get_size(caps.get()); i++) {
        GstStructure* str = gst_caps_get_structure(caps.get(), i);

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

        presets.append(VideoPreset { { size, WTFMove(frameRates) } });
    }

    if (presets.isEmpty()) {
        GST_INFO("Could not find any presets for caps: %" GST_PTR_FORMAT " just let anything go out.", caps.get());

        for (auto& size : standardVideoSizes()) {
            Vector<FrameRateRange> frameRates;

            frameRates.append({ 0, G_MAXDOUBLE});
            presets.append(VideoPreset { { size, WTFMove(frameRates) } });
        }
    }

    setSupportedPresets(WTFMove(presets));
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
