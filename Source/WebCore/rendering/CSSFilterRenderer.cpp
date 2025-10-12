/*
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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
#include "CSSFilterRenderer.h"

#include "ColorMatrix.h"
#include "DropShadowFilterOperationWithStyleColor.h"
#include "FEColorMatrix.h"
#include "FEComponentTransfer.h"
#include "FEDropShadow.h"
#include "FEGaussianBlur.h"
#include "FilterOperations.h"
#include "Logging.h"
#include "ReferenceFilterOperation.h"
#include "ReferencedSVGResources.h"
#include "RenderElement.h"
#include "RenderObjectInlines.h"
#include "SVGFilterElement.h"
#include "SVGFilterRenderer.h"
#include "SourceGraphic.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CSSFilterRenderer);

RefPtr<CSSFilterRenderer> CSSFilterRenderer::createGeneric(RenderElement& renderer, const auto& filter, OptionSet<FilterRenderingMode> preferredFilterRenderingModes, const FloatSize& filterScale, const FloatRect& targetBoundingBox, const GraphicsContext& destinationContext)
{
    bool hasFilterThatMovesPixels = filter.hasFilterThatMovesPixels();
    bool hasFilterThatShouldBeRestrictedBySecurityOrigin = filter.hasFilterThatShouldBeRestrictedBySecurityOrigin();

    auto filterRenderer = adoptRef(*new CSSFilterRenderer(filterScale, hasFilterThatMovesPixels, hasFilterThatShouldBeRestrictedBySecurityOrigin));

    if (!filterRenderer->buildFilterFunctions(renderer, filter, preferredFilterRenderingModes, targetBoundingBox, destinationContext)) {
        LOG_WITH_STREAM(Filters, stream << "CSSFilterRenderer::create: failed to build filters " << filter);
        return nullptr;
    }

    filterRenderer->setFilterRenderingModes(preferredFilterRenderingModes);

    LOG_WITH_STREAM(Filters, stream << "CSSFilterRenderer::create built filter " << filterRenderer.get() << " for " << filter << " supported rendering mode(s) " << filterRenderer->filterRenderingModes());

    return filterRenderer;
}


RefPtr<CSSFilterRenderer> CSSFilterRenderer::create(RenderElement& renderer, const Style::Filter& filter, OptionSet<FilterRenderingMode> preferredFilterRenderingModes, const FloatSize& filterScale, const FloatRect& targetBoundingBox, const GraphicsContext& destinationContext)
{
    return createGeneric(renderer, filter, preferredFilterRenderingModes, filterScale, targetBoundingBox, destinationContext);
}

RefPtr<CSSFilterRenderer> CSSFilterRenderer::create(RenderElement& renderer, const FilterOperations& operations, OptionSet<FilterRenderingMode> preferredFilterRenderingModes, const FloatSize& filterScale, const FloatRect& targetBoundingBox, const GraphicsContext& destinationContext)
{
    return createGeneric(renderer, operations, preferredFilterRenderingModes, filterScale, targetBoundingBox, destinationContext);
}

Ref<CSSFilterRenderer> CSSFilterRenderer::create(Vector<Ref<FilterFunction>>&& functions)
{
    return adoptRef(*new CSSFilterRenderer(WTFMove(functions)));
}

Ref<CSSFilterRenderer> CSSFilterRenderer::create(Vector<Ref<FilterFunction>>&& functions, OptionSet<FilterRenderingMode> filterRenderingModes, const FloatSize& filterScale, const FloatRect& filterRegion)
{
    Ref filter = adoptRef(*new CSSFilterRenderer(WTFMove(functions), filterScale, filterRegion));
    // Setting filter rendering modes cannot be moved to the constructor because it ends up
    // calling supportedFilterRenderingModes() which is a virtual function.
    filter->setFilterRenderingModes(filterRenderingModes);
    return filter;
}

CSSFilterRenderer::CSSFilterRenderer(const FloatSize& filterScale, bool hasFilterThatMovesPixels, bool hasFilterThatShouldBeRestrictedBySecurityOrigin)
    : Filter(Filter::Type::CSSFilterRenderer, filterScale)
    , m_hasFilterThatMovesPixels(hasFilterThatMovesPixels)
    , m_hasFilterThatShouldBeRestrictedBySecurityOrigin(hasFilterThatShouldBeRestrictedBySecurityOrigin)
{
}

CSSFilterRenderer::CSSFilterRenderer(Vector<Ref<FilterFunction>>&& functions)
    : Filter(Type::CSSFilterRenderer)
    , m_functions(WTFMove(functions))
{
}

CSSFilterRenderer::CSSFilterRenderer(Vector<Ref<FilterFunction>>&& functions, const FloatSize& filterScale, const FloatRect& filterRegion)
    : Filter(Type::CSSFilterRenderer, filterScale, filterRegion)
    , m_functions(WTFMove(functions))
{
    clampFilterRegionIfNeeded();
}

static RefPtr<FilterEffect> createBlurEffect(const BlurFilterOperation& blurOperation)
{
    float stdDeviation = blurOperation.stdDeviation();
    return FEGaussianBlur::create(stdDeviation, stdDeviation, EdgeModeType::None);
}

static RefPtr<FilterEffect> createBrightnessEffect(const BasicComponentTransferFilterOperation& componentTransferOperation)
{
    float amount = narrowPrecisionToFloat(componentTransferOperation.amount());
    ColorMatrix<5, 4> brightnessMatrix = brightnessColorMatrix(amount);
    return FEColorMatrix::create(ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX, brightnessMatrix.data());
}

static RefPtr<FilterEffect> createContrastEffect(const BasicComponentTransferFilterOperation& componentTransferOperation)
{
    float amount = narrowPrecisionToFloat(componentTransferOperation.amount());
    ColorMatrix<5, 4> contrastMatrix = contrastColorMatrix(amount);
    return FEColorMatrix::create(ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX, contrastMatrix.data());
}

static RefPtr<FilterEffect> createDropShadowEffect(const DropShadowFilterOperation& dropShadowOperation)
{
    float std = dropShadowOperation.stdDeviation();
    return FEDropShadow::create(std, std, dropShadowOperation.x(), dropShadowOperation.y(), dropShadowOperation.color(), 1);
}

static RefPtr<FilterEffect> createDropShadowEffect(const Style::DropShadowFilterOperationWithStyleColor& dropShadowOperation, const RenderStyle& style)
{
    float std = dropShadowOperation.stdDeviation();
    return FEDropShadow::create(std, std, dropShadowOperation.x(), dropShadowOperation.y(), style.colorResolvingCurrentColor(dropShadowOperation.styleColor()), 1);
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
    float amount = narrowPrecisionToFloat(componentTransferOperation.amount());
    ColorMatrix<5, 4> invertMatrix = invertColorMatrix(amount);
    return FEColorMatrix::create(ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX, invertMatrix.data());
}

static RefPtr<FilterEffect> createOpacityEffect(const BasicComponentTransferFilterOperation& componentTransferOperation)
{
    float amount = narrowPrecisionToFloat(componentTransferOperation.amount());
    ColorMatrix<5, 4> opacityMatrix = opacityColorMatrix(amount);
    return FEColorMatrix::create(ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX, opacityMatrix.data());
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

static RefPtr<SVGFilterElement> referenceFilterElement(const Style::ReferenceFilterOperation& filterOperation, RenderElement& renderer)
{
    RefPtr filterElement = ReferencedSVGResources::referencedFilterElement(renderer.protectedTreeScopeForSVGReferences(), filterOperation);

    if (!filterElement) {
        LOG_WITH_STREAM(Filters, stream << " buildReferenceFilter: failed to find filter renderer, adding pending resource " << filterOperation.url());
        // Although we did not find the referenced filter, it might exist later in the document.
        // FIXME: This skips anonymous RenderObjects. <https://webkit.org/b/131085>
        // FIXME: Unclear if this does anything.
        return nullptr;
    }

    return filterElement;
}

static bool isIdentityReferenceFilter(const Style::ReferenceFilterOperation& filterOperation, RenderElement& renderer)
{
    RefPtr filterElement = referenceFilterElement(filterOperation, renderer);
    if (!filterElement)
        return false;

    return SVGFilterRenderer::isIdentity(*filterElement);
}

static IntOutsets calculateReferenceFilterOutsets(const Style::ReferenceFilterOperation& filterOperation, RenderElement& renderer, const FloatRect& targetBoundingBox)
{
    RefPtr filterElement = referenceFilterElement(filterOperation, renderer);
    if (!filterElement)
        return { };

    return SVGFilterRenderer::calculateOutsets(*filterElement, targetBoundingBox);
}

static RefPtr<SVGFilterRenderer> createReferenceFilter(CSSFilterRenderer& filter, const Style::ReferenceFilterOperation& filterOperation, RenderElement& renderer, OptionSet<FilterRenderingMode> preferredFilterRenderingModes, const FloatRect& targetBoundingBox, const GraphicsContext& destinationContext)
{
    RefPtr filterElement = referenceFilterElement(filterOperation, renderer);
    if (!filterElement)
        return nullptr;

    auto filterRegion = SVGLengthContext::resolveRectangle<SVGFilterElement>(filterElement.get(), filterElement->filterUnits(), targetBoundingBox);

    return SVGFilterRenderer::create(*filterElement, preferredFilterRenderingModes, filter.filterScale(), filterRegion, targetBoundingBox, destinationContext);
}

RefPtr<FilterFunction> CSSFilterRenderer::buildFilterFunction(RenderElement& renderer, const FilterOperation& operation, OptionSet<FilterRenderingMode> preferredFilterRenderingModes, const FloatRect& targetBoundingBox, const GraphicsContext& destinationContext)
{
    switch (operation.type()) {
    case FilterOperation::Type::AppleInvertLightness:
        ASSERT_NOT_REACHED(); // AppleInvertLightness is only used in -apple-color-filter.
        break;

    case FilterOperation::Type::Blur:
        return createBlurEffect(uncheckedDowncast<BlurFilterOperation>(operation));

    case FilterOperation::Type::Brightness:
        return createBrightnessEffect(downcast<BasicComponentTransferFilterOperation>(operation));

    case FilterOperation::Type::Contrast:
        return createContrastEffect(downcast<BasicComponentTransferFilterOperation>(operation));

    case FilterOperation::Type::DropShadow:
        return createDropShadowEffect(uncheckedDowncast<DropShadowFilterOperation>(operation));

    case FilterOperation::Type::DropShadowWithStyleColor:
        return createDropShadowEffect(uncheckedDowncast<Style::DropShadowFilterOperationWithStyleColor>(operation), renderer.style());

    case FilterOperation::Type::Grayscale:
        return createGrayScaleEffect(downcast<BasicColorMatrixFilterOperation>(operation));

    case FilterOperation::Type::HueRotate:
        return createHueRotateEffect(downcast<BasicColorMatrixFilterOperation>(operation));

    case FilterOperation::Type::Invert:
        return createInvertEffect(downcast<BasicComponentTransferFilterOperation>(operation));

    case FilterOperation::Type::Opacity:
        return createOpacityEffect(downcast<BasicComponentTransferFilterOperation>(operation));

    case FilterOperation::Type::Saturate:
        return createSaturateEffect(downcast<BasicColorMatrixFilterOperation>(operation));

    case FilterOperation::Type::Sepia:
        return createSepiaEffect(downcast<BasicColorMatrixFilterOperation>(operation));

    case FilterOperation::Type::Reference:
        return createReferenceFilter(*this, uncheckedDowncast<Style::ReferenceFilterOperation>(operation), renderer, preferredFilterRenderingModes, targetBoundingBox, destinationContext);

    default:
        break;
    }

    return nullptr;
}

bool CSSFilterRenderer::buildFilterFunctions(RenderElement& renderer, const auto& filter, OptionSet<FilterRenderingMode> preferredFilterRenderingModes, const FloatRect& targetBoundingBox, const GraphicsContext& destinationContext)
{
    for (auto& value : filter) {
        auto function = buildFilterFunction(renderer, value.get(), preferredFilterRenderingModes, targetBoundingBox, destinationContext);
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

FilterEffectVector CSSFilterRenderer::effectsOfType(FilterFunction::Type filterType) const
{
    FilterEffectVector effects;

    for (auto& function : m_functions) {
        if (function->filterType() == filterType) {
            effects.append({ downcast<FilterEffect>(function.get()) });
            continue;
        }

        if (RefPtr filter = dynamicDowncast<SVGFilterRenderer>(function))
            effects.appendVector(filter->effectsOfType(filterType));
    }

    return effects;
}

OptionSet<FilterRenderingMode> CSSFilterRenderer::supportedFilterRenderingModes(OptionSet<FilterRenderingMode> preferredFilterRenderingModes) const
{
    OptionSet<FilterRenderingMode> modes = allFilterRenderingModes;

    for (auto& function : m_functions)
        modes = modes & function->supportedFilterRenderingModes(preferredFilterRenderingModes);

    ASSERT(modes);
    return modes;
}

RefPtr<FilterImage> CSSFilterRenderer::apply(FilterImage* sourceImage, FilterResults& results)
{
    ASSERT(filterRenderingModes().contains(FilterRenderingMode::Software));

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

FilterStyleVector CSSFilterRenderer::createFilterStyles(GraphicsContext& context, const FilterStyle& sourceStyle) const
{
    ASSERT(filterRenderingModes().contains(FilterRenderingMode::GraphicsContext));

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

void CSSFilterRenderer::setFilterRegion(const FloatRect& filterRegion)
{
    Filter::setFilterRegion(filterRegion);
    clampFilterRegionIfNeeded();
}

bool CSSFilterRenderer::isIdentity(RenderElement& renderer, const Style::Filter& filter)
{
    if (filter.hasFilterThatShouldBeRestrictedBySecurityOrigin())
        return false;

    for (auto& value : filter) {
        Ref operation = value.value;
        if (RefPtr referenceOperation = dynamicDowncast<Style::ReferenceFilterOperation>(operation)) {
            if (!isIdentityReferenceFilter(*referenceOperation, renderer))
                return false;
            continue;
        }

        if (!operation->isIdentity())
            return false;
    }

    return true;
}
bool CSSFilterRenderer::isIdentity(RenderElement& renderer, const FilterOperations& operations)
{
    if (operations.hasFilterThatShouldBeRestrictedBySecurityOrigin())
        return false;

    for (auto& operation : operations) {
        if (RefPtr referenceOperation = dynamicDowncast<Style::ReferenceFilterOperation>(operation)) {
            if (!isIdentityReferenceFilter(*referenceOperation, renderer))
                return false;
            continue;
        }

        if (!operation->isIdentity())
            return false;
    }

    return true;
}

IntOutsets CSSFilterRenderer::calculateOutsets(RenderElement& renderer, const Style::Filter& filter, const FloatRect& targetBoundingBox)
{
    IntOutsets outsets;

    for (auto& value : filter) {
        Ref operation = value.value;
        if (RefPtr referenceOperation = dynamicDowncast<Style::ReferenceFilterOperation>(operation)) {
            outsets += calculateReferenceFilterOutsets(*referenceOperation, renderer, targetBoundingBox);
            continue;
        }

        outsets += operation->outsets();
    }

    return outsets;
}

IntOutsets CSSFilterRenderer::calculateOutsets(RenderElement& renderer, const FilterOperations& operations, const FloatRect& targetBoundingBox)
{
    IntOutsets outsets;

    for (auto& operation : operations) {
        if (RefPtr referenceOperation = dynamicDowncast<Style::ReferenceFilterOperation>(operation)) {
            outsets += calculateReferenceFilterOutsets(*referenceOperation, renderer, targetBoundingBox);
            continue;
        }

        outsets += operation->outsets();
    }

    return outsets;
}

TextStream& CSSFilterRenderer::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
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
