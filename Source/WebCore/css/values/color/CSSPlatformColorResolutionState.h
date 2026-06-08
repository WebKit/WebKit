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

#include "CSSToLengthConversionData.h"
#include "Color.h"
#include "StyleColor.h"
#include "StyleForVisitedLink.h"
#include <wtf/Forward.h>
#include <wtf/OptionSet.h>

namespace WebCore {
namespace CSS {

enum class LightDarkColorAppearance : bool;

// Used to resolves a CSS::Color to a WebCore::Color (platform color) without going through Style::Color / style building.

class WEBCORE_EXPORT PlatformColorResolutionDelegate {
public:
    virtual ~PlatformColorResolutionDelegate();

    // Colors to use that usually get resolved dynamically using Document & RenderStyle.
    virtual WebCore::Color currentColor() const;              // For CSSValueCurrentcolor
    virtual WebCore::Color internalDocumentTextColor() const; // For CSSValueInternalDocumentTextColor
    virtual WebCore::Color webkitLink() const;                // For CSSValueWebkitLink [Style::ForVisitedLink::No]
    virtual WebCore::Color webkitLinkVisited() const;         // For CSSValueWebkitLink [Style::ForVisitedLink::Yes]
    virtual WebCore::Color webkitActiveLink() const;          // For CSSValueWebkitActivelink
    virtual WebCore::Color webkitFocusRingColor() const;      // For CSSValueWebkitFocusRingColor
};

struct PlatformColorResolutionState {
    // Delegate for lazily computing color values.
    PlatformColorResolutionDelegate* delegate = nullptr;

    // Level of nesting inside other colors the resolution currently is.
    unsigned nestingLevel = 0;

    // Conversion data needed to evaluate `calc()` expressions with relative length units.
    // If unset, colors that require conversion data will return the invalid Color.
    std::optional<CSSToLengthConversionData> conversionData = std::nullopt;

    // Whether links should be resolved to the visited style.
    Style::ForVisitedLink forVisitedLink = Style::ForVisitedLink::No;

    // Options to pass when resolving any other keyword with CSS::colorFromKeyword()
    OptionSet<StyleColorOptions> keywordOptions = { };

    // Appearance used to select from a light-dark() color function.
    // If unset, light-dark() colors will return the invalid Color.
    std::optional<LightDarkColorAppearance> appearance = std::nullopt;

    // Colors are resolved:
    //   1. Checking if the color is set below, and if it is, returning it.
    //   2. If a delegate has been set, calling the associated delegate function,
    //      storing the result below, and returning that color.
    //   3. Returning the invalid `Color` value.
    mutable std::optional<WebCore::Color> resolvedCurrentColor = std::nullopt;
    mutable std::optional<WebCore::Color> resolvedInternalDocumentTextColor = std::nullopt;
    mutable std::optional<WebCore::Color> resolvedWebkitLink = std::nullopt;
    mutable std::optional<WebCore::Color> resolvedWebkitLinkVisited = std::nullopt;
    mutable std::optional<WebCore::Color> resolvedWebkitActiveLink = std::nullopt;
    mutable std::optional<WebCore::Color> resolvedWebkitFocusRingColor = std::nullopt;

    WebCore::Color currentColor() const
    {
        return resolveColor(resolvedCurrentColor, &PlatformColorResolutionDelegate::currentColor);
    }

    WebCore::Color internalDocumentTextColor() const
    {
        return resolveColor(resolvedInternalDocumentTextColor, &PlatformColorResolutionDelegate::internalDocumentTextColor);
    }

    WebCore::Color webkitLink() const
    {
        return resolveColor(resolvedWebkitLink, &PlatformColorResolutionDelegate::webkitLink);
    }

    WebCore::Color webkitLinkVisited() const
    {
        return resolveColor(resolvedWebkitLinkVisited, &PlatformColorResolutionDelegate::webkitLinkVisited);
    }

    WebCore::Color webkitActiveLink() const
    {
        return resolveColor(resolvedWebkitActiveLink, &PlatformColorResolutionDelegate::webkitActiveLink);
    }

    WebCore::Color webkitFocusRingColor() const
    {
        return resolveColor(resolvedWebkitFocusRingColor, &PlatformColorResolutionDelegate::webkitFocusRingColor);
    }

private:
    WebCore::Color resolveColor(std::optional<WebCore::Color>& existing, WebCore::Color (PlatformColorResolutionDelegate::*resolver)() const) const
    {
        if (existing)
            return *existing;

        if (delegate) {
            auto resolved = ((*delegate).*resolver)();
            existing = resolved;
            return resolved;
        }

        return { };
    }
};

// RAII helper to increment/decrement nesting level.
struct PlatformColorResolutionStateNester {
    PlatformColorResolutionStateNester(PlatformColorResolutionState& state)
        : state { state }
    {
        state.nestingLevel++;
    }

    ~PlatformColorResolutionStateNester()
    {
        state.nestingLevel--;
    }

    PlatformColorResolutionState& state;
};

} // namespace CSS
} // namespace WebCore
