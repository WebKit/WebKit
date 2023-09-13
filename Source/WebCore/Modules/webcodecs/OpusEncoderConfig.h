/*
 * Copyright (C) 2023 Igalia S.L
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

#pragma once

#if ENABLE(WEB_CODECS)

#include <optional>
#include <wtf/Algorithms.h>
#include <wtf/Vector.h>

namespace WebCore {

enum class OpusBitstreamFormat : uint8_t {
    Opus,
    Ogg
};

// https://www.w3.org/TR/webcodecs-opus-codec-registration/#opus-encoder-config
struct OpusEncoderConfig {
    bool isValid()
    {
        float frameDurationMs = frameDuration / 1000.0;
        if (!WTF::anyOf(Vector<float> { 2.5, 5, 10, 20, 40, 60, 120 }, [frameDurationMs](auto value) -> bool {
            return WTF::areEssentiallyEqual(value, frameDurationMs);
        }))
            return false;

        if (complexity && *complexity > 10)
            return false;

        if (packetlossperc > 100)
            return false;

        return true;
    }

    using BitstreamFormat = OpusBitstreamFormat;
    OpusBitstreamFormat format { OpusBitstreamFormat::Opus };
    uint64_t frameDuration { 20000 };
    std::optional<size_t> complexity;
    size_t packetlossperc { 0 };
    bool useinbandfec { false };
    bool usedtx { false };
};

}

#endif // ENABLE(WEB_CODECS)
