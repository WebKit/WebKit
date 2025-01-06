/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include <wtf/Vector.h>

namespace WebCore {

class Color;
enum class BlendMode : uint8_t;

namespace CSS {

struct PlatformColorResolutionState;

struct ColorLayers {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    BlendMode blendMode;
    CommaSeparatedVector<Color> colors;

    bool operator==(const ColorLayers&) const = default;
};


WebCore::Color createColor(const ColorLayers&, PlatformColorResolutionState&);
bool containsCurrentColor(const ColorLayers&);
bool containsColorSchemeDependentColor(const ColorLayers&);

template<> struct Serialize<ColorLayers> { void operator()(StringBuilder&, const ColorLayers&); };
template<> struct ComputedStyleDependenciesCollector<ColorLayers> { void operator()(ComputedStyleDependencies&, const ColorLayers&); };
template<> struct CSSValueChildrenVisitor<ColorLayers> { IterationStatus operator()(const Function<IterationStatus(CSSValue&)>&, const ColorLayers&); };

} // namespace CSS
} // namespace WebCore
