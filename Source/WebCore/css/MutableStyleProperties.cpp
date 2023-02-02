/**
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MutableStyleProperties.h"

#include "CSSCustomPropertyValue.h"
#include "CSSParser.h"
#include "CSSValuePool.h"
#include "PropertySetCSSStyleDeclaration.h"
#include "StylePropertiesInlines.h"
#include "StylePropertyShorthand.h"

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MutableStyleProperties);

MutableStyleProperties::MutableStyleProperties(CSSParserMode mode)
    : StyleProperties(mode)
{
}

MutableStyleProperties::MutableStyleProperties(Vector<CSSProperty>&& properties)
    : StyleProperties(HTMLStandardMode)
    , m_propertyVector(WTFMove(properties))
{
}

MutableStyleProperties::~MutableStyleProperties() = default;

MutableStyleProperties::MutableStyleProperties(const StyleProperties& other)
    : StyleProperties(other.cssParserMode())
{
    if (is<MutableStyleProperties>(other))
        m_propertyVector = downcast<MutableStyleProperties>(other).m_propertyVector;
    else {
        m_propertyVector = WTF::map(downcast<ImmutableStyleProperties>(other), [](auto property) {
            return property.toCSSProperty();
        });
    }
}

Ref<MutableStyleProperties> MutableStyleProperties::create()
{
    return adoptRef(*new MutableStyleProperties(HTMLQuirksMode));
}

Ref<MutableStyleProperties> MutableStyleProperties::create(CSSParserMode mode)
{
    return adoptRef(*new MutableStyleProperties(mode));
}

Ref<MutableStyleProperties> MutableStyleProperties::create(Vector<CSSProperty>&& properties)
{
    return adoptRef(*new MutableStyleProperties(WTFMove(properties)));
}

Ref<MutableStyleProperties> MutableStyleProperties::createEmpty()
{
    return adoptRef(*new MutableStyleProperties({ }));
}

// FIXME: Change StylePropertyShorthand::properties to return a Span and delete this.
static inline Span<const CSSPropertyID> span(const StylePropertyShorthand& shorthand)
{
    return { shorthand.properties(), shorthand.length() };
}

inline bool MutableStyleProperties::removeShorthandProperty(CSSPropertyID propertyID, String* returnText)
{
    // FIXME: Use serializeShorthandValue here to return the value of the removed shorthand as we do when removing a longhand.
    if (returnText)
        *returnText = String();
    return removeProperties(span(shorthandForProperty(propertyID)));
}

bool MutableStyleProperties::removePropertyAtIndex(int index, String* returnText)
{
    if (index == -1) {
        if (returnText)
            *returnText = String();
        return false;
    }

    if (returnText) {
        auto property = propertyAt(index);
        *returnText = WebCore::serializeLonghandValue(property.id(), *property.value());
    }

    // A more efficient removal strategy would involve marking entries as empty
    // and sweeping them when the vector grows too big.
    m_propertyVector.remove(index);
    return true;
}

inline bool MutableStyleProperties::removeLonghandProperty(CSSPropertyID propertyID, String* returnText)
{
    return removePropertyAtIndex(findPropertyIndex(propertyID), returnText);
}

bool MutableStyleProperties::removeProperty(CSSPropertyID propertyID, String* returnText)
{
    return isLonghand(propertyID) ? removeLonghandProperty(propertyID, returnText) : removeShorthandProperty(propertyID, returnText);
}

bool MutableStyleProperties::removeCustomProperty(const String& propertyName, String* returnText)
{
    return removePropertyAtIndex(findCustomPropertyIndex(propertyName), returnText);
}

bool MutableStyleProperties::setProperty(CSSPropertyID propertyID, const String& value, bool important, CSSParserContext parserContext, bool* didFailParsing)
{
    if (!isExposed(propertyID, &parserContext.propertySettings) && !isInternal(propertyID)) {
        // Allow internal properties as we use them to handle certain DOM-exposed values
        // (e.g. -webkit-font-size-delta from execCommand('FontSizeDelta')).
        ASSERT_NOT_REACHED();
        return false;
    }

    // Setting the value to an empty string just removes the property in both IE and Gecko.
    // Setting it to null seems to produce less consistent results, but we treat it just the same.
    if (value.isEmpty())
        return removeProperty(propertyID);

    // When replacing an existing property value, this moves the property to the end of the list.
    // Firefox preserves the position, and MSIE moves the property to the beginning.
    parserContext.mode = cssParserMode();
    auto parseResult = CSSParser::parseValue(*this, propertyID, value, important, parserContext);
    if (didFailParsing)
        *didFailParsing = parseResult == CSSParser::ParseResult::Error;
    return parseResult == CSSParser::ParseResult::Changed;
}

bool MutableStyleProperties::setProperty(CSSPropertyID propertyID, const String& value, bool important, bool* didFailParsing)
{
    CSSParserContext parserContext(cssParserMode());
    return setProperty(propertyID, value, important, parserContext, didFailParsing);
}

bool MutableStyleProperties::setCustomProperty(const String& propertyName, const String& value, bool important, CSSParserContext parserContext)
{
    // Setting the value to an empty string just removes the property in both IE and Gecko.
    // Setting it to null seems to produce less consistent results, but we treat it just the same.
    if (value.isEmpty())
        return removeCustomProperty(propertyName);

    // When replacing an existing property value, this moves the property to the end of the list.
    // Firefox preserves the position, and MSIE moves the property to the beginning.
    parserContext.mode = cssParserMode();
    return CSSParser::parseCustomPropertyValue(*this, AtomString { propertyName }, value, important, parserContext) == CSSParser::ParseResult::Changed;
}

void MutableStyleProperties::setProperty(CSSPropertyID propertyID, RefPtr<CSSValue>&& value, bool important)
{
    if (isLonghand(propertyID)) {
        setProperty(CSSProperty(propertyID, WTFMove(value), important));
        return;
    }
    auto shorthand = shorthandForProperty(propertyID);
    removeProperties(span(shorthand));
    for (auto longhand : shorthand)
        m_propertyVector.append(CSSProperty(longhand, value.copyRef(), important));
}

bool MutableStyleProperties::canUpdateInPlace(const CSSProperty& property, CSSProperty* toReplace) const
{
    // If the property is in a logical property group, we can't just update the value in-place,
    // because afterwards there might be another property of the same group but different mapping logic.
    // In that case the latter might override the former, so setProperty would have no effect.
    CSSPropertyID id = property.id();
    if (CSSProperty::isInLogicalPropertyGroup(id)) {
        ASSERT(toReplace >= m_propertyVector.begin());
        ASSERT(toReplace < m_propertyVector.end());
        for (CSSProperty* it = toReplace + 1; it != m_propertyVector.end(); ++it) {
            if (CSSProperty::areInSameLogicalPropertyGroupWithDifferentMappingLogic(id, it->id()))
                return false;
        }
    }
    return true;
}

bool MutableStyleProperties::setProperty(const CSSProperty& property, CSSProperty* slot)
{
    ASSERT(property.id() == CSSPropertyCustom || isLonghand(property.id()));
    auto* toReplace = slot;
    if (!slot) {
        if (property.id() == CSSPropertyCustom) {
            if (property.value())
                toReplace = findCustomCSSPropertyWithName(downcast<CSSCustomPropertyValue>(*property.value()).name());
        } else
            toReplace = findCSSPropertyWithID(property.id());
    }
    if (toReplace) {
        if (canUpdateInPlace(property, toReplace)) {
            if (*toReplace == property)
                return false;
            *toReplace = property;
            return true;
        }
        m_propertyVector.remove(toReplace - m_propertyVector.begin());
    }
    m_propertyVector.append(property);
    return true;
}

bool MutableStyleProperties::setProperty(CSSPropertyID propertyID, CSSValueID identifier, bool important)
{
    ASSERT(isLonghand(propertyID));
    return setProperty(CSSProperty(propertyID, CSSPrimitiveValue::create(identifier), important));
}

bool MutableStyleProperties::parseDeclaration(const String& styleDeclaration, CSSParserContext context)
{
    auto oldProperties = WTFMove(m_propertyVector);
    m_propertyVector.clear();

    context.mode = cssParserMode();
    CSSParser(context).parseDeclaration(*this, styleDeclaration);

    // We could do better. Just changing property order does not require style invalidation.
    return oldProperties != m_propertyVector;
}

bool MutableStyleProperties::addParsedProperties(const ParsedPropertyVector& properties)
{
    bool anyChanged = false;
    m_propertyVector.reserveCapacity(m_propertyVector.size() + properties.size());
    for (const auto& property : properties) {
        if (addParsedProperty(property))
            anyChanged = true;
    }
    return anyChanged;
}

bool MutableStyleProperties::addParsedProperty(const CSSProperty& property)
{
    if (property.id() == CSSPropertyCustom) {
        if ((property.value() && !customPropertyIsImportant(downcast<CSSCustomPropertyValue>(*property.value()).name())) || property.isImportant())
            return setProperty(property);
        return false;
    }
    return setProperty(property);
}

void MutableStyleProperties::mergeAndOverrideOnConflict(const StyleProperties& other)
{
    for (auto property : other)
        addParsedProperty(property.toCSSProperty());
}

void MutableStyleProperties::clear()
{
    m_propertyVector.clear();
}

bool MutableStyleProperties::removeProperties(Span<const CSSPropertyID> properties)
{
    if (m_propertyVector.isEmpty())
        return false;

    // FIXME: This is always used with static sets and in that case constructing the hash repeatedly is pretty pointless.
    HashSet<CSSPropertyID> toRemove;
    toRemove.add(properties.begin(), properties.end());

    return m_propertyVector.removeAllMatching([&toRemove](const CSSProperty& property) {
        return toRemove.contains(property.id());
    }) > 0;
}

int MutableStyleProperties::findPropertyIndex(CSSPropertyID propertyID) const
{
    // Convert here propertyID into an uint16_t to compare it with the metadata's m_propertyID to avoid
    // the compiler converting it to an int multiple times in the loop.
    auto* properties = m_propertyVector.data();
    uint16_t id = static_cast<uint16_t>(propertyID);
    for (int n = m_propertyVector.size() - 1 ; n >= 0; --n) {
        if (properties[n].metadata().m_propertyID == id)
            return n;
    }
    return -1;
}

int MutableStyleProperties::findCustomPropertyIndex(StringView propertyName) const
{
    auto* properties = m_propertyVector.data();
    for (int n = m_propertyVector.size() - 1 ; n >= 0; --n) {
        if (properties[n].metadata().m_propertyID == CSSPropertyCustom) {
            // We found a custom property. See if the name matches.
            if (!properties[n].value())
                continue;
            if (downcast<CSSCustomPropertyValue>(*properties[n].value()).name() == propertyName)
                return n;
        }
    }
    return -1;
}

CSSProperty* MutableStyleProperties::findCSSPropertyWithID(CSSPropertyID propertyID)
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return nullptr;
    return &m_propertyVector.at(foundPropertyIndex);
}

CSSProperty* MutableStyleProperties::findCustomCSSPropertyWithName(const String& propertyName)
{
    int foundPropertyIndex = findCustomPropertyIndex(propertyName);
    if (foundPropertyIndex == -1)
        return nullptr;
    return &m_propertyVector.at(foundPropertyIndex);
}

CSSStyleDeclaration& MutableStyleProperties::ensureInlineCSSStyleDeclaration(StyledElement& parentElement)
{
    if (!m_cssomWrapper)
        m_cssomWrapper = makeUnique<InlineCSSStyleDeclaration>(*this, parentElement);
    ASSERT(m_cssomWrapper->parentElement() == &parentElement);
    return *m_cssomWrapper;
}

}
