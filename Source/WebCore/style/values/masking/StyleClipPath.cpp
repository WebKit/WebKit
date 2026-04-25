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
#include "StyleClipPath.h"

#include "StylePrimitiveNumericTypes+Blending.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<ClipPath>::operator()(BuilderState& state, const CSSValue& value) -> ClipPath
{
    return ClipPath { toStyleFromCSSValue<RefPtr<PathOperation>>(state, value, SupportRayPathOperation::No) };
}

// MARK: - Blending

auto Blending<ClipPath>::canBlend(const ClipPath& a, const ClipPath& b) -> bool
{
    if (a.isNone() || b.isNone())
        return false;

    Ref aOperation = *a.operation;
    Ref bOperation = *b.operation;
    return aOperation->canBlend(bOperation);
}

auto Blending<ClipPath>::blend(const ClipPath& a, const ClipPath& b, const BlendingContext& context) -> ClipPath
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? b : a;
    }

    ASSERT(canBlend(a, b));
    Ref aOperation = *a.operation;
    Ref bOperation = *b.operation;
    return ClipPath { aOperation->blend(bOperation.ptr(), context) };
}

} // namespace Style
} // namespace WebCore
