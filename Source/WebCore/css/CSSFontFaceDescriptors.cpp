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
#include "CSSFontFaceDescriptors.h"

#include "CSSFontFaceRule.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSSerializationContext.h"
#include "CSSStyleSheet.h"
#include "DeprecatedCSSOMValueList.h"
#include "Document.h"
#include "MutableStyleProperties.h"
#include "Settings.h"
#include "StyleAttributeMutationScope.h"
#include "StyleSheetContents.h"
#include "StyledElement.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(CSSFontFaceDescriptors);

CSSFontFaceDescriptors::CSSFontFaceDescriptors(MutableStyleProperties& propertySet, CSSFontFaceRule& parentRule)
    : m_parentRule(&parentRule)
    , m_propertySet(propertySet)
{
}

CSSFontFaceDescriptors::~CSSFontFaceDescriptors() = default;

unsigned CSSFontFaceDescriptors::length() const
{
    unsigned exposed = 0;
    for (auto property : m_propertySet.get()) {
        if (isExposed(property.id()))
            exposed++;
    }
    return exposed;
}

String CSSFontFaceDescriptors::item(unsigned i) const
{
    for (unsigned j = 0; j <= i && j < m_propertySet->propertyCount(); j++) {
        if (!isExposed(m_propertySet->propertyAt(j).id()))
            i++;
    }

    if (i >= m_propertySet->propertyCount())
        return String();

    return m_propertySet->propertyAt(i).cssName();
}

String CSSFontFaceDescriptors::cssText() const
{
    return m_propertySet->asText(CSS::defaultSerializationContext());
}

ExceptionOr<void> CSSFontFaceDescriptors::setCssText(const String& text)
{
    StyleAttributeMutationScope mutationScope { parentElement() };
    if (!willMutate())
        return { };

    bool changed = m_propertySet->parseDeclaration(text, cssParserContext());
    didMutate(changed ? MutationType::PropertyChanged : MutationType::StyleAttributeChanged);

    mutationScope.enqueueMutationRecord();
    return { };
}

RefPtr<DeprecatedCSSOMValue> CSSFontFaceDescriptors::getPropertyCSSValue(const String& propertyName)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return nullptr;
    return wrapForDeprecatedCSSOM(m_propertySet->getPropertyCSSValue(propertyID).get());
}

String CSSFontFaceDescriptors::getPropertyValue(const String& propertyName)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return String();
    return getPropertyValueInternal(propertyID);
}

String CSSFontFaceDescriptors::getPropertyPriority(const String& propertyName)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return emptyString();
    return m_propertySet->propertyIsImportant(propertyID) ? "important"_s : emptyString();
}

String CSSFontFaceDescriptors::getPropertyShorthand(const String& propertyName)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return String();
    return m_propertySet->getPropertyShorthand(propertyID);
}

bool CSSFontFaceDescriptors::isPropertyImplicit(const String& propertyName)
{
    return m_propertySet->isPropertyImplicit(cssPropertyID(propertyName));
}

ExceptionOr<void> CSSFontFaceDescriptors::setProperty(const String& propertyName, const String& value, const String& priority)
{
    StyleAttributeMutationScope mutationScope { parentElement() };

    auto propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return { };

    if (!willMutate())
        return { };

    bool important = equalLettersIgnoringASCIICase(priority, "important"_s);
    if (!important && !priority.isEmpty())
        return { };

    bool changed = m_propertySet->setProperty(propertyID, value, cssParserContext(), important ? IsImportant::Yes : IsImportant::No);

    didMutate(changed ? MutationType::PropertyChanged : MutationType::NoChanges);

    if (changed) {
        // CSS DOM requires raising SyntaxError of parsing failed, but this is too dangerous for compatibility,
        // see <http://bugs.webkit.org/show_bug.cgi?id=7296>.
        mutationScope.enqueueMutationRecord();
    }

    return { };
}

ExceptionOr<String> CSSFontFaceDescriptors::removeProperty(const String& propertyName)
{
    StyleAttributeMutationScope mutationScope { parentElement() };

    auto propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return String();

    if (!willMutate())
        return String();

    String result;
    bool changed = m_propertySet->removeProperty(propertyID, &result);

    didMutate(changed ? MutationType::PropertyChanged : MutationType::NoChanges);

    if (changed)
        mutationScope.enqueueMutationRecord();
    return result;
}

String CSSFontFaceDescriptors::getPropertyValueInternal(CSSPropertyID propertyID) const
{
    if (!isExposed(propertyID))
        return { };

    auto value = m_propertySet->getPropertyValue(propertyID);

    if (!value.isEmpty())
        return value;

    return { };
}

ExceptionOr<void> CSSFontFaceDescriptors::setPropertyInternal(CSSPropertyID propertyID, const String& value, IsImportant important)
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

bool CSSFontFaceDescriptors::isExposed(CSSPropertyID propertyID) const
{
    if (propertyID == CSSPropertyInvalid)
        return false;

    auto parserContext = cssParserContext();
    return WebCore::isExposed(propertyID, &parserContext.propertySettings);
}

RefPtr<DeprecatedCSSOMValue> CSSFontFaceDescriptors::wrapForDeprecatedCSSOM(CSSValue* internalValue)
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

bool CSSFontFaceDescriptors::willMutate()
{
    if (!m_parentRule || !m_parentRule->parentStyleSheet())
        return false;
    m_parentRule->parentStyleSheet()->willMutateRules();
    return true;
}

void CSSFontFaceDescriptors::didMutate(MutationType type)
{
    ASSERT(m_parentRule);
    ASSERT(m_parentRule->parentStyleSheet());

    if (type == MutationType::PropertyChanged)
        m_cssomValueWrappers.clear();

    // Style sheet mutation needs to be signaled even if the change failed. willMutate*/didMutate* must pair.
    m_parentRule->parentStyleSheet()->didMutateRuleFromCSSStyleDeclaration();
}

CSSStyleSheet* CSSFontFaceDescriptors::parentStyleSheet() const
{
    return m_parentRule ? m_parentRule->parentStyleSheet() : nullptr;
}

CSSRule* CSSFontFaceDescriptors::parentRule() const
{
    return m_parentRule;
}

CSSParserContext CSSFontFaceDescriptors::cssParserContext() const
{
    auto* cssStyleSheet = parentStyleSheet();
    auto context = cssStyleSheet ? cssStyleSheet->contents().parserContext() : CSSParserContext(m_propertySet->cssParserMode());

    context.enclosingRuleType = StyleRuleType::FontFace;

    return context;
}

void CSSFontFaceDescriptors::reattach(MutableStyleProperties& propertySet)
{
    m_propertySet = propertySet;
}

// MARK: - Descriptors

// @font-face 'src'
String CSSFontFaceDescriptors::src() const
{
    return getPropertyValueInternal(CSSPropertySrc);
}

ExceptionOr<void> CSSFontFaceDescriptors::setSrc(const String& value)
{
    return setPropertyInternal(CSSPropertySrc, value, IsImportant::No);
}

// @font-face 'fontFamily'
String CSSFontFaceDescriptors::fontFamily() const
{
    return getPropertyValueInternal(CSSPropertyFontFamily);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontFamily(const String& value)
{
    return setPropertyInternal(CSSPropertyFontFamily, value, IsImportant::No);
}

// @font-face 'font-style'
String CSSFontFaceDescriptors::fontStyle() const
{
    return getPropertyValueInternal(CSSPropertyFontStyle);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontStyle(const String& value)
{
    return setPropertyInternal(CSSPropertyFontStyle, value, IsImportant::No);
}

// @font-face 'font-weight'
String CSSFontFaceDescriptors::fontWeight() const
{
    return getPropertyValueInternal(CSSPropertyFontWeight);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontWeight(const String& value)
{
    return setPropertyInternal(CSSPropertyFontWeight, value, IsImportant::No);
}

// @font-face 'font-stretch'
String CSSFontFaceDescriptors::fontStretch() const
{
    return getPropertyValueInternal(CSSPropertyFontWidth); // NOTE: 'font-stretch' is an alias for 'font-width'.
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontStretch(const String& value)
{
    return setPropertyInternal(CSSPropertyFontWidth, value, IsImportant::No); // NOTE: 'font-stretch' is an alias for 'font-width'.
}

// @font-face 'font-width'
String CSSFontFaceDescriptors::fontWidth() const
{
    return getPropertyValueInternal(CSSPropertyFontWidth);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontWidth(const String& value)
{
    return setPropertyInternal(CSSPropertyFontWidth, value, IsImportant::No);
}

// @font-face 'size-adjust'
String CSSFontFaceDescriptors::sizeAdjust() const
{
    return getPropertyValueInternal(CSSPropertySizeAdjust);
}

ExceptionOr<void> CSSFontFaceDescriptors::setSizeAdjust(const String& value)
{
    return setPropertyInternal(CSSPropertySizeAdjust, value, IsImportant::No);
}

// @font-face 'unicode-range'
String CSSFontFaceDescriptors::unicodeRange() const
{
    return getPropertyValueInternal(CSSPropertyUnicodeRange);
}

ExceptionOr<void> CSSFontFaceDescriptors::setUnicodeRange(const String& value)
{
    return setPropertyInternal(CSSPropertyUnicodeRange, value, IsImportant::No);
}

// @font-face 'font-feature-settings'
String CSSFontFaceDescriptors::fontFeatureSettings() const
{
    return getPropertyValueInternal(CSSPropertyFontFeatureSettings);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontFeatureSettings(const String& value)
{
    return setPropertyInternal(CSSPropertyFontFeatureSettings, value, IsImportant::No);
}

// @font-face 'font-display'
String CSSFontFaceDescriptors::fontDisplay() const
{
    return getPropertyValueInternal(CSSPropertyFontDisplay);
}

ExceptionOr<void> CSSFontFaceDescriptors::setFontDisplay(const String& value)
{
    return setPropertyInternal(CSSPropertyFontDisplay, value, IsImportant::No);
}

} // namespace WebCore
