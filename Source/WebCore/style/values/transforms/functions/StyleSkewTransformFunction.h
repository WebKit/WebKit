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

// skew() = skew( [ <angle> | <zero> ] , [ <angle> | <zero> ]? )
// https://drafts.csswg.org/css-transforms/#funcdef-transform-skew
// skewX() = skewX( [ <angle> | <zero> ] )
// https://drafts.csswg.org/css-transforms/#funcdef-transform-skew
// skewY() = skewY( [ <angle> | <zero> ] )
// https://drafts.csswg.org/css-transforms/#funcdef-transform-skewy

class SkewTransformFunction final : public TransformFunctionBase {
public:
    static Ref<const SkewTransformFunction> create(Angle<>, Angle<>, TransformFunctionBase::Type);

    Ref<const TransformFunctionBase> clone() const override;
    Ref<TransformOperation> toPlatform(const FloatSize&) const override;

    Angle<> angleX() const { return m_angleX; }
    Angle<> angleY() const { return m_angleY; }

    bool isIdentity() const override { return m_angleX.isZero() && m_angleY.isZero(); }
    bool isAffectedByTransformOrigin() const override { return !isIdentity(); }

    bool operator==(const SkewTransformFunction& other) const { return operator==(static_cast<const TransformFunctionBase&>(other)); }
    bool operator==(const TransformFunctionBase&) const override;

    void apply(TransformationMatrix&, const FloatSize& borderBoxSize) const override;

    Ref<const TransformFunctionBase> blend(const TransformFunctionBase* from, const BlendingContext&, bool blendToIdentity = false) const override;

    void dump(WTF::TextStream&) const override;

private:
    SkewTransformFunction(Angle<>, Angle<>, TransformFunctionBase::Type);

    Angle<> m_angleX;
    Angle<> m_angleY;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_TRANSFORM_FUNCTION(WebCore::Style::SkewTransformFunction, WebCore::Style::TransformFunctionBase::isSkewTransformFunctionType)
