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

#pragma once

#include "StyleTransformFunctions.h"

namespace WebCore {
namespace Style {

// Helper for the "individual" transform CSS properties, `rotate`, `scale`, and `translate`.

template<typename Property> Property blendProperty(const Property& from, const Property& to, const BlendingContext& context)
{
    return std::visit(WTF::makeVisitor(
        [&](const None&, const None&) -> Property {
            return { None { } };
        },
        [&]<typename From>(const From& from, const None&) -> Property {
            return { Style::blend(from, From::identity(), context) };
        },
        [&]<typename To>(const None&, const To& to) -> Property {
            return { Style::blend(To::identity(), to, context) };
        },
        [&]<typename From, typename To>(const From& from, const To& to) -> Property {
            if constexpr (std::is_same_v<From, To>)
                return { Style::blend(from, to, context) };
            else if constexpr (HaveSharedPrimitive2DTransform<From, To>())
                return { Style::blend(from.toPrimitive2DTransform(), to.toPrimitive2DTransform(), context) };
            else if constexpr (HaveSharedPrimitive3DTransform<From, To>())
                return { Style::blend(from.toPrimitive3DTransform(), to.toPrimitive3DTransform(), context) };
        }
    ), from.value, to.value);
}

} // namespace Style
} // namespace WebCore
