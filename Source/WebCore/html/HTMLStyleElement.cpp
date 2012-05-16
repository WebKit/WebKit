/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2010 Apple Inc. All rights reserved.
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

#include "Attribute.h"
#include "Document.h"
#include "Event.h"
#include "EventSender.h"
#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptEventListener.h"
#include "ScriptableDocumentParser.h"


namespace WebCore {

using namespace HTMLNames;

static StyleEventSender& styleLoadEventSender()
{
    DEFINE_STATIC_LOCAL(StyleEventSender, sharedLoadEventSender, (eventNames().loadEvent));
    return sharedLoadEventSender;
}

inline HTMLStyleElement::HTMLStyleElement(const QualifiedName& tagName, Document* document, bool createdByParser)
    : HTMLElement(tagName, document)
    , StyleElement(document, createdByParser)
    , m_firedLoad(false)
    , m_loadedSheet(false)
#if ENABLE(STYLE_SCOPED)
    , m_isRegisteredWithScopingNode(false)
#endif
{
    ASSERT(hasTagName(styleTag));
}

HTMLStyleElement::~HTMLStyleElement()
{
    // During tear-down, willRemove isn't called, so m_isRegisteredWithScopingNode may still be set here.
    // Therefore we can't ASSERT(!m_isRegisteredWithScopingNode).
    StyleElement::clearDocumentData(document(), this);

    styleLoadEventSender().cancelEvent(this);
}

PassRefPtr<HTMLStyleElement> HTMLStyleElement::create(const QualifiedName& tagName, Document* document, bool createdByParser)
{
    return adoptRef(new HTMLStyleElement(tagName, document, createdByParser));
}

void HTMLStyleElement::parseAttribute(const Attribute& attribute)
{
    if (attribute.name() == titleAttr && m_sheet)
        m_sheet->setTitle(attribute.value());
    else if (attribute.name() == onloadAttr)
        setAttributeEventListener(eventNames().loadEvent, createAttributeEventListener(this, attribute));
    else if (attribute.name() == onerrorAttr)
        setAttributeEventListener(eventNames().errorEvent, createAttributeEventListener(this, attribute));
#if ENABLE(STYLE_SCOPED)
    else if (attribute.name() == scopedAttr) {
        if (!attribute.isNull() && !m_isRegisteredWithScopingNode && inDocument())
            registerWithScopingNode();
        else if (attribute.isNull() && m_isRegisteredWithScopingNode)
            unregisterWithScopingNode();
    }
#endif
    else
        HTMLElement::parseAttribute(attribute);
}

void HTMLStyleElement::finishParsingChildren()
{
    StyleElement::finishParsingChildren(this);
    HTMLElement::finishParsingChildren();
}

#if ENABLE(STYLE_SCOPED)
void HTMLStyleElement::registerWithScopingNode()
{
    // Note: We cannot rely on the 'scoped' element already being present when this method is invoked.
    // Therefore we cannot rely on scoped()!
    ASSERT(!m_isRegisteredWithScopingNode);
    ASSERT(inDocument());
    if (m_isRegisteredWithScopingNode)
        return;
    if (!RuntimeEnabledFeatures::styleScopedEnabled())
        return;

    ContainerNode* scope = parentNode();
    if (!scope)
        return;
    if (!scope->isElementNode() && !scope->isShadowRoot()) {
        // DocumentFragment nodes should never be inDocument,
        // <style> should not be a child of Document, PI or some such.
        ASSERT_NOT_REACHED();
        return;
    }

    scope->registerScopedHTMLStyleChild();
    scope->setNeedsStyleRecalc();
    if (inDocument() && !document()->parsing() && document()->renderer())
        document()->styleResolverChanged(DeferRecalcStyle);

    m_isRegisteredWithScopingNode = true;
}

void HTMLStyleElement::unregisterWithScopingNode()
{
    // Note: We cannot rely on the 'scoped' element still being present when this method is invoked.
    // Therefore we cannot rely on scoped()!
    ASSERT(m_isRegisteredWithScopingNode || !RuntimeEnabledFeatures::styleScopedEnabled());
    if (!m_isRegisteredWithScopingNode)
        return;
    if (!RuntimeEnabledFeatures::styleScopedEnabled())
        return;

    ContainerNode* scope = parentNode();
    ASSERT(scope);
    if (scope) {
        ASSERT(scope->hasScopedHTMLStyleChild());
        scope->unregisterScopedHTMLStyleChild();
        scope->setNeedsStyleRecalc();
    }
    if (inDocument() && !document()->parsing() && document()->renderer())
        document()->styleResolverChanged(DeferRecalcStyle);

    m_isRegisteredWithScopingNode = false;
}
#endif

Node::InsertionNotificationRequest HTMLStyleElement::insertedInto(Node* insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);
    if (insertionPoint->inDocument())
        StyleElement::insertedIntoDocument(document(), this);
#if ENABLE(STYLE_SCOPED)
    if (scoped() && !m_isRegisteredWithScopingNode)
        registerWithScopingNode();
#endif
    return InsertionDone;
}

void HTMLStyleElement::removedFrom(Node* insertionPoint)
{
    HTMLElement::removedFrom(insertionPoint);

#if ENABLE(STYLE_SCOPED)
    // In the current implementation, <style scoped> is only registered if the node is in the document.
    // That is, because willRemove() is also called if an ancestor is removed from the document.
    // Now, if we want to register <style scoped> even if it's not inDocument,
    // we'd need to find a way to discern whether that is the case, or whether <style scoped> itself is about to be removed.
    if (m_isRegisteredWithScopingNode)
        unregisterWithScopingNode();
#endif

    if (insertionPoint->inDocument())
        StyleElement::removedFromDocument(document(), this);
}

void HTMLStyleElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    HTMLElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    StyleElement::childrenChanged(this);
}

const AtomicString& HTMLStyleElement::media() const
{
    return getAttribute(mediaAttr);
}

const AtomicString& HTMLStyleElement::type() const
{
    return getAttribute(typeAttr);
}

#if ENABLE(STYLE_SCOPED)
bool HTMLStyleElement::scoped() const
{
    return fastHasAttribute(scopedAttr);
}

void HTMLStyleElement::setScoped(bool scopedValue)
{
    setBooleanAttribute(scopedAttr, scopedValue);
}

Element* HTMLStyleElement::scopingElement() const
{
    if (!scoped())
        return 0;

    // FIXME: This probably needs to be refined for scoped stylesheets within shadow DOM.
    // As written, such a stylesheet could style the host element, as well as children of the host.
    // OTOH, this paves the way for a :bound-element implementation.
    ContainerNode* parentOrHost = parentOrHostNode();
    if (!parentOrHost || !parentOrHost->isElementNode())
        return 0;

    return toElement(parentOrHost);
}
#endif // ENABLE(STYLE_SCOPED)

void HTMLStyleElement::dispatchPendingLoadEvents()
{
    styleLoadEventSender().dispatchPendingEvents();
}

void HTMLStyleElement::dispatchPendingEvent(StyleEventSender* eventSender)
{
    ASSERT_UNUSED(eventSender, eventSender == &styleLoadEventSender());
    if (m_loadedSheet)
        dispatchEvent(Event::create(eventNames().loadEvent, false, false));
    else
        dispatchEvent(Event::create(eventNames().errorEvent, false, false));
}

void HTMLStyleElement::notifyLoadedSheetAndAllCriticalSubresources(bool errorOccurred)
{
    if (m_firedLoad)
        return;
    m_loadedSheet = !errorOccurred;
    styleLoadEventSender().dispatchEventSoon(this);
    m_firedLoad = true;
}

void HTMLStyleElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{    
    HTMLElement::addSubresourceAttributeURLs(urls);

    if (CSSStyleSheet* styleSheet = const_cast<HTMLStyleElement*>(this)->sheet())
        styleSheet->internal()->addSubresourceStyleURLs(urls);
}

bool HTMLStyleElement::disabled() const
{
    if (!m_sheet)
        return false;

    return m_sheet->disabled();
}

void HTMLStyleElement::setDisabled(bool setDisabled)
{
    if (CSSStyleSheet* styleSheet = sheet())
        styleSheet->setDisabled(setDisabled);
}

}
