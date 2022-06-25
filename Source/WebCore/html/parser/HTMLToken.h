/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
 * Copyright (C) 2015-2021 Apple Inc. All Rights Reserved.
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

#include "Attribute.h"

namespace WebCore {

struct DoctypeData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Vector<UChar> publicIdentifier;
    Vector<UChar> systemIdentifier;
    bool hasPublicIdentifier { false };
    bool hasSystemIdentifier { false };
    bool forceQuirks { false };
};

class HTMLToken {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Type : uint8_t {
        Uninitialized,
        DOCTYPE,
        StartTag,
        EndTag,
        Comment,
        Character,
        EndOfFile,
    };

    struct Attribute {
        Vector<UChar, 32> name;
        Vector<UChar, 64> value;
    };

    typedef Vector<Attribute, 10> AttributeList;
    typedef Vector<UChar, 256> DataVector;

    HTMLToken() = default;

    void clear();

    Type type() const;

    // EndOfFile

    void makeEndOfFile();

    // StartTag, EndTag, DOCTYPE.

    const DataVector& name() const;

    void appendToName(UChar);

    // DOCTYPE.

    void beginDOCTYPE();
    void beginDOCTYPE(UChar);

    void setForceQuirks();

    void setPublicIdentifierToEmptyString();
    void setSystemIdentifierToEmptyString();

    void appendToPublicIdentifier(UChar);
    void appendToSystemIdentifier(UChar);

    std::unique_ptr<DoctypeData> releaseDoctypeData();

    // StartTag, EndTag.

    bool selfClosing() const;
    const AttributeList& attributes() const;

    void beginStartTag(LChar);

    void beginEndTag(LChar);
    void beginEndTag(const Vector<LChar, 32>&);

    void beginAttribute();
    void appendToAttributeName(UChar);
    void appendToAttributeValue(UChar);
    void appendToAttributeValue(unsigned index, StringView value);
    template<typename CharacterType> void appendToAttributeValue(Span<const CharacterType>);
    void endAttribute();

    void setSelfClosing();

    // Character.

    // Starting a character token works slightly differently than starting
    // other types of tokens because we want to save a per-character branch.
    // There is no beginCharacters, and appending a character sets the type.

    const DataVector& characters() const;
    bool charactersIsAll8BitData() const;

    void appendToCharacter(LChar);
    void appendToCharacter(UChar);
    void appendToCharacter(const Vector<LChar, 32>&);
    template<typename CharacterType> void appendToCharacter(Span<const CharacterType>);

    // Comment.

    const DataVector& comment() const;
    bool commentIsAll8BitData() const;

    void beginComment();
    void appendToComment(char);
    void appendToComment(ASCIILiteral);
    void appendToComment(UChar);

private:
    DataVector m_data;
    UChar m_data8BitCheck { 0 };
    Type m_type { Type::Uninitialized };

    // For StartTag and EndTag
    bool m_selfClosing;
    AttributeList m_attributes;
    Attribute* m_currentAttribute;

    // For DOCTYPE
    std::unique_ptr<DoctypeData> m_doctypeData;
};

const HTMLToken::Attribute* findAttribute(const Vector<HTMLToken::Attribute>&, StringView name);

inline void HTMLToken::clear()
{
    m_type = Type::Uninitialized;
    m_data.clear();
    m_data8BitCheck = 0;
}

inline HTMLToken::Type HTMLToken::type() const
{
    return m_type;
}

inline void HTMLToken::makeEndOfFile()
{
    ASSERT(m_type == Type::Uninitialized);
    m_type = Type::EndOfFile;
}

inline const HTMLToken::DataVector& HTMLToken::name() const
{
    ASSERT(m_type == Type::StartTag || m_type == Type::EndTag || m_type == Type::DOCTYPE);
    return m_data;
}

inline void HTMLToken::appendToName(UChar character)
{
    ASSERT(m_type == Type::StartTag || m_type == Type::EndTag || m_type == Type::DOCTYPE);
    ASSERT(character);
    m_data.append(character);
    m_data8BitCheck |= character;
}

inline void HTMLToken::setForceQuirks()
{
    ASSERT(m_type == Type::DOCTYPE);
    m_doctypeData->forceQuirks = true;
}

inline void HTMLToken::beginDOCTYPE()
{
    ASSERT(m_type == Type::Uninitialized);
    m_type = Type::DOCTYPE;
    m_doctypeData = makeUnique<DoctypeData>();
}

inline void HTMLToken::beginDOCTYPE(UChar character)
{
    ASSERT(character);
    beginDOCTYPE();
    m_data.append(character);
    m_data8BitCheck |= character;
}

inline void HTMLToken::setPublicIdentifierToEmptyString()
{
    ASSERT(m_type == Type::DOCTYPE);
    m_doctypeData->hasPublicIdentifier = true;
    m_doctypeData->publicIdentifier.clear();
}

inline void HTMLToken::setSystemIdentifierToEmptyString()
{
    ASSERT(m_type == Type::DOCTYPE);
    m_doctypeData->hasSystemIdentifier = true;
    m_doctypeData->systemIdentifier.clear();
}

inline void HTMLToken::appendToPublicIdentifier(UChar character)
{
    ASSERT(character);
    ASSERT(m_type == Type::DOCTYPE);
    ASSERT(m_doctypeData->hasPublicIdentifier);
    m_doctypeData->publicIdentifier.append(character);
}

inline void HTMLToken::appendToSystemIdentifier(UChar character)
{
    ASSERT(character);
    ASSERT(m_type == Type::DOCTYPE);
    ASSERT(m_doctypeData->hasSystemIdentifier);
    m_doctypeData->systemIdentifier.append(character);
}

inline std::unique_ptr<DoctypeData> HTMLToken::releaseDoctypeData()
{
    return WTFMove(m_doctypeData);
}

inline bool HTMLToken::selfClosing() const
{
    ASSERT(m_type == Type::StartTag || m_type == Type::EndTag);
    return m_selfClosing;
}

inline void HTMLToken::setSelfClosing()
{
    ASSERT(m_type == Type::StartTag || m_type == Type::EndTag);
    m_selfClosing = true;
}

inline void HTMLToken::beginStartTag(LChar character)
{
    ASSERT(character);
    ASSERT(m_type == Type::Uninitialized);
    m_type = Type::StartTag;
    m_selfClosing = false;
    m_attributes.clear();

#if ASSERT_ENABLED
    m_currentAttribute = nullptr;
#endif

    m_data.append(character);
}

inline void HTMLToken::beginEndTag(LChar character)
{
    ASSERT(m_type == Type::Uninitialized);
    m_type = Type::EndTag;
    m_selfClosing = false;
    m_attributes.clear();

#if ASSERT_ENABLED
    m_currentAttribute = nullptr;
#endif

    m_data.append(character);
}

inline void HTMLToken::beginEndTag(const Vector<LChar, 32>& characters)
{
    ASSERT(m_type == Type::Uninitialized);
    m_type = Type::EndTag;
    m_selfClosing = false;
    m_attributes.clear();

#if ASSERT_ENABLED
    m_currentAttribute = nullptr;
#endif

    m_data.appendVector(characters);
}

inline void HTMLToken::beginAttribute()
{
    ASSERT(m_type == Type::StartTag || m_type == Type::EndTag);
    m_attributes.grow(m_attributes.size() + 1);
    m_currentAttribute = &m_attributes.last();
}

inline void HTMLToken::endAttribute()
{
    ASSERT(m_currentAttribute);
#if ASSERT_ENABLED
    m_currentAttribute = nullptr;
#endif
}

inline void HTMLToken::appendToAttributeName(UChar character)
{
    ASSERT(character);
    ASSERT(m_type == Type::StartTag || m_type == Type::EndTag);
    ASSERT(m_currentAttribute);
    m_currentAttribute->name.append(character);
}

inline void HTMLToken::appendToAttributeValue(UChar character)
{
    ASSERT(character);
    ASSERT(m_type == Type::StartTag || m_type == Type::EndTag);
    ASSERT(m_currentAttribute);
    m_currentAttribute->value.append(character);
}

template<typename CharacterType>
inline void HTMLToken::appendToAttributeValue(Span<const CharacterType> characters)
{
    ASSERT(m_type == Type::StartTag || m_type == Type::EndTag);
    ASSERT(m_currentAttribute);
    m_currentAttribute->value.append(characters);
}

inline void HTMLToken::appendToAttributeValue(unsigned i, StringView value)
{
    ASSERT(!value.isEmpty());
    ASSERT(m_type == Type::StartTag || m_type == Type::EndTag);
    append(m_attributes[i].value, value);
}

inline const HTMLToken::AttributeList& HTMLToken::attributes() const
{
    ASSERT(m_type == Type::StartTag || m_type == Type::EndTag);
    return m_attributes;
}

inline const HTMLToken::DataVector& HTMLToken::characters() const
{
    ASSERT(m_type == Type::Character);
    return m_data;
}

inline bool HTMLToken::charactersIsAll8BitData() const
{
    ASSERT(m_type == Type::Character);
    return m_data8BitCheck <= 0xFF;
}

inline void HTMLToken::appendToCharacter(LChar character)
{
    ASSERT(m_type == Type::Uninitialized || m_type == Type::Character);
    m_type = Type::Character;
    m_data.append(character);
}

inline void HTMLToken::appendToCharacter(UChar character)
{
    ASSERT(m_type == Type::Uninitialized || m_type == Type::Character);
    m_type = Type::Character;
    m_data.append(character);
    m_data8BitCheck |= character;
}

inline void HTMLToken::appendToCharacter(const Vector<LChar, 32>& characters)
{
    ASSERT(m_type == Type::Uninitialized || m_type == Type::Character);
    m_type = Type::Character;
    m_data.appendVector(characters);
}

template<typename CharacterType>
inline void HTMLToken::appendToCharacter(Span<const CharacterType> characters)
{
    m_type = Type::Character;
    m_data.append(characters);
    if constexpr (std::is_same_v<CharacterType, UChar>) {
        if (!charactersIsAll8BitData())
            return;
        for (auto character : characters)
            m_data8BitCheck |= character;
    }
}

inline const HTMLToken::DataVector& HTMLToken::comment() const
{
    ASSERT(m_type == Type::Comment);
    return m_data;
}

inline bool HTMLToken::commentIsAll8BitData() const
{
    ASSERT(m_type == Type::Comment);
    return m_data8BitCheck <= 0xFF;
}

inline void HTMLToken::beginComment()
{
    ASSERT(m_type == Type::Uninitialized);
    m_type = Type::Comment;
}

inline void HTMLToken::appendToComment(char character)
{
    ASSERT(character);
    ASSERT(m_type == Type::Comment);
    m_data.append(character);
}

inline void HTMLToken::appendToComment(ASCIILiteral literal)
{
    ASSERT(m_type == Type::Comment);
    m_data.append(literal.characters8(), literal.length());
}

inline void HTMLToken::appendToComment(UChar character)
{
    ASSERT(character);
    ASSERT(m_type == Type::Comment);
    m_data.append(character);
    m_data8BitCheck |= character;
}

inline const HTMLToken::Attribute* findAttribute(const HTMLToken::AttributeList& attributes, Span<const UChar> name)
{
    for (auto& attribute : attributes) {
        // FIXME: The one caller that uses this probably wants to ignore letter case.
        if (attribute.name.size() == name.size() && equal(attribute.name.data(), name.data(), name.size()))
            return &attribute;
    }
    return nullptr;
}

} // namespace WebCore
