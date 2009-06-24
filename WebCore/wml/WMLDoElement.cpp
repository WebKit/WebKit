/**
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#if ENABLE(WML)
#include "WMLDoElement.h"

#include "Event.h"
#include "EventNames.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "MappedAttribute.h"
#include "Page.h"
#include "RenderButton.h"
#include "WMLCardElement.h"
#include "WMLDocument.h"
#include "WMLTaskElement.h"
#include "WMLTimerElement.h"
#include "WMLNames.h"
#include "WMLPageState.h"
#include "WMLVariables.h"

namespace WebCore {

using namespace WMLNames;

WMLDoElement::WMLDoElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
    , m_task(0)
    , m_isActive(false)
    , m_isNoop(false)
    , m_isOptional(false)
{
}

void WMLDoElement::defaultEventHandler(Event* event)
{
    if (m_isOptional)
        return;

    if (event->type() == eventNames().keypressEvent) {
        WMLElement::defaultEventHandler(event);
        return;
    }

    if (event->type() != eventNames().clickEvent && event->type() != eventNames().keydownEvent)
        return;
             
    if (event->isKeyboardEvent()
        && static_cast<KeyboardEvent*>(event)->keyIdentifier() != "Enter")
        return;

    if (m_type == "accept" || m_type == "options") {
        if (m_task)
            m_task->executeTask(event);
    } else if (m_type == "prev") {
        WMLPageState* pageState = wmlPageStateForDocument(document());
        if (!pageState)
            return;

        // Stop the timer of the current card if it is active
        if (WMLCardElement* card = pageState->activeCard()) {
            if (WMLTimerElement* eventTimer = card->eventTimer())
                eventTimer->stop();
        }

        pageState->page()->goBack();
    } else if (m_type == "reset") {
        WMLPageState* pageState = wmlPageStateForDocument(document());
        if (!pageState)
            return;

        pageState->reset();
    }
}

void WMLDoElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == HTMLNames::typeAttr)
        m_type = parseValueForbiddingVariableReferences(attr->value());
    else if (attr->name() == HTMLNames::nameAttr)
        m_name = parseValueForbiddingVariableReferences(attr->value());
    else if (attr->name() == optionalAttr)
        m_isOptional = (attr->value() == "true");
    else
        WMLElement::parseMappedAttribute(attr);
}

void WMLDoElement::insertedIntoDocument()
{
    WMLElement::insertedIntoDocument();

    // Spec: An unspecified 'name' defaults to the value of the 'type' attribute.
    if (!hasAttribute(HTMLNames::nameAttr))
        m_name = m_type;

    Node* parent = parentNode();
    ASSERT(parent);

    if (!parent || !parent->isWMLElement())
        return;

    if (WMLEventHandlingElement* eventHandlingElement = toWMLEventHandlingElement(static_cast<WMLElement*>(parent)))
        eventHandlingElement->registerDoElement(this, document());
}

RenderObject* WMLDoElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    if (!m_isActive || m_isOptional || m_isNoop)
        return 0;

    if (style) {
        style->setUnique();
        style->setBackgroundColor(Color::lightGray);
    }

    return new (arena) RenderButton(this);
}

void WMLDoElement::recalcStyle(StyleChange change)
{
    WMLElement::recalcStyle(change);

    if (renderer())
        renderer()->updateFromElement();
}

String WMLDoElement::label() const
{
    return parseValueSubstitutingVariableReferences(getAttribute(HTMLNames::labelAttr));
}

}

#endif
