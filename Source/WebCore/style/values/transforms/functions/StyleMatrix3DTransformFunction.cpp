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

#include "config.h"
#include "StyleMatrix3DTransformFunction.h"

#include "AnimationUtilities.h"
#include "Matrix3DTransformOperation.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <algorithm>

namespace WebCore {
namespace Style {

Matrix3DTransformFunction::Matrix3DTransformFunction(const TransformationMatrix& matrix)
    : TransformFunctionBase(TransformFunctionBase::Type::Matrix3D)
    , m_matrix(matrix)
{
}

Ref<const Matrix3DTransformFunction> Matrix3DTransformFunction::create(const TransformationMatrix& matrix)
{
    return adoptRef(*new Matrix3DTransformFunction(matrix));
}

Ref<const TransformFunctionBase> Matrix3DTransformFunction::clone() const
{
    return adoptRef(*new Matrix3DTransformFunction(m_matrix));
}

Ref<TransformOperation> Matrix3DTransformFunction::toPlatform(const FloatSize&) const
{
    return Matrix3DTransformOperation::create(m_matrix);
}

bool Matrix3DTransformFunction::operator==(const TransformFunctionBase& other) const
{
    if (!isSameType(other))
        return false;

    auto& otherMatrix3D = downcast<Matrix3DTransformFunction>(other);
    return m_matrix == otherMatrix3D.m_matrix;
}

void Matrix3DTransformFunction::apply(TransformationMatrix& transform, const FloatSize&) const
{
    transform.multiply(m_matrix);
}

Ref<const TransformFunctionBase> Matrix3DTransformFunction::blend(const TransformFunctionBase* from, const BlendingContext& context, bool blendToIdentity) const
{
    auto createOperation = [](TransformationMatrix& to, TransformationMatrix& from, const BlendingContext& context) {
        to.blend(from, context.progress, context.compositeOperation);
        return Matrix3DTransformFunction::create(to);
    };

    if (!sharedPrimitiveType(from))
        return *this;

    // Convert the TransformFunctions into matrices
    TransformationMatrix toT = matrix();
    TransformationMatrix fromT;
    if (from)
        fromT = downcast<Matrix3DTransformFunction>(*from).matrix();

    if (blendToIdentity)
        return createOperation(fromT, toT, context);
    return createOperation(toT, fromT, context);
}

void Matrix3DTransformFunction::dump(TextStream& ts) const
{
    ts << type() << '(' << m_matrix << ')';
}

} // namespace Style
} // namespace WebCore
