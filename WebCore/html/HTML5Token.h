/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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

#ifndef HTML5Token_h
#define HTML5Token_h

#include "NamedNodeMap.h"
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {

class HTML5Token : public Noncopyable {
public:
    enum Type {
        Uninitialized,
        DOCTYPE,
        StartTag,
        EndTag,
        Comment,
        Character,
        EndOfFile,
    };

    class Attribute {
    public:
        WTF::Vector<UChar, 32> m_name;
        WTF::Vector<UChar, 32> m_value;
    };

    typedef WTF::Vector<Attribute> AttributeList;
    typedef WTF::Vector<UChar, 1024> DataVector;

    HTML5Token() { clear(); }

    void clear()
    {
        m_type = Uninitialized;
    }

    void beginStartTag(UChar character)
    {
        ASSERT(m_type == Uninitialized);
        m_type = StartTag;
        clearData();
        m_selfClosing = false;
        m_currentAttribute = 0;
        m_attributes.clear();

        m_data.append(character);
    }

    template<typename T>
    void beginEndTag(T characters)
    {
        ASSERT(m_type == Uninitialized);
        m_type = EndTag;
        clearData();
        m_selfClosing = false;
        m_currentAttribute = 0;
        m_attributes.clear();

        m_data.append(characters);
    }

    void beginCharacter(UChar character)
    {
        ASSERT(m_type == Uninitialized);
        m_type = Character;
        clearData();
        m_data.append(character);
    }

    void beginComment()
    {
        ASSERT(m_type == Uninitialized);
        m_type = Comment;
        clearData();
    }

    void beginDOCTYPE()
    {
        ASSERT(m_type == Uninitialized);
        m_type = DOCTYPE;
        clearData();
        m_doctypeData.set(new DoctypeData());
    }

    void beginDOCTYPE(UChar character)
    {
        beginDOCTYPE();
        m_data.append(character);
    }

    void appendToName(UChar character)
    {
        ASSERT(m_type == StartTag || m_type == EndTag || m_type == DOCTYPE);
        m_data.append(character);
    }

    template<typename T>
    void appendToCharacter(T characters)
    {
        ASSERT(m_type == Character);
        m_data.append(characters);
    }

    void appendToComment(UChar character)
    {
        ASSERT(m_type == Comment);
        m_data.append(character);
    }

    void addNewAttribute()
    {
        ASSERT(m_type == StartTag || m_type == EndTag);
        m_attributes.grow(m_attributes.size() + 1);
        m_currentAttribute = &m_attributes.last();
    }

    void appendToAttributeName(UChar character)
    {
        ASSERT(m_type == StartTag || m_type == EndTag);
        m_currentAttribute->m_name.append(character);
    }

    void appendToAttributeValue(UChar character)
    {
        ASSERT(m_type == StartTag || m_type == EndTag);
        m_currentAttribute->m_value.append(character);
    }

    Type type() const { return m_type; }

    bool selfClosing() const
    {
        ASSERT(m_type == StartTag || m_type == EndTag);
        return m_selfClosing;
    }

    AttributeList& attributes()
    {
        ASSERT(m_type == StartTag || m_type == EndTag);
        return m_attributes;
    }

    AtomicString name()
    {
        ASSERT(m_type == StartTag || m_type == EndTag || m_type == DOCTYPE);
        if (!m_data.isEmpty() && m_dataAsNameAtom.isEmpty())
            m_dataAsNameAtom = AtomicString(adoptDataAsStringImpl());
        return m_dataAsNameAtom;
    }

    PassRefPtr<StringImpl> adoptDataAsStringImpl()
    {
        ASSERT(!m_dataAsNameAtom); // An attempt to make sure this isn't called twice.
        return StringImpl::adopt(m_data);
    }

    const DataVector& characters()
    {
        ASSERT(m_type == Character);
        ASSERT(!m_dataAsNameAtom);
        return m_data;
    }

    const DataVector& comment()
    {
        ASSERT(m_type == Comment);
        ASSERT(!m_dataAsNameAtom);
        return m_data;
    }

    // FIXME: Should be removed once we stop using the old parser.
    String takeCharacters()
    {
        ASSERT(m_type == Character);
        return String(adoptDataAsStringImpl());
    }

    // FIXME: Should be removed once we stop using the old parser.
    String takeComment()
    {
        ASSERT(m_type == Comment);
        return String(adoptDataAsStringImpl());
    }

    // FIXME: Distinguish between a missing public identifer and an empty one.
    const WTF::Vector<UChar>& publicIdentifier()
    {
        ASSERT(m_type == DOCTYPE);
        return m_doctypeData->m_publicIdentifier;
    }

    // FIXME: Distinguish between a missing system identifer and an empty one.
    const WTF::Vector<UChar>& systemIdentifier()
    {
        ASSERT(m_type == DOCTYPE);
        return m_doctypeData->m_systemIdentifier;
    }

    void setPublicIdentifierToEmptyString()
    {
        ASSERT(m_type == DOCTYPE);
        m_doctypeData->m_hasPublicIdentifier = true;
        m_doctypeData->m_publicIdentifier.clear();
    }

    void setSystemIdentifierToEmptyString()
    {
        ASSERT(m_type == DOCTYPE);
        m_doctypeData->m_hasSystemIdentifier = true;
        m_doctypeData->m_systemIdentifier.clear();
    }

    void appendToPublicIdentifier(UChar character)
    {
        ASSERT(m_type == DOCTYPE);
        ASSERT(m_doctypeData->m_hasPublicIdentifier);
        m_doctypeData->m_publicIdentifier.append(character);
    }

    void appendToSystemIdentifier(UChar character)
    {
        ASSERT(m_type == DOCTYPE);
        ASSERT(m_doctypeData->m_hasSystemIdentifier);
        m_doctypeData->m_systemIdentifier.append(character);
    }

private:
    class DoctypeData {
    public:
        DoctypeData()
            : m_hasPublicIdentifier(false)
            , m_hasSystemIdentifier(false)
            , m_forceQuirks(false)
        {
        }

        bool m_hasPublicIdentifier;
        bool m_hasSystemIdentifier;
        bool m_forceQuirks;
        WTF::Vector<UChar> m_publicIdentifier;
        WTF::Vector<UChar> m_systemIdentifier;
    };

    void clearData()
    {
        m_data.clear();
        m_dataAsNameAtom = AtomicString();
    }

    Type m_type;

    // "name" for DOCTYPE, StartTag, and EndTag
    // "characters" for Character
    // "data" for Comment
    DataVector m_data;
    AtomicString m_dataAsNameAtom;

    // For DOCTYPE
    OwnPtr<DoctypeData> m_doctypeData;

    // For StartTag and EndTag
    bool m_selfClosing;
    AttributeList m_attributes; // Old tokenizer reserves 10.

    // A pointer into m_attributes used during lexing.
    Attribute* m_currentAttribute;
};

}

#endif
