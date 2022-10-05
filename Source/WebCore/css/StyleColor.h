/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2016, 2022 Apple Inc. All rights reserved.
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

#include "CSSPrimitiveValue.h"
#include "CSSValueKeywords.h"
#include "Color.h"
#include "ColorInterpolationMethod.h"
#include <wtf/OptionSet.h>

namespace WebCore {

class CSSPrimitiveValue;
class StyleColor;

enum class StyleColorOptions : uint8_t {
    ForVisitedLink = 1 << 0,
    UseSystemAppearance = 1 << 1,
    UseDarkAppearance = 1 << 2,
    UseElevatedUserInterfaceLevel = 1 << 3
};

struct CurrentColor {
    bool operator==(const CurrentColor&) const = default;
};

class StyleColor {
public:
    // The default constructor initializes to currentcolor to preserve old behavior,
    // we might want to change it to invalid color at some point.
    StyleColor()
        : m_color { CurrentColor { } }
    {
    }

    StyleColor(const Color& color)
        : m_color { Color { color } }
    {
    }

    StyleColor(const SRGBA<uint8_t>& color)
        : m_color { Color { color } }
    {
    }

    StyleColor(const StyleColor&) = default;
    StyleColor(StyleColor&&) = default;
    StyleColor& operator=(const StyleColor&) = default;
    bool operator==(const StyleColor&) const = default;

    static StyleColor currentColor() { return StyleColor { CurrentColor { } }; }

    static Color colorFromKeyword(CSSValueID, OptionSet<StyleColorOptions>);
    static Color colorFromAbsoluteKeyword(CSSValueID);

    static bool isAbsoluteColorKeyword(CSSValueID);
    static bool isCurrentColorKeyword(CSSValueID id) { return id == CSSValueCurrentcolor; }
    static bool isCurrentColor(const CSSPrimitiveValue& value) { return isCurrentColorKeyword(value.valueID()); }

    WEBCORE_EXPORT static bool isSystemColorKeyword(CSSValueID);
    static bool isDeprecatedSystemColorKeyword(CSSValueID);

    enum class CSSColorType : uint8_t {
        Absolute = 1 << 0,
        Current = 1 << 1,
        System = 1 << 2,
    };

    // https://drafts.csswg.org/css-color-4/#typedef-color
    static bool isColorKeyword(CSSValueID, OptionSet<CSSColorType> = { CSSColorType::Absolute, CSSColorType::Current, CSSColorType::System });

    bool isCurrentColor() const;
    bool isAbsoluteColor() const;
    const Color& absoluteColor() const;

    WEBCORE_EXPORT Color resolveColor(const Color& colorPropertyValue) const;
    WEBCORE_EXPORT Color resolveColorWithoutCurrentColor() const;

    friend WTF::TextStream& operator<<(WTF::TextStream&, const StyleColor&);
    String debugDescription() const;

private:
    using ColorKind = std::variant<Color, CurrentColor>;

    StyleColor(const ColorKind& color)
        : m_color { color }
    {
    }

    ColorKind m_color;
};

WEBCORE_EXPORT String serializationForRenderTreeAsText(const StyleColor&);
WEBCORE_EXPORT String serializationForCSS(const StyleColor&);

} // namespace WebCore
