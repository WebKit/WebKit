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

#include "FloatRect.h"
#include <optional>
#include <wtf/Forward.h>

namespace WebCore {

struct NaturalDimensions;

// A Media Fragments space segment, the value of xywh=[pixel:|percent:]x,y,w,h.
// https://www.w3.org/TR/media-frags/#naming-space
struct SpatialMediaFragment {
    enum class Unit : bool { Pixel, Percent };

    static std::optional<SpatialMediaFragment> parse(StringView);

    // The viewBox this fragment selects in a document with these natural dimensions, if any.
    std::optional<FloatRect> viewBox(const NaturalDimensions&) const;

    Unit unit { Unit::Pixel };
    FloatRect rect;
};

// The viewBox selected by an SVG fragment identifier made of media fragment segments, if any.
// https://w3c.github.io/svgwg/svg2-draft/linking.html#SVGFragmentIdentifiersDefinitions
std::optional<FloatRect> viewBoxForSpatialMediaFragment(StringView fragmentIdentifier, const NaturalDimensions&);

} // namespace WebCore
