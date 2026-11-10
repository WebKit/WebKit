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
#include "Settings.h"
#include "StyleAttributeMutationScope.h"
#include "StyleRuleType.h"
#include "StyleSheetContents.h"
#include "StyledElement.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PropertySetCSSDescriptors);

PropertySetCSSDescriptors::PropertySetCSSDescriptors(MutableStyleProperties& propertySet, CSSRule& parentRule)
    : m_parentRule(&parentRule)
    , m_propertySet(propertySet)
{
}

PropertySetCSSDescriptors::~PropertySetCSSDescriptors() = default;

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

RefPtr<StringImpl> PropertySetCSSDescriptors::item(unsigned i) const
{
    Ref propertySet = m_propertySet;

    for (unsigned j = 0; j <= i && j < propertySet->propertyCount(); j++) {
        if (!isExposed(propertySet->propertyAt(j).id()))
            i++;
    }

    if (i >= propertySet->propertyCount())
        return nullptr;

    return propertySet->propertyAt(i).cssName();
}

RefPtr<StringImpl> PropertySetCSSDescriptors::cssText() const
{
    return protect(propertySet())->asText(CSS::defaultSerializationContext());
}

ExceptionOr<void> PropertySetCSSDescriptors::setCssText(StringImpl* text)
{
    StyleAttributeMutationScope mutationScope { parentElement() };
    if (!willMutate())
        return { };

    bool changed = protect(propertySet())->parseDeclaration(text, cssParserContext());
    didMutate(changed ? MutationType::PropertyChanged : MutationType::StyleAttributeChanged);

    mutationScope.enqueueMutationRecord();
    return { };
}

CSSPropertyID PropertySetCSSDescriptors::resolvePropertyName(StringImpl* propertyName) const
{
    auto propertyID = cssPropertyID(propertyName);
    if (propertyID == CSSPropertyID::Size && ruleType() == StyleRuleType::Page)
        return CSSPropertyID::PageSize;
    return propertyID;
}

RefPtr<DeprecatedCSSOMValue> PropertySetCSSDescriptors::getPropertyCSSValue(StringImpl* propertyName)
{
    auto propertyID = resolvePropertyName(propertyName);
    if (!isExposed(propertyID))
        return nullptr;
    return wrapForDeprecatedCSSOM(protect(propertySet())->getPropertyCSSValue(propertyID).get());
}

RefPtr<StringImpl> PropertySetCSSDescriptors::getPropertyValue(StringImpl* propertyName)
{
    if (styleDeclarationType() == StyleDeclarationType::Function && isCustomPropertyName(propertyName))
        return protect(propertySet())->getCustomPropertyValue(propertyName);

    auto propertyID = resolvePropertyName(propertyName);
    if (!isExposed(propertyID))
        return nullptr;
    return getPropertyValueInternal(propertyID);
}

RefPtr<StringImpl> PropertySetCSSDescriptors::getPropertyPriority(StringImpl* propertyName)
{
    auto propertyID = resolvePropertyName(propertyName);
    if (!isExposed(propertyID))
        return emptyString();
    return protect(propertySet())->propertyIsImportant(propertyID) ? "important"_si : emptyString();
}

RefPtr<StringImpl> PropertySetCSSDescriptors::getPropertyShorthand(StringImpl* propertyName)
{
    auto propertyID = resolvePropertyName(propertyName);
    if (!isExposed(propertyID))
        return nullptr;
    return protect(propertySet())->getPropertyShorthand(propertyID);
}

bool PropertySetCSSDescriptors::isPropertyImplicit(StringImpl* propertyName)
{
    return protect(propertySet())->isPropertyImplicit(resolvePropertyName(propertyName));
}

ExceptionOr<void> PropertySetCSSDescriptors::setProperty(StringImpl* propertyName, StringImpl* value, StringImpl* priority)
{
    StyleAttributeMutationScope mutationScope { parentElement() };

    auto propertyID = resolvePropertyName(propertyName);
    if (!isExposed(propertyID))
        return { };

    if (!willMutate())
        return { };

    bool important = equalLettersIgnoringASCIICase(priority, "important"_s);
    if (!important && !isEmpty(priority))
        return { };

    bool changed = protect(propertySet())->setProperty(propertyID, value, cssParserContext(), important ? IsImportant::Yes : IsImportant::No);

    didMutate(changed ? MutationType::PropertyChanged : MutationType::NoChanges);

    if (changed) {
        // CSS DOM requires raising SyntaxError of parsing failed, but this is too dangerous for compatibility,
        // see <http://bugs.webkit.org/show_bug.cgi?id=7296>.
        mutationScope.enqueueMutationRecord();
    }

    return { };
}

ExceptionOr<RefPtr<StringImpl>> PropertySetCSSDescriptors::removeProperty(StringImpl* propertyName)
{
    StyleAttributeMutationScope mutationScope { parentElement() };

    auto propertyID = resolvePropertyName(propertyName);
    if (!isExposed(propertyID))
        return nullptr;

    if (!willMutate())
        return nullptr;

    RefPtr<StringImpl> result;
    bool changed = protect(propertySet())->removeProperty(propertyID, &result);

    didMutate(changed ? MutationType::PropertyChanged : MutationType::NoChanges);

    if (changed)
        mutationScope.enqueueMutationRecord();
    return result;
}

RefPtr<StringImpl> PropertySetCSSDescriptors::getPropertyValueInternal(CSSPropertyID propertyID) const
{
    if (!isExposed(propertyID))
        return { };

    auto value = protect(propertySet())->getPropertyValue(propertyID);

    if (!isEmpty(value))
        return value;

    return { };
}

ExceptionOr<void> PropertySetCSSDescriptors::setPropertyInternal(CSSPropertyID propertyID, StringImpl* value, IsImportant important)
{
    StyleAttributeMutationScope mutationScope { parentElement() };
    if (!willMutate())
        return { };

    if (!isExposed(propertyID))
        return { };

    if (protect(propertySet())->setProperty(propertyID, value, cssParserContext(), important)) {
        didMutate(MutationType::PropertyChanged);
        mutationScope.enqueueMutationRecord();
    } else
        didMutate(MutationType::NoChanges);

    return { };
}

bool PropertySetCSSDescriptors::isExposed(CSSPropertyID propertyID) const
{
    if (propertyID == CSSPropertyID::Invalid)
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
    auto* strongParentRule = m_parentRule.get();
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
    auto context = cssStyleSheet ? cssStyleSheet->contents().parserContext() : CSSParserContext(propertySet().cssParserMode());

    context.enclosingRuleType = ruleType();

    return context;
}

void PropertySetCSSDescriptors::reattach(MutableStyleProperties& propertySet)
{
    m_propertySet = propertySet;
}

} // namespace WebCore
