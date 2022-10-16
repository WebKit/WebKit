/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
 * Copyright (C) 2015 Apple Inc. All Rights Reserved.
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

#pragma once

#include "HTMLNameCache.h"
#include "HTMLNames.h"
#include "HTMLToken.h"
#include "TagName.h"
#include <wtf/HashSet.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class AtomHTMLToken {
public:
    using Type = HTMLToken::Type;

    explicit AtomHTMLToken(HTMLToken&);
    AtomHTMLToken(Type, TagName, const AtomString& name, Vector<Attribute>&& = { }); // Only StartTag or EndTag.
    AtomHTMLToken(Type, TagName, Vector<Attribute>&& = { }); // Only StartTag or EndTag.

    AtomHTMLToken(const AtomHTMLToken&) = delete;
    AtomHTMLToken(AtomHTMLToken&&) = default;

    Type type() const;

    // StartTag, EndTag, DOCTYPE.

    void setTagName(TagName);

    const AtomString& name() const;

    // DOCTYPE.

    bool forceQuirks() const;
    String publicIdentifier() const;
    String systemIdentifier() const;

    // StartTag, EndTag.

    Vector<Attribute>& attributes();

    TagName tagName() const;
    bool selfClosing() const;
    const Vector<Attribute>& attributes() const;

    // Characters

    Span<const UChar> characters() const;
    bool charactersIsAll8BitData() const;

    // Comment

    const String& comment() const;
    String& comment();

    bool hasDuplicateAttribute() const { return m_hasDuplicateAttribute; }

private:
    void initializeAttributes(const HTMLToken::AttributeList& attributes);

    AtomString m_name; // StartTag, EndTag, DOCTYPE.
    String m_data; // Comment
    std::unique_ptr<DoctypeData> m_doctypeData; // DOCTYPE.
    Vector<Attribute> m_attributes; // StartTag, EndTag.

    // We don't want to copy the characters out of the HTMLToken, so we keep a pointer to its buffer instead.
    // This buffer is owned by the HTMLToken and causes a lifetime dependence between these objects.
    // FIXME: Add a mechanism for "internalizing" the characters when the HTMLToken is destroyed.
    Span<const UChar> m_externalCharacters; // Character

    Type m_type;
    TagName m_tagName; // StartTag, EndTag.
    bool m_externalCharactersIsAll8BitData; // Character
    bool m_selfClosing { false }; // StartTag, EndTag.
    bool m_hasDuplicateAttribute { false };
};

const Attribute* findAttribute(const Vector<Attribute>&, const QualifiedName&);
bool hasAttribute(const Vector<Attribute>&, const AtomString& localName);

inline auto AtomHTMLToken::type() const -> Type
{
    return m_type;
}

inline const AtomString& AtomHTMLToken::name() const
{
    ASSERT(m_type == Type::StartTag || m_type == Type::EndTag || m_type == Type::DOCTYPE);
    if (!m_name.isNull())
        return m_name;

    ASSERT(m_type == Type::StartTag || m_type == Type::EndTag);
    ASSERT(m_tagName != TagName::Unknown);
    return tagNameAsString(m_tagName);
}

inline void AtomHTMLToken::setTagName(TagName tagName)
{
    ASSERT(m_type == Type::StartTag || m_type == Type::EndTag);
    ASSERT(tagName != TagName::Unknown);
    m_tagName = tagName;
}

inline TagName AtomHTMLToken::tagName() const
{
    ASSERT(m_type == Type::StartTag || m_type == Type::EndTag);
    return m_tagName;
}

inline bool AtomHTMLToken::selfClosing() const
{
    ASSERT(m_type == Type::StartTag || m_type == Type::EndTag);
    return m_selfClosing;
}

inline Vector<Attribute>& AtomHTMLToken::attributes()
{
    ASSERT(m_type == Type::StartTag || m_type == Type::EndTag);
    return m_attributes;
}

inline const Vector<Attribute>& AtomHTMLToken::attributes() const
{
    ASSERT(m_type == Type::StartTag || m_type == Type::EndTag);
    return m_attributes;
}

inline Span<const UChar> AtomHTMLToken::characters() const
{
    ASSERT(m_type == Type::Character);
    return m_externalCharacters;
}

inline bool AtomHTMLToken::charactersIsAll8BitData() const
{
    ASSERT(m_type == Type::Character);
    return m_externalCharactersIsAll8BitData;
}

inline const String& AtomHTMLToken::comment() const
{
    ASSERT(m_type == Type::Comment);
    return m_data;
}

inline String& AtomHTMLToken::comment()
{
    ASSERT(m_type == Type::Comment);
    return m_data;
}

inline bool AtomHTMLToken::forceQuirks() const
{
    ASSERT(m_type == Type::DOCTYPE);
    return m_doctypeData->forceQuirks;
}

inline String AtomHTMLToken::publicIdentifier() const
{
    ASSERT(m_type == Type::DOCTYPE);
    if (!m_doctypeData->hasPublicIdentifier)
        return String();
    return StringImpl::create8BitIfPossible(m_doctypeData->publicIdentifier);
}

inline String AtomHTMLToken::systemIdentifier() const
{
    if (!m_doctypeData->hasSystemIdentifier)
        return String();
    return StringImpl::create8BitIfPossible(m_doctypeData->systemIdentifier);
}

inline const Attribute* findAttribute(const Vector<Attribute>& attributes, const QualifiedName& name)
{
    for (auto& attribute : attributes) {
        if (attribute.name().matches(name))
            return &attribute;
    }
    return nullptr;
}

inline bool hasAttribute(const Vector<Attribute>& attributes, const AtomString& localName)
{
    for (auto& attribute : attributes) {
        if (attribute.localName() == localName)
            return true;
    }
    return false;
}

inline void AtomHTMLToken::initializeAttributes(const HTMLToken::AttributeList& attributes)
{
    unsigned size = attributes.size();
    if (!size)
        return;

    HashSet<AtomString> addedAttributes;
    addedAttributes.reserveInitialCapacity(size);
    m_attributes.reserveInitialCapacity(size);
    for (auto& attribute : attributes) {
        if (attribute.name.isEmpty())
            continue;

        auto qualifiedName = HTMLNameCache::makeAttributeQualifiedName(attribute.name);

        if (addedAttributes.add(qualifiedName.localName()).isNewEntry)
            m_attributes.uncheckedAppend(Attribute(WTFMove(qualifiedName), HTMLNameCache::makeAttributeValue(attribute.value)));
        else
            m_hasDuplicateAttribute = true;
    }
}

inline AtomHTMLToken::AtomHTMLToken(HTMLToken& token)
    : m_type(token.type())
{
    switch (m_type) {
    case Type::Uninitialized:
        ASSERT_NOT_REACHED();
        return;
    case Type::DOCTYPE:
        if (LIKELY(token.name().size() == 4 && equal(HTMLNames::htmlTag->localName().impl(), token.name().data(), 4)))
            m_name = HTMLNames::htmlTag->localName();
        else
            m_name = AtomString(token.name().data(), token.name().size());
        m_doctypeData = token.releaseDoctypeData();
        return;
    case Type::EndOfFile:
        return;
    case Type::StartTag:
    case Type::EndTag:
        m_selfClosing = token.selfClosing();
        m_tagName = findTagName(token.name());
        if (UNLIKELY(m_tagName == TagName::Unknown))
            m_name = AtomString(token.name().data(), token.name().size());
        initializeAttributes(token.attributes());
        return;
    case Type::Comment: {
        if (token.commentIsAll8BitData())
            m_data = String::make8Bit(token.comment().data(), token.comment().size());
        else
            m_data = String(token.comment().data(), token.comment().size());
        return;
    }
    case Type::Character:
        m_externalCharacters = token.characters().span();
        m_externalCharactersIsAll8BitData = token.charactersIsAll8BitData();
        return;
    }
    ASSERT_NOT_REACHED();
}

inline AtomHTMLToken::AtomHTMLToken(HTMLToken::Type type, TagName tagName, const AtomString& name, Vector<Attribute>&& attributes)
    : m_name(name)
    , m_attributes(WTFMove(attributes))
    , m_type(type)
    , m_tagName(tagName)
{
    ASSERT(type == Type::StartTag || type == Type::EndTag);
    ASSERT(findTagName(name) == tagName);
}

inline AtomHTMLToken::AtomHTMLToken(HTMLToken::Type type, TagName tagName, Vector<Attribute>&& attributes)
    : m_attributes(WTFMove(attributes))
    , m_type(type)
    , m_tagName(tagName)
{
    ASSERT(type == Type::StartTag || type == Type::EndTag);
    ASSERT(tagName != TagName::Unknown);
}

} // namespace WebCore
