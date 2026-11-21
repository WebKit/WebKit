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

#include "GRefPtrGStreamer.h"
#include "PageIdentifier.h"
#include "VirtualAudioDevice.h"
#include <span>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/glib/GThreadSafeWeakPtr.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

// Receives the audio rendered on a virtual audio device. renderAudioData() is called from a
// GStreamer streaming thread, with the data formatted as described by the device configuration,
// and the timestamp of its first sample in nanoseconds of the device stream running time.
class VirtualAudioDeviceSink : public ThreadSafeRefCounted<VirtualAudioDeviceSink> {
public:
    virtual ~VirtualAudioDeviceSink() = default;
    virtual void renderAudioData(std::span<const uint8_t>, uint64_t timestamp) = 0;
};

// Registry of the virtual audio output devices of the current process. Virtual devices are
// exposed to the GStreamer audio pipelines as GstDevices, keyed by their persistent ID, whose
// element converts the audio to the device format and hands it to a VirtualAudioDeviceSink.
// Virtual devices are always rendered by the GStreamerAudioMixer, so that all the producers
// rendering on the same device are mixed. The mixer of a virtual device outputs buffers of the
// device rate and number of frames, see configuration().
//
// Devices are registered by owner, the page using them. Several pages of the process can register
// the same device, for instance a page kept in the back/forward cache and the page that replaced
// it: the GstDevice is shared, so that the mixer pipelines it already created are reused, and the
// audio is delivered to the sink of the last owner that registered it. The device is removed when
// its last owner unregisters it. A device registered again while the GstDevice is still alive, for
// instance kept by an idle mixer pipeline, reuses it, so that the existing mixer pipelines deliver
// the audio to the new owner. A persistent ID always has the same configuration: registering it
// with another configuration fails.
class GStreamerVirtualAudioDeviceManager {
    friend NeverDestroyed<GStreamerVirtualAudioDeviceManager>;
public:
    WEBCORE_EXPORT static GStreamerVirtualAudioDeviceManager& singleton();

    // Returns false if the device can't be registered, because its configuration is invalid or
    // because the GStreamer elements needed by virtual devices are not available.
    WEBCORE_EXPORT bool registerDevice(PageIdentifier owner, const String& persistentId, const VirtualAudioDeviceConfiguration&, Ref<VirtualAudioDeviceSink>&&);
    WEBCORE_EXPORT void unregisterDevices(PageIdentifier owner);

    GRefPtr<GstDevice> deviceForPersistentId(const String&) const;
    static bool isVirtualDevice(GstDevice*);
    // The configuration of a virtual device, null for other devices. It never changes and lives as
    // long as the device.
    static const VirtualAudioDeviceConfiguration* configuration(GstDevice*);

private:
    GStreamerVirtualAudioDeviceManager();

    mutable Lock m_lock;
    HashMap<String, GRefPtr<GstDevice>> m_devices WTF_GUARDED_BY_LOCK(m_lock);
    // Devices without owners, that may still be used by mixer pipelines.
    HashMap<String, GThreadSafeWeakPtr<GstDevice>> m_unownedDevices WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace WebCore

#endif // ENABLE(VIRTUAL_AUDIO_DEVICES)
