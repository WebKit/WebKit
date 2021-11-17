/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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
#include "FEMerge.h"
#include "FilterEffectRenderer.h"
#include "FilterOperations.h"
#include "GraphicsContext.h"
#include "LengthFunctions.h"
#include "Logging.h"
#include "ReferencedSVGResources.h"
#include "RenderElement.h"
#include "SVGFilter.h"
#include "SVGFilterBuilder.h"
#include "SVGFilterElement.h"
#include "SourceGraphic.h"

#if USE(DIRECT2D)
#include <d2d1.h>
#endif

namespace WebCore {

RefPtr<CSSFilter> CSSFilter::create(const FilterOperations& operations, RenderingMode renderingMode, float scaleFactor)
{
    bool hasFilterThatMovesPixels = operations.hasFilterThatMovesPixels();
    bool hasFilterThatShouldBeRestrictedBySecurityOrigin = operations.hasFilterThatShouldBeRestrictedBySecurityOrigin();

    auto filter = adoptRef(*new CSSFilter(hasFilterThatMovesPixels, hasFilterThatShouldBeRestrictedBySecurityOrigin, scaleFactor));

    filter->setRenderingMode(renderingMode);
    return filter;
}

CSSFilter::CSSFilter(bool hasFilterThatMovesPixels, bool hasFilterThatShouldBeRestrictedBySecurityOrigin, float scaleFactor)
    : Filter(Filter::Type::CSSFilter, FloatSize { scaleFactor, scaleFactor })
    , m_hasFilterThatMovesPixels(hasFilterThatMovesPixels)
    , m_hasFilterThatShouldBeRestrictedBySecurityOrigin(hasFilterThatShouldBeRestrictedBySecurityOrigin)
{
}

static RefPtr<FilterEffect> createBlurEffect(const BlurFilterOperation& blurOperation, FilterConsumer consumer)
{
    float stdDeviation = floatValueForLength(blurOperation.stdDeviation(), 0);
    return FEGaussianBlur::create(stdDeviation, stdDeviation, consumer == FilterConsumer::FilterProperty ? EDGEMODE_NONE : EDGEMODE_DUPLICATE);
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

static RefPtr<SVGFilter> createSVGFilter(CSSFilter& filter, const ReferenceFilterOperation& filterOperation, RenderElement& renderer, FilterEffect& previousEffect)
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

    SVGFilterBuilder builder;
    return SVGFilter::create(*filterElement, builder, filter.filterScale(), filter.sourceImageRect(), filter.filterRegion(), previousEffect);
}

static void setupLastEffectProperties(FilterEffect& effect, FilterConsumer consumer)
{
    // Unlike SVG Filters and CSSFilterImages, filter functions on the filter
    // property applied here should not clip to their primitive subregions.
    effect.setClipsToBounds(consumer == FilterConsumer::FilterFunction);
    effect.setOperatingColorSpace(DestinationColorSpace::SRGB());
}

bool CSSFilter::buildFilterFunctions(RenderElement& renderer, const FilterOperations& operations, FilterConsumer consumer)
{
    m_functions.clear();
    m_outsets = { };

    RefPtr<FilterEffect> previousEffect = SourceGraphic::create();
    RefPtr<SVGFilter> filter;
    
    for (auto& operation : operations.operations()) {
        RefPtr<FilterEffect> effect;

        switch (operation->type()) {
        case FilterOperation::APPLE_INVERT_LIGHTNESS:
            ASSERT_NOT_REACHED(); // APPLE_INVERT_LIGHTNESS is only used in -apple-color-filter.
            break;

        case FilterOperation::BLUR:
            effect = createBlurEffect(downcast<BlurFilterOperation>(*operation), consumer);
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
            filter = createSVGFilter(*this, downcast<ReferenceFilterOperation>(*operation), renderer, *previousEffect);
            effect = nullptr;
            break;

        default:
            break;
        }

        if ((filter || effect) && m_functions.isEmpty()) {
            ASSERT(previousEffect->filterType() == FilterEffect::Type::SourceGraphic);
            m_functions.append({ *previousEffect });
        }
        
        if (filter) {
            effect = filter->lastEffect();
            setupLastEffectProperties(*effect, consumer);
            m_functions.append(filter.releaseNonNull());
            previousEffect = WTFMove(effect);
            continue;
        }

        if (effect) {
            setupLastEffectProperties(*effect, consumer);
            effect->inputEffects() = { WTFMove(previousEffect) };
            m_functions.append({ *effect });
            previousEffect = WTFMove(effect);
        }
    }

    // If we didn't make any effects, tell our caller we are not valid.
    if (m_functions.isEmpty())
        return false;

    m_functions.shrinkToFit();

#if USE(CORE_IMAGE)
    if (!m_filterRenderer)
        m_filterRenderer = FilterEffectRenderer::tryCreate(renderer.settings().coreImageAcceleratedFilterRenderEnabled(), *lastEffect());
#endif
    return true;
}

GraphicsContext* CSSFilter::inputContext()
{
    return sourceImage() ? &sourceImage()->context() : nullptr;
}

bool CSSFilter::updateBackingStoreRect(const FloatRect& filterRect)
{
    if (filterRect.isEmpty() || ImageBuffer::sizeNeedsClamping(filterRect.size()))
        return false;

    if (filterRect == sourceImageRect())
        return false;

    setSourceImageRect(filterRect);
    return true;
}

void CSSFilter::allocateBackingStoreIfNeeded(const GraphicsContext& targetContext)
{
    // At this point the effect chain has been built, and the
    // source image sizes set. We just need to attach the graphic
    // buffer if we have not yet done so.

    if (m_graphicsBufferAttached)
        return;

    auto logicalSize = sourceImageRect().size();
    if (!sourceImage() || sourceImage()->logicalSize() != logicalSize) {
#if USE(DIRECT2D)
        setSourceImage(ImageBuffer::create(logicalSize, renderingMode(), &targetContext, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8));
#else
        UNUSED_PARAM(targetContext);
        RenderingMode mode = m_filterRenderer ? RenderingMode::Accelerated : renderingMode();
        setSourceImage(ImageBuffer::create(logicalSize, mode, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8));
#endif
        if (auto context = inputContext())
            context->scale(filterScale());
    }

    m_graphicsBufferAttached = true;
}

RefPtr<FilterEffect> CSSFilter::lastEffect()
{
    if (m_functions.isEmpty())
        return nullptr;

    auto& function = m_functions.last();
    if (function->isSVGFilter())
        return downcast<SVGFilter>(function.ptr())->lastEffect();

    return downcast<FilterEffect>(function.ptr());
}

void CSSFilter::determineFilterPrimitiveSubregion()
{
    auto effect = lastEffect();
    effect->determineFilterPrimitiveSubregion(*this);
    FloatRect subRegion = effect->maxEffectRect();
    // At least one FilterEffect has a too big image size, recalculate the effect sizes with new scale factors.
    FloatSize filterScale { 1, 1 };
    if (ImageBuffer::sizeNeedsClamping(subRegion.size(), filterScale)) {
        setFilterScale(filterScale);
        effect->determineFilterPrimitiveSubregion(*this);
    }
}

void CSSFilter::clearIntermediateResults()
{
    for (auto& function : m_functions)
        function->clearResult();
}

bool CSSFilter::apply()
{
    auto effect = lastEffect();
    if (m_filterRenderer) {
        m_filterRenderer->applyEffects(*this, *effect);
        if (m_filterRenderer->hasResult()) {
            effect->transformResultColorSpace(DestinationColorSpace::SRGB());
            return true;
        }
    }

    for (auto& function : m_functions) {
        if (!function->apply(*this))
            return false;
    }

    effect->transformResultColorSpace(DestinationColorSpace::SRGB());
    return true;
}

LayoutRect CSSFilter::computeSourceImageRectForDirtyRect(const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect)
{
    // The result of this function is the area in the "filterBoxRect" that needs to be repainted, so that we fully cover the "dirtyRect".
    auto rectForRepaint = dirtyRect;
    if (hasFilterThatMovesPixels())
        rectForRepaint += outsets();
    rectForRepaint.intersect(filterBoxRect);
    return rectForRepaint;
}

ImageBuffer* CSSFilter::output()
{
    if (m_filterRenderer && m_filterRenderer->hasResult())
        return m_filterRenderer->output();
    
    return lastEffect()->imageBufferResult();
}

void CSSFilter::setSourceImageRect(const FloatRect& sourceImageRect)
{
    auto scaledSourceImageRect = sourceImageRect;
    scaledSourceImageRect.scale(filterScale());

    Filter::setFilterRegion(sourceImageRect);
    Filter::setSourceImageRect(scaledSourceImageRect);

    for (auto& function : m_functions) {
        if (function->isSVGFilter()) {
            downcast<SVGFilter>(function.ptr())->setFilterRegion(sourceImageRect);
            downcast<SVGFilter>(function.ptr())->setSourceImageRect(scaledSourceImageRect);
        }
    }

    m_graphicsBufferAttached = false;
}

IntRect CSSFilter::outputRect()
{
    auto effect = lastEffect();

    if (effect->hasResult() || (m_filterRenderer && m_filterRenderer->hasResult()))
        return effect->requestedRegionOfInputPixelBuffer(IntRect { filterRegion() });

    return { };
}

IntOutsets CSSFilter::outsets() const
{
    if (!m_hasFilterThatMovesPixels)
        return { };

    if (!m_outsets.isZero())
        return m_outsets;

    for (auto& function : m_functions)
        m_outsets += function->outsets();
    return m_outsets;
}

} // namespace WebCore
