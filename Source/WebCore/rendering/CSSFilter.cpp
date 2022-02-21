/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "CSSFilter.h"

#include "FEColorMatrix.h"
#include "FEComponentTransfer.h"
#include "FEDropShadow.h"
#include "FEGaussianBlur.h"
#include "FilterOperations.h"
#include "Logging.h"
#include "ReferencedSVGResources.h"
#include "RenderElement.h"
#include "SVGFilter.h"
#include "SVGFilterBuilder.h"
#include "SVGFilterElement.h"
#include "SourceGraphic.h"

namespace WebCore {

RefPtr<CSSFilter> CSSFilter::create(RenderElement& renderer, const FilterOperations& operations, RenderingMode renderingMode, const FloatSize& filterScale, ClipOperation clipOperation, const FloatRect& targetBoundingBox)
{
    bool hasFilterThatMovesPixels = operations.hasFilterThatMovesPixels();
    bool hasFilterThatShouldBeRestrictedBySecurityOrigin = operations.hasFilterThatShouldBeRestrictedBySecurityOrigin();

    auto filter = adoptRef(*new CSSFilter(renderingMode, filterScale, clipOperation, hasFilterThatMovesPixels, hasFilterThatShouldBeRestrictedBySecurityOrigin));

    if (!filter->buildFilterFunctions(renderer, operations, targetBoundingBox))
        return nullptr;

    if (renderingMode == RenderingMode::Accelerated && !filter->supportsAcceleratedRendering())
        filter->setRenderingMode(RenderingMode::Unaccelerated);

    return filter;
}

RefPtr<CSSFilter> CSSFilter::create(Vector<Ref<FilterFunction>>&& functions)
{
    return adoptRef(new CSSFilter(WTFMove(functions)));
}

CSSFilter::CSSFilter(RenderingMode renderingMode, const FloatSize& filterScale, ClipOperation clipOperation, bool hasFilterThatMovesPixels, bool hasFilterThatShouldBeRestrictedBySecurityOrigin)
    : Filter(Filter::Type::CSSFilter, renderingMode, filterScale, clipOperation)
    , m_hasFilterThatMovesPixels(hasFilterThatMovesPixels)
    , m_hasFilterThatShouldBeRestrictedBySecurityOrigin(hasFilterThatShouldBeRestrictedBySecurityOrigin)
{
}

CSSFilter::CSSFilter(Vector<Ref<FilterFunction>>&& functions)
    : Filter(Type::CSSFilter)
    , m_functions(WTFMove(functions))
{
}

static RefPtr<FilterEffect> createBlurEffect(const BlurFilterOperation& blurOperation, Filter::ClipOperation clipOperation)
{
    float stdDeviation = floatValueForLength(blurOperation.stdDeviation(), 0);
    return FEGaussianBlur::create(stdDeviation, stdDeviation, clipOperation == Filter::ClipOperation::Unite ? EdgeModeType::None : EdgeModeType::Duplicate);
}

static RefPtr<FilterEffect> createBrightnessEffect(const BasicComponentTransferFilterOperation& componentTransferOperation)
{
    ComponentTransferFunction transferFunction;
    transferFunction.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
    transferFunction.slope = narrowPrecisionToFloat(componentTransferOperation.amount());
    transferFunction.intercept = 0;

    ComponentTransferFunction nullFunction;
    return FEComponentTransfer::create(transferFunction, transferFunction, transferFunction, nullFunction);
}

static RefPtr<FilterEffect> createContrastEffect(const BasicComponentTransferFilterOperation& componentTransferOperation)
{
    ComponentTransferFunction transferFunction;
    transferFunction.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
    float amount = narrowPrecisionToFloat(componentTransferOperation.amount());
    transferFunction.slope = amount;
    transferFunction.intercept = -0.5 * amount + 0.5;

    ComponentTransferFunction nullFunction;
    return FEComponentTransfer::create(transferFunction, transferFunction, transferFunction, nullFunction);
}

static RefPtr<FilterEffect> createDropShadowEffect(const DropShadowFilterOperation& dropShadowOperation)
{
    float std = dropShadowOperation.stdDeviation();
    return FEDropShadow::create(std, std, dropShadowOperation.x(), dropShadowOperation.y(), dropShadowOperation.color(), 1);
}

static RefPtr<FilterEffect> createGrayScaleEffect(const BasicColorMatrixFilterOperation& colorMatrixOperation)
{
    double oneMinusAmount = clampTo(1 - colorMatrixOperation.amount(), 0.0, 1.0);

    // See https://dvcs.w3.org/hg/FXTF/raw-file/tip/filters/index.html#grayscaleEquivalent
    // for information on parameters.

    Vector<float> inputParameters {
        narrowPrecisionToFloat(0.2126 + 0.7874 * oneMinusAmount),
        narrowPrecisionToFloat(0.7152 - 0.7152 * oneMinusAmount),
        narrowPrecisionToFloat(0.0722 - 0.0722 * oneMinusAmount),
        0,
        0,

        narrowPrecisionToFloat(0.2126 - 0.2126 * oneMinusAmount),
        narrowPrecisionToFloat(0.7152 + 0.2848 * oneMinusAmount),
        narrowPrecisionToFloat(0.0722 - 0.0722 * oneMinusAmount),
        0,
        0,

        narrowPrecisionToFloat(0.2126 - 0.2126 * oneMinusAmount),
        narrowPrecisionToFloat(0.7152 - 0.7152 * oneMinusAmount),
        narrowPrecisionToFloat(0.0722 + 0.9278 * oneMinusAmount),
        0,
        0,

        0,
        0,
        0,
        1,
        0,
    };

    return FEColorMatrix::create(FECOLORMATRIX_TYPE_MATRIX, WTFMove(inputParameters));
}

static RefPtr<FilterEffect> createHueRotateEffect(const BasicColorMatrixFilterOperation& colorMatrixOperation)
{
    Vector<float> inputParameters { narrowPrecisionToFloat(colorMatrixOperation.amount()) };
    return FEColorMatrix::create(FECOLORMATRIX_TYPE_HUEROTATE, WTFMove(inputParameters));
}

static RefPtr<FilterEffect> createInvertEffect(const BasicComponentTransferFilterOperation& componentTransferOperation)
{
    ComponentTransferFunction transferFunction;
    transferFunction.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
    float amount = narrowPrecisionToFloat(componentTransferOperation.amount());
    transferFunction.slope = 1 - 2 * amount;
    transferFunction.intercept = amount;

    ComponentTransferFunction nullFunction;
    return FEComponentTransfer::create(transferFunction, transferFunction, transferFunction, nullFunction);
}

static RefPtr<FilterEffect> createOpacityEffect(const BasicComponentTransferFilterOperation& componentTransferOperation)
{
    ComponentTransferFunction transferFunction;
    transferFunction.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
    float amount = narrowPrecisionToFloat(componentTransferOperation.amount());
    transferFunction.slope = amount;
    transferFunction.intercept = 0;

    ComponentTransferFunction nullFunction;
    return FEComponentTransfer::create(nullFunction, nullFunction, nullFunction, transferFunction);
}

static RefPtr<FilterEffect> createSaturateEffect(const BasicColorMatrixFilterOperation& colorMatrixOperation)
{
    Vector<float> inputParameters { narrowPrecisionToFloat(colorMatrixOperation.amount()) };
    return FEColorMatrix::create(FECOLORMATRIX_TYPE_SATURATE, WTFMove(inputParameters));
}

static RefPtr<FilterEffect> createSepiaEffect(const BasicColorMatrixFilterOperation& colorMatrixOperation)
{
    double oneMinusAmount = clampTo(1 - colorMatrixOperation.amount(), 0.0, 1.0);

    // See https://dvcs.w3.org/hg/FXTF/raw-file/tip/filters/index.html#sepiaEquivalent
    // for information on parameters.

    Vector<float> inputParameters {
        narrowPrecisionToFloat(0.393 + 0.607 * oneMinusAmount),
        narrowPrecisionToFloat(0.769 - 0.769 * oneMinusAmount),
        narrowPrecisionToFloat(0.189 - 0.189 * oneMinusAmount),
        0,
        0,

        narrowPrecisionToFloat(0.349 - 0.349 * oneMinusAmount),
        narrowPrecisionToFloat(0.686 + 0.314 * oneMinusAmount),
        narrowPrecisionToFloat(0.168 - 0.168 * oneMinusAmount),
        0,
        0,

        narrowPrecisionToFloat(0.272 - 0.272 * oneMinusAmount),
        narrowPrecisionToFloat(0.534 - 0.534 * oneMinusAmount),
        narrowPrecisionToFloat(0.131 + 0.869 * oneMinusAmount),
        0,
        0,

        0,
        0,
        0,
        1,
        0,
    };

    return FEColorMatrix::create(FECOLORMATRIX_TYPE_MATRIX, WTFMove(inputParameters));
}

static RefPtr<SVGFilter> createSVGFilter(CSSFilter& filter, const ReferenceFilterOperation& filterOperation, RenderElement& renderer, const FloatRect& targetBoundingBox)
{
    auto& referencedSVGResources = renderer.ensureReferencedSVGResources();
    auto* filterElement = referencedSVGResources.referencedFilterElement(renderer.document(), filterOperation);

    if (!filterElement) {
        LOG_WITH_STREAM(Filters, stream << " buildReferenceFilter: failed to find filter renderer, adding pending resource " << filterOperation.fragment());
        // Although we did not find the referenced filter, it might exist later in the document.
        // FIXME: This skips anonymous RenderObjects. <https://webkit.org/b/131085>
        // FIXME: Unclear if this does anything.
        return nullptr;
    }

    auto filterRegion = SVGLengthContext::resolveRectangle<SVGFilterElement>(filterElement, filterElement->filterUnits(), targetBoundingBox);

    SVGFilterBuilder builder;
    return SVGFilter::create(*filterElement, builder, filter.renderingMode(), filter.filterScale(), filter.clipOperation(), filterRegion, targetBoundingBox);
}

bool CSSFilter::buildFilterFunctions(RenderElement& renderer, const FilterOperations& operations, const FloatRect& targetBoundingBox)
{
    RefPtr<FilterEffect> effect;
    RefPtr<SVGFilter> filter;

    for (auto& operation : operations.operations()) {
        switch (operation->type()) {
        case FilterOperation::APPLE_INVERT_LIGHTNESS:
            ASSERT_NOT_REACHED(); // APPLE_INVERT_LIGHTNESS is only used in -apple-color-filter.
            break;

        case FilterOperation::BLUR:
            effect = createBlurEffect(downcast<BlurFilterOperation>(*operation), clipOperation());
            break;

        case FilterOperation::BRIGHTNESS:
            effect = createBrightnessEffect(downcast<BasicComponentTransferFilterOperation>(*operation));
            break;

        case FilterOperation::CONTRAST:
            effect = createContrastEffect(downcast<BasicComponentTransferFilterOperation>(*operation));
            break;

        case FilterOperation::DROP_SHADOW:
            effect = createDropShadowEffect(downcast<DropShadowFilterOperation>(*operation));
            break;

        case FilterOperation::GRAYSCALE:
            effect = createGrayScaleEffect(downcast<BasicColorMatrixFilterOperation>(*operation));
            break;

        case FilterOperation::HUE_ROTATE:
            effect = createHueRotateEffect(downcast<BasicColorMatrixFilterOperation>(*operation));
            break;

        case FilterOperation::INVERT:
            effect = createInvertEffect(downcast<BasicComponentTransferFilterOperation>(*operation));
            break;

        case FilterOperation::OPACITY:
            effect = createOpacityEffect(downcast<BasicComponentTransferFilterOperation>(*operation));
            break;

        case FilterOperation::SATURATE:
            effect = createSaturateEffect(downcast<BasicColorMatrixFilterOperation>(*operation));
            break;

        case FilterOperation::SEPIA:
            effect = createSepiaEffect(downcast<BasicColorMatrixFilterOperation>(*operation));
            break;

        case FilterOperation::REFERENCE:
            filter = createSVGFilter(*this, downcast<ReferenceFilterOperation>(*operation), renderer, targetBoundingBox);
            break;

        default:
            break;
        }

        if (!filter && !effect)
            continue;

        if (m_functions.isEmpty())
            m_functions.append(SourceGraphic::create());

        if (filter)
            m_functions.append(filter.releaseNonNull());
        else
            m_functions.append(effect.releaseNonNull());
    }

    // If we didn't make any effects, tell our caller we are not valid.
    if (m_functions.isEmpty())
        return false;

    m_functions.shrinkToFit();
    return true;
}

FilterEffectVector CSSFilter::effectsOfType(FilterFunction::Type filterType) const
{
    FilterEffectVector effects;

    for (auto& function : m_functions) {
        if (function->filterType() == filterType) {
            effects.append({ downcast<FilterEffect>(function.get()) });
            continue;
        }

        if (function->isSVGFilter()) {
            auto& filter = downcast<SVGFilter>(function.get());
            effects.appendVector(filter.effectsOfType(filterType));
        }
    }

    return effects;
}

bool CSSFilter::supportsAcceleratedRendering() const
{
    if (renderingMode() == RenderingMode::Unaccelerated)
        return false;

    for (auto& function : m_functions) {
        if (!function->supportsAcceleratedRendering())
            return false;
    }

    return true;
}

RefPtr<FilterImage> CSSFilter::apply(FilterImage* sourceImage, FilterResults& results)
{
    if (!sourceImage)
        return nullptr;
    
    RefPtr<FilterImage> result = sourceImage;

    for (auto& function : m_functions) {
        result = function->apply(*this, *result, results);
        if (!result)
            return nullptr;
    }

    return result;
}

void CSSFilter::setFilterRegion(const FloatRect& filterRegion)
{
    Filter::setFilterRegion(filterRegion);
    clampFilterRegionIfNeeded();
}

IntOutsets CSSFilter::outsets() const
{
    if (!m_hasFilterThatMovesPixels)
        return { };

    if (!m_outsets.isZero())
        return m_outsets;

    for (auto& function : m_functions)
        m_outsets += function->outsets(*this);
    return m_outsets;
}

TextStream& CSSFilter::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    unsigned level = 0;

    for (auto it = m_functions.rbegin(), end = m_functions.rend(); it != end; ++it) {
        auto& function = *it;
        
        // SourceAlpha is a built-in effect. No need to say SourceGraphic is its input.
        if (function->filterType() == FilterEffect::Type::SourceAlpha)
            ++it;

        TextStream::IndentScope indentScope(ts, level++);
        function->externalRepresentation(ts, representation);
    }

    return ts;
}

} // namespace WebCore
