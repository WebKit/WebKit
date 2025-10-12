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
#include "StylePerspectiveTransformFunction.h"

#include "AnimationUtilities.h"
#include "PerspectiveTransformOperation.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <wtf/MathExtras.h>

namespace WebCore {
namespace Style {

PerspectiveTransformFunction::PerspectiveTransformFunction(Perspective p)
    : TransformFunctionBase(TransformFunctionBase::Type::Perspective)
    , m_p(p)
{
}

Ref<const PerspectiveTransformFunction> PerspectiveTransformFunction::create(Perspective p)
{
    return adoptRef(*new PerspectiveTransformFunction(p));
}

Ref<const TransformFunctionBase> PerspectiveTransformFunction::clone() const
{
    return adoptRef(*new PerspectiveTransformFunction(m_p));
}

Ref<TransformOperation> PerspectiveTransformFunction::toPlatform(const FloatSize&) const
{
    return WTF::switchOn(m_p,
        [](const CSS::Keyword::None&) -> Ref<TransformOperation> {
            return PerspectiveTransformOperation::create(std::nullopt);
        },
        [](const Perspective::Length& value) -> Ref<TransformOperation> {
            return PerspectiveTransformOperation::create(evaluate<float>(value, ZoomNeeded { }));
        }
    );
}

bool PerspectiveTransformFunction::operator==(const TransformFunctionBase& other) const
{
    if (!isSameType(other))
        return false;
    auto& otherPerspective = downcast<PerspectiveTransformFunction>(other);
    return m_p == otherPerspective.m_p;
}

void PerspectiveTransformFunction::apply(TransformationMatrix& transform, const FloatSize&) const
{
    if (auto value = m_p.tryValue())
        transform.applyPerspective(std::max(1.0f, evaluate<float>(*value, ZoomNeeded { })));
}

std::optional<float> PerspectiveTransformFunction::unresolvedFloatValue() const
{
    return WTF::switchOn(m_p,
        [](const CSS::Keyword::None&) -> std::optional<float> {
            return std::nullopt;
        },
        [](const Perspective::Length& value) -> std::optional<float> {
            // From https://drafts.csswg.org/css-transforms-2/#perspective-property:
            // "As very small <length> values can produce bizarre rendering results and stress the numerical accuracy of
            // transform calculations, values less than 1px must be treated as 1px for rendering purposes. (This clamping
            // does not affect the underlying value, so perspective: 0; in a stylesheet will still serialize back as 0.)"
            return std::max(1.0f, value.unresolvedValue());
        }
    );
}

Ref<const TransformFunctionBase> PerspectiveTransformFunction::blend(const TransformFunctionBase* from, const BlendingContext& context, bool blendToIdentity) const
{
    if (!sharedPrimitiveType(from))
        return *this;

    // https://drafts.csswg.org/css-transforms-2/#interpolation-of-transform-functions
    // says that we should run matrix decomposition and then run the rules for
    // interpolation of matrices, but we know what those rules are going to
    // yield, so just do that directly.
    auto getInverse = [](const auto& operation) {
        if (auto value = operation->unresolvedFloatValue())
            return 1.0 / *value;
        return 0.0;
    };

    double ourInverse = getInverse(this);
    double fromPInverse, toPInverse;
    if (blendToIdentity) {
        fromPInverse = ourInverse;
        toPInverse = 0.0;
    } else {
        fromPInverse = from ? getInverse(downcast<PerspectiveTransformFunction>(from)) : 0.0;
        toPInverse = ourInverse;
    }

    double pInverse = WebCore::blend(fromPInverse, toPInverse, context);
    if (pInverse > 0.0 && std::isnormal(pInverse))
        return PerspectiveTransformFunction::create(Perspective::Length { static_cast<float>(1.0 / pInverse) });
    return PerspectiveTransformFunction::create(CSS::Keyword::None { });
}

void PerspectiveTransformFunction::dump(TextStream& ts) const
{
    ts << type() << '(' << m_p << ')';
}

} // namespace Style
} // namespace WebCore
