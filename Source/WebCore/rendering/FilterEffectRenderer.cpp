/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#if ENABLE(CSS_FILTERS)

#include "FilterEffectRenderer.h"

#include "CachedSVGDocument.h"
#include "CachedSVGDocumentReference.h"
#include "ColorSpace.h"
#include "Document.h"
#include "ElementIterator.h"
#include "FEColorMatrix.h"
#include "FEComponentTransfer.h"
#include "FEDropShadow.h"
#include "FEGaussianBlur.h"
#include "FEMerge.h"
#include "FloatConversion.h"
#include "Frame.h"
#include "RenderLayer.h"
#include "SVGElement.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "Settings.h"
#include "SourceAlpha.h"

#include <algorithm>
#include <wtf/MathExtras.h>

namespace WebCore {

static inline void endMatrixRow(Vector<float>& parameters)
{
    parameters.append(0);
    parameters.append(0);
}

static inline void lastMatrixRow(Vector<float>& parameters)
{
    parameters.append(0);
    parameters.append(0);
    parameters.append(0);
    parameters.append(1);
    parameters.append(0);
}

FilterEffectRenderer::FilterEffectRenderer()
    : m_graphicsBufferAttached(false)
    , m_hasFilterThatMovesPixels(false)
{
    setFilterResolution(FloatSize(1, 1));
    m_sourceGraphic = SourceGraphic::create(this);
}

FilterEffectRenderer::~FilterEffectRenderer()
{
}

GraphicsContext* FilterEffectRenderer::inputContext()
{
    return sourceImage() ? sourceImage()->context() : 0;
}

PassRefPtr<FilterEffect> FilterEffectRenderer::buildReferenceFilter(RenderElement* renderer, PassRefPtr<FilterEffect> previousEffect, ReferenceFilterOperation* filterOperation)
{
    if (!renderer)
        return 0;

    Document* document = &renderer->document();

    CachedSVGDocumentReference* cachedSVGDocumentReference = filterOperation->cachedSVGDocumentReference();
    CachedSVGDocument* cachedSVGDocument = cachedSVGDocumentReference ? cachedSVGDocumentReference->document() : 0;

    // If we have an SVG document, this is an external reference. Otherwise
    // we look up the referenced node in the current document.
    if (cachedSVGDocument)
        document = cachedSVGDocument->document();

    if (!document)
        return 0;

    Element* filter = document->getElementById(filterOperation->fragment());
    if (!filter) {
        // Although we did not find the referenced filter, it might exist later in the document.
        // FIXME: This skips anonymous RenderObjects. <https://webkit.org/b/131085>
        if (Element* element = renderer->element())
            document->accessSVGExtensions()->addPendingResource(filterOperation->fragment(), element);
        return 0;
    }

    RefPtr<FilterEffect> effect;

    // FIXME: Figure out what to do with SourceAlpha. Right now, we're
    // using the alpha of the original input layer, which is obviously
    // wrong. We should probably be extracting the alpha from the 
    // previousEffect, but this requires some more processing.  
    // This may need a spec clarification.
    auto builder = std::make_unique<SVGFilterBuilder>(previousEffect, SourceAlpha::create(this));

    for (auto& effectElement : childrenOfType<SVGFilterPrimitiveStandardAttributes>(*filter)) {
        effect = effectElement.build(builder.get(), this);
        if (!effect)
            continue;

        effectElement.setStandardAttributes(effect.get());
        builder->add(effectElement.result(), effect);
        m_effects.append(effect);
    }
    return effect;
}

bool FilterEffectRenderer::build(RenderElement* renderer, const FilterOperations& operations, FilterConsumer consumer)
{
    m_hasFilterThatMovesPixels = operations.hasFilterThatMovesPixels();
    if (m_hasFilterThatMovesPixels)
        m_outsets = operations.outsets();

    m_effects.clear();

    RefPtr<FilterEffect> previousEffect = m_sourceGraphic;
    for (size_t i = 0; i < operations.operations().size(); ++i) {
        RefPtr<FilterEffect> effect;
        FilterOperation* filterOperation = operations.operations().at(i).get();
        switch (filterOperation->type()) {
        case FilterOperation::REFERENCE: {
            ReferenceFilterOperation* referenceOperation = toReferenceFilterOperation(filterOperation);
            effect = buildReferenceFilter(renderer, previousEffect, referenceOperation);
            referenceOperation->setFilterEffect(effect);
            break;
        }
        case FilterOperation::GRAYSCALE: {
            BasicColorMatrixFilterOperation* colorMatrixOperation = toBasicColorMatrixFilterOperation(filterOperation);
            Vector<float> inputParameters;
            double oneMinusAmount = clampTo(1 - colorMatrixOperation->amount(), 0.0, 1.0);

            // See https://dvcs.w3.org/hg/FXTF/raw-file/tip/filters/index.html#grayscaleEquivalent
            // for information on parameters.

            inputParameters.append(narrowPrecisionToFloat(0.2126 + 0.7874 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.7152 - 0.7152 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.0722 - 0.0722 * oneMinusAmount));
            endMatrixRow(inputParameters);

            inputParameters.append(narrowPrecisionToFloat(0.2126 - 0.2126 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.7152 + 0.2848 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.0722 - 0.0722 * oneMinusAmount));
            endMatrixRow(inputParameters);

            inputParameters.append(narrowPrecisionToFloat(0.2126 - 0.2126 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.7152 - 0.7152 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.0722 + 0.9278 * oneMinusAmount));
            endMatrixRow(inputParameters);

            lastMatrixRow(inputParameters);

            effect = FEColorMatrix::create(this, FECOLORMATRIX_TYPE_MATRIX, inputParameters);
            break;
        }
        case FilterOperation::SEPIA: {
            BasicColorMatrixFilterOperation* colorMatrixOperation = toBasicColorMatrixFilterOperation(filterOperation);
            Vector<float> inputParameters;
            double oneMinusAmount = clampTo(1 - colorMatrixOperation->amount(), 0.0, 1.0);

            // See https://dvcs.w3.org/hg/FXTF/raw-file/tip/filters/index.html#sepiaEquivalent
            // for information on parameters.

            inputParameters.append(narrowPrecisionToFloat(0.393 + 0.607 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.769 - 0.769 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.189 - 0.189 * oneMinusAmount));
            endMatrixRow(inputParameters);

            inputParameters.append(narrowPrecisionToFloat(0.349 - 0.349 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.686 + 0.314 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.168 - 0.168 * oneMinusAmount));
            endMatrixRow(inputParameters);

            inputParameters.append(narrowPrecisionToFloat(0.272 - 0.272 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.534 - 0.534 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.131 + 0.869 * oneMinusAmount));
            endMatrixRow(inputParameters);

            lastMatrixRow(inputParameters);

            effect = FEColorMatrix::create(this, FECOLORMATRIX_TYPE_MATRIX, inputParameters);
            break;
        }
        case FilterOperation::SATURATE: {
            BasicColorMatrixFilterOperation* colorMatrixOperation = toBasicColorMatrixFilterOperation(filterOperation);
            Vector<float> inputParameters;
            inputParameters.append(narrowPrecisionToFloat(colorMatrixOperation->amount()));
            effect = FEColorMatrix::create(this, FECOLORMATRIX_TYPE_SATURATE, inputParameters);
            break;
        }
        case FilterOperation::HUE_ROTATE: {
            BasicColorMatrixFilterOperation* colorMatrixOperation = toBasicColorMatrixFilterOperation(filterOperation);
            Vector<float> inputParameters;
            inputParameters.append(narrowPrecisionToFloat(colorMatrixOperation->amount()));
            effect = FEColorMatrix::create(this, FECOLORMATRIX_TYPE_HUEROTATE, inputParameters);
            break;
        }
        case FilterOperation::INVERT: {
            BasicComponentTransferFilterOperation* componentTransferOperation = toBasicComponentTransferFilterOperation(filterOperation);
            ComponentTransferFunction transferFunction;
            transferFunction.type = FECOMPONENTTRANSFER_TYPE_TABLE;
            Vector<float> transferParameters;
            transferParameters.append(narrowPrecisionToFloat(componentTransferOperation->amount()));
            transferParameters.append(narrowPrecisionToFloat(1 - componentTransferOperation->amount()));
            transferFunction.tableValues = transferParameters;

            ComponentTransferFunction nullFunction;
            effect = FEComponentTransfer::create(this, transferFunction, transferFunction, transferFunction, nullFunction);
            break;
        }
        case FilterOperation::OPACITY: {
            BasicComponentTransferFilterOperation* componentTransferOperation = toBasicComponentTransferFilterOperation(filterOperation);
            ComponentTransferFunction transferFunction;
            transferFunction.type = FECOMPONENTTRANSFER_TYPE_TABLE;
            Vector<float> transferParameters;
            transferParameters.append(0);
            transferParameters.append(narrowPrecisionToFloat(componentTransferOperation->amount()));
            transferFunction.tableValues = transferParameters;

            ComponentTransferFunction nullFunction;
            effect = FEComponentTransfer::create(this, nullFunction, nullFunction, nullFunction, transferFunction);
            break;
        }
        case FilterOperation::BRIGHTNESS: {
            BasicComponentTransferFilterOperation* componentTransferOperation = toBasicComponentTransferFilterOperation(filterOperation);
            ComponentTransferFunction transferFunction;
            transferFunction.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
            transferFunction.slope = narrowPrecisionToFloat(componentTransferOperation->amount());
            transferFunction.intercept = 0;

            ComponentTransferFunction nullFunction;
            effect = FEComponentTransfer::create(this, transferFunction, transferFunction, transferFunction, nullFunction);
            break;
        }
        case FilterOperation::CONTRAST: {
            BasicComponentTransferFilterOperation* componentTransferOperation = toBasicComponentTransferFilterOperation(filterOperation);
            ComponentTransferFunction transferFunction;
            transferFunction.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
            float amount = narrowPrecisionToFloat(componentTransferOperation->amount());
            transferFunction.slope = amount;
            transferFunction.intercept = -0.5 * amount + 0.5;
            
            ComponentTransferFunction nullFunction;
            effect = FEComponentTransfer::create(this, transferFunction, transferFunction, transferFunction, nullFunction);
            break;
        }
        case FilterOperation::BLUR: {
            BlurFilterOperation* blurOperation = toBlurFilterOperation(filterOperation);
            float stdDeviation = floatValueForLength(blurOperation->stdDeviation(), 0);
            effect = FEGaussianBlur::create(this, stdDeviation, stdDeviation, consumer == FilterProperty ? EDGEMODE_NONE : EDGEMODE_DUPLICATE);
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            DropShadowFilterOperation* dropShadowOperation = toDropShadowFilterOperation(filterOperation);
            effect = FEDropShadow::create(this, dropShadowOperation->stdDeviation(), dropShadowOperation->stdDeviation(),
                                                dropShadowOperation->x(), dropShadowOperation->y(), dropShadowOperation->color(), 1);
            break;
        }
        default:
            break;
        }

        if (effect) {
            // Unlike SVG Filters and CSSFilterImages, filter functions on the filter
            // property applied here should not clip to their primitive subregions.
            effect->setClipsToBounds(consumer == FilterFunction);
            effect->setOperatingColorSpace(ColorSpaceDeviceRGB);
            
            if (filterOperation->type() != FilterOperation::REFERENCE) {
                effect->inputEffects().append(previousEffect);
                m_effects.append(effect);
            }
            previousEffect = effect.release();
        }
    }

    // If we didn't make any effects, tell our caller we are not valid
    if (!m_effects.size())
        return false;

    setMaxEffectRects(m_sourceDrawingRegion);
    
    return true;
}

bool FilterEffectRenderer::updateBackingStoreRect(const FloatRect& filterRect)
{
    if (!filterRect.isZero() && FilterEffect::isFilterSizeValid(filterRect)) {
        FloatRect currentSourceRect = sourceImageRect();
        if (filterRect != currentSourceRect) {
            setSourceImageRect(filterRect);
            return true;
        }
    }
    return false;
}

void FilterEffectRenderer::allocateBackingStoreIfNeeded()
{
    // At this point the effect chain has been built, and the
    // source image sizes set. We just need to attach the graphic
    // buffer if we have not yet done so.
    if (!m_graphicsBufferAttached) {
        IntSize logicalSize(m_sourceDrawingRegion.width(), m_sourceDrawingRegion.height());
        if (!sourceImage() || sourceImage()->logicalSize() != logicalSize)
            setSourceImage(ImageBuffer::create(logicalSize, 1, ColorSpaceDeviceRGB, renderingMode()));
        m_graphicsBufferAttached = true;
    }
}

void FilterEffectRenderer::clearIntermediateResults()
{
    m_sourceGraphic->clearResult();
    for (size_t i = 0; i < m_effects.size(); ++i)
        m_effects[i]->clearResult();
}

void FilterEffectRenderer::apply()
{
    RefPtr<FilterEffect> effect = lastEffect();
    effect->apply();
    effect->transformResultColorSpace(ColorSpaceDeviceRGB);
}

LayoutRect FilterEffectRenderer::computeSourceImageRectForDirtyRect(const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect)
{
    // The result of this function is the area in the "filterBoxRect" that needs to be repainted, so that we fully cover the "dirtyRect".
    LayoutRect rectForRepaint = dirtyRect;
    if (hasFilterThatMovesPixels()) {
        // Note that the outsets are reversed here because we are going backwards -> we have the dirty rect and
        // need to find out what is the rectangle that might influence the result inside that dirty rect.
        rectForRepaint.move(-m_outsets.right(), -m_outsets.bottom());
        rectForRepaint.expand(m_outsets.left() + m_outsets.right(), m_outsets.top() + m_outsets.bottom());
    }
    rectForRepaint.intersect(filterBoxRect);
    return rectForRepaint;
}

bool FilterEffectRendererHelper::prepareFilterEffect(RenderLayer* renderLayer, const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect, const LayoutRect& layerRepaintRect)
{
    ASSERT(m_haveFilterEffect && renderLayer->filterRenderer());
    m_renderLayer = renderLayer;
    m_repaintRect = dirtyRect;

    FilterEffectRenderer* filter = renderLayer->filterRenderer();
    LayoutRect filterSourceRect = filter->computeSourceImageRectForDirtyRect(filterBoxRect, dirtyRect);
    m_paintOffset = filterSourceRect.location();

    if (filterSourceRect.isEmpty()) {
        // The dirty rect is not in view, just bail out.
        m_haveFilterEffect = false;
        return false;
    }
    
    bool hasUpdatedBackingStore = filter->updateBackingStoreRect(filterSourceRect);
    if (filter->hasFilterThatMovesPixels()) {
        if (hasUpdatedBackingStore)
            m_repaintRect = filterSourceRect;
        else {
            m_repaintRect.unite(layerRepaintRect);
            m_repaintRect.intersect(filterSourceRect);
        }
    }
    return true;
}

GraphicsContext* FilterEffectRendererHelper::filterContext() const
{
    if (!m_haveFilterEffect)
        return 0;

    FilterEffectRenderer* filter = m_renderLayer->filterRenderer();
    return filter->inputContext();
}

bool FilterEffectRendererHelper::beginFilterEffect()
{
    ASSERT(m_renderLayer);
    
    FilterEffectRenderer* filter = m_renderLayer->filterRenderer();
    filter->allocateBackingStoreIfNeeded();
    // Paint into the context that represents the SourceGraphic of the filter.
    GraphicsContext* sourceGraphicsContext = filter->inputContext();
    if (!sourceGraphicsContext || !FilterEffect::isFilterSizeValid(filter->filterRegion())) {
        // Disable the filters and continue.
        m_haveFilterEffect = false;
        return false;
    }
    
    // Translate the context so that the contents of the layer is captuterd in the offscreen memory buffer.
    sourceGraphicsContext->save();
    sourceGraphicsContext->translate(-m_paintOffset.x(), -m_paintOffset.y());
    sourceGraphicsContext->clearRect(m_repaintRect);
    sourceGraphicsContext->clip(m_repaintRect);

    m_startedFilterEffect = true;
    return true;
}

void FilterEffectRendererHelper::applyFilterEffect(GraphicsContext* destinationContext)
{
    ASSERT(m_haveFilterEffect && m_renderLayer->filterRenderer());
    FilterEffectRenderer* filter = m_renderLayer->filterRenderer();
    filter->inputContext()->restore();

    filter->apply();
    
    // Get the filtered output and draw it in place.
    LayoutRect destRect = filter->outputRect();
    destRect.move(m_paintOffset.x(), m_paintOffset.y());
    
    destinationContext->drawImageBuffer(filter->output(), m_renderLayer->renderer().style().colorSpace(), pixelSnappedIntRect(destRect), CompositeSourceOver);
    
    filter->clearIntermediateResults();
}

} // namespace WebCore

#endif // ENABLE(CSS_FILTERS)
