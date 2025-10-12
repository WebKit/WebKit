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

#include <WebCore/StyleLengthWrapper.h>
#include <WebCore/StylePrimitiveNumericTypes.h>
#include <WebCore/StyleTransformFunctionBase.h>

namespace WebCore {
namespace Style {

// translate() = translate( <length-percentage> , <length-percentage>? )
// https://drafts.csswg.org/css-transforms/#funcdef-transform-translate
// translate3d() = translate3d( <length-percentage> , <length-percentage> , <length> )
// https://drafts.csswg.org/css-transforms-2/#funcdef-translate3d
// translateX() = translateX( <length-percentage> )
// https://drafts.csswg.org/css-transforms/#funcdef-transform-translatex
// translateY() = translateY( <length-percentage> )
// https://drafts.csswg.org/css-transforms/#funcdef-transform-translatey
// translateZ() = translateZ( <length> )
// https://drafts.csswg.org/css-transforms-2/#funcdef-translatez

struct TranslateLengthPercentage : LengthWrapperBase<LengthPercentage<>> {
    using Base::Base;
};

class TranslateTransformFunction final : public TransformFunctionBase {
public:
    using LengthPercentage = Style::TranslateLengthPercentage;
    using Length = Style::Length<>;

    static Ref<const TranslateTransformFunction> create(const LengthPercentage&, const LengthPercentage&, TransformFunctionBase::Type);
    static Ref<const TranslateTransformFunction> create(const LengthPercentage&, const LengthPercentage&, const Length&, TransformFunctionBase::Type);

    Ref<const TransformFunctionBase> clone() const override;
    Ref<TransformOperation> toPlatform(const FloatSize&) const override;

    TransformFunctionBase::Type primitiveType() const override { return isRepresentableIn2D() ? Type::Translate : Type::Translate3D; }

    const LengthPercentage& x() const { return m_x; }
    const LengthPercentage& y() const { return m_y; }
    Length z() const { return m_z; }

    bool isIdentity() const override { return m_x.isKnownZero() && m_y.isKnownZero() && m_z.isZero(); }
    bool isRepresentableIn2D() const override { return m_z.isZero(); }

    TransformFunctionSizeDependencies computeSizeDependencies() const override;

    bool operator==(const TranslateTransformFunction& other) const { return operator==(static_cast<const TransformFunctionBase&>(other)); }
    bool operator==(const TransformFunctionBase&) const override;

    void apply(TransformationMatrix&, const FloatSize& borderBoxSize) const override;

    Ref<const TransformFunctionBase> blend(const TransformFunctionBase* from, const BlendingContext&, bool blendToIdentity = false) const override;

    void dump(WTF::TextStream&) const override;

private:
    TranslateTransformFunction(const LengthPercentage&, const LengthPercentage&, const Length&, TransformFunctionBase::Type);

    LengthPercentage m_x;
    LengthPercentage m_y;
    Length m_z;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_TRANSFORM_FUNCTION(WebCore::Style::TranslateTransformFunction, WebCore::Style::TransformFunctionBase::isTranslateTransformFunctionType)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TranslateLengthPercentage)
