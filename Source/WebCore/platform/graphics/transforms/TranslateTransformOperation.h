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

#include <WebCore/TransformOperation.h>
#include <wtf/Ref.h>

namespace WebCore {

struct BlendingContext;

class TranslateTransformOperation final : public TransformOperation {
public:
    static Ref<TranslateTransformOperation> create(float tx, float ty, TransformOperation::Type type)
    {
        return adoptRef(*new TranslateTransformOperation(tx, ty, 0, type));
    }

    WEBCORE_EXPORT static Ref<TranslateTransformOperation> create(float, float, float, TransformOperation::Type);

    Ref<TransformOperation> clone() const override
    {
        return adoptRef(*new TranslateTransformOperation(m_x, m_y, m_z, type()));
    }

    float x() const { return m_x; }
    float y() const { return m_y; }
    float z() const { return m_z; }

    TransformOperation::Type primitiveType() const final { return !m_z ? Type::Translate : Type::Translate3D; }

    void apply(TransformationMatrix& transform) const final
    {
        transform.translate3d(m_x, m_y, m_z);
    }

    bool operator==(const TranslateTransformOperation& other) const { return operator==(static_cast<const TransformOperation&>(other)); }
    bool operator==(const TransformOperation&) const final;

    Ref<TransformOperation> blend(const TransformOperation* from, const BlendingContext&, bool blendToIdentity = false) const final;

private:
    void dump(WTF::TextStream&) const final;

    TranslateTransformOperation(float, float, float, TransformOperation::Type);

    float m_x;
    float m_y;
    float m_z;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_TRANSFORMOPERATION(WebCore::TranslateTransformOperation, WebCore::TransformOperation::isTranslateTransformOperationType)
