/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "FontShadow.h"
#include "RenderStyleConstants.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS NSDictionary;
OBJC_CLASS NSFont;
OBJC_CLASS NSTextList;
OBJC_CLASS UIFont;

namespace WebCore {

struct TextList {
    ListStyleType style { ListStyleType::None };
    int startingItemNumber { 0 };
    bool ordered { false };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<TextList> decode(Decoder&);

#if PLATFORM(COCOA)
    RetainPtr<NSTextList> createTextList() const;
#endif
};

template<class Encoder> inline void TextList::encode(Encoder& encoder) const
{
    encoder << static_cast<uint8_t>(style) << startingItemNumber << ordered;
}

template<class Decoder> inline std::optional<TextList> TextList::decode(Decoder& decoder)
{
    std::optional<uint8_t> style;
    decoder >> style;
    if (!style)
        return std::nullopt;

    std::optional<int> startingItemNumber;
    decoder >> startingItemNumber;
    if (!startingItemNumber)
        return std::nullopt;

    std::optional<bool> ordered;
    decoder >> ordered;
    if (!ordered)
        return std::nullopt;

    return {{ static_cast<ListStyleType>(WTFMove(*style)), WTFMove(*startingItemNumber), WTFMove(*ordered) }};
}

struct FontAttributes {
    enum class SubscriptOrSuperscript : uint8_t { None, Subscript, Superscript };
    enum class HorizontalAlignment : uint8_t { Left, Center, Right, Justify, Natural };

#if PLATFORM(COCOA)
    WEBCORE_EXPORT RetainPtr<NSDictionary> createDictionary() const;
#endif

#if PLATFORM(MAC)
    RetainPtr<NSFont> font;
#elif PLATFORM(IOS_FAMILY)
    RetainPtr<UIFont> font;
#endif
    Color backgroundColor;
    Color foregroundColor;
    FontShadow fontShadow;
    SubscriptOrSuperscript subscriptOrSuperscript { SubscriptOrSuperscript::None };
    HorizontalAlignment horizontalAlignment { HorizontalAlignment::Left };
    Vector<TextList> textLists;
    bool hasUnderline { false };
    bool hasStrikeThrough { false };
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::FontAttributes::SubscriptOrSuperscript> {
    using values = EnumValues<
        WebCore::FontAttributes::SubscriptOrSuperscript,
        WebCore::FontAttributes::SubscriptOrSuperscript::None,
        WebCore::FontAttributes::SubscriptOrSuperscript::Subscript,
        WebCore::FontAttributes::SubscriptOrSuperscript::Superscript
    >;
};

template<> struct EnumTraits<WebCore::FontAttributes::HorizontalAlignment> {
    using values = EnumValues<
        WebCore::FontAttributes::HorizontalAlignment,
        WebCore::FontAttributes::HorizontalAlignment::Left,
        WebCore::FontAttributes::HorizontalAlignment::Center,
        WebCore::FontAttributes::HorizontalAlignment::Right,
        WebCore::FontAttributes::HorizontalAlignment::Justify,
        WebCore::FontAttributes::HorizontalAlignment::Natural
    >;
};

} // namespace WTF
