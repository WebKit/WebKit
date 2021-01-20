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
#include "Matrix3DTransformOperation.h"

#include <algorithm>
#include <wtf/text/TextStream.h>

namespace WebCore {

bool Matrix3DTransformOperation::operator==(const TransformOperation& other) const
{
    return isSameType(other) && m_matrix == downcast<Matrix3DTransformOperation>(other).m_matrix;
}

static Ref<TransformOperation> createOperation(TransformationMatrix& to, TransformationMatrix& from, double progress)
{
    to.blend(from, progress);
    return Matrix3DTransformOperation::create(to);
}

Ref<TransformOperation> Matrix3DTransformOperation::blend(const TransformOperation* from, double progress, bool blendToIdentity)
{
    if (from && !from->isSameType(*this))
        return *this;

    // Convert the TransformOperations into matrices
    FloatSize size;
    TransformationMatrix fromT;
    TransformationMatrix toT;
    if (from)
        from->apply(fromT, size);

    apply(toT, size);

    if (blendToIdentity)
        return createOperation(fromT, toT, progress);
    return createOperation(toT, fromT, progress);
}

bool Matrix3DTransformOperation::isRepresentableIn2D() const
{
    return m_matrix.isAffine();
}

void Matrix3DTransformOperation::dump(TextStream& ts) const
{
    ts << type() << "(" << m_matrix << ")";
}

} // namespace WebCore
