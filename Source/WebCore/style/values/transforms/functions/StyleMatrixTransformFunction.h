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

#include "StylePrimitiveNumericTypes.h"
#include "StyleTransformFunctionBase.h"

namespace WebCore {
namespace Style {

// matrix() = matrix( <number>#{6} )
// https://drafts.csswg.org/css-transforms/#funcdef-transform-matrix

class MatrixTransformFunction final : public TransformFunctionBase {
public:
    static Ref<const MatrixTransformFunction> createIdentity();
    static Ref<const MatrixTransformFunction> create(Number<>, Number<>, Number<>, Number<>, Number<>, Number<>);
    static Ref<const MatrixTransformFunction> create(const TransformationMatrix&);

    Ref<const TransformFunctionBase> clone() const override;
    Ref<TransformOperation> toPlatform(const FloatSize&) const override;

    TransformationMatrix matrix() const { return TransformationMatrix(m_a.value, m_b.value, m_c.value, m_d.value, m_e.value, m_f.value); }

    bool isIdentity() const override { return m_a == 1 && m_b == 0 && m_c == 0 && m_d == 1 && m_e == 0 && m_f == 0; }
    bool isAffectedByTransformOrigin() const override { return !isIdentity(); }

    bool operator==(const MatrixTransformFunction& other) const { return operator==(static_cast<const TransformFunctionBase&>(other)); }
    bool operator==(const TransformFunctionBase&) const override;

    void apply(TransformationMatrix&, const FloatSize& borderBoxSize) const override;

    Ref<const TransformFunctionBase> blend(const TransformFunctionBase* from, const BlendingContext&, bool blendToIdentity = false) const override;

    void dump(WTF::TextStream&) const override;

private:
    MatrixTransformFunction(Number<>, Number<>, Number<>, Number<>, Number<>, Number<>);
    MatrixTransformFunction(const TransformationMatrix&);

    Number<> m_a;
    Number<> m_b;
    Number<> m_c;
    Number<> m_d;
    Number<> m_e;
    Number<> m_f;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_TRANSFORM_FUNCTION(WebCore::Style::MatrixTransformFunction, WebCore::Style::TransformFunctionBase::Type::Matrix ==)
