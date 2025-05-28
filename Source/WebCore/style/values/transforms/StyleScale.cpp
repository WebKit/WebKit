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
#include "StyleScale.h"

#include "StylePrimitiveNumericTypes+Blending.h"

namespace WebCore {
namespace Style {

// MARK: - Blending

auto Blending<Scale>::blend(const Scale& from, const Scale& to, const BlendingContext& context) -> Scale
{
    RefPtr fromOperation = from.operation();
    RefPtr toOperation = to.operation();

    if (!fromOperation && !toOperation)
        return Scale { CSS::Keyword::None { } };

    if (!fromOperation)
        fromOperation = ScaleTransformOperation::create(1, 1, 1, toOperation->type());
    else if (!toOperation)
        toOperation = ScaleTransformOperation::create(1, 1, 1, fromOperation->type());

    // Ensure the two transforms have the same type.
    if (!fromOperation->isSameType(*toOperation)) {
        RefPtr<ScaleTransformOperation> normalizedFrom;
        RefPtr<ScaleTransformOperation> normalizedTo;
        if (fromOperation->is3DOperation() || toOperation->is3DOperation()) {
            normalizedFrom = ScaleTransformOperation::create(fromOperation->x(), fromOperation->y(), fromOperation->z(), TransformOperation::Type::Scale3D);
            normalizedTo = ScaleTransformOperation::create(toOperation->x(), toOperation->y(), toOperation->z(), TransformOperation::Type::Scale3D);
        } else {
            normalizedFrom = ScaleTransformOperation::create(fromOperation->x(), fromOperation->y(), TransformOperation::Type::Scale);
            normalizedTo = ScaleTransformOperation::create(toOperation->x(), toOperation->y(), TransformOperation::Type::Scale);
        }
        return blend(Scale { normalizedFrom.releaseNonNull() }, Scale { normalizedTo.releaseNonNull() }, context);
    }

    if (auto blendedOperation = toOperation->blend(fromOperation.get(), context); auto scale = dynamicDowncast<ScaleTransformOperation>(blendedOperation.get()))
        return Scale { ScaleTransformOperation::create(scale->x(), scale->y(), scale->z(), scale->type()) };

    return Scale { CSS::Keyword::None { } };
}

} // namespace Style
} // namespace WebCore
