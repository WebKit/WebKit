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
#include "GStreamerAudioCaptureSource.h"

#include "GStreamerAudioData.h"
#include "GStreamerAudioStreamDescription.h"
#include "GStreamerCaptureDeviceManager.h"

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static CapabilityValueOrRange defaultVolumeCapability()
{
    return CapabilityValueOrRange(0.0, 1.0);
}
const static RealtimeMediaSourceCapabilities::EchoCancellation defaultEchoCancellationCapability = RealtimeMediaSourceCapabilities::EchoCancellation::ReadWrite;

GST_DEBUG_CATEGORY(webkit_audio_capture_source_debug);
#define GST_CAT_DEFAULT webkit_audio_capture_source_debug

static void initializeGStreamerDebug()
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_audio_capture_source_debug, "webkitaudiocapturesource", 0, "WebKit Audio Capture Source.");
    });
}

class GStreamerAudioCaptureSourceFactory : public AudioCaptureFactory {
public:
    CaptureSourceOrError createAudioCaptureSource(const CaptureDevice& device, String&& hashSalt, const MediaConstraints* constraints) final
    {
        return GStreamerAudioCaptureSource::create(String { device.persistentId() }, WTFMove(hashSalt), constraints);
    }
private:
    CaptureDeviceManager& audioCaptureDeviceManager() final { return GStreamerAudioCaptureDeviceManager::singleton(); }
};

static GStreamerAudioCaptureSourceFactory& libWebRTCAudioCaptureSourceFactory()
{
    static NeverDestroyed<GStreamerAudioCaptureSourceFactory> factory;
    return factory.get();
}

CaptureSourceOrError GStreamerAudioCaptureSource::create(String&& deviceID, String&& hashSalt, const MediaConstraints* constraints)
{
    auto device = GStreamerAudioCaptureDeviceManager::singleton().gstreamerDeviceWithUID(deviceID);
    if (!device) {
        auto errorMessage = makeString("GStreamerAudioCaptureSource::create(): GStreamer did not find the device: ", deviceID, '.');
        return CaptureSourceOrError(WTFMove(errorMessage));
    }

    auto source = adoptRef(*new GStreamerAudioCaptureSource(device.value(), WTFMove(hashSalt)));

    if (constraints) {
        if (auto result = source->applyConstraints(*constraints))
            return WTFMove(result->badConstraint);
    }
    return CaptureSourceOrError(WTFMove(source));
}

AudioCaptureFactory& GStreamerAudioCaptureSource::factory()
{
    return libWebRTCAudioCaptureSourceFactory();
}

GStreamerAudioCaptureSource::GStreamerAudioCaptureSource(GStreamerCaptureDevice device, String&& hashSalt)
    : RealtimeMediaSource(RealtimeMediaSource::Type::Audio, String { device.persistentId() }, String { device.label() }, WTFMove(hashSalt))
    , m_capturer(std::make_unique<GStreamerAudioCapturer>(device))
{
    initializeGStreamerDebug();
}

GStreamerAudioCaptureSource::GStreamerAudioCaptureSource(String&& deviceID, String&& name, String&& hashSalt)
    : RealtimeMediaSource(RealtimeMediaSource::Type::Audio, WTFMove(deviceID), WTFMove(name), WTFMove(hashSalt))
    , m_capturer(std::make_unique<GStreamerAudioCapturer>())
{
    initializeGStreamerDebug();
}

GStreamerAudioCaptureSource::~GStreamerAudioCaptureSource()
{
}

void GStreamerAudioCaptureSource::startProducingData()
{
    m_capturer->setupPipeline();
    m_capturer->setSampleRate(sampleRate());
    g_signal_connect(m_capturer->sink(), "new-sample", G_CALLBACK(newSampleCallback), this);
    m_capturer->play();
}

GstFlowReturn GStreamerAudioCaptureSource::newSampleCallback(GstElement* sink, GStreamerAudioCaptureSource* source)
{
    auto sample = adoptGRef(gst_app_sink_pull_sample(GST_APP_SINK(sink)));

    // FIXME - figure out a way to avoid copying (on write) the data.
    GstBuffer* buf = gst_sample_get_buffer(sample.get());
    auto frames(std::unique_ptr<GStreamerAudioData>(new GStreamerAudioData(WTFMove(sample))));
    auto streamDesc(std::unique_ptr<GStreamerAudioStreamDescription>(new GStreamerAudioStreamDescription(frames->getAudioInfo())));

    source->audioSamplesAvailable(
        MediaTime(GST_TIME_AS_USECONDS(GST_BUFFER_PTS(buf)), G_USEC_PER_SEC),
        *frames, *streamDesc, gst_buffer_get_size(buf) / frames->getAudioInfo().bpf);

    return GST_FLOW_OK;
}

void GStreamerAudioCaptureSource::stopProducingData()
{
    g_signal_handlers_disconnect_by_func(m_capturer->sink(), reinterpret_cast<gpointer>(newSampleCallback), this);
    m_capturer->stop();
}

const RealtimeMediaSourceCapabilities& GStreamerAudioCaptureSource::capabilities()
{
    if (m_capabilities)
        return m_capabilities.value();

    uint i;
    GRefPtr<GstCaps> caps = m_capturer->caps();
    int minSampleRate = 0, maxSampleRate = 0;
    for (i = 0; i < gst_caps_get_size(caps.get()); i++) {
        int capabilityMinSampleRate = 0, capabilityMaxSampleRate = 0;
        GstStructure* str = gst_caps_get_structure(caps.get(), i);

        // Only accept raw audio for now.
        if (!gst_structure_has_name(str, "audio/x-raw"))
            continue;

        gst_structure_get(str, "rate", GST_TYPE_INT_RANGE, &capabilityMinSampleRate, &capabilityMaxSampleRate, nullptr);
        if (i > 0) {
            minSampleRate = std::min(minSampleRate, capabilityMinSampleRate);
            maxSampleRate = std::max(maxSampleRate, capabilityMaxSampleRate);
        } else {
            minSampleRate = capabilityMinSampleRate;
            maxSampleRate = capabilityMaxSampleRate;
        }
    }

    RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());
    capabilities.setDeviceId(hashedId());
    capabilities.setEchoCancellation(defaultEchoCancellationCapability);
    capabilities.setVolume(defaultVolumeCapability());
    capabilities.setSampleRate(CapabilityValueOrRange(minSampleRate, maxSampleRate));
    m_capabilities = WTFMove(capabilities);

    return m_capabilities.value();
}

void GStreamerAudioCaptureSource::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
{
    if (settings.contains(RealtimeMediaSourceSettings::Flag::SampleRate))
        m_capturer->setSampleRate(sampleRate());
}

const RealtimeMediaSourceSettings& GStreamerAudioCaptureSource::settings()
{
    if (!m_currentSettings) {
        RealtimeMediaSourceSettings settings;
        settings.setDeviceId(hashedId());

        RealtimeMediaSourceSupportedConstraints supportedConstraints;
        supportedConstraints.setSupportsDeviceId(true);
        supportedConstraints.setSupportsEchoCancellation(true);
        supportedConstraints.setSupportsVolume(true);
        supportedConstraints.setSupportsSampleRate(true);
        settings.setSupportedConstraints(supportedConstraints);

        m_currentSettings = WTFMove(settings);
    }

    m_currentSettings->setVolume(volume());
    m_currentSettings->setSampleRate(sampleRate());
    m_currentSettings->setEchoCancellation(echoCancellation());

    return m_currentSettings.value();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
