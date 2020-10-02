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

#include "Length.h"
#include "LengthFunctions.h"
#include "TransformOperation.h"
#include <wtf/Ref.h>

namespace WebCore {

class TranslateTransformOperation final : public TransformOperation {
public:
    static Ref<TranslateTransformOperation> create(const Length& tx, const Length& ty, OperationType type)
    {
        return adoptRef(*new TranslateTransformOperation(tx, ty, Length(0, Fixed), type));
    }

    static Ref<TranslateTransformOperation> create(const Length& tx, const Length& ty, const Length& tz, OperationType type)
    {
        return adoptRef(*new TranslateTransformOperation(tx, ty, tz, type));
    }

    Ref<TransformOperation> clone() const override
    {
        return adoptRef(*new TranslateTransformOperation(m_x, m_y, m_z, type()));
    }

    double x(const FloatSize& borderBoxSize) const { return floatValueForLength(m_x, borderBoxSize.width()); }
    double y(const FloatSize& borderBoxSize) const { return floatValueForLength(m_y, borderBoxSize.height()); }
    double z(const FloatSize&) const { return floatValueForLength(m_z, 1); }

    Length x() const { return m_x; }
    Length y() const { return m_y; }
    Length z() const { return m_z; }

private:
    bool isIdentity() const override { return !floatValueForLength(m_x, 1) && !floatValueForLength(m_y, 1) && !floatValueForLength(m_z, 1); }

    bool isRepresentableIn2D() const final { return m_z.isZero(); }

    bool operator==(const TransformOperation&) const override;

    bool apply(TransformationMatrix& transform, const FloatSize& borderBoxSize) const override
    {
        transform.translate3d(x(borderBoxSize), y(borderBoxSize), z(borderBoxSize));
        return m_x.isPercent() || m_y.isPercent();
    }

    Ref<TransformOperation> blend(const TransformOperation* from, double progress, bool blendToIdentity = false) override;

    void dump(WTF::TextStream&) const final;

    TranslateTransformOperation(const Length& tx, const Length& ty, const Length& tz, OperationType type)
        : TransformOperation(type)
        , m_x(tx)
        , m_y(ty)
        , m_z(tz)
    {
        ASSERT(isTranslateTransformOperationType());
    }

    Length m_x;
    Length m_y;
    Length m_z;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_TRANSFORMOPERATION(WebCore::TranslateTransformOperation, isTranslateTransformOperationType())
