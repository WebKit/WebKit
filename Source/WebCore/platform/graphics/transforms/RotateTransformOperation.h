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

#ifndef RotateTransformOperation_h
#define RotateTransformOperation_h

#include "TransformOperation.h"

namespace WebCore {

class RotateTransformOperation final : public TransformOperation {
public:
    static PassRefPtr<RotateTransformOperation> create(double angle, OperationType type)
    {
        return adoptRef(new RotateTransformOperation(0, 0, 1, angle, type));
    }

    static PassRefPtr<RotateTransformOperation> create(double x, double y, double z, double angle, OperationType type)
    {
        return adoptRef(new RotateTransformOperation(x, y, z, angle, type));
    }

    double x() const { return m_x; }
    double y() const { return m_y; }
    double z() const { return m_z; }
    double angle() const { return m_angle; }

private:
    virtual bool isIdentity() const override { return m_angle == 0; }

    virtual OperationType type() const override { return m_type; }
    virtual bool isSameType(const TransformOperation& o) const override { return o.type() == m_type; }

    virtual bool operator==(const TransformOperation&) const override;

    virtual bool apply(TransformationMatrix& transform, const FloatSize& /*borderBoxSize*/) const override
    {
        transform.rotate3d(m_x, m_y, m_z, m_angle);
        return false;
    }

    virtual PassRefPtr<TransformOperation> blend(const TransformOperation* from, double progress, bool blendToIdentity = false) override;

    RotateTransformOperation(double x, double y, double z, double angle, OperationType type)
        : m_x(x)
        , m_y(y)
        , m_z(z)
        , m_angle(angle)
        , m_type(type)
    {
        ASSERT(isRotateTransformOperationType());
    }

    double m_x;
    double m_y;
    double m_z;
    double m_angle;
    OperationType m_type;
};

TRANSFORMOPERATION_TYPE_CASTS(RotateTransformOperation, isRotateTransformOperationType());

} // namespace WebCore

#endif // RotateTransformOperation_h
