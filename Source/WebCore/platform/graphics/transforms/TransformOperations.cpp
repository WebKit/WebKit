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
#include "IdentityTransformOperation.h"
#include "Matrix3DTransformOperation.h"
#include <algorithm>
#include <wtf/text/TextStream.h>

namespace WebCore {

TransformOperations::TransformOperations(bool makeIdentity)
{
    if (makeIdentity)
        m_operations.append(IdentityTransformOperation::create());
}

bool TransformOperations::operator==(const TransformOperations& o) const
{
    if (m_operations.size() != o.m_operations.size())
        return false;
        
    unsigned s = m_operations.size();
    for (unsigned i = 0; i < s; i++) {
        if (*m_operations[i] != *o.m_operations[i])
            return false;
    }
    
    return true;
}

void SharedPrimitivesPrefix::update(const TransformOperations& operations)
{
    size_t maxIteration = operations.operations().size();
    if (m_indexOfFirstMismatch.has_value())
        maxIteration = std::min(*m_indexOfFirstMismatch, maxIteration);

    for (size_t i = 0; i < maxIteration; ++i) {
        const auto* operation = operations.at(i);

        // If we haven't seen an operation at this index before, we can simply use our primitive type.
        if (i >= m_primitives.size()) {
            ASSERT(i == m_primitives.size());
            m_primitives.append(operation->primitiveType());
            continue;
        }

        if (auto sharedPrimitive = operation->sharedPrimitiveType(m_primitives[i]))
            m_primitives[i] = *sharedPrimitive;
        else {
            m_indexOfFirstMismatch = i;
            m_primitives.shrink(i);
            return;
        }
    }
}

bool TransformOperations::affectedByTransformOrigin() const
{
    for (const auto& operation : m_operations) {
        if (operation->isAffectedByTransformOrigin())
            return true;
    }
    return false;
}

bool TransformOperations::shouldFallBackToDiscreteAnimation(const TransformOperations& from, const LayoutSize& boxSize) const
{
    return (from.hasMatrixOperation() || hasMatrixOperation()) && (!from.isInvertible(boxSize) || !isInvertible(boxSize));
}

TransformOperations TransformOperations::blend(const TransformOperations& from, const BlendingContext& context, const LayoutSize& boxSize, std::optional<unsigned> prefixLength) const
{
    TransformOperations result;

    unsigned fromOperationCount = from.operations().size();
    unsigned toOperationCount = operations().size();
    unsigned maxOperationCount = std::max(fromOperationCount, toOperationCount);

    if (shouldFallBackToDiscreteAnimation(from, boxSize)) {
        result.operations().append(createBlendedMatrixOperationFromOperationsSuffix(from, 0, context, boxSize));
        return result;
    }

    for (unsigned i = 0; i < maxOperationCount; i++) {
        RefPtr<TransformOperation> fromOperation = (i < fromOperationCount) ? from.operations()[i].get() : nullptr;
        RefPtr<TransformOperation> toOperation = (i < toOperationCount) ? operations()[i].get() : nullptr;

        // If either of the transform list is empty, then we should not attempt to do a matrix blend.
        if (fromOperationCount && toOperationCount) {
            if ((prefixLength && i >= *prefixLength) || (fromOperation && toOperation && !fromOperation->sharedPrimitiveType(toOperation.get()))) {
                result.operations().append(createBlendedMatrixOperationFromOperationsSuffix(from, i, context, boxSize));
                return result;
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
        result.operations().append(blendedOperation);
    }

    return result;
}

RefPtr<TransformOperation> TransformOperations::createBlendedMatrixOperationFromOperationsSuffix(const TransformOperations& from, unsigned start, const BlendingContext& context, const LayoutSize& referenceBoxSize) const
{
    TransformationMatrix fromTransform;
    from.apply(start, referenceBoxSize, fromTransform);
    TransformationMatrix toTransform;
    apply(start, referenceBoxSize, toTransform);

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
    for (const auto& operation : ops.operations())
        ts << *operation;
    return ts;
}

} // namespace WebCore
