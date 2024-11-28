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

#include "Color.h"
#include "StyleBuilderState.h"
#include "StyleColor.h"
#include <wtf/Forward.h>
#include <wtf/OptionSet.h>

namespace WebCore {

enum class CSSUnresolvedLightDarkAppearance : bool;

class WEBCORE_EXPORT CSSUnresolvedColorResolutionDelegate {
public:
    virtual ~CSSUnresolvedColorResolutionDelegate();

    // Colors to use that usually get resolved dynamically using Document & RenderStyle.
    virtual Color currentColor() const { return { }; }              // For CSSValueCurrentcolor
    virtual Color internalDocumentTextColor() const { return { }; } // For CSSValueInternalDocumentTextColor
    virtual Color webkitLink() const { return { }; }                // For CSSValueWebkitLink [Style::ForVisitedLink::No]
    virtual Color webkitLinkVisited() const { return { }; }         // For CSSValueWebkitLink [Style::ForVisitedLink::Yes]
    virtual Color webkitActiveLink() const { return { }; }          // For CSSValueWebkitActivelink
    virtual Color webkitFocusRingColor() const { return { }; }      // For CSSValueWebkitFocusRingColor
};

struct CSSUnresolvedColorResolutionState {
    // Delegate for lazily computing color values.
    CSSUnresolvedColorResolutionDelegate* delegate = nullptr;

    // Level of nesting inside other colors the resolution currently is.
    unsigned nestingLevel = 0;

    // Conversion data needed to evaluate `calc()` expressions with relative length units.
    // If unset, colors that require conversion data will return the invalid Color.
    std::optional<CSSToLengthConversionData> conversionData = std::nullopt;

    // Whether links should be resolved to the visited style.
    Style::ForVisitedLink forVisitedLink = Style::ForVisitedLink::No;

    // Options to pass when resolving any other keyword with StyleColor::colorFromKeyword()
    OptionSet<StyleColorOptions> keywordOptions = { };

    // Appearance used to select from a light-dark() color function.
    // If unset, light-dark() colors will return the invalid Color.
    std::optional<CSSUnresolvedLightDarkAppearance> appearance = std::nullopt;

    // Colors are resolved:
    //   1. Checking if the color is set below, and if it is, returning it.
    //   2. If a delegate has been set, calling the associated delegate function,
    //      storing the result below, and returning that color.
    //   3. Returning the invalid `Color` value.
    mutable std::optional<Color> resolvedCurrentColor = std::nullopt;
    mutable std::optional<Color> resolvedInternalDocumentTextColor = std::nullopt;
    mutable std::optional<Color> resolvedWebkitLink = std::nullopt;
    mutable std::optional<Color> resolvedWebkitLinkVisited = std::nullopt;
    mutable std::optional<Color> resolvedWebkitActiveLink = std::nullopt;
    mutable std::optional<Color> resolvedWebkitFocusRingColor = std::nullopt;

    Color currentColor() const
    {
        return resolveColor(resolvedCurrentColor, &CSSUnresolvedColorResolutionDelegate::currentColor);
    }

    Color internalDocumentTextColor() const
    {
        return resolveColor(resolvedInternalDocumentTextColor, &CSSUnresolvedColorResolutionDelegate::internalDocumentTextColor);
    }

    Color webkitLink() const
    {
        return resolveColor(resolvedWebkitLink, &CSSUnresolvedColorResolutionDelegate::webkitLink);
    }

    Color webkitLinkVisited() const
    {
        return resolveColor(resolvedWebkitLinkVisited, &CSSUnresolvedColorResolutionDelegate::webkitLinkVisited);
    }

    Color webkitActiveLink() const
    {
        return resolveColor(resolvedWebkitActiveLink, &CSSUnresolvedColorResolutionDelegate::webkitActiveLink);
    }

    Color webkitFocusRingColor() const
    {
        return resolveColor(resolvedWebkitFocusRingColor, &CSSUnresolvedColorResolutionDelegate::webkitFocusRingColor);
    }

private:
    Color resolveColor(std::optional<Color>& existing, Color (CSSUnresolvedColorResolutionDelegate::*resolver)() const) const
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
struct CSSUnresolvedColorResolutionNester {
    CSSUnresolvedColorResolutionNester(CSSUnresolvedColorResolutionState& state)
        : state { state }
    {
        state.nestingLevel++;
    }

    ~CSSUnresolvedColorResolutionNester()
    {
        state.nestingLevel--;
    }

    CSSUnresolvedColorResolutionState& state;
};

} // namespace WebCore
