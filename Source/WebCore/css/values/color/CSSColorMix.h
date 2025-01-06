/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSColor.h"
#include "CSSPrimitiveNumericTypes.h"
#include "ColorInterpolationMethod.h"
#include "StylePrimitiveNumericTypes.h"
#include <wtf/Forward.h>

namespace WebCore {

class Color;
class CSSToLengthConversionData;

namespace CSS {

struct PlatformColorResolutionState;

struct ColorMix {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    struct Component {
        using Percentage = CSS::Percentage<Range{0, 100}>;

        Color color;
        std::optional<Percentage> percentage;

        bool operator==(const Component&) const = default;
    };

    ColorInterpolationMethod colorInterpolationMethod;
    Component mixComponents1;
    Component mixComponents2;

    bool operator==(const ColorMix&) const = default;
};

WebCore::Color createColor(const ColorMix&, PlatformColorResolutionState&);
bool containsCurrentColor(const ColorMix&);
bool containsColorSchemeDependentColor(const ColorMix&);

template<> struct Serialize<ColorMix> { void operator()(StringBuilder&, const ColorMix&); };
template<> struct ComputedStyleDependenciesCollector<ColorMix> { void operator()(ComputedStyleDependencies&, const ColorMix&); };
template<> struct CSSValueChildrenVisitor<ColorMix> { IterationStatus operator()(const Function<IterationStatus(CSSValue&)>&, const ColorMix&); };

} // namespace CSS
} // namespace WebCore
