/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "PropertySetCSSStyleDeclaration.h"

#include "CSSPropertyParser.h"
#include "CSSRule.h"
#include "CSSStyleSheet.h"
#include "CustomElementReactionQueue.h"
#include "DOMWindow.h"
#include "HTMLNames.h"
#include "InspectorInstrumentation.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMWindowBase.h"
#include "MutationObserverInterestGroup.h"
#include "MutationRecord.h"
#include "Quirks.h"
#include "StyleProperties.h"
#include "StyleSheetContents.h"
#include "StyledElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(PropertySetCSSStyleDeclaration);
WTF_MAKE_ISO_ALLOCATED_IMPL(StyleRuleCSSStyleDeclaration);
WTF_MAKE_ISO_ALLOCATED_IMPL(InlineCSSStyleDeclaration);

class StyleAttributeMutationScope {
    WTF_MAKE_NONCOPYABLE(StyleAttributeMutationScope);
public:
    StyleAttributeMutationScope(PropertySetCSSStyleDeclaration* decl)
    {
        ++s_scopeCount;

        if (s_scopeCount != 1) {
            ASSERT(s_currentDecl == decl);
            return;
        }

        ASSERT(!s_currentDecl);
        s_currentDecl = decl;

        auto* element = s_currentDecl->parentElement();
        if (!element)
            return;

        bool shouldReadOldValue = false;

        m_mutationRecipients = MutationObserverInterestGroup::createForAttributesMutation(*s_currentDecl->parentElement(), HTMLNames::styleAttr);
        if (m_mutationRecipients && m_mutationRecipients->isOldValueRequested())
            shouldReadOldValue = true;

        if (UNLIKELY(element->isDefinedCustomElement())) {
            auto* reactionQueue = element->reactionQueue();
            if (reactionQueue && reactionQueue->observesStyleAttribute()) {
                m_customElement = element;
                shouldReadOldValue = true;
            }
        }

        if (shouldReadOldValue)
            m_oldValue = s_currentDecl->parentElement()->getAttribute(HTMLNames::styleAttr);
    }

    ~StyleAttributeMutationScope()
    {
        --s_scopeCount;
        if (s_scopeCount)
            return;

        if (s_shouldDeliver) {
            if (m_mutationRecipients) {
                auto mutation = MutationRecord::createAttributes(*s_currentDecl->parentElement(), HTMLNames::styleAttr, m_oldValue);
                m_mutationRecipients->enqueueMutationRecord(WTFMove(mutation));
            }
            if (m_customElement) {
                auto& newValue = m_customElement->getAttribute(HTMLNames::styleAttr);
                CustomElementReactionQueue::enqueueAttributeChangedCallbackIfNeeded(*m_customElement, HTMLNames::styleAttr, m_oldValue, newValue);
            }
        }

        s_shouldDeliver = false;
        if (!s_shouldNotifyInspector) {
            s_currentDecl = nullptr;
            return;
        }
        // We have to clear internal state before calling Inspector's code.
        PropertySetCSSStyleDeclaration* localCopyStyleDecl = s_currentDecl;
        s_currentDecl = nullptr;
        s_shouldNotifyInspector = false;

        if (auto* parentElement = localCopyStyleDecl->parentElement())
            InspectorInstrumentation::didInvalidateStyleAttr(*parentElement);
    }

    void enqueueMutationRecord()
    {
        s_shouldDeliver = true;
    }

    void didInvalidateStyleAttr()
    {
        s_shouldNotifyInspector = true;
    }

private:
    static unsigned s_scopeCount;
    static PropertySetCSSStyleDeclaration* s_currentDecl;
    static bool s_shouldNotifyInspector;
    static bool s_shouldDeliver;

    std::unique_ptr<MutationObserverInterestGroup> m_mutationRecipients;
    AtomString m_oldValue;
    RefPtr<Element> m_customElement;
};

unsigned StyleAttributeMutationScope::s_scopeCount = 0;
PropertySetCSSStyleDeclaration* StyleAttributeMutationScope::s_currentDecl = nullptr;
bool StyleAttributeMutationScope::s_shouldNotifyInspector = false;
bool StyleAttributeMutationScope::s_shouldDeliver = false;

void PropertySetCSSStyleDeclaration::ref()
{ 
    m_propertySet->ref();
}

void PropertySetCSSStyleDeclaration::deref()
{
    m_propertySet->deref(); 
}

unsigned PropertySetCSSStyleDeclaration::length() const
{
    unsigned exposed = 0;
    for (auto property : *m_propertySet) {
        if (isExposed(property.id()))
            exposed++;
    }
    return exposed;
}

String PropertySetCSSStyleDeclaration::item(unsigned i) const
{
    for (unsigned j = 0; j <= i && j < m_propertySet->propertyCount(); j++) {
        if (!isExposed(m_propertySet->propertyAt(j).id()))
            i++;
    }

    if (i >= m_propertySet->propertyCount())
        return String();

    return m_propertySet->propertyAt(i).cssName();
}

String PropertySetCSSStyleDeclaration::cssText() const
{
    return m_propertySet->asText();
}

ExceptionOr<void> PropertySetCSSStyleDeclaration::setCssText(const String& text)
{
    StyleAttributeMutationScope mutationScope(this);
    if (!willMutate())
        return { };

    bool changed = m_propertySet->parseDeclaration(text, cssParserContext());

    didMutate(changed ? PropertyChanged : NoChanges);

    mutationScope.enqueueMutationRecord();
    return { };
}

RefPtr<DeprecatedCSSOMValue> PropertySetCSSStyleDeclaration::getPropertyCSSValue(const String& propertyName)
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
    return wrapForDeprecatedCSSOM(getPropertyCSSValueInternal(propertyID).get());
}

String PropertySetCSSStyleDeclaration::getPropertyValue(const String& propertyName)
{
    if (isCustomPropertyName(propertyName))
        return m_propertySet->getCustomPropertyValue(propertyName);

    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return String();
    return getPropertyValueInternal(propertyID);
}

String PropertySetCSSStyleDeclaration::getPropertyPriority(const String& propertyName)
{
    if (isCustomPropertyName(propertyName))
        return m_propertySet->customPropertyIsImportant(propertyName) ? "important"_s : emptyString();

    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return emptyString();
    return m_propertySet->propertyIsImportant(propertyID) ? "important"_s : emptyString();
}

String PropertySetCSSStyleDeclaration::getPropertyShorthand(const String& propertyName)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (!isExposed(propertyID))
        return String();
    return m_propertySet->getPropertyShorthand(propertyID);
}

bool PropertySetCSSStyleDeclaration::isPropertyImplicit(const String& propertyName)
{
    return m_propertySet->isPropertyImplicit(cssPropertyID(propertyName));
}

ExceptionOr<void> PropertySetCSSStyleDeclaration::setProperty(const String& propertyName, const String& value, const String& priority)
{
    StyleAttributeMutationScope mutationScope(this);

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
    if (UNLIKELY(propertyID == CSSPropertyCustom))
        changed = m_propertySet->setCustomProperty(propertyName, value, important, cssParserContext());
    else
        changed = m_propertySet->setProperty(propertyID, value, important, cssParserContext());

    didMutate(changed ? PropertyChanged : NoChanges);

    if (changed) {
        // CSS DOM requires raising SyntaxError of parsing failed, but this is too dangerous for compatibility,
        // see <http://bugs.webkit.org/show_bug.cgi?id=7296>.
        mutationScope.enqueueMutationRecord();
    }

    return { };
}

ExceptionOr<String> PropertySetCSSStyleDeclaration::removeProperty(const String& propertyName)
{
    StyleAttributeMutationScope mutationScope(this);
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (isCustomPropertyName(propertyName))
        propertyID = CSSPropertyCustom;
    if (!isExposed(propertyID))
        return String();

    if (!willMutate())
        return String();

    String result;
    bool changed = propertyID != CSSPropertyCustom ? m_propertySet->removeProperty(propertyID, &result) : m_propertySet->removeCustomProperty(propertyName, &result);

    didMutate(changed ? PropertyChanged : NoChanges);

    if (changed)
        mutationScope.enqueueMutationRecord();
    return result;
}

RefPtr<CSSValue> PropertySetCSSStyleDeclaration::getPropertyCSSValueInternal(CSSPropertyID propertyID)
{
    return m_propertySet->getPropertyCSSValue(propertyID);
}

String PropertySetCSSStyleDeclaration::getPropertyValueInternal(CSSPropertyID propertyID)
{
    if (!isExposed(propertyID))
        return { };

    auto value = m_propertySet->getPropertyValue(propertyID);

    if (!value.isEmpty())
        return value;

    return { };
}

ExceptionOr<void> PropertySetCSSStyleDeclaration::setPropertyInternal(CSSPropertyID propertyID, const String& value, bool important)
{ 
    StyleAttributeMutationScope mutationScope { this };
    if (!willMutate())
        return { };

    if (!isExposed(propertyID))
        return { };

    if (m_propertySet->setProperty(propertyID, value, important, cssParserContext())) {
        didMutate(PropertyChanged);
        mutationScope.enqueueMutationRecord();
    } else
        didMutate(NoChanges);
        
    return { };
}

bool PropertySetCSSStyleDeclaration::isExposed(CSSPropertyID propertyID) const
{
    if (propertyID == CSSPropertyInvalid)
        return false;

    auto parserContext = cssParserContext();
    bool parsingDescriptor = parserContext.enclosingRuleType && *parserContext.enclosingRuleType != StyleRuleType::Style;

    return WebCore::isExposed(propertyID, &parserContext.propertySettings)
        && (!CSSProperty::isDescriptorOnly(propertyID) || parsingDescriptor);
}

RefPtr<DeprecatedCSSOMValue> PropertySetCSSStyleDeclaration::wrapForDeprecatedCSSOM(CSSValue* internalValue)
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

StyleSheetContents* PropertySetCSSStyleDeclaration::contextStyleSheet() const
{ 
    CSSStyleSheet* cssStyleSheet = parentStyleSheet();
    return cssStyleSheet ? &cssStyleSheet->contents() : nullptr;
}

CSSParserContext PropertySetCSSStyleDeclaration::cssParserContext() const
{
    return CSSParserContext(m_propertySet->cssParserMode());
}

Ref<MutableStyleProperties> PropertySetCSSStyleDeclaration::copyProperties() const
{
    return m_propertySet->mutableCopy();
}
    
StyleRuleCSSStyleDeclaration::StyleRuleCSSStyleDeclaration(MutableStyleProperties& propertySet, CSSRule& parentRule)
    : PropertySetCSSStyleDeclaration(propertySet)
    , m_refCount(1)
    , m_parentRuleType(parentRule.styleRuleType())
    , m_parentRule(&parentRule)
{
    m_propertySet->ref();
}

StyleRuleCSSStyleDeclaration::~StyleRuleCSSStyleDeclaration()
{
    m_propertySet->deref();
}

void StyleRuleCSSStyleDeclaration::ref()
{ 
    ++m_refCount;
}

void StyleRuleCSSStyleDeclaration::deref()
{ 
    ASSERT(m_refCount);
    if (!--m_refCount)
        delete this;
}

bool StyleRuleCSSStyleDeclaration::willMutate()
{
    if (!m_parentRule || !m_parentRule->parentStyleSheet())
        return false;
    m_parentRule->parentStyleSheet()->willMutateRules();
    return true;
}

void StyleRuleCSSStyleDeclaration::didMutate(MutationType type)
{
    ASSERT(m_parentRule);
    ASSERT(m_parentRule->parentStyleSheet());

    if (type == PropertyChanged)
        m_cssomValueWrappers.clear();

    // Style sheet mutation needs to be signaled even if the change failed. willMutate*/didMutate* must pair.
    m_parentRule->parentStyleSheet()->didMutateRuleFromCSSStyleDeclaration();
}

CSSStyleSheet* StyleRuleCSSStyleDeclaration::parentStyleSheet() const
{
    return m_parentRule ? m_parentRule->parentStyleSheet() : nullptr;
}

CSSParserContext StyleRuleCSSStyleDeclaration::cssParserContext() const
{
    auto* styleSheet = contextStyleSheet();
    if (!styleSheet)
        return PropertySetCSSStyleDeclaration::cssParserContext();

    auto context = styleSheet->parserContext();
    context.enclosingRuleType = m_parentRuleType;
    
    return context;
}

void StyleRuleCSSStyleDeclaration::reattach(MutableStyleProperties& propertySet)
{
    m_propertySet->deref();
    m_propertySet = &propertySet;
    m_propertySet->ref();
}

bool InlineCSSStyleDeclaration::willMutate()
{
    if (m_parentElement)
        InspectorInstrumentation::willInvalidateStyleAttr(*m_parentElement);
    return true;
}

void InlineCSSStyleDeclaration::didMutate(MutationType type)
{
    if (type == NoChanges)
        return;

    m_cssomValueWrappers.clear();

    if (!m_parentElement)
        return;

    m_parentElement->invalidateStyleAttribute();
    StyleAttributeMutationScope(this).didInvalidateStyleAttr();
}

CSSStyleSheet* InlineCSSStyleDeclaration::parentStyleSheet() const
{
    return nullptr;
}

CSSParserContext InlineCSSStyleDeclaration::cssParserContext() const
{
    if (!m_parentElement)
        return PropertySetCSSStyleDeclaration::cssParserContext();

    CSSParserContext context(m_parentElement->document());
    context.mode = m_propertySet->cssParserMode();
    return context;
}

} // namespace WebCore
