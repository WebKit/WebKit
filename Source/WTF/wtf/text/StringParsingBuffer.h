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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

template<typename T>
class StringParsingBuffer final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using CharacterType = T;

    constexpr StringParsingBuffer() = default;

    constexpr StringParsingBuffer(std::span<const CharacterType> characters)
        : m_data { characters }
    {
        ASSERT(m_data.data() || m_data.empty());
    }

    constexpr auto position() const { return m_data.data(); }
    constexpr auto end() const { return m_data.data() + m_data.size(); }

    constexpr bool hasCharactersRemaining() const { return !m_data.empty(); }
    constexpr bool atEnd() const { return m_data.empty(); }

    constexpr size_t lengthRemaining() const { return m_data.size(); }

    constexpr void setPosition(const CharacterType* position)
    {
        ASSERT(position <= m_data.data() + m_data.size());
        m_data = { position, m_data.data() + m_data.size() };
    }

    StringView stringViewOfCharactersRemaining() const { return span(); }

    CharacterType consume()
    {
        ASSERT(hasCharactersRemaining());
        auto character = m_data.front();
        m_data = m_data.subspan(1);
        return character;
    }

    std::span<const CharacterType> span() const { return m_data; }

    std::span<const CharacterType> consume(size_t count)
    {
        ASSERT(count <= lengthRemaining());
        auto result = m_data;
        m_data = m_data.subspan(count);
        return result;
    }

    CharacterType operator[](size_t i) const
    {
        ASSERT(i < lengthRemaining());
        return m_data[i];
    }

    constexpr CharacterType operator*() const
    {
        ASSERT(hasCharactersRemaining());
        return m_data.front();
    }

    constexpr void advance()
    {
        ASSERT(hasCharactersRemaining());
        m_data = m_data.subspan(1);
    }

    constexpr void advanceBy(size_t places)
    {
        ASSERT(places <= lengthRemaining());
        m_data = m_data.subspan(places);
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
    std::span<const CharacterType> m_data;
};

template<typename StringType, typename Function> decltype(auto) readCharactersForParsing(StringType&& string, Function&& functor)
{
    if (string.is8Bit())
        return functor(StringParsingBuffer { string.span8() });
    return functor(StringParsingBuffer { string.span16() });
}

} // namespace WTF

using WTF::StringParsingBuffer;
using WTF::readCharactersForParsing;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
