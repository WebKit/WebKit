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
    explicit TransformOperations(bool makeIdentity = false);
    
    bool operator==(const TransformOperations& o) const;
    bool operator!=(const TransformOperations& o) const
    {
        return !(*this == o);
    }
    
    void apply(const FloatSize& sz, TransformationMatrix& t) const
    {
        for (unsigned i = 0; i < m_operations.size(); ++i)
            m_operations[i]->apply(t, sz);
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

    bool isRepresentableIn2D() const
    {
        for (const auto& operation : m_operations) {
            if (!operation->isRepresentableIn2D())
                return false;
        }
        return true;
    }

    bool operationsMatch(const TransformOperations&) const;
    
    void clear()
    {
        m_operations.clear();
    }

    bool affectedByTransformOrigin() const;
    
    Vector<RefPtr<TransformOperation>>& operations() { return m_operations; }
    const Vector<RefPtr<TransformOperation>>& operations() const { return m_operations; }

    size_t size() const { return m_operations.size(); }
    const TransformOperation* at(size_t index) const { return index < m_operations.size() ? m_operations.at(index).get() : 0; }

    TransformOperations blendByMatchingOperations(const TransformOperations& from, const BlendingContext&) const;
    TransformOperations blendByUsingMatrixInterpolation(const TransformOperations& from, const BlendingContext&, const LayoutSize&) const;
    TransformOperations blend(const TransformOperations& from, const BlendingContext&, const LayoutSize&) const;

private:
    Vector<RefPtr<TransformOperation>> m_operations;
};

WTF::TextStream& operator<<(WTF::TextStream&, const TransformOperations&);

} // namespace WebCore

