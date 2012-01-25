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
#include "HTMLNames.h"
#include "ScriptEventListener.h"
#include "ScriptableDocumentParser.h"


namespace WebCore {

using namespace HTMLNames;

inline HTMLStyleElement::HTMLStyleElement(const QualifiedName& tagName, Document* document, bool createdByParser)
    : HTMLElement(tagName, document)
    , StyleElement(document, createdByParser)
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
}

PassRefPtr<HTMLStyleElement> HTMLStyleElement::create(const QualifiedName& tagName, Document* document, bool createdByParser)
{
    return adoptRef(new HTMLStyleElement(tagName, document, createdByParser));
}

void HTMLStyleElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == titleAttr && m_sheet)
        m_sheet->setTitle(attr->value());
#if ENABLE(STYLE_SCOPED)
    else if (attr->name() == scopedAttr) {
        if (!attr->isNull() && !m_isRegisteredWithScopingNode && inDocument())
            registerWithScopingNode();
        else if (attr->isNull() && m_isRegisteredWithScopingNode)
            unregisterWithScopingNode();

    }
#endif
    else
        HTMLElement::parseMappedAttribute(attr);
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
    if (!m_isRegisteredWithScopingNode) {
        Element* scope = parentElement();
        if (!scope)
            return;

        scope->registerScopedHTMLStyleChild();
        scope->setNeedsStyleRecalc();
        if (inDocument() && !document()->parsing() && document()->renderer())
            document()->styleSelectorChanged(DeferRecalcStyle);

        m_isRegisteredWithScopingNode = true;
    }
}

void HTMLStyleElement::unregisterWithScopingNode()
{
    // Note: We cannot rely on the 'scoped' element still being present when this method is invoked.
    // Therefore we cannot rely on scoped()!
    ASSERT(m_isRegisteredWithScopingNode);
    if (m_isRegisteredWithScopingNode) {
        Element* scope = parentElement();
        ASSERT(scope);
        if (scope) {
            ASSERT(scope->hasScopedHTMLStyleChild());
            scope->unregisterScopedHTMLStyleChild();
            scope->setNeedsStyleRecalc();
        }
        if (inDocument() && !document()->parsing() && document()->renderer())
            document()->styleSelectorChanged(DeferRecalcStyle);

        m_isRegisteredWithScopingNode = false;
    }
}
#endif

void HTMLStyleElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();
    StyleElement::insertedIntoDocument(document(), this);
#if ENABLE(STYLE_SCOPED)
    if (scoped() && !m_isRegisteredWithScopingNode)
        registerWithScopingNode();
#endif
}

void HTMLStyleElement::removedFromDocument()
{
#if ENABLE(STYLE_SCOPED)
    ASSERT(!m_isRegisteredWithScopingNode);
#endif
    HTMLElement::removedFromDocument();
    StyleElement::removedFromDocument(document(), this);
}


#if ENABLE(STYLE_SCOPED)
void HTMLStyleElement::willRemove()
{
    // In the current implementation, <style scoped> is only registered if the node is in the document.
    // That is, because willRemove() is also called if an ancestor is removed from the document.
    // Now, if we want to register <style scoped> even if it's not inDocument,
    // we'd need to find a way to discern whether that is the case, or whether <style scoped> itself is about to be removed.
    ASSERT(!scoped() || !inDocument() || m_isRegisteredWithScopingNode);
    if (m_isRegisteredWithScopingNode)
        unregisterWithScopingNode();
    HTMLElement::willRemove();
}
#endif

void HTMLStyleElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    StyleElement::childrenChanged(this);
    HTMLElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
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

void HTMLStyleElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{    
    HTMLElement::addSubresourceAttributeURLs(urls);

    if (CSSStyleSheet* styleSheet = const_cast<HTMLStyleElement*>(this)->sheet())
        styleSheet->addSubresourceStyleURLs(urls);
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
