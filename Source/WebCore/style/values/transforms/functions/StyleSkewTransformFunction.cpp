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
#include "StyleSkewTransformFunction.h"

#include "AnimationUtilities.h"
#include "SkewTransformOperation.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

SkewTransformFunction::SkewTransformFunction(Angle<> angleX, Angle<> angleY, TransformFunctionBase::Type type)
    : TransformFunctionBase(type)
    , m_angleX(angleX)
    , m_angleY(angleY)
{
    RELEASE_ASSERT(isSkewTransformFunctionType(type));
}

Ref<const SkewTransformFunction> SkewTransformFunction::create(Angle<> angleX, Angle<> angleY, TransformFunctionBase::Type type)
{
    return adoptRef(*new SkewTransformFunction(angleX, angleY, type));
}

Ref<const TransformFunctionBase> SkewTransformFunction::clone() const
{
    return adoptRef(*new SkewTransformFunction(m_angleX, m_angleY, type()));
}

Ref<TransformOperation> SkewTransformFunction::toPlatform(const FloatSize&) const
{
    return SkewTransformOperation::create(m_angleX.value, m_angleY.value, Style::toPlatform(type()));
}

bool SkewTransformFunction::operator==(const TransformFunctionBase& other) const
{
    if (!isSameType(other))
        return false;

    auto& otherSkew = downcast<SkewTransformFunction>(other);
    return m_angleX == otherSkew.m_angleX && m_angleY == otherSkew.m_angleY;
}

void SkewTransformFunction::apply(TransformationMatrix& transform, const FloatSize&) const
{
    transform.skew(m_angleX.value, m_angleY.value);
}

Ref<const TransformFunctionBase> SkewTransformFunction::blend(const TransformFunctionBase* from, const BlendingContext& context, bool blendToIdentity) const
{
    if (blendToIdentity)
        return SkewTransformFunction::create(Style::blend(m_angleX, Angle<> { 0.0 }, context), Style::blend(m_angleY, Angle<> { 0.0 }, context), type());

    auto outputType = sharedPrimitiveType(from);
    if (!outputType)
        return *this;

    auto* fromOp = downcast<SkewTransformFunction>(from);
    auto fromAngleX = fromOp ? fromOp->m_angleX : Angle<> { 0 };
    auto fromAngleY = fromOp ? fromOp->m_angleY : Angle<> { 0 };
    return SkewTransformFunction::create(Style::blend(fromAngleX, m_angleX, context), Style::blend(fromAngleY, m_angleY, context), *outputType);
}

void SkewTransformFunction::dump(TextStream& ts) const
{
    ts << type() << '(' << m_angleX << ", "_s << m_angleY << ')';
}

} // namespace Style
} // namespace WebCore
