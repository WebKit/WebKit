/*
 * Copyright (C) 2011-2024 Apple Inc. All rights reserved.
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
#include "FilterOperations.h"

#include "AnimationUtilities.h"
#include "FEGaussianBlur.h"
#include "ImageBuffer.h"
#include "IntSize.h"
#include "LengthFunctions.h"
#include <ranges>
#include <wtf/text/TextStream.h>

namespace WebCore {

FilterOperations::FilterOperations(Vector<Ref<FilterOperation>>&& operations)
    : m_operations(WTFMove(operations))
{
}

bool FilterOperations::operator==(const FilterOperations& other) const
{
    static_assert(std::ranges::sized_range<decltype(m_operations)>);

    return std::ranges::equal(m_operations, other.m_operations, [](auto& a, auto& b) { return a.get() == b.get(); });
}

bool FilterOperations::operationsMatch(const FilterOperations& other) const
{
    static_assert(std::ranges::sized_range<decltype(m_operations)>);

    return std::ranges::equal(m_operations, other.m_operations, [](auto& a, auto& b) { return a->isSameType(b.get()); });
}

bool FilterOperations::hasReferenceFilter() const
{
    return hasFilterOfType<FilterOperation::Type::Reference>();
}

bool FilterOperations::isReferenceFilter() const
{
    return m_operations.size() == 1 && m_operations[0]->type() == FilterOperation::Type::Reference;
}

IntOutsets FilterOperations::outsets() const
{
    IntOutsets totalOutsets;
    for (auto& operation : m_operations) {
        switch (operation->type()) {
        case FilterOperation::Type::Blur: {
            auto& blurOperation = downcast<BlurFilterOperation>(operation.get());
            float stdDeviation = floatValueForLength(blurOperation.stdDeviation(), 0);
            IntSize outsetSize = FEGaussianBlur::calculateOutsetSize({ stdDeviation, stdDeviation });
            IntOutsets outsets(outsetSize.height(), outsetSize.width(), outsetSize.height(), outsetSize.width());
            totalOutsets += outsets;
            break;
        }
        case FilterOperation::Type::DropShadow: {
            auto& dropShadowOperation = downcast<DropShadowFilterOperation>(operation.get());
            float stdDeviation = dropShadowOperation.stdDeviation();
            IntSize outsetSize = FEGaussianBlur::calculateOutsetSize({ stdDeviation, stdDeviation });
            
            int top = std::max(0, outsetSize.height() - dropShadowOperation.y());
            int right = std::max(0, outsetSize.width() + dropShadowOperation.x());
            int bottom = std::max(0, outsetSize.height() + dropShadowOperation.y());
            int left = std::max(0, outsetSize.width() - dropShadowOperation.x());
            
            auto outsets = IntOutsets { top, right, bottom, left };
            totalOutsets += outsets;
            break;
        }
        case FilterOperation::Type::Reference:
            ASSERT_NOT_REACHED();
            break;
        default:
            break;
        }
    }
    return totalOutsets;
}

bool FilterOperations::transformColor(Color& color) const
{
    if (isEmpty() || !color.isValid())
        return false;
    // Color filter does not apply to semantic CSS colors (like "Windowframe").
    if (color.isSemantic())
        return false;

    auto sRGBAColor = color.toColorTypeLossy<SRGBA<float>>();

    for (auto& operation : m_operations) {
        if (!operation->transformColor(sRGBAColor))
            return false;
    }

    color = convertColor<SRGBA<uint8_t>>(sRGBAColor);
    return true;
}

bool FilterOperations::inverseTransformColor(Color& color) const
{
    if (isEmpty() || !color.isValid())
        return false;
    // Color filter does not apply to semantic CSS colors (like "Windowframe").
    if (color.isSemantic())
        return false;

    auto sRGBAColor = color.toColorTypeLossy<SRGBA<float>>();

    for (auto& operation : m_operations) {
        if (!operation->inverseTransformColor(sRGBAColor))
            return false;
    }

    color = convertColor<SRGBA<uint8_t>>(sRGBAColor);
    return true;
}

bool FilterOperations::hasFilterThatAffectsOpacity() const
{
    return std::ranges::any_of(m_operations, [](auto& op) { return op->affectsOpacity(); });
}

bool FilterOperations::hasFilterThatMovesPixels() const
{
    return std::ranges::any_of(m_operations, [](auto& op) { return op->movesPixels(); });
}

bool FilterOperations::hasFilterThatShouldBeRestrictedBySecurityOrigin() const
{
    return std::ranges::any_of(m_operations, [](auto& op) { return op->shouldBeRestrictedBySecurityOrigin(); });
}

bool FilterOperations::canInterpolate(const FilterOperations& to, CompositeOperation compositeOperation) const
{
    // https://drafts.fxtf.org/filter-effects/#interpolation-of-filters

    // We can't interpolate between lists if a reference filter is involved.
    if (hasReferenceFilter() || to.hasReferenceFilter())
        return false;

    // Additive and accumulative composition will always yield interpolation.
    if (compositeOperation != CompositeOperation::Replace)
        return true;

    // Provided the two filter lists have a shared set of initial primitives, we will be able to interpolate.
    // Note that this means that if either list is empty, interpolation is supported.
    auto numItems = std::min(size(), to.size());
    for (size_t i = 0; i < numItems; ++i) {
        auto* fromOperation = at(i);
        auto* toOperation = to.at(i);
        if (!!fromOperation != !!toOperation)
            return false;
        if (fromOperation && toOperation && fromOperation->type() != toOperation->type())
            return false;
    }

    return true;
}

FilterOperations FilterOperations::blend(const FilterOperations& to, const BlendingContext& context) const
{
    if (context.compositeOperation == CompositeOperation::Add) {
        ASSERT(context.progress == 1.0);

        Vector<Ref<FilterOperation>> operations;
        operations.reserveInitialCapacity(size() + to.size());

        operations.appendVector(m_operations);
        operations.appendVector(to.m_operations);

        return FilterOperations { WTFMove(operations) };
    }

    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : *this;
    }

    auto fromSize = m_operations.size();
    auto toSize = to.m_operations.size();
    auto size = std::max(fromSize, toSize);

    Vector<Ref<FilterOperation>> operations;
    operations.reserveInitialCapacity(size);

    for (size_t i = 0; i < size; ++i) {
        RefPtr<FilterOperation> fromOp = (i < fromSize) ? m_operations[i].ptr() : nullptr;
        RefPtr<FilterOperation> toOp = (i < toSize) ? to.m_operations[i].ptr() : nullptr;

        RefPtr<FilterOperation> blendedOp;
        if (toOp)
            blendedOp = toOp->blend(fromOp.get(), context);
        else if (fromOp)
            blendedOp = fromOp->blend(nullptr, context, true);

        if (blendedOp)
            operations.append(blendedOp.releaseNonNull());
        else {
            auto identityOp = PassthroughFilterOperation::create();
            if (context.progress > 0.5) {
                if (toOp)
                    operations.append(toOp.releaseNonNull());
                else
                    operations.append(WTFMove(identityOp));
            } else {
                if (fromOp)
                    operations.append(fromOp.releaseNonNull());
                else
                    operations.append(WTFMove(identityOp));
            }
        }
    }

    return FilterOperations { WTFMove(operations) };
}

TextStream& operator<<(TextStream& ts, const FilterOperations& filters)
{
    return ts << filters.m_operations;
}

} // namespace WebCore
