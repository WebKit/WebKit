/*
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
#include "WMLTemplateElement.h"

#include "MappedAttribute.h"
#include "NodeList.h"
#include "WMLCardElement.h"
#include "WMLDocument.h"
#include "WMLIntrinsicEventHandler.h"
#include "WMLNames.h"

namespace WebCore {

using namespace WMLNames;

WMLTemplateElement::WMLTemplateElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
{
}

WMLTemplateElement::~WMLTemplateElement()
{
}

void WMLTemplateElement::parseMappedAttribute(MappedAttribute* attr)
{
    WMLIntrinsicEventType eventType = WMLIntrinsicEventUnknown;

    if (attr->name() == onenterforwardAttr)
        eventType = WMLIntrinsicEventOnEnterForward;
    else if (attr->name() == onenterbackwardAttr)
        eventType = WMLIntrinsicEventOnEnterBackward;
    else if (attr->name() == ontimerAttr)
        eventType = WMLIntrinsicEventOnTimer;
    else {
        WMLElement::parseMappedAttribute(attr);
        return;
    }

    if (eventType == WMLIntrinsicEventUnknown)
        return;

    // Register intrinsic event in card
    RefPtr<WMLIntrinsicEvent> event = WMLIntrinsicEvent::create(document(), attr->value());

    createEventHandlerIfNeeded();
    eventHandler()->registerIntrinsicEvent(eventType, event);
}

void WMLTemplateElement::registerTemplatesInDocument(Document* doc)
{
    ASSERT(doc);

    // Build list of cards in document
    RefPtr<NodeList> nodeList = doc->getElementsByTagName("card");
    if (!nodeList)
        return;

    unsigned length = nodeList->length();
    if (length < 1)
        return;

    HashSet<WMLCardElement*> cards;
    for (unsigned i = 0; i < length; ++i)
        cards.add(static_cast<WMLCardElement*>(nodeList->item(i)));    

    if (cards.isEmpty())
        return;

    // Register template element to all cards
    nodeList = doc->getElementsByTagName("template");
    if (!nodeList)
        return;

    length = nodeList->length();
    if (length < 1)
        return;

    // Only one template element should be allowed in a document
    // Calling setTemplateElement() twice on a WMLCardElement, will result in a tokenizer error.
    for (unsigned i = 0; i < length; ++i) {
        WMLTemplateElement* temp = static_cast<WMLTemplateElement*>(nodeList->item(i));

        HashSet<WMLCardElement*>::iterator it = cards.begin();
        HashSet<WMLCardElement*>::iterator end = cards.end();

        for (; it != end; ++it)
            (*it)->setTemplateElement(temp);
    }
}

}

#endif
