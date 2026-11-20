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

} // namespace WebCore
