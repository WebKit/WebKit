/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
#include "StyleRotateProperty.h"

#include "StyleIndividualTransformProperties.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include "TransformList.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

auto RotateProperty::toPlatform(const FloatSize& referenceBox) const -> RotateProperty::Platform
{
    return WTF::switchOn(value,
        [](None) -> Platform {
            return { };
        },
        [&](const auto& value) -> Platform {
            return { { WebCore::TransformFunction { Style::toPlatform(value, referenceBox) } } };
        }
    );
}

// MARK: - Blending

auto Blending<RotateProperty>::blend(const RotateProperty& from, const RotateProperty& to, const BlendingContext& context) -> RotateProperty
{
    return blendProperty(from, to, context);
}

// MARK: - Text Stream

WTF::TextStream& operator<<(WTF::TextStream& ts, const RotateProperty& value)
{
    return WTF::switchOn(value.value, [&](const auto& value) -> WTF::TextStream& { return ts << value; });}

} // namespace Style
} // namespace WebCore
