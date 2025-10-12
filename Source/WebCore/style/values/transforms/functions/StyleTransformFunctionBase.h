/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005-2008, 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/FloatSize.h>
#include <WebCore/StyleValueTypes.h>
#include <WebCore/TransformOperation.h>
#include <WebCore/TransformationMatrix.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

struct BlendingContext;

namespace Style {

enum class TransformFunctionType : uint8_t {
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
};

struct TransformFunctionSizeDependencies {
    bool isWidthDependent = false;
    bool isHeightDependent = false;
};

class TransformFunctionBase : public RefCounted<TransformFunctionBase> {
public:
    using Type = TransformFunctionType;

    TransformFunctionBase(Type type)
        : m_type(type)
    {
    }

    virtual ~TransformFunctionBase() = default;

    virtual Ref<const TransformFunctionBase> clone() const = 0;

    virtual Ref<TransformOperation> toPlatform(const FloatSize& borderBoxSize) const = 0;

    virtual bool operator==(const TransformFunctionBase&) const = 0;

    virtual void apply(TransformationMatrix&, const FloatSize& borderBoxSize) const = 0;

    virtual Ref<const TransformFunctionBase> blend(const TransformFunctionBase* from, const BlendingContext&, bool blendToIdentity = false) const = 0;

    Type type() const { return m_type; }
    bool isSameType(const TransformFunctionBase& other) const { return type() == other.type(); }

    virtual Type primitiveType() const { return m_type; }
    std::optional<Type> sharedPrimitiveType(Type other) const;
    std::optional<Type> sharedPrimitiveType(const TransformFunctionBase* other) const;

    virtual bool isIdentity() const = 0;
    virtual bool isAffectedByTransformOrigin() const { return false; }
    virtual bool isRepresentableIn2D() const { return true; }

    virtual TransformFunctionSizeDependencies computeSizeDependencies() const { return { }; }

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

    static bool isRotateTransformFunctionType(Type type)
    {
        return type == Type::RotateX
            || type == Type::RotateY
            || type == Type::RotateZ
            || type == Type::Rotate
            || type == Type::Rotate3D;
    }

    static bool isScaleTransformFunctionType(Type type)
    {
        return type == Type::ScaleX
            || type == Type::ScaleY
            || type == Type::ScaleZ
            || type == Type::Scale
            || type == Type::Scale3D;
    }

    static bool isSkewTransformFunctionType(Type type)
    {
        return type == Type::SkewX
            || type == Type::SkewY
            || type == Type::Skew;
    }

    static bool isTranslateTransformFunctionType(Type type)
    {
        return type == Type::TranslateX
            || type == Type::TranslateY
            || type == Type::TranslateZ
            || type == Type::Translate
            || type == Type::Translate3D;
    }

    virtual void dump(WTF::TextStream&) const = 0;

private:
    Type m_type;
};

WTF::TextStream& operator<<(WTF::TextStream&, TransformFunctionBase::Type);
WTF::TextStream& operator<<(WTF::TextStream&, const TransformFunctionBase&);

// MARK: - Platform

template<> struct ToPlatform<TransformFunctionType> { auto operator()(TransformFunctionType) -> TransformOperationType; };

} // namespace Style
} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_STYLE_TRANSFORM_FUNCTION(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(ToValueTypeName) \
    static bool isType(const WebCore::Style::TransformFunctionBase& operation) { return predicate(operation.type()); } \
SPECIALIZE_TYPE_TRAITS_END()
