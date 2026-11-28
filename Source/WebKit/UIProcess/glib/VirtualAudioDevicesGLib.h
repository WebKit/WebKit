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

#pragma once

#if ENABLE(VIRTUAL_AUDIO_DEVICES)

#include "WebPageProxyIdentifier.h"
#include <WebCore/CaptureDevice.h>
#include <WebCore/VirtualAudioDevice.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>

typedef struct _WebKitVirtualAudioDevice WebKitVirtualAudioDevice;

namespace API {
class PageConfiguration;
}

namespace WebKit {

struct VirtualAudioDeviceWithConfiguration {
    GRefPtr<WebKitVirtualAudioDevice> device;
    WebCore::VirtualAudioDeviceConfiguration configuration;
};

// The virtual audio devices of a page configuration, already validated by WebKitWebView. Only the
// first device marked as default is the default device.
Vector<VirtualAudioDeviceWithConfiguration> virtualAudioDevices(const API::PageConfiguration&);

// The persistent ID of a device is the same in every web process of the page, so that web
// content keeps its device selection across process swaps.
String virtualAudioDevicePersistentId(WebPageProxyIdentifier, const String& name);

// The virtual audio devices of a page configuration as speakers whose group ID is their persistent
// ID, in configuration order. They are exposed to web content without requiring microphone access:
// providing them counts as the user's consent.
Vector<WebCore::CaptureDevice> virtualAudioOutputCaptureDevices(WebPageProxyIdentifier, const API::PageConfiguration&);

} // namespace WebKit

#endif // ENABLE(VIRTUAL_AUDIO_DEVICES)
