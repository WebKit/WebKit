/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "CSSColorDescriptors.h"

namespace WebCore {

class Color;

namespace CSS {

// https://drafts.csswg.org/css-color-5/#relative-alpha

struct RelativeAlphaColor {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(RelativeAlphaColor);

    using Descriptor = AlphaDescriptor;
    using Alpha = GetCSSColorParseTypeWithCalcAndSymbolsComponentResult<Descriptor, 0>;

    Color origin;
    std::optional<Alpha> alpha;

    bool operator==(const RelativeAlphaColor&) const;
};

WebCore::Color createColor(const RelativeAlphaColor&, PlatformColorResolutionState&);

bool containsCurrentColor(const RelativeAlphaColor&);
bool containsColorSchemeDependentColor(const RelativeAlphaColor&);

template<> struct Serialize<RelativeAlphaColor> { void operator()(StringBuilder&, const SerializationContext&, const RelativeAlphaColor&); };
template<> struct ComputedStyleDependenciesCollector<RelativeAlphaColor> { void operator()(ComputedStyleDependencies&, const RelativeAlphaColor&); };
template<> struct CSSValueChildrenVisitor<RelativeAlphaColor> { IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const RelativeAlphaColor&); };

} // namespace CSS
} // namespace WebCore
