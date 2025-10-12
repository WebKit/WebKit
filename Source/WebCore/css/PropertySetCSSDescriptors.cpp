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
#include "PropertySetCSSDescriptors.h"

#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSRule.h"
#include "CSSSerializationContext.h"
#include "CSSStyleSheet.h"
#include "DeprecatedCSSOMValue.h"
#include "DeprecatedCSSOMValueList.h"
#include "Document.h"
#include "MutableStyleProperties.h"
#include "NodeInlines.h"
#include "Settings.h"
#include "StyleAttributeMutationScope.h"
#include "StyleSheetContents.h"
#include "StyledElement.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(PropertySetCSSDescriptors);

PropertySetCSSDescriptors::PropertySetCSSDescriptors(MutableStyleProperties& propertySet, CSSRule& parentRule)
    : m_parentRule(&parentRule)
    , m_propertySet(propertySet)
{
}

PropertySetCSSDescriptors::~PropertySetCSSDescriptors() = default;

Ref<MutableStyleProperties> PropertySetCSSDescriptors::protectedPropertySet() const
{
    return m_propertySet;
}

unsigned PropertySetCSSDescriptors::length() const
{
    Ref propertySet = m_propertySet;

    unsigned exposed = 0;
    for (auto property : propertySet.get()) {
        if (isExposed(property.id()))
            exposed++;
    }
    return exposed;
}

String PropertySetCSSDescriptors::item(unsigned i) const
{
    Ref propertySet = m_propertySet;

    for (unsigned j = 0; j <= i && j < propertySet->propertyCount(); j++) {
        if (!isExposed(propertySet->propertyAt(j).id()))
            i++;
    }

    if (i >= propertySet->propertyCount())
        return String();

    return propertySet->propertyAt(i).cssName();
}

String PropertySetCSSDescriptors::cssText() const
{
    return protectedPropertySet()->asText(CSS::defaultSerializationContext());
}

ExceptionOr<void> PropertySetCSSDescriptors::setCssText(const String& text)
{
    StyleAttributeMutationScope mutationScope { parentElement() };
    if (!willMutate())
        return { };

    bool changed = protectedPropertySet()->parseDeclaration(text, cssParserContext());
    didMutate(changed ? MutationType::PropertyChanged : MutationType::StyleAttributeChanged);

    mutationScope.enqueueMutationRecord();
    return { };
}

RefPtr<DeprecatedCSSOMValue> PropertySetCSSDescriptors::getPropertyCSSValue(const String& propertyName)
{
    auto propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return nullptr;
    return wrapForDeprecatedCSSOM(protectedPropertySet()->getPropertyCSSValue(propertyID).get());
}

String PropertySetCSSDescriptors::getPropertyValue(const String& propertyName)
{
    if (styleDeclarationType() == StyleDeclarationType::Function && isCustomPropertyName(propertyName))
        return protectedPropertySet()->getCustomPropertyValue(propertyName);

    auto propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return String();
    return getPropertyValueInternal(propertyID);
}

String PropertySetCSSDescriptors::getPropertyPriority(const String& propertyName)
{
    auto propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return emptyString();
    return protectedPropertySet()->propertyIsImportant(propertyID) ? "important"_s : emptyString();
}

String PropertySetCSSDescriptors::getPropertyShorthand(const String& propertyName)
{
    auto propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return String();
    return protectedPropertySet()->getPropertyShorthand(propertyID);
}

bool PropertySetCSSDescriptors::isPropertyImplicit(const String& propertyName)
{
    return protectedPropertySet()->isPropertyImplicit(cssPropertyID(propertyName));
}

ExceptionOr<void> PropertySetCSSDescriptors::setProperty(const String& propertyName, const String& value, const String& priority)
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

    bool changed = protectedPropertySet()->setProperty(propertyID, value, cssParserContext(), important ? IsImportant::Yes : IsImportant::No);

    didMutate(changed ? MutationType::PropertyChanged : MutationType::NoChanges);

    if (changed) {
        // CSS DOM requires raising SyntaxError of parsing failed, but this is too dangerous for compatibility,
        // see <http://bugs.webkit.org/show_bug.cgi?id=7296>.
        mutationScope.enqueueMutationRecord();
    }

    return { };
}

ExceptionOr<String> PropertySetCSSDescriptors::removeProperty(const String& propertyName)
{
    StyleAttributeMutationScope mutationScope { parentElement() };

    auto propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return String();

    if (!willMutate())
        return String();

    String result;
    bool changed = protectedPropertySet()->removeProperty(propertyID, &result);

    didMutate(changed ? MutationType::PropertyChanged : MutationType::NoChanges);

    if (changed)
        mutationScope.enqueueMutationRecord();
    return result;
}

String PropertySetCSSDescriptors::getPropertyValueInternal(CSSPropertyID propertyID) const
{
    if (!isExposed(propertyID))
        return { };

    auto value = protectedPropertySet()->getPropertyValue(propertyID);

    if (!value.isEmpty())
        return value;

    return { };
}

ExceptionOr<void> PropertySetCSSDescriptors::setPropertyInternal(CSSPropertyID propertyID, const String& value, IsImportant important)
{
    StyleAttributeMutationScope mutationScope { parentElement() };
    if (!willMutate())
        return { };

    if (!isExposed(propertyID))
        return { };

    if (protectedPropertySet()->setProperty(propertyID, value, cssParserContext(), important)) {
        didMutate(MutationType::PropertyChanged);
        mutationScope.enqueueMutationRecord();
    } else
        didMutate(MutationType::NoChanges);

    return { };
}

bool PropertySetCSSDescriptors::isExposed(CSSPropertyID propertyID) const
{
    if (propertyID == CSSPropertyInvalid)
        return false;

    auto parserContext = cssParserContext();
    return WebCore::isExposed(propertyID, &parserContext.propertySettings);
}

RefPtr<DeprecatedCSSOMValue> PropertySetCSSDescriptors::wrapForDeprecatedCSSOM(CSSValue* internalValue)
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

bool PropertySetCSSDescriptors::willMutate()
{
    if (styleDeclarationType() == StyleDeclarationType::Function) {
        // FIXME: Use <declaration-rule-list> parsing.
        return false;
    }

    RefPtr strongParentRule = m_parentRule.get();
    ASSERT(strongParentRule);
    if (!strongParentRule)
        return false;

    RefPtr strongParentStyleSheet = strongParentRule->parentStyleSheet();
    ASSERT(strongParentStyleSheet);
    if (!strongParentStyleSheet)
        return false;

    strongParentStyleSheet->willMutateRules();
    return true;
}

void PropertySetCSSDescriptors::didMutate(MutationType type)
{
    if (type == MutationType::PropertyChanged)
        m_cssomValueWrappers.clear();

    RefPtr strongParentRule = m_parentRule.get();
    ASSERT(strongParentRule);
    if (!strongParentRule)
        return;

    RefPtr strongParentStyleSheet = strongParentRule->parentStyleSheet();
    ASSERT(strongParentStyleSheet);
    if (!strongParentStyleSheet)
        return;

    // Style sheet mutation needs to be signaled even if the change failed. willMutate*/didMutate* must pair.
    strongParentStyleSheet->didMutateRuleFromCSSStyleDeclaration();
}

CSSStyleSheet* PropertySetCSSDescriptors::parentStyleSheet() const
{
    RefPtr strongParentRule = m_parentRule.get();
    if (!strongParentRule)
        return nullptr;
    return strongParentRule->parentStyleSheet();
}

CSSRule* PropertySetCSSDescriptors::parentRule() const
{
    return m_parentRule.get();
}

CSSParserContext PropertySetCSSDescriptors::cssParserContext() const
{
    RefPtr cssStyleSheet = parentStyleSheet();
    auto context = cssStyleSheet ? cssStyleSheet->contents().parserContext() : CSSParserContext(protectedPropertySet()->cssParserMode());

    context.enclosingRuleType = ruleType();

    return context;
}

void PropertySetCSSDescriptors::reattach(MutableStyleProperties& propertySet)
{
    m_propertySet = propertySet;
}

} // namespace WebCore
