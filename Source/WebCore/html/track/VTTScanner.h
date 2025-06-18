/*
 * Copyright (c) 2013, Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// Helper class for "scanning" an input string and performing parsing of
// "micro-syntax"-like constructs.
//
// There's two primary operations: match and scan.
//
// The 'match' operation matches an explicitly or implicitly specified sequence
// against the characters ahead of the current input pointer, and returns true
// if the sequence can be matched.
//
// The 'scan' operation performs a 'match', and if the match is successful it
// advance the input pointer past the matched sequence.
class VTTScanner {
    WTF_MAKE_NONCOPYABLE(VTTScanner);
public:
    explicit VTTScanner(const String& line);

    using Position = const void*;

    class Run {
    public:
        explicit Run(std::span<const LChar> data)
            : m_is8Bit(true)
        {
            m_data.characters8 = data;
        }

        explicit Run(std::span<const char16_t> data)
            : m_is8Bit(false)
        {
            m_data.characters16 = data;
        }

        std::span<const LChar> span8() const { RELEASE_ASSERT(m_is8Bit); return m_data.characters8; }
        std::span<const char16_t> span16() const { RELEASE_ASSERT(!m_is8Bit); return m_data.characters16; }

        Position start() const
        {
            if (m_is8Bit)
                return m_data.characters8.data();
            return m_data.characters16.data();
        }
        Position end() const
        {
            if (m_is8Bit)
                return std::to_address(m_data.characters8.end());
            return std::to_address(m_data.characters16.end());
        }

        bool isEmpty() const { return !length(); }
        size_t length() const { return m_is8Bit ? m_data.characters8.size() : m_data.characters16.size(); }

    private:
        union Characters {
            Characters()
                : characters8()
            { }
            std::span<const LChar> characters8;
            std::span<const char16_t> characters16;
        } m_data;
        bool m_is8Bit;
    };

    // Check if the input pointer points at the specified position.
    bool isAt(Position checkPosition) const { return position() == checkPosition; }
    // Check if the input pointer points at the end of the input.
    bool isAtEnd() const { return position() == end(); }
    // Match the character |c| against the character at the input pointer (~lookahead).
    bool match(char c) const { return !isAtEnd() && currentChar() == c; }
    // Scan the character |c|.
    bool scan(char);
    // Scan the first |charactersCount| characters of the string |characters|.
    bool scan(std::span<const LChar> characters);

    // Skip (advance the input pointer) as long as the specified
    // |characterPredicate| returns true, and the input pointer is not passed
    // the end of the input.
    template<bool characterPredicate(char16_t)>
    void skipWhile();

    // Like skipWhile, but using a negated predicate.
    template<bool characterPredicate(char16_t)>
    void skipUntil();

    // Return the run of characters for which the specified
    // |characterPredicate| returns true. The start of the run will be the
    // current input pointer.
    template<bool characterPredicate(char16_t)>
    Run collectWhile();

    // Like collectWhile, but using a negated predicate.
    template<bool characterPredicate(char16_t)>
    Run collectUntil();

    // Scan the string |toMatch|, using the specified |run| as the sequence to
    // match against.
    bool scanRun(const Run&, const String& toMatch);

    // Skip to the end of the specified |run|.
    void skipRun(const Run&);

    // Return the String made up of the characters in |run|, and advance the
    // input pointer to the end of the run.
    String extractString(const Run&);

    // Return a String constructed from the rest of the input (between input
    // pointer and end of input), and advance the input pointer accordingly.
    String restOfInputAsString();

    // Scan a set of ASCII digits from the input. Return the number of digits
    // scanned, and set |number| to the computed value. If the digits make up a
    // number that does not fit the 'unsigned' type, |number| is set to UINT_MAX.
    // Note: Does not handle sign.
    unsigned scanDigits(unsigned& number);

    // Scan a floating point value on one of the forms: \d+\.? \d+\.\d+ \.\d+
    bool scanFloat(float& number, bool* isNegative = nullptr);

protected:
    Run createRun(Position start, Position end) const;
    Position position() const
    {
        if (m_is8Bit)
            return m_data.characters8.data();
        return m_data.characters16.data();
    }
    Position end() const
    {
        if (m_is8Bit)
            return std::to_address(m_data.characters8.end());
        return std::to_address(m_data.characters16.end());
    }
    void seekTo(Position);
    char16_t currentChar() const;
    void advance(size_t amount = 1);
    union Characters {
        Characters()
            : characters8()
        { }
        std::span<const LChar> characters8;
        std::span<const char16_t> characters16;
    } m_data;
    const String m_source;
    bool m_is8Bit;
};

template<bool characterPredicate(char16_t)>
inline void VTTScanner::skipWhile()
{
    if (m_is8Bit)
        WTF::skipWhile<LCharPredicateAdapter<characterPredicate>>(m_data.characters8);
    else
        WTF::skipWhile<characterPredicate>(m_data.characters16);
}

template<bool characterPredicate(char16_t)>
inline void VTTScanner::skipUntil()
{
    if (m_is8Bit)
        WTF::skipUntil<LCharPredicateAdapter<characterPredicate>>(m_data.characters8);
    else
        WTF::skipUntil<characterPredicate>(m_data.characters16);
}

template<bool characterPredicate(char16_t)>
inline VTTScanner::Run VTTScanner::collectWhile()
{
    if (m_is8Bit) {
        auto current = m_data.characters8;
        WTF::skipWhile<LCharPredicateAdapter<characterPredicate>>(current);
        return Run { m_data.characters8.first(current.data() - m_data.characters8.data()) };
    }
    auto current = m_data.characters16;
    WTF::skipWhile<characterPredicate>(current);
    return Run { m_data.characters16.first(current.data() - m_data.characters16.data()) };
}

template<bool characterPredicate(char16_t)>
inline VTTScanner::Run VTTScanner::collectUntil()
{
    if (m_is8Bit) {
        auto current = m_data.characters8;
        WTF::skipUntil<LCharPredicateAdapter<characterPredicate>>(current);
        return Run { m_data.characters8.first(current.data() - m_data.characters8.data()) };
    }
    auto current = m_data.characters16;
    WTF::skipUntil<characterPredicate>(current);
    return Run { m_data.characters16.first(current.data() - m_data.characters16.data()) };
}

inline void VTTScanner::seekTo(Position position)
{
    if (m_is8Bit) {
        auto span8 = m_source.span8();
        auto* position8 = static_cast<const LChar*>(position);
        RELEASE_ASSERT(position8 >= span8.data());
        m_data.characters8 = span8.subspan(position8 - span8.data());
    } else {
        auto span16 = m_source.span16();
        auto* position16 = static_cast<const char16_t*>(position);
        RELEASE_ASSERT(position16 >= span16.data());
        m_data.characters16 = span16.subspan(position16 - span16.data());
    }
}

inline char16_t VTTScanner::currentChar() const
{
    return m_is8Bit ? m_data.characters8.front() : m_data.characters16.front();
}

inline void VTTScanner::advance(size_t amount)
{
    if (m_is8Bit)
        skip(m_data.characters8, amount);
    else
        skip(m_data.characters16, amount);
}

} // namespace WebCore
