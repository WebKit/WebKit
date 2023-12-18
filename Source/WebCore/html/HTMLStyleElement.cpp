/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2010, 2013 Apple Inc. All rights reserved.
 *           (C) 2007 Rob Buis (buis@kde.org)
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
#include "HTMLStyleElement.h"

#include "CSSRule.h"
#include "CachedResource.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "EventSender.h"
#include "HTMLNames.h"
#include "MediaQueryParser.h"
#include "MediaQueryParserContext.h"
#include "NodeName.h"
#include "Page.h"
#include "ScriptableDocumentParser.h"
#include "ShadowRoot.h"
#include "StyleScope.h"
#include "StyleSheetContents.h"
#include "TextNodeTraversal.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLStyleElement);

using namespace HTMLNames;

static StyleEventSender& styleLoadEventSender()
{
    static NeverDestroyed<StyleEventSender> sharedLoadEventSender;
    return sharedLoadEventSender;
}

inline HTMLStyleElement::HTMLStyleElement(const QualifiedName& tagName, Document& document, bool createdByParser)
    : HTMLElement(tagName, document)
    , m_styleSheetOwner(document, createdByParser)
{
    ASSERT(hasTagName(styleTag));
}

HTMLStyleElement::~HTMLStyleElement()
{
    m_styleSheetOwner.clearDocumentData(*this);

    styleLoadEventSender().cancelEvent(*this);
}

Ref<HTMLStyleElement> HTMLStyleElement::create(const QualifiedName& tagName, Document& document, bool createdByParser)
{
    return adoptRef(*new HTMLStyleElement(tagName, document, createdByParser));
}

Ref<HTMLStyleElement> HTMLStyleElement::create(Document& document)
{
    return adoptRef(*new HTMLStyleElement(styleTag, document, false));
}

void HTMLStyleElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::titleAttr:
        if (sheet() && !isInShadowTree())
            sheet()->setTitle(newValue);
        break;
    case AttributeNames::mediaAttr:
        m_styleSheetOwner.setMedia(newValue);
        if (sheet()) {
            sheet()->setMediaQueries(MQ::MediaQueryParser::parse(newValue, MediaQueryParserContext(document())));
            if (auto* scope = m_styleSheetOwner.styleScope())
                scope->didChangeStyleSheetContents();
        } else
            m_styleSheetOwner.childrenChanged(*this);
        break;
    case AttributeNames::typeAttr:
        m_styleSheetOwner.setContentType(newValue);
        m_styleSheetOwner.childrenChanged(*this);
        if (auto* scope = m_styleSheetOwner.styleScope())
            scope->didChangeStyleSheetContents();
        break;
    default:
        HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
        break;
    }
}

void HTMLStyleElement::finishParsingChildren()
{
    m_styleSheetOwner.finishParsingChildren(*this);
    HTMLElement::finishParsingChildren();
}

Node::InsertedIntoAncestorResult HTMLStyleElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    auto result = HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.connectedToDocument)
        m_styleSheetOwner.insertedIntoDocument(*this);
    return result;
}

void HTMLStyleElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    if (removalType.disconnectedFromDocument)
        m_styleSheetOwner.removedFromDocument(*this);
}

void HTMLStyleElement::childrenChanged(const ChildChange& change)
{
    HTMLElement::childrenChanged(change);
    m_styleSheetOwner.childrenChanged(*this);
}

void HTMLStyleElement::dispatchPendingLoadEvents(Page* page)
{
    styleLoadEventSender().dispatchPendingEvents(page);
}

void HTMLStyleElement::dispatchPendingEvent(StyleEventSender* eventSender, const AtomString& eventType)
{
    ASSERT_UNUSED(eventSender, eventSender == &styleLoadEventSender());
    dispatchEvent(Event::create(eventType, Event::CanBubble::No, Event::IsCancelable::No));
}

void HTMLStyleElement::notifyLoadedSheetAndAllCriticalSubresources(bool errorOccurred)
{
    m_loadedSheet = !errorOccurred;
    styleLoadEventSender().dispatchEventSoon(*this, m_loadedSheet ? eventNames().loadEvent : eventNames().errorEvent);
}

void HTMLStyleElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    HTMLElement::addSubresourceAttributeURLs(urls);

    if (RefPtr styleSheet = this->sheet()) {
        styleSheet->contents().traverseSubresources([&] (auto& resource) {
            urls.add(resource.url());
            return false;
        });
    }
}

bool HTMLStyleElement::disabled() const
{
    if (!sheet())
        return false;

    return sheet()->disabled();
}

void HTMLStyleElement::setDisabled(bool setDisabled)
{
    if (CSSStyleSheet* styleSheet = sheet())
        styleSheet->setDisabled(setDisabled);
}

}
