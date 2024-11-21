/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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
#include "HTMLScriptElement.h"

#include "Document.h"
#include "ElementInlines.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "JSRequestPriority.h"
#include "NodeName.h"
#include "RequestPriority.h"
#include "Settings.h"
#include "Text.h"
#include "TrustedType.h"
#include <wtf/Ref.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(HTMLScriptElement);

using namespace HTMLNames;

inline HTMLScriptElement::HTMLScriptElement(const QualifiedName& tagName, Document& document, bool wasInsertedByParser, bool alreadyStarted)
    : HTMLElement(tagName, document)
    , ScriptElement(*this, wasInsertedByParser, alreadyStarted)
{
    ASSERT(hasTagName(scriptTag));
}

Ref<HTMLScriptElement> HTMLScriptElement::create(const QualifiedName& tagName, Document& document, bool wasInsertedByParser, bool alreadyStarted)
{
    return adoptRef(*new HTMLScriptElement(tagName, document, wasInsertedByParser, alreadyStarted));
}

bool HTMLScriptElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr || HTMLElement::isURLAttribute(attribute);
}

void HTMLScriptElement::childrenChanged(const ChildChange& change)
{
    HTMLElement::childrenChanged(change);
    ScriptElement::childrenChanged(change);
}

void HTMLScriptElement::finishParsingChildren()
{
    HTMLElement::finishParsingChildren();
    ScriptElement::finishParsingChildren();
}

void HTMLScriptElement::removedFromAncestor(RemovalType type, ContainerNode& container)
{
    HTMLElement::removedFromAncestor(type, container);
    unblockRendering();
}

void HTMLScriptElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == srcAttr)
        handleSourceAttribute(newValue);
    else if (name == asyncAttr)
        handleAsyncAttribute();
    else if (name == blockingAttr) {
        blocking().associatedAttributeValueChanged();
        if (!blocking().contains("render"_s))
            unblockRendering();
    } else
        HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

Node::InsertedIntoAncestorResult HTMLScriptElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    return ScriptElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
}

void HTMLScriptElement::didFinishInsertingNode()
{
    ScriptElement::didFinishInsertingNode();
}

void HTMLScriptElement::setText(String&& value)
{
    setTextContent(WTFMove(value));
}

DOMTokenList& HTMLScriptElement::blocking()
{
    if (!m_blockingList) {
        m_blockingList = makeUniqueWithoutRefCountedCheck<DOMTokenList>(*this, HTMLNames::blockingAttr, [](Document&, StringView token) {
            if (equalLettersIgnoringASCIICase(token, "render"_s))
                return true;
            return false;
        });
    }
    return *m_blockingList;
}

// https://html.spec.whatwg.org/multipage/scripting.html#script-processing-model:implicitly-potentially-render-blocking
bool HTMLScriptElement::isImplicitlyPotentiallyRenderBlocking() const
{
    return scriptType() == ScriptType::Classic && isParserInserted() == ParserInserted::Yes && !hasDeferAttribute() && !hasAsyncAttribute();
}

// https://html.spec.whatwg.org/multipage/urls-and-fetching.html#potentially-render-blocking
void HTMLScriptElement::potentiallyBlockRendering()
{
    bool explicitRenderBlocking = m_blockingList && m_blockingList->contains("render"_s);
    if (explicitRenderBlocking || isImplicitlyPotentiallyRenderBlocking()) {
        document().blockRenderingOn(*this, explicitRenderBlocking ? Document::ImplicitRenderBlocking::No : Document::ImplicitRenderBlocking::Yes);
        m_isRenderBlocking = true;
    }
}

void HTMLScriptElement::unblockRendering()
{
    if (m_isRenderBlocking) {
        document().unblockRenderingOn(*this);
        m_isRenderBlocking = false;
    }
}

// https://html.spec.whatwg.org/multipage/scripting.html#dom-script-text
ExceptionOr<void> HTMLScriptElement::setText(std::variant<RefPtr<TrustedScript>, String>&& value)
{
    return setTextContent(trustedTypeCompliantString(*scriptExecutionContext(), WTFMove(value), "HTMLScriptElement text"_s));
}

ExceptionOr<void> HTMLScriptElement::setTextContent(std::optional<std::variant<RefPtr<TrustedScript>, String>>&& value)
{
    return setTextContent(trustedTypeCompliantString(*scriptExecutionContext(), value ? WTFMove(*value) : emptyString(), "HTMLScriptElement textContent"_s));
}

ExceptionOr<void> HTMLScriptElement::setTextContent(ExceptionOr<String> value)
{
    if (value.hasException())
        return value.releaseException();

    auto newValue = value.releaseReturnValue();

    setTrustedScriptText(newValue);
    setTextContent(WTFMove(newValue));
    return { };
}

ExceptionOr<void> HTMLScriptElement::setInnerText(std::variant<RefPtr<TrustedScript>, String>&& value)
{
    auto stringValueHolder = trustedTypeCompliantString(*scriptExecutionContext(), WTFMove(value), "HTMLScriptElement innerText"_s);
    if (stringValueHolder.hasException())
        return stringValueHolder.releaseException();

    auto newValue = stringValueHolder.releaseReturnValue();

    setTrustedScriptText(newValue);
    setInnerText(WTFMove(newValue));
    return { };
}

void HTMLScriptElement::setAsync(bool async)
{
    setBooleanAttribute(asyncAttr, async);
    handleAsyncAttribute();
}

bool HTMLScriptElement::async() const
{
    return hasAttributeWithoutSynchronization(asyncAttr) || forceAsync();
}

void HTMLScriptElement::setCrossOrigin(const AtomString& value)
{
    setAttributeWithoutSynchronization(crossoriginAttr, value);
}

String HTMLScriptElement::crossOrigin() const
{
    return parseCORSSettingsAttribute(attributeWithoutSynchronization(crossoriginAttr));
}

String HTMLScriptElement::src() const
{
    return getURLAttributeForBindings(WebCore::HTMLNames::srcAttr).string();
}

ExceptionOr<void> HTMLScriptElement::setSrc(std::variant<RefPtr<TrustedScriptURL>, String>&& value)
{
    auto stringValueHolder = trustedTypeCompliantString(*scriptExecutionContext(), WTFMove(value), "HTMLScriptElement src"_s);
    if (stringValueHolder.hasException())
        return stringValueHolder.releaseException();

    setAttributeWithoutSynchronization(HTMLNames::srcAttr, AtomString { stringValueHolder.releaseReturnValue() });
    return { };
}

void HTMLScriptElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    HTMLElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(sourceAttributeValue()));
}

String HTMLScriptElement::sourceAttributeValue() const
{
    return attributeWithoutSynchronization(srcAttr).string();
}

AtomString HTMLScriptElement::charsetAttributeValue() const
{
    return attributeWithoutSynchronization(charsetAttr);
}

String HTMLScriptElement::typeAttributeValue() const
{
    return attributeWithoutSynchronization(typeAttr).string();
}

String HTMLScriptElement::languageAttributeValue() const
{
    return attributeWithoutSynchronization(languageAttr).string();
}

bool HTMLScriptElement::hasAsyncAttribute() const
{
    return hasAttributeWithoutSynchronization(asyncAttr);
}

bool HTMLScriptElement::hasDeferAttribute() const
{
    return hasAttributeWithoutSynchronization(deferAttr);
}

bool HTMLScriptElement::hasNoModuleAttribute() const
{
    return hasAttributeWithoutSynchronization(nomoduleAttr);
}

bool HTMLScriptElement::hasSourceAttribute() const
{
    return hasAttributeWithoutSynchronization(srcAttr);
}

void HTMLScriptElement::dispatchLoadEvent()
{
    ASSERT(!haveFiredLoadEvent());
    setHaveFiredLoadEvent(true);

    dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

bool HTMLScriptElement::isScriptPreventedByAttributes() const
{
    auto& eventAttribute = attributeWithoutSynchronization(eventAttr);
    auto& forAttribute = attributeWithoutSynchronization(forAttr);
    if (!eventAttribute.isNull() && !forAttribute.isNull()) {
        if (!equalLettersIgnoringASCIICase(StringView(forAttribute).trim(isASCIIWhitespace<UChar>), "window"_s))
            return true;

        auto eventAttributeView = StringView(eventAttribute).trim(isASCIIWhitespace<UChar>);
        if (!equalLettersIgnoringASCIICase(eventAttributeView, "onload"_s) && !equalLettersIgnoringASCIICase(eventAttributeView, "onload()"_s))
            return true;
    }
    return false;
}

Ref<Element> HTMLScriptElement::cloneElementWithoutAttributesAndChildren(TreeScope& treeScope)
{
    return adoptRef(*new HTMLScriptElement(tagQName(), treeScope.documentScope(), false, alreadyStarted()));
}

void HTMLScriptElement::setReferrerPolicyForBindings(const AtomString& value)
{
    setAttributeWithoutSynchronization(referrerpolicyAttr, value);
}

String HTMLScriptElement::referrerPolicyForBindings() const
{
    return referrerPolicyToString(referrerPolicy());
}

ReferrerPolicy HTMLScriptElement::referrerPolicy() const
{
    return parseReferrerPolicy(attributeWithoutSynchronization(referrerpolicyAttr), ReferrerPolicySource::ReferrerPolicyAttribute).value_or(ReferrerPolicy::EmptyString);
}

void HTMLScriptElement::setFetchPriorityForBindings(const AtomString& value)
{
    setAttributeWithoutSynchronization(fetchpriorityAttr, value);
}

String HTMLScriptElement::fetchPriorityForBindings() const
{
    return convertEnumerationToString(fetchPriorityHint());
}

RequestPriority HTMLScriptElement::fetchPriorityHint() const
{
    if (document().settings().fetchPriorityEnabled())
        return parseEnumerationFromString<RequestPriority>(attributeWithoutSynchronization(fetchpriorityAttr)).value_or(RequestPriority::Auto);
    return RequestPriority::Auto;
}

}
