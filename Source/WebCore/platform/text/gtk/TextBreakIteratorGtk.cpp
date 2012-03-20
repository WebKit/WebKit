/*
 * Copyright (C) 2006 Lars Knoll <lars@trolltech.com>
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Jürg Billeter <j@bitron.ch>
 * Copyright (C) 2008 Dominik Röttsches <dominik.roettsches@access-company.com>
 * Copyright (C) 2010 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "TextBreakIterator.h"

#include <wtf/gobject/GOwnPtr.h>
#include <pango/pango.h>
using namespace std;

#define UTF8_IS_SURROGATE(character) (character >= 0x10000 && character <= 0x10FFFF)

namespace WebCore {

class CharacterIterator {
public:
    bool setText(const UChar* string, int length);
    const gchar* getText() { return m_utf8.get(); }
    int getLength() { return m_length; }
    glong getSize() { return m_size; }
    void setIndex(int index);
    int getIndex() { return m_index; }
    void setUTF16Index(int index);
    int getUTF16Index() { return m_utf16Index; }
    int getUTF16Length() { return m_utf16Length; }
    int first();
    int last();
    int next();
    int previous();
private:
    int characterSize(int index);

    GOwnPtr<char> m_utf8;
    int m_length;
    long m_size;
    int m_index;
    int m_utf16Index;
    int m_utf16Length;
};

int CharacterIterator::characterSize(int index)
{
    if (index == m_length || index < 0)
        return 0;
    if (m_length == m_utf16Length)
        return 1;

    gchar* indexPtr = g_utf8_offset_to_pointer(m_utf8.get(), index);
    gunichar character = g_utf8_get_char(indexPtr);
    return UTF8_IS_SURROGATE(character) ? 2 : 1;
}

bool CharacterIterator::setText(const UChar* string, int length)
{
    long utf8Size = 0;
    m_utf8.set(g_utf16_to_utf8(string, length, 0, &utf8Size, 0));
    if (!utf8Size)
        return false;

    m_utf16Length = length;
    m_length = g_utf8_strlen(m_utf8.get(), utf8Size);
    m_size = utf8Size;
    m_index = 0;
    m_utf16Index = 0;

    return true;
}

void CharacterIterator::setIndex(int index)
{
    if (index == m_index)
        return;
    if (index <= 0)
        m_index = m_utf16Index = 0;
    else if (index >= m_length) {
        m_index = m_length;
        m_utf16Index = m_utf16Length;
    } else if (m_length == m_utf16Length)
        m_index = m_utf16Index = index;
    else {
        m_index = index;
        int utf16Index = 0;
        int utf8Index = 0;
        while (utf8Index < index) {
            utf16Index += characterSize(utf8Index);
            utf8Index++;
        }
        m_utf16Index = utf16Index;
    }
}

void CharacterIterator::setUTF16Index(int index)
{
    if (index == m_utf16Index)
        return;
    if (index <= 0)
        m_utf16Index = m_index = 0;
    else if (index >= m_utf16Length) {
        m_utf16Index = m_utf16Length;
        m_index = m_length;
    } else if (m_length == m_utf16Length)
        m_utf16Index = m_index = index;
    else {
        m_utf16Index = index;
        int utf16Index = 0;
        int utf8Index = 0;
        while (utf16Index < index) {
            utf16Index += characterSize(utf8Index);
            utf8Index++;
        }
        m_index = utf8Index;
    }
}

int CharacterIterator::first()
{
    m_index = m_utf16Index = 0;
    return m_index;
}

int CharacterIterator::last()
{
    m_index = m_length;
    m_utf16Index = m_utf16Length;
    return m_index;
}

int CharacterIterator::next()
{
    int next = m_index + 1;

    if (next <= m_length) {
        m_utf16Index = min(m_utf16Index + characterSize(m_index), m_utf16Length);
        m_index = next;
    } else {
        m_index = TextBreakDone;
        m_utf16Index = TextBreakDone;
    }

    return m_index;
}

int CharacterIterator::previous()
{
    int previous = m_index - 1;

    if (previous >= 0) {
        m_utf16Index = max(m_utf16Index - characterSize(previous), 0);
        m_index = previous;
    } else {
        m_index = TextBreakDone;
        m_utf16Index = TextBreakDone;
    }

    return m_index;
}

enum UBreakIteratorType {
    UBRK_CHARACTER,
    UBRK_WORD,
    UBRK_LINE,
    UBRK_SENTENCE
};

class TextBreakIterator {
public:
    UBreakIteratorType m_type;
    PangoLogAttr* m_logAttrs;
    CharacterIterator m_charIterator;
};

static TextBreakIterator* setUpIterator(bool& createdIterator, TextBreakIterator*& iterator,
    UBreakIteratorType type, const UChar* string, int length)
{
    if (!string)
        return 0;

    if (!createdIterator) {
        iterator = new TextBreakIterator();
        createdIterator = true;
    }
    if (!iterator)
        return 0;

    if (!iterator->m_charIterator.setText(string, length))
        return 0;

    int charLength = iterator->m_charIterator.getLength();

    iterator->m_type = type;
    if (createdIterator)
        g_free(iterator->m_logAttrs);
    iterator->m_logAttrs = g_new0(PangoLogAttr, charLength + 1);
    pango_get_log_attrs(iterator->m_charIterator.getText(), iterator->m_charIterator.getSize(),
                        -1, 0, iterator->m_logAttrs, charLength + 1);

    return iterator;
}

TextBreakIterator* characterBreakIterator(const UChar* string, int length)
{
    static bool createdCharacterBreakIterator = false;
    static TextBreakIterator* staticCharacterBreakIterator;
    return setUpIterator(createdCharacterBreakIterator, staticCharacterBreakIterator, UBRK_CHARACTER, string, length);
}

TextBreakIterator* cursorMovementIterator(const UChar* string, int length)
{
    // FIXME: This needs closer inspection to achieve behaviour identical to the ICU version.
    return characterBreakIterator(string, length);
}

TextBreakIterator* wordBreakIterator(const UChar* string, int length)
{
    static bool createdWordBreakIterator = false;
    static TextBreakIterator* staticWordBreakIterator;
    return setUpIterator(createdWordBreakIterator, staticWordBreakIterator, UBRK_WORD, string, length);
}

static bool createdLineBreakIterator = false;
static TextBreakIterator* staticLineBreakIterator;

TextBreakIterator* acquireLineBreakIterator(const UChar* string, int length, const AtomicString&)
{
    TextBreakIterator* lineBreakIterator = 0;
    if (!createdLineBreakIterator || staticLineBreakIterator) {
        setUpIterator(createdLineBreakIterator, staticLineBreakIterator, UBRK_LINE, string, length);
        swap(staticLineBreakIterator, lineBreakIterator);
    }

    if (!lineBreakIterator) {
        bool createdNewLineBreakIterator = false;
        setUpIterator(createdNewLineBreakIterator, lineBreakIterator, UBRK_LINE, string, length);
    }

    return lineBreakIterator;
}

void releaseLineBreakIterator(TextBreakIterator* iterator)
{
    ASSERT(createdLineBreakIterator);
    ASSERT(iterator);

    if (!staticLineBreakIterator)
        staticLineBreakIterator = iterator;
    else
        delete iterator;
}

TextBreakIterator* sentenceBreakIterator(const UChar* string, int length)
{
    static bool createdSentenceBreakIterator = false;
    static TextBreakIterator* staticSentenceBreakIterator;
    return setUpIterator(createdSentenceBreakIterator, staticSentenceBreakIterator, UBRK_SENTENCE, string, length);
}

int textBreakFirst(TextBreakIterator* iterator)
{
    iterator->m_charIterator.first();
    return iterator->m_charIterator.getUTF16Index();
}

int textBreakLast(TextBreakIterator* iterator)
{
    // TextBreakLast is not meant to find just any break according to bi->m_type 
    // but really the one near the last character.
    // (cmp ICU documentation for ubrk_first and ubrk_last)
    // From ICU docs for ubrk_last:
    // "Determine the index immediately beyond the last character in the text being scanned." 

    // So we should advance or traverse back based on bi->m_logAttrs cursor positions.
    // If last character position in the original string is a whitespace,
    // traverse to the left until the first non-white character position is found
    // and return the position of the first white-space char after this one.
    // Otherwise return m_length, as "the first character beyond the last" is outside our string.
    
    bool whiteSpaceAtTheEnd = true;
    int nextWhiteSpacePos = iterator->m_charIterator.getLength();

    int pos = iterator->m_charIterator.last();
    while (pos >= 0 && whiteSpaceAtTheEnd) {
        if (iterator->m_logAttrs[pos].is_cursor_position) {
            if (whiteSpaceAtTheEnd = iterator->m_logAttrs[pos].is_white)
                nextWhiteSpacePos = pos;
        }
        pos = iterator->m_charIterator.previous();
    }
    iterator->m_charIterator.setIndex(nextWhiteSpacePos);
    return iterator->m_charIterator.getUTF16Index();
}

int textBreakNext(TextBreakIterator* iterator)
{
    while (iterator->m_charIterator.next() != TextBreakDone) {
        int index = iterator->m_charIterator.getIndex();

        // FIXME: UBRK_WORD case: Single multibyte characters (i.e. white space around them), such as the euro symbol €, 
        // are not marked as word_start & word_end as opposed to the way ICU does it.
        // This leads to - for example - different word selection behaviour when right clicking.

        if ((iterator->m_type == UBRK_LINE && iterator->m_logAttrs[index].is_line_break)
            || (iterator->m_type == UBRK_WORD && (iterator->m_logAttrs[index].is_word_start || iterator->m_logAttrs[index].is_word_end))
            || (iterator->m_type == UBRK_CHARACTER && iterator->m_logAttrs[index].is_cursor_position)
            || (iterator->m_type == UBRK_SENTENCE && iterator->m_logAttrs[index].is_sentence_boundary)) {
            break;
        }
    }
    return iterator->m_charIterator.getUTF16Index();
}

int textBreakPrevious(TextBreakIterator* iterator)
{
    while (iterator->m_charIterator.previous() != TextBreakDone) {
        int index = iterator->m_charIterator.getIndex();

        if ((iterator->m_type == UBRK_LINE && iterator->m_logAttrs[index].is_line_break)
            || (iterator->m_type == UBRK_WORD && (iterator->m_logAttrs[index].is_word_start || iterator->m_logAttrs[index].is_word_end))
            || (iterator->m_type == UBRK_CHARACTER && iterator->m_logAttrs[index].is_cursor_position)
            || (iterator->m_type == UBRK_SENTENCE && iterator->m_logAttrs[index].is_sentence_boundary)) {
            break;
        }
    }
    return iterator->m_charIterator.getUTF16Index();
}

int textBreakPreceding(TextBreakIterator* iterator, int offset)
{
    if (offset > iterator->m_charIterator.getUTF16Length())
        return TextBreakDone;
    if (offset < 0)
        return 0;
    iterator->m_charIterator.setUTF16Index(offset);
    return textBreakPrevious(iterator);
}

int textBreakFollowing(TextBreakIterator* iterator, int offset)
{
    if (offset > iterator->m_charIterator.getUTF16Length())
        return TextBreakDone;
    if (offset < 0)
        return 0;
    iterator->m_charIterator.setUTF16Index(offset);
    return textBreakNext(iterator);
}

int textBreakCurrent(TextBreakIterator* iterator)
{
    return iterator->m_charIterator.getUTF16Index();
}

bool isTextBreak(TextBreakIterator* iterator, int offset)
{
    if (!offset)
        return true;
    if (offset > iterator->m_charIterator.getUTF16Length())
        return false;

    iterator->m_charIterator.setUTF16Index(offset);

    int index = iterator->m_charIterator.getIndex();
    iterator->m_charIterator.previous();
    textBreakNext(iterator);
    return iterator->m_charIterator.getIndex() == index;
}

bool isWordTextBreak(TextBreakIterator*)
{
    return true;
}

}
