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

#include "FilterOperations.h"
#include "Logging.h"
#include "ReferencedSVGResources.h"
#include "RenderElement.h"
#include "SVGFilter.h"
#include "SVGFilterElement.h"
#include "SourceGraphic.h"

namespace WebCore {

RefPtr<CSSFilter> CSSFilter::create(RenderElement& renderer, const FilterOperations& operations, RenderingMode renderingMode, const FloatSize& filterScale, ClipOperation clipOperation, const FloatRect& targetBoundingBox, const GraphicsContext& destinationContext)
{
    bool hasFilterThatMovesPixels = operations.hasFilterThatMovesPixels();
    bool hasFilterThatShouldBeRestrictedBySecurityOrigin = operations.hasFilterThatShouldBeRestrictedBySecurityOrigin();

    auto filter = adoptRef(*new CSSFilter(renderingMode, filterScale, clipOperation, hasFilterThatMovesPixels, hasFilterThatShouldBeRestrictedBySecurityOrigin));

    if (!filter->buildFilterFunctions(renderer, operations, targetBoundingBox, destinationContext))
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

static SVGFilterElement* referenceFilterElement(const ReferenceFilterOperation& filterOperation, RenderElement& renderer)
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

    return filterElement;
}

static bool isIdentityReferenceFilter(const ReferenceFilterOperation& filterOperation, RenderElement& renderer)
{
    auto filterElement = referenceFilterElement(filterOperation, renderer);
    if (!filterElement)
        return false;

    return SVGFilter::isIdentity(*filterElement);
}

static IntOutsets calculateReferenceFilterOutsets(const ReferenceFilterOperation& filterOperation, RenderElement& renderer, const FloatRect& targetBoundingBox)
{
    auto filterElement = referenceFilterElement(filterOperation, renderer);
    if (!filterElement)
        return { };

    return SVGFilter::calculateOutsets(*filterElement, targetBoundingBox);
}

static RefPtr<SVGFilter> createReferenceFilter(CSSFilter& filter, const ReferenceFilterOperation& filterOperation, RenderElement& renderer, const FloatRect& targetBoundingBox, const GraphicsContext& destinationContext)
{
    auto filterElement = referenceFilterElement(filterOperation, renderer);
    if (!filterElement)
        return nullptr;

    auto filterRegion = SVGLengthContext::resolveRectangle<SVGFilterElement>(filterElement, filterElement->filterUnits(), targetBoundingBox);

    return SVGFilter::create(*filterElement, filter.renderingMode(), filter.filterScale(), filter.clipOperation(), filterRegion, targetBoundingBox, destinationContext);
}

bool CSSFilter::buildFilterFunctions(RenderElement& renderer, const FilterOperations& operations, const FloatRect& targetBoundingBox, const GraphicsContext& destinationContext)
{
    RefPtr<FilterFunction> function;

    for (auto& operation : operations.operations()) {
        if (operation->type() == FilterOperation::APPLE_INVERT_LIGHTNESS) {
            ASSERT_NOT_REACHED(); // APPLE_INVERT_LIGHTNESS is only used in -apple-color-filter.
            continue;
        }

        if (operation->type() == FilterOperation::REFERENCE)
            function = createReferenceFilter(*this, downcast<ReferenceFilterOperation>(*operation), renderer, targetBoundingBox, destinationContext);
        else
            function = operation->createFilterFunction(*this);

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

bool CSSFilter::isIdentity(RenderElement& renderer, const FilterOperations& operations)
{
    if (operations.hasFilterThatShouldBeRestrictedBySecurityOrigin())
        return false;

    for (auto& operation : operations.operations()) {
        if (operation->type() == FilterOperation::REFERENCE) {
            if (!isIdentityReferenceFilter(downcast<ReferenceFilterOperation>(*operation), renderer))
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

    for (auto& operation : operations.operations()) {
        if (operation->type() == FilterOperation::REFERENCE) {
            outsets += calculateReferenceFilterOutsets(downcast<ReferenceFilterOperation>(*operation), renderer, targetBoundingBox);
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
