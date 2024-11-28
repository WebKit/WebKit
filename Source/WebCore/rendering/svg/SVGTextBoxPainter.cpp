/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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
#include "SVGTextBoxPainter.h"

#include "GraphicsContext.h"
#include "GraphicsContextStateSaver.h"
#include "LegacyRenderSVGResourceSolidColor.h"
#include "RenderInline.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGText.h"
#include "SVGInlineTextBox.h"
#include "SVGPaintServerHandling.h"
#include "SVGRenderStyle.h"
#include "SVGResourcesCache.h"
#include "SVGTextFragment.h"
#include "TextPainter.h"

namespace WebCore {

LegacySVGTextBoxPainter::LegacySVGTextBoxPainter(const SVGInlineTextBox& textBox, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
    : SVGTextBoxPainter(InlineIterator::BoxLegacyPath { &textBox }, paintInfo, paintOffset)
{
}

template<typename TextBoxPath>
SVGTextBoxPainter<TextBoxPath>::SVGTextBoxPainter(TextBoxPath&& textBox, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
    : m_textBox(WTFMove(textBox))
    , m_renderer(downcast<RenderSVGInlineText>(m_textBox.renderer()))
    , m_paintInfo(paintInfo)
    , m_selectableRange(m_textBox.selectableRange())
    , m_paintOffset(paintOffset)
    , m_haveSelection(!m_renderer.document().printing() && m_renderer.view().selection().highlightStateForTextBox(m_renderer, m_selectableRange) != RenderObject::HighlightState::None)
{
}

template<typename TextBoxPath>
InlineIterator::SVGTextBoxIterator SVGTextBoxPainter<TextBoxPath>::textBoxIterator() const
{
    return { m_textBox };
}


template<typename TextBoxPath>
const RenderBoxModelObject& SVGTextBoxPainter<TextBoxPath>::parentRenderer() const
{
    return textBoxIterator()->parentInlineBox()->renderer();
}

template<typename TextBoxPath>
FloatRect SVGTextBoxPainter<TextBoxPath>::selectionRectForTextFragment(const SVGTextFragment& fragment, unsigned startPosition, unsigned endPosition, const RenderStyle& style) const
{
    ASSERT(startPosition < endPosition);

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

static inline bool textShouldBePainted(const RenderSVGInlineText& textRenderer)
{
    return textRenderer.scaledFont().size() >= 0.5;
}

template<typename TextBoxPath>
void SVGTextBoxPainter<TextBoxPath>::paintSelectionBackground()
{
    ASSERT(m_paintInfo.shouldPaintWithinRoot(renderer()));
    ASSERT(m_paintInfo.phase == PaintPhase::Foreground || m_paintInfo.phase == PaintPhase::Selection);

    if (renderer().style().usedVisibility() != Visibility::Visible)
        return;

    auto& parentRenderer = this->parentRenderer();
    ASSERT(!parentRenderer.document().printing());

    // Determine whether or not we're selected.
    bool paintSelectedTextOnly = m_paintInfo.phase == PaintPhase::Selection;
    if (!m_haveSelection || paintSelectedTextOnly)
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
    for (auto& fragment : textBoxIterator()->textFragments()) {
        ASSERT(!m_legacyPaintingResource);

        fragmentStartPosition = startPosition;
        fragmentEndPosition = endPosition;
        if (!mapStartEndPositionsIntoFragmentCoordinates(fragment, fragmentStartPosition, fragmentEndPosition))
            continue;

        GraphicsContextStateSaver stateSaver(m_paintInfo.context());
        fragment.buildFragmentTransform(fragmentTransform);
        if (!fragmentTransform.isIdentity())
            m_paintInfo.context().concatCTM(fragmentTransform);

        m_paintInfo.context().setFillColor(backgroundColor);
        m_paintInfo.context().fillRect(selectionRectForTextFragment(fragment, fragmentStartPosition, fragmentEndPosition, style), backgroundColor);

        m_paintingResourceMode = { };
    }

    ASSERT(!m_legacyPaintingResource);
}

template<typename TextBoxPath>
void SVGTextBoxPainter<TextBoxPath>::paint()
{
    ASSERT(m_paintInfo.shouldPaintWithinRoot(renderer()));
    ASSERT(m_paintInfo.phase == PaintPhase::Foreground || m_paintInfo.phase == PaintPhase::Selection);

    if (renderer().style().usedVisibility() != Visibility::Visible)
        return;

    // Note: We're explicitly not supporting composition & custom underlines and custom highlighters - unlike LegacyInlineTextBox.
    // If we ever need that for SVG, it's very easy to refactor and reuse the code.

    auto& parentRenderer = this->parentRenderer();

    bool paintSelectedTextOnly = m_paintInfo.phase == PaintPhase::Selection;
    bool shouldPaintSelectionHighlight = !(m_paintInfo.paintBehavior.contains(PaintBehavior::SkipSelectionHighlight));
    bool hasSelection = !parentRenderer.document().printing() && m_haveSelection;
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
    for (auto& fragment : textBoxIterator()->textFragments()) {
        ASSERT(!m_legacyPaintingResource);

        GraphicsContextStateSaver stateSaver(m_paintInfo.context());
        fragment.buildFragmentTransform(fragmentTransform);
        if (!fragmentTransform.isIdentity())
            m_paintInfo.context().concatCTM(fragmentTransform);

        // Spec: All text decorations except line-through should be drawn before the text is filled and stroked; thus, the text is rendered on top of these decorations.
        auto decorations = style.textDecorationsInEffect();
        if (decorations & TextDecorationLine::Underline)
            paintDecoration(TextDecorationLine::Underline, fragment);
        if (decorations & TextDecorationLine::Overline)
            paintDecoration(TextDecorationLine::Overline, fragment);

        for (auto type : RenderStyle::paintTypesForPaintOrder(style.paintOrder())) {
            switch (type) {
            case PaintType::Fill:
                if (!hasFill)
                    continue;
                m_paintingResourceMode = { RenderSVGResourceMode::ApplyToFill, RenderSVGResourceMode::ApplyToText };
                ASSERT(selectionStyle);
                paintText(style, *selectionStyle, fragment, hasSelection, paintSelectedTextOnly);
                break;
            case PaintType::Stroke:
                if (!hasVisibleStroke)
                    continue;
                m_paintingResourceMode = { RenderSVGResourceMode::ApplyToStroke, RenderSVGResourceMode::ApplyToText };
                ASSERT(selectionStyle);
                paintText(style, *selectionStyle, fragment, hasSelection, paintSelectedTextOnly);
                break;
            case PaintType::Markers:
                continue;
            }
        }

        // Spec: Line-through should be drawn after the text is filled and stroked; thus, the line-through is rendered on top of the text.
        if (decorations & TextDecorationLine::LineThrough)
            paintDecoration(TextDecorationLine::LineThrough, fragment);

        m_paintingResourceMode = { };
    }

    // Finally, paint the outline if any.
    if (renderer().style().hasOutline()) {
        if (CheckedPtr renderInline = dynamicDowncast<RenderInline>(parentRenderer))
            renderInline->paintOutline(m_paintInfo, m_paintOffset);
    }

    ASSERT(!m_legacyPaintingResource);
}

template<typename TextBoxPath>
bool SVGTextBoxPainter<TextBoxPath>::acquirePaintingResource(SVGPaintServerHandling& paintServerHandling, float scalingFactor, const RenderBoxModelObject& renderer, const RenderStyle& style)
{
    ASSERT(scalingFactor);
    ASSERT(!paintingResourceMode().isEmpty());
    ASSERT(renderer.document().settings().layerBasedSVGEngineEnabled());

    auto& context = paintServerHandling.context();
    if (paintingResourceMode().contains(RenderSVGResourceMode::ApplyToFill)) {
        if (!paintServerHandling.preparePaintOperation<SVGPaintServerHandling::Operation::Fill>(parentRenderer(), style))
            return false;
        context.save();
        context.setTextDrawingMode(TextDrawingMode::Fill);
    } else if (paintingResourceMode().contains(RenderSVGResourceMode::ApplyToStroke)) {
        if (!paintServerHandling.preparePaintOperation<SVGPaintServerHandling::Operation::Stroke>(parentRenderer(), style))
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

template<typename TextBoxPath>
void SVGTextBoxPainter<TextBoxPath>::releasePaintingResource(SVGPaintServerHandling& paintServerHandling)
{
    auto& context = paintServerHandling.context();

    if (context.fillGradient() || context.strokeGradient() || context.fillPattern() || context.strokePattern())
        context.endTransparencyLayer();

    context.restore();
}

template<typename TextBoxPath>
bool SVGTextBoxPainter<TextBoxPath>::acquireLegacyPaintingResource(GraphicsContext*& context, float scalingFactor, RenderBoxModelObject& renderer, const RenderStyle& style)
{
    ASSERT(scalingFactor);
    ASSERT(paintingResourceMode().containsAny({ RenderSVGResourceMode::ApplyToFill, RenderSVGResourceMode::ApplyToStroke }));

    Color fallbackColor;
    if (paintingResourceMode().contains(RenderSVGResourceMode::ApplyToFill))
        m_legacyPaintingResource = LegacyRenderSVGResource::fillPaintingResource(renderer, style, fallbackColor);
    else if (paintingResourceMode().contains(RenderSVGResourceMode::ApplyToStroke))
        m_legacyPaintingResource = LegacyRenderSVGResource::strokePaintingResource(renderer, style, fallbackColor);

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

template<typename TextBoxPath>
void SVGTextBoxPainter<TextBoxPath>::releaseLegacyPaintingResource(GraphicsContext*& context, const Path* path)
{
    ASSERT(m_legacyPaintingResource);

    m_legacyPaintingResource->postApplyResource(const_cast<RenderBoxModelObject&>(parentRenderer()), context, paintingResourceMode(), path, /*LegacyRenderSVGShape*/ nullptr);
    m_legacyPaintingResource = nullptr;
}

template<typename TextBoxPath>
bool SVGTextBoxPainter<TextBoxPath>::mapStartEndPositionsIntoFragmentCoordinates(const SVGTextFragment& fragment, unsigned& startPosition, unsigned& endPosition) const
{
    unsigned startFragment = fragment.characterOffset - m_textBox.start();
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

template<typename TextBoxPath>
void SVGTextBoxPainter<TextBoxPath>::paintDecoration(OptionSet<TextDecorationLine> decoration, const SVGTextFragment& fragment)
{
    if (renderer().style().textDecorationsInEffect().isEmpty())
        return;

    // Find out which render style defined the text-decoration, as its fill/stroke properties have to be used for drawing instead of ours.

    auto decorationRenderer = [&]  {
        auto parentBox = textBoxIterator()->parentInlineBox();

        // Lookup first render object in parent hierarchy which has text-decoration set.
        CheckedPtr<const RenderBoxModelObject> renderer = nullptr;
        while (parentBox) {
            renderer = &parentBox->renderer();

            if (!renderer->style().textDecorationLine().isEmpty())
                break;

            parentBox = parentBox->parentInlineBox();
        }

        return renderer;
    }();

    ASSERT(decorationRenderer);

    const RenderStyle& decorationStyle = decorationRenderer->style();

    if (decorationStyle.usedVisibility() == Visibility::Hidden)
        return;

    const SVGRenderStyle& svgDecorationStyle = decorationStyle.svgStyle();

    for (auto type : RenderStyle::paintTypesForPaintOrder(renderer().style().paintOrder())) {
        switch (type) {
        case PaintType::Fill:
            if (svgDecorationStyle.hasFill()) {
                m_paintingResourceMode = RenderSVGResourceMode::ApplyToFill;
                paintDecorationWithStyle(decoration, fragment, *decorationRenderer);
            }
            break;
        case PaintType::Stroke:
            if (decorationStyle.hasVisibleStroke()) {
                m_paintingResourceMode = RenderSVGResourceMode::ApplyToStroke;
                paintDecorationWithStyle(decoration, fragment, *decorationRenderer);
            }
            break;
        case PaintType::Markers:
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }
}

template<typename TextBoxPath>
void SVGTextBoxPainter<TextBoxPath>::paintDecorationWithStyle(OptionSet<TextDecorationLine> decoration, const SVGTextFragment& fragment, const RenderBoxModelObject& decorationRenderer)
{
    ASSERT(!m_legacyPaintingResource);
    ASSERT(!paintingResourceMode().isEmpty());

    auto& context = m_paintInfo.context();
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
    if (acquireLegacyPaintingResource(usedContext, scalingFactor, const_cast<RenderBoxModelObject&>(decorationRenderer), decorationStyle))
        releaseLegacyPaintingResource(usedContext, &path);
}

template<typename TextBoxPath>
void SVGTextBoxPainter<TextBoxPath>::paintTextWithShadows(const RenderStyle& style, TextRun& textRun, const SVGTextFragment& fragment, unsigned startPosition, unsigned endPosition)
{
    auto& context = m_paintInfo.context();

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
            return acquirePaintingResource(paintServerHandling, scalingFactor, parentRenderer(), style);
        return acquireLegacyPaintingResource(usedContext, scalingFactor, const_cast<RenderBoxModelObject&>(parentRenderer()), style);
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

template<typename TextBoxPath>
void SVGTextBoxPainter<TextBoxPath>::paintText(const RenderStyle& style, const RenderStyle& selectionStyle, const SVGTextFragment& fragment, bool hasSelection, bool paintSelectedTextOnly)
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
        paintTextWithShadows(style, textRun, fragment, 0, fragment.length);
        return;
    }

    // Eventually draw text using regular style until the start position of the selection
    if (startPosition > 0 && !paintSelectedTextOnly)
        paintTextWithShadows(style, textRun, fragment, 0, startPosition);

    // Draw text using selection style from the start to the end position of the selection
    {
        SVGResourcesCache::SetStyleForScope temporaryStyleChange(const_cast<RenderBoxModelObject&>(parentRenderer()), style, selectionStyle);
        paintTextWithShadows(selectionStyle, textRun, fragment, startPosition, endPosition);
    }

    // Eventually draw text using regular style from the end position of the selection to the end of the current chunk part
    if (endPosition < fragment.length && !paintSelectedTextOnly)
        paintTextWithShadows(style, textRun, fragment, endPosition, fragment.length);
}

template<typename TextBoxPath>
TextRun SVGTextBoxPainter<TextBoxPath>::constructTextRun(const RenderStyle& style, const SVGTextFragment& fragment) const
{
    TextRun run(StringView(renderer().text()).substring(fragment.characterOffset, fragment.length),
        0, /* xPos, only relevant with allowTabs=true */
        0, /* padding, only relevant for justified text, not relevant for SVG */
        ExpansionBehavior::allowRightOnly(),
        m_textBox.direction(),
        style.rtlOrdering() == Order::Visual /* directionalOverride */);

    // We handle letter & word spacing ourselves.
    run.disableSpacing();
    return run;
}

template<typename TextBoxPath>
std::pair<unsigned, unsigned> SVGTextBoxPainter<TextBoxPath>::selectionStartEnd() const
{
    return m_renderer.view().selection().rangeForTextBox(m_renderer, m_selectableRange);
}

template class SVGTextBoxPainter<InlineIterator::BoxModernPath>;
template class SVGTextBoxPainter<InlineIterator::BoxLegacyPath>;

}
