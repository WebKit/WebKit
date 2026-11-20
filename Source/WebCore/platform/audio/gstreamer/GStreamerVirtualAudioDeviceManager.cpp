/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GStreamerVirtualAudioDeviceManager.h"

#if ENABLE(VIRTUAL_AUDIO_DEVICES)

#include "GStreamerAudioMixer.h"
#include "GStreamerCommon.h"
#include "GStreamerQuirks.h"
#include <array>
#include <bit>
#include <gst/app/gstappsink.h>
#include <gst/audio/audio.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/GLibExtras.h>
#include <wtf/glib/WTFGType.h>

GST_DEBUG_CATEGORY_STATIC(webkit_virtual_audio_gst_device_debug);
#define GST_CAT_DEFAULT webkit_virtual_audio_gst_device_debug

typedef struct _WebKitVirtualAudioGstDevice WebKitVirtualAudioGstDevice;
typedef struct _WebKitVirtualAudioGstDeviceClass WebKitVirtualAudioGstDeviceClass;
typedef struct _WebKitVirtualAudioGstDevicePrivate WebKitVirtualAudioGstDevicePrivate;

struct _WebKitVirtualAudioGstDevice {
    GstDevice parent;
    WebKitVirtualAudioGstDevicePrivate* priv;
};

struct _WebKitVirtualAudioGstDeviceClass {
    GstDeviceClass parentClass;
};

struct _WebKitVirtualAudioGstDevicePrivate {
    WebCore::VirtualAudioDeviceConfiguration configuration;
    GRefPtr<GstCaps> caps;

    // The sinks of the pages owning the device, the audio is delivered to the last one.
    Lock sinksLock;
    Vector<std::pair<WebCore::PageIdentifier, Ref<WebCore::VirtualAudioDeviceSink>>> sinks WTF_GUARDED_BY_LOCK(sinksLock);
};

GType webkit_virtual_audio_gst_device_get_type();
#define WEBKIT_TYPE_VIRTUAL_AUDIO_GST_DEVICE (webkit_virtual_audio_gst_device_get_type())
#define WEBKIT_VIRTUAL_AUDIO_GST_DEVICE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_VIRTUAL_AUDIO_GST_DEVICE, WebKitVirtualAudioGstDevice))
#define WEBKIT_IS_VIRTUAL_AUDIO_GST_DEVICE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_VIRTUAL_AUDIO_GST_DEVICE))

WEBKIT_DEFINE_TYPE(WebKitVirtualAudioGstDevice, webkit_virtual_audio_gst_device, GST_TYPE_DEVICE)

static RefPtr<WebCore::VirtualAudioDeviceSink> webkitVirtualAudioGstDeviceActiveSink(WebKitVirtualAudioGstDevice* device)
{
    auto* priv = device->priv;
    Locker locker { priv->sinksLock };
    if (priv->sinks.isEmpty())
        return nullptr;
    return priv->sinks.last().second.ptr();
}

// State of the appsink of a virtual audio device element, only used from its streaming thread.
struct VirtualAudioDeviceSinkState {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(VirtualAudioDeviceSinkState);

    // The sink receiving the audio can change while the element is alive, see
    // GStreamerVirtualAudioDeviceManager::registerDevice().
    GRefPtr<GstDevice> device;
    // The appsink caps are the fixed device caps, so the audio info is the same for every buffer.
    GstAudioInfo info;
    // Every buffer delivered to the sink contains exactly this number of frames.
    unsigned framesPerBuffer;
    std::optional<uint64_t> nextTimestamp;
    // Reused to deliver the planes of non-interleaved buffers one after the other, and to pad
    // short buffers.
    Vector<uint8_t> outputBuffer;
    bool didWarnAboutLongBuffer { false };
};

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(VirtualAudioDeviceSinkState);

static uint64_t virtualAudioDeviceBufferFrames(const VirtualAudioDeviceSinkState& state, GstBuffer* buffer)
{
    // Non-interleaved buffers may contain padding between the planes.
    auto* audioMeta = gst_buffer_get_audio_meta(buffer);
    return audioMeta ? audioMeta->samples : gst_buffer_get_size(buffer) / GST_AUDIO_INFO_BPF(&state.info);
}

static void virtualAudioDeviceAppendSilence(VirtualAudioDeviceSinkState& state, size_t size)
{
    auto offset = state.outputBuffer.size();
    state.outputBuffer.grow(offset + size);
    gst_audio_format_info_fill_silence(state.info.finfo, state.outputBuffer.mutableSpan().subspan(offset).data(), size);
}

static uint64_t virtualAudioDeviceSampleTimestamp(VirtualAudioDeviceSinkState& state, GstSample* sample, GstBuffer* buffer)
{
    // Timestamps are in the running time of the device stream, so that they are contiguous
    // while audio is rendered on the device. Buffers without a valid timestamp are assumed to
    // follow the previous one.
    std::optional<uint64_t> timestamp;
    auto* segment = gst_sample_get_segment(sample);
    if (segment && segment->format == GST_FORMAT_TIME && GST_BUFFER_PTS_IS_VALID(buffer)) {
        auto runningTime = gst_segment_to_running_time(segment, GST_FORMAT_TIME, GST_BUFFER_PTS(buffer));
        if (GST_CLOCK_TIME_IS_VALID(runningTime))
            timestamp = runningTime;
    }
    if (!timestamp)
        timestamp = state.nextTimestamp.value_or(0);

    // Short buffers are padded with silence: every delivered buffer lasts framesPerBuffer frames.
    state.nextTimestamp = *timestamp + GST_FRAMES_TO_CLOCK_TIME(state.framesPerBuffer, GST_AUDIO_INFO_RATE(&state.info));
    return *timestamp;
}

static void virtualAudioDeviceRenderSample(VirtualAudioDeviceSinkState& state, GstSample* sample)
{
    auto* buffer = gst_sample_get_buffer(sample);
    if (!buffer)
        return;

    RefPtr sink = webkitVirtualAudioGstDeviceActiveSink(WEBKIT_VIRTUAL_AUDIO_GST_DEVICE(state.device.get()));
    if (!sink)
        return;

    // The mixer is configured to output buffers of the device size, but the last buffer before
    // a flush or the end of the stream can be shorter: pad it with silence, so that the sink
    // always receives buffers of the configured number of frames.
    auto frames = virtualAudioDeviceBufferFrames(state, buffer);
    if (!frames)
        return;
    if (frames > state.framesPerBuffer) {
        if (!state.didWarnAboutLongBuffer) {
            GST_WARNING("Dropping buffers of %" G_GUINT64_FORMAT " frames, longer than the %u frames of the device", frames, state.framesPerBuffer);
            state.didWarnAboutLongBuffer = true;
        }
        return;
    }
    size_t paddingFrames = state.framesPerBuffer - frames;

    auto timestamp = virtualAudioDeviceSampleTimestamp(state, sample, buffer);

    if (GST_AUDIO_INFO_LAYOUT(&state.info) == GST_AUDIO_LAYOUT_INTERLEAVED) {
        WebCore::GstMappedBuffer mappedBuffer(buffer, GST_MAP_READ);
        if (!mappedBuffer)
            return;
        auto data = mappedBuffer.span<uint8_t>().first(frames * GST_AUDIO_INFO_BPF(&state.info));
        if (!paddingFrames) {
            sink->renderAudioData(data, timestamp);
            return;
        }
        state.outputBuffer.shrink(0);
        state.outputBuffer.append(data);
        virtualAudioDeviceAppendSilence(state, paddingFrames * GST_AUDIO_INFO_BPF(&state.info));
        sink->renderAudioData(state.outputBuffer.span(), timestamp);
        return;
    }

    // Planes of non-interleaved buffers are not necessarily contiguous, nor at the start of the
    // buffer, even with a single channel: make sure the planes are delivered one after the other.
    WebCore::GstMappedAudioBuffer mappedBuffer(buffer, state.info, GST_MAP_READ);
    if (!mappedBuffer)
        return;

    auto planes = mappedBuffer.samples<uint8_t>(0);
    size_t planeSize = planes[0].size();
    auto firstPlaneAddress = reinterpret_cast<uintptr_t>(planes[0].data());
    bool isContiguous = true;
    for (size_t i = 1; i < planes.size() && isContiguous; ++i)
        isContiguous = reinterpret_cast<uintptr_t>(planes[i].data()) == firstPlaneAddress + i * planeSize;

    if (isContiguous && !paddingFrames) {
        sink->renderAudioData(unsafeMakeSpan(planes[0].data(), planeSize * planes.size()), timestamp);
        return;
    }

    size_t paddingSize = paddingFrames * GST_AUDIO_INFO_BPS(&state.info);
    state.outputBuffer.shrink(0);
    state.outputBuffer.reserveCapacity((planeSize + paddingSize) * planes.size());
    for (auto plane : planes) {
        state.outputBuffer.append(plane);
        if (paddingSize)
            virtualAudioDeviceAppendSilence(state, paddingSize);
    }
    sink->renderAudioData(state.outputBuffer.span(), timestamp);
}

static GstElement* webkitVirtualAudioGstDeviceCreateElement(GstDevice* device, const gchar* name)
{
    auto* priv = WEBKIT_VIRTUAL_AUDIO_GST_DEVICE(device)->priv;

    GRefPtr<GstElement> audioConvert = WebCore::makeGStreamerElement("audioconvert"_s);
    GRefPtr<GstElement> audioResample = WebCore::makeGStreamerElement("audioresample"_s);
    GRefPtr<GstElement> appSink = WebCore::makeGStreamerElement("appsink"_s);
    if (!audioConvert || !audioResample || !appSink) {
        GST_ERROR_OBJECT(device, "Missing required elements to create the virtual audio device sink");
        ASSERT_NOT_REACHED_WITH_MESSAGE("Missing required elements to create the virtual audio device sink");
        return nullptr;
    }

    GstAudioInfo info;
    if (!gst_audio_info_from_caps(&info, priv->caps.get()) || !GST_AUDIO_INFO_BPF(&info)) {
        GST_ERROR_OBJECT(device, "Invalid virtual audio device caps %" GST_PTR_FORMAT, priv->caps.get());
        ASSERT_NOT_REACHED_WITH_MESSAGE("Invalid virtual audio device caps");
        return nullptr;
    }

    // The sink synchronizes on the clock, so that the audio is delivered in real time like
    // a real audio output device would consume it.
    g_object_set(appSink.get(), "caps", priv->caps.get(), "sync", TRUE, "enable-last-sample", FALSE, "emit-signals", FALSE, nullptr);

    GstAppSinkCallbacks callbacks { };
    callbacks.new_sample = [](GstAppSink* appSink, gpointer userData) -> GstFlowReturn {
        GRefPtr<GstSample> sample = adoptGRef(gst_app_sink_pull_sample(appSink));
        if (sample)
            virtualAudioDeviceRenderSample(*static_cast<VirtualAudioDeviceSinkState*>(userData), sample.get());
        return GST_FLOW_OK;
    };
    callbacks.new_preroll = [](GstAppSink* appSink, gpointer) -> GstFlowReturn {
        // The preroll sample is rendered again in PLAYING, just drop it.
        GRefPtr<GstSample> sample = adoptGRef(gst_app_sink_pull_preroll(appSink));
        return GST_FLOW_OK;
    };
    auto* state = new VirtualAudioDeviceSinkState { GRefPtr { device }, info, priv->configuration.framesPerBuffer, std::nullopt, { } };
    gst_app_sink_set_callbacks(GST_APP_SINK(appSink.get()), &callbacks, state, [](gpointer userData) {
        delete static_cast<VirtualAudioDeviceSinkState*>(userData);
    });

    // The bin is returned floating, as expected from GstDevice::create_element.
    auto* bin = gst_bin_new(name);
    gst_bin_add_many(GST_BIN_CAST(bin), audioConvert.get(), audioResample.get(), appSink.get(), nullptr);
    if (!gst_element_link_many(audioConvert.get(), audioResample.get(), appSink.get(), nullptr)) {
        GST_ERROR_OBJECT(device, "Failed to link the virtual audio device sink elements");
        ASSERT_NOT_REACHED_WITH_MESSAGE("Failed to link the virtual audio device sink elements");
        // Sink the floating reference before releasing the bin.
        gst_object_unref(gst_object_ref_sink(bin));
        return nullptr;
    }

    GRefPtr pad = adoptGRef(gst_element_get_static_pad(audioConvert.get(), "sink"));
    gst_element_add_pad(bin, gst_ghost_pad_new("sink", pad.get()));

    GST_DEBUG_OBJECT(device, "Created virtual audio sink %" GST_PTR_FORMAT " with caps %" GST_PTR_FORMAT, bin, priv->caps.get());
    return bin;
}

static gboolean webkitVirtualAudioGstDeviceReconfigureElement(GstDevice*, GstElement*)
{
    // Virtual devices can't reconfigure other sinks, switching to and from a virtual
    // device is handled by the audio mixer.
    return FALSE;
}

static void webkit_virtual_audio_gst_device_class_init(WebKitVirtualAudioGstDeviceClass* klass)
{
    auto* deviceClass = GST_DEVICE_CLASS(klass);
    deviceClass->create_element = webkitVirtualAudioGstDeviceCreateElement;
    deviceClass->reconfigure_element = webkitVirtualAudioGstDeviceReconfigureElement;
}

namespace WebCore {

static GstAudioFormat virtualAudioDeviceGstAudioFormat(VirtualAudioSampleFormat format)
{
    switch (format) {
    case VirtualAudioSampleFormat::S8:
        return GST_AUDIO_FORMAT_S8;
    case VirtualAudioSampleFormat::U8:
        return GST_AUDIO_FORMAT_U8;
    case VirtualAudioSampleFormat::S16LittleEndian:
        return GST_AUDIO_FORMAT_S16LE;
    case VirtualAudioSampleFormat::S16BigEndian:
        return GST_AUDIO_FORMAT_S16BE;
    case VirtualAudioSampleFormat::U16LittleEndian:
        return GST_AUDIO_FORMAT_U16LE;
    case VirtualAudioSampleFormat::U16BigEndian:
        return GST_AUDIO_FORMAT_U16BE;
    case VirtualAudioSampleFormat::S24LittleEndian:
        return GST_AUDIO_FORMAT_S24LE;
    case VirtualAudioSampleFormat::S24BigEndian:
        return GST_AUDIO_FORMAT_S24BE;
    case VirtualAudioSampleFormat::S24In32LittleEndian:
        return GST_AUDIO_FORMAT_S24_32LE;
    case VirtualAudioSampleFormat::S24In32BigEndian:
        return GST_AUDIO_FORMAT_S24_32BE;
    case VirtualAudioSampleFormat::S32LittleEndian:
        return GST_AUDIO_FORMAT_S32LE;
    case VirtualAudioSampleFormat::S32BigEndian:
        return GST_AUDIO_FORMAT_S32BE;
    case VirtualAudioSampleFormat::U32LittleEndian:
        return GST_AUDIO_FORMAT_U32LE;
    case VirtualAudioSampleFormat::U32BigEndian:
        return GST_AUDIO_FORMAT_U32BE;
    case VirtualAudioSampleFormat::F32LittleEndian:
        return GST_AUDIO_FORMAT_F32LE;
    case VirtualAudioSampleFormat::F32BigEndian:
        return GST_AUDIO_FORMAT_F32BE;
    case VirtualAudioSampleFormat::F64LittleEndian:
        return GST_AUDIO_FORMAT_F64LE;
    case VirtualAudioSampleFormat::F64BigEndian:
        return GST_AUDIO_FORMAT_F64BE;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static GRefPtr<GstCaps> virtualAudioDeviceCapsForConfiguration(const VirtualAudioDeviceConfiguration& configuration)
{
    if (!configuration.sampleRate || !configuration.numberOfChannels || configuration.numberOfChannels > maximumVirtualAudioDeviceChannels)
        return nullptr;

    auto channels = static_cast<int>(configuration.numberOfChannels);
    auto channelMask = configuration.channelMask;
    if (channelMask && static_cast<unsigned>(std::popcount(channelMask)) != configuration.numberOfChannels) {
        GST_WARNING("Channel mask 0x%" G_GINT64_MODIFIER "x doesn't match the number of channels %d, using default positions", channelMask, channels);
        channelMask = 0;
    }
    if (!channelMask && channels > 2)
        channelMask = gst_audio_channel_get_fallback_mask(channels);

    std::array<GstAudioChannelPosition, 64> positions;
    GstAudioChannelPosition* positionsPointer = nullptr;
    if (channelMask && gst_audio_channel_positions_from_mask(channels, channelMask, positions.data()))
        positionsPointer = positions.data();

    GstAudioInfo info;
    gst_audio_info_init(&info);
    gst_audio_info_set_format(&info, virtualAudioDeviceGstAudioFormat(configuration.format), static_cast<int>(configuration.sampleRate), channels, positionsPointer);
    info.layout = configuration.layout == VirtualAudioLayout::NonInterleaved ? GST_AUDIO_LAYOUT_NON_INTERLEAVED : GST_AUDIO_LAYOUT_INTERLEAVED;
    return adoptGRef(gst_audio_info_to_caps(&info));
}

GStreamerVirtualAudioDeviceManager& GStreamerVirtualAudioDeviceManager::singleton()
{
    static NeverDestroyed<GStreamerVirtualAudioDeviceManager> manager;
    return manager;
}

GStreamerVirtualAudioDeviceManager::GStreamerVirtualAudioDeviceManager()
{
    ensureGStreamerInitialized();
    GST_DEBUG_CATEGORY_INIT(webkit_virtual_audio_gst_device_debug, "webkitvirtualaudiodevice", 0, "WebKit virtual audio device");
}

bool GStreamerVirtualAudioDeviceManager::registerDevice(PageIdentifier owner, const String& persistentId, const VirtualAudioDeviceConfiguration& configuration, Ref<VirtualAudioDeviceSink>&& sink)
{
    {
        // A device already registered by another page, or still alive after its last owner
        // unregistered it, only gets a new sink, so that the mixer pipelines created for it are
        // reused and deliver to the new sink.
        Locker locker { m_lock };
        m_unownedDevices.removeIf([](auto& entry) {
            return !entry.value.get();
        });
        GRefPtr<GstDevice> existingDevice = m_devices.get(persistentId);
        if (!existingDevice) {
            if (auto it = m_unownedDevices.find(persistentId); it != m_unownedDevices.end())
                existingDevice = it->value.get();
        }
        if (existingDevice) {
            auto* priv = WEBKIT_VIRTUAL_AUDIO_GST_DEVICE(existingDevice.get())->priv;
            if (priv->configuration != configuration) {
                GST_WARNING("Virtual audio device %s is already registered with another configuration", makeUTF8CString(persistentId).legacyCStringPointer());
                return false;
            }

            GST_INFO_OBJECT(existingDevice.get(), "Registering virtual audio device %s for another page", makeUTF8CString(persistentId).legacyCStringPointer());
            {
                Locker sinksLocker { priv->sinksLock };
                priv->sinks.removeAllMatching([&](auto& item) {
                    return item.first == owner;
                });
                priv->sinks.append({ owner, WTF::move(sink) });
            }
            m_unownedDevices.remove(persistentId);
            m_devices.set(persistentId, WTF::move(existingDevice));
            return true;
        }
    }

    // The mixer is needed to mix the producers rendering on the device and to deliver buffers
    // of the configured size.
    if (!GStreamerAudioMixer::isAvailable()) {
        GST_WARNING("The audiomixer and inter GStreamer elements are required by virtual audio devices");
        return false;
    }
    if (!GStreamerQuirksManager::singleton().doesSupportAudioMixer()) {
        GST_WARNING("Virtual audio devices are not supported on this platform, the audio can't be rendered through the audio mixer");
        return false;
    }

    // The device element converts the mixed audio to the device format and hands it to the sink.
    if (!isGStreamerPluginAvailable("app"_s) || !isGStreamerPluginAvailable("audioconvert"_s) || !isGStreamerPluginAvailable("audioresample"_s)) {
        GST_WARNING("The appsink, audioconvert and audioresample GStreamer elements are required by virtual audio devices");
        return false;
    }

    GRefPtr<GstCaps> caps = virtualAudioDeviceCapsForConfiguration(configuration);
    if (!caps || !configuration.framesPerBuffer) {
        GST_WARNING("Invalid configuration for virtual audio device %s", makeUTF8CString(persistentId).legacyCStringPointer());
        return false;
    }

    GRefPtr<GstDevice> device = GST_DEVICE_CAST(gObjectNew(WEBKIT_TYPE_VIRTUAL_AUDIO_GST_DEVICE,
        "display-name", makeUTF8CString(configuration.label),
        "device-class", "Audio/Sink"_s,
        "caps", caps.get()));
    auto* priv = WEBKIT_VIRTUAL_AUDIO_GST_DEVICE(device.get())->priv;
    priv->configuration = configuration;
    priv->caps = WTF::move(caps);
    {
        Locker sinksLocker { priv->sinksLock };
        priv->sinks.append({ owner, WTF::move(sink) });
    }

    GST_INFO_OBJECT(device.get(), "Registering virtual audio device %s with caps %" GST_PTR_FORMAT, makeUTF8CString(persistentId).legacyCStringPointer(), priv->caps.get());
    Locker locker { m_lock };
    m_devices.set(persistentId, WTF::move(device));
    return true;
}

void GStreamerVirtualAudioDeviceManager::unregisterDevices(PageIdentifier owner)
{
    Locker locker { m_lock };
    Vector<std::pair<String, GRefPtr<GstDevice>>> unownedDevices;
    m_devices.removeIf([&](auto& entry) {
        auto* priv = WEBKIT_VIRTUAL_AUDIO_GST_DEVICE(entry.value.get())->priv;
        Locker sinksLocker { priv->sinksLock };
        if (!priv->sinks.removeAllMatching([&](auto& item) { return item.first == owner; }))
            return false;
        if (!priv->sinks.isEmpty())
            return false;
        GST_INFO("Unregistering virtual audio device %s", makeUTF8CString(entry.key).legacyCStringPointer());
        unownedDevices.append({ entry.key, entry.value });
        return true;
    });
    for (auto& [persistentId, device] : unownedDevices) {
        m_unownedDevices.remove(persistentId);
        m_unownedDevices.add(persistentId, GThreadSafeWeakPtr<GstDevice> { device.get() });
    }
}

GRefPtr<GstDevice> GStreamerVirtualAudioDeviceManager::deviceForPersistentId(const String& persistentId) const
{
    if (persistentId.isEmpty())
        return nullptr;

    Locker locker { m_lock };
    return m_devices.get(persistentId);
}

bool GStreamerVirtualAudioDeviceManager::isVirtualDevice(GstDevice* device)
{
    return device && WEBKIT_IS_VIRTUAL_AUDIO_GST_DEVICE(device);
}

const VirtualAudioDeviceConfiguration* GStreamerVirtualAudioDeviceManager::configuration(GstDevice* device)
{
    if (!isVirtualDevice(device))
        return nullptr;
    return &WEBKIT_VIRTUAL_AUDIO_GST_DEVICE(device)->priv->configuration;
}

} // namespace WebCore

#undef GST_CAT_DEFAULT

#endif // ENABLE(VIRTUAL_AUDIO_DEVICES)
