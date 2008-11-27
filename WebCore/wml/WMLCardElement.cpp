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
#include "NodeList.h"
#include "Page.h"
#include "RenderStyle.h"
#include "WMLDocument.h"
#include "WMLPageState.h"

namespace WebCore {

WMLCardElement::WMLCardElement(const QualifiedName& tagName, Document* doc)
    : WMLEventHandlingElement(tagName, doc)
    , m_isVisible(false)
{
}

WMLCardElement::~WMLCardElement()
{
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
            if (Frame* frame = pageState->page()->mainFrame())
                if (FrameLoader* loader = frame->loader())
                    loader->setForceReloadWmlDeck(true);

            activeCard = card;
            break;
        }
    }

    if (activeCard) {
        // Hide all cards - except the destination card - in document
        for (unsigned i = 0; i < length; ++i) {
            WMLCardElement* card = static_cast<WMLCardElement*>(nodeList->item(i));
            bool cardShouldBeVisible = (card == activeCard);

            if (cardShouldBeVisible && !card->isVisible()) {
                card->setChanged();
                card->setVisible(true);
            } else if (!cardShouldBeVisible && card->isVisible()) {
                card->setChanged();
                card->setVisible(false);
            }
        }
    } else {
        // The first card should already be visible.
        activeCard = static_cast<WMLCardElement*>(nodeList->item(0));
        ASSERT(activeCard->isVisible());
    }

    // Update the document title
    doc->setTitle(activeCard->title());

    // Set the active activeCard in the WML page state object
    pageState->setActiveCard(activeCard);
    return activeCard;
}

}

#endif
