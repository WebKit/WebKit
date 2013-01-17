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


class DoctypeDataBase {
    WTF_MAKE_NONCOPYABLE(DoctypeDataBase); WTF_MAKE_FAST_ALLOCATED;
public:
    DoctypeDataBase()
        : m_hasPublicIdentifier(false)
        , m_hasSystemIdentifier(false)
    {
    }

    bool m_hasPublicIdentifier;
    bool m_hasSystemIdentifier;
    WTF::Vector<UChar> m_publicIdentifier;
    WTF::Vector<UChar> m_systemIdentifier;
};

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

template<typename TypeSet, typename DoctypeDataType = DoctypeDataBase, typename AttributeType = AttributeBase>
class MarkupTokenBase {
    WTF_MAKE_NONCOPYABLE(MarkupTokenBase);
    WTF_MAKE_FAST_ALLOCATED;
public:
    typedef TypeSet Type;
    typedef AttributeType Attribute;
    typedef DoctypeDataType DoctypeData;

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

    void beginDOCTYPE()
    {
        ASSERT(m_type == TypeSet::Uninitialized);
        m_type = TypeSet::DOCTYPE;
        m_doctypeData = adoptPtr(new DoctypeData);
    }

    void beginDOCTYPE(UChar character)
    {
        ASSERT(character);
        beginDOCTYPE();
        m_data.append(character);
        m_orAllData |= character;
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

    // FIXME: Distinguish between a missing public identifer and an empty one.
    const WTF::Vector<UChar>& publicIdentifier() const
    {
        ASSERT(m_type == TypeSet::DOCTYPE);
        return m_doctypeData->m_publicIdentifier;
    }

    // FIXME: Distinguish between a missing system identifer and an empty one.
    const WTF::Vector<UChar>& systemIdentifier() const
    {
        ASSERT(m_type == TypeSet::DOCTYPE);
        return m_doctypeData->m_systemIdentifier;
    }

    void setPublicIdentifierToEmptyString()
    {
        ASSERT(m_type == TypeSet::DOCTYPE);
        m_doctypeData->m_hasPublicIdentifier = true;
        m_doctypeData->m_publicIdentifier.clear();
    }

    void setSystemIdentifierToEmptyString()
    {
        ASSERT(m_type == TypeSet::DOCTYPE);
        m_doctypeData->m_hasSystemIdentifier = true;
        m_doctypeData->m_systemIdentifier.clear();
    }

    void appendToPublicIdentifier(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == TypeSet::DOCTYPE);
        ASSERT(m_doctypeData->m_hasPublicIdentifier);
        m_doctypeData->m_publicIdentifier.append(character);
    }

    void appendToSystemIdentifier(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == TypeSet::DOCTYPE);
        ASSERT(m_doctypeData->m_hasSystemIdentifier);
        m_doctypeData->m_systemIdentifier.append(character);
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

    // FIXME: I'm not sure what the final relationship between MarkupTokenBase and
    // AtomicMarkupTokenBase will be. I'm marking this a friend for now, but we'll
    // want to end up with a cleaner interface between the two classes.
    template<typename Token>
    friend class AtomicMarkupTokenBase;

    typename Type::Type m_type;
    typename Attribute::Range m_range; // Always starts at zero.
    int m_baseOffset;
    DataVector m_data;
    UChar m_orAllData;

    // For DOCTYPE
    OwnPtr<DoctypeData> m_doctypeData;

    // For StartTag and EndTag
    bool m_selfClosing;
    AttributeList m_attributes;

    // A pointer into m_attributes used during lexing.
    Attribute* m_currentAttribute;
};

template<typename Token>
class AtomicMarkupTokenBase {
    WTF_MAKE_NONCOPYABLE(AtomicMarkupTokenBase);
public:
    AtomicMarkupTokenBase(Token* token)
        : m_type(token->type())
    {
        ASSERT(token);

        switch (m_type) {
        case Token::Type::Uninitialized:
            ASSERT_NOT_REACHED();
            break;
        case Token::Type::DOCTYPE:
            m_name = AtomicString(token->nameString());
            m_doctypeData = token->m_doctypeData.release();
            break;
        case Token::Type::EndOfFile:
            break;
        case Token::Type::StartTag:
        case Token::Type::EndTag: {
            m_selfClosing = token->selfClosing();
            m_name = AtomicString(token->nameString());
            initializeAttributes(token->attributes());
            break;
        }
        case Token::Type::Comment:
            if (token->isAll8BitData())
                m_data = String::make8BitFrom16BitSource(token->comment().data(), token->comment().size());
            else
                m_data = String(token->comment().data(), token->comment().size());
            break;
        case Token::Type::Character:
            m_externalCharacters = token->characters().data();
            m_externalCharactersLength = token->characters().size();
            m_isAll8BitData = token->isAll8BitData();
            break;
        default:
            break;
        }
    }

    explicit AtomicMarkupTokenBase(typename Token::Type::Type type)
        : m_type(type)
        , m_externalCharacters(0)
        , m_externalCharactersLength(0)
        , m_isAll8BitData(false)
        , m_selfClosing(false)
    {
    }

    AtomicMarkupTokenBase(typename Token::Type::Type type, const AtomicString& name, const Vector<Attribute>& attributes = Vector<Attribute>())
        : m_type(type)
        , m_name(name)
        , m_externalCharacters(0)
        , m_externalCharactersLength(0)
        , m_isAll8BitData(false)
        , m_attributes(attributes)
    {
        ASSERT(usesName());
    }

    typename Token::Type::Type type() const { return m_type; }

    const AtomicString& name() const
    {
        ASSERT(usesName());
        return m_name;
    }

    void setName(const AtomicString& name)
    {
        ASSERT(usesName());
        m_name = name;
    }

    bool selfClosing() const
    {
        ASSERT(m_type == Token::Type::StartTag || m_type == Token::Type::EndTag);
        return m_selfClosing;
    }

    Attribute* getAttributeItem(const QualifiedName& attributeName)
    {
        ASSERT(usesAttributes());
        return findAttributeInVector(m_attributes, attributeName);
    }

    Vector<Attribute>& attributes()
    {
        ASSERT(usesAttributes());
        return m_attributes;
    }

    const Vector<Attribute>& attributes() const
    {
        ASSERT(usesAttributes());
        return m_attributes;
    }

    const UChar* characters() const
    {
        ASSERT(m_type == Token::Type::Character);
        return m_externalCharacters;
    }

    size_t charactersLength() const
    {
        ASSERT(m_type == Token::Type::Character);
        return m_externalCharactersLength;
    }

    bool isAll8BitData() const
    {
        return m_isAll8BitData;
    }

    const String& comment() const
    {
        ASSERT(m_type == Token::Type::Comment);
        return m_data;
    }

    // FIXME: Distinguish between a missing public identifer and an empty one.
    WTF::Vector<UChar>& publicIdentifier() const
    {
        ASSERT(m_type == Token::Type::DOCTYPE);
        return m_doctypeData->m_publicIdentifier;
    }

    // FIXME: Distinguish between a missing system identifer and an empty one.
    WTF::Vector<UChar>& systemIdentifier() const
    {
        ASSERT(m_type == Token::Type::DOCTYPE);
        return m_doctypeData->m_systemIdentifier;
    }

    void clearExternalCharacters()
    {
        m_externalCharacters = 0;
        m_externalCharactersLength = 0;
        m_isAll8BitData = false;
    }

protected:
    typename Token::Type::Type m_type;

    void initializeAttributes(const typename Token::AttributeList& attributes);
    QualifiedName nameForAttribute(const typename Token::Attribute&) const;

    bool usesName() const;

    bool usesAttributes() const;

    // "name" for DOCTYPE, StartTag, and EndTag
    AtomicString m_name;

    // "data" for Comment
    String m_data;

    // "characters" for Character
    //
    // We don't want to copy the the characters out of the Token, so we
    // keep a pointer to its buffer instead. This buffer is owned by the
    // Token and causes a lifetime dependence between these objects.
    //
    // FIXME: Add a mechanism for "internalizing" the characters when the
    //        HTMLToken is destructed.
    const UChar* m_externalCharacters;
    size_t m_externalCharactersLength;
    bool m_isAll8BitData;

    // For DOCTYPE
    OwnPtr<typename Token::DoctypeData> m_doctypeData;

    // For StartTag and EndTag
    bool m_selfClosing;

    Vector<Attribute> m_attributes;
};

template<typename Token>
inline void AtomicMarkupTokenBase<Token>::initializeAttributes(const typename Token::AttributeList& attributes)
{
    size_t size = attributes.size();
    if (!size)
        return;

    m_attributes.clear();
    m_attributes.reserveInitialCapacity(size);
    for (size_t i = 0; i < size; ++i) {
        const typename Token::Attribute& attribute = attributes[i];
        if (attribute.m_name.isEmpty())
            continue;

        // FIXME: We should be able to add the following ASSERT once we fix
        // https://bugs.webkit.org/show_bug.cgi?id=62971
        //   ASSERT(attribute.m_nameRange.m_start);
        ASSERT(attribute.m_nameRange.m_end);
        ASSERT(attribute.m_valueRange.m_start);
        ASSERT(attribute.m_valueRange.m_end);

        AtomicString value(attribute.m_value.data(), attribute.m_value.size());
        const QualifiedName& name = nameForAttribute(attribute);
        if (!findAttributeInVector(m_attributes, name))
            m_attributes.append(Attribute(name, value));
    }
}

}

#endif // MarkupTokenBase_h
