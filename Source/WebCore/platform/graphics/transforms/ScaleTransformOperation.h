/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005-2008, 2017 Apple Inc. All rights reserved.
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

#include "TransformOperation.h"
#include <wtf/Ref.h>

namespace WebCore {

struct BlendingContext;

class ScaleTransformOperation final : public TransformOperation {
public:
    static Ref<ScaleTransformOperation> create(double sx, double sy, OperationType type)
    {
        return adoptRef(*new ScaleTransformOperation(sx, sy, 1, type));
    }

    static Ref<ScaleTransformOperation> create(double sx, double sy, double sz, OperationType type)
    {
        return adoptRef(*new ScaleTransformOperation(sx, sy, sz, type));
    }

    Ref<TransformOperation> clone() const override
    {
        return adoptRef(*new ScaleTransformOperation(m_x, m_y, m_z, type()));
    }

    double x() const { return m_x; }
    double y() const { return m_y; }
    double z() const { return m_z; }

    bool operator==(const TransformOperation&) const final;

    Ref<TransformOperation> blend(const TransformOperation* from, const BlendingContext&, bool blendToIdentity = false) final;

    bool isIdentity() const final { return m_x == 1 &&  m_y == 1 &&  m_z == 1; }

    bool isRepresentableIn2D() const final { return m_z == 1; }

private:
    bool isAffectedByTransformOrigin() const override { return !isIdentity(); }

    bool apply(TransformationMatrix& transform, const FloatSize&) const override
    {
        transform.scale3d(m_x, m_y, m_z);
        return false;
    }

    void dump(WTF::TextStream&) const final;

    ScaleTransformOperation(double sx, double sy, double sz, OperationType type)
        : TransformOperation(type)
        , m_x(sx)
        , m_y(sy)
        , m_z(sz)
    {
        ASSERT(isScaleTransformOperationType());
    }
        
    double m_x;
    double m_y;
    double m_z;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_TRANSFORMOPERATION(WebCore::ScaleTransformOperation, isScaleTransformOperationType())
