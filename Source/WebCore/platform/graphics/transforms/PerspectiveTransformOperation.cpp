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

bool PerspectiveTransformOperation::operator==(const TransformOperation& other) const
{
    if (!isSameType(other))
        return false;
    return m_p == downcast<PerspectiveTransformOperation>(other).m_p;
}

Ref<TransformOperation> PerspectiveTransformOperation::blend(const TransformOperation* from, const BlendingContext& context, bool blendToIdentity)
{
    if (from && !from->isSameType(*this))
        return *this;
    
    if (blendToIdentity) {
        double p = floatValueForLength(m_p, 1);
        p = WebCore::blend(p, 1.0, context); // FIXME: this seems wrong. https://bugs.webkit.org/show_bug.cgi?id=52700
        return PerspectiveTransformOperation::create(Length(clampToPositiveInteger(p), LengthType::Fixed));
    }
    
    const PerspectiveTransformOperation* fromOp = downcast<PerspectiveTransformOperation>(from);
    Length fromP = fromOp ? fromOp->m_p : Length(m_p.type());
    Length toP = m_p;

    TransformationMatrix fromT;
    TransformationMatrix toT;
    fromT.applyPerspective(floatValueForLength(fromP, 1));
    toT.applyPerspective(floatValueForLength(toP, 1));
    toT.blend(fromT, context.progress);
    TransformationMatrix::Decomposed4Type decomp;
    toT.decompose4(decomp);

    if (decomp.perspectiveZ) {
        double val = -1.0 / decomp.perspectiveZ;
        return PerspectiveTransformOperation::create(Length(clampToPositiveInteger(val), LengthType::Fixed));
    }
    return PerspectiveTransformOperation::create(Length(0, LengthType::Fixed));
}

void PerspectiveTransformOperation::dump(TextStream& ts) const
{
    ts << type() << "(" << m_p << ")";
}

} // namespace WebCore
