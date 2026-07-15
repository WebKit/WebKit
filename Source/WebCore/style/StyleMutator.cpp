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

#include "config.h"
#include "StyleMutator.h"
#include "StyleMutatorInlines.h"

#include "CSSFontSelector.h"
#include "DocumentInlines.h"
#include "DocumentView.h"
#include "ElementInlines.h"
#include "FontCache.h"
#include "FrameDestructionObserverInlines.h"
#include "LocalFrame.h"
#include "SVGElementTypeHelpers.h"
#include "SVGSVGElement.h"
#include "Settings.h"
#include "StyleFontSizeFunctions.h"

namespace WebCore {
namespace Style {

Mutator::Mutator(ComputedStyle& style, MutatorContext&& context)
    : m_style(style)
    , m_document(WTF::move(context.document))
    , m_element(WTF::move(context.element))
    , m_parentStyle(context.parentStyle)
    , m_rootElementStyle(context.rootElementStyle)
{
}

// SVG handles zooming in a different way compared to CSS. The whole document is scaled instead
// of each individual length value in the render style / tree. CSSPrimitiveValue::resolveAsLength*()
// multiplies each resolved length with the zoom multiplier - so for SVG we need to disable that.
// Though all CSS values that can be applied to outermost <svg> elements (width/height/border/padding...)
// need to respect the scaling. RenderBox (the parent class of LegacyRenderSVGRoot) grabs values like
// width/height/border/padding/... from the ComputedStyle -> for SVG these values would never scale,
// if we'd pass a 1.0 zoom factor everywhere. So we only pass a zoom factor of 1.0 for specific
// properties that are NOT allowed to scale within a zoomed SVG document (letter/word-spacing/font-size).
bool Mutator::useSVGZoomRules() const
{
    return is<SVGElement>(element());
}

bool Mutator::useSVGZoomRulesForLength() const
{
    return is<SVGElement>(element()) && !(is<SVGSVGElement>(*element()) && element()->parentNode());
}

void Mutator::updateFont()
{
    Ref fontSelector = const_cast<Document&>(document()).fontSelector();

    auto needsUpdate = m_fontDirty || !m_style.fontCascade().fonts();
    if (!needsUpdate)
        return;

#if ENABLE(TEXT_AUTOSIZING)
    updateFontForTextSizeAdjust();
#endif
    updateFontForGenericFamilyChange();
    updateFontForZoomChange();
    updateFontForOrientationChange();
    updateFontForSizeChange();

    m_style.fontCascade().update(fontSelector.ptr());

    m_fontDirty = false;
}

#if ENABLE(TEXT_AUTOSIZING)
void Mutator::updateFontForTextSizeAdjust()
{
    if (m_style.textSizeAdjust().isAuto()
        || !document().settings().textAutosizingEnabled()
        || (document().settings().textAutosizingUsesIdempotentMode()
            && !m_style.textSizeAdjust().isNone()
            && !document().settings().idempotentModeAutosizingOnlyHonorsPercentages()))
        return;

    auto newFontDescription = m_style.fontDescription();
    auto baseSize = newFontDescription.specifiedSize();
    if (!m_style.textSizeAdjust().isNone())
        baseSize *= m_style.textSizeAdjust().multiplier();

    float zoomFactor = m_style.usedZoom();
    if (auto* frame = document().frame(); frame && m_style.textZoom() != TextZoom::Reset)
        zoomFactor *= frame->textZoomFactor();
    newFontDescription.setComputedSize(baseSize * zoomFactor, zoomFactor);

    m_style.setFontDescriptionWithoutUpdate(WTF::move(newFontDescription));
}
#endif

void Mutator::updateFontForZoomChange()
{
    if (m_style.usedZoom() == parentStyle().usedZoom() && m_style.textZoom() == parentStyle().textZoom())
        return;

#if ENABLE(TEXT_AUTOSIZING)
    // When text-size-adjust has an active percentage, updateFontForTextSizeAdjust() has already
    // computed the correct size (incorporating both the multiplier and the current zoom factor).
    // Skip recalculation here to avoid overwriting that result, which would lose the
    // text-size-adjust multiplier.
    if (document().settings().textAutosizingEnabled()
        && !m_style.textSizeAdjust().isAuto()
        && !m_style.textSizeAdjust().isNone()
        && (!document().settings().textAutosizingUsesIdempotentMode()
            || document().settings().idempotentModeAutosizingOnlyHonorsPercentages()))
        return;
#endif

    setFontDescriptionFontSize(m_style.fontDescription().specifiedSize());
}

void Mutator::updateFontForGenericFamilyChange()
{
    const auto& childFont = m_style.fontDescription();

    if (childFont.isAbsoluteSize())
        return;

    const auto& parentFont = parentStyle().fontDescription();
    if (childFont.useFixedDefaultSize() == parentFont.useFixedDefaultSize())
        return;

    // We know the parent is monospace or the child is monospace, and that font
    // size was unspecified. We want to scale our font size as appropriate.
    // If the font uses a keyword size, then we refetch from the table rather than
    // multiplying by our scale factor.
    float size = [&] {
        if (CSSValueID sizeIdentifier = childFont.keywordSizeAsIdentifier())
            return Style::fontSizeForKeyword(sizeIdentifier, childFont.useFixedDefaultSize(), document());

        auto fixedSize =  document().settings().defaultFixedFontSize();
        auto defaultSize =  document().settings().defaultFontSize();
        float fixedScaleFactor = (fixedSize && defaultSize) ? static_cast<float>(fixedSize) / defaultSize : 1;
        return parentFont.useFixedDefaultSize() ? childFont.specifiedSize() / fixedScaleFactor : childFont.specifiedSize() * fixedScaleFactor;
    }();

    auto newFontDescription = childFont;
    setFontSize(newFontDescription, size);
    m_style.setFontDescriptionWithoutUpdate(WTF::move(newFontDescription));
}

void Mutator::updateFontForOrientationChange()
{
    auto [fontOrientation, glyphOrientation] = m_style.fontAndGlyphOrientation();

    const auto& fontDescription = m_style.fontDescription();
    if (fontDescription.orientation() == fontOrientation && fontDescription.nonCJKGlyphOrientation() == glyphOrientation)
        return;

    auto newFontDescription = fontDescription;
    newFontDescription.setNonCJKGlyphOrientation(glyphOrientation);
    newFontDescription.setOrientation(fontOrientation);
    m_style.setFontDescriptionWithoutUpdate(WTF::move(newFontDescription));
}

void Mutator::updateFontForSizeChange()
{
    m_style.synchronizeLetterSpacingWithFontCascadeWithoutUpdate();
    m_style.synchronizeWordSpacingWithFontCascadeWithoutUpdate();
}

void Mutator::setFontSize(FontCascadeDescription& fontDescription, float size)
{
    fontDescription.setSpecifiedSize(size);
    auto computedFontSize = Style::computedFontSizeFromSpecifiedSize(size, fontDescription.isAbsoluteSize(), useSVGZoomRules(), style(), document());
    fontDescription.setComputedSize(computedFontSize.size, computedFontSize.usedZoomFactor);
}

} // namespace Style
} // namespace WebCore
