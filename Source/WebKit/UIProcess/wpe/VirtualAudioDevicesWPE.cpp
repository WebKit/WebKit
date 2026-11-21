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
#include "VirtualAudioDevicesWPE.h"

#if ENABLE(VIRTUAL_AUDIO_DEVICES)

#include "APIPageConfiguration.h"
#include "WPEVirtualAudioDevicePrivate.h"
#include <wtf/text/MakeString.h>
#include <wtf/text/UTF8CStringView.h>

namespace WebKit {

static WebCore::VirtualAudioSampleFormat toVirtualAudioSampleFormat(WPEAudioSampleFormat format)
{
    switch (format) {
    case WPE_AUDIO_SAMPLE_FORMAT_S8:
        return WebCore::VirtualAudioSampleFormat::S8;
    case WPE_AUDIO_SAMPLE_FORMAT_U8:
        return WebCore::VirtualAudioSampleFormat::U8;
    case WPE_AUDIO_SAMPLE_FORMAT_S16LE:
        return WebCore::VirtualAudioSampleFormat::S16LittleEndian;
    case WPE_AUDIO_SAMPLE_FORMAT_S16BE:
        return WebCore::VirtualAudioSampleFormat::S16BigEndian;
    case WPE_AUDIO_SAMPLE_FORMAT_U16LE:
        return WebCore::VirtualAudioSampleFormat::U16LittleEndian;
    case WPE_AUDIO_SAMPLE_FORMAT_U16BE:
        return WebCore::VirtualAudioSampleFormat::U16BigEndian;
    case WPE_AUDIO_SAMPLE_FORMAT_S24LE:
        return WebCore::VirtualAudioSampleFormat::S24LittleEndian;
    case WPE_AUDIO_SAMPLE_FORMAT_S24BE:
        return WebCore::VirtualAudioSampleFormat::S24BigEndian;
    case WPE_AUDIO_SAMPLE_FORMAT_S24_32LE:
        return WebCore::VirtualAudioSampleFormat::S24In32LittleEndian;
    case WPE_AUDIO_SAMPLE_FORMAT_S24_32BE:
        return WebCore::VirtualAudioSampleFormat::S24In32BigEndian;
    case WPE_AUDIO_SAMPLE_FORMAT_S32LE:
        return WebCore::VirtualAudioSampleFormat::S32LittleEndian;
    case WPE_AUDIO_SAMPLE_FORMAT_S32BE:
        return WebCore::VirtualAudioSampleFormat::S32BigEndian;
    case WPE_AUDIO_SAMPLE_FORMAT_U32LE:
        return WebCore::VirtualAudioSampleFormat::U32LittleEndian;
    case WPE_AUDIO_SAMPLE_FORMAT_U32BE:
        return WebCore::VirtualAudioSampleFormat::U32BigEndian;
    case WPE_AUDIO_SAMPLE_FORMAT_F32LE:
        return WebCore::VirtualAudioSampleFormat::F32LittleEndian;
    case WPE_AUDIO_SAMPLE_FORMAT_F32BE:
        return WebCore::VirtualAudioSampleFormat::F32BigEndian;
    case WPE_AUDIO_SAMPLE_FORMAT_F64LE:
        return WebCore::VirtualAudioSampleFormat::F64LittleEndian;
    case WPE_AUDIO_SAMPLE_FORMAT_F64BE:
        return WebCore::VirtualAudioSampleFormat::F64BigEndian;
    }

    return WebCore::VirtualAudioSampleFormat::F32LittleEndian;
}

Vector<VirtualAudioDevice> virtualAudioDevices(const API::PageConfiguration& pageConfiguration)
{
    Vector<VirtualAudioDevice> devices;
    bool hasDefaultDevice = false;
    for (auto& device : pageConfiguration.virtualAudioDevices()) {
        WebCore::VirtualAudioDeviceConfiguration configuration {
            .name = String::fromUTF8(UTF8CStringView::unsafeFromUTF8(wpe_virtual_audio_device_get_name(device.get())).span()),
            .label = String::fromUTF8(UTF8CStringView::unsafeFromUTF8(wpe_virtual_audio_device_get_description(device.get())).span()),
            .format = toVirtualAudioSampleFormat(wpe_virtual_audio_device_get_format(device.get())),
            .sampleRate = wpe_virtual_audio_device_get_rate(device.get()),
            .numberOfChannels = wpe_virtual_audio_device_get_channels(device.get()),
            .channelMask = wpe_virtual_audio_device_get_channel_mask(device.get()),
            .layout = wpe_virtual_audio_device_get_layout(device.get()) == WPE_AUDIO_LAYOUT_NON_INTERLEAVED ? WebCore::VirtualAudioLayout::NonInterleaved : WebCore::VirtualAudioLayout::Interleaved,
            .framesPerBuffer = wpe_virtual_audio_device_get_frames_per_buffer(device.get()),
            .isDefault = !hasDefaultDevice && wpe_virtual_audio_device_get_is_default(device.get())
        };
        if (configuration.label.isEmpty())
            configuration.label = configuration.name;
        hasDefaultDevice |= configuration.isDefault;
        devices.append({ device, WTF::move(configuration) });
    }
    return devices;
}

String virtualAudioDevicePersistentId(WebPageProxyIdentifier pageID, const String& name)
{
    return makeString("virtual-audio-device:"_s, pageID.toUInt64(), ':', name);
}

Vector<WebCore::CaptureDevice> virtualAudioOutputCaptureDevices(WebPageProxyIdentifier pageID, const API::PageConfiguration& pageConfiguration)
{
    Vector<WebCore::CaptureDevice> captureDevices;
    for (auto& device : virtualAudioDevices(pageConfiguration)) {
        auto& configuration = device.configuration;
        auto persistentId = virtualAudioDevicePersistentId(pageID, configuration.name);
        WebCore::CaptureDevice captureDevice(persistentId, WebCore::CaptureDevice::DeviceType::Speaker, configuration.label, persistentId, true, configuration.isDefault);
        if (configuration.isDefault)
            captureDevices.insert(0, WTF::move(captureDevice));
        else
            captureDevices.append(WTF::move(captureDevice));
    }
    return captureDevices;
}

} // namespace WebKit

#endif // ENABLE(VIRTUAL_AUDIO_DEVICES)
