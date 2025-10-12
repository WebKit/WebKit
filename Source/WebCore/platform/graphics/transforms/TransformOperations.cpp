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
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TransformOperations);

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

    return std::ranges::equal(m_operations, o.m_operations, [](auto& a, auto& b) { return arePointingToEqualData(a, b); });
}

TransformOperations TransformOperations::clone() const
{
    return TransformOperations { m_operations.map([](const auto& op) { return op->clone(); }) };
}

void TransformOperations::apply(TransformationMatrix& matrix, unsigned start) const
{
    for (unsigned i = start; i < m_operations.size(); ++i)
        m_operations[i]->apply(matrix);
}

bool TransformOperations::isInvertible() const
{
    TransformationMatrix transform;
    apply(transform);
    return transform.isInvertible();
}

bool TransformOperations::containsNonInvertibleMatrix() const
{
    return (hasTransformOfType<TransformOperation::Type::Matrix>() || hasTransformOfType<TransformOperation::Type::Matrix3D>()) && !isInvertible();
}

TransformOperations blend(const TransformOperations& from, const TransformOperations& to, const BlendingContext& context)
{
    bool shouldFallBackToDiscreteInterpolation = from.containsNonInvertibleMatrix() || to.containsNonInvertibleMatrix();

    auto createBlendedMatrixOperationFromOperationsSuffix = [&](unsigned start) -> Ref<TransformOperation> {
        TransformationMatrix fromTransform;
        from.apply(fromTransform, start);

        TransformationMatrix toTransform;
        to.apply(toTransform, start);

        auto progress = context.progress;
        auto compositeOperation = context.compositeOperation;
        if (shouldFallBackToDiscreteInterpolation) {
            progress = progress < 0.5 ? 0 : 1;
            compositeOperation = CompositeOperation::Replace;
        }

        toTransform.blend(fromTransform, progress, compositeOperation);
        return Matrix3DTransformOperation::create(toTransform);
    };

    if (shouldFallBackToDiscreteInterpolation)
        return TransformOperations { createBlendedMatrixOperationFromOperationsSuffix(0) };

    unsigned fromOperationCount = from.size();
    unsigned toOperationCount = to.size();
    unsigned maxOperationCount = std::max(fromOperationCount, toOperationCount);

    Vector<Ref<TransformOperation>> operations;
    operations.reserveInitialCapacity(maxOperationCount);

    for (unsigned i = 0; i < maxOperationCount; i++) {
        RefPtr<TransformOperation> fromOperation = (i < fromOperationCount) ? from[i].ptr() : nullptr;
        RefPtr<TransformOperation> toOperation = (i < toOperationCount) ? to[i].ptr() : nullptr;

        // If either of the transform list is empty, then we should not attempt to do a matrix blend.
        if (fromOperationCount && toOperationCount) {
            if (fromOperation && toOperation && !fromOperation->sharedPrimitiveType(toOperation.get())) {
                operations.append(createBlendedMatrixOperationFromOperationsSuffix(i));
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

TextStream& operator<<(TextStream& ts, const TransformOperations& ops)
{
    return ts << ops.m_operations;
}

} // namespace WebCore
