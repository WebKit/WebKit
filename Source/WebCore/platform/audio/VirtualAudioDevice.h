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

#include <span>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// Audio output devices implemented by the embedder. They are exposed to web content as audio
// output devices and the audio rendered on them is delivered to the embedder.

enum class VirtualAudioSampleFormat : uint8_t {
    S8,
    U8,
    S16LittleEndian,
    S16BigEndian,
    U16LittleEndian,
    U16BigEndian,
    S24LittleEndian,
    S24BigEndian,
    S24In32LittleEndian,
    S24In32BigEndian,
    S32LittleEndian,
    S32BigEndian,
    U32LittleEndian,
    U32BigEndian,
    F32LittleEndian,
    F32BigEndian,
    F64LittleEndian,
    F64BigEndian
};

constexpr size_t virtualAudioSampleSize(VirtualAudioSampleFormat format)
{
    switch (format) {
    case VirtualAudioSampleFormat::S8:
    case VirtualAudioSampleFormat::U8:
        return 1;
    case VirtualAudioSampleFormat::S16LittleEndian:
    case VirtualAudioSampleFormat::S16BigEndian:
    case VirtualAudioSampleFormat::U16LittleEndian:
    case VirtualAudioSampleFormat::U16BigEndian:
        return 2;
    case VirtualAudioSampleFormat::S24LittleEndian:
    case VirtualAudioSampleFormat::S24BigEndian:
        return 3;
    case VirtualAudioSampleFormat::S24In32LittleEndian:
    case VirtualAudioSampleFormat::S24In32BigEndian:
    case VirtualAudioSampleFormat::S32LittleEndian:
    case VirtualAudioSampleFormat::S32BigEndian:
    case VirtualAudioSampleFormat::U32LittleEndian:
    case VirtualAudioSampleFormat::U32BigEndian:
    case VirtualAudioSampleFormat::F32LittleEndian:
    case VirtualAudioSampleFormat::F32BigEndian:
        return 4;
    case VirtualAudioSampleFormat::F64LittleEndian:
    case VirtualAudioSampleFormat::F64BigEndian:
        return 8;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static constexpr uint32_t maximumVirtualAudioDeviceChannels = 64;

// Audio buffers are sent as single packets to the UI process, keep them well below the default
// maximum size of a socket buffer.
static constexpr size_t maximumVirtualAudioDeviceBufferSize = 64 * 1024;

enum class VirtualAudioLayout : bool {
    Interleaved,
    NonInterleaved
};

struct VirtualAudioDeviceConfiguration {
    String name; // Identifies the device within its page.
    String label; // Exposed as MediaDeviceInfo.label.
    VirtualAudioSampleFormat format { VirtualAudioSampleFormat::F32LittleEndian };
    uint32_t sampleRate { 48000 };
    uint32_t numberOfChannels { 2 };
    uint64_t channelMask { 0 };
    VirtualAudioLayout layout { VirtualAudioLayout::Interleaved };
    uint32_t framesPerBuffer { 0 };
    bool isDefault { false };

    friend bool operator==(const VirtualAudioDeviceConfiguration&, const VirtualAudioDeviceConfiguration&) = default;
};

// Receives the audio rendered on a virtual audio device. renderAudioData() is called from a
// media streaming thread, with the data formatted as described by the device configuration,
// and the timestamp of its first sample in nanoseconds of the device stream running time.
class VirtualAudioDeviceSink : public ThreadSafeRefCounted<VirtualAudioDeviceSink> {
public:
    virtual ~VirtualAudioDeviceSink() = default;
    virtual void renderAudioData(std::span<const uint8_t>, uint64_t timestamp) = 0;
};

} // namespace WebCore
