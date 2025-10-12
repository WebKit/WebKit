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
#include "StyleTranslateTransformFunction.h"

#include "AnimationUtilities.h"
#include "StyleLengthWrapper+Blending.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include "TranslateTransformOperation.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

TranslateTransformFunction::TranslateTransformFunction(const LengthPercentage& x, const LengthPercentage& y, const Length& z, TransformFunctionBase::Type type)
    : TransformFunctionBase(type)
    , m_x(x)
    , m_y(y)
    , m_z(z)
{
    RELEASE_ASSERT(isTranslateTransformFunctionType(type));
}

Ref<const TranslateTransformFunction> TranslateTransformFunction::create(const LengthPercentage& x, const LengthPercentage& y, TransformFunctionBase::Type type)
{
    return adoptRef(*new TranslateTransformFunction(x, y, 0_css_px, type));
}

Ref<const TranslateTransformFunction> TranslateTransformFunction::create(const LengthPercentage& x, const LengthPercentage& y, const Length& z, TransformFunctionBase::Type type)
{
    return adoptRef(*new TranslateTransformFunction(x, y, z, type));
}

Ref<const TransformFunctionBase> TranslateTransformFunction::clone() const
{
    return adoptRef(*new TranslateTransformFunction(m_x, m_y, m_z, type()));
}

Ref<TransformOperation> TranslateTransformFunction::toPlatform(const FloatSize& borderBoxSize) const
{
    return TranslateTransformOperation::create(
        evaluate<float>(m_x, borderBoxSize.width(), ZoomNeeded { }),
        evaluate<float>(m_y, borderBoxSize.height(), ZoomNeeded { }),
        evaluate<float>(m_z, ZoomNeeded { }),
        Style::toPlatform(type())
    );
}

TransformFunctionSizeDependencies TranslateTransformFunction::computeSizeDependencies() const
{
    return {
        .isWidthDependent = m_x.isPercent(),
        .isHeightDependent = m_y.isPercent(),
    };
}

bool TranslateTransformFunction::operator==(const TransformFunctionBase& other) const
{
    if (!isSameType(other))
        return false;

    auto& otherTranslate = downcast<TranslateTransformFunction>(other);
    return m_x == otherTranslate.m_x && m_y == otherTranslate.m_y && m_z == otherTranslate.m_z;
}

void TranslateTransformFunction::apply(TransformationMatrix& transform, const FloatSize& borderBoxSize) const
{
    transform.translate3d(
        evaluate<float>(m_x, borderBoxSize.width(), ZoomNeeded { }),
        evaluate<float>(m_y, borderBoxSize.height(), ZoomNeeded { }),
        evaluate<float>(m_z, ZoomNeeded { })
    );
}

Ref<const TransformFunctionBase> TranslateTransformFunction::blend(const TransformFunctionBase* from, const BlendingContext& context, bool blendToIdentity) const
{
    if (blendToIdentity) {
        return TranslateTransformFunction::create(
            Style::blend(m_x, LengthPercentage { 0_css_px }, context),
            Style::blend(m_y, LengthPercentage { 0_css_px }, context),
            Style::blend(m_z, Length { 0_css_px }, context),
            type()
        );
    }

    auto outputType = sharedPrimitiveType(from);
    if (!outputType)
        return *this;

    RefPtr fromOp = downcast<TranslateTransformFunction>(from);
    auto fromX = fromOp ? fromOp->m_x : LengthPercentage { 0_css_px };
    auto fromY = fromOp ? fromOp->m_y : LengthPercentage { 0_css_px };
    auto fromZ = fromOp ? fromOp->m_z : Length { 0_css_px };

    return TranslateTransformFunction::create(
        Style::blend(fromX, x(), context),
        Style::blend(fromY, y(), context),
        Style::blend(fromZ, z(), context),
        *outputType
    );
}

void TranslateTransformFunction::dump(TextStream& ts) const
{
    ts << type() << '(' << m_x << ", "_s << m_y << ", "_s << m_z << ')';
}

} // namespace Style
} // namespace WebCore
