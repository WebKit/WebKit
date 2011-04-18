/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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
 *
 */

#include "config.h"
#include "HTMLDetailsElement.h"

#include "HTMLNames.h"
#include "HTMLSummaryElement.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "RenderDetails.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

PassRefPtr<HTMLDetailsElement> HTMLDetailsElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLDetailsElement(tagName, document));
}

HTMLDetailsElement::HTMLDetailsElement(const QualifiedName& tagName, Document* document)
    : HTMLElement(tagName, document)
    , m_mainSummary(0)
    , m_isOpen(false)
{
    ASSERT(hasTagName(detailsTag));
}

RenderObject* HTMLDetailsElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderDetails(this);
}

Node* HTMLDetailsElement::findSummaryFor(ContainerNode* container)
{
    for (Node* child = container->firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(summaryTag))
            return child;
    }

    return 0;
}

Node* HTMLDetailsElement::findMainSummary()
{
    Node* found = findSummaryFor(this);
    if (found) {
        removeShadowRoot();
        return found;
    }
    
    createShadowSubtree();
    found = findSummaryFor(shadowRoot());
    ASSERT(found);
    return found;
}

void HTMLDetailsElement::refreshMainSummary(RefreshRenderer refreshRenderer)
{
    RefPtr<Node> oldSummary = m_mainSummary;
    m_mainSummary = findMainSummary();

    if (oldSummary == m_mainSummary || !attached())
        return;

    if (oldSummary && oldSummary->parentNodeForRenderingAndStyle()) {
        oldSummary->detach();
        oldSummary->attach();
    }
        
    if (refreshRenderer == RefreshRendererAllowed) {
        m_mainSummary->detach();
        m_mainSummary->attach();
    }
}

void HTMLDetailsElement::createShadowSubtree()
{
    if (shadowRoot())
        return;

    RefPtr<HTMLSummaryElement> defaultSummary = HTMLSummaryElement::create(summaryTag, document());
    ExceptionCode ec = 0;
    defaultSummary->appendChild(Text::create(document(), defaultDetailsSummaryText()), ec);
    ensureShadowRoot()->appendChild(defaultSummary, ec, true);
}


void HTMLDetailsElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    HTMLElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    // If childCountDelta is less then zero and the main summary has changed it must be because previous main
    // summary was removed. The new main summary was then inside the unrevealed content and needs to be
    // reattached to create its renderer. If childCountDelta is not less then zero then a new <summary> element
    // has been added and it will be attached without our help.
    if (!changedByParser)
        refreshMainSummary(childCountDelta < 0 ? RefreshRendererAllowed : RefreshRendererSupressed);
}

void HTMLDetailsElement::finishParsingChildren()
{
    HTMLElement::finishParsingChildren();
    refreshMainSummary(RefreshRendererAllowed);
}

void HTMLDetailsElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == openAttr) {
        bool oldValue = m_isOpen;
        m_isOpen =  !attr->value().isNull();
        if (attached() && oldValue != m_isOpen) {
            detach();
            attach();
        }
    } else
        HTMLElement::parseMappedAttribute(attr);
}

bool HTMLDetailsElement::childShouldCreateRenderer(Node* child) const
{
    return m_isOpen || child == m_mainSummary;
}

void HTMLDetailsElement::toggleOpen()
{
    setAttribute(openAttr, m_isOpen ? nullAtom : emptyAtom);
}

}
