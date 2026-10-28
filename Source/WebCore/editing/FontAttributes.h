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

#include <WebCore/Font.h>
#include <WebCore/FontShadow.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleListStyleType.h>
#include <wtf/Platform.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS NSDictionary;
OBJC_CLASS NSPresentationIntent;
OBJC_CLASS NSTextList;

namespace WebCore {

struct TextList {
    Style::ListStyleType styleType { CSS::Keyword::None { } };
    int startingItemNumber { 0 };
    bool ordered { false };

#if PLATFORM(COCOA)
    RetainPtr<NSTextList> createTextList() const;
#endif
};

// A platform-neutral NSPresentationIntent for a block, without its identity or parent.
struct BlockPresentationIntent {
    enum class Kind : uint8_t { Paragraph, Header, OrderedList, UnorderedList, ListItem, CodeBlock, BlockQuote, ThematicBreak };

    Kind kind { Kind::Paragraph };
    uint8_t headerLevel { 0 };
    int ordinal { 0 };
    String languageHint;

    friend bool operator==(const BlockPresentationIntent&, const BlockPresentationIntent&) = default;

#if PLATFORM(COCOA)
    // Uses the innermost intent, making a list item, or a paragraph in one, its list. Returns null for table kinds,
    // invalid header levels and list items outside a list.
    WEBCORE_EXPORT static std::optional<BlockPresentationIntent> fromPresentationIntent(NSPresentationIntent *);
#endif
};

struct FontAttributes {
    enum class SubscriptOrSuperscript : uint8_t { None, Subscript, Superscript };
    enum class HorizontalAlignment : uint8_t { Left, Center, Right, Justify, Natural };

#if PLATFORM(COCOA)
    WEBCORE_EXPORT RetainPtr<NSDictionary> createDictionary() const;
#endif

    RefPtr<Font> font;
    Color backgroundColor;
    Color foregroundColor;
    FontShadow fontShadow;
    SubscriptOrSuperscript subscriptOrSuperscript { SubscriptOrSuperscript::None };
    HorizontalAlignment horizontalAlignment { HorizontalAlignment::Left };
    Vector<TextList> textLists;
    // From the outermost block to the innermost.
    Vector<BlockPresentationIntent> blockPresentationIntents;
    bool hasUnderline { false };
    bool hasStrikeThrough { false };
    bool hasMultipleFonts { false };
};

} // namespace WebCore
