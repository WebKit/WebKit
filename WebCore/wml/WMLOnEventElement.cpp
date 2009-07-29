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
#include "WMLOnEventElement.h"

#include "HTMLNames.h"
#include "MappedAttribute.h"
#include "WMLErrorHandling.h"
#include "WMLEventHandlingElement.h"
#include "WMLIntrinsicEventHandler.h"
#include "WMLNames.h"
#include "WMLVariables.h"

namespace WebCore {

using namespace WMLNames;

WMLOnEventElement::WMLOnEventElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
    , m_type(WMLIntrinsicEventUnknown)
{
}

void WMLOnEventElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == HTMLNames::typeAttr) {
        String parsedValue = parseValueForbiddingVariableReferences(attr->value());
        if (parsedValue.isEmpty())
            return;

        if (parsedValue == onenterforwardAttr)
            m_type = WMLIntrinsicEventOnEnterForward;
        else if (parsedValue == onenterbackwardAttr)
            m_type = WMLIntrinsicEventOnEnterBackward;
        else if (parsedValue == ontimerAttr)
            m_type = WMLIntrinsicEventOnTimer;
        else if (parsedValue == onpickAttr)
            m_type = WMLIntrinsicEventOnPick;
    } else
        WMLElement::parseMappedAttribute(attr);
}

static inline WMLEventHandlingElement* eventHandlingParent(Node* parent)
{
    if (!parent || !parent->isWMLElement())
        return 0;

    return toWMLEventHandlingElement(static_cast<WMLElement*>(parent));
}

void WMLOnEventElement::registerTask(WMLTaskElement* task)
{
    if (m_type == WMLIntrinsicEventUnknown)
        return;

    // Register intrinsic event to the event handler of the owner of onevent element 
    WMLEventHandlingElement* eventHandlingElement = eventHandlingParent(parentNode());
    if (!eventHandlingElement)
        return;

    eventHandlingElement->createEventHandlerIfNeeded();

    RefPtr<WMLIntrinsicEvent> event = WMLIntrinsicEvent::createWithTask(task);
    if (!eventHandlingElement->eventHandler()->registerIntrinsicEvent(m_type, event))
        reportWMLError(document(), WMLErrorConflictingEventBinding);
}

void WMLOnEventElement::deregisterTask(WMLTaskElement*)
{
    WMLEventHandlingElement* eventHandlingElement = eventHandlingParent(parentNode());
    if (!eventHandlingElement)
        return;

    eventHandlingElement->eventHandler()->deregisterIntrinsicEvent(m_type);
}

}

#endif
