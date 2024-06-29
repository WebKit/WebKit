/*
 * Copyright (C) 2011-2023 Apple Inc. All rights reserved.
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

#include "ColorMatrix.h"
#include "FEColorMatrix.h"
#include "FEComponentTransfer.h"
#include "FEDropShadow.h"
#include "FEGaussianBlur.h"
#include "FilterOperations.h"
#include "Logging.h"
#include "ReferencedSVGResources.h"
#include "RenderElement.h"
#include "SVGFilter.h"
#include "SVGFilterElement.h"
#include "SourceGraphic.h"

namespace WebCore {

RefPtr<CSSFilter> CSSFilter::create(RenderElement& renderer, const FilterOperations& operations, OptionSet<FilterRenderingMode> preferredFilterRenderingModes, const FloatSize& filterScale, const FloatRect& targetBoundingBox, const GraphicsContext& destinationContext)
{
    bool hasFilterThatMovesPixels = operations.hasFilterThatMovesPixels();
    bool hasFilterThatShouldBeRestrictedBySecurityOrigin = operations.hasFilterThatShouldBeRestrictedBySecurityOrigin();

    auto filter = adoptRef(*new CSSFilter(filterScale, hasFilterThatMovesPixels, hasFilterThatShouldBeRestrictedBySecurityOrigin));

    if (!filter->buildFilterFunctions(renderer, operations, preferredFilterRenderingModes, targetBoundingBox, destinationContext)) {
        LOG_WITH_STREAM(Filters, stream << "CSSFilter::create: failed to build filters " << operations);
        return nullptr;
    }

    LOG_WITH_STREAM(Filters, stream << "CSSFilter::create built filter " << filter.get() << " for " << operations);

    filter->setFilterRenderingModes(preferredFilterRenderingModes);
    return filter;
}

Ref<CSSFilter> CSSFilter::create(Vector<Ref<FilterFunction>>&& functions)
{
    return adoptRef(*new CSSFilter(WTFMove(functions)));
}

Ref<CSSFilter> CSSFilter::create(Vector<Ref<FilterFunction>>&& functions, OptionSet<FilterRenderingMode> filterRenderingModes, const FloatSize& filterScale, const FloatRect& filterRegion)
{
    Ref filter = adoptRef(*new CSSFilter(WTFMove(functions), filterScale, filterRegion));
    // Setting filter rendering modes cannot be moved to the constructor because it ends up
    // calling supportedFilterRenderingModes() which is a virtual function.
    filter->setFilterRenderingModes(filterRenderingModes);
    return filter;
}

CSSFilter::CSSFilter(const FloatSize& filterScale, bool hasFilterThatMovesPixels, bool hasFilterThatShouldBeRestrictedBySecurityOrigin)
    : Filter(Filter::Type::CSSFilter, filterScale)
    , m_hasFilterThatMovesPixels(hasFilterThatMovesPixels)
    , m_hasFilterThatShouldBeRestrictedBySecurityOrigin(hasFilterThatShouldBeRestrictedBySecurityOrigin)
{
}

CSSFilter::CSSFilter(Vector<Ref<FilterFunction>>&& functions)
    : Filter(Type::CSSFilter)
    , m_functions(WTFMove(functions))
{
}

CSSFilter::CSSFilter(Vector<Ref<FilterFunction>>&& functions, const FloatSize& filterScale, const FloatRect& filterRegion)
    : Filter(Type::CSSFilter, filterScale, filterRegion)
    , m_functions(WTFMove(functions))
{
    clampFilterRegionIfNeeded();
}

static RefPtr<FilterEffect> createBlurEffect(const BlurFilterOperation& blurOperation)
{
    float stdDeviation = floatValueForLength(blurOperation.stdDeviation(), 0);
    return FEGaussianBlur::create(stdDeviation, stdDeviation, EdgeModeType::None);
}

static RefPtr<FilterEffect> createBrightnessEffect(const BasicComponentTransferFilterOperation& componentTransferOperation)
{
    ComponentTransferFunction transferFunction;
    transferFunction.type = ComponentTransferType::FECOMPONENTTRANSFER_TYPE_LINEAR;
    transferFunction.slope = narrowPrecisionToFloat(componentTransferOperation.amount());
    transferFunction.intercept = 0;

    ComponentTransferFunction nullFunction;
    return FEComponentTransfer::create(transferFunction, transferFunction, transferFunction, nullFunction);
}

static RefPtr<FilterEffect> createContrastEffect(const BasicComponentTransferFilterOperation& componentTransferOperation)
{
    ComponentTransferFunction transferFunction;
    transferFunction.type = ComponentTransferType::FECOMPONENTTRANSFER_TYPE_LINEAR;
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
    auto grayscaleMatrix = grayscaleColorMatrix(colorMatrixOperation.amount());
    Vector<float> inputParameters {
        grayscaleMatrix.at(0, 0), grayscaleMatrix.at(0, 1), grayscaleMatrix.at(0, 2), 0, 0,
        grayscaleMatrix.at(1, 0), grayscaleMatrix.at(1, 1), grayscaleMatrix.at(1, 2), 0, 0,
        grayscaleMatrix.at(2, 0), grayscaleMatrix.at(2, 1), grayscaleMatrix.at(2, 2), 0, 0,
        0, 0, 0, 1, 0,
    };

    return FEColorMatrix::create(ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX, WTFMove(inputParameters));
}

static RefPtr<FilterEffect> createHueRotateEffect(const BasicColorMatrixFilterOperation& colorMatrixOperation)
{
    Vector<float> inputParameters { narrowPrecisionToFloat(colorMatrixOperation.amount()) };
    return FEColorMatrix::create(ColorMatrixType::FECOLORMATRIX_TYPE_HUEROTATE, WTFMove(inputParameters));
}

static RefPtr<FilterEffect> createInvertEffect(const BasicComponentTransferFilterOperation& componentTransferOperation)
{
    ComponentTransferFunction transferFunction;
    transferFunction.type = ComponentTransferType::FECOMPONENTTRANSFER_TYPE_LINEAR;
    float amount = narrowPrecisionToFloat(componentTransferOperation.amount());
    transferFunction.slope = 1 - 2 * amount;
    transferFunction.intercept = amount;

    ComponentTransferFunction nullFunction;
    return FEComponentTransfer::create(transferFunction, transferFunction, transferFunction, nullFunction);
}

static RefPtr<FilterEffect> createOpacityEffect(const BasicComponentTransferFilterOperation& componentTransferOperation)
{
    ComponentTransferFunction transferFunction;
    transferFunction.type = ComponentTransferType::FECOMPONENTTRANSFER_TYPE_LINEAR;
    float amount = narrowPrecisionToFloat(componentTransferOperation.amount());
    transferFunction.slope = amount;
    transferFunction.intercept = 0;

    ComponentTransferFunction nullFunction;
    return FEComponentTransfer::create(nullFunction, nullFunction, nullFunction, transferFunction);
}

static RefPtr<FilterEffect> createSaturateEffect(const BasicColorMatrixFilterOperation& colorMatrixOperation)
{
    Vector<float> inputParameters { narrowPrecisionToFloat(colorMatrixOperation.amount()) };
    return FEColorMatrix::create(ColorMatrixType::FECOLORMATRIX_TYPE_SATURATE, WTFMove(inputParameters));
}

static RefPtr<FilterEffect> createSepiaEffect(const BasicColorMatrixFilterOperation& colorMatrixOperation)
{
    auto sepiaMatrix = sepiaColorMatrix(colorMatrixOperation.amount());
    Vector<float> inputParameters {
        sepiaMatrix.at(0, 0), sepiaMatrix.at(0, 1), sepiaMatrix.at(0, 2), 0, 0,
        sepiaMatrix.at(1, 0), sepiaMatrix.at(1, 1), sepiaMatrix.at(1, 2), 0, 0,
        sepiaMatrix.at(2, 0), sepiaMatrix.at(2, 1), sepiaMatrix.at(2, 2), 0, 0,
        0, 0, 0, 1, 0,
    };

    return FEColorMatrix::create(ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX, WTFMove(inputParameters));
}

static RefPtr<SVGFilterElement> referenceFilterElement(const ReferenceFilterOperation& filterOperation, RenderElement& renderer)
{
    RefPtr filterElement = ReferencedSVGResources::referencedFilterElement(renderer.treeScopeForSVGReferences(), filterOperation);

    if (!filterElement) {
        LOG_WITH_STREAM(Filters, stream << " buildReferenceFilter: failed to find filter renderer, adding pending resource " << filterOperation.fragment());
        // Although we did not find the referenced filter, it might exist later in the document.
        // FIXME: This skips anonymous RenderObjects. <https://webkit.org/b/131085>
        // FIXME: Unclear if this does anything.
        return nullptr;
    }

    return filterElement;
}

static bool isIdentityReferenceFilter(const ReferenceFilterOperation& filterOperation, RenderElement& renderer)
{
    RefPtr filterElement = referenceFilterElement(filterOperation, renderer);
    if (!filterElement)
        return false;

    return SVGFilter::isIdentity(*filterElement);
}

static IntOutsets calculateReferenceFilterOutsets(const ReferenceFilterOperation& filterOperation, RenderElement& renderer, const FloatRect& targetBoundingBox)
{
    RefPtr filterElement = referenceFilterElement(filterOperation, renderer);
    if (!filterElement)
        return { };

    return SVGFilter::calculateOutsets(*filterElement, targetBoundingBox);
}

static RefPtr<SVGFilter> createReferenceFilter(CSSFilter& filter, const ReferenceFilterOperation& filterOperation, RenderElement& renderer, OptionSet<FilterRenderingMode> preferredFilterRenderingModes, const FloatRect& targetBoundingBox, const GraphicsContext& destinationContext)
{
    RefPtr filterElement = referenceFilterElement(filterOperation, renderer);
    if (!filterElement)
        return nullptr;

    auto filterRegion = SVGLengthContext::resolveRectangle<SVGFilterElement>(filterElement.get(), filterElement->filterUnits(), targetBoundingBox);

    return SVGFilter::create(*filterElement, preferredFilterRenderingModes, filter.filterScale(), filterRegion, targetBoundingBox, destinationContext);
}

bool CSSFilter::buildFilterFunctions(RenderElement& renderer, const FilterOperations& operations, OptionSet<FilterRenderingMode> preferredFilterRenderingModes, const FloatRect& targetBoundingBox, const GraphicsContext& destinationContext)
{
    RefPtr<FilterFunction> function;

    for (auto& operation : operations) {
        switch (operation->type()) {
        case FilterOperation::Type::AppleInvertLightness:
            ASSERT_NOT_REACHED(); // AppleInvertLightness is only used in -apple-color-filter.
            break;

        case FilterOperation::Type::Blur:
            function = createBlurEffect(uncheckedDowncast<BlurFilterOperation>(operation));
            break;

        case FilterOperation::Type::Brightness:
            function = createBrightnessEffect(downcast<BasicComponentTransferFilterOperation>(operation));
            break;

        case FilterOperation::Type::Contrast:
            function = createContrastEffect(downcast<BasicComponentTransferFilterOperation>(operation));
            break;

        case FilterOperation::Type::DropShadow:
            function = createDropShadowEffect(uncheckedDowncast<DropShadowFilterOperation>(operation));
            break;

        case FilterOperation::Type::Grayscale:
            function = createGrayScaleEffect(downcast<BasicColorMatrixFilterOperation>(operation));
            break;

        case FilterOperation::Type::HueRotate:
            function = createHueRotateEffect(downcast<BasicColorMatrixFilterOperation>(operation));
            break;

        case FilterOperation::Type::Invert:
            function = createInvertEffect(downcast<BasicComponentTransferFilterOperation>(operation));
            break;

        case FilterOperation::Type::Opacity:
            function = createOpacityEffect(downcast<BasicComponentTransferFilterOperation>(operation));
            break;

        case FilterOperation::Type::Saturate:
            function = createSaturateEffect(downcast<BasicColorMatrixFilterOperation>(operation));
            break;

        case FilterOperation::Type::Sepia:
            function = createSepiaEffect(downcast<BasicColorMatrixFilterOperation>(operation));
            break;

        case FilterOperation::Type::Reference:
            function = createReferenceFilter(*this, uncheckedDowncast<ReferenceFilterOperation>(operation), renderer, preferredFilterRenderingModes, targetBoundingBox, destinationContext);
            break;

        default:
            break;
        }

        if (!function)
            continue;

        if (m_functions.isEmpty())
            m_functions.append(SourceGraphic::create());

        m_functions.append(function.releaseNonNull());
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

        if (RefPtr filter = dynamicDowncast<SVGFilter>(function))
            effects.appendVector(filter->effectsOfType(filterType));
    }

    return effects;
}

OptionSet<FilterRenderingMode> CSSFilter::supportedFilterRenderingModes() const
{
    OptionSet<FilterRenderingMode> modes = allFilterRenderingModes;

    for (auto& function : m_functions)
        modes = modes & function->supportedFilterRenderingModes();

    ASSERT(modes);
    return modes;
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

FilterStyleVector CSSFilter::createFilterStyles(GraphicsContext& context, const FilterStyle& sourceStyle) const
{
    ASSERT(supportedFilterRenderingModes().contains(FilterRenderingMode::GraphicsContext));

    FilterStyleVector styles;
    FilterStyle lastStyle = sourceStyle;

    for (auto& function : m_functions) {
        if (function->filterType() == FilterEffect::Type::SourceGraphic)
            continue;

        auto result = function->createFilterStyles(context, *this, lastStyle);
        if (result.isEmpty())
            return { };

        lastStyle = result.last();
        styles.appendVector(WTFMove(result));
    }

    return styles;
}

void CSSFilter::setFilterRegion(const FloatRect& filterRegion)
{
    Filter::setFilterRegion(filterRegion);
    clampFilterRegionIfNeeded();
}

bool CSSFilter::isIdentity(RenderElement& renderer, const FilterOperations& operations)
{
    if (operations.hasFilterThatShouldBeRestrictedBySecurityOrigin())
        return false;

    for (auto& operation : operations) {
        if (RefPtr referenceOperation = dynamicDowncast<ReferenceFilterOperation>(operation)) {
            if (!isIdentityReferenceFilter(*referenceOperation, renderer))
                return false;
            continue;
        }

        if (!operation->isIdentity())
            return false;
    }

    return true;
}

IntOutsets CSSFilter::calculateOutsets(RenderElement& renderer, const FilterOperations& operations, const FloatRect& targetBoundingBox)
{
    IntOutsets outsets;

    for (auto& operation : operations) {
        if (RefPtr referenceOperation = dynamicDowncast<ReferenceFilterOperation>(operation)) {
            outsets += calculateReferenceFilterOutsets(*referenceOperation, renderer, targetBoundingBox);
            continue;
        }

        outsets += operation->outsets();
    }

    return outsets;
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
