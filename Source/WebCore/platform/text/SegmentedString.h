/*
    Copyright (C) 2004-2016 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#pragma once

#include <wtf/Deque.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// FIXME: This should not start with "k".
// FIXME: This is a shared tokenizer concept, not a SegmentedString concept, but this is the only common header for now.
constexpr LChar kEndOfFileMarker = 0;

class SegmentedString {
public:
    SegmentedString() = default;
    SegmentedString(String&&);
    SegmentedString(const String&);

    SegmentedString(SegmentedString&&) = delete;
    SegmentedString(const SegmentedString&) = delete;

    SegmentedString& operator=(SegmentedString&&);
    SegmentedString& operator=(const SegmentedString&) = default;

    void clear();
    void close();

    void append(SegmentedString&&);
    void append(const SegmentedString&);

    void append(String&&);
    void append(const String&);

    void pushBack(String&&);

    void setExcludeLineNumbers();

    bool isEmpty() const { return !m_currentSubstring.length; }
    unsigned length() const;

    bool isClosed() const { return m_isClosed; }

    void advance();
    void advancePastNonNewline(); // Faster than calling advance when we know the current character is not a newline.
    void advancePastNewline(); // Faster than calling advance when we know the current character is a newline.

    enum AdvancePastResult { DidNotMatch, DidMatch, NotEnoughCharacters };
    template<unsigned length> AdvancePastResult advancePast(const char (&literal)[length]) { return advancePast<length, false>(literal); }
    template<unsigned length> AdvancePastResult advancePastLettersIgnoringASCIICase(const char (&literal)[length]) { return advancePast<length, true>(literal); }

    unsigned numberOfCharactersConsumed() const;

    String toString() const;

    UChar currentCharacter() const { return m_currentCharacter; }

    OrdinalNumber currentColumn() const;
    OrdinalNumber currentLine() const;

    // Sets value of line/column variables. Column is specified indirectly by a parameter columnAfterProlog
    // which is a value of column that we should get after a prolog (first prologLength characters) has been consumed.
    void setCurrentPosition(OrdinalNumber line, OrdinalNumber columnAfterProlog, int prologLength);

private:
    struct Substring {
        Substring() = default;
        Substring(String&&);

        UChar currentCharacter() const;
        UChar currentCharacterPreIncrement();

        unsigned numberOfCharactersConsumed() const;
        void appendTo(StringBuilder&) const;

        String string;
        unsigned length { 0 };
        bool is8Bit;
        union {
            const LChar* currentCharacter8;
            const UChar* currentCharacter16;
        };
        bool doNotExcludeLineNumbers { true };
    };

    enum FastPathFlags {
        NoFastPath = 0,
        Use8BitAdvanceAndUpdateLineNumbers = 1 << 0,
        Use8BitAdvance = 1 << 1,
    };

    void appendSubstring(Substring&&);

    void processPossibleNewline();
    void startNewLine();

    void advanceWithoutUpdatingLineNumber();
    void advanceWithoutUpdatingLineNumber16();
    void advanceAndUpdateLineNumber16();
    void advancePastSingleCharacterSubstringWithoutUpdatingLineNumber();
    void advancePastSingleCharacterSubstring();
    void advanceEmpty();

    void updateAdvanceFunctionPointers();
    void updateAdvanceFunctionPointersForEmptyString();
    void updateAdvanceFunctionPointersForSingleCharacterSubstring();

    void decrementAndCheckLength();

    template<typename CharacterType> static bool characterMismatch(CharacterType, char, bool lettersIgnoringASCIICase);
    template<unsigned length, bool lettersIgnoringASCIICase> AdvancePastResult advancePast(const char (&literal)[length]);
    AdvancePastResult advancePastSlowCase(const char* literal, bool lettersIgnoringASCIICase);

    Substring m_currentSubstring;
    Deque<Substring> m_otherSubstrings;

    bool m_isClosed { false };

    UChar m_currentCharacter { 0 };

    unsigned m_numberOfCharactersConsumedPriorToCurrentSubstring { 0 };
    unsigned m_numberOfCharactersConsumedPriorToCurrentLine { 0 };
    int m_currentLine { 0 };

    unsigned char m_fastPathFlags { NoFastPath };
    void (SegmentedString::*m_advanceWithoutUpdatingLineNumberFunction)() { &SegmentedString::advanceEmpty };
    void (SegmentedString::*m_advanceAndUpdateLineNumberFunction)() { &SegmentedString::advanceEmpty };
};

inline SegmentedString::Substring::Substring(String&& passedString)
    : string(WTFMove(passedString))
    , length(string.length())
{
    if (length) {
        is8Bit = string.impl()->is8Bit();
        if (is8Bit)
            currentCharacter8 = string.impl()->characters8();
        else
            currentCharacter16 = string.impl()->characters16();
    }
}

inline unsigned SegmentedString::Substring::numberOfCharactersConsumed() const
{
    return string.length() - length;
}

ALWAYS_INLINE UChar SegmentedString::Substring::currentCharacter() const
{
    ASSERT(length);
    return is8Bit ? *currentCharacter8 : *currentCharacter16;
}

ALWAYS_INLINE UChar SegmentedString::Substring::currentCharacterPreIncrement()
{
    ASSERT(length);
    return is8Bit ? *++currentCharacter8 : *++currentCharacter16;
}

inline SegmentedString::SegmentedString(String&& string)
    : m_currentSubstring(WTFMove(string))
{
    if (m_currentSubstring.length) {
        m_currentCharacter = m_currentSubstring.currentCharacter();
        updateAdvanceFunctionPointers();
    }
}

inline SegmentedString::SegmentedString(const String& string)
    : SegmentedString(String { string })
{
}

ALWAYS_INLINE void SegmentedString::decrementAndCheckLength()
{
    ASSERT(m_currentSubstring.length > 1);
    if (UNLIKELY(--m_currentSubstring.length == 1))
        updateAdvanceFunctionPointersForSingleCharacterSubstring();
}

ALWAYS_INLINE void SegmentedString::advanceWithoutUpdatingLineNumber()
{
    if (LIKELY(m_fastPathFlags & Use8BitAdvance)) {
        m_currentCharacter = *++m_currentSubstring.currentCharacter8;
        decrementAndCheckLength();
        return;
    }

    (this->*m_advanceWithoutUpdatingLineNumberFunction)();
}

inline void SegmentedString::startNewLine()
{
    ++m_currentLine;
    m_numberOfCharactersConsumedPriorToCurrentLine = numberOfCharactersConsumed();
}

inline void SegmentedString::processPossibleNewline()
{
    if (m_currentCharacter == '\n')
        startNewLine();
}

inline void SegmentedString::advance()
{
    if (LIKELY(m_fastPathFlags & Use8BitAdvance)) {
        ASSERT(m_currentSubstring.length > 1);
        bool lastCharacterWasNewline = m_currentCharacter == '\n';
        m_currentCharacter = *++m_currentSubstring.currentCharacter8;
        bool haveOneCharacterLeft = --m_currentSubstring.length == 1;
        if (LIKELY(!(lastCharacterWasNewline | haveOneCharacterLeft)))
            return;
        if (lastCharacterWasNewline & !!(m_fastPathFlags & Use8BitAdvanceAndUpdateLineNumbers))
            startNewLine();
        if (haveOneCharacterLeft)
            updateAdvanceFunctionPointersForSingleCharacterSubstring();
        return;
    }

    (this->*m_advanceAndUpdateLineNumberFunction)();
}

ALWAYS_INLINE void SegmentedString::advancePastNonNewline()
{
    ASSERT(m_currentCharacter != '\n');
    advanceWithoutUpdatingLineNumber();
}

inline void SegmentedString::advancePastNewline()
{
    ASSERT(m_currentCharacter == '\n');
    if (m_currentSubstring.length > 1) {
        if (m_currentSubstring.doNotExcludeLineNumbers)
            startNewLine();
        m_currentCharacter = m_currentSubstring.currentCharacterPreIncrement();
        decrementAndCheckLength();
        return;
    }

    (this->*m_advanceAndUpdateLineNumberFunction)();
}

inline unsigned SegmentedString::numberOfCharactersConsumed() const
{
    return m_numberOfCharactersConsumedPriorToCurrentSubstring + m_currentSubstring.numberOfCharactersConsumed();
}

template<typename CharacterType> ALWAYS_INLINE bool SegmentedString::characterMismatch(CharacterType a, char b, bool lettersIgnoringASCIICase)
{
    return lettersIgnoringASCIICase ? !isASCIIAlphaCaselessEqual(a, b) : a != b;
}

template<unsigned lengthIncludingTerminator, bool lettersIgnoringASCIICase> SegmentedString::AdvancePastResult SegmentedString::advancePast(const char (&literal)[lengthIncludingTerminator])
{
    constexpr unsigned length = lengthIncludingTerminator - 1;
    ASSERT(!literal[length]);
    ASSERT(!strchr(literal, '\n'));
    if (length + 1 < m_currentSubstring.length) {
        if (m_currentSubstring.is8Bit) {
            for (unsigned i = 0; i < length; ++i) {
                if (characterMismatch(m_currentSubstring.currentCharacter8[i], literal[i], lettersIgnoringASCIICase))
                    return DidNotMatch;
            }
            m_currentSubstring.currentCharacter8 += length;
            m_currentCharacter = *m_currentSubstring.currentCharacter8;
        } else {
            for (unsigned i = 0; i < length; ++i) {
                if (characterMismatch(m_currentSubstring.currentCharacter16[i], literal[i], lettersIgnoringASCIICase))
                    return DidNotMatch;
            }
            m_currentSubstring.currentCharacter16 += length;
            m_currentCharacter = *m_currentSubstring.currentCharacter16;
        }
        m_currentSubstring.length -= length;
        return DidMatch;
    }
    return advancePastSlowCase(literal, lettersIgnoringASCIICase);
}

inline void SegmentedString::updateAdvanceFunctionPointers()
{
    if (m_currentSubstring.length > 1) {
        if (m_currentSubstring.is8Bit) {
            m_fastPathFlags = Use8BitAdvance;
            if (m_currentSubstring.doNotExcludeLineNumbers)
                m_fastPathFlags |= Use8BitAdvanceAndUpdateLineNumbers;
            return;
        }
        m_fastPathFlags = NoFastPath;
        m_advanceWithoutUpdatingLineNumberFunction = &SegmentedString::advanceWithoutUpdatingLineNumber16;
        if (m_currentSubstring.doNotExcludeLineNumbers)
            m_advanceAndUpdateLineNumberFunction = &SegmentedString::advanceAndUpdateLineNumber16;
        else
            m_advanceAndUpdateLineNumberFunction = &SegmentedString::advanceWithoutUpdatingLineNumber16;
        return;
    }

    if (!m_currentSubstring.length) {
        updateAdvanceFunctionPointersForEmptyString();
        return;
    }

    updateAdvanceFunctionPointersForSingleCharacterSubstring();
}

}
