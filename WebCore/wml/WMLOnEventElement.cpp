/**
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 *               http://www.torchmobile.com/
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
        const AtomicString& value = attr->value();
        if (containsVariableReference(value)) {
            // FIXME: error reporting
            // WMLHelper::tokenizer()->reportError(InvalidVariableReferenceError);
            return;
        }

        if (value == onenterforwardAttr)
            m_type = WMLIntrinsicEventOnEnterForward;
        else if (value == onenterbackwardAttr)
            m_type = WMLIntrinsicEventOnEnterBackward;
        else if (value == ontimerAttr)
            m_type = WMLIntrinsicEventOnTimer;
        else if (value == onpickAttr)
            m_type = WMLIntrinsicEventOnPick;
    } else
        WMLElement::parseMappedAttribute(attr);
}

void WMLOnEventElement::registerTask(WMLTaskElement* task)
{
    if (m_type == WMLIntrinsicEventUnknown)
        return;

    // Register intrinsic event to the event handler of the owner of onevent element 
    Node* parent = parentNode();
    ASSERT(parent);

    if (!parent || !parent->isWMLElement())
        return;

    WMLElement* parentElement = static_cast<WMLElement*>(parent);
    if (!parentElement->isWMLEventHandlingElement())
        return;

    WMLEventHandlingElement* eventHandlingElement = static_cast<WMLEventHandlingElement*>(parentElement);
    eventHandlingElement->createEventHandlerIfNeeded();

    RefPtr<WMLIntrinsicEvent> event = WMLIntrinsicEvent::createWithTask(task);
    eventHandlingElement->eventHandler()->registerIntrinsicEvent(m_type, event.releaseRef());
}

}

#endif
