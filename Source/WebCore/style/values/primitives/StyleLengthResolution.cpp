/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleLengthResolution.h"

#include "BoxSides.h"
#include "CSSPrimitiveNumericUnits.h"
#include "CSSToLengthConversionData.h"
#include "ContainerNodeInlines.h"
#include "ContainerQueryEvaluator.h"
#include "Document.h"
#include "Element.h"
#include "FontCascadeInlines.h"
#include "FontCascadeDescription.h"
#include "FontMetrics.h"
#include "LocalFrame.h"
#include "NodeRenderStyle.h"
#include "RenderBox.h"
#include "RenderBoxInlines.h"
#include "RenderView.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleLineHeight.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

// MARK: - Font Metric Zoom

// Raw font metrics include the usedZoomFactor and must be normalized.
static double LEGACY_NODELETE unzoomFontMetric(double metric, const FontDescription& fontDescription)
{
    if (auto usedZoomFactor = fontDescription.usedZoomFactor(); usedZoomFactor > 0)
        return metric / usedZoomFactor;
    return metric;
}

// MARK: - "font dependent" and "root font dependent" resolution functions

// Resolve the "em", "rem" and "quirky-em" units.
// https://drafts.csswg.org/css-values-4/#em
// https://drafts.csswg.org/css-values-4/#rem
static double LEGACY_NODELETE resolveEm(const FontCascade& fontCascadeForUnit)
{
    auto& fontDescription = fontCascadeForUnit.fontDescription();
    return fontDescription.computedSize();
}

// Resolve the "ex" and "rex" units.
// https://drafts.csswg.org/css-values-4/#ex
// https://drafts.csswg.org/css-values-4/#rex
static double resolveEx(const FontCascade& fontCascadeForUnit)
{
    auto& fontDescription = fontCascadeForUnit.fontDescription();
    auto& fontMetrics = fontCascadeForUnit.metricsOfPrimaryFont();
    if (fontMetrics.xHeight())
        return unzoomFontMetric(fontMetrics.xHeight().value(), fontDescription);

    return fontDescription.computedSize() / 2.0;
}

// Resolve the "cap" and "rcap" units.
// https://drafts.csswg.org/css-values-4/#cap
// https://drafts.csswg.org/css-values-4/#rcap
static double resolveCap(const FontCascade& fontCascadeForUnit)
{
    auto& fontDescription = fontCascadeForUnit.fontDescription();
    auto& fontMetrics = fontCascadeForUnit.metricsOfPrimaryFont();
    if (fontMetrics.capHeight())
        return unzoomFontMetric(fontMetrics.capHeight().value(), fontDescription);
    return unzoomFontMetric(fontMetrics.intAscent(), fontDescription);
}

// Resolve the "ch" and "rch" units.
// https://drafts.csswg.org/css-values-4/#ch
// https://drafts.csswg.org/css-values-4/#rch
static double resolveCh(const FontCascade& fontCascadeForUnit)
{
    return unzoomFontMetric(fontCascadeForUnit.zeroWidth(), fontCascadeForUnit.fontDescription());
}

// Resolve the "ic" and "ric" units.
// https://drafts.csswg.org/css-values-4/#ic
// https://drafts.csswg.org/css-values-4/#ric
static double resolveIc(const FontCascade& fontCascadeForUnit)
{
    auto& fontDescription = fontCascadeForUnit.fontDescription();
    auto ideogramWidth = fontCascadeForUnit.metricsOfPrimaryFont().ideogramWidth();
    if (!ideogramWidth)
        return fontDescription.unzoomedUsedSize();
    return unzoomFontMetric(ideogramWidth.value(), fontDescription);
}

// Resolve the "lh" and "rlh" units.
// https://drafts.csswg.org/css-values-4/#lh
// https://drafts.csswg.org/css-values-4/#rlh
static double resolveLh(const ComputedStyle* style, const FontCascade& fallbackFontCascadeForUnit, const auto& adaptor)
{
    if (style) {
        auto& fontCascade = style->fontCascade();
        auto& fontDescription = fontCascade.fontDescription();

        auto lineHeight = evaluate<float>(
            style->lineHeight(),
            LineHeightEvaluationContext {
                fontDescription.computedSize(),
                static_cast<float>(unzoomFontMetric(fontCascade.metricsOfPrimaryFont().lineSpacing(), fontDescription)),
            },
            ZoomFactor::none()
        );

        // A <length-percentage> line-height had text zoom applied when it was resolved (see CSSValueConversion<LineHeight>),
        // but, like the other font-relative units, we need to return the unzoomed value.
        if (style->lineHeight().holdsAlternative<LineHeight::Length>())
            lineHeight /= adaptor.textZoomFactor(*style);

        return lineHeight;
    }

    return unzoomFontMetric(fallbackFontCascadeForUnit.metricsOfPrimaryFont().lineSpacing(), fallbackFontCascadeForUnit.fontDescription());
}

// MARK: - "viewport-percentage" resolution functions

enum class ViewportType : uint8_t {
    Default,
    Small,
    Large,
    Dynamic,
};

enum class ViewportDimension : uint8_t {
    Height,
    Width,
    Max,
    Min,
    Inline,
    Block,
};

template<LogicalBoxAxis axis>
static double LEGACY_NODELETE resolveViewportLogicalAxis(const FloatSize& size, const ComputedStyle& style)
{
    switch (mapAxisLogicalToPhysical(style.writingMode(), axis)) {
    case BoxAxis::Horizontal:
        return size.width();
    case BoxAxis::Vertical:
        return size.height();
    }

    RELEASE_ASSERT_NOT_REACHED();
}

template<LogicalBoxAxis axis>
static double LEGACY_NODELETE resolveViewportLogicalAxis(const FloatSize& size, const RenderView& renderView, const auto& adaptor)
{
    if (auto* style = adaptor.styleForViewportUnits())
        return resolveViewportLogicalAxis<axis>(size, *style);

    auto* rootElement = renderView.document().documentElement();
    if (!rootElement)
        return 0;
    auto* rootStyle = rootElement->renderStyle();
    if (!rootStyle)
        return 0;
    return resolveViewportLogicalAxis<axis>(size, *rootStyle);
}

template<ViewportDimension axis>
static double LEGACY_NODELETE resolveViewportAxis(const FloatSize& size, const RenderView& renderView, const auto& adaptor)
{
    if constexpr (axis == ViewportDimension::Height)
        return size.height();
    else if constexpr (axis == ViewportDimension::Width)
        return size.width();
    else if constexpr (axis == ViewportDimension::Max)
        return size.maxDimension();
    else if constexpr (axis == ViewportDimension::Min)
        return size.minDimension();
    else if constexpr (axis == ViewportDimension::Inline)
        return resolveViewportLogicalAxis<LogicalBoxAxis::Inline>(size, renderView, adaptor);
    else if constexpr (axis == ViewportDimension::Block)
        return resolveViewportLogicalAxis<LogicalBoxAxis::Block>(size, renderView, adaptor);
}

template<ViewportType type>
static FloatSize resolveViewportType(const RenderView& renderView)
{
    if constexpr (type == ViewportType::Default)
        return renderView.sizeForCSSDefaultViewportUnits();
    else if constexpr (type == ViewportType::Small)
        return renderView.sizeForCSSSmallViewportUnits();
    else if constexpr (type == ViewportType::Large)
        return renderView.sizeForCSSLargeViewportUnits();
    else if constexpr (type == ViewportType::Dynamic)
        return renderView.sizeForCSSDynamicViewportUnits();
}


template<ViewportType type, ViewportDimension axis>
static double resolveViewportUnit(const auto& adaptor)
{
    adaptor.setUsesViewportUnits();

    auto* renderView = adaptor.renderViewForViewportUnits();
    if (!renderView) {
        if constexpr (axis == ViewportDimension::Max || axis == ViewportDimension::Min)
            return 1;
        else
            return 0;
    }

    return resolveViewportAxis<axis>(resolveViewportType<type>(*renderView), *renderView, adaptor) / 100.0 / renderView->pageZoomFactor();
}

// MARK: - "container-percentage" resolution functions

static std::optional<double> resolveContainerUnit(CQ::Axis physicalAxis, const auto& adaptor)
{
    ASSERT(physicalAxis == CQ::Axis::Width || physicalAxis == CQ::Axis::Height);

    adaptor.setUsesContainerUnits();

    RefPtr element = adaptor.elementForContainerUnits();
    if (!element)
        return { };

    auto mode = !adaptor.styleForContainerUnits()->pseudoElementType()
        ? ContainerQueryEvaluator::SelectionMode::Element
        : ContainerQueryEvaluator::SelectionMode::PseudoElement;

    // "The query container for each axis is the nearest ancestor container that accepts container size queries on that axis."
    while ((element = ContainerQueryEvaluator::selectContainer(CQ::ContainerRequirements { physicalAxis }, nullString(), *element, mode))) {
        auto* containerRenderer = dynamicDowncast<RenderBox>(element->renderer());
        if (containerRenderer && containerRenderer->hasEligibleContainmentForSizeQuery()) {
            auto widthOrHeight = physicalAxis == CQ::Axis::Width ? containerRenderer->contentBoxWidth() : containerRenderer->contentBoxHeight();
            auto adjustedWidthOrHeight = widthOrHeight.toDouble();

            // FIXME: Document why `font-size` is excluded or remove this special case.
            if (adaptor.property() != CSSPropertyID::FontSize)
                adjustedWidthOrHeight = adjustedWidthOrHeight / adaptor.renderViewForViewportUnits()->pageZoomFactor();

            return adjustedWidthOrHeight / 100;
        }
        // For pseudo-elements the element itself can be the container. Avoid looping forever.
        mode = ContainerQueryEvaluator::SelectionMode::Element;
    }
    return { };
}

// MARK: - Length Resolution

struct CSSToLengthConversionDataAdaptor {
    const CSSToLengthConversionData& conversionData;

    void LEGACY_NODELETE setUsesViewportUnits() const
    {
        if (CheckedPtr builderState = conversionData.styleBuilderState())
            builderState->setUsesViewportUnits();
    }

    void LEGACY_NODELETE setUsesContainerUnits() const
    {
        if (CheckedPtr builderState = conversionData.styleBuilderState())
            builderState->setIsContainerDependent();
    }

    CSSPropertyID LEGACY_NODELETE property() const
    {
        return conversionData.property();
    }

    bool LEGACY_NODELETE computingFontProperty() const
    {
        // FIXME: This should be generated but first we need to determine if using `font-*` so literally is the right choice. It might make more sense to have this be all properties that effect font-relative units, so would need to include things like `math-depth`.
        // See https://github.com/w3c/csswg-drafts/issues/14306.
        switch (conversionData.property()) {
        case CSSPropertyID::FontFeatureSettings:
        case CSSPropertyID::FontKerning:
        case CSSPropertyID::FontPalette:
        case CSSPropertyID::FontSize:
        case CSSPropertyID::FontSizeAdjust:
        case CSSPropertyID::FontStyle:
        case CSSPropertyID::FontSynthesisSmallCaps:
        case CSSPropertyID::FontSynthesisStyle:
        case CSSPropertyID::FontSynthesisWeight:
        case CSSPropertyID::FontVariantAlternates:
        case CSSPropertyID::FontVariantCaps:
        case CSSPropertyID::FontVariantEastAsian:
        case CSSPropertyID::FontVariantEmoji:
        case CSSPropertyID::FontVariantLigatures:
        case CSSPropertyID::FontVariantNumeric:
        case CSSPropertyID::FontVariantPosition:
        case CSSPropertyID::FontWeight:
        case CSSPropertyID::FontWidth:
#if ENABLE(VARIATION_FONTS)
        case CSSPropertyID::FontOpticalSizing:
        case CSSPropertyID::FontVariationSettings:
#endif
            return true;
        default:
            return false;
        }
    }

    bool LEGACY_NODELETE computingLineHeightProperty() const
    {
        return conversionData.property() == CSSPropertyID::LineHeight;
    }

    double LEGACY_NODELETE applyTextZoom(double value) const
    {
        // The computed value of `font-size` excludes text zoom; it is applied when computing the used font size (see usedFontSizeFromComputedSize()).
        if (conversionData.property() == CSSPropertyID::FontSize)
            return value;

        if (CheckedPtr builderState = conversionData.styleBuilderState())
            return value * builderState->zoomWithTextZoomFactor();
        return value;
    }

    float textZoomFactor(const ComputedStyle& style) const
    {
        if (style.textZoom() == TextZoom::Reset)
            return 1.0f;

        RefPtr frame = [&] -> LocalFrame* {
            if (CheckedPtr builderState = conversionData.styleBuilderState())
                return builderState->document().frame();
            if (auto* renderView = conversionData.renderView())
                return renderView->document().frame();
            return nullptr;
        }();
        return frame ? frame->textZoomFactor() : 1.0f;
    }

    const FontCascade& LEGACY_NODELETE fontCascadeForFontUnits() const
    {
        if (computingFontProperty()) {
            ASSERT(conversionData.parentStyle());
            return conversionData.parentStyle()->fontCascade();
        }

        return conversionData.style().fontCascade();
    }

    const FontCascade& LEGACY_NODELETE fontCascadeForRootFontUnits() const
    {
        if (conversionData.rootStyle())
            return conversionData.rootStyle()->fontCascade();

        // FIXME: This should fallback to the initial style, not the element style. To fix this, we will need to make the initial style (Document::initialStyle()) available to CSSToLengthConversionData.
        return fontCascadeForFontUnits();
    }

    const ComputedStyle* LEGACY_NODELETE styleForLineHeightUnits() const
    {
        if (computingLineHeightProperty() || computingFontProperty())
            return conversionData.parentStyle();

        // FIXME: This should fallback to the initial style, not the element style. To fix this, we will need to make the initial style (Document::initialStyle()) available to CSSToLengthConversionData.
        return &conversionData.style();
    }

    const ComputedStyle* LEGACY_NODELETE styleForRootLineHeightUnits() const
    {
        return conversionData.rootStyle();
    }

    const ComputedStyle* LEGACY_NODELETE styleForViewportUnits() const
    {
        return &conversionData.style();
    }

    const RenderView* LEGACY_NODELETE renderViewForViewportUnits() const
    {
        return conversionData.renderView();
    }

    const ComputedStyle* LEGACY_NODELETE styleForContainerUnits() const
    {
        return &conversionData.style();
    }

    const Element* LEGACY_NODELETE elementForContainerUnits() const
    {
        return conversionData.elementForContainerUnitResolution();
    }

    bool LEGACY_NODELETE isHorizontalForContainerUnits() const
    {
        return conversionData.style().writingMode().isHorizontal();
    }
};

struct DirectDataAdaptor {
    CSSPropertyID propertyToCompute;
    const FontCascade& fontCascadeForUnit;
    const RenderView* renderViewForUnit;

    void LEGACY_NODELETE setUsesViewportUnits() const
    {
    }

    void LEGACY_NODELETE setUsesContainerUnits() const
    {
    }

    CSSPropertyID LEGACY_NODELETE property() const
    {
        return propertyToCompute;
    }

    double LEGACY_NODELETE applyTextZoom(double value) const
    {
        // FIXME: Add support for text zoom for direct data overload.
        return value;
    }

    float textZoomFactor(const ComputedStyle&) const
    {
        return 1.0f;
    }

    const FontCascade& LEGACY_NODELETE fontCascadeForFontUnits() const
    {
        return fontCascadeForUnit;
    }

    const FontCascade& LEGACY_NODELETE fontCascadeForRootFontUnits() const
    {
        return fontCascadeForUnit;
    }

    const ComputedStyle* LEGACY_NODELETE styleForLineHeightUnits() const
    {
        return nullptr;
    }

    const ComputedStyle* LEGACY_NODELETE styleForRootLineHeightUnits() const
    {
        return nullptr;
    }

    const ComputedStyle* LEGACY_NODELETE styleForViewportUnits() const
    {
        return nullptr;
    }

    const RenderView* LEGACY_NODELETE renderViewForViewportUnits() const
    {
        return renderViewForUnit;
    }

    const ComputedStyle* LEGACY_NODELETE styleForContainerUnits() const
    {
        return nullptr;
    }

    const Element* LEGACY_NODELETE elementForContainerUnits() const
    {
        return nullptr;
    }

    bool LEGACY_NODELETE isHorizontalForContainerUnits() const
    {
        return true;
    }
};

static double resolveLengthWithoutTextZoom(double value, CSS::LengthUnit lengthUnit, const auto& adaptor)
{
    using enum CSS::LengthUnit;

    switch (lengthUnit) {
    case Px:
        return value;
    case Cm:
        return value * CSS::pixelsPerCm;
    case Mm:
        return value * CSS::pixelsPerMm;
    case Q:
        return value * CSS::pixelsPerQ;
    case In:
        return value * CSS::pixelsPerInch;
    case Pt:
        return value * CSS::pixelsPerPt;
    case Pc:
        return value * CSS::pixelsPerPc;

    // MARK: "font dependent" resolution

    case Em:
    case QuirkyEm:
        return value * resolveEm(adaptor.fontCascadeForFontUnits());
    case Ex:
        return value * resolveEx(adaptor.fontCascadeForFontUnits());
    case Cap:
        return value * resolveCap(adaptor.fontCascadeForFontUnits());

    case Ch:
        return value * resolveCh(adaptor.fontCascadeForFontUnits());
    case Ic:
        return value * resolveIc(adaptor.fontCascadeForFontUnits());

    case Lh:
        return value * resolveLh(adaptor.styleForLineHeightUnits(), adaptor.fontCascadeForFontUnits(), adaptor);

    // MARK: "root font dependent" resolution

    case Rem:
        return value * resolveEm(adaptor.fontCascadeForRootFontUnits());
    case Rcap:
        return value * resolveCap(adaptor.fontCascadeForRootFontUnits());
    case Rch:
        return value * resolveCh(adaptor.fontCascadeForRootFontUnits());
    case Rex:
        return value * resolveEx(adaptor.fontCascadeForRootFontUnits());
    case Ric:
        return value * resolveIc(adaptor.fontCascadeForRootFontUnits());

    case Rlh:
        return value * resolveLh(adaptor.styleForRootLineHeightUnits(), adaptor.fontCascadeForRootFontUnits(), adaptor);

    // MARK: "viewport-percentage" resolution

    case Vh:
        return value * resolveViewportUnit<ViewportType::Default, ViewportDimension::Height>(adaptor);
    case Vw:
        return value * resolveViewportUnit<ViewportType::Default, ViewportDimension::Width>(adaptor);
    case Vmax:
        return value * resolveViewportUnit<ViewportType::Default, ViewportDimension::Max>(adaptor);
    case Vmin:
        return value * resolveViewportUnit<ViewportType::Default, ViewportDimension::Min>(adaptor);
    case Vb:
        return value * resolveViewportUnit<ViewportType::Default, ViewportDimension::Block>(adaptor);
    case Vi:
        return value * resolveViewportUnit<ViewportType::Default, ViewportDimension::Inline>(adaptor);

    case Svh:
        return value * resolveViewportUnit<ViewportType::Small, ViewportDimension::Height>(adaptor);
    case Svw:
        return value * resolveViewportUnit<ViewportType::Small, ViewportDimension::Width>(adaptor);
    case Svmax:
        return value * resolveViewportUnit<ViewportType::Small, ViewportDimension::Max>(adaptor);
    case Svmin:
        return value * resolveViewportUnit<ViewportType::Small, ViewportDimension::Min>(adaptor);
    case Svb:
        return value * resolveViewportUnit<ViewportType::Small, ViewportDimension::Block>(adaptor);
    case Svi:
        return value * resolveViewportUnit<ViewportType::Small, ViewportDimension::Inline>(adaptor);

    case Lvh:
        return value * resolveViewportUnit<ViewportType::Large, ViewportDimension::Height>(adaptor);
    case Lvw:
        return value * resolveViewportUnit<ViewportType::Large, ViewportDimension::Width>(adaptor);
    case Lvmax:
        return value * resolveViewportUnit<ViewportType::Large, ViewportDimension::Max>(adaptor);
    case Lvmin:
        return value * resolveViewportUnit<ViewportType::Large, ViewportDimension::Min>(adaptor);
    case Lvb:
        return value * resolveViewportUnit<ViewportType::Large, ViewportDimension::Block>(adaptor);
    case Lvi:
        return value * resolveViewportUnit<ViewportType::Large, ViewportDimension::Inline>(adaptor);

    case Dvh:
        return value * resolveViewportUnit<ViewportType::Dynamic, ViewportDimension::Height>(adaptor);
    case Dvw:
        return value * resolveViewportUnit<ViewportType::Dynamic, ViewportDimension::Width>(adaptor);
    case Dvmax:
        return value * resolveViewportUnit<ViewportType::Dynamic, ViewportDimension::Max>(adaptor);
    case Dvmin:
        return value * resolveViewportUnit<ViewportType::Dynamic, ViewportDimension::Min>(adaptor);
    case Dvb:
        return value * resolveViewportUnit<ViewportType::Dynamic, ViewportDimension::Block>(adaptor);
    case Dvi:
        return value * resolveViewportUnit<ViewportType::Dynamic, ViewportDimension::Inline>(adaptor);

    // MARK: "container-percentage" resolution

    case Cqw:
        if (auto resolvedValue = resolveContainerUnit(CQ::Axis::Width, adaptor))
            return value * *resolvedValue;
        return resolveLengthWithoutTextZoom(value, Svw, adaptor);

    case Cqh:
        if (auto resolvedValue = resolveContainerUnit(CQ::Axis::Height, adaptor))
            return value * *resolvedValue;
        return resolveLengthWithoutTextZoom(value, Svh, adaptor);

    case Cqi:
        if (auto resolvedValue = resolveContainerUnit(adaptor.isHorizontalForContainerUnits() ? CQ::Axis::Width : CQ::Axis::Height, adaptor))
            return value * *resolvedValue;
        return resolveLengthWithoutTextZoom(value, Svi, adaptor);

    case Cqb:
        if (auto resolvedValue = resolveContainerUnit(adaptor.isHorizontalForContainerUnits() ? CQ::Axis::Height : CQ::Axis::Width, adaptor))
            return value * *resolvedValue;
        return resolveLengthWithoutTextZoom(value, Svb, adaptor);

    case Cqmax:
        if (value < 0)
            return std::min(resolveLengthWithoutTextZoom(value, Cqb, adaptor), resolveLengthWithoutTextZoom(value, Cqi, adaptor));
        return std::max(resolveLengthWithoutTextZoom(value, Cqb, adaptor), resolveLengthWithoutTextZoom(value, Cqi, adaptor));

    case Cqmin:
        if (value < 0)
            return std::max(resolveLengthWithoutTextZoom(value, Cqb, adaptor), resolveLengthWithoutTextZoom(value, Cqi, adaptor));
        return std::min(resolveLengthWithoutTextZoom(value, Cqb, adaptor), resolveLengthWithoutTextZoom(value, Cqi, adaptor));
    }

    RELEASE_ASSERT_NOT_REACHED();
}

// Properties that size text features, so all their lengths scale with text zoom, not just font-relative ones.
static bool textZoomAppliesToAllLengths(CSSPropertyID property)
{
    switch (property) {
    case CSSPropertyID::LineHeight:
    case CSSPropertyID::LetterSpacing:
    case CSSPropertyID::WordSpacing:
    case CSSPropertyID::TextUnderlineOffset:
    case CSSPropertyID::TextDecorationThickness:
    case CSSPropertyID::TextDecorationInset:
        return true;
    default:
        return false;
    }
}

static double resolveLengthApplyingTextZoom(double value, CSS::LengthUnit lengthUnit, const auto& adaptor)
{
    auto resolvedValue = resolveLengthWithoutTextZoom(value, lengthUnit, adaptor);

    if (CSS::isFontOrRootFontRelativeLength(lengthUnit) || textZoomAppliesToAllLengths(adaptor.property()))
        return adaptor.applyTextZoom(resolvedValue);
    return resolvedValue;
}

double resolveLengthForCalc(double value, CSS::LengthUnit lengthUnit, const CSSToLengthConversionData& conversionData)
{
    auto adaptor = CSSToLengthConversionDataAdaptor {
        .conversionData = conversionData,
    };

    // calc() results are resolved again as px when converted to Style values, which applies the text zoom
    // that all lengths get for this property. Leave it out here so it isn't applied twice.
    if (textZoomAppliesToAllLengths(adaptor.property()))
        return resolveLengthWithoutTextZoom(value, lengthUnit, adaptor);
    return resolveLengthApplyingTextZoom(value, lengthUnit, adaptor);
}

double resolveLength(double value, CSS::LengthUnit lengthUnit, const BuilderState& state)
{
    return resolveLengthApplyingTextZoom(value, lengthUnit, CSSToLengthConversionDataAdaptor {
        .conversionData = state.cssToLengthConversionData(),
    });
}

double resolveLength(double value, CSS::LengthUnit lengthUnit, const CSSToLengthConversionData& conversionData)
{
    return resolveLengthApplyingTextZoom(value, lengthUnit, CSSToLengthConversionDataAdaptor {
        .conversionData = conversionData,
    });
}

double resolveLength(double value, CSS::LengthUnit lengthUnit, CSSPropertyID propertyToCompute, const FontCascade& fontCascadeForUnit, const RenderView* renderViewForUnit)
{
    return resolveLengthApplyingTextZoom(value, lengthUnit, DirectDataAdaptor {
        .propertyToCompute = propertyToCompute,
        .fontCascadeForUnit = fontCascadeForUnit,
        .renderViewForUnit = renderViewForUnit,
    });
}

double resolveLength(double value, CSS::LengthUnit lengthUnit, NoConversionDataRequiredToken)
{
    using enum CSS::LengthUnit;

    switch (lengthUnit) {
    case Px:
        return value;
    case Cm:
        return value * CSS::pixelsPerCm;
    case Mm:
        return value * CSS::pixelsPerMm;
    case Q:
        return value * CSS::pixelsPerQ;
    case In:
        return value * CSS::pixelsPerInch;
    case Pt:
        return value * CSS::pixelsPerPt;
    case Pc:
        return value * CSS::pixelsPerPc;

    case Em:
    case QuirkyEm:
    case Ex:
    case Cap:
    case Ch:
    case Ic:
    case Lh:
    case Rem:
    case Rcap:
    case Rch:
    case Rex:
    case Ric:
    case Rlh:
    case Vh:
    case Vw:
    case Vmax:
    case Vmin:
    case Vb:
    case Vi:
    case Svh:
    case Svw:
    case Svmax:
    case Svmin:
    case Svb:
    case Svi:
    case Lvh:
    case Lvw:
    case Lvmax:
    case Lvmin:
    case Lvb:
    case Lvi:
    case Dvh:
    case Dvw:
    case Dvmax:
    case Dvmin:
    case Dvb:
    case Dvi:
    case Cqw:
    case Cqh:
    case Cqi:
    case Cqb:
    case Cqmax:
    case Cqmin:
        ASSERT_NOT_REACHED();
        return -1;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

bool equalForLengthResolution(const ComputedStyle& styleA, const ComputedStyle& styleB)
{
    // These properties affect results of `resolveLength` above.

    if (styleA.fontDescription().usedSize() != styleB.fontDescription().usedSize())
        return false;
    if (styleA.fontDescription().computedSize() != styleB.fontDescription().computedSize())
        return false;
    if (styleA.metricsOfPrimaryFont().xHeight() != styleB.metricsOfPrimaryFont().xHeight())
        return false;
    if (styleA.metricsOfPrimaryFont().zeroWidth() != styleB.metricsOfPrimaryFont().zeroWidth())
        return false;
    if (styleA.metricsOfPrimaryFont().lineSpacing() != styleB.metricsOfPrimaryFont().lineSpacing())
        return false;
    if (styleA.lineHeight() != styleB.lineHeight())
        return false;
    if (styleA.zoom() != styleB.zoom())
        return false;

    return true;
}

// MARK: - em-to-px utility functions

double emToPxDouble(double value, const ComputedStyle& style)
{
    return value * style.fontCascade().fontDescription().computedSize();
}

double emToPxDoubleZoomed(double value, const ComputedStyle& style)
{
    return value * style.fontCascade().fontDescription().usedSize();
}

} // namespace Style
} // namespace WebCore
