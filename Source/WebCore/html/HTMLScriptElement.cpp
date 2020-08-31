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
#include "Event.h"
#include "EventNames.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "Settings.h"
#include "Text.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Ref.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLScriptElement);

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

void HTMLScriptElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == srcAttr)
        handleSourceAttribute(value);
    else if (name == asyncAttr)
        handleAsyncAttribute();
    else
        HTMLElement::parseAttribute(name, value);
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

// https://html.spec.whatwg.org/multipage/scripting.html#dom-script-text
void HTMLScriptElement::setText(const String& value)
{
    setTextContent(value);
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

URL HTMLScriptElement::src() const
{
    return document().completeURL(sourceAttributeValue());
}

void HTMLScriptElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    HTMLElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, src());
}

String HTMLScriptElement::sourceAttributeValue() const
{
    return attributeWithoutSynchronization(srcAttr).string();
}

String HTMLScriptElement::charsetAttributeValue() const
{
    return attributeWithoutSynchronization(charsetAttr).string();
}

String HTMLScriptElement::typeAttributeValue() const
{
    return attributeWithoutSynchronization(typeAttr).string();
}

String HTMLScriptElement::languageAttributeValue() const
{
    return attributeWithoutSynchronization(languageAttr).string();
}

String HTMLScriptElement::forAttributeValue() const
{
    return attributeWithoutSynchronization(forAttr).string();
}

String HTMLScriptElement::eventAttributeValue() const
{
    return attributeWithoutSynchronization(eventAttr).string();
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

Ref<Element> HTMLScriptElement::cloneElementWithoutAttributesAndChildren(Document& targetDocument)
{
    return adoptRef(*new HTMLScriptElement(tagQName(), targetDocument, false, alreadyStarted()));
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
    if (document().settings().referrerPolicyAttributeEnabled())
        return parseReferrerPolicy(attributeWithoutSynchronization(referrerpolicyAttr), ReferrerPolicySource::ReferrerPolicyAttribute).valueOr(ReferrerPolicy::EmptyString);
    return ReferrerPolicy::EmptyString;
}

}
