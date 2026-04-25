/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "StylePrimitiveNumericTypes.h"
#include "StyleTransformFunctionBase.h"
#include "TransformationMatrix.h"

namespace WebCore {
namespace Style {

// matrix3d() = matrix3d( <number>#{16} )
// https://drafts.csswg.org/css-transforms-2/#funcdef-matrix3d

class Matrix3DTransformFunction final : public TransformFunctionBase {
public:
    static Ref<const Matrix3DTransformFunction> create(const TransformationMatrix&);

    Ref<const TransformFunctionBase> clone() const override;
    Ref<TransformOperation> toPlatform(const FloatSize&) const override;

    TransformationMatrix matrix() const { return m_matrix; }

    bool isIdentity() const override { return m_matrix.isIdentity(); }
    bool isAffectedByTransformOrigin() const override { return !isIdentity(); }
    bool isRepresentableIn2D() const override { return m_matrix.isAffine(); }

    bool operator==(const Matrix3DTransformFunction& other) const { return operator==(static_cast<const TransformFunctionBase&>(other)); }
    bool operator==(const TransformFunctionBase&) const override;

    void apply(TransformationMatrix&, const FloatSize& borderBoxSize) const override;

    Ref<const TransformFunctionBase> blend(const TransformFunctionBase* from, const BlendingContext&, bool blendToIdentity = false) const override;

    void dump(WTF::TextStream&) const override;

private:
    Matrix3DTransformFunction(const TransformationMatrix&);

    TransformationMatrix m_matrix;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_TRANSFORM_FUNCTION(WebCore::Style::Matrix3DTransformFunction, WebCore::Style::TransformFunctionBase::Type::Matrix3D ==)
