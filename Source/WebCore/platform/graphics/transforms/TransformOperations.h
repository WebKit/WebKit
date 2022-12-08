/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#pragma once

#include "LayoutSize.h"
#include "TransformOperation.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

struct BlendingContext;

class TransformOperations {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT explicit TransformOperations(bool makeIdentity = false);
    WEBCORE_EXPORT explicit TransformOperations(Vector<RefPtr<TransformOperation>>&&);
    
    bool operator==(const TransformOperations& o) const;
    bool operator!=(const TransformOperations& o) const
    {
        return !(*this == o);
    }
    
    void apply(const FloatSize& size, TransformationMatrix& matrix) const { apply(0, size, matrix); }
    void apply(unsigned start, const FloatSize& size, TransformationMatrix& matrix) const
    {
        for (unsigned i = start; i < m_operations.size(); ++i)
            m_operations[i]->apply(matrix, size);
    }

    
    // Return true if any of the operation types are 3D operation types (even if the
    // values describe affine transforms)
    bool has3DOperation() const
    {
        for (const auto& operation : m_operations) {
            if (operation->is3DOperation())
                return true;
        }
        return false;
    }
    
    bool hasMatrixOperation() const
    {
        return std::any_of(m_operations.begin(), m_operations.end(), [](auto operation) {
            return operation->type() == WebCore::TransformOperation::Type::Matrix;
        });
    }

    bool isRepresentableIn2D() const
    {
        for (const auto& operation : m_operations) {
            if (!operation->isRepresentableIn2D())
                return false;
        }
        return true;
    }

    void clear()
    {
        m_operations.clear();
    }

    bool affectedByTransformOrigin() const;
    
    Vector<RefPtr<TransformOperation>>& operations() { return m_operations; }
    const Vector<RefPtr<TransformOperation>>& operations() const { return m_operations; }

    size_t size() const { return m_operations.size(); }
    const TransformOperation* at(size_t index) const { return index < m_operations.size() ? m_operations.at(index).get() : 0; }
    bool isInvertible(const LayoutSize& size) const
    {
        TransformationMatrix transform;
        apply(size, transform);
        return transform.isInvertible();
    }
    
    bool shouldFallBackToDiscreteAnimation(const TransformOperations&, const LayoutSize&) const;
    
    RefPtr<TransformOperation> createBlendedMatrixOperationFromOperationsSuffix(const TransformOperations& from, unsigned start, const BlendingContext&, const LayoutSize& referenceBoxSize) const;
    TransformOperations blend(const TransformOperations& from, const BlendingContext&, const LayoutSize&, std::optional<unsigned> prefixLength = std::nullopt) const;

private:
    Vector<RefPtr<TransformOperation>> m_operations;
};

// SharedPrimitivesPrefix is used to find a shared prefix of transform function primitives (as
// defined by CSS Transforms Level 1 & 2). Given a series of TransformOperations in the keyframes
// of an animation. After update() is called with the TransformOperations of every keyframe,
// primitive() will return the prefix of primitives that are shared by all keyframes passed
// to update().
class SharedPrimitivesPrefix {
public:
    SharedPrimitivesPrefix() = default;
    virtual ~SharedPrimitivesPrefix() = default;
    void update(const TransformOperations&);
    bool hadIncompatibleTransformFunctions() { return m_indexOfFirstMismatch.has_value(); }
    const Vector<TransformOperation::Type>& primitives() const { return m_primitives; }

private:
    std::optional<size_t> m_indexOfFirstMismatch;
    Vector<TransformOperation::Type> m_primitives;
};

WTF::TextStream& operator<<(WTF::TextStream&, const TransformOperations&);

} // namespace WebCore

