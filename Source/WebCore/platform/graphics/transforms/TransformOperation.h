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

#include "CompositeOperation.h"
#include "FloatSize.h"
#include "TransformationMatrix.h"
#include <wtf/EnumTraits.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

struct BlendingContext;

class TransformOperation : public RefCounted<TransformOperation> {
public:
    enum class Type : uint8_t {
        ScaleX,
        ScaleY,
        Scale,
        TranslateX,
        TranslateY,
        Translate,
        RotateX,
        RotateY,
        Rotate,
        SkewX,
        SkewY,
        Skew,
        Matrix,
        ScaleZ,
        Scale3D,
        TranslateZ,
        Translate3D,
        RotateZ,
        Rotate3D,
        Matrix3D,
        Perspective,
        Identity,
        None
    };

    TransformOperation(Type type)
        : m_type(type)
    {
    }
    virtual ~TransformOperation() = default;

    virtual Ref<TransformOperation> clone() const = 0;

    virtual bool operator==(const TransformOperation&) const = 0;
    bool operator!=(const TransformOperation& o) const { return !(*this == o); }

    virtual bool isIdentity() const = 0;

    // Return true if the borderBoxSize was used in the computation, false otherwise.
    virtual bool apply(TransformationMatrix&, const FloatSize& borderBoxSize) const = 0;

    virtual Ref<TransformOperation> blend(const TransformOperation* from, const BlendingContext&, bool blendToIdentity = false) = 0;

    Type type() const { return m_type; }
    bool isSameType(const TransformOperation& other) const { return type() == other.type(); }

    virtual Type primitiveType() const { return m_type; }
    std::optional<Type> sharedPrimitiveType(Type other) const;
    std::optional<Type> sharedPrimitiveType(const TransformOperation* other) const;

    virtual bool isAffectedByTransformOrigin() const { return false; }
    
    bool is3DOperation() const
    {
        Type opType = type();
        return opType == Type::ScaleZ
            || opType == Type::Scale3D
            || opType == Type::TranslateZ
            || opType == Type::Translate3D
            || opType == Type::RotateX
            || opType == Type::RotateY
            || opType == Type::Rotate3D
            || opType == Type::Matrix3D
            || opType == Type::Perspective;
    }
    
    virtual bool isRepresentableIn2D() const { return true; }

    bool isRotateTransformOperationType() const
    {
        return type() == Type::RotateX || type() == Type::RotateY || type() == Type::RotateZ || type() == Type::Rotate || type() == Type::Rotate3D;
    }

    bool isScaleTransformOperationType() const
    {
        return type() == Type::ScaleX || type() == Type::ScaleY || type() == Type::ScaleZ || type() == Type::Scale || type() == Type::Scale3D;
    }

    bool isSkewTransformOperationType() const
    {
        return type() == Type::SkewX || type() == Type::SkewY || type() == Type::Skew;
    }

    bool isTranslateTransformOperationType() const
    {
        return type() == Type::TranslateX || type() == Type::TranslateY || type() == Type::TranslateZ || type() == Type::Translate || type() == Type::Translate3D;
    }
    
    virtual void dump(WTF::TextStream&) const = 0;

private:
    Type m_type;
};

WTF::TextStream& operator<<(WTF::TextStream&, TransformOperation::Type);
WTF::TextStream& operator<<(WTF::TextStream&, const TransformOperation&);

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::TransformOperation::Type> {
    using values = EnumValues<
        WebCore::TransformOperation::Type,
        WebCore::TransformOperation::Type::ScaleX,
        WebCore::TransformOperation::Type::ScaleY,
        WebCore::TransformOperation::Type::Scale,
        WebCore::TransformOperation::Type::TranslateX,
        WebCore::TransformOperation::Type::TranslateY,
        WebCore::TransformOperation::Type::Translate,
        WebCore::TransformOperation::Type::RotateX,
        WebCore::TransformOperation::Type::RotateY,
        WebCore::TransformOperation::Type::Rotate,
        WebCore::TransformOperation::Type::SkewX,
        WebCore::TransformOperation::Type::SkewY,
        WebCore::TransformOperation::Type::Skew,
        WebCore::TransformOperation::Type::Matrix,
        WebCore::TransformOperation::Type::ScaleZ,
        WebCore::TransformOperation::Type::Scale3D,
        WebCore::TransformOperation::Type::TranslateZ,
        WebCore::TransformOperation::Type::Translate3D,
        WebCore::TransformOperation::Type::RotateZ,
        WebCore::TransformOperation::Type::Rotate3D,
        WebCore::TransformOperation::Type::Matrix3D,
        WebCore::TransformOperation::Type::Perspective,
        WebCore::TransformOperation::Type::Identity,
        WebCore::TransformOperation::Type::None
    >;
};

} // namespace WTF

#define SPECIALIZE_TYPE_TRAITS_TRANSFORMOPERATION(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(ToValueTypeName) \
    static bool isType(const WebCore::TransformOperation& operation) { return operation.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
