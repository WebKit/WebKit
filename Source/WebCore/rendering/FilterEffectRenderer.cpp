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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "FEColorMatrix.h"
#include "FEComponentTransfer.h"
#include "FEDropShadow.h"
#include "FEGaussianBlur.h"
#include "FEMerge.h"
#include "FilterEffectObserver.h"
#include "FloatConversion.h"
#include "RenderLayer.h"

#include <algorithm>
#include <wtf/MathExtras.h>

#if ENABLE(CSS_SHADERS) && ENABLE(WEBGL)
#include "CustomFilterProgram.h"
#include "CustomFilterOperation.h"
#include "FECustomFilter.h"
#include "Settings.h"
#endif

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
    
    
#if ENABLE(CSS_SHADERS) && ENABLE(WEBGL)
static bool isWebGLEnabled(Document* document)
{
    // We only want to enable shaders if WebGL is also enabled on this platform.
    Settings* settings = document->settings();
    return settings && settings->webGLEnabled();
}
#endif

FilterEffectRenderer::FilterEffectRenderer(FilterEffectObserver* observer)
    : m_observer(observer)
    , m_graphicsBufferAttached(false)
{
    setFilterResolution(FloatSize(1, 1));
    m_sourceGraphic = SourceGraphic::create(this);
}

FilterEffectRenderer::~FilterEffectRenderer()
{
#if ENABLE(CSS_SHADERS)
    removeCustomFilterClients();
#endif
}

GraphicsContext* FilterEffectRenderer::inputContext()
{
    return sourceImage() ? sourceImage()->context() : 0;
}

void FilterEffectRenderer::build(Document* document, const FilterOperations& operations)
{
#if !ENABLE(CSS_SHADERS) || !ENABLE(WEBGL)
    UNUSED_PARAM(document);
#else
    CustomFilterProgramList cachedCustomFilterPrograms;
#endif

    m_effects.clear();

    RefPtr<FilterEffect> effect;
    RefPtr<FilterEffect> previousEffect;
    for (size_t i = 0; i < operations.operations().size(); ++i) {
        FilterOperation* filterOperation = operations.operations().at(i).get();
        switch (filterOperation->getOperationType()) {
        case FilterOperation::REFERENCE: {
            // FIXME: Not yet implemented.
            // https://bugs.webkit.org/show_bug.cgi?id=72443
            break;
        }
        case FilterOperation::GRAYSCALE: {
            BasicColorMatrixFilterOperation* colorMatrixOperation = static_cast<BasicColorMatrixFilterOperation*>(filterOperation);
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
            BasicColorMatrixFilterOperation* colorMatrixOperation = static_cast<BasicColorMatrixFilterOperation*>(filterOperation);
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
            BasicColorMatrixFilterOperation* colorMatrixOperation = static_cast<BasicColorMatrixFilterOperation*>(filterOperation);
            Vector<float> inputParameters;
            inputParameters.append(narrowPrecisionToFloat(colorMatrixOperation->amount()));
            effect = FEColorMatrix::create(this, FECOLORMATRIX_TYPE_SATURATE, inputParameters);
            break;
        }
        case FilterOperation::HUE_ROTATE: {
            BasicColorMatrixFilterOperation* colorMatrixOperation = static_cast<BasicColorMatrixFilterOperation*>(filterOperation);
            Vector<float> inputParameters;
            inputParameters.append(narrowPrecisionToFloat(colorMatrixOperation->amount()));
            effect = FEColorMatrix::create(this, FECOLORMATRIX_TYPE_HUEROTATE, inputParameters);
            break;
        }
        case FilterOperation::INVERT: {
            BasicComponentTransferFilterOperation* componentTransferOperation = static_cast<BasicComponentTransferFilterOperation*>(filterOperation);
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
            BasicComponentTransferFilterOperation* componentTransferOperation = static_cast<BasicComponentTransferFilterOperation*>(filterOperation);
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
            BasicComponentTransferFilterOperation* componentTransferOperation = static_cast<BasicComponentTransferFilterOperation*>(filterOperation);
            ComponentTransferFunction transferFunction;
            transferFunction.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
            transferFunction.slope = narrowPrecisionToFloat(componentTransferOperation->amount());

            ComponentTransferFunction nullFunction;
            effect = FEComponentTransfer::create(this, transferFunction, transferFunction, transferFunction, nullFunction);
            break;
        }
        case FilterOperation::CONTRAST: {
            BasicComponentTransferFilterOperation* componentTransferOperation = static_cast<BasicComponentTransferFilterOperation*>(filterOperation);
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
            BlurFilterOperation* blurOperation = static_cast<BlurFilterOperation*>(filterOperation);
            float stdDeviation = blurOperation->stdDeviation().calcFloatValue(0);
            effect = FEGaussianBlur::create(this, stdDeviation, stdDeviation);
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            DropShadowFilterOperation* dropShadowOperation = static_cast<DropShadowFilterOperation*>(filterOperation);
            effect = FEDropShadow::create(this, dropShadowOperation->stdDeviation(), dropShadowOperation->stdDeviation(),
                                                dropShadowOperation->x(), dropShadowOperation->y(), dropShadowOperation->color(), 1);
            break;
        }
#if ENABLE(CSS_SHADERS)
        case FilterOperation::CUSTOM: {
#if ENABLE(WEBGL)
            if (!isWebGLEnabled(document))
                continue;
            
            CustomFilterOperation* customFilterOperation = static_cast<CustomFilterOperation*>(filterOperation);
            RefPtr<CustomFilterProgram> program = customFilterOperation->program();
            cachedCustomFilterPrograms.append(program);
            program->addClient(this);
            if (program->isLoaded()) {
                effect = FECustomFilter::create(this, document, program,
                                                customFilterOperation->meshRows(), customFilterOperation->meshColumns(),
                                                customFilterOperation->meshBoxType(), customFilterOperation->meshType());
            }
#endif
            break;
        }
#endif
        default:
            break;
        }

        if (effect) {
            // Unlike SVG, filters applied here should not clip to their primitive subregions.
            effect->setClipsToBounds(false);
            
            if (previousEffect)
                effect->inputEffects().append(previousEffect);
            m_effects.append(effect);
            previousEffect = effect;
        }
    }

    // If we didn't make a real filter, create a null-op (FEMerge with one input).
    if (!effect)
        m_effects.append(FEMerge::create(this));

    m_effects.first()->inputEffects().append(m_sourceGraphic);
    setMaxEffectRects(m_sourceDrawingRegion);
    
#if ENABLE(CSS_SHADERS) && ENABLE(WEBGL)
    removeCustomFilterClients();
    m_cachedCustomFilterPrograms.swap(cachedCustomFilterPrograms);
#endif
}

void FilterEffectRenderer::updateBackingStore(const FloatRect& filterRect)
{
    if (!filterRect.isZero()) {
        FloatRect currentSourceRect = sourceImageRect();
        if (filterRect != currentSourceRect)
            setSourceImageRect(filterRect);
    }
}

#if ENABLE(CSS_SHADERS)
void FilterEffectRenderer::notifyCustomFilterProgramLoaded(CustomFilterProgram*)
{
    m_observer->filterNeedsRepaint();
}

void FilterEffectRenderer::removeCustomFilterClients()
{
    for (CustomFilterProgramList::iterator iter = m_cachedCustomFilterPrograms.begin(), end = m_cachedCustomFilterPrograms.end(); iter != end; ++iter)
        iter->get()->removeClient(this);
}
#endif

void FilterEffectRenderer::prepare()
{
    // At this point the effect chain has been built, and the
    // source image sizes set. We just need to attach the graphic
    // buffer if we have not yet done so.
    if (!m_graphicsBufferAttached) {
        setSourceImage(ImageBuffer::create(IntSize(m_sourceDrawingRegion.width(), m_sourceDrawingRegion.height()), ColorSpaceDeviceRGB, renderingMode()));
        m_graphicsBufferAttached = true;
    }
    m_sourceGraphic->clearResult();
    lastEffect()->clearResult();
}

void FilterEffectRenderer::apply()
{
    lastEffect()->apply();
}


GraphicsContext* FilterEffectRendererHelper::beginFilterEffect(RenderLayer* renderLayer, GraphicsContext* oldContext, const LayoutRect& filterRect)
{
    ASSERT(m_haveFilterEffect && renderLayer->filter());
    m_savedGraphicsContext = oldContext;
    m_renderLayer = renderLayer;
    m_paintOffset = filterRect.location();
    
    FloatRect filterSourceRect = filterRect;
    filterSourceRect.setLocation(LayoutPoint());
    
    FilterEffectRenderer* filter = renderLayer->filter();
    filter->updateBackingStore(filterSourceRect);
    filter->prepare();
    
    // Paint into the context that represents the SourceGraphic of the filter.
    GraphicsContext* sourceGraphicsContext = filter->inputContext();
    if (!sourceGraphicsContext) {
        // Could not allocate a new graphics context. Disable the filters and continue.
        m_haveFilterEffect = false;
        return m_savedGraphicsContext;
    }
    
    sourceGraphicsContext->save();
    sourceGraphicsContext->translate(-filterRect.x(), -filterRect.y());
    sourceGraphicsContext->clearRect(filterRect);
    
    return sourceGraphicsContext;
}

GraphicsContext* FilterEffectRendererHelper::applyFilterEffect()
{
    ASSERT(m_haveFilterEffect && m_renderLayer->filter());
    FilterEffectRenderer* filter = m_renderLayer->filter();
    filter->apply();
    
    filter->inputContext()->restore();
    
    // Get the filtered output and draw it in place.
    IntRect destRect = filter->outputRect();
    destRect.move(m_paintOffset.x(), m_paintOffset.y());
    
    m_savedGraphicsContext->drawImageBuffer(filter->output(), m_renderLayer->renderer()->style()->colorSpace(), destRect, CompositeSourceOver);
    
    return m_savedGraphicsContext;
}

} // namespace WebCore

#endif // ENABLE(CSS_FILTERS)
