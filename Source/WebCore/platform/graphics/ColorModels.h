/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include <array>
#include <limits>

namespace WebCore {

template<typename> struct AlphaTraits;
template<typename> struct ColorComponentRange;
template<typename> struct ExtendedRGBModel;
template<typename> struct HSLModel;
template<typename> struct HWBModel;
template<typename> struct LCHModel;
template<typename> struct LabModel;
template<typename> struct RGBModel;
template<typename> struct XYZModel;


template<> struct AlphaTraits<float> {
    static constexpr float transparent = 0.0f;
    static constexpr float opaque = 1.0f;
};

template<> struct AlphaTraits<uint8_t> {
    static constexpr uint8_t transparent = 0;
    static constexpr uint8_t opaque = 255;
};

template<typename T> struct ColorComponentRange {
    T min;
    T max;
};

template<> struct ExtendedRGBModel<float> {
    static constexpr std::array<ColorComponentRange<float>, 3> ranges { {
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() }
    } };
    static constexpr bool isInvertible = false;
};

template<> struct HSLModel<float> {
    static constexpr std::array<ColorComponentRange<float>, 3> ranges { {
        { 0, 360 },
        { 0, 100 },
        { 0, 100 }
    } };
    static constexpr bool isInvertible = false;
};

template<> struct HWBModel<float> {
    static constexpr std::array<ColorComponentRange<float>, 3> ranges { {
        { 0, 360 },
        { 0, 100 },
        { 0, 100 }
    } };
    static constexpr bool isInvertible = false;
};

template<> struct LabModel<float> {
    static constexpr std::array<ColorComponentRange<float>, 3> ranges { {
        { 0, std::numeric_limits<float>::infinity() },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() }
    } };
    static constexpr bool isInvertible = false;
};

template<> struct LCHModel<float> {
    static constexpr std::array<ColorComponentRange<float>, 3> ranges { {
        { 0, std::numeric_limits<float>::infinity() },
        { 0, std::numeric_limits<float>::infinity() },
        { 0, 360 }
    } };
    static constexpr bool isInvertible = false;
};

template<> struct RGBModel<float> {
    static constexpr std::array<ColorComponentRange<float>, 3> ranges { {
        { 0, 1 },
        { 0, 1 },
        { 0, 1 }
    } };
    static constexpr bool isInvertible = true;
};

template<> struct RGBModel<uint8_t> {
    static constexpr std::array<ColorComponentRange<uint8_t>, 3> ranges { {
        { 0, 255 },
        { 0, 255 },
        { 0, 255 }
    } };
    static constexpr bool isInvertible = true;
};

template<> struct XYZModel<float> {
    static constexpr std::array<ColorComponentRange<float>, 3> ranges { {
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() }
    } };
    static constexpr bool isInvertible = false;
};

}
