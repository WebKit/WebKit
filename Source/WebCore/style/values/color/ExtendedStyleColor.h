/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSColorDescriptors.h"
#include "StyleColorLayers.h"
#include "StyleColorMix.h"
#include "StyleContrastColor.h"
#include "StyleRelativeColor.h"

namespace WebCore {

namespace Style {

using ExtendedColorKind = std::variant<
    ColorMix,
    ColorLayers,
    ContrastColor,
    RelativeColor<RGBFunctionModernRelative>,
    RelativeColor<HSLFunctionModern>,
    RelativeColor<HWBFunction>,
    RelativeColor<LabFunction>,
    RelativeColor<LCHFunction>,
    RelativeColor<OKLabFunction>,
    RelativeColor<OKLCHFunction>,
    RelativeColor<ColorRGBFunction<ExtendedA98RGB<float>>>,
    RelativeColor<ColorRGBFunction<ExtendedDisplayP3<float>>>,
    RelativeColor<ColorRGBFunction<ExtendedProPhotoRGB<float>>>,
    RelativeColor<ColorRGBFunction<ExtendedRec2020<float>>>,
    RelativeColor<ColorRGBFunction<ExtendedSRGBA<float>>>,
    RelativeColor<ColorRGBFunction<ExtendedLinearSRGBA<float>>>,
    RelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D50>>>,
    RelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D65>>>
    >;

class ExtendedStyleColor : public RefCounted<ExtendedStyleColor> {
public:

    template <typename T>
    static Ref<ExtendedStyleColor> create(T&& t) { return adoptRef(*new ExtendedStyleColor(WTFMove(t))); }

    bool containsCurrentColor() const;
    WEBCORE_EXPORT WebCore::Color resolveColor(const WebCore::Color& currentColor) const;

    bool operator==(const ExtendedStyleColor& other) const { return m_color == other.m_color; }

    friend WEBCORE_EXPORT String serializationForCSS(const ExtendedStyleColor&);
    friend void serializationForCSS(StringBuilder&, const ExtendedStyleColor&);
    friend WTF::TextStream& operator<<(WTF::TextStream&, const ExtendedStyleColor&);

private:
    ExtendedStyleColor(ExtendedColorKind&&);
    ExtendedColorKind m_color;
};

bool containsCurrentColor(const ExtendedStyleColor&);
void serializationForCSS(StringBuilder&, const ExtendedStyleColor&);
WEBCORE_EXPORT String serializationForCSS(const ExtendedStyleColor&);
WTF::TextStream& operator<<(WTF::TextStream&, const ExtendedStyleColor&);

} // namespace Style

} // namespace WebCore
