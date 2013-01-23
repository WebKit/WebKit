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

#include "CompactHTMLToken.h"
#include "HTMLTokenTypes.h"
#include "MarkupTokenBase.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class DoctypeData {
    WTF_MAKE_NONCOPYABLE(DoctypeData);
public:
    DoctypeData()
        : m_hasPublicIdentifier(false)
        , m_hasSystemIdentifier(false)
        , m_forceQuirks(false)
    {
    }

    // FIXME: This should use String instead of Vector<UChar>.
    bool m_hasPublicIdentifier;
    bool m_hasSystemIdentifier;
    WTF::Vector<UChar> m_publicIdentifier;
    WTF::Vector<UChar> m_systemIdentifier;
    bool m_forceQuirks;
};

class HTMLToken : public MarkupTokenBase<HTMLTokenTypes> {
public:
    void appendToName(UChar character)
    {
        ASSERT(m_type == HTMLTokenTypes::StartTag || m_type == HTMLTokenTypes::EndTag || m_type == HTMLTokenTypes::DOCTYPE);
        MarkupTokenBase<HTMLTokenTypes>::appendToName(character);
    }

    const DataVector& name() const
    {
        ASSERT(m_type == HTMLTokenTypes::StartTag || m_type == HTMLTokenTypes::EndTag || m_type == HTMLTokenTypes::DOCTYPE);
        return MarkupTokenBase<HTMLTokenTypes>::name();
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

    void beginDOCTYPE()
    {
        ASSERT(m_type == HTMLTokenTypes::Uninitialized);
        m_type = HTMLTokenTypes::DOCTYPE;
        m_doctypeData = adoptPtr(new DoctypeData);
    }

    void beginDOCTYPE(UChar character)
    {
        ASSERT(character);
        beginDOCTYPE();
        m_data.append(character);
        m_orAllData |= character;
    }

    // FIXME: Distinguish between a missing public identifer and an empty one.
    const WTF::Vector<UChar>& publicIdentifier() const
    {
        ASSERT(m_type == HTMLTokenTypes::DOCTYPE);
        return m_doctypeData->m_publicIdentifier;
    }

    // FIXME: Distinguish between a missing system identifer and an empty one.
    const WTF::Vector<UChar>& systemIdentifier() const
    {
        ASSERT(m_type == HTMLTokenTypes::DOCTYPE);
        return m_doctypeData->m_systemIdentifier;
    }

    void setPublicIdentifierToEmptyString()
    {
        ASSERT(m_type == HTMLTokenTypes::DOCTYPE);
        m_doctypeData->m_hasPublicIdentifier = true;
        m_doctypeData->m_publicIdentifier.clear();
    }

    void setSystemIdentifierToEmptyString()
    {
        ASSERT(m_type == HTMLTokenTypes::DOCTYPE);
        m_doctypeData->m_hasSystemIdentifier = true;
        m_doctypeData->m_systemIdentifier.clear();
    }

    void appendToPublicIdentifier(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == HTMLTokenTypes::DOCTYPE);
        ASSERT(m_doctypeData->m_hasPublicIdentifier);
        m_doctypeData->m_publicIdentifier.append(character);
    }

    void appendToSystemIdentifier(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == HTMLTokenTypes::DOCTYPE);
        ASSERT(m_doctypeData->m_hasSystemIdentifier);
        m_doctypeData->m_systemIdentifier.append(character);
    }

    PassOwnPtr<DoctypeData> releaseDoctypeData()
    {
        return m_doctypeData.release();
    }

private:
    // For DOCTYPE
    OwnPtr<DoctypeData> m_doctypeData;
};

class AtomicHTMLToken : public RefCounted<AtomicHTMLToken> {
    WTF_MAKE_NONCOPYABLE(AtomicHTMLToken);
public:
    static PassRefPtr<AtomicHTMLToken> create(HTMLToken& token)
    {
        return adoptRef(new AtomicHTMLToken(token));
    }

#if ENABLE(THREADED_HTML_PARSER)

    static PassRefPtr<AtomicHTMLToken> create(const CompactHTMLToken& token)
    {
        return adoptRef(new AtomicHTMLToken(token));
    }

#endif

    static PassRefPtr<AtomicHTMLToken> create(HTMLTokenTypes::Type type, const AtomicString& name, const Vector<Attribute>& attributes = Vector<Attribute>())
    {
        return adoptRef(new AtomicHTMLToken(type, name, attributes));
    }

    bool forceQuirks() const
    {
        ASSERT(m_type == HTMLTokenTypes::DOCTYPE);
        return m_doctypeData->m_forceQuirks;
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
        ASSERT(m_type == HTMLTokenTypes::Character);
        return m_externalCharacters;
    }

    size_t charactersLength() const
    {
        ASSERT(m_type == HTMLTokenTypes::Character);
        return m_externalCharactersLength;
    }

    bool isAll8BitData() const
    {
        return m_isAll8BitData;
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

    void clearExternalCharacters()
    {
        m_externalCharacters = 0;
        m_externalCharactersLength = 0;
        m_isAll8BitData = false;
    }

private:
    explicit AtomicHTMLToken(HTMLToken& token)
        : m_type(token.type())
    {
        switch (m_type) {
        case HTMLTokenTypes::Uninitialized:
            ASSERT_NOT_REACHED();
            break;
        case HTMLTokenTypes::DOCTYPE:
            m_name = AtomicString(token.nameString());
            m_doctypeData = token.releaseDoctypeData();
            break;
        case HTMLTokenTypes::EndOfFile:
            break;
        case HTMLTokenTypes::StartTag:
        case HTMLTokenTypes::EndTag: {
            m_selfClosing = token.selfClosing();
            m_name = AtomicString(token.nameString());
            initializeAttributes(token.attributes());
            break;
        }
        case HTMLTokenTypes::Comment:
            if (token.isAll8BitData())
                m_data = String::make8BitFrom16BitSource(token.comment().data(), token.comment().size());
            else
                m_data = String(token.comment().data(), token.comment().size());
            break;
        case HTMLTokenTypes::Character:
            m_externalCharacters = token.characters().data();
            m_externalCharactersLength = token.characters().size();
            m_isAll8BitData = token.isAll8BitData();
            break;
        default:
            break;
        }
    }

#if ENABLE(THREADED_HTML_PARSER)

    explicit AtomicHTMLToken(const CompactHTMLToken& token)
        : m_type(token.type())
    {
        switch (m_type) {
        case HTMLTokenTypes::Uninitialized:
            ASSERT_NOT_REACHED();
            break;
        case HTMLTokenTypes::DOCTYPE:
            m_name = token.data();
            m_doctypeData = adoptPtr(new DoctypeData());
            m_doctypeData->m_hasPublicIdentifier = true;
            m_doctypeData->m_publicIdentifier.append(token.publicIdentifier().characters(), token.publicIdentifier().length());
            m_doctypeData->m_hasSystemIdentifier = true;
            m_doctypeData->m_systemIdentifier.append(token.systemIdentifier().characters(), token.systemIdentifier().length());
            break;
        case HTMLTokenTypes::EndOfFile:
            break;
        case HTMLTokenTypes::StartTag:
            m_attributes.reserveInitialCapacity(token.attributes().size());
            for (Vector<CompactAttribute>::const_iterator it = token.attributes().begin(); it != token.attributes().end(); ++it)
                m_attributes.append(Attribute(QualifiedName(nullAtom, it->name(), nullAtom), it->value()));
            // Fall through!
        case HTMLTokenTypes::EndTag:
            m_selfClosing = token.selfClosing();
            m_name = AtomicString(token.data());
            break;
        case HTMLTokenTypes::Comment:
            m_data = token.data();
            break;
        case HTMLTokenTypes::Character:
            m_externalCharacters = token.data().characters();
            m_externalCharactersLength = token.data().length();
            m_isAll8BitData = token.isAll8BitData();
            break;
        default:
            break;
        }
    }

#endif

    explicit AtomicHTMLToken(HTMLTokenTypes::Type type)
        : m_type(type)
        , m_externalCharacters(0)
        , m_externalCharactersLength(0)
        , m_isAll8BitData(false)
        , m_selfClosing(false)
    {
    }

    AtomicHTMLToken(HTMLTokenTypes::Type type, const AtomicString& name, const Vector<Attribute>& attributes = Vector<Attribute>())
        : m_type(type)
        , m_name(name)
        , m_externalCharacters(0)
        , m_externalCharactersLength(0)
        , m_isAll8BitData(false)
        , m_selfClosing(false)
        , m_attributes(attributes)
    {
        ASSERT(usesName());
    }

    HTMLTokenTypes::Type m_type;

    void initializeAttributes(const HTMLToken::AttributeList& attributes);
    QualifiedName nameForAttribute(const HTMLToken::Attribute&) const;

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
    OwnPtr<DoctypeData> m_doctypeData;

    // For StartTag and EndTag
    bool m_selfClosing;

    Vector<Attribute> m_attributes;
};

inline void AtomicHTMLToken::initializeAttributes(const HTMLToken::AttributeList& attributes)
{
    size_t size = attributes.size();
    if (!size)
        return;

    m_attributes.clear();
    m_attributes.reserveInitialCapacity(size);
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

        AtomicString value(attribute.m_value.data(), attribute.m_value.size());
        const QualifiedName& name = nameForAttribute(attribute);
        if (!findAttributeInVector(m_attributes, name))
            m_attributes.append(Attribute(name, value));
    }
}

}

#endif
