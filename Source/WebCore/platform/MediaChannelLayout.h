/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <bit>
#include <cstdint>
#include <optional>

namespace WebCore {

// https://webaudio.github.io/web-audio-api/#ChannelOrdering
// The order is the one of the WAVE channel mask.
enum class WebAudioChannel : uint8_t {
    FrontLeft,
    FrontRight,
    FrontCenter,
    LowFrequency,
    BackLeft,
    BackRight,
    FrontLeftOfCenter,
    FrontRightOfCenter,
    BackCenter,
    SideLeft,
    SideRight,
    TopCenter,
    TopFrontLeft,
    TopFrontCenter,
    TopFrontRight,
    TopBackLeft,
    TopBackCenter,
    TopBackRight,
};

static constexpr unsigned numberOfWebAudioChannels = static_cast<unsigned>(WebAudioChannel::TopBackRight) + 1;

constexpr uint32_t webAudioChannelBit(WebAudioChannel channel)
{
    auto index = static_cast<uint8_t>(channel);
    return index < numberOfWebAudioChannels ? 1U << index : 0;
}

enum class MediaChannelLayoutKind : uint8_t {
    // Each channel has a speaker position, given by the mask.
    Positional,
    // The channels have no speaker position.
    Discrete,
    // Ambisonic components in ACN order with SN3D normalization. Channels after the components are
    // non-diegetic: a stereo pair, given by the mask, played as is rather than as part of the sound field.
    Ambisonic,
};

// A channel layout is always interpreted with the number of channels it applies to, as given by
// AudioInfo::channels(); isValid() checks that the two are consistent.
struct MediaChannelLayout {
    using Kind = MediaChannelLayoutKind;

    Kind kind { Kind::Discrete };
    // With Kind::Positional, bit n is set when a channel is at WebAudioChannel n, and the channels are in
    // ascending bit order, as in https://webaudio.github.io/web-audio-api/#ChannelOrdering.
    // With Kind::Ambisonic, 0 or nonDiegeticStereoMask. Otherwise 0.
    uint32_t mask { 0 };

    static constexpr uint32_t nonDiegeticStereoMask = webAudioChannelBit(WebAudioChannel::FrontLeft) | webAudioChannelBit(WebAudioChannel::FrontRight);

    // The ambisonic order, when the components of the layout fill an order, from 0, of the full sphere.
    std::optional<uint32_t> ambisonicOrder(uint32_t numberOfChannels) const
    {
        if (kind != Kind::Ambisonic || (mask && mask != nonDiegeticStereoMask))
            return { };
        uint32_t nonDiegeticChannels = std::popcount(mask);
        if (numberOfChannels <= nonDiegeticChannels)
            return { };
        uint64_t components = numberOfChannels - nonDiegeticChannels;
        uint64_t order = 0;
        while ((order + 1) * (order + 1) < components)
            ++order;
        if ((order + 1) * (order + 1) != components)
            return { };
        return static_cast<uint32_t>(order);
    }

    bool isValid(uint32_t numberOfChannels) const
    {
        if (!numberOfChannels)
            return false;
        switch (kind) {
        case Kind::Positional:
            return mask < (1U << numberOfWebAudioChannels) && static_cast<uint32_t>(std::popcount(mask)) == numberOfChannels;
        case Kind::Discrete:
            // The channels have no position; the mask is unused and must be 0 so that equal layouts compare equal.
            return !mask;
        case Kind::Ambisonic:
            return !!ambisonicOrder(numberOfChannels);
        }
        return false;
    }

    bool operator==(const MediaChannelLayout&) const = default;
};

} // namespace WebCore
