/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSStyleProperties.h"

#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSRule.h"
#include "CSSSerializationContext.h"
#include "CSSStyleSheet.h"
#include "DeprecatedCSSOMValue.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "HTMLNames.h"
#include "InspectorInstrumentation.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMWindowBase.h"
#include "LocalDOMWindow.h"
#include "MutableStyleProperties.h"
#include "Quirks.h"
#include "Settings.h"
#include "StyleAttributeMutationScope.h"
#include "StyleProperties.h"
#include "StyleSheetContents.h"
#include "StyledElement.h"
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CSSStyleProperties);
WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(PropertySetCSSStyleProperties);
WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(StyleRuleCSSStyleProperties);
WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(InlineCSSStyleProperties);

namespace {

enum class PropertyNamePrefix { None, Epub, WebKit };

static inline bool matchesCSSPropertyNamePrefix(const StringImpl& propertyName, ASCIILiteral prefix)
{
    ASSERT(toASCIILower(propertyName[0]) == prefix[0zu]);
    const size_t offset = 1;

#ifndef NDEBUG
    for (auto character : prefix.span8())
        ASSERT(isASCIILower(character));
    ASSERT(propertyName.length());
#endif

    // The prefix within the property name must be followed by a capital letter.
    // Other characters in the prefix within the property name must be lowercase.
    if (propertyName.length() < prefix.length() + 1)
        return false;

    for (size_t i = offset; i < prefix.length(); ++i) {
        if (propertyName[i] != prefix[i])
            return false;
    }

    if (!isASCIIUpper(propertyName[prefix.length()]))
        return false;

    return true;
}

static PropertyNamePrefix propertyNamePrefix(const StringImpl& propertyName)
{
    ASSERT(propertyName.length());

    // First character of the prefix within the property name may be upper or lowercase.
    char16_t firstChar = toASCIILower(propertyName[0]);
    switch (firstChar) {
    case 'e':
        if (matchesCSSPropertyNamePrefix(propertyName, "epub"_s))
            return PropertyNamePrefix::Epub;
        break;
    case 'w':
        if (matchesCSSPropertyNamePrefix(propertyName, "webkit"_s))
            return PropertyNamePrefix::WebKit;
        break;
    default:
        break;
    }
    return PropertyNamePrefix::None;
}

static inline void writeWebKitPrefix(std::span<char>& buffer)
{
    memcpySpan(consumeSpan(buffer, 8), "-webkit-"_span);
}

static inline void writeEpubPrefix(std::span<char>& buffer)
{
    memcpySpan(consumeSpan(buffer, 6), "-epub-"_span);
}

static CSSPropertyID parseJavaScriptCSSPropertyName(const AtomString& propertyName)
{
    using CSSPropertyIDMap = HashMap<AtomString, CSSPropertyID>;
    static NeverDestroyed<CSSPropertyIDMap> propertyIDCache;

    auto* propertyNameString = propertyName.impl();
    if (!propertyNameString)
        return CSSPropertyInvalid;

    unsigned length = propertyNameString->length();
    if (!length)
        return CSSPropertyInvalid;

    if (auto id = propertyIDCache.get().get(propertyName))
        return id;

    constexpr size_t bufferSize = maxCSSPropertyNameLength;
    std::array<char, bufferSize> buffer;
    std::span<char> bufferSpan { buffer };
    const char* name = buffer.data();

    unsigned i = 0;
    switch (propertyNamePrefix(*propertyNameString)) {
    case PropertyNamePrefix::None:
        if (isASCIIUpper((*propertyNameString)[0]))
            return CSSPropertyInvalid;
        break;
    case PropertyNamePrefix::Epub:
        writeEpubPrefix(bufferSpan);
        i += 4;
        break;
    case PropertyNamePrefix::WebKit:
        writeWebKitPrefix(bufferSpan);
        i += 6;
        break;
    }

    consume(bufferSpan) = toASCIILower((*propertyNameString)[i++]);

    char* stringEnd = std::to_address(std::span { buffer }.first(buffer.size() - 1).end());
    size_t bufferSizeLeft = stringEnd - bufferSpan.data();
    size_t propertySizeLeft = length - i;
    if (propertySizeLeft > bufferSizeLeft)
        return CSSPropertyInvalid;

    for (; i < length; ++i) {
        char16_t c = (*propertyNameString)[i];
        if (!c || !isASCII(c))
            return CSSPropertyInvalid; // illegal character
        if (isASCIIUpper(c)) {
            size_t bufferSizeLeft = stringEnd - bufferSpan.data();
            size_t propertySizeLeft = length - i + 1;
            if (propertySizeLeft > bufferSizeLeft)
                return CSSPropertyInvalid;
            bufferSpan[0] = '-';
            bufferSpan[1] = toASCIILowerUnchecked(c);
            skip(bufferSpan, 2);
        } else
            consume(bufferSpan) = c;
        ASSERT(!bufferSpan.empty());
    }
    ASSERT(!bufferSpan.empty());

    unsigned outputLength = bufferSpan.data() - buffer.data();
    auto id = findCSSProperty(name, outputLength);
    // FIXME: Why aren't we memoizing CSS property names we fail to find?
    if (id != CSSPropertyInvalid)
        propertyIDCache.get().add(propertyName, id);
    return id;
}

}

CSSPropertyID CSSStyleProperties::getCSSPropertyIDFromJavaScriptPropertyName(const AtomString& propertyName)
{
    // FIXME: This exposes properties disabled by settings. Pass result of CSSStyleProperties::settings instead of null?
    Settings* settings = nullptr;
    auto property = parseJavaScriptCSSPropertyName(propertyName);
    return isExposed(property, settings) ? property : CSSPropertyInvalid;
}

enum class CSSPropertyLookupMode { ConvertUsingDashPrefix, ConvertUsingNoDashPrefix, NoConversion };

template<CSSPropertyLookupMode mode> static CSSPropertyID lookupCSSPropertyFromIDLAttribute(const AtomString& attribute)
{
    static NeverDestroyed<HashMap<AtomString, CSSPropertyID>> cache;

    if (auto id = cache.get().get(attribute))
        return id;

    std::array<char, maxCSSPropertyNameLength> outputBuffer;
    size_t outputIndex = 0;

    if constexpr (mode == CSSPropertyLookupMode::ConvertUsingDashPrefix || mode == CSSPropertyLookupMode::ConvertUsingNoDashPrefix) {
        // Conversion is implementing the "IDL attribute to CSS property algorithm"
        // from https://drafts.csswg.org/cssom/#idl-attribute-to-css-property.

        if constexpr (mode == CSSPropertyLookupMode::ConvertUsingDashPrefix)
            outputBuffer[outputIndex++] = '-';

        readCharactersForParsing(attribute, [&](auto buffer) {
            while (buffer.hasCharactersRemaining()) {
                auto c = *buffer++;
                ASSERT_WITH_MESSAGE(isASCII(c), "Invalid property name: %s", attribute.string().utf8().data());
                if (isASCIIUpper(c)) {
                    outputBuffer[outputIndex++] = '-';
                    outputBuffer[outputIndex++] = toASCIILowerUnchecked(c);
                } else
                    outputBuffer[outputIndex++] = c;
            }
        });
    } else {
        readCharactersForParsing(attribute, [&](auto buffer) {
            while (buffer.hasCharactersRemaining()) {
                auto c = *buffer++;
                ASSERT_WITH_MESSAGE(c == '-' || isASCIILower(c), "Invalid property name: %s", attribute.string().utf8().data());
                outputBuffer[outputIndex++] = c;
            }
        });
    }

    auto id = findCSSProperty(outputBuffer.data(), outputIndex);
    ASSERT_WITH_MESSAGE(id != CSSPropertyInvalid, "Invalid property name: %s", attribute.string().utf8().data());
    cache.get().add(attribute, id);
    return id;
}

String CSSStyleProperties::propertyValueForCamelCasedIDLAttribute(const AtomString& attribute)
{
    auto propertyID = lookupCSSPropertyFromIDLAttribute<CSSPropertyLookupMode::ConvertUsingNoDashPrefix>(attribute);
    ASSERT_WITH_MESSAGE(propertyID != CSSPropertyInvalid, "Invalid attribute: %s", attribute.string().utf8().data());
    return getPropertyValueInternal(propertyID);
}

ExceptionOr<void> CSSStyleProperties::setPropertyValueForCamelCasedIDLAttribute(const AtomString& attribute, const String& value)
{
    auto propertyID = lookupCSSPropertyFromIDLAttribute<CSSPropertyLookupMode::ConvertUsingNoDashPrefix>(attribute);
    ASSERT_WITH_MESSAGE(propertyID != CSSPropertyInvalid, "Invalid attribute: %s", attribute.string().utf8().data());
    return setPropertyInternal(propertyID, value, IsImportant::No);
}

String CSSStyleProperties::propertyValueForWebKitCasedIDLAttribute(const AtomString& attribute)
{
    auto propertyID = lookupCSSPropertyFromIDLAttribute<CSSPropertyLookupMode::ConvertUsingDashPrefix>(attribute);
    ASSERT_WITH_MESSAGE(propertyID != CSSPropertyInvalid, "Invalid attribute: %s", attribute.string().utf8().data());
    return getPropertyValueInternal(propertyID);
}

ExceptionOr<void> CSSStyleProperties::setPropertyValueForWebKitCasedIDLAttribute(const AtomString& attribute, const String& value)
{
    auto propertyID = lookupCSSPropertyFromIDLAttribute<CSSPropertyLookupMode::ConvertUsingDashPrefix>(attribute);
    ASSERT_WITH_MESSAGE(propertyID != CSSPropertyInvalid, "Invalid attribute: %s", attribute.string().utf8().data());
    return setPropertyInternal(propertyID, value, IsImportant::No);
}

String CSSStyleProperties::propertyValueForDashedIDLAttribute(const AtomString& attribute)
{
    auto propertyID = lookupCSSPropertyFromIDLAttribute<CSSPropertyLookupMode::NoConversion>(attribute);
    ASSERT_WITH_MESSAGE(propertyID != CSSPropertyInvalid, "Invalid attribute: %s", attribute.string().utf8().data());
    return getPropertyValueInternal(propertyID);
}

ExceptionOr<void> CSSStyleProperties::setPropertyValueForDashedIDLAttribute(const AtomString& attribute, const String& value)
{
    auto propertyID = lookupCSSPropertyFromIDLAttribute<CSSPropertyLookupMode::NoConversion>(attribute);
    ASSERT_WITH_MESSAGE(propertyID != CSSPropertyInvalid, "Invalid attribute: %s", attribute.string().utf8().data());
    return setPropertyInternal(propertyID, value, IsImportant::No);
}

String CSSStyleProperties::propertyValueForEpubCasedIDLAttribute(const AtomString& attribute)
{
    auto propertyID = lookupCSSPropertyFromIDLAttribute<CSSPropertyLookupMode::ConvertUsingDashPrefix>(attribute);
    ASSERT_WITH_MESSAGE(propertyID != CSSPropertyInvalid, "Invalid attribute: %s", attribute.string().utf8().data());
    return getPropertyValueInternal(propertyID);
}

ExceptionOr<void> CSSStyleProperties::setPropertyValueForEpubCasedIDLAttribute(const AtomString& attribute, const String& value)
{
    auto propertyID = lookupCSSPropertyFromIDLAttribute<CSSPropertyLookupMode::ConvertUsingDashPrefix>(attribute);
    ASSERT_WITH_MESSAGE(propertyID != CSSPropertyInvalid, "Invalid attribute: %s", attribute.string().utf8().data());
    return setPropertyInternal(propertyID, value, IsImportant::No);
}

String CSSStyleProperties::cssFloat()
{
    return getPropertyValueInternal(CSSPropertyFloat);
}

ExceptionOr<void> CSSStyleProperties::setCssFloat(const String& value)
{
    return setPropertyInternal(CSSPropertyFloat, value, IsImportant::No);
}

// MARK: - PropertySetCSSStyleProperties

void PropertySetCSSStyleProperties::ref() const
{
    m_propertySet->ref();
}

void PropertySetCSSStyleProperties::deref() const
{
    m_propertySet->deref();
}

unsigned PropertySetCSSStyleProperties::length() const
{
    unsigned exposed = 0;
    for (auto property : *m_propertySet) {
        if (isExposed(property.id()))
            exposed++;
    }
    return exposed;
}

String PropertySetCSSStyleProperties::item(unsigned i) const
{
    for (unsigned j = 0; j <= i && j < m_propertySet->propertyCount(); j++) {
        if (!isExposed(m_propertySet->propertyAt(j).id()))
            i++;
    }

    if (i >= m_propertySet->propertyCount())
        return String();

    return m_propertySet->propertyAt(i).cssName();
}

String PropertySetCSSStyleProperties::cssText() const
{
    return m_propertySet->asText(CSS::defaultSerializationContext());
}

ExceptionOr<void> PropertySetCSSStyleProperties::setCssText(const String& text)
{
    StyleAttributeMutationScope mutationScope { parentElement() };
    if (!willMutate())
        return { };

    bool changed = m_propertySet->parseDeclaration(text, cssParserContext());
    didMutate(changed ? MutationType::PropertyChanged : MutationType::StyleAttributeChanged);

    mutationScope.enqueueMutationRecord();
    return { };
}

RefPtr<DeprecatedCSSOMValue> PropertySetCSSStyleProperties::getPropertyCSSValue(const String& propertyName)
{
    if (isCustomPropertyName(propertyName)) {
        RefPtr<CSSValue> value = m_propertySet->getCustomPropertyCSSValue(propertyName);
        if (!value)
            return nullptr;
        return wrapForDeprecatedCSSOM(value.get());
    }

    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return nullptr;
    return wrapForDeprecatedCSSOM(m_propertySet->getPropertyCSSValue(propertyID).get());
}

String PropertySetCSSStyleProperties::getPropertyValue(const String& propertyName)
{
    if (isCustomPropertyName(propertyName))
        return m_propertySet->getCustomPropertyValue(propertyName);

    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return String();
    return getPropertyValueInternal(propertyID);
}

String PropertySetCSSStyleProperties::getPropertyPriority(const String& propertyName)
{
    if (isCustomPropertyName(propertyName))
        return m_propertySet->customPropertyIsImportant(propertyName) ? "important"_s : emptyString();

    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return emptyString();
    return m_propertySet->propertyIsImportant(propertyID) ? "important"_s : emptyString();
}

String PropertySetCSSStyleProperties::getPropertyShorthand(const String& propertyName)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return String();
    return m_propertySet->getPropertyShorthand(propertyID);
}

bool PropertySetCSSStyleProperties::isPropertyImplicit(const String& propertyName)
{
    return m_propertySet->isPropertyImplicit(cssPropertyID(propertyName));
}

ExceptionOr<void> PropertySetCSSStyleProperties::setProperty(const String& propertyName, const String& value, const String& priority)
{
    StyleAttributeMutationScope mutationScope { parentElement() };

    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (isCustomPropertyName(propertyName))
        propertyID = CSSPropertyCustom;

    if (!isExposed(propertyID))
        return { };

    if (!willMutate())
        return { };

    bool important = equalLettersIgnoringASCIICase(priority, "important"_s);
    if (!important && !priority.isEmpty())
        return { };

    bool changed;
    if (propertyID == CSSPropertyCustom) [[unlikely]]
        changed = m_propertySet->setCustomProperty(propertyName, value, cssParserContext(), important ? IsImportant::Yes : IsImportant::No);
    else
        changed = m_propertySet->setProperty(propertyID, value, cssParserContext(), important ? IsImportant::Yes : IsImportant::No);

    didMutate(changed ? MutationType::PropertyChanged : MutationType::NoChanges);

    if (changed) {
        // CSS DOM requires raising SyntaxError of parsing failed, but this is too dangerous for compatibility,
        // see <http://bugs.webkit.org/show_bug.cgi?id=7296>.
        mutationScope.enqueueMutationRecord();
    }

    return { };
}

ExceptionOr<String> PropertySetCSSStyleProperties::removeProperty(const String& propertyName)
{
    StyleAttributeMutationScope mutationScope { parentElement() };
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (isCustomPropertyName(propertyName))
        propertyID = CSSPropertyCustom;
    if (!isExposed(propertyID))
        return String();

    if (!willMutate())
        return String();

    String result;
    bool changed = propertyID != CSSPropertyCustom ? m_propertySet->removeProperty(propertyID, &result) : m_propertySet->removeCustomProperty(propertyName, &result);

    didMutate(changed ? MutationType::PropertyChanged : MutationType::NoChanges);

    if (changed)
        mutationScope.enqueueMutationRecord();
    return result;
}

String PropertySetCSSStyleProperties::getPropertyValueInternal(CSSPropertyID propertyID)
{
    if (!isExposed(propertyID))
        return { };

    auto value = m_propertySet->getPropertyValue(propertyID);

    if (!value.isEmpty())
        return value;

    return { };
}

ExceptionOr<void> PropertySetCSSStyleProperties::setPropertyInternal(CSSPropertyID propertyID, const String& value, IsImportant important)
{
    StyleAttributeMutationScope mutationScope { parentElement() };
    if (!willMutate())
        return { };

    if (!isExposed(propertyID))
        return { };

    if (m_propertySet->setProperty(propertyID, value, cssParserContext(), important)) {
        didMutate(MutationType::PropertyChanged);
        mutationScope.enqueueMutationRecord();
    } else
        didMutate(MutationType::NoChanges);

    return { };
}

bool PropertySetCSSStyleProperties::isExposed(CSSPropertyID propertyID) const
{
    if (propertyID == CSSPropertyInvalid)
        return false;

    auto parserContext = cssParserContext();
    bool parsingDescriptor = parserContext->enclosingRuleType && *parserContext->enclosingRuleType != StyleRuleType::Style;

    return WebCore::isExposed(propertyID, &parserContext->propertySettings)
        && (!CSSProperty::isDescriptorOnly(propertyID) || parsingDescriptor);
}

RefPtr<DeprecatedCSSOMValue> PropertySetCSSStyleProperties::wrapForDeprecatedCSSOM(CSSValue* internalValue)
{
    if (!internalValue)
        return nullptr;

    // The map is here to maintain the object identity of the CSSValues over multiple invocations.
    // FIXME: It is likely that the identity is not important for web compatibility and this code should be removed.
    auto& clonedValue = m_cssomValueWrappers.add(internalValue, WeakPtr<DeprecatedCSSOMValue>()).iterator->value;
    if (clonedValue)
        return clonedValue.get();

    auto wrapper = internalValue->createDeprecatedCSSOMWrapper(*this);
    clonedValue = wrapper;
    return wrapper;
}

StyleSheetContents* PropertySetCSSStyleProperties::contextStyleSheet() const
{
    CSSStyleSheet* cssStyleSheet = parentStyleSheet();
    return cssStyleSheet ? &cssStyleSheet->contents() : nullptr;
}

OptionalOrReference<CSSParserContext> PropertySetCSSStyleProperties::cssParserContext() const
{
    return CSSParserContext(m_propertySet->cssParserMode());
}

Ref<MutableStyleProperties> PropertySetCSSStyleProperties::copyProperties() const
{
    return m_propertySet->mutableCopy();
}

// MARK: - StyleRuleCSSStyleProperties

StyleRuleCSSStyleProperties::StyleRuleCSSStyleProperties(MutableStyleProperties& propertySet, CSSRule& parentRule)
    : PropertySetCSSStyleProperties(propertySet)
    , m_parentRuleType(parentRule.styleRuleType())
    , m_parentRule(&parentRule)
{
    m_propertySet->ref();
}

StyleRuleCSSStyleProperties::~StyleRuleCSSStyleProperties()
{
    m_propertySet->deref();
}

bool StyleRuleCSSStyleProperties::willMutate()
{
    if (!m_parentRule || !m_parentRule->parentStyleSheet())
        return false;
    m_parentRule->parentStyleSheet()->willMutateRules();
    return true;
}

void StyleRuleCSSStyleProperties::didMutate(MutationType type)
{
    ASSERT(m_parentRule);
    ASSERT(m_parentRule->parentStyleSheet());

    if (type == MutationType::PropertyChanged)
        m_cssomValueWrappers.clear();

    // Style sheet mutation needs to be signaled even if the change failed. willMutate*/didMutate* must pair.
    m_parentRule->parentStyleSheet()->didMutateRuleFromCSSStyleDeclaration();
}

CSSStyleSheet* StyleRuleCSSStyleProperties::parentStyleSheet() const
{
    return m_parentRule ? m_parentRule->parentStyleSheet() : nullptr;
}

OptionalOrReference<CSSParserContext> StyleRuleCSSStyleProperties::cssParserContext() const
{
    auto* styleSheet = contextStyleSheet();
    if (!styleSheet)
        return PropertySetCSSStyleProperties::cssParserContext();

    auto context = styleSheet->parserContext();
    context.enclosingRuleType = m_parentRuleType;

    return context;
}

void StyleRuleCSSStyleProperties::reattach(MutableStyleProperties& propertySet)
{
    m_propertySet->deref();
    m_propertySet = &propertySet;
    m_propertySet->ref();
}

// MARK: - InlineCSSStyleProperties

bool InlineCSSStyleProperties::willMutate()
{
    if (m_parentElement)
        InspectorInstrumentation::willInvalidateStyleAttr(*m_parentElement);
    return true;
}

void InlineCSSStyleProperties::didMutate(MutationType type)
{
    if (type == MutationType::NoChanges)
        return;

    if (type == MutationType::StyleAttributeChanged && m_parentElement) {
        m_parentElement->dirtyStyleAttribute();
        return;
    }

    m_cssomValueWrappers.clear();

    if (!m_parentElement)
        return;

    m_parentElement->invalidateStyleAttribute();
    InspectorInstrumentation::didInvalidateStyleAttr(*m_parentElement);
}

CSSStyleSheet* InlineCSSStyleProperties::parentStyleSheet() const
{
    return nullptr;
}

OptionalOrReference<CSSParserContext> InlineCSSStyleProperties::cssParserContext() const
{
    if (!m_parentElement)
        return PropertySetCSSStyleProperties::cssParserContext();

    auto& documentContext = m_parentElement->document().cssParserContext();
    if (documentContext.mode == m_propertySet->cssParserMode())
        return documentContext;

    CSSParserContext context(documentContext);
    context.mode = m_propertySet->cssParserMode();
    return context;
}

}
