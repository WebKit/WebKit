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

#include "ParsingUtilities.h"
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

    typedef const LChar* Position;

    class Run {
    public:
        Run(Position start, Position end, bool is8Bit)
            : m_start(start), m_end(end), m_is8Bit(is8Bit) { }

        Position start() const { return m_start; }
        Position end() const { return m_end; }

        bool isEmpty() const { return m_start == m_end; }
        size_t length() const;

    private:
        Position m_start;
        Position m_end;
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
    bool scan(const LChar* characters, size_t charactersCount);

    // Scan the literal |characters|.
    template<unsigned charactersCount>
    bool scan(const char (&characters)[charactersCount]);

    // Skip (advance the input pointer) as long as the specified
    // |characterPredicate| returns true, and the input pointer is not passed
    // the end of the input.
    template<bool characterPredicate(UChar)>
    void skipWhile();

    // Like skipWhile, but using a negated predicate.
    template<bool characterPredicate(UChar)>
    void skipUntil();

    // Return the run of characters for which the specified
    // |characterPredicate| returns true. The start of the run will be the
    // current input pointer.
    template<bool characterPredicate(UChar)>
    Run collectWhile();

    // Like collectWhile, but using a negated predicate.
    template<bool characterPredicate(UChar)>
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
    // number that does not fit the 'int' type, |number| is set to INT_MAX.
    // Note: Does not handle sign.
    unsigned scanDigits(int& number);

    // Scan a floating point value on one of the forms: \d+\.? \d+\.\d+ \.\d+
    bool scanFloat(float& number, bool* isNegative = nullptr);

protected:
    Position position() const { return m_data.characters8; }
    Position end() const { return m_end.characters8; }
    void seekTo(Position);
    UChar currentChar() const;
    void advance(unsigned amount = 1);
    // Adapt a UChar-predicate to an LChar-predicate.
    // (For use with skipWhile/Until from ParsingUtilities.h).
    template<bool characterPredicate(UChar)>
    static inline bool LCharPredicateAdapter(LChar c) { return characterPredicate(c); }
    union {
        const LChar* characters8;
        const UChar* characters16;
    } m_data;
    union {
        const LChar* characters8;
        const UChar* characters16;
    } m_end;
    bool m_is8Bit;
};

inline size_t VTTScanner::Run::length() const
{
    if (m_is8Bit)
        return m_end - m_start;
    return reinterpret_cast<const UChar*>(m_end) - reinterpret_cast<const UChar*>(m_start);
}

template<unsigned charactersCount>
inline bool VTTScanner::scan(const char (&characters)[charactersCount])
{
    return scan(reinterpret_cast<const LChar*>(characters), charactersCount - 1);
}

template<bool characterPredicate(UChar)>
inline void VTTScanner::skipWhile()
{
    if (m_is8Bit)
        WebCore::skipWhile<LChar, LCharPredicateAdapter<characterPredicate> >(m_data.characters8, m_end.characters8);
    else
        WebCore::skipWhile<UChar, characterPredicate>(m_data.characters16, m_end.characters16);
}

template<bool characterPredicate(UChar)>
inline void VTTScanner::skipUntil()
{
    if (m_is8Bit)
        WebCore::skipUntil<LChar, LCharPredicateAdapter<characterPredicate> >(m_data.characters8, m_end.characters8);
    else
        WebCore::skipUntil<UChar, characterPredicate>(m_data.characters16, m_end.characters16);
}

template<bool characterPredicate(UChar)>
inline VTTScanner::Run VTTScanner::collectWhile()
{
    if (m_is8Bit) {
        const LChar* current = m_data.characters8;
        WebCore::skipWhile<LChar, LCharPredicateAdapter<characterPredicate> >(current, m_end.characters8);
        return Run(position(), current, m_is8Bit);
    }
    const UChar* current = m_data.characters16;
    WebCore::skipWhile<UChar, characterPredicate>(current, m_end.characters16);
    return Run(position(), reinterpret_cast<Position>(current), m_is8Bit);
}

template<bool characterPredicate(UChar)>
inline VTTScanner::Run VTTScanner::collectUntil()
{
    if (m_is8Bit) {
        const LChar* current = m_data.characters8;
        WebCore::skipUntil<LChar, LCharPredicateAdapter<characterPredicate> >(current, m_end.characters8);
        return Run(position(), current, m_is8Bit);
    }
    const UChar* current = m_data.characters16;
    WebCore::skipUntil<UChar, characterPredicate>(current, m_end.characters16);
    return Run(position(), reinterpret_cast<Position>(current), m_is8Bit);
}

inline void VTTScanner::seekTo(Position position)
{
    ASSERT(position <= end());
    m_data.characters8 = position;
}

inline UChar VTTScanner::currentChar() const
{
    ASSERT(position() < end());
    return m_is8Bit ? *m_data.characters8 : *m_data.characters16;
}

inline void VTTScanner::advance(unsigned amount)
{
    ASSERT(position() < end());
    if (m_is8Bit)
        m_data.characters8 += amount;
    else
        m_data.characters16 += amount;
}

} // namespace WebCore
