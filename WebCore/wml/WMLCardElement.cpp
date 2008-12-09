/*
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
#include "WMLCardElement.h"

#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLNames.h"
#include "NodeList.h"
#include "RenderStyle.h"
#include "WMLDocument.h"
#include "WMLIntrinsicEventHandler.h"
#include "WMLNames.h"
#include "WMLTimerElement.h"
#include "WMLVariables.h"

namespace WebCore {

using namespace WMLNames;

WMLCardElement::WMLCardElement(const QualifiedName& tagName, Document* doc)
    : WMLEventHandlingElement(tagName, doc)
    , m_isNewContext(false)
    , m_isOrdered(false)
    , m_isVisible(false)
    , m_eventTimer(0)
{
}

WMLCardElement::~WMLCardElement()
{
}

void WMLCardElement::showCard()
{
    ASSERT(attached());

    if (m_isVisible) {
        ASSERT(renderer());
        return;
    }

    m_isVisible = true;
    ASSERT(!renderer());

    detach();
    attach();

    ASSERT(attached());
    ASSERT(renderer());
}

void WMLCardElement::hideCard()
{
    ASSERT(attached());

    if (!m_isVisible) {
        ASSERT(!renderer());
        return;
    }

    m_isVisible = false;
    ASSERT(renderer());

    detach();
    attach();

    ASSERT(attached());
    ASSERT(!renderer());
}

void WMLCardElement::setIntrinsicEventTimer(WMLTimerElement* timer)
{
    // Only one timer is allowed in a card 
    if (m_eventTimer) {
        m_eventTimer = 0;     
        reportWMLError(document(), WMLErrorMultipleTimerElements);
        return;
    }

    m_eventTimer = timer;
}

void WMLCardElement::handleIntrinsicEventIfNeeded()
{
    WMLPageState* pageState = wmlPageStateForDocument(document());
    if (!pageState)
        return;

    Frame* frame = document()->frame();
    if (!frame)
        return;

    FrameLoader* loader = frame->loader();
    if (!loader)
        return;
    
    int currentHistoryLength = loader->getHistoryLength();
    int lastHistoryLength = pageState->historyLength();

    // Calculate the entry method of current card 
    WMLIntrinsicEventType eventType = WMLIntrinsicEventUnknown;
    if (lastHistoryLength > currentHistoryLength)
        eventType = WMLIntrinsicEventOnEnterBackward;
    else if (lastHistoryLength < currentHistoryLength)
        eventType = WMLIntrinsicEventOnEnterForward;

    // Synchronize history length with WMLPageState
    pageState->setHistoryLength(currentHistoryLength);
 
    // Figure out target event handler
    WMLIntrinsicEventHandler* eventHandler = this->eventHandler();
    bool hasIntrinsicEvent = false;

    if (eventType != WMLIntrinsicEventUnknown) {
        if (eventHandler && eventHandler->hasIntrinsicEvent(eventType))
            hasIntrinsicEvent = true;

        /* FIXME: template support
        else if (m_template) {
            eventHandler = m_template->eventHandler();
            if (eventHandler && eventHandler->hasIntrinsicEvent(eventType))
                hasIntrinsicEvent = true;
        }
        */
    }
 
    if (hasIntrinsicEvent)
        eventHandler->triggerIntrinsicEvent(eventType);

    // Start the timer if it exists in current card
    if (m_eventTimer)
        m_eventTimer->start();

    // FIXME: Initialize input/select  elements in this card
    /*
    Node* node = this;
    while (node = node->traverseNextNode()) {
        if (node->hasTagName(inputTag))
            static_cast<WMLInputElement*>(node)->init();
        else if (node->hasTagName(selectTag))
            static_cast<WMLSelectElement*>(node)->selectInitialOptions();
    }
    */
}

void WMLCardElement::parseMappedAttribute(MappedAttribute* attr)
{
    WMLIntrinsicEventType eventType = WMLIntrinsicEventUnknown;

    if (attr->name() == onenterforwardAttr)
        eventType = WMLIntrinsicEventOnEnterForward;
    else if (attr->name() == onenterbackwardAttr)
        eventType = WMLIntrinsicEventOnEnterBackward;
    else if (attr->name() == ontimerAttr)
        eventType = WMLIntrinsicEventOnTimer;
    else if (attr->name() == newcontextAttr)
        m_isNewContext = (attr->value() == "true");
    else if (attr->name() == orderedAttr)
        m_isOrdered = (attr->value() == "true");
    else {
        WMLEventHandlingElement::parseMappedAttribute(attr);
        return;
    }

    if (eventType == WMLIntrinsicEventUnknown)
        return;

    // Register intrinsic event in card
    RefPtr<WMLIntrinsicEvent> event = WMLIntrinsicEvent::create(document(), attr->value());

    createEventHandlerIfNeeded();
    eventHandler()->registerIntrinsicEvent(eventType, event);
}

void WMLCardElement::insertedIntoDocument()
{
    WMLEventHandlingElement::insertedIntoDocument();

    // The first card inserted into a document, is visible by default.
    if (!m_isVisible) {
        RefPtr<NodeList> nodeList = document()->getElementsByTagName("card");
        if (nodeList && nodeList->length() == 1 && nodeList->item(0) == this)
            m_isVisible = true;
    }
}

RenderObject* WMLCardElement::createRenderer(RenderArena* arena, RenderStyle* style) 
{
    if (!m_isVisible) {
        style->setUnique();
        style->setDisplay(NONE);
    }

    return WMLElement::createRenderer(arena, style);
}

WMLCardElement* WMLCardElement::setActiveCardInDocument(Document* doc, const KURL& targetUrl)
{
    WMLPageState* pageState = wmlPageStateForDocument(doc);
    if (!pageState)
        return 0;

    RefPtr<NodeList> nodeList = doc->getElementsByTagName("card");
    if (!nodeList)
        return 0;

    unsigned length = nodeList->length();
    if (length < 1)
        return 0;

    // Figure out the new target card
    WMLCardElement* activeCard = 0;
    KURL url = targetUrl.isEmpty() ? doc->url() : targetUrl;

    if (url.hasRef()) {
        String ref = url.ref();

        for (unsigned i = 0; i < length; ++i) {
            WMLCardElement* card = static_cast<WMLCardElement*>(nodeList->item(i));
            if (card->getIDAttribute() != ref)
                continue;

            // Force frame loader to load the URL with fragment identifier
            if (Frame* frame = doc->frame()) {
                if (FrameLoader* loader = frame->loader())
                    loader->setForceReloadWmlDeck(true);
            }

            activeCard = card;
            break;
        }
    }

    if (activeCard) {
        // Hide all cards - except the destination card - in document
        for (unsigned i = 0; i < length; ++i) {
            WMLCardElement* card = static_cast<WMLCardElement*>(nodeList->item(i));

            if (card == activeCard)
                card->showCard();
            else
                card->hideCard();
        }
    } else {
        // If the target URL didn't contain a fragment identifier, activeCard
        // is 0, and has to be set to the first card element in the deck.
        activeCard = static_cast<WMLCardElement*>(nodeList->item(0));
        activeCard->showCard();
    }

    // Assure destination card is visible
    ASSERT(activeCard->isVisible());
    ASSERT(activeCard->attached());
    ASSERT(activeCard->renderer());

    // Update the document title
    doc->setTitle(activeCard->title());

    // Set the active activeCard in the WMLPageState object
    pageState->setActiveCard(activeCard);
    return activeCard;
}

}

#endif
