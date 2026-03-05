/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleValueTypes.h>
#include <wtf/OptionSet.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// text-decoration-line = none | [ underline || overline || line-through || blink ] | spelling-error | grammar-error
// https://www.w3.org/TR/css-text-decor-4/#text-decoration-line-property

// We are representing TextDecorationLine in 5 bits.
// 1 bit is used for defining the Type (SingleValue or Flags)
// 4 bits are used for defining the Value
// Values for SingleValue: None, SpellingError, GrammarError
// Values for Flags: Any combination of Underline, Overline, LineThrough, Blink
// Therefore, we are packing its content with the following layout:
// Bits 7-5 : Reserved
// Bit 4    : Type (SingleValue or Flags)
// Bits 3-0 : When Type=1 (Underline=0x1, Overline=0x2, LineThrough=0x4, Blink=0x8)
//          : When Type=0 (None = 0, SpellingError = 1, GrammarError = 2)
struct TextDecorationLine {
    enum class Type : uint8_t {
        SingleValue   = 0,
        Flags         = 1 << 4
    };

    static constexpr uint8_t TypeMask = 1 << 4; // 0001 0000
    static constexpr uint8_t ValuesMask = 0x0F;

    // Values when Type is SingleValue
    enum class SingleValue : uint8_t {
        None  = 0,
        SpellingError,
        GrammarError
    };

    enum class Flag : uint8_t {
        Underline     = 1 << 0,
        Overline      = 1 << 1,
        LineThrough   = 1 << 2,
        Blink         = 1 << 3,
    };

    // Values when Type is Flags
    static constexpr uint8_t UnderlineBit   = static_cast<uint8_t>(TextDecorationLine::Flag::Underline);
    static constexpr uint8_t OverlineBit    = static_cast<uint8_t>(TextDecorationLine::Flag::Overline);
    static constexpr uint8_t LineThroughBit = static_cast<uint8_t>(TextDecorationLine::Flag::LineThrough);
    static constexpr uint8_t BlinkBit       = static_cast<uint8_t>(TextDecorationLine::Flag::Blink);

    static constexpr uint8_t SingleValueNone          = static_cast<uint8_t>(Type::SingleValue) | static_cast<uint8_t>(SingleValue::None);
    static constexpr uint8_t SingleValueSpellingError = static_cast<uint8_t>(Type::SingleValue) | static_cast<uint8_t>(SingleValue::SpellingError);
    static constexpr uint8_t SingleValueGrammarError  = static_cast<uint8_t>(Type::SingleValue) | static_cast<uint8_t>(SingleValue::GrammarError);

    constexpr TextDecorationLine() = default;

    constexpr TextDecorationLine(CSS::Keyword::None)
        : m_packed(SingleValueNone)
    {
    }

    constexpr TextDecorationLine(CSS::Keyword::SpellingError)
        : m_packed(SingleValueSpellingError)
    {
    }

    constexpr TextDecorationLine(CSS::Keyword::GrammarError)
        : m_packed(SingleValueGrammarError)
    {
    }

    constexpr TextDecorationLine(OptionSet<TextDecorationLine::Flag> flags)
        : m_packed(flags.isEmpty() ? SingleValueNone : packFlags(flags))
    {
    }

    constexpr TextDecorationLine(TextDecorationLine::Flag flag)
        : TextDecorationLine(OptionSet<TextDecorationLine::Flag>(flag))
    {
    }

    static constexpr TextDecorationLine fromRaw(uint8_t rawValue) { return TextDecorationLine(rawValue); }
    constexpr uint8_t toRaw() const { return m_packed; }

    constexpr Type type() const { return static_cast<Type>(m_packed & TypeMask); }
    constexpr bool isNone() const { return m_packed == SingleValueNone; }
    constexpr bool isSpellingError() const { return m_packed == SingleValueSpellingError; }
    constexpr bool isGrammarError() const { return m_packed == SingleValueGrammarError; }
    constexpr bool isFlags() const { return type() == Type::Flags; }

    constexpr bool hasUnderline() const
    {
        return (isFlags()) && (m_packed & UnderlineBit);
    }

    constexpr bool hasOverline() const
    {
        return (isFlags()) && (m_packed & OverlineBit);
    }

    constexpr bool hasLineThrough() const
    {
        return (isFlags()) && (m_packed & LineThroughBit);
    }

    constexpr bool hasBlink() const
    {
        return (isFlags()) && (m_packed & BlinkBit);
    }

    constexpr bool containsAny(OptionSet<TextDecorationLine::Flag> options) const
    {
        if (!isFlags())
            return false;
        return (m_packed & packFlags(options));
    }

    constexpr bool contains(TextDecorationLine::Flag option) const
    {
        if (!isFlags())
            return false;
        return (m_packed & packFlagValue(option));
    }

    constexpr void remove(TextDecorationLine::Flag option)
    {
        if (type() == Type::Flags) {
            m_packed &= ~packFlagValue(option);
            // If none flags are set we should represent this as Type::None
            if (!(m_packed & ValuesMask))
                setNone();
        }
    }

    void addOrReplaceIfNotNone(TextDecorationLine);

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (type()) {
        case Type::Flags:
            return visitor(unpackFlags());
        case Type::SingleValue:
            if (isNone())
                return visitor(CSS::Keyword::None { });
            if (isSpellingError())
                return visitor(CSS::Keyword::SpellingError { });
            ASSERT(isGrammarError());
            return visitor(CSS::Keyword::GrammarError { });
        }
        ASSERT_NOT_REACHED();
        return visitor(CSS::Keyword::None { });
    }

    constexpr void setNone() { m_packed = SingleValueNone; }
    constexpr void setSpellingError() { m_packed = SingleValueSpellingError; }
    constexpr void setGrammarError() { m_packed = SingleValueGrammarError; }
    constexpr void setFlags(OptionSet<TextDecorationLine::Flag> flags)
    {
        if (isFlags())
            m_packed |= packFlags(flags);
        else
            m_packed = packFlags(flags);
    }

    constexpr operator bool() const { return !isNone(); }
    constexpr bool operator==(const TextDecorationLine&) const = default;

    static constexpr uint8_t packFlags(OptionSet<TextDecorationLine::Flag> flags)
    {
        uint8_t result = static_cast<uint8_t>(Type::Flags);
        if (flags.contains(TextDecorationLine::Flag::Underline))
            result |= UnderlineBit;
        if (flags.contains(TextDecorationLine::Flag::Overline))
            result |= OverlineBit;
        if (flags.contains(TextDecorationLine::Flag::LineThrough))
            result |= LineThroughBit;
        if (flags.contains(TextDecorationLine::Flag::Blink))
            result |= BlinkBit;
        return result;
    }

private:
    constexpr TextDecorationLine(uint8_t rawValue)
        : m_packed(rawValue)
    {
    }

    // Returns only the value bits, not to be confused with "toRaw", which returns the whole packed raw representation
    constexpr uint8_t rawValue() const { return m_packed & ValuesMask; }

    // Note that this function packs only the 'Value' bit, ignoring the Type. This is useful for bitwise operations.
    static constexpr uint8_t packFlagValue(TextDecorationLine::Flag flag)
    {
        switch (flag) {
        case TextDecorationLine::Flag::Underline:
            return UnderlineBit;
        case TextDecorationLine::Flag::Overline:
            return OverlineBit;
        case TextDecorationLine::Flag::LineThrough:
            return LineThroughBit;
        case TextDecorationLine::Flag::Blink:
            return BlinkBit;
        }
        ASSERT_NOT_REACHED();
        return 0;
    }

    constexpr OptionSet<TextDecorationLine::Flag> unpackFlags() const
    {
        ASSERT(isFlags());
        OptionSet<TextDecorationLine::Flag> flags;
        if (m_packed & UnderlineBit)
            flags.add(TextDecorationLine::Flag::Underline);
        if (m_packed & OverlineBit)
            flags.add(TextDecorationLine::Flag::Overline);
        if (m_packed & LineThroughBit)
            flags.add(TextDecorationLine::Flag::LineThrough);
        if (m_packed & BlinkBit)
            flags.add(TextDecorationLine::Flag::Blink);
        return flags;
    }

    uint8_t m_packed { 0 };
};

// MARK: - Conversion

template<> struct CSSValueConversion<TextDecorationLine> {
    auto operator()(BuilderState&, const CSSValue&) -> TextDecorationLine;
};

template<> struct CSSValueCreation<OptionSet<TextDecorationLine::Flag>> {
    auto operator()(CSSValuePool&, const RenderStyle&, const  OptionSet<TextDecorationLine::Flag>&) -> Ref<CSSValue>;
};

// MARK: Serialization

template<> struct Serialize<OptionSet<TextDecorationLine::Flag>> {
    void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const OptionSet<TextDecorationLine::Flag>&);
};

WTF::TextStream& operator<<(WTF::TextStream&, const TextDecorationLine&);

} // namespace Style

WTF::TextStream& operator<<(WTF::TextStream&, Style::TextDecorationLine::Flag);

} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TextDecorationLine)
