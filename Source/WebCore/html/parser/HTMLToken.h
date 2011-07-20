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

#ifndef HTMLToken_h
#define HTMLToken_h

#include "MarkupTokenBase.h"

namespace WebCore {

class HTMLTokenTypes {
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

    class DoctypeData : public DoctypeDataBase {
        WTF_MAKE_NONCOPYABLE(DoctypeData);
    public:
        DoctypeData()
            : DoctypeDataBase()
            , m_forceQuirks(false)
        {
        }

        bool m_forceQuirks;
    };
};

class HTMLToken : public MarkupTokenBase<HTMLTokenTypes, HTMLTokenTypes::DoctypeData> {
public:
    void appendToName(UChar character)
    {
        ASSERT(m_type == HTMLTokenTypes::StartTag || m_type == HTMLTokenTypes::EndTag || m_type == HTMLTokenTypes::DOCTYPE);
        MarkupTokenBase<HTMLTokenTypes, HTMLTokenTypes::DoctypeData>::appendToName(character);
    }
    
    const DataVector& name() const
    {
        ASSERT(m_type == HTMLTokenTypes::StartTag || m_type == HTMLTokenTypes::EndTag || m_type == HTMLTokenTypes::DOCTYPE);
        return MarkupTokenBase<HTMLTokenTypes, HTMLTokenTypes::DoctypeData>::name();
    }

    bool forceQuirks() const
    {
        ASSERT(m_type == HTMLTokenTypes::DOCTYPE);
        return m_doctypeData->m_forceQuirks;
    }

    void setForceQuirks()
    {
        ASSERT(m_type == HTMLTokenTypes::DOCTYPE);
        m_doctypeData->m_forceQuirks = true;
    }

};

// FIXME: This class should eventually be named HTMLToken once we move the
// exiting HTMLToken to be internal to the HTMLTokenizer.
class AtomicHTMLToken {
    WTF_MAKE_NONCOPYABLE(AtomicHTMLToken);
public:
    AtomicHTMLToken(HTMLToken& token)
        : m_type(token.type())
    {
        switch (m_type) {
        case HTMLTokenTypes::Uninitialized:
            ASSERT_NOT_REACHED();
            break;
        case HTMLTokenTypes::DOCTYPE:
            m_name = AtomicString(token.name().data(), token.name().size());
            m_doctypeData = token.m_doctypeData.release();
            break;
        case HTMLTokenTypes::EndOfFile:
            break;
        case HTMLTokenTypes::StartTag:
        case HTMLTokenTypes::EndTag: {
            m_selfClosing = token.selfClosing();
            m_name = AtomicString(token.name().data(), token.name().size());
            initializeAttributes(token.attributes());
            break;
        }
        case HTMLTokenTypes::Comment:
            m_data = String(token.comment().data(), token.comment().size());
            break;
        case HTMLTokenTypes::Character:
            m_externalCharacters = &token.characters();
            break;
        default:
            break;
        }
    }

    AtomicHTMLToken(HTMLTokenTypes::Type type, AtomicString name, PassRefPtr<NamedNodeMap> attributes = 0)
        : m_type(type)
        , m_name(name)
        , m_attributes(attributes)
    {
        ASSERT(usesName());
    }

    HTMLTokenTypes::Type type() const { return m_type; }

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
        ASSERT(m_type == HTMLTokenTypes::StartTag || m_type == HTMLTokenTypes::EndTag);
        return m_selfClosing;
    }

    Attribute* getAttributeItem(const QualifiedName& attributeName)
    {
        ASSERT(usesAttributes());
        if (!m_attributes)
            return 0;
        return m_attributes->getAttributeItem(attributeName);
    }

    NamedNodeMap* attributes() const
    {
        ASSERT(usesAttributes());
        return m_attributes.get();
    }

    PassRefPtr<NamedNodeMap> takeAtributes()
    {
        ASSERT(usesAttributes());
        return m_attributes.release();
    }

    const HTMLToken::DataVector& characters() const
    {
        ASSERT(m_type == HTMLTokenTypes::Character);
        return *m_externalCharacters;
    }

    const String& comment() const
    {
        ASSERT(m_type == HTMLTokenTypes::Comment);
        return m_data;
    }

    // FIXME: Distinguish between a missing public identifer and an empty one.
    WTF::Vector<UChar>& publicIdentifier() const
    {
        ASSERT(m_type == HTMLTokenTypes::DOCTYPE);
        return m_doctypeData->m_publicIdentifier;
    }

    // FIXME: Distinguish between a missing system identifer and an empty one.
    WTF::Vector<UChar>& systemIdentifier() const
    {
        ASSERT(m_type == HTMLTokenTypes::DOCTYPE);
        return m_doctypeData->m_systemIdentifier;
    }

    bool forceQuirks() const
    {
        ASSERT(m_type == HTMLTokenTypes::DOCTYPE);
        return m_doctypeData->m_forceQuirks;
    }

private:
    HTMLTokenTypes::Type m_type;

    void initializeAttributes(const HTMLToken::AttributeList& attributes);
    
    bool usesName() const
    {
        return m_type == HTMLTokenTypes::StartTag || m_type == HTMLTokenTypes::EndTag || m_type == HTMLTokenTypes::DOCTYPE;
    }

    bool usesAttributes() const
    {
        return m_type == HTMLTokenTypes::StartTag || m_type == HTMLTokenTypes::EndTag;
    }

    // "name" for DOCTYPE, StartTag, and EndTag
    AtomicString m_name;

    // "data" for Comment
    String m_data;

    // "characters" for Character
    //
    // We don't want to copy the the characters out of the HTMLToken, so we
    // keep a pointer to its buffer instead.  This buffer is owned by the
    // HTMLToken and causes a lifetime dependence between these objects.
    //
    // FIXME: Add a mechanism for "internalizing" the characters when the
    //        HTMLToken is destructed.
    const HTMLToken::DataVector* m_externalCharacters;

    // For DOCTYPE
    OwnPtr<HTMLTokenTypes::DoctypeData> m_doctypeData;

    // For StartTag and EndTag
    bool m_selfClosing;

    RefPtr<NamedNodeMap> m_attributes;
};

inline void AtomicHTMLToken::initializeAttributes(const HTMLToken::AttributeList& attributes)
{
    size_t size = attributes.size();
    if (!size)
        return;

    m_attributes = NamedNodeMap::create();
    m_attributes->reserveInitialCapacity(size);
    for (size_t i = 0; i < size; ++i) {
        const HTMLToken::Attribute& attribute = attributes[i];
        if (attribute.m_name.isEmpty())
            continue;

        // FIXME: We should be able to add the following ASSERT once we fix
        // https://bugs.webkit.org/show_bug.cgi?id=62971
        //   ASSERT(attribute.m_nameRange.m_start);
        ASSERT(attribute.m_nameRange.m_end);
        ASSERT(attribute.m_valueRange.m_start);
        ASSERT(attribute.m_valueRange.m_end);

        String name(attribute.m_name.data(), attribute.m_name.size());
        String value(attribute.m_value.data(), attribute.m_value.size());
        m_attributes->insertAttribute(Attribute::createMapped(name, value), false);
    }
}

}

#endif
