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

#include "CachedResource.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "EventSender.h"
#include "HTMLNames.h"
#include "MediaList.h"
#include "MediaQueryParser.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptableDocumentParser.h"
#include "ShadowRoot.h"
#include "StyleScope.h"
#include "StyleSheetContents.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLStyleElement);

using namespace HTMLNames;

static StyleEventSender& styleLoadEventSender()
{
    static NeverDestroyed<StyleEventSender> sharedLoadEventSender(eventNames().loadEvent);
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

void HTMLStyleElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == titleAttr && sheet() && !isInShadowTree())
        sheet()->setTitle(value);
    else if (name == mediaAttr) {
        m_styleSheetOwner.setMedia(value);
        if (sheet()) {
            sheet()->setMediaQueries(MediaQuerySet::create(value, MediaQueryParserContext(document())));
            if (auto* scope = m_styleSheetOwner.styleScope())
                scope->didChangeStyleSheetContents();
        } else
            m_styleSheetOwner.childrenChanged(*this);
    } else if (name == typeAttr)
        m_styleSheetOwner.setContentType(value);
    else
        HTMLElement::parseAttribute(name, value);
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

void HTMLStyleElement::dispatchPendingLoadEvents()
{
    styleLoadEventSender().dispatchPendingEvents();
}

void HTMLStyleElement::dispatchPendingEvent(StyleEventSender* eventSender)
{
    ASSERT_UNUSED(eventSender, eventSender == &styleLoadEventSender());
    if (m_loadedSheet)
        dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No));
    else
        dispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void HTMLStyleElement::notifyLoadedSheetAndAllCriticalSubresources(bool errorOccurred)
{
    if (m_firedLoad)
        return;
    m_loadedSheet = !errorOccurred;
    styleLoadEventSender().dispatchEventSoon(*this);
    m_firedLoad = true;
}

void HTMLStyleElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{    
    HTMLElement::addSubresourceAttributeURLs(urls);

    if (auto styleSheet = makeRefPtr(this->sheet())) {
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
