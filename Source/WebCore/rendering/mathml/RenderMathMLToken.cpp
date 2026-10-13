/*
 * Copyright (C) 2014 Frédéric Wang (fred.wang@free.fr). All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RenderMathMLToken.h"

#if ENABLE(MATHML)

#include "FontCascadeInlines.h"
#include "FontInlines.h"
#include "GlyphBuffer.h"
#include "GlyphPage.h"
#include "HighlightRegistry.h"
#include "InlineIteratorTextBox.h"
#include "LocalFrameView.h"
#include "MathMLElement.h"
#include "MathMLNames.h"
#include "MathMLTokenElement.h"
#include "NodeTraversal.h"
#include "PaintInfo.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderCombineText.h"
#include "RenderElement.h"
#include "RenderElementInlines.h"
#include "RenderHighlight.h"
#include "RenderIterator.h"
#include "RenderObjectInlines.h"
#include "RenderSelection.h"
#include "RenderText.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyledMarkedText.h"
#include "Text.h"
#include "TextBoxSelectableRange.h"
#include "TextPainter.h"
#include "TextRun.h"
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace MathMLNames;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderMathMLToken);

RenderMathMLToken::RenderMathMLToken(Type type, MathMLTokenElement& element, Style::ComputedStyle&& style)
    : RenderMathMLBlock(type, element, WTF::move(style))
{
}

RenderMathMLToken::RenderMathMLToken(Type type, Document& document, Style::ComputedStyle&& style)
    : RenderMathMLBlock(type, document, WTF::move(style))
{
}

RenderMathMLToken::~RenderMathMLToken() = default;

MathMLTokenElement& RenderMathMLToken::element() const
{
    return static_cast<MathMLTokenElement&>(nodeForNonAnonymous());
}

void RenderMathMLToken::updateTokenContent()
{
    RenderMathMLBlock::updateFromElement();
    setMathVariantGlyphDirty();
}

void RenderMathMLToken::computeIntrinsicLogicalWidthContributions()
{
    ASSERT(hasInvalidContentLogicalWidths());

    if (document().settings().coreMathMLDeprecateLegacyMathvariant())
        return RenderMathMLBlock::computeIntrinsicLogicalWidthContributions();

    if (m_mathVariantGlyphDirty)
        updateMathVariantGlyph();

    if (m_mathVariantCodePoint) {
        auto mathVariantGlyph = style().fontCascade().glyphDataForCharacter(m_mathVariantCodePoint.value(), m_mathVariantIsMirrored);
        if (mathVariantGlyph.font) {
            m_maxContentLogicalWidthContribution = protect(mathVariantGlyph.font)->widthForGlyph(mathVariantGlyph.glyph);
            m_minContentLogicalWidthContribution = m_maxContentLogicalWidthContribution;
            adjustContentLogicalWidthsForBorderAndPadding();
            clearContentLogicalWidthsInvalidation();
            return;
        }
    }

    RenderMathMLBlock::computeIntrinsicLogicalWidthContributions();
}

void RenderMathMLToken::updateMathVariantGlyph()
{
    ASSERT(m_mathVariantGlyphDirty);

    m_mathVariantCodePoint = std::nullopt;
    m_mathVariantGlyphDirty = false;

    // Early return if the token element contains RenderElements.
    // Note that the renderers corresponding to the children of the token element are wrapped inside an anonymous RenderBlock.
    if (const auto& block = downcast<RenderElement>(firstChild())) {
        if (childrenOfType<RenderElement>(*block).first())
            return;
    }

    const Ref tokenElement = element();
    auto text = tokenElement->textContent();
    StringView view = text;
    auto trimmed = view.trim(isASCIIWhitespaceWithoutFF<char16_t>);
    if (auto codePoint = trimmed.convertToSingleCodePoint()) {
        MathVariant mathvariant = mathMLStyle().mathVariant();
        if (mathvariant == MathVariant::None)
            mathvariant = tokenElement->hasTagName(MathMLNames::miTag) ? MathVariant::Italic : MathVariant::Normal;
        char32_t transformedCodePoint = mathVariantMapCodePoint(codePoint.value(), mathvariant);
        if (transformedCodePoint != codePoint.value()) {
            m_mathVariantCodePoint = transformedCodePoint;
            m_mathVariantSourceStart = 0;
            while (m_mathVariantSourceStart < view.length() && isASCIIWhitespaceWithoutFF(view[m_mathVariantSourceStart]))
                ++m_mathVariantSourceStart;
            m_mathVariantSourceLength = trimmed.length();
            m_mathVariantIsMirrored = writingMode().isBidiRTL();
        }
    }
}

void RenderMathMLToken::setMathVariantGlyphDirty()
{
    m_mathVariantGlyphDirty = true;
    setNeedsLayoutAndInvalidateContentLogicalWidths();
}

void RenderMathMLToken::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    RenderMathMLBlock::styleDidChange(diff, oldStyle);
    setMathVariantGlyphDirty();
}

void RenderMathMLToken::updateFromElement()
{
    RenderMathMLBlock::updateFromElement();
    setMathVariantGlyphDirty();
}

std::optional<LayoutUnit> RenderMathMLToken::firstLineBaseline() const
{
    if (m_mathVariantCodePoint) {
        auto mathVariantGlyph = style().fontCascade().glyphDataForCharacter(m_mathVariantCodePoint.value(), m_mathVariantIsMirrored);
        if (mathVariantGlyph.font) {
            auto baseline = LayoutUnit(-protect(mathVariantGlyph.font)->boundsForGlyph(mathVariantGlyph.glyph).y());
            return { borderAndPaddingBefore() + baseline };
        }
    }
    return RenderMathMLBlock::firstLineBaseline();
}

void RenderMathMLToken::layoutBlock(RelayoutChildren relayoutChildren, LayoutUnit pageLogicalHeight)
{
    ASSERT(needsLayout());

    insertPositionedChildrenIntoContainingBlock();

    if (relayoutChildren == RelayoutChildren::No && simplifiedLayout())
        return;

    layoutFloatingChildren();

    GlyphData mathVariantGlyph;
    if (m_mathVariantCodePoint)
        mathVariantGlyph = style().fontCascade().glyphDataForCharacter(m_mathVariantCodePoint.value(), m_mathVariantIsMirrored);

    if (!mathVariantGlyph.font) {
        RenderMathMLBlock::layoutBlock(relayoutChildren, pageLogicalHeight);
        return;
    }

    recomputeLogicalWidth();
    for (CheckedPtr child = firstInFlowChildBox(); child; child = child->nextInFlowSiblingBox())
        child->layoutIfNeeded();
    setLogicalWidth(LayoutUnit(protect(mathVariantGlyph.font)->widthForGlyph(mathVariantGlyph.glyph)));
    setLogicalHeight(LayoutUnit(protect(mathVariantGlyph.font)->boundsForGlyph(mathVariantGlyph.glyph).height()));

    adjustLayoutForBorderAndPadding();

    updateLogicalHeight();

    layoutOutOfFlowBoxes(relayoutChildren);
    addSpecializedGlyphOverflow();
}

struct SpecializedGlyphSourceRange {
    CheckedRef<RenderText> renderer;
    unsigned start;
    unsigned length;
};

static Vector<SpecializedGlyphSourceRange, 2> specializedGlyphSourceRanges(const RenderMathMLToken& token, unsigned start, unsigned length)
{
    Vector<SpecializedGlyphSourceRange, 2> result;
    unsigned offset = 0;
    auto& tokenNode = token.element();
    for (CheckedPtr node = tokenNode.firstChild(); node; node = NodeTraversal::next(*node, &tokenNode)) {
        RefPtr text = dynamicDowncast<Text>(*node);
        if (!text)
            continue;
        auto textLength = text->length();
        auto from = std::max(start, offset);
        auto to = std::min(start + length, offset + textLength);
        if (from < to) {
            if (CheckedPtr renderer = text->renderer())
                result.append({ CheckedRef { *renderer }, from - offset, to - from });
        }
        offset += textLength;
        if (offset >= start + length)
            break;
    }
    return result;
}

std::optional<std::pair<unsigned, unsigned>> RenderMathMLToken::specializedGlyphSourceRange() const
{
    if (!m_mathVariantCodePoint)
        return std::nullopt;
    return std::pair { m_mathVariantSourceStart, m_mathVariantSourceLength };
}

LayoutUnit RenderMathMLToken::specializedGlyphBaseline() const
{
    auto glyph = style().fontCascade().glyphDataForCharacter(*m_mathVariantCodePoint, m_mathVariantIsMirrored);
    return borderAndPaddingBefore() + LayoutUnit { -protect(glyph.font)->boundsForGlyph(glyph.glyph).y() };
}

LayoutRect RenderMathMLToken::specializedGlyphInkRect(float strokeOutset) const
{
    auto glyph = style().fontCascade().glyphDataForCharacter(*m_mathVariantCodePoint, m_mathVariantIsMirrored);
    auto bounds = protect(glyph.font)->boundsForGlyph(glyph.glyph);
    bounds.move(borderLeft() + paddingLeft(), specializedGlyphBaseline());
    bounds.inflate(strokeOutset);
    return enclosingLayoutRect(bounds);
}

std::optional<LayoutRect> RenderMathMLToken::specializedGlyphRect() const
{
    if (!m_mathVariantCodePoint)
        return std::nullopt;
    auto glyph = style().fontCascade().glyphDataForCharacter(*m_mathVariantCodePoint, m_mathVariantIsMirrored);
    if (!glyph.font)
        return std::nullopt;
    auto bounds = protect(glyph.font)->boundsForGlyph(glyph.glyph);
    auto& metrics = protect(glyph.font)->fontMetrics();
    auto ascent = std::max(-bounds.y(), metrics.ascent());
    auto descent = std::max({ bounds.maxY(), metrics.descent(), protect(glyph.font)->platformData().size() - ascent });
    return LayoutRect { borderLeft() + paddingLeft(), borderAndPaddingBefore() - bounds.y() - ascent, LayoutUnit { protect(glyph.font)->widthForGlyph(glyph.glyph) }, LayoutUnit { ascent + descent } };
}

std::optional<LayoutRect> RenderMathMLToken::specializedGlyphSelectionRect(const RenderText& text, unsigned start, unsigned end) const
{
    auto sourceRange = specializedGlyphSourceRange();
    if (!sourceRange)
        return std::nullopt;
    for (auto& source : specializedGlyphSourceRanges(*this, sourceRange->first, sourceRange->second)) {
        if (&source.renderer.get() == &text && TextBoxSelectableRange { source.start, source.length }.intersects(start, end))
            return specializedGlyphRect();
    }
    return std::nullopt;
}

std::optional<LayoutRect> RenderMathMLToken::specializedGlyphRepaintRect(const RenderText& text, unsigned start, unsigned end) const
{
    auto sourceRange = specializedGlyphSourceRange();
    if (!sourceRange)
        return std::nullopt;
    auto sources = specializedGlyphSourceRanges(*this, sourceRange->first, sourceRange->second);
    for (auto& source : sources) {
        if (&source.renderer.get() == &text && TextBoxSelectableRange { source.start, source.length }.intersects(start, end)) {
            auto rect = specializedGlyphRepaintRect(sources.span());
            // Boxless style replacement can discard the old pseudo-style cache
            // before the source renderer repaints. Its paint owner's overflow
            // still covers those previously painted effects.
            if (rect)
                rect->unite(visualOverflowRect());
            return rect;
        }
    }
    return std::nullopt;
}

std::optional<LayoutRect> RenderMathMLToken::specializedGlyphRepaintRect() const
{
    auto sourceRange = specializedGlyphSourceRange();
    if (!sourceRange)
        return std::nullopt;
    return specializedGlyphRepaintRect(specializedGlyphSourceRanges(*this, sourceRange->first, sourceRange->second).span());
}

std::optional<LayoutRect> RenderMathMLToken::specializedGlyphRepaintRect(std::span<const SpecializedGlyphSourceRange> sources) const
{
    auto rect = specializedGlyphRect();
    if (!rect || sources.empty())
        return std::nullopt;

    Vector<const Style::ComputedStyle*, 4> pseudoStyles;
    auto includeStyle = [&](const Style::ComputedStyle& pseudoStyle) {
        if (!pseudoStyles.contains(&pseudoStyle))
            pseudoStyles.append(&pseudoStyle);
    };
    Vector<const Style::ComputedStyle*, 2> originatingStyles;
    auto includeCachedEffects = [&](const Style::ComputedStyle& originatingStyle) {
        if (originatingStyles.contains(&originatingStyle))
            return;
        originatingStyles.append(&originatingStyle);
        for (auto& entry : originatingStyle.pseudoElementStyles()) {
            if (entry.key.type == PseudoElementType::Highlight || entry.key.type == PseudoElementType::Selection)
                includeStyle(*entry.value);
        }
    };
    for (auto& source : sources) {
        // Resolve through the same source renderer as painting. Retained styles
        // cover erasure after a highlight is removed from the registry.
        if (RefPtr registry = document().highlightRegistryIfExists()) {
            for (auto& name : registry->highlightNames()) {
                if (auto* pseudoStyle = source.renderer->highlightPseudoStyle(name))
                    includeStyle(*pseudoStyle);
            }
        }
        if (auto* pseudoStyle = source.renderer->selectionPseudoStyle())
            includeStyle(*pseudoStyle);
        if (auto* ancestor = source.renderer->firstNonAnonymousAncestor())
            includeCachedEffects(ancestor->style());
        RefPtr text = source.renderer->textNode();
        RefPtr element = text ? text->parentElementInComposedTree() : nullptr;
        if (element && element->hasDisplayContents()) {
            if (auto* originatingStyle = element->renderOrDisplayContentsStyle())
                includeCachedEffects(*originatingStyle);
        }
    }
    auto baseline = specializedGlyphBaseline();
    auto baseRect = *rect;
    baseRect.unite(specializedGlyphInkRect());
    auto& metrics = style().metricsOfPrimaryFont();
    baseRect.unite(LayoutRect { borderLeft() + paddingLeft(), baseline - metrics.ascent(), rect->width(), LayoutUnit { metrics.height() } });
    float strokeOutset = 0;
    for (auto* pseudoStyle : pseudoStyles) {
        TextPaintStyle textStyle;
        textStyle.strokeWidth = pseudoStyle->usedStrokeWidth(view().frameView().size());
        textStyle.lineJoin = pseudoStyle->joinStyle();
        textStyle.miterLimit = pseudoStyle->strokeMiterLimit().value.value;
        strokeOutset = std::max(strokeOutset, strokeOutsetForTextPaintStyle(textStyle));
    }
    if (strokeOutset)
        baseRect.unite(specializedGlyphInkRect(strokeOutset));
    for (auto* pseudoStyle : pseudoStyles) {
        auto effectRect = baseRect;
        auto decorations = inkOverflowForDecorations(*pseudoStyle);
        effectRect.expand(LayoutBoxExtent { decorations.top(), decorations.right(), decorations.bottom(), decorations.left() });
        Style::adjustRectForShadow(effectRect, pseudoStyle->textShadow(), pseudoStyle->usedZoomForLength());
        rect->unite(effectRect);
    }
    return rect;
}

auto RenderMathMLToken::localRectsForRepaint(RepaintOutlineBounds outlineBounds) const -> RepaintRects
{
    if (isInsideEntirelyHiddenLayer())
        return { };
    auto rects = RenderMathMLBlock::localRectsForRepaint(outlineBounds);
    if (auto rect = specializedGlyphRepaintRect()) {
        rect->move(view().frameView().layoutContext().layoutDelta());
        rects.clippedOverflowRect.unite(*rect);
    }
    return rects;
}

void RenderMathMLToken::addSpecializedGlyphOverflow()
{
    if (auto rect = specializedGlyphRepaintRect())
        addVisualOverflow(*rect);
}

void RenderMathMLToken::invalidateHighlightOverflow()
{
    if (auto rect = specializedGlyphRepaintRect())
        invalidateHighlightOverflow(*rect);
}

void RenderMathMLToken::invalidateHighlightOverflow(const LayoutRect& rect)
{
    if (!visualOverflowRect().contains(rect)) {
        // Recompute ancestor paint bounds through normal overflow propagation.
        setNeedsLayout();
    }
}

void RenderMathMLToken::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLBlock::paint(info, paintOffset);

    if (info.context().paintingDisabled() || (info.phase != PaintPhase::Foreground && info.phase != PaintPhase::Selection) || info.paintBehavior.contains(PaintBehavior::ExcludeText) || usedStyle().visibility() != UsedVisibility::Visible || !m_mathVariantCodePoint)
        return;

    auto mathVariantGlyph = style().fontCascade().glyphDataForCharacter(m_mathVariantCodePoint.value(), m_mathVariantIsMirrored);
    if (!mathVariantGlyph.font)
        return;

    GraphicsContextStateSaver stateSaver(info.context());
    info.context().setFillColor(style().visitedDependentTextFillColorApplyingColorFilter());

    auto glyphAscent = -protect(mathVariantGlyph.font)->boundsForGlyph(mathVariantGlyph.glyph).y();
    // FIXME: If we're just drawing a single glyph, why do we need to compute an advance?
    auto advance = makeGlyphBufferAdvance(protect(mathVariantGlyph.font)->widthForGlyph(mathVariantGlyph.glyph));
    auto location = paintOffset + this->location() + LayoutPoint { borderLeft() + paddingLeft(), glyphAscent + borderAndPaddingBefore() };
    if (style().writingMode().isHorizontal())
        location.setY(roundToDevicePixel(LayoutUnit { location.y() }, protect(document())->deviceScaleFactor()));
    else
        location.setX(roundToDevicePixel(LayoutUnit { location.x() }, protect(document())->deviceScaleFactor()));

    if (document().printing() || (!document().hasHighlight() && !view().selection().start())) {
        if (info.phase == PaintPhase::Foreground)
            info.context().drawGlyphs(protect(*mathVariantGlyph.font), singleElementSpan(mathVariantGlyph.glyph), singleElementSpan(advance), location, style().fontCascade().fontDescription().usedFontSmoothing());
        return;
    }
    GlyphBuffer glyphBuffer;
    glyphBuffer.add(mathVariantGlyph.glyph, *mathVariantGlyph.font, advance, 0);
    auto adjustedPaintOffset = LayoutPoint { location.x() - this->location().x() - borderLeft() - paddingLeft(), location.y() - this->location().y() - LayoutUnit { glyphAscent } - borderAndPaddingBefore() };
    if (!paintHighlightedGlyphs(info, adjustedPaintOffset, location, *m_mathVariantCodePoint, &glyphBuffer, [&](TextPainter& painter, const TextPaintStyle&, const FloatRect& rect, const FloatPoint& origin) {
        painter.paintGlyph(mathVariantGlyph, rect, origin);
    }))
        info.context().drawGlyphs(protect(*mathVariantGlyph.font), singleElementSpan(mathVariantGlyph.glyph), singleElementSpan(advance), location, style().fontCascade().fontDescription().usedFontSmoothing());
}

bool RenderMathMLToken::paintHighlightedGlyphs(PaintInfo& info, const LayoutPoint& paintOffset, const FloatPoint& location, char32_t codePoint, const GlyphBuffer* glyphBuffer, NOESCAPE const Function<void(TextPainter&, const TextPaintStyle&, const FloatRect&, const FloatPoint&)>& paintGlyphs, NOESCAPE const Function<Vector<FloatSegment>(const FloatPoint&, const FloatRect&)>* glyphIntersections)
{
    if (document().printing() || (!document().hasHighlight() && !view().selection().start()))
        return info.phase == PaintPhase::Selection;
    auto sourceRange = specializedGlyphSourceRange();
    auto glyphRect = specializedGlyphRect();
    if (!sourceRange || !glyphRect)
        return false;
    // Source offsets belong to the original text; the mapped output is one visual cluster.
    auto sources = specializedGlyphSourceRanges(*this, sourceRange->first, sourceRange->second);
    auto rangeForHighlight = [&](RenderHighlight& highlight) -> std::pair<unsigned, unsigned> {
        if (highlight.start() == highlight.end() && highlight.startOffset() == highlight.endOffset())
            return { 0, 0 };
        for (auto& source : sources) {
            auto [start, end] = highlight.rangeForTextBox(source.renderer, { 0, source.renderer->text().length() });
            if (TextBoxSelectableRange { source.start, source.length }.intersects(start, end))
                return { 0, 1 };
        }
        return { 0, 0 };
    };
    bool isSelected = !document().printing() && rangeForHighlight(view().selection()).second;
    if (info.phase == PaintPhase::Selection && !isSelected)
        return true;
    if (isSelected && info.paintBehavior.contains(PaintBehavior::ExcludeSelection))
        return true;

    Vector<MarkedText> highlights;
    if (!sources.isEmpty() && !document().printing() && info.phase != PaintPhase::Selection)
        highlights = MarkedText::collectForHighlights(sources[0].renderer.get(), MarkedText::PaintPhase::Foreground, WTF::move(rangeForHighlight));
    if (highlights.isEmpty() && !isSelected)
        return false;

    auto rect = FloatRect { *glyphRect };
    rect.moveBy(paintOffset + this->location());
    auto& sourceRenderer = sources[0].renderer.get();
    auto paintBackground = [&](const Vector<MarkedText>& marks) {
        for (auto& mark : StyledMarkedText::subdivideAndResolve(marks, sourceRenderer, false, info)) {
            if (mark.style.backgroundColor.isVisible())
                info.context().fillRect(snapRectToDevicePixels(LayoutRect { rect }, document().deviceScaleFactor()), mark.style.backgroundColor);
        }
    };
    if (info.phase == PaintPhase::Foreground && !document().printing()) {
        paintBackground(highlights);
        if (isSelected)
            paintBackground(Vector<MarkedText> { { 0, 1, MarkedText::Type::Selection } });
    }
    if (info.phase != PaintPhase::Selection)
        highlights.insert(0, { 0, 1, MarkedText::Type::Unmarked });
    if (isSelected)
        highlights.append({ 0, 1, MarkedText::Type::Selection });

    for (auto& mark : StyledMarkedText::subdivideAndResolve(highlights, sourceRenderer, false, info)) {
        TextPainter painter { info.context(), style().fontCascade(), style(), mark.style.textStyles, mark.style.textShadow, style().appleColorFilter(), nullAtom(), 0, nullptr };
        auto decorations = TextDecorationPainter::textDecorationsInEffectForStyle(mark.style.textDecorationStyles);
        TextDecorationPainter decorationPainter { info.context(), style().fontCascade(), mark.style.textShadow, style().appleColorFilter(), document().printing(), writingMode() };
        auto fontAscent = style().metricsOfPrimaryFont().ascent();
        FloatPoint decorationOrigin { location.x(), location.y() - fontAscent };
        auto autoThickness = ceilToDevicePixel(Style::TextDecorationThickness { CSS::Keyword::Auto { } }.resolve(style()), document().deviceScaleFactor());
        auto thicknessFor = [&](const TextDecorationPainter::Styles::DecorationStyleAndColor& decoration) {
            return ceilToDevicePixel(decoration.thickness.value_or(Style::TextDecorationThickness { CSS::Keyword::Auto { } }).resolve(style()), document().deviceScaleFactor());
        };
        auto textBox = InlineIterator::textBoxesFor(sourceRenderer).begin();
        if (!decorations.isNone() && textBox && info.phase != PaintPhase::Selection) {
            auto thickness = thicknessFor(decorations.hasUnderline() ? mark.style.textDecorationStyles.underline : decorations.hasOverline() ? mark.style.textDecorationStyles.overline : mark.style.textDecorationStyles.linethrough);
            auto inlineBox = textBox->parentInlineBox();
            auto underlineOffset = underlineOffsetForTextBoxPainting(*inlineBox, style(), mark.style.textDecorationStyles.underlineOffset);
            if (mark.style.textDecorationStyles.underline.decorationStyle == TextDecorationStyle::Wavy)
                underlineOffset += wavyOffsetFromDecoration();
            auto overlineOffset = overlineOffsetForTextBoxPainting(*inlineBox, style()) + autoThickness - thickness;
            if (mark.style.textDecorationStyles.overline.decorationStyle == TextDecorationStyle::Wavy)
                overlineOffset -= wavyOffsetFromDecoration();
            // The run supplies character classification; ink skipping uses the exact mathematical glyph.
            auto mappedText = String::fromCodePoint(codePoint);
            TextRun run { mappedText };
            decorationPainter.paintBackgroundDecorations(style(), run, { location, decorationOrigin, rect.width(), thickness, underlineOffset, overlineOffset, 2 * fontAscent / 3 + (autoThickness - thickness) / 2, fontAscent + 2, wavyStrokeParameters(style().usedFontSize()) }, decorations, mark.style.textDecorationStyles, document().deviceScaleFactor(), glyphBuffer, glyphIntersections);
        }
        updateGraphicsContext(info.context(), mark.style.textStyles);
        paintGlyphs(painter, mark.style.textStyles, rect, location);
        if (decorations.hasLineThrough() && info.phase != PaintPhase::Selection) {
            auto thickness = thicknessFor(mark.style.textDecorationStyles.linethrough);
            decorationPainter.paintForegroundDecorations({ decorationOrigin, rect.width(), thickness, 2 * fontAscent / 3 + (autoThickness - thickness) / 2, wavyStrokeParameters(style().usedFontSize()) }, mark.style.textDecorationStyles);
        }
    }
    return true;
}

void RenderMathMLToken::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    if (m_mathVariantCodePoint) {
        auto mathVariantGlyph = style().fontCascade().glyphDataForCharacter(m_mathVariantCodePoint.value(), m_mathVariantIsMirrored);
        if (mathVariantGlyph.font)
            return;
    }

    RenderMathMLBlock::paintChildren(paintInfo, paintOffset, paintInfoForChild, usePrintRect);
}

}

#endif // ENABLE(MATHML)
