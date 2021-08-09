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

#include "Attribute.h"

namespace WebCore {

struct DoctypeData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    bool hasPublicIdentifier { false };
    bool hasSystemIdentifier { false };
    Vector<UChar> publicIdentifier;
    Vector<UChar> systemIdentifier;
    bool forceQuirks { false };
};

class HTMLToken {
    WTF_MAKE_FAST_ALLOCATED;
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

    struct Attribute {
        Vector<UChar, 32> name;
        Vector<UChar, 64> value;

        // Used by HTMLSourceTracker.
        unsigned startOffset;
        unsigned endOffset;
    };

    typedef Vector<Attribute, 10> AttributeList;
    typedef Vector<UChar, 256> DataVector;

    HTMLToken();

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

    void beginStartTag(UChar);

    void beginEndTag(LChar);
    void beginEndTag(const Vector<LChar, 32>&);

    void beginAttribute(unsigned offset);
    void appendToAttributeName(UChar);
    void appendToAttributeValue(UChar);
    void endAttribute(unsigned offset);

    void setSelfClosing();

    // Used by HTMLTokenizer on behalf of HTMLSourceTracker.
    void setAttributeBaseOffset(unsigned attributeBaseOffset) { m_attributeBaseOffset = attributeBaseOffset; }

public:
    // Used by the XSSAuditor to nuke XSS-laden attributes.
    void eraseValueOfAttribute(unsigned index);
    void appendToAttributeValue(unsigned index, StringView value);

    // Character.

    // Starting a character token works slightly differently than starting
    // other types of tokens because we want to save a per-character branch.
    // There is no beginCharacters, and appending a character sets the type.

    const DataVector& characters() const;
    bool charactersIsAll8BitData() const;

    void appendToCharacter(LChar);
    void appendToCharacter(UChar);
    void appendToCharacter(const Vector<LChar, 32>&);

    // Comment.

    const DataVector& comment() const;
    bool commentIsAll8BitData() const;

    void beginComment();
    void appendToComment(UChar);

private:
    Type m_type;

    DataVector m_data;
    UChar m_data8BitCheck;

    // For StartTag and EndTag
    bool m_selfClosing;
    AttributeList m_attributes;
    Attribute* m_currentAttribute;

    // For DOCTYPE
    std::unique_ptr<DoctypeData> m_doctypeData;

    unsigned m_attributeBaseOffset { 0 }; // Changes across document.write() boundaries.
};

const HTMLToken::Attribute* findAttribute(const Vector<HTMLToken::Attribute>&, StringView name);

inline HTMLToken::HTMLToken()
    : m_type(Uninitialized)
    , m_data8BitCheck(0)
{
}

inline void HTMLToken::clear()
{
    m_type = Uninitialized;
    m_data.clear();
    m_data8BitCheck = 0;
}

inline HTMLToken::Type HTMLToken::type() const
{
    return m_type;
}

inline void HTMLToken::makeEndOfFile()
{
    ASSERT(m_type == Uninitialized);
    m_type = EndOfFile;
}

inline const HTMLToken::DataVector& HTMLToken::name() const
{
    ASSERT(m_type == StartTag || m_type == EndTag || m_type == DOCTYPE);
    return m_data;
}

inline void HTMLToken::appendToName(UChar character)
{
    ASSERT(m_type == StartTag || m_type == EndTag || m_type == DOCTYPE);
    ASSERT(character);
    m_data.append(character);
    m_data8BitCheck |= character;
}

inline void HTMLToken::setForceQuirks()
{
    ASSERT(m_type == DOCTYPE);
    m_doctypeData->forceQuirks = true;
}

inline void HTMLToken::beginDOCTYPE()
{
    ASSERT(m_type == Uninitialized);
    m_type = DOCTYPE;
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
    ASSERT(m_type == DOCTYPE);
    m_doctypeData->hasPublicIdentifier = true;
    m_doctypeData->publicIdentifier.clear();
}

inline void HTMLToken::setSystemIdentifierToEmptyString()
{
    ASSERT(m_type == DOCTYPE);
    m_doctypeData->hasSystemIdentifier = true;
    m_doctypeData->systemIdentifier.clear();
}

inline void HTMLToken::appendToPublicIdentifier(UChar character)
{
    ASSERT(character);
    ASSERT(m_type == DOCTYPE);
    ASSERT(m_doctypeData->hasPublicIdentifier);
    m_doctypeData->publicIdentifier.append(character);
}

inline void HTMLToken::appendToSystemIdentifier(UChar character)
{
    ASSERT(character);
    ASSERT(m_type == DOCTYPE);
    ASSERT(m_doctypeData->hasSystemIdentifier);
    m_doctypeData->systemIdentifier.append(character);
}

inline std::unique_ptr<DoctypeData> HTMLToken::releaseDoctypeData()
{
    return WTFMove(m_doctypeData);
}

inline bool HTMLToken::selfClosing() const
{
    ASSERT(m_type == StartTag || m_type == EndTag);
    return m_selfClosing;
}

inline void HTMLToken::setSelfClosing()
{
    ASSERT(m_type == StartTag || m_type == EndTag);
    m_selfClosing = true;
}

inline void HTMLToken::beginStartTag(UChar character)
{
    ASSERT(character);
    ASSERT(m_type == Uninitialized);
    m_type = StartTag;
    m_selfClosing = false;
    m_attributes.clear();

#if ASSERT_ENABLED
    m_currentAttribute = nullptr;
#endif

    m_data.append(character);
    m_data8BitCheck = character;
}

inline void HTMLToken::beginEndTag(LChar character)
{
    ASSERT(m_type == Uninitialized);
    m_type = EndTag;
    m_selfClosing = false;
    m_attributes.clear();

#if ASSERT_ENABLED
    m_currentAttribute = nullptr;
#endif

    m_data.append(character);
}

inline void HTMLToken::beginEndTag(const Vector<LChar, 32>& characters)
{
    ASSERT(m_type == Uninitialized);
    m_type = EndTag;
    m_selfClosing = false;
    m_attributes.clear();

#if ASSERT_ENABLED
    m_currentAttribute = nullptr;
#endif

    m_data.appendVector(characters);
}

inline void HTMLToken::beginAttribute(unsigned offset)
{
    ASSERT(m_type == StartTag || m_type == EndTag);
    ASSERT(offset);

    m_attributes.grow(m_attributes.size() + 1);
    m_currentAttribute = &m_attributes.last();

    m_currentAttribute->startOffset = offset - m_attributeBaseOffset;
}

inline void HTMLToken::endAttribute(unsigned offset)
{
    ASSERT(offset);
    ASSERT(m_currentAttribute);
    m_currentAttribute->endOffset = offset - m_attributeBaseOffset;
#if ASSERT_ENABLED
    m_currentAttribute = nullptr;
#endif
}

inline void HTMLToken::appendToAttributeName(UChar character)
{
    ASSERT(character);
    ASSERT(m_type == StartTag || m_type == EndTag);
    ASSERT(m_currentAttribute);
    m_currentAttribute->name.append(character);
}

inline void HTMLToken::appendToAttributeValue(UChar character)
{
    ASSERT(character);
    ASSERT(m_type == StartTag || m_type == EndTag);
    ASSERT(m_currentAttribute);
    m_currentAttribute->value.append(character);
}

inline void HTMLToken::appendToAttributeValue(unsigned i, StringView value)
{
    ASSERT(!value.isEmpty());
    ASSERT(m_type == StartTag || m_type == EndTag);
    append(m_attributes[i].value, value);
}

inline const HTMLToken::AttributeList& HTMLToken::attributes() const
{
    ASSERT(m_type == StartTag || m_type == EndTag);
    return m_attributes;
}

// Used by the XSSAuditor to nuke XSS-laden attributes.
inline void HTMLToken::eraseValueOfAttribute(unsigned i)
{
    ASSERT(m_type == StartTag || m_type == EndTag);
    ASSERT(i < m_attributes.size());
    m_attributes[i].value.clear();
}

inline const HTMLToken::DataVector& HTMLToken::characters() const
{
    ASSERT(m_type == Character);
    return m_data;
}

inline bool HTMLToken::charactersIsAll8BitData() const
{
    ASSERT(m_type == Character);
    return m_data8BitCheck <= 0xFF;
}

inline void HTMLToken::appendToCharacter(LChar character)
{
    ASSERT(m_type == Uninitialized || m_type == Character);
    m_type = Character;
    m_data.append(character);
}

inline void HTMLToken::appendToCharacter(UChar character)
{
    ASSERT(m_type == Uninitialized || m_type == Character);
    m_type = Character;
    m_data.append(character);
    m_data8BitCheck |= character;
}

inline void HTMLToken::appendToCharacter(const Vector<LChar, 32>& characters)
{
    ASSERT(m_type == Uninitialized || m_type == Character);
    m_type = Character;
    m_data.appendVector(characters);
}

inline const HTMLToken::DataVector& HTMLToken::comment() const
{
    ASSERT(m_type == Comment);
    return m_data;
}

inline bool HTMLToken::commentIsAll8BitData() const
{
    ASSERT(m_type == Comment);
    return m_data8BitCheck <= 0xFF;
}

inline void HTMLToken::beginComment()
{
    ASSERT(m_type == Uninitialized);
    m_type = Comment;
}

inline void HTMLToken::appendToComment(UChar character)
{
    ASSERT(character);
    ASSERT(m_type == Comment);
    m_data.append(character);
    m_data8BitCheck |= character;
}

inline bool nameMatches(const HTMLToken::Attribute& attribute, StringView name)
{
    unsigned size = name.length();
    if (attribute.name.size() != size)
        return false;
    for (unsigned i = 0; i < size; ++i) {
        // FIXME: The one caller that uses this probably wants to ignore letter case.
        if (attribute.name[i] != name[i])
            return false;
    }
    return true;
}

inline const HTMLToken::Attribute* findAttribute(const HTMLToken::AttributeList& attributes, StringView name)
{
    for (auto& attribute : attributes) {
        if (nameMatches(attribute, name))
            return &attribute;
    }
    return nullptr;
}

} // namespace WebCore
