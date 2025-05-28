/*
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleRotate.h"

#include "StylePrimitiveNumericTypes+Blending.h"

namespace WebCore {
namespace Style {

// MARK: - Blending

auto Blending<Rotate>::blend(const Rotate& from, const Rotate& to, const BlendingContext& context) -> Rotate
{
    RefPtr fromOperation = from.operation();
    RefPtr toOperation = to.operation();

    if (!fromOperation && !toOperation)
        return Rotate { CSS::Keyword::None { } };

    if (!fromOperation)
        fromOperation = RotateTransformOperation::create(0, toOperation->type());
    else if (!toOperation)
        toOperation = RotateTransformOperation::create(0, fromOperation->type());

    // Ensure the two transforms have the same type.
    if (!fromOperation->isSameType(*toOperation)) {
        RefPtr<RotateTransformOperation> normalizedFrom;
        RefPtr<RotateTransformOperation> normalizedTo;
        if (fromOperation->is3DOperation() || toOperation->is3DOperation()) {
            normalizedFrom = RotateTransformOperation::create(fromOperation->x(), fromOperation->y(), fromOperation->z(), fromOperation->angle(), TransformOperation::Type::Rotate3D);
            normalizedTo = RotateTransformOperation::create(toOperation->x(), toOperation->y(), toOperation->z(), toOperation->angle(), TransformOperation::Type::Rotate3D);
        } else {
            normalizedFrom = RotateTransformOperation::create(fromOperation->angle(), TransformOperation::Type::Rotate);
            normalizedTo = RotateTransformOperation::create(toOperation->angle(), TransformOperation::Type::Rotate);
        }
        return blend(Rotate { normalizedFrom.releaseNonNull() }, Rotate { normalizedTo.releaseNonNull() }, context);
    }

    if (auto blendedOperation = toOperation->blend(fromOperation.get(), context); auto rotate = dynamicDowncast<RotateTransformOperation>(blendedOperation.get()))
        return Rotate { RotateTransformOperation::create(rotate->x(), rotate->y(), rotate->z(), rotate->angle(), rotate->type()) };

    return Rotate { CSS::Keyword::None { } };
}

} // namespace Style
} // namespace WebCore
