/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "StyleMatrixTransformFunction.h"

#include "AnimationUtilities.h"
#include "MatrixTransformOperation.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <algorithm>

namespace WebCore {
namespace Style {

MatrixTransformFunction::MatrixTransformFunction(Number<> a, Number<> b, Number<> c, Number<> d, Number<> e, Number<> f)
    : TransformFunctionBase(TransformFunctionBase::Type::Matrix)
    , m_a(a)
    , m_b(b)
    , m_c(c)
    , m_d(d)
    , m_e(e)
    , m_f(f)
{
}

MatrixTransformFunction::MatrixTransformFunction(const TransformationMatrix& matrix)
    : MatrixTransformFunction(matrix.a(), matrix.b(), matrix.c(), matrix.d(), matrix.e(), matrix.f())
{
}

Ref<const MatrixTransformFunction> MatrixTransformFunction::createIdentity()
{
    return adoptRef(*new MatrixTransformFunction(1, 0, 0, 1, 0, 0));
}

Ref<const MatrixTransformFunction> MatrixTransformFunction::create(Number<> a, Number<> b, Number<> c, Number<> d, Number<> e, Number<> f)
{
    return adoptRef(*new MatrixTransformFunction(a, b, c, d, e, f));
}

Ref<const MatrixTransformFunction> MatrixTransformFunction::create(const TransformationMatrix& matrix)
{
    return adoptRef(*new MatrixTransformFunction(matrix));
}

Ref<const TransformFunctionBase> MatrixTransformFunction::clone() const
{
    return adoptRef(*new MatrixTransformFunction(m_a, m_b, m_c, m_d, m_e, m_f));
}

Ref<TransformOperation> MatrixTransformFunction::toPlatform(const FloatSize&) const
{
    return MatrixTransformOperation::create(m_a.value, m_b.value, m_c.value, m_d.value, m_e.value, m_f.value);
}

bool MatrixTransformFunction::operator==(const TransformFunctionBase& other) const
{
    if (!isSameType(other))
        return false;

    auto& otherMatrix = downcast<MatrixTransformFunction>(other);
    return m_a == otherMatrix.m_a
        && m_b == otherMatrix.m_b
        && m_c == otherMatrix.m_c
        && m_d == otherMatrix.m_d
        && m_e == otherMatrix.m_e
        && m_f == otherMatrix.m_f;
}

void MatrixTransformFunction::apply(TransformationMatrix& transform, const FloatSize&) const
{
    transform.multiply(matrix());
}

Ref<const TransformFunctionBase> MatrixTransformFunction::blend(const TransformFunctionBase* from, const BlendingContext& context, bool blendToIdentity) const
{
    auto createOperation = [] (TransformationMatrix& to, TransformationMatrix& from, const BlendingContext& context) {
        to.blend(from, context.progress, context.compositeOperation);
        return MatrixTransformFunction::create(to);
    };

    if (!sharedPrimitiveType(from))
        return *this;

    // convert the TransformFunctions into matrices
    TransformationMatrix toT = matrix();
    TransformationMatrix fromT;
    if (from)
        fromT = downcast<MatrixTransformFunction>(*from).matrix();

    if (blendToIdentity)
        return createOperation(fromT, toT, context);
    return createOperation(toT, fromT, context);
}

void MatrixTransformFunction::dump(TextStream& ts) const
{
    ts << '('
       << m_a << ", "_s
       << m_b << ", "_s
       << m_c << ", "_s
       << m_d << ", "_s
       << m_e << ", "_s
       << m_f << ')';
}

} // namespace Style
} // namespace WebCore
