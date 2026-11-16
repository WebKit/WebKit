/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <array>
#include <utility>
#include <wtf/ASCIICType.h>
#include <wtf/Forward.h>
#include <wtf/OptionSet.h>
#include <wtf/text/Latin1Character.h>
#include <wtf/text/StringView.h>

namespace WebCore {

class QualifiedName;
template<typename> class ExceptionOr;

enum class CustomElementNameValidationStatus : uint8_t {
    Valid,
    FirstCharacterIsNotLowercaseASCIILetter,
    ContainsNoHyphen,
    ContainsUppercaseASCIILetter,
    ContainsDisallowedCharacter,
    ConflictsWithStandardElementName
};

// https://dom.spec.whatwg.org/#validate-and-extract
namespace NameValidation {

// https://dom.spec.whatwg.org/#valid-element-name
NODELETE bool isValidElementName(StringView);
NODELETE constexpr bool isInvalidElementNameCharacterAfterAlphaStart(char16_t character)
{
    return !character || isASCIIWhitespace(character) || character == '/' || character == '>';
}
NODELETE bool isValidElementName(const QualifiedName&);

enum class NameCharacterProperty : uint8_t {
    InvalidAfterAlphaStart = 1 << 0,
    Hyphen = 1 << 1,
    ASCIIUpper = 1 << 2,
};

inline constexpr auto latin1NameCharacterProperties = [] {
    std::array<OptionSet<NameCharacterProperty>, 256> properties { };
    for (unsigned codeUnit = 0; codeUnit < properties.size(); ++codeUnit) {
        if (isInvalidElementNameCharacterAfterAlphaStart(codeUnit))
            properties[codeUnit].add(NameCharacterProperty::InvalidAfterAlphaStart);
        if (codeUnit == '-')
            properties[codeUnit].add(NameCharacterProperty::Hyphen);
        if (isASCIIUpper(codeUnit))
            properties[codeUnit].add(NameCharacterProperty::ASCIIUpper);
    }
    return properties;
}();

NODELETE constexpr OptionSet<NameCharacterProperty> nameCharacterProperties(Latin1Character character)
{
    return latin1NameCharacterProperties[character];
}

NODELETE constexpr OptionSet<NameCharacterProperty> nameCharacterProperties(char16_t character)
{
    // Only ASCII characters have properties.
    return character < latin1NameCharacterProperties.size() ? latin1NameCharacterProperties[character] : OptionSet<NameCharacterProperty> { };
}

// Stops at the first character that isn't allowed after an ASCII alpha.
template<typename CharacterType>
NODELETE constexpr OptionSet<NameCharacterProperty> combinedNameCharacterProperties(std::span<const CharacterType> characters)
{
    OptionSet<NameCharacterProperty> combined;
    for (auto character : characters) {
        auto properties = nameCharacterProperties(character);
        if (properties.contains(NameCharacterProperty::InvalidAfterAlphaStart))
            return properties;
        combined.add(properties);
    }
    return combined;
}

enum class ElementNameKind : uint8_t { Invalid, CustomElementName, OtherElementName };

// Checks for a valid element name and a valid custom element name in one pass. Unlike validateCustomElementName(),
// doesn't check for the standard element names that valid custom element names exclude.
NODELETE ALWAYS_INLINE ElementNameKind classifyElementName(StringView name)
{
    if (name.isEmpty())
        return ElementNameKind::Invalid;

    auto firstCharacter = name[0];
    if (!isASCIIAlpha(firstCharacter))
        return isValidElementName(name) ? ElementNameKind::OtherElementName : ElementNameKind::Invalid;

    auto properties = name.is8Bit() ? combinedNameCharacterProperties(name.span8()) : combinedNameCharacterProperties(name.span16());
    if (properties.contains(NameCharacterProperty::InvalidAfterAlphaStart))
        return ElementNameKind::Invalid;

    if (isASCIILower(firstCharacter) && properties.contains(NameCharacterProperty::Hyphen) && !properties.contains(NameCharacterProperty::ASCIIUpper))
        return ElementNameKind::CustomElementName;
    return ElementNameKind::OtherElementName;
}

// https://html.spec.whatwg.org/#valid-custom-element-name
CustomElementNameValidationStatus validateCustomElementName(const AtomString&);

// https://dom.spec.whatwg.org/#valid-attribute-name
NODELETE bool isValidAttributeName(StringView);

// https://dom.spec.whatwg.org/#valid-doctype-name
NODELETE bool isValidDoctypeName(StringView);

// https://www.w3.org/TR/xml/#NT-Name - used by createProcessingInstruction
NODELETE bool isValidXMLName(StringView);

ExceptionOr<QualifiedName> parseQualifiedElementName(const AtomString& namespaceURI, const AtomString& qualifiedName);
ExceptionOr<QualifiedName> parseQualifiedAttributeName(const AtomString& namespaceURI, const AtomString& qualifiedName);
ExceptionOr<std::pair<AtomString, AtomString>> parseQualifiedAttributeName(const AtomString& qualifiedName);

NODELETE bool hasValidNamespaceForElements(const QualifiedName&);
NODELETE bool hasValidNamespaceForAttributes(const QualifiedName&);

} // namespace NameValidation

} // namespace WebCore
