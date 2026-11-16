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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/ScrollTypes.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'scrollbar-width'> = auto | thin | none
// https://drafts.csswg.org/css-scrollbars/#propdef-scrollbar-width
enum class ScrollbarWidth : uint8_t {
    Auto,
    Thin,
    None
};

// MARK: - Conversion

// `ScrollbarWidth` is special-cased to apply `needsScrollbarWidthThinDisabledQuirk` quirk.
template<> struct CSSValueConversion<ScrollbarWidth> { auto operator()(BuilderState&, const CSSValue&) -> ScrollbarWidth; };

// MARK: - Platform

template<> struct ToPlatform<ScrollbarWidth> {
    auto operator()(ScrollbarWidth value) -> WebCore::ScrollbarWidth
    {
        switch (value) {
        case ScrollbarWidth::Auto:
            return WebCore::ScrollbarWidth::Auto;
        case ScrollbarWidth::Thin:
            return WebCore::ScrollbarWidth::Thin;
        case ScrollbarWidth::None:
            return WebCore::ScrollbarWidth::None;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }
};

} // namespace Style
} // namespace WebCore
