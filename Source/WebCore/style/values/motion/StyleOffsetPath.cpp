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
#include "StyleOffsetPath.h"

#include "StylePrimitiveKeyword+CSSValueCreation.h"
#include "StylePrimitiveKeyword+Serialization.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<OffsetPath>::operator()(BuilderState& state, const CSSValue& value) -> OffsetPath
{
    return OffsetPath { toStyleFromCSSValue<RefPtr<PathOperation>>(state, value, SupportRayPathOperation::Yes) };
}

Ref<CSSValue> CSSValueCreation<OffsetPath>::operator()(CSSValuePool& pool, const RenderStyle& style, const OffsetPath& value)
{
    return WTF::switchOn(value,
        [&](const BasicShapePath& path) {
            return createCSSValue(pool, style, path, PathConversion::ForceAbsolute);
        },
        [&](const auto& path) {
            return createCSSValue(pool, style, path);
        }
    );
}

// MARK: - Serialization

void Serialize<OffsetPath>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const OffsetPath& value)
{
    return WTF::switchOn(value,
        [&](const BasicShapePath& path) {
            serializationForCSS(builder, context, style, path, PathConversion::ForceAbsolute);
        },
        [&](const auto& path) {
            serializationForCSS(builder, context, style, path);
        }
    );
}

// MARK: - Blending

auto Blending<OffsetPath>::canBlend(const OffsetPath& a, const OffsetPath& b) -> bool
{
    if (a.isNone() || b.isNone())
        return false;

    Ref aOperation = *a.operation;
    Ref bOperation = *b.operation;
    return aOperation->canBlend(bOperation);
}

auto Blending<OffsetPath>::blend(const OffsetPath& a, const OffsetPath& b, const BlendingContext& context) -> OffsetPath
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? b : a;
    }

    ASSERT(canBlend(a, b));
    Ref aOperation = *a.operation;
    Ref bOperation = *b.operation;
    return OffsetPath { aOperation->blend(bOperation.ptr(), context) };
}

// MARK: - Platform

auto ToPlatform<OffsetPath>::operator()(const OffsetPath& value) -> RefPtr<PathOperation>
{
    return value.operation;
}

} // namespace Style
} // namespace WebCore
