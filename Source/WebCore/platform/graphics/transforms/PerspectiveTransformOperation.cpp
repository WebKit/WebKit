/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
#include "PerspectiveTransformOperation.h"

#include "AnimationUtilities.h"
#include <wtf/MathExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<PerspectiveTransformOperation> PerspectiveTransformOperation::create(const std::optional<Length>& p)
{
    return adoptRef(*new PerspectiveTransformOperation(p));
}

PerspectiveTransformOperation::PerspectiveTransformOperation(const std::optional<Length>& p)
    : TransformOperation(TransformOperation::Type::Perspective)
    , m_p(p)
{
    ASSERT(!p || (*p).isFixed());
}

bool PerspectiveTransformOperation::operator==(const TransformOperation& other) const
{
    if (!isSameType(other))
        return false;
    return m_p == downcast<PerspectiveTransformOperation>(other).m_p;
}

Ref<TransformOperation> PerspectiveTransformOperation::blend(const TransformOperation* from, const BlendingContext& context, bool blendToIdentity)
{
    if (!sharedPrimitiveType(from))
        return *this;

    // https://drafts.csswg.org/css-transforms-2/#interpolation-of-transform-functions
    // says that we should run matrix decomposition and then run the rules for
    // interpolation of matrices, but we know what those rules are going to
    // yield, so just do that directly.
    auto getInverse = [](const auto& operation) {
        return !operation->isIdentity() ? (1.0 / (*operation->floatValue())) : 0.0;
    };

    double ourInverse = getInverse(this);
    double fromPInverse, toPInverse;
    if (blendToIdentity) {
        fromPInverse = ourInverse;
        toPInverse = 0.0;
    } else {
        fromPInverse = from ? getInverse(downcast<PerspectiveTransformOperation>(from)) : 0.0;
        toPInverse = ourInverse;
    }

    double pInverse = WebCore::blend(fromPInverse, toPInverse, context);
    std::optional<Length> p;
    if (pInverse > 0.0 && std::isnormal(pInverse)) {
        p = Length(1.0 / pInverse, LengthType::Fixed);
    }
    return PerspectiveTransformOperation::create(p);
}

void PerspectiveTransformOperation::dump(TextStream& ts) const
{
    ts << type() << "(";
    if (!m_p)
        ts << "none";
    else
        ts << m_p;
    ts << ")";
}

} // namespace WebCore
