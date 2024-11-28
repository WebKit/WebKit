/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "CSSColorDescriptors.h"
#include "CSSUnresolvedAbsoluteColor.h"
#include "CSSUnresolvedAbsoluteResolvedColor.h"
#include "CSSUnresolvedColorHex.h"
#include "CSSUnresolvedColorKeyword.h"
#include "CSSUnresolvedColorLayers.h"
#include "CSSUnresolvedColorMix.h"
#include "CSSUnresolvedContrastColor.h"
#include "CSSUnresolvedLightDark.h"
#include "CSSUnresolvedRelativeColor.h"
#include <variant>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

struct CSSUnresolvedColorResolutionState;
struct CSSUnresolvedStyleColorResolutionState;

class CSSUnresolvedColor {
    WTF_MAKE_TZONE_ALLOCATED(CSSUnresolvedColor);
public:
    template<typename T> explicit CSSUnresolvedColor(T&& value)
        : m_value { std::forward<T>(value) }
    {
    }

    CSSUnresolvedColor(CSSUnresolvedColor&&) = default;
    CSSUnresolvedColor& operator=(CSSUnresolvedColor&&) = default;
    ~CSSUnresolvedColor();

    bool containsCurrentColor() const;
    bool containsColorSchemeDependentColor() const;

    void serializationForCSS(StringBuilder&) const;
    String serializationForCSS() const;

    bool equals(const CSSUnresolvedColor&) const;

    StyleColor createStyleColor(CSSUnresolvedStyleColorResolutionState&) const;
    Color createColor(CSSUnresolvedColorResolutionState&) const;

    std::optional<CSSUnresolvedAbsoluteResolvedColor> absolute() const;
    std::optional<CSSUnresolvedColorKeyword> keyword() const;
    std::optional<CSSUnresolvedColorHex> hex() const;

private:
    std::variant<
        CSSUnresolvedAbsoluteResolvedColor,
        CSSUnresolvedColorKeyword,
        CSSUnresolvedColorHex,
        CSSUnresolvedColorLayers,
        CSSUnresolvedColorMix,
        CSSUnresolvedContrastColor,
        CSSUnresolvedLightDark,
        CSSUnresolvedAbsoluteColor<RGBFunctionLegacy<CSS::Number<>>>,
        CSSUnresolvedAbsoluteColor<RGBFunctionLegacy<CSS::Percentage<>>>,
        CSSUnresolvedAbsoluteColor<RGBFunctionModernAbsolute>,
        CSSUnresolvedAbsoluteColor<HSLFunctionLegacy>,
        CSSUnresolvedAbsoluteColor<HSLFunctionModern>,
        CSSUnresolvedAbsoluteColor<HWBFunction>,
        CSSUnresolvedAbsoluteColor<LabFunction>,
        CSSUnresolvedAbsoluteColor<LCHFunction>,
        CSSUnresolvedAbsoluteColor<OKLabFunction>,
        CSSUnresolvedAbsoluteColor<OKLCHFunction>,
        CSSUnresolvedAbsoluteColor<ColorRGBFunction<ExtendedA98RGB<float>>>,
        CSSUnresolvedAbsoluteColor<ColorRGBFunction<ExtendedDisplayP3<float>>>,
        CSSUnresolvedAbsoluteColor<ColorRGBFunction<ExtendedProPhotoRGB<float>>>,
        CSSUnresolvedAbsoluteColor<ColorRGBFunction<ExtendedRec2020<float>>>,
        CSSUnresolvedAbsoluteColor<ColorRGBFunction<ExtendedSRGBA<float>>>,
        CSSUnresolvedAbsoluteColor<ColorRGBFunction<ExtendedLinearSRGBA<float>>>,
        CSSUnresolvedAbsoluteColor<ColorXYZFunction<XYZA<float, WhitePoint::D50>>>,
        CSSUnresolvedAbsoluteColor<ColorXYZFunction<XYZA<float, WhitePoint::D65>>>,
        CSSUnresolvedRelativeColor<RGBFunctionModernRelative>,
        CSSUnresolvedRelativeColor<HSLFunctionModern>,
        CSSUnresolvedRelativeColor<HWBFunction>,
        CSSUnresolvedRelativeColor<LabFunction>,
        CSSUnresolvedRelativeColor<LCHFunction>,
        CSSUnresolvedRelativeColor<OKLabFunction>,
        CSSUnresolvedRelativeColor<OKLCHFunction>,
        CSSUnresolvedRelativeColor<ColorRGBFunction<ExtendedA98RGB<float>>>,
        CSSUnresolvedRelativeColor<ColorRGBFunction<ExtendedDisplayP3<float>>>,
        CSSUnresolvedRelativeColor<ColorRGBFunction<ExtendedProPhotoRGB<float>>>,
        CSSUnresolvedRelativeColor<ColorRGBFunction<ExtendedRec2020<float>>>,
        CSSUnresolvedRelativeColor<ColorRGBFunction<ExtendedSRGBA<float>>>,
        CSSUnresolvedRelativeColor<ColorRGBFunction<ExtendedLinearSRGBA<float>>>,
        CSSUnresolvedRelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D50>>>,
        CSSUnresolvedRelativeColor<ColorXYZFunction<XYZA<float, WhitePoint::D65>>>
    > m_value;
};

void serializationForCSS(StringBuilder&, const CSSUnresolvedColor&);
void serializationForCSS(StringBuilder&, const UniqueRef<CSSUnresolvedColor>&);
String serializationForCSS(const CSSUnresolvedColor&);
String serializationForCSS(const UniqueRef<CSSUnresolvedColor>&);

bool operator==(const UniqueRef<CSSUnresolvedColor>&, const UniqueRef<CSSUnresolvedColor>&);

} // namespace WebCore
