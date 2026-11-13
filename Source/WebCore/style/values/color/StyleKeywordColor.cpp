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

#include "config.h"
#include "StyleKeywordColor.h"

#include "CSSKeywordColor.h"
#include "Color.h"
#include "ColorSerialization.h"
#include "Document.h"
#include "PlatformRenderTheme.h"
#include "RenderTheme.h"
#include "StyleAccentColor.h"
#include "StyleBuilderState.h"
#include "StyleColor.h"
#include "StyleColorResolutionState.h"
#include "StyleComputedStyle.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleContrastColor.h"
#include "StyleCurrentColor.h"
#include "StyleResolvedColors.h"

namespace WebCore {
namespace Style {

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(KeywordColor);

static Color resolveAccentColorKeyword(const ColorResolutionState& state)
{
    if (state.propertyToResolve == CSSPropertyID::AccentColor && state.parentStyle) {
        // accent-color: AccentColor should resolve to the _computed_ value of the parent's accent-color.
#if ENABLE(AX_CUSTOM_COLOR_MODE)
        auto computedStyleColor = state.parentStyle->accentColorResolvingCurrentColor();
        auto renderingColor = state.parentStyle->accentColorResolvingCurrentColorApplyingColorFilter();
        if (computedStyleColor != renderingColor)
            return KeywordColor { WTF::move(computedStyleColor), WTF::move(renderingColor) };
        return ResolvedColor { WTF::move(renderingColor) };
#else
        return ResolvedColor { state.parentStyle->accentColorResolvingCurrentColorApplyingColorFilter() };
#endif
    }

    return state.style->accentColor().colorOrDefaultColor();
}

Color toStyleColor(const CSS::KeywordColor& unresolved, ColorResolutionState& state)
{
    switch (unresolved.valueID) {
    case CSSValueID::InternalDocumentTextColor:
        return { state.document->textColor() };
    case CSSValueID::Currentcolor:
        return { CurrentColor() };
    case CSSValueID::Accentcolor:
        return resolveAccentColorKeyword(state);
    case CSSValueID::Accentcolortext:
        return { ContrastColor { resolveAccentColorKeyword(state) } };
    default:
        break;
    }

    auto resolveColor = [&](StyleColorOptionsPurpose purpose) {
        switch (unresolved.valueID) {
        case CSSValueID::WebkitLink:
            return state.forVisitedLink == ForVisitedLink::Yes ? state.document->visitedLinkColor(state.style, purpose) : state.document->linkColor(state.style, purpose);
        case CSSValueID::WebkitActivelink:
            return state.document->activeLinkColor(state.style, purpose);
        case CSSValueID::WebkitFocusRingColor:
            return RenderTheme::singleton().focusRingColor(state.document->styleColorOptions(state.style.ptr(), purpose));
        default:
            return CSS::colorFromKeyword(unresolved.valueID, state.document->styleColorOptions(state.style.ptr(), purpose));
        }
    };

#if ENABLE(AX_CUSTOM_COLOR_MODE)
    if (state.document->isAXCustomColorModeActive()) {
        auto computedStyleColor = resolveColor(StyleColorOptionsPurpose::ComputedStyle);
        auto renderColor = resolveColor(StyleColorOptionsPurpose::Rendering);

        if (computedStyleColor != renderColor)
            return KeywordColor { WTF::move(computedStyleColor), WTF::move(renderColor) };
        return ResolvedColor { WTF::move(computedStyleColor) };
    }
#endif

    return ResolvedColor { resolveColor(StyleColorOptionsPurpose::Rendering) };
}

WebCore::Color resolveColor(const KeywordColor& color, const ResolvedColors& context)
{
    switch (context.styleColorOptionsPurpose()) {
    case StyleColorOptionsPurpose::ComputedStyle:
        return color.computedStyleColor;

    case StyleColorOptionsPurpose::Rendering:
        return color.renderColor;
    }

    ASSERT_NOT_REACHED();
    return color.computedStyleColor;
}

void serializationForCSSTokenization(StringBuilder& builder, const CSS::SerializationContext&, const KeywordColor& keywordColor)
{
    builder.append(WebCore::serializationForCSS(keywordColor.computedStyleColor));
}

WTF::String serializationForCSSTokenization(const CSS::SerializationContext&, const KeywordColor& keywordColor)
{
    return WebCore::serializationForCSS(keywordColor.computedStyleColor);
}

// MARK: - TextStream

WTF::TextStream& operator<<(WTF::TextStream& ts, const KeywordColor& keywordColor)
{
    return ts << "keywordColor(style color: "_s << keywordColor.computedStyleColor.debugDescription() << ", render color: " << keywordColor.renderColor.debugDescription() << ')';
}

} // namespace Style
} // namespace WebCore
