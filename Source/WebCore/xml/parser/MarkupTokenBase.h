/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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

#ifndef MarkupTokenBase_h
#define MarkupTokenBase_h

#include "ElementAttributeData.h"
#include <wtf/Vector.h>

#ifndef NDEBUG
#include <stdio.h>
#endif

namespace WebCore {

static inline Attribute* findAttributeInVector(Vector<Attribute>& attributes, const QualifiedName& name)
{
    for (unsigned i = 0; i < attributes.size(); ++i) {
        if (attributes.at(i).name().matches(name))
            return &attributes.at(i);
    }
    return 0;
}

class AttributeBase {
public:
    class Range {
    public:
        int m_start;
        int m_end;
    };

    Range m_nameRange;
    Range m_valueRange;
    WTF::Vector<UChar, 32> m_name;
    WTF::Vector<UChar, 32> m_value;
};

template<typename TypeSet, typename AttributeType = AttributeBase>
class MarkupTokenBase {
    WTF_MAKE_NONCOPYABLE(MarkupTokenBase);
    WTF_MAKE_FAST_ALLOCATED;
public:
    typedef TypeSet Type;
    typedef AttributeType Attribute;

    typedef WTF::Vector<Attribute, 10> AttributeList;
    typedef WTF::Vector<UChar, 1024> DataVector;

    MarkupTokenBase() { clear(); }
    virtual ~MarkupTokenBase() { }

    virtual void clear()
    {
        m_type = TypeSet::Uninitialized;
        m_range.m_start = 0;
        m_range.m_end = 0;
        m_baseOffset = 0;
        m_data.clear();
        m_orAllData = 0;
    }

    bool isUninitialized() { return m_type == TypeSet::Uninitialized; }

    int startIndex() const { return m_range.m_start; }
    int endIndex() const { return m_range.m_end; }

    void setBaseOffset(int offset)
    {
        m_baseOffset = offset;
    }

    void end(int endOffset)
    {
        m_range.m_end = endOffset - m_baseOffset;
    }

    void makeEndOfFile()
    {
        ASSERT(m_type == TypeSet::Uninitialized);
        m_type = TypeSet::EndOfFile;
    }

    void beginStartTag(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == TypeSet::Uninitialized);
        m_type = TypeSet::StartTag;
        m_selfClosing = false;
        m_currentAttribute = 0;
        m_attributes.clear();

        m_data.append(character);
        m_orAllData |= character;
    }

    void beginEndTag(LChar character)
    {
        ASSERT(m_type == TypeSet::Uninitialized);
        m_type = TypeSet::EndTag;
        m_selfClosing = false;
        m_currentAttribute = 0;
        m_attributes.clear();

        m_data.append(character);
    }

    void beginEndTag(const Vector<LChar, 32>& characters)
    {
        ASSERT(m_type == TypeSet::Uninitialized);
        m_type = TypeSet::EndTag;
        m_selfClosing = false;
        m_currentAttribute = 0;
        m_attributes.clear();

        m_data.appendVector(characters);
    }

    // Starting a character token works slightly differently than starting
    // other types of tokens because we want to save a per-character branch.
    void ensureIsCharacterToken()
    {
        ASSERT(m_type == TypeSet::Uninitialized || m_type == TypeSet::Character);
        m_type = TypeSet::Character;
    }

    void beginComment()
    {
        ASSERT(m_type == TypeSet::Uninitialized);
        m_type = TypeSet::Comment;
    }

    void appendToCharacter(char character)
    {
        ASSERT(m_type == TypeSet::Character);
        m_data.append(character);
    }

    void appendToCharacter(UChar character)
    {
        ASSERT(m_type == TypeSet::Character);
        m_data.append(character);
        m_orAllData |= character;
    }

    void appendToCharacter(const Vector<LChar, 32>& characters)
    {
        ASSERT(m_type == TypeSet::Character);
        m_data.appendVector(characters);
    }

    void appendToComment(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == TypeSet::Comment);
        m_data.append(character);
        m_orAllData |= character;
    }

    void addNewAttribute()
    {
        ASSERT(m_type == TypeSet::StartTag || m_type == TypeSet::EndTag);
        m_attributes.grow(m_attributes.size() + 1);
        m_currentAttribute = &m_attributes.last();
#ifndef NDEBUG
        m_currentAttribute->m_nameRange.m_start = 0;
        m_currentAttribute->m_nameRange.m_end = 0;
        m_currentAttribute->m_valueRange.m_start = 0;
        m_currentAttribute->m_valueRange.m_end = 0;
#endif
    }

    void beginAttributeName(int offset)
    {
        m_currentAttribute->m_nameRange.m_start = offset - m_baseOffset;
    }

    void endAttributeName(int offset)
    {
        int index = offset - m_baseOffset;
        m_currentAttribute->m_nameRange.m_end = index;
        m_currentAttribute->m_valueRange.m_start = index;
        m_currentAttribute->m_valueRange.m_end = index;
    }

    void beginAttributeValue(int offset)
    {
        m_currentAttribute->m_valueRange.m_start = offset - m_baseOffset;
#ifndef NDEBUG
        m_currentAttribute->m_valueRange.m_end = 0;
#endif
    }

    void endAttributeValue(int offset)
    {
        m_currentAttribute->m_valueRange.m_end = offset - m_baseOffset;
    }

    void appendToAttributeName(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == TypeSet::StartTag || m_type == TypeSet::EndTag);
        // FIXME: We should be able to add the following ASSERT once we fix
        // https://bugs.webkit.org/show_bug.cgi?id=62971
        //   ASSERT(m_currentAttribute->m_nameRange.m_start);
        m_currentAttribute->m_name.append(character);
    }

    void appendToAttributeValue(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == TypeSet::StartTag || m_type == TypeSet::EndTag);
        ASSERT(m_currentAttribute->m_valueRange.m_start);
        m_currentAttribute->m_value.append(character);
    }

    void appendToAttributeValue(size_t i, const String& value)
    {
        ASSERT(!value.isEmpty());
        ASSERT(m_type == TypeSet::StartTag || m_type == TypeSet::EndTag);
        m_attributes[i].m_value.append(value.characters(), value.length());
    }

    typename Type::Type type() const { return m_type; }

    bool selfClosing() const
    {
        ASSERT(m_type == TypeSet::StartTag || m_type == TypeSet::EndTag);
        return m_selfClosing;
    }

    void setSelfClosing()
    {
        ASSERT(m_type == TypeSet::StartTag || m_type == TypeSet::EndTag);
        m_selfClosing = true;
    }

    const AttributeList& attributes() const
    {
        ASSERT(m_type == TypeSet::StartTag || m_type == TypeSet::EndTag);
        return m_attributes;
    }

    void eraseCharacters()
    {
        ASSERT(m_type == TypeSet::Character);
        m_data.clear();
        m_orAllData = 0;
    }

    void eraseValueOfAttribute(size_t i)
    {
        ASSERT(m_type == TypeSet::StartTag || m_type == TypeSet::EndTag);
        m_attributes[i].m_value.clear();
    }

    const DataVector& characters() const
    {
        ASSERT(m_type == TypeSet::Character);
        return m_data;
    }

    const DataVector& comment() const
    {
        ASSERT(m_type == TypeSet::Comment);
        return m_data;
    }

    const DataVector& data() const
    {
        ASSERT(m_type == TypeSet::Character || m_type == TypeSet::Comment || m_type == TypeSet::StartTag || m_type == TypeSet::EndTag);
        return m_data;
    }

    bool isAll8BitData() const
    {
        return (m_orAllData <= 0xff);
    }

    const DataVector& name() const
    {
        return m_data;
    }

    String nameString() const
    {
        if (!m_data.size())
            return emptyString();
        if (isAll8BitData())
            return String::make8BitFrom16BitSource(m_data.data(), m_data.size());
        return String(m_data.data(), m_data.size());
    }

protected:

#ifndef NDEBUG
    void printString(const DataVector& string) const
    {
        DataVector::const_iterator iter = string.begin();
        for (; iter != string.end(); ++iter)
            fprintf(stderr, "%lc", wchar_t(*iter));
    }
#endif // NDEBUG

    void appendToName(UChar character)
    {
        ASSERT(character);
        m_data.append(character);
        m_orAllData |= character;
    }

    typename Type::Type m_type;
    typename Attribute::Range m_range; // Always starts at zero.
    int m_baseOffset;
    DataVector m_data;
    UChar m_orAllData;

    // For StartTag and EndTag
    bool m_selfClosing;
    AttributeList m_attributes;

    // A pointer into m_attributes used during lexing.
    Attribute* m_currentAttribute;
};

}

#endif // MarkupTokenBase_h
