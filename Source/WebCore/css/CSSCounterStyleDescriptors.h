/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Forward.h>
#include <wtf/OptionSet.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class StyleProperties;

struct CSSCounterStyleDescriptors {
    using Name = AtomString;
    using Symbol = String;
    using Ranges = Vector<std::pair<int, int>>;
    using AdditiveSymbols = Vector<std::pair<Symbol, unsigned>>;
    // The keywords that can be used as values for the counter-style `system` descriptor.
    // https://www.w3.org/TR/css-counter-styles-3/#counter-style-system
    enum class System : uint8_t {
        Cyclic,
        Numeric,
        Alphabetic,
        Symbolic,
        Additive,
        Fixed,
        Extends
    };
    enum class SpeakAs : uint8_t {
        Auto,
        Bullets,
        Numbers,
        Words,
        SpellOut,
        CounterStyleNameReference,
    };
    struct Pad {
        unsigned m_padMinimumLength = 0;
        Symbol m_padSymbol = "-"_s;
        bool operator==(const Pad& other) const { return m_padMinimumLength == other.m_padMinimumLength && m_padSymbol == other.m_padSymbol; }
        bool operator!=(const Pad& other) const { return !(*this == other); }
    };
    struct NegativeSymbols {
        Symbol m_prefix = "-"_s;
        Symbol m_suffix;
        bool operator==(const NegativeSymbols& other) const { return m_prefix == other.m_prefix && m_suffix == other.m_suffix; }
        bool operator!=(const NegativeSymbols& other) const { return !(*this == other); }
    };
    enum class ExplicitlySetDescriptors: uint16_t {
        System = 1 << 0,
        Negative = 1 << 1,
        Prefix = 1 << 2,
        Suffix = 1 << 3,
        Range = 1 << 4,
        Pad = 1 << 5,
        Fallback = 1 << 6,
        Symbols = 1 << 7,
        AdditiveSymbols = 1 << 8,
        SpeakAs = 1 << 9
    };

    // create() is prefered here rather than a custom constructor, so that the Struct still classifies as an aggregate.
    static CSSCounterStyleDescriptors create(AtomString name, const StyleProperties&);
    bool operator==(const CSSCounterStyleDescriptors& other) const
    {
        return m_name == other.m_name
            && m_system == other.m_system
            && m_negativeSymbols == other.m_negativeSymbols
            && m_prefix == other.m_prefix
            && m_suffix == other.m_suffix
            && m_ranges == other.m_ranges
            && m_pad == other.m_pad
            && m_fallbackName == other.m_fallbackName
            && m_symbols == other.m_symbols
            && m_additiveSymbols == other.m_additiveSymbols
            && m_speakAs == other.m_speakAs
            && m_extendsName == other.m_extendsName
            && m_fixedSystemFirstSymbolValue == other.m_fixedSystemFirstSymbolValue
            && m_explicitlySetDescriptors == other.m_explicitlySetDescriptors;
    }
    bool operator!=(const CSSCounterStyleDescriptors& other) const { return !(*this == other); }
    void setExplicitlySetDescriptors(const StyleProperties&);

    Name m_name;
    System m_system;
    NegativeSymbols m_negativeSymbols;
    Symbol m_prefix;
    Symbol m_suffix;
    Ranges m_ranges;
    Pad m_pad;
    Name m_fallbackName;
    Vector<Symbol> m_symbols;
    AdditiveSymbols m_additiveSymbols;
    SpeakAs m_speakAs;
    Name m_extendsName;
    int m_fixedSystemFirstSymbolValue;
    OptionSet<ExplicitlySetDescriptors> m_explicitlySetDescriptors;
};

} // namespace WebCore
