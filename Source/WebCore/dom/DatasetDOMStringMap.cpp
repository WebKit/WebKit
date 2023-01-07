/*
 * Copyright (C) 2010, 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DatasetDOMStringMap.h"

#include "ElementInlines.h"
#include <wtf/ASCIICType.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DatasetDOMStringMap);

static bool isValidAttributeName(const String& name)
{
    if (!name.startsWith("data-"_s))
        return false;

    unsigned length = name.length();
    for (unsigned i = 5; i < length; ++i) {
        if (isASCIIUpper(name[i]))
            return false;
    }

    return true;
}

static String convertAttributeNameToPropertyName(const String& name)
{
    StringBuilder stringBuilder;

    unsigned length = name.length();
    for (unsigned i = 5; i < length; ++i) {
        UChar character = name[i];
        if (character != '-')
            stringBuilder.append(character);
        else {
            if ((i + 1 < length) && isASCIILower(name[i + 1])) {
                stringBuilder.append(toASCIIUpper(name[i + 1]));
                ++i;
            } else
                stringBuilder.append(character);
        }
    }

    return stringBuilder.toString();
}

static bool isValidPropertyName(const String& name)
{
    unsigned length = name.length();
    for (unsigned i = 0; i < length; ++i) {
        if (name[i] == '-' && (i + 1 < length) && isASCIILower(name[i + 1]))
            return false;
    }
    return true;
}

template<typename CharacterType>
static inline AtomString convertPropertyNameToAttributeName(const StringImpl& name)
{
    const CharacterType dataPrefix[] = { 'd', 'a', 't', 'a', '-' };

    Vector<CharacterType, 32> buffer;

    unsigned length = name.length();
    buffer.reserveInitialCapacity(std::size(dataPrefix) + length);

    buffer.append(dataPrefix, std::size(dataPrefix));

    const CharacterType* characters = name.characters<CharacterType>();
    for (unsigned i = 0; i < length; ++i) {
        CharacterType character = characters[i];
        if (isASCIIUpper(character)) {
            buffer.append('-');
            buffer.append(toASCIILower(character));
        } else
            buffer.append(character);
    }
    return AtomString(buffer.data(), buffer.size());
}

static AtomString convertPropertyNameToAttributeName(const String& name)
{
    if (name.isNull())
        return nullAtom();

    StringImpl* nameImpl = name.impl();
    if (nameImpl->is8Bit())
        return convertPropertyNameToAttributeName<LChar>(*nameImpl);
    return convertPropertyNameToAttributeName<UChar>(*nameImpl);
}

void DatasetDOMStringMap::ref()
{
    m_element.ref();
}

void DatasetDOMStringMap::deref()
{
    m_element.deref();
}

bool DatasetDOMStringMap::isSupportedPropertyName(const String& propertyName) const
{
    if (!m_element.hasAttributes())
        return false;

    auto attributeIteratorAccessor = m_element.attributesIterator();
    if (attributeIteratorAccessor.attributeCount() == 1) {
        // Avoid creating AtomString when there is only one attribute.
        const auto& attribute = *attributeIteratorAccessor.begin();
        if (convertAttributeNameToPropertyName(attribute.localName()) == propertyName)
            return true;
    } else {
        auto attributeName = convertPropertyNameToAttributeName(propertyName);
        for (const Attribute& attribute : attributeIteratorAccessor) {
            if (attribute.localName() == attributeName)
                return true;
        }
    }
    
    return false;
}

Vector<String> DatasetDOMStringMap::supportedPropertyNames() const
{
    Vector<String> names;

    if (!m_element.hasAttributes())
        return names;

    for (auto& attribute : m_element.attributesIterator()) {
        if (isValidAttributeName(attribute.localName()))
            names.append(convertAttributeNameToPropertyName(attribute.localName()));
    }

    return names;
}

const AtomString* DatasetDOMStringMap::item(const String& propertyName) const
{
    if (m_element.hasAttributes()) {
        AttributeIteratorAccessor attributeIteratorAccessor = m_element.attributesIterator();

        if (attributeIteratorAccessor.attributeCount() == 1) {
            // Avoid creating AtomString when there is only one attribute.
            const Attribute& attribute = *attributeIteratorAccessor.begin();
            if (convertAttributeNameToPropertyName(attribute.localName()) == propertyName)
                return &attribute.value();
        } else {
            AtomString attributeName = convertPropertyNameToAttributeName(propertyName);
            for (const Attribute& attribute : attributeIteratorAccessor) {
                if (attribute.localName() == attributeName)
                    return &attribute.value();
            }
        }
    }

    return nullptr;
}

String DatasetDOMStringMap::namedItem(const AtomString& name) const
{
    if (const auto* value = item(name))
        return *value;
    return String { };
}

ExceptionOr<void> DatasetDOMStringMap::setNamedItem(const String& name, const AtomString& value)
{
    if (!isValidPropertyName(name))
        return Exception { SyntaxError };
    return m_element.setAttribute(convertPropertyNameToAttributeName(name), value);
}

bool DatasetDOMStringMap::deleteNamedProperty(const String& name)
{
    return m_element.removeAttribute(convertPropertyNameToAttributeName(name));
}

} // namespace WebCore
