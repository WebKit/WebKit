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
#include "VirtualAudioDevicesGLib.h"

#if ENABLE(VIRTUAL_AUDIO_DEVICES)

#include "APIPageConfiguration.h"
#include "WebKitVirtualAudioDevicePrivate.h"
#include <wtf/text/MakeString.h>

namespace WebKit {

Vector<VirtualAudioDeviceWithConfiguration> virtualAudioDevices(const API::PageConfiguration& pageConfiguration)
{
    Vector<VirtualAudioDeviceWithConfiguration> devices;
    bool hasDefaultDevice = false;
    for (auto& device : pageConfiguration.virtualAudioDevices()) {
        WebCore::VirtualAudioDeviceConfiguration configuration {
            .name = unsafeSpan(byteCast<char8_t>(webkit_virtual_audio_device_get_name(device.get()))),
            .label = unsafeSpan(byteCast<char8_t>(webkit_virtual_audio_device_get_description(device.get()))),
            .format = webkitAudioSampleFormatToVirtualAudioSampleFormat(webkit_virtual_audio_device_get_format(device.get())),
            .sampleRate = webkit_virtual_audio_device_get_rate(device.get()),
            .numberOfChannels = webkit_virtual_audio_device_get_channels(device.get()),
            .channelMask = webkit_virtual_audio_device_get_channel_mask(device.get()),
            .layout = webkit_virtual_audio_device_get_layout(device.get()) == WEBKIT_AUDIO_LAYOUT_NON_INTERLEAVED ? WebCore::VirtualAudioLayout::NonInterleaved : WebCore::VirtualAudioLayout::Interleaved,
            .framesPerBuffer = webkit_virtual_audio_device_get_frames_per_buffer(device.get()),
            .isDefault = !hasDefaultDevice && webkit_virtual_audio_device_get_is_default(device.get())
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
        captureDevices.append({ persistentId, WebCore::CaptureDevice::DeviceType::Speaker, configuration.label, persistentId, true, configuration.isDefault });
    }
    return captureDevices;
}

} // namespace WebKit

#endif // ENABLE(VIRTUAL_AUDIO_DEVICES)
