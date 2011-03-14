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

#include "Frame.h"
#include "HTMLNames.h"
#include "MouseEvent.h"
#include "PlatformMouseEvent.h"
#include "RenderDetails.h"

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

void HTMLDetailsElement::findMainSummary()
{
    m_mainSummary = 0;

    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(summaryTag)) {
            m_mainSummary = child;
            break;
        }
    }
}

void HTMLDetailsElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    HTMLElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    if (!changedByParser) {
        Node* oldSummary = m_mainSummary;
        findMainSummary();

        if (oldSummary != m_mainSummary && !m_isOpen && attached()) {
            if (oldSummary && oldSummary->attached())
                oldSummary->detach();
            if (m_mainSummary && childCountDelta < 0 && !m_mainSummary->renderer()) {
                // If childCountDelta is less then zero and the main summary has changed it must be because previous main
                // summary was removed. The new main summary was then inside the unrevealed content and needs to be
                // reattached to create its renderer. If childCountDelta is not less then zero then a new <summary> element
                // has been added and it will be attached without our help.
                m_mainSummary->detach();
                m_mainSummary->attach();
            }
        }
    }
}

void HTMLDetailsElement::finishParsingChildren()
{
    HTMLElement::finishParsingChildren();
    findMainSummary();
    if (attached() && m_mainSummary && !m_mainSummary->renderer()) {
        m_mainSummary->detach();
        m_mainSummary->attach();
    }
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

void HTMLDetailsElement::defaultEventHandler(Event* event)
{
    HTMLElement::defaultEventHandler(event);

    if (!renderer() || !renderer()->isDetails() || !event->isMouseEvent() || event->type() != eventNames().clickEvent || event->defaultHandled())
        return;

    MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
    if (mouseEvent->button() != LeftButton)
        return;

    RenderDetails* renderDetails = static_cast<RenderDetails*>(renderer());

    float factor = document() && document()->frame() ? document()->frame()->pageZoomFactor() : 1.0f;
    FloatPoint pos = renderDetails->absoluteToLocal(FloatPoint(mouseEvent->pageX() * factor, mouseEvent->pageY() * factor));

    if (renderDetails->interactiveArea().contains(pos.x(), pos.y())) {
        setAttribute(openAttr, m_isOpen ? String() : String(""));
        event->setDefaultHandled();
    }
}

}
