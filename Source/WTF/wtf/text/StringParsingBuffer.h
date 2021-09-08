/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <wtf/text/StringView.h>

namespace WTF {

template<typename T>
class StringParsingBuffer final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using CharacterType = T;

    constexpr StringParsingBuffer() = default;

    constexpr StringParsingBuffer(const CharacterType* characters, unsigned length)
        : m_position { characters }
        , m_end { characters + length }
    {
        ASSERT(characters || !length);
    }

    constexpr StringParsingBuffer(const CharacterType* characters, const CharacterType* end)
        : m_position { characters }
        , m_end { end }
    {
        ASSERT(characters <= end);
        ASSERT(!characters == !end);
        ASSERT(static_cast<size_t>(end - characters) <= std::numeric_limits<unsigned>::max());
    }

    constexpr auto position() const { return m_position; }
    constexpr auto end() const { return m_end; }

    constexpr bool hasCharactersRemaining() const { return m_position < m_end; }
    constexpr bool atEnd() const { return m_position == m_end; }

    constexpr unsigned lengthRemaining() const { return m_end - m_position; }

    StringView stringViewOfCharactersRemaining() const { return { m_position, lengthRemaining() }; }

    CharacterType operator[](unsigned i) const
    {
        ASSERT(i < lengthRemaining());
        return m_position[i];
    }

    constexpr CharacterType operator*() const
    {
        ASSERT(hasCharactersRemaining());
        return *m_position;
    }

    constexpr void advance()
    {
        ASSERT(hasCharactersRemaining());
        ++m_position;
    }

    constexpr void advanceBy(unsigned places)
    {
        ASSERT(places <= lengthRemaining());
        m_position += places;
    }

    constexpr StringParsingBuffer& operator++()
    {
        advance();
        return *this;
    }

    constexpr StringParsingBuffer operator++(int)
    {
        auto result = *this;
        ++*this;
        return result;
    }

    constexpr StringParsingBuffer& operator+=(int places)
    {
        advanceBy(places);
        return *this;
    }

private:
    const CharacterType* m_position { nullptr };
    const CharacterType* m_end { nullptr };
};

template<typename StringType, typename Function> decltype(auto) readCharactersForParsing(StringType&& string, Function&& functor)
{
    if (string.is8Bit())
        return functor(StringParsingBuffer { string.characters8(), string.length() });
    return functor(StringParsingBuffer { string.characters16(), string.length() });
}

} // namespace WTF

using WTF::StringParsingBuffer;
using WTF::readCharactersForParsing;
