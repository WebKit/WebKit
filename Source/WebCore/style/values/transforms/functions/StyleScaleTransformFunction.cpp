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
#include "StyleScaleTransformFunction.h"

#include "AnimationUtilities.h"
#include "ScaleTransformOperation.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

ScaleTransformFunction::ScaleTransformFunction(NumberOrPercentageResolvedToNumber<> x, NumberOrPercentageResolvedToNumber<> y, NumberOrPercentageResolvedToNumber<> z, TransformFunctionBase::Type type)
    : TransformFunctionBase(type)
    , m_x(x)
    , m_y(y)
    , m_z(z)
{
    RELEASE_ASSERT(isScaleTransformFunctionType(type));
}

Ref<const ScaleTransformFunction> ScaleTransformFunction::create(NumberOrPercentageResolvedToNumber<> x, NumberOrPercentageResolvedToNumber<> y, TransformFunctionBase::Type type)
{
    return adoptRef(*new ScaleTransformFunction(x, y, NumberOrPercentageResolvedToNumber<> { 1 }, type));
}

Ref<const ScaleTransformFunction> ScaleTransformFunction::create(NumberOrPercentageResolvedToNumber<> x, NumberOrPercentageResolvedToNumber<> y, NumberOrPercentageResolvedToNumber<> z, TransformFunctionBase::Type type)
{
    return adoptRef(*new ScaleTransformFunction(x, y, z, type));
}

Ref<const TransformFunctionBase> ScaleTransformFunction::clone() const
{
    return adoptRef(*new ScaleTransformFunction(m_x, m_y, m_z, type()));
}

Ref<TransformOperation> ScaleTransformFunction::toPlatform(const FloatSize&) const
{
    return ScaleTransformOperation::create(m_x.value.value, m_y.value.value, m_z.value.value, Style::toPlatform(type()));
}

bool ScaleTransformFunction::operator==(const TransformFunctionBase& other) const
{
    if (!isSameType(other))
        return false;

    auto& otherScale = downcast<ScaleTransformFunction>(other);
    return m_x == otherScale.m_x && m_y == otherScale.m_y && m_z == otherScale.m_z;
}

void ScaleTransformFunction::apply(TransformationMatrix& transform, const FloatSize&) const
{
    transform.scale3d(m_x.value.value, m_y.value.value, m_z.value.value);
}

static NumberOrPercentageResolvedToNumber<> blendScaleComponent(NumberOrPercentageResolvedToNumber<> from, NumberOrPercentageResolvedToNumber<> to, const BlendingContext& context)
{
    switch (context.compositeOperation) {
    case CompositeOperation::Replace:
        return Style::blend(from, to, context);
    case CompositeOperation::Add:
        ASSERT(context.progress == 1.0);
        return NumberOrPercentageResolvedToNumber<> { from.value.value * to.value.value };
    case CompositeOperation::Accumulate:
        ASSERT(context.progress == 1.0);
        return NumberOrPercentageResolvedToNumber<> { from.value.value + to.value.value - 1 };
    }
    RELEASE_ASSERT_NOT_REACHED();
}

Ref<const TransformFunctionBase> ScaleTransformFunction::blend(const TransformFunctionBase* from, const BlendingContext& context, bool blendToIdentity) const
{
    if (blendToIdentity)
        return ScaleTransformFunction::create(blendScaleComponent(m_x, 1.0, context), blendScaleComponent(m_y, 1.0, context), blendScaleComponent(m_z, 1.0, context), type());

    auto outputType = sharedPrimitiveType(from);
    if (!outputType)
        return *this;

    auto* fromOp = downcast<ScaleTransformFunction>(from);
    auto fromX = fromOp ? fromOp->m_x : NumberOrPercentageResolvedToNumber<> { 1.0 };
    auto fromY = fromOp ? fromOp->m_y : NumberOrPercentageResolvedToNumber<> { 1.0 };
    auto fromZ = fromOp ? fromOp->m_z : NumberOrPercentageResolvedToNumber<> { 1.0 };
    return ScaleTransformFunction::create(blendScaleComponent(fromX, m_x, context), blendScaleComponent(fromY, m_y, context), blendScaleComponent(fromZ, m_z, context), *outputType);
}

void ScaleTransformFunction::dump(TextStream& ts) const
{
    ts << type() << '(' << m_x << ", "_s << m_y << ", "_s << m_z << ')';
}

} // namespace Style
} // namespace WebCore
