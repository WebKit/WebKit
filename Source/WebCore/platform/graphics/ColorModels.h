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
template<typename> struct ColorComponentInfo;
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

enum class ColorComponentType {
    Angle,
    Number,
    Percentage
};

template<typename T> struct ColorComponentInfo {
    T min;
    T max;
    ColorComponentType type;
};

template<> struct ExtendedRGBModel<float> {
    static constexpr std::array<ColorComponentInfo<float>, 3> componentInfo { {
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), ColorComponentType::Number },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), ColorComponentType::Number },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), ColorComponentType::Number }
    } };
    static constexpr bool isInvertible = false;
};

template<> struct HSLModel<float> {
    static constexpr std::array<ColorComponentInfo<float>, 3> componentInfo { {
        { 0, 360, ColorComponentType::Angle },
        { 0, 100, ColorComponentType::Percentage },
        { 0, 100, ColorComponentType::Percentage }
    } };
    static constexpr bool isInvertible = false;
};

template<> struct HWBModel<float> {
    static constexpr std::array<ColorComponentInfo<float>, 3> componentInfo { {
        { 0, 360, ColorComponentType::Angle },
        { 0, 100, ColorComponentType::Percentage },
        { 0, 100, ColorComponentType::Percentage }
    } };
    static constexpr bool isInvertible = false;
};

template<> struct LabModel<float> {
    static constexpr std::array<ColorComponentInfo<float>, 3> componentInfo { {
        { 0, std::numeric_limits<float>::infinity(), ColorComponentType::Number },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), ColorComponentType::Number },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), ColorComponentType::Number }
    } };
    static constexpr bool isInvertible = false;
};

template<> struct LCHModel<float> {
    static constexpr std::array<ColorComponentInfo<float>, 3> componentInfo { {
        { 0, std::numeric_limits<float>::infinity(), ColorComponentType::Number },
        { 0, std::numeric_limits<float>::infinity(), ColorComponentType::Number },
        { 0, 360, ColorComponentType::Angle }
    } };
    static constexpr bool isInvertible = false;
};

template<> struct RGBModel<float> {
    static constexpr std::array<ColorComponentInfo<float>, 3> componentInfo { {
        { 0, 1, ColorComponentType::Number },
        { 0, 1, ColorComponentType::Number },
        { 0, 1, ColorComponentType::Number }
    } };
    static constexpr bool isInvertible = true;
};

template<> struct RGBModel<uint8_t> {
    static constexpr std::array<ColorComponentInfo<uint8_t>, 3> componentInfo { {
        { 0, 255, ColorComponentType::Number },
        { 0, 255, ColorComponentType::Number },
        { 0, 255, ColorComponentType::Number }
    } };
    static constexpr bool isInvertible = true;
};

template<> struct XYZModel<float> {
    static constexpr std::array<ColorComponentInfo<float>, 3> componentInfo { {
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), ColorComponentType::Number },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), ColorComponentType::Number },
        { -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), ColorComponentType::Number }
    } };
    static constexpr bool isInvertible = false;
};

}
