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

#include "NamedMappedAttrMap.h"
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

    HTML5Token() { clear(); }

    void clear() { m_type = Uninitialized; }

    void beginStartTag(UChar character)
    {
        ASSERT(m_type == Uninitialized);
        m_type = StartTag;
        m_data.clear();
        m_selfClosing = false;
        m_currentAttribute = 0;

        m_data.append(character);
    }

    template<typename T>
    void beginEndTag(T characters)
    {
        ASSERT(m_type == Uninitialized);
        m_type = EndTag;
        m_data.clear();
        m_selfClosing = false;
        m_currentAttribute = 0;

        m_data.append(characters);
    }

    void beginCharacter(UChar character)
    {
        ASSERT(m_type == Uninitialized);
        m_type = Character;
        m_data.clear();
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

    AtomicString name()
    {
        ASSERT(m_type == StartTag || m_type == EndTag || m_type == DOCTYPE);
        if (!m_data.isEmpty())
            m_cachedName = AtomicString(StringImpl::adopt(m_data));
        ASSERT(!m_cachedName.isEmpty());
        return m_cachedName;
    }

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

    String characters()
    {
        ASSERT(m_type == Character);
        return String(StringImpl::adopt(m_data));
    }

private:
    Type m_type;

    // "name" for DOCTYPE, StartTag, and EndTag
    // "characters" for Character
    // "data" for Comment
    WTF::Vector<UChar, 1024> m_data;

    // For DOCTYPE
    String m_publicIdentifier;
    String m_systemIdentifier;
    bool m_forceQuirks;

    // For StartTag and EndTag
    bool m_selfClosing;
    AttributeList m_attributes; // Old tokenizer reserves 10.

    // A pointer into m_attributes used during lexing.
    Attribute* m_currentAttribute;

    AtomicString m_cachedName;
};

}

#endif
