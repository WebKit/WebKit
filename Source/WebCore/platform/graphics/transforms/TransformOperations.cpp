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

bool TransformOperations::operationsMatch(const TransformOperations& other) const
{
    size_t numOperations = operations().size();
    // If the sizes of the function lists don't match, the lists don't match
    if (numOperations != other.operations().size())
        return false;
    
    // If the types of each function are not the same, the lists don't match
    for (size_t i = 0; i < numOperations; ++i) {
        if (!operations()[i]->isSameType(*other.operations()[i]))
            return false;
    }
    return true;
}

bool TransformOperations::affectedByTransformOrigin() const
{
    for (const auto& operation : m_operations) {
        if (operation->isAffectedByTransformOrigin())
            return true;
    }
    return false;
}

TransformOperations TransformOperations::blendByMatchingOperations(const TransformOperations& from, const BlendingContext& context) const
{
    TransformOperations result;

    unsigned fromSize = from.operations().size();
    unsigned toSize = operations().size();
    unsigned size = std::max(fromSize, toSize);
    for (unsigned i = 0; i < size; i++) {
        RefPtr<TransformOperation> fromOperation = (i < fromSize) ? from.operations()[i].get() : nullptr;
        RefPtr<TransformOperation> toOperation = (i < toSize) ? operations()[i].get() : nullptr;
        RefPtr<TransformOperation> blendedOperation = toOperation ? toOperation->blend(fromOperation.get(), context) : (fromOperation ? RefPtr<TransformOperation>(fromOperation->blend(nullptr, context, true)) : nullptr);
        if (blendedOperation)
            result.operations().append(blendedOperation);
        else {
            auto identityOperation = IdentityTransformOperation::create();
            if (context.progress > 0.5)
                result.operations().append(toOperation ? toOperation : WTFMove(identityOperation));
            else
                result.operations().append(fromOperation ? fromOperation : WTFMove(identityOperation));
        }
    }

    return result;
}

TransformOperations TransformOperations::blendByUsingMatrixInterpolation(const TransformOperations& from, const BlendingContext& context, const LayoutSize& size) const
{
    TransformOperations result;

    // Convert the TransformOperations into matrices
    TransformationMatrix fromTransform;
    TransformationMatrix toTransform;
    from.apply(size, fromTransform);
    apply(size, toTransform);

    toTransform.blend(fromTransform, context.progress);

    // Append the result
    result.operations().append(Matrix3DTransformOperation::create(toTransform));

    return result;
}

TransformOperations TransformOperations::blend(const TransformOperations& from, const BlendingContext& context, const LayoutSize& size) const
{
    if (from == *this)
        return *this;

    if (from.size() && from.operationsMatch(*this))
        return blendByMatchingOperations(from, context);

    return blendByUsingMatrixInterpolation(from, context, size);
}

TextStream& operator<<(TextStream& ts, const TransformOperations& ops)
{
    for (const auto& operation : ops.operations())
        ts << *operation;
    return ts;
}

} // namespace WebCore
