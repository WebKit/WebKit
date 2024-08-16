/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 * Copyright (C) 2023, 2024 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGInlineTextBox.h"

#include "FloatConversion.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "LegacyInlineFlowBox.h"
#include "LegacyRenderSVGResourceSolidColor.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "PointerEventsHitRules.h"
#include "RenderBlock.h"
#include "RenderInline.h"
#include "RenderSVGText.h"
#include "RenderStyleInlines.h"
#include "RenderView.h"
#include "SVGInlineTextBoxInlines.h"
#include "SVGPaintServerHandling.h"
#include "SVGRenderStyle.h"
#include "SVGRenderingContext.h"
#include "SVGResourcesCache.h"
#include "SVGRootInlineBox.h"
#include "TextBoxSelectableRange.h"
#include "TextPainter.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGInlineTextBox);

struct ExpectedSVGInlineTextBoxSize : public LegacyInlineTextBox {
    float float1;
    uint32_t bitfields : 5;
    void* pointer;
    SVGPaintServerOrColor paintServerOrColor;
    Vector<SVGTextFragment> vector;
};

static_assert(sizeof(SVGInlineTextBox) == sizeof(ExpectedSVGInlineTextBoxSize), "SVGInlineTextBox is not of expected size");

SVGInlineTextBox::SVGInlineTextBox(RenderSVGInlineText& renderer)
    : LegacyInlineTextBox(renderer)
    , m_legacyPaintingResourceMode(OptionSet<RenderSVGResourceMode>().toRaw())
    , m_startsNewTextChunk(false)
{
}

void SVGInlineTextBox::dirtyOwnLineBoxes()
{
    LegacyInlineTextBox::dirtyLineBoxes();

    // Clear the now stale text fragments
    clearTextFragments();
}

void SVGInlineTextBox::dirtyLineBoxes()
{
    dirtyOwnLineBoxes();

    // And clear any following text fragments as the text on which they
    // depend may now no longer exist, or glyph positions may be wrong
    for (auto* nextBox = nextTextBox(); nextBox; nextBox = nextBox->nextTextBox())
        nextBox->dirtyOwnLineBoxes();
}

int SVGInlineTextBox::offsetForPositionInFragment(const SVGTextFragment& fragment, float position) const
{
    float scalingFactor = renderer().scalingFactor();
    ASSERT(scalingFactor);

    TextRun textRun = constructTextRun(renderer().style(), fragment);

    // Eventually handle lengthAdjust="spacingAndGlyphs".
    // FIXME: Handle vertical text.
    AffineTransform fragmentTransform;
    fragment.buildFragmentTransform(fragmentTransform);
    if (!fragmentTransform.isIdentity())
        textRun.setHorizontalGlyphStretch(narrowPrecisionToFloat(fragmentTransform.xScale()));

    const bool includePartialGlyphs = true;
    return fragment.characterOffset - start() + renderer().scaledFont().offsetForPosition(textRun, position * scalingFactor, includePartialGlyphs);
}

FloatRect SVGInlineTextBox::selectionRectForTextFragment(const SVGTextFragment& fragment, unsigned startPosition, unsigned endPosition, const RenderStyle& style) const
{
    ASSERT_WITH_SECURITY_IMPLICATION(startPosition < endPosition);

    float scalingFactor = renderer().scalingFactor();
    ASSERT(scalingFactor);

    const FontCascade& scaledFont = renderer().scaledFont();
    const FontMetrics& scaledFontMetrics = scaledFont.metricsOfPrimaryFont();
    FloatPoint textOrigin(fragment.x, fragment.y);
    if (scalingFactor != 1)
        textOrigin.scale(scalingFactor);

    textOrigin.move(0, -scaledFontMetrics.ascent());

    LayoutRect selectionRect { textOrigin, LayoutSize(0, LayoutUnit(fragment.height * scalingFactor)) };
    TextRun run = constructTextRun(style, fragment);
    scaledFont.adjustSelectionRectForText(renderer().canUseSimplifiedTextMeasuring().value_or(false), run, selectionRect, startPosition, endPosition);
    FloatRect snappedSelectionRect = snapRectToDevicePixelsWithWritingDirection(selectionRect, renderer().document().deviceScaleFactor(), run.ltr());
    if (scalingFactor == 1)
        return snappedSelectionRect;

    snappedSelectionRect.scale(1 / scalingFactor);
    return snappedSelectionRect;
}

LayoutRect SVGInlineTextBox::localSelectionRect(unsigned start, unsigned end) const
{
    auto [clampedStart, clampedEnd] = selectableRange().clamp(start, end);

    if (clampedStart >= clampedEnd)
        return LayoutRect();

    auto& style = renderer().style();

    AffineTransform fragmentTransform;
    FloatRect selectionRect;
    unsigned fragmentStartPosition = 0;
    unsigned fragmentEndPosition = 0;

    unsigned textFragmentsSize = m_textFragments.size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        const SVGTextFragment& fragment = m_textFragments.at(i);

        fragmentStartPosition = clampedStart;
        fragmentEndPosition = clampedEnd;
        if (!mapStartEndPositionsIntoFragmentCoordinates(fragment, fragmentStartPosition, fragmentEndPosition))
            continue;

        FloatRect fragmentRect = selectionRectForTextFragment(fragment, fragmentStartPosition, fragmentEndPosition, style);
        fragment.buildFragmentTransform(fragmentTransform);
        if (!fragmentTransform.isIdentity())
            fragmentRect = fragmentTransform.mapRect(fragmentRect);

        selectionRect.unite(fragmentRect);
    }

    return enclosingIntRect(selectionRect);
}

static inline bool textShouldBePainted(const RenderSVGInlineText& textRenderer)
{
    return textRenderer.scaledFont().size() >= 0.5;
}

void SVGInlineTextBox::paintSelectionBackground(PaintInfo& paintInfo)
{
    ASSERT(paintInfo.shouldPaintWithinRoot(renderer()));
    ASSERT(paintInfo.phase == PaintPhase::Foreground || paintInfo.phase == PaintPhase::Selection);

    if (renderer().style().usedVisibility() != Visibility::Visible)
        return;

    auto& parentRenderer = parent()->renderer();
    ASSERT(!parentRenderer.document().printing());

    // Determine whether or not we're selected.
    bool paintSelectedTextOnly = paintInfo.phase == PaintPhase::Selection;
    bool hasSelection = selectionState() != RenderObject::HighlightState::None;
    if (!hasSelection || paintSelectedTextOnly)
        return;

    Color backgroundColor = renderer().selectionBackgroundColor();
    if (!backgroundColor.isVisible())
        return;

    if (!textShouldBePainted(renderer()))
        return;

    auto& style = parentRenderer.style();

    auto [startPosition, endPosition] = selectionStartEnd();

    unsigned fragmentStartPosition = 0;
    unsigned fragmentEndPosition = 0;
    AffineTransform fragmentTransform;
    unsigned textFragmentsSize = m_textFragments.size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        SVGTextFragment& fragment = m_textFragments.at(i);
        ASSERT(!m_legacyPaintingResource);

        fragmentStartPosition = startPosition;
        fragmentEndPosition = endPosition;
        if (!mapStartEndPositionsIntoFragmentCoordinates(fragment, fragmentStartPosition, fragmentEndPosition))
            continue;

        GraphicsContextStateSaver stateSaver(paintInfo.context());
        fragment.buildFragmentTransform(fragmentTransform);
        if (!fragmentTransform.isIdentity())
            paintInfo.context().concatCTM(fragmentTransform);

        paintInfo.context().setFillColor(backgroundColor);
        paintInfo.context().fillRect(selectionRectForTextFragment(fragment, fragmentStartPosition, fragmentEndPosition, style), backgroundColor);

        setPaintingResourceMode({ });
    }

    ASSERT(!m_legacyPaintingResource);
}

void SVGInlineTextBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit, LayoutUnit)
{
    ASSERT(paintInfo.shouldPaintWithinRoot(renderer()));
    ASSERT(paintInfo.phase == PaintPhase::Foreground || paintInfo.phase == PaintPhase::Selection);

    if (renderer().style().usedVisibility() != Visibility::Visible)
        return;

    // Note: We're explicitly not supporting composition & custom underlines and custom highlighters - unlike LegacyInlineTextBox.
    // If we ever need that for SVG, it's very easy to refactor and reuse the code.

    auto& parentRenderer = parent()->renderer();

    bool paintSelectedTextOnly = paintInfo.phase == PaintPhase::Selection;
    bool shouldPaintSelectionHighlight = !(paintInfo.paintBehavior.contains(PaintBehavior::SkipSelectionHighlight));
    bool hasSelection = !parentRenderer.document().printing() && selectionState() != RenderObject::HighlightState::None;
    if (!hasSelection && paintSelectedTextOnly)
        return;

    if (!textShouldBePainted(renderer()))
        return;

    auto& style = parentRenderer.style();

    const SVGRenderStyle& svgStyle = style.svgStyle();

    bool hasFill = svgStyle.hasFill();
    bool hasVisibleStroke = style.hasVisibleStroke();

    const RenderStyle* selectionStyle = &style;
    if (hasSelection && shouldPaintSelectionHighlight) {
        selectionStyle = parentRenderer.getCachedPseudoStyle({ PseudoId::Selection });
        if (selectionStyle) {
            if (!hasFill)
                hasFill = selectionStyle->svgStyle().hasFill();
            if (!hasVisibleStroke)
                hasVisibleStroke = selectionStyle->hasVisibleStroke();
        } else
            selectionStyle = &style;
    }

    if (renderer().view().frameView().paintBehavior().contains(PaintBehavior::RenderingSVGClipOrMask)) {
        hasFill = true;
        hasVisibleStroke = false;
    }

    AffineTransform fragmentTransform;
    unsigned textFragmentsSize = m_textFragments.size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        SVGTextFragment& fragment = m_textFragments.at(i);
        ASSERT(!m_legacyPaintingResource);

        GraphicsContextStateSaver stateSaver(paintInfo.context());
        fragment.buildFragmentTransform(fragmentTransform);
        if (!fragmentTransform.isIdentity())
            paintInfo.context().concatCTM(fragmentTransform);

        // Spec: All text decorations except line-through should be drawn before the text is filled and stroked; thus, the text is rendered on top of these decorations.
        auto decorations = style.textDecorationsInEffect();
        if (decorations & TextDecorationLine::Underline)
            paintDecoration(paintInfo.context(), TextDecorationLine::Underline, fragment);
        if (decorations & TextDecorationLine::Overline)
            paintDecoration(paintInfo.context(), TextDecorationLine::Overline, fragment);

        for (auto type : RenderStyle::paintTypesForPaintOrder(style.paintOrder())) {
            switch (type) {
            case PaintType::Fill:
                if (!hasFill)
                    continue;
                setPaintingResourceMode({ RenderSVGResourceMode::ApplyToFill, RenderSVGResourceMode::ApplyToText });
                ASSERT(selectionStyle);
                paintText(paintInfo.context(), style, *selectionStyle, fragment, hasSelection, paintSelectedTextOnly);
                break;
            case PaintType::Stroke:
                if (!hasVisibleStroke)
                    continue;
                setPaintingResourceMode({ RenderSVGResourceMode::ApplyToStroke, RenderSVGResourceMode::ApplyToText});
                ASSERT(selectionStyle);
                paintText(paintInfo.context(), style, *selectionStyle, fragment, hasSelection, paintSelectedTextOnly);
                break;
            case PaintType::Markers:
                continue;
            }
        }

        // Spec: Line-through should be drawn after the text is filled and stroked; thus, the line-through is rendered on top of the text.
        if (decorations & TextDecorationLine::LineThrough)
            paintDecoration(paintInfo.context(), TextDecorationLine::LineThrough, fragment);

        setPaintingResourceMode({ });
    }

    // Finally, paint the outline if any.
    if (renderer().style().hasOutline()) {
        if (CheckedPtr renderInline = dynamicDowncast<RenderInline>(parentRenderer))
            renderInline->paintOutline(paintInfo, paintOffset);
    }

    ASSERT(!m_legacyPaintingResource);
}

bool SVGInlineTextBox::acquirePaintingResource(SVGPaintServerHandling& paintServerHandling, float scalingFactor, RenderBoxModelObject& renderer, const RenderStyle& style)
{
    ASSERT(scalingFactor);
    ASSERT(!paintingResourceMode().isEmpty());
    ASSERT(renderer.document().settings().layerBasedSVGEngineEnabled());

    auto& context = paintServerHandling.context();
    if (paintingResourceMode().contains(RenderSVGResourceMode::ApplyToFill)) {
        if (!paintServerHandling.preparePaintOperation<SVGPaintServerHandling::Operation::Fill>(parent()->renderer(), style))
            return false;
        context.save();
        context.setTextDrawingMode(TextDrawingMode::Fill);
    } else if (paintingResourceMode().contains(RenderSVGResourceMode::ApplyToStroke)) {
        if (!paintServerHandling.preparePaintOperation<SVGPaintServerHandling::Operation::Stroke>(parent()->renderer(), style))
            return false;
        context.save();
        context.setTextDrawingMode(TextDrawingMode::Stroke);

        if (style.svgStyle().vectorEffect() == VectorEffect::NonScalingStroke) {
            if (style.fontDescription().textRenderingMode() == TextRenderingMode::GeometricPrecision)
                scalingFactor = 1.0 / RenderSVGInlineText::computeScalingFactorForRenderer(renderer);
            else
                scalingFactor = 1.0;

            if (auto zoomFactor = renderer.style().usedZoom(); zoomFactor != 1.0)
                scalingFactor *= zoomFactor;

            if (auto deviceScaleFactor = renderer.document().deviceScaleFactor(); deviceScaleFactor != 1.0)
                scalingFactor *= deviceScaleFactor;
        }

        if (scalingFactor != 1.0)
            context.setStrokeThickness(context.strokeThickness() * scalingFactor);
    }

    if (context.fillGradient() || context.strokeGradient() || context.fillPattern() || context.strokePattern()) {
        context.beginTransparencyLayer(1);
        context.setCompositeOperation(CompositeOperator::SourceOver);
    }

    return true;
}

void SVGInlineTextBox::releasePaintingResource(SVGPaintServerHandling& paintServerHandling)
{
    auto& context = paintServerHandling.context();

    if (context.fillGradient() || context.strokeGradient() || context.fillPattern() || context.strokePattern())
        context.endTransparencyLayer();

    context.restore();
}

bool SVGInlineTextBox::acquireLegacyPaintingResource(GraphicsContext*& context, float scalingFactor, RenderBoxModelObject& renderer, const RenderStyle& style)
{
    ASSERT(scalingFactor);
    ASSERT(!paintingResourceMode().isEmpty());

    Color fallbackColor;
    if (paintingResourceMode().contains(RenderSVGResourceMode::ApplyToFill))
        m_legacyPaintingResource = LegacyRenderSVGResource::fillPaintingResource(renderer, style, fallbackColor);
    else if (paintingResourceMode().contains(RenderSVGResourceMode::ApplyToStroke))
        m_legacyPaintingResource = LegacyRenderSVGResource::strokePaintingResource(renderer, style, fallbackColor);
    else {
        // We're either called for stroking or filling.
        ASSERT_NOT_REACHED();
    }

    if (!m_legacyPaintingResource)
        return false;

    if (!resourceWasApplied(m_legacyPaintingResource->applyResource(renderer, style, context, paintingResourceMode()))) {
        if (!fallbackColor.isValid()) {
            m_legacyPaintingResource = nullptr;
            return false;
        }
        
        auto* fallbackResource = LegacyRenderSVGResource::sharedSolidPaintingResource();
        fallbackResource->setColor(fallbackColor);

        m_legacyPaintingResource = fallbackResource;
        if (!resourceWasApplied(m_legacyPaintingResource->applyResource(renderer, style, context, paintingResourceMode()))) {
            m_legacyPaintingResource = nullptr;
            return false;
        }
    }

    if (paintingResourceMode().contains(RenderSVGResourceMode::ApplyToStroke)) {
        if (style.svgStyle().vectorEffect() == VectorEffect::NonScalingStroke) {
            if (style.fontDescription().textRenderingMode() == TextRenderingMode::GeometricPrecision)
                scalingFactor = 1.0 / RenderSVGInlineText::computeScalingFactorForRenderer(renderer);
            else
                scalingFactor = 1.0;

            if (auto zoomFactor = renderer.style().usedZoom(); zoomFactor != 1.0)
                scalingFactor *= zoomFactor;

            if (auto deviceScaleFactor = renderer.document().deviceScaleFactor(); deviceScaleFactor != 1.0)
                scalingFactor *= deviceScaleFactor;
        }

        if (scalingFactor != 1.0)
            context->setStrokeThickness(context->strokeThickness() * scalingFactor);
    }

    return true;
}

void SVGInlineTextBox::releaseLegacyPaintingResource(GraphicsContext*& context, const Path* path)
{
    ASSERT(m_legacyPaintingResource);

    m_legacyPaintingResource->postApplyResource(parent()->renderer(), context, paintingResourceMode(), path, /*LegacyRenderSVGShape*/ nullptr);
    m_legacyPaintingResource = nullptr;
}

TextRun SVGInlineTextBox::constructTextRun(const RenderStyle& style, const SVGTextFragment& fragment) const
{
    TextRun run(StringView(renderer().text()).substring(fragment.characterOffset, fragment.length),
        0, /* xPos, only relevant with allowTabs=true */
        0, /* padding, only relevant for justified text, not relevant for SVG */
        ExpansionBehavior::allowRightOnly(),
        direction(),
        style.rtlOrdering() == Order::Visual /* directionalOverride */);

    // We handle letter & word spacing ourselves.
    run.disableSpacing();
    return run;
}

bool SVGInlineTextBox::mapStartEndPositionsIntoFragmentCoordinates(const SVGTextFragment& fragment, unsigned& startPosition, unsigned& endPosition) const
{
    unsigned startFragment = fragment.characterOffset - start();
    unsigned endFragment = startFragment + fragment.length;

    // Find intersection between the intervals: [startFragment..endFragment) and [startPosition..endPosition)
    startPosition = std::max(startFragment, startPosition);
    endPosition = std::min(endFragment, endPosition);

    if (startPosition >= endPosition)
        return false;

    startPosition -= startFragment;
    endPosition -= startFragment;

    return true;
}

static inline float positionOffsetForDecoration(OptionSet<TextDecorationLine> decoration, const FontMetrics& fontMetrics, float thickness)
{
    // FIXME: For SVG Fonts we need to use the attributes defined in the <font-face> if specified.
    // Compatible with Batik/Opera.
    const float ascent = fontMetrics.ascent();
    if (decoration == TextDecorationLine::Underline)
        return ascent + thickness * 1.5f;
    if (decoration == TextDecorationLine::Overline)
        return thickness;
    if (decoration == TextDecorationLine::LineThrough)
        return ascent * 5 / 8.0f;

    ASSERT_NOT_REACHED();
    return 0.0f;
}

static inline float thicknessForDecoration(OptionSet<TextDecorationLine>, const FontCascade& font)
{
    // FIXME: For SVG Fonts we need to use the attributes defined in the <font-face> if specified.
    // Compatible with Batik/Opera
    return font.size() / 20.0f;
}

static inline RenderBoxModelObject& findRendererDefininingTextDecoration(LegacyInlineFlowBox* parentBox)
{
    // Lookup first render object in parent hierarchy which has text-decoration set.
    RenderBoxModelObject* renderer = nullptr;
    while (parentBox) {
        renderer = &parentBox->renderer();

        if (!renderer->style().textDecorationLine().isEmpty())
            break;

        parentBox = parentBox->parent();
    }

    ASSERT(renderer);
    return *renderer;
}

void SVGInlineTextBox::paintDecoration(GraphicsContext& context, OptionSet<TextDecorationLine> decoration, const SVGTextFragment& fragment)
{
    if (renderer().style().textDecorationsInEffect().isEmpty())
        return;

    // Find out which render style defined the text-decoration, as its fill/stroke properties have to be used for drawing instead of ours.
    auto& decorationRenderer = findRendererDefininingTextDecoration(parent());
    const RenderStyle& decorationStyle = decorationRenderer.style();

    if (decorationStyle.usedVisibility() == Visibility::Hidden)
        return;

    const SVGRenderStyle& svgDecorationStyle = decorationStyle.svgStyle();

    for (auto type : RenderStyle::paintTypesForPaintOrder(renderer().style().paintOrder())) {
        switch (type) {
        case PaintType::Fill:
            if (svgDecorationStyle.hasFill()) {
                setPaintingResourceMode(RenderSVGResourceMode::ApplyToFill);
                paintDecorationWithStyle(context, decoration, fragment, decorationRenderer);
            }
            break;
        case PaintType::Stroke:
            if (decorationStyle.hasVisibleStroke()) {
                setPaintingResourceMode(RenderSVGResourceMode::ApplyToStroke);
                paintDecorationWithStyle(context, decoration, fragment, decorationRenderer);
            }
            break;
        case PaintType::Markers:
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }
}

void SVGInlineTextBox::paintDecorationWithStyle(GraphicsContext& context, OptionSet<TextDecorationLine> decoration, const SVGTextFragment& fragment, RenderBoxModelObject& decorationRenderer)
{
    ASSERT(!m_legacyPaintingResource);
    ASSERT(!paintingResourceMode().isEmpty());

    auto& decorationStyle = decorationRenderer.style();

    float scalingFactor = 1;
    FontCascade scaledFont;
    RenderSVGInlineText::computeNewScaledFontForStyle(decorationRenderer, decorationStyle, scalingFactor, scaledFont);
    ASSERT(scalingFactor);

    // The initial y value refers to overline position.
    float thickness = thicknessForDecoration(decoration, scaledFont);

    if (fragment.width <= 0 && thickness <= 0)
        return;

    FloatPoint decorationOrigin(fragment.x, fragment.y);
    float width = fragment.width;
    const FontMetrics& scaledFontMetrics = scaledFont.metricsOfPrimaryFont();

    GraphicsContextStateSaver stateSaver(context);
    if (scalingFactor != 1) {
        width *= scalingFactor;
        decorationOrigin.scale(scalingFactor);
        context.scale(1 / scalingFactor);
    }

    decorationOrigin.move(0, -scaledFontMetrics.ascent() + positionOffsetForDecoration(decoration, scaledFontMetrics, thickness));

    Path path;
    path.addRect(FloatRect(decorationOrigin, FloatSize(width, thickness)));

    if (decorationRenderer.document().settings().layerBasedSVGEngineEnabled()) {
        SVGPaintServerHandling paintServerHandling { context };
        if (acquirePaintingResource(paintServerHandling, scalingFactor, decorationRenderer, decorationStyle)) {
            if (paintingResourceMode().contains(RenderSVGResourceMode::ApplyToFill))
                context.fillPath(path);
            else if (paintingResourceMode().contains(RenderSVGResourceMode::ApplyToStroke))
                context.strokePath(path);

            releasePaintingResource(paintServerHandling);
        }
        return;
    }

    GraphicsContext* usedContext = &context;
    if (acquireLegacyPaintingResource(usedContext, scalingFactor, decorationRenderer, decorationStyle))
        releaseLegacyPaintingResource(usedContext, &path);
}

void SVGInlineTextBox::paintTextWithShadows(GraphicsContext& context, const RenderStyle& style, TextRun& textRun, const SVGTextFragment& fragment, unsigned startPosition, unsigned endPosition)
{
    float scalingFactor = renderer().scalingFactor();
    ASSERT(scalingFactor);

    const FontCascade& scaledFont = renderer().scaledFont();
    const ShadowData* shadow = style.textShadow();

    FloatPoint textOrigin(fragment.x, fragment.y);
    FloatSize textSize(fragment.width, fragment.height);

    if (scalingFactor != 1) {
        textOrigin.scale(scalingFactor);
        textSize.scale(scalingFactor);
    }

    FloatRect shadowRect(FloatPoint(textOrigin.x(), textOrigin.y() - scaledFont.metricsOfPrimaryFont().ascent()), textSize);

    GraphicsContext* usedContext = &context;
    SVGPaintServerHandling paintServerHandling { context };

    auto prepareGraphicsContext = [&]() -> bool {
        if (renderer().document().settings().layerBasedSVGEngineEnabled())
            return acquirePaintingResource(paintServerHandling, scalingFactor, parent()->renderer(), style);
        return acquireLegacyPaintingResource(usedContext, scalingFactor, parent()->renderer(), style);
    };

    auto restoreGraphicsContext = [&]() {
        if (renderer().document().settings().layerBasedSVGEngineEnabled()) {
            releasePaintingResource(paintServerHandling);
            return;
        }
        releaseLegacyPaintingResource(usedContext, /* path */nullptr);
    };

    do {
        if (!prepareGraphicsContext())
            break;

        {
            // Optimized code path to support gradient/pattern fill/stroke on text without using temporary ImageBuffers / masking.
            RefPtr<Gradient> gradient;
            RefPtr<Pattern> pattern;
            if (renderer().document().settings().layerBasedSVGEngineEnabled()) {
                auto* textRootBlock = RenderSVGText::locateRenderSVGTextAncestor(renderer());
                ASSERT(textRootBlock);

                AffineTransform gradientSpaceTransform;
                if (paintingResourceMode().contains(RenderSVGResourceMode::ApplyToFill)) {
                    gradient = usedContext->fillGradient();
                    gradientSpaceTransform = usedContext->fillGradientSpaceTransform();
                    pattern = usedContext->fillPattern();
                } else {
                    gradient = usedContext->strokeGradient();
                    gradientSpaceTransform = usedContext->strokeGradientSpaceTransform();
                    pattern = usedContext->strokePattern();
                }

                ASSERT(!(gradient && pattern));
                if (gradient || pattern) {
                    if (gradient)
                        usedContext->fillRect(textRootBlock->repaintRectInLocalCoordinates(), *gradient, gradientSpaceTransform);
                    else {
                        // FIXME: should be fillRect(const FloatRect&, Pattern&) on GraphicsContext.
                        GraphicsContextStateSaver stateSaver(*usedContext);
                        if (usedContext->strokePattern())
                            usedContext->setFillPattern(*usedContext->strokePattern());
                        usedContext->fillRect(textRootBlock->repaintRectInLocalCoordinates());
                    }
                    usedContext->setCompositeOperation(CompositeOperator::DestinationIn);
                    usedContext->beginTransparencyLayer(1);
                }
            }

            ShadowApplier shadowApplier(style, *usedContext, shadow, nullptr, shadowRect);

            if (!shadowApplier.didSaveContext())
                usedContext->save();

            usedContext->scale(1 / scalingFactor);
            scaledFont.drawText(*usedContext, textRun, textOrigin + shadowApplier.extraOffset(), startPosition, endPosition);

            if (!shadowApplier.didSaveContext())
                usedContext->restore();

            if (gradient || pattern)
                usedContext->endTransparencyLayer();
        }

        restoreGraphicsContext();

        if (!shadow)
            break;

        shadow = shadow->next();
    } while (shadow);
}

void SVGInlineTextBox::paintText(GraphicsContext& context, const RenderStyle& style, const RenderStyle& selectionStyle, const SVGTextFragment& fragment, bool hasSelection, bool paintSelectedTextOnly)
{
    unsigned startPosition = 0;
    unsigned endPosition = 0;
    if (hasSelection) {
        std::tie(startPosition, endPosition) = selectionStartEnd();
        hasSelection = mapStartEndPositionsIntoFragmentCoordinates(fragment, startPosition, endPosition);
    }

    // Fast path if there is no selection, just draw the whole chunk part using the regular style
    TextRun textRun = constructTextRun(style, fragment);
    if (!hasSelection || startPosition >= endPosition) {
        paintTextWithShadows(context, style, textRun, fragment, 0, fragment.length);
        return;
    }

    // Eventually draw text using regular style until the start position of the selection
    if (startPosition > 0 && !paintSelectedTextOnly)
        paintTextWithShadows(context, style, textRun, fragment, 0, startPosition);

    // Draw text using selection style from the start to the end position of the selection
    {
        SVGResourcesCache::SetStyleForScope temporaryStyleChange(parent()->renderer(), style, selectionStyle);
        paintTextWithShadows(context, selectionStyle, textRun, fragment, startPosition, endPosition);
    }

    // Eventually draw text using regular style from the end position of the selection to the end of the current chunk part
    if (endPosition < fragment.length && !paintSelectedTextOnly)
        paintTextWithShadows(context, style, textRun, fragment, endPosition, fragment.length);
}

FloatRect SVGInlineTextBox::calculateBoundaries() const
{
    FloatRect textRect;

    float scalingFactor = renderer().scalingFactor();
    ASSERT(scalingFactor);

    float baseline = renderer().scaledFont().metricsOfPrimaryFont().ascent() / scalingFactor;

    AffineTransform fragmentTransform;
    unsigned textFragmentsSize = m_textFragments.size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        const SVGTextFragment& fragment = m_textFragments.at(i);
        FloatRect fragmentRect(fragment.x, fragment.y - baseline, fragment.width, fragment.height);
        fragment.buildFragmentTransform(fragmentTransform);
        if (!fragmentTransform.isIdentity())
            fragmentRect = fragmentTransform.mapRect(fragmentRect);

        textRect.unite(fragmentRect);
    }

    return textRect;
}

bool SVGInlineTextBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit, LayoutUnit, HitTestAction)
{
    // FIXME: integrate with LegacyInlineTextBox::nodeAtPoint better.
    ASSERT(!isLineBreak());

    PointerEventsHitRules hitRules(PointerEventsHitRules::HitTestingTargetType::SVGText, request, renderer().usedPointerEvents());
    if (isVisibleToHitTesting(renderer().style(), request) || !hitRules.requireVisible) {
        if ((hitRules.canHitStroke && (renderer().style().svgStyle().hasStroke() || !hitRules.requireStroke))
            || (hitRules.canHitFill && (renderer().style().svgStyle().hasFill() || !hitRules.requireFill))) {
            FloatPoint boxOrigin(x(), y());
            boxOrigin.moveBy(accumulatedOffset);
            FloatRect rect(boxOrigin, size());
            if (locationInContainer.intersects(rect)) {

                float scalingFactor = renderer().scalingFactor();
                ASSERT(scalingFactor);
                
                float baseline = renderer().scaledFont().metricsOfPrimaryFont().ascent() / scalingFactor;

                AffineTransform fragmentTransform;
                for (auto& fragment : m_textFragments) {
                    FloatQuad fragmentQuad(FloatRect(fragment.x, fragment.y - baseline, fragment.width, fragment.height));
                    fragment.buildFragmentTransform(fragmentTransform);
                    if (!fragmentTransform.isIdentity())
                        fragmentQuad = fragmentTransform.mapQuad(fragmentQuad);
                    
                    if (fragmentQuad.containsPoint(locationInContainer.point())) {
                        renderer().updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
                        if (result.addNodeToListBasedTestResult(renderer().protectedNodeForHitTest().get(), request, locationInContainer, rect) == HitTestProgress::Stop)
                            return true;
                    }
                }
             }
        }
    }
    return false;
}

} // namespace WebCore
