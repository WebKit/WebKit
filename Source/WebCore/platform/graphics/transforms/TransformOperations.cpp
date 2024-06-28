/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "TransformOperations.h"

#include "AnimationUtilities.h"
#include "Matrix3DTransformOperation.h"
#include <algorithm>
#include <ranges>
#include <wtf/text/TextStream.h>

namespace WebCore {

TransformOperations::TransformOperations(Ref<TransformOperation>&& operation)
    : m_operations({ WTFMove(operation) })
{
}

TransformOperations::TransformOperations(Vector<Ref<TransformOperation>>&& operations)
    : m_operations(WTFMove(operations))
{
}

bool TransformOperations::operator==(const TransformOperations& o) const
{
    static_assert(std::ranges::sized_range<decltype(m_operations)>);

    return std::ranges::equal(m_operations, o.m_operations, [](auto& a, auto& b) { return a.get() == b.get(); });
}

TransformOperations TransformOperations::clone() const
{
    return TransformOperations { m_operations.map([](const auto& op) { return op->clone(); }) };
}

TransformOperations TransformOperations::selfOrCopyWithResolvedCalculatedValues(const FloatSize& size) const
{
    return TransformOperations { m_operations.map([&size](const auto& op) { return op->selfOrCopyWithResolvedCalculatedValues(size); }) };
}

void TransformOperations::apply(TransformationMatrix& matrix, const FloatSize& size, unsigned start) const
{
    for (unsigned i = start; i < m_operations.size(); ++i)
        m_operations[i]->apply(matrix, size);
}

bool TransformOperations::has3DOperation() const
{
    return std::ranges::any_of(m_operations, [](auto& op) { return op->is3DOperation(); });
}

bool TransformOperations::isRepresentableIn2D() const
{
    return std::ranges::all_of(m_operations, [](auto& op) { return op->isRepresentableIn2D(); });
}

bool TransformOperations::affectedByTransformOrigin() const
{
    return std::ranges::any_of(m_operations, [](auto& op) { return op->isAffectedByTransformOrigin(); });
}

bool TransformOperations::isInvertible(const LayoutSize& size) const
{
    TransformationMatrix transform;
    apply(transform, size);
    return transform.isInvertible();
}

bool TransformOperations::containsNonInvertibleMatrix(const LayoutSize& boxSize) const
{
    return (hasTransformOfType<TransformOperation::Type::Matrix>() || hasTransformOfType<TransformOperation::Type::Matrix3D>()) && !isInvertible(boxSize);
}

bool TransformOperations::shouldFallBackToDiscreteAnimation(const TransformOperations& from, const LayoutSize& boxSize) const
{
    return from.containsNonInvertibleMatrix(boxSize) || containsNonInvertibleMatrix(boxSize);
}

TransformOperations TransformOperations::blend(const TransformOperations& from, const BlendingContext& context, const LayoutSize& boxSize, std::optional<unsigned> prefixLength) const
{
    if (shouldFallBackToDiscreteAnimation(from, boxSize))
        return TransformOperations { createBlendedMatrixOperationFromOperationsSuffix(from, 0, context, boxSize) };

    unsigned fromOperationCount = from.size();
    unsigned toOperationCount = size();
    unsigned maxOperationCount = std::max(fromOperationCount, toOperationCount);

    Vector<Ref<TransformOperation>> operations;
    operations.reserveInitialCapacity(maxOperationCount);

    for (unsigned i = 0; i < maxOperationCount; i++) {
        RefPtr<TransformOperation> fromOperation = (i < fromOperationCount) ? from.m_operations[i].ptr() : nullptr;
        RefPtr<TransformOperation> toOperation = (i < toOperationCount) ? m_operations[i].ptr() : nullptr;

        // If either of the transform list is empty, then we should not attempt to do a matrix blend.
        if (fromOperationCount && toOperationCount) {
            if ((prefixLength && i >= *prefixLength) || (fromOperation && toOperation && !fromOperation->sharedPrimitiveType(toOperation.get()))) {
                operations.append(createBlendedMatrixOperationFromOperationsSuffix(from, i, context, boxSize));
                operations.shrinkToFit();

                return TransformOperations { WTFMove(operations) };
            }
        }

        RefPtr<TransformOperation> blendedOperation;
        if (fromOperation && toOperation)
            blendedOperation = toOperation->blend(fromOperation.get(), context);
        else if (!fromOperation)
            blendedOperation = toOperation->blend(nullptr, 1 - context.progress, true);
        else if (!toOperation)
            blendedOperation = fromOperation->blend(nullptr, context, true);

        // We should have exited early above if the fromOperation and toOperation didn't share a transform
        // function primitive, so blending the two operations should always yield a result.
        ASSERT(blendedOperation);
        operations.append(blendedOperation.releaseNonNull());
    }

    return TransformOperations { WTFMove(operations) };
}

Ref<TransformOperation> TransformOperations::createBlendedMatrixOperationFromOperationsSuffix(const TransformOperations& from, unsigned start, const BlendingContext& context, const LayoutSize& referenceBoxSize) const
{
    TransformationMatrix fromTransform;
    from.apply(fromTransform, referenceBoxSize, start);

    TransformationMatrix toTransform;
    apply(toTransform, referenceBoxSize, start);

    auto progress = context.progress;
    auto compositeOperation = context.compositeOperation;
    if (shouldFallBackToDiscreteAnimation(from, referenceBoxSize)) {
        progress = progress < 0.5 ? 0 : 1;
        compositeOperation = CompositeOperation::Replace;
    }

    toTransform.blend(fromTransform, progress, compositeOperation);
    return Matrix3DTransformOperation::create(toTransform);
}

TextStream& operator<<(TextStream& ts, const TransformOperations& ops)
{
    return ts << ops.m_operations;
}

} // namespace WebCore
