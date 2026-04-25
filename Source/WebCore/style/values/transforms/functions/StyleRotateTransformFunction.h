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

#include <WebCore/StylePrimitiveNumericTypes.h>
#include <WebCore/StyleTransformFunctionBase.h>

namespace WebCore {
namespace Style {

// rotate() = rotate( [ <angle> | <zero> ] )
// https://drafts.csswg.org/css-transforms/#funcdef-transform-rotate
// rotate3d() = rotate3d( <number> , <number> , <number> , [ <angle> | <zero> ] )
// https://drafts.csswg.org/css-transforms-2/#funcdef-rotate3d
// rotateX() = rotateX( [ <angle> | <zero> ] )
// https://drafts.csswg.org/css-transforms-2/#funcdef-rotatex
// rotateY() = rotateY( [ <angle> | <zero> ] )
// https://drafts.csswg.org/css-transforms-2/#funcdef-rotatey
// rotateZ() = rotateZ( [ <angle> | <zero> ] )
// https://drafts.csswg.org/css-transforms-2/#funcdef-rotatez

class RotateTransformFunction final : public TransformFunctionBase {
public:
    static Ref<const RotateTransformFunction> create(Angle<>, TransformFunctionBase::Type);
    static Ref<const RotateTransformFunction> create(Number<>, Number<>, Number<>, Angle<>, TransformFunctionBase::Type);

    Ref<const TransformFunctionBase> clone() const override;
    Ref<TransformOperation> toPlatform(const FloatSize&) const override;

    Number<> x() const { return m_x; }
    Number<> y() const { return m_y; }
    Number<> z() const { return m_z; }
    Angle<> angle() const { return m_angle; }

    TransformFunctionBase::Type primitiveType() const override { return type() == Type::Rotate ? Type::Rotate : Type::Rotate3D; }

    bool operator==(const RotateTransformFunction& other) const { return operator==(static_cast<const TransformFunctionBase&>(other)); }
    bool operator==(const TransformFunctionBase&) const override;

    bool isIdentity() const override { return m_angle.isZero(); }
    bool isRepresentableIn2D() const override { return (m_x.isZero() && m_y.isZero()) || m_angle.isZero(); }
    bool isAffectedByTransformOrigin() const override { return !isIdentity(); }

    void apply(TransformationMatrix&, const FloatSize& borderBoxSize) const override;

    Ref<const TransformFunctionBase> blend(const TransformFunctionBase* from, const BlendingContext&, bool blendToIdentity = false) const override;

    void dump(WTF::TextStream&) const override;

private:
    RotateTransformFunction(Number<>, Number<>, Number<>, Angle<>, TransformFunctionBase::Type);

    Number<> m_x;
    Number<> m_y;
    Number<> m_z;
    Angle<> m_angle;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_TRANSFORM_FUNCTION(WebCore::Style::RotateTransformFunction, WebCore::Style::TransformFunctionBase::isRotateTransformFunctionType)
