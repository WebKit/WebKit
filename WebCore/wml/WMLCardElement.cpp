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

    // Hide all cards in document
    for (unsigned i = 0; i < length; ++i) {
        WMLCardElement* card = static_cast<WMLCardElement*>(nodeList->item(i));

        // Only need to recalculate the card style if the card was visible
        // before otherwhise we have no associated RenderObject anyway
        if (card->isVisible())
            card->setChanged();

        card->setVisible(false);
    }

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

    // Show active card
    if (!activeCard)
        activeCard = static_cast<WMLCardElement*>(nodeList->item(0));

    activeCard->setChanged();
    activeCard->setVisible(true);

    // Update the document title
    doc->setTitle(activeCard->title());

    // Set the active activeCard in the WML page state object
    pageState->setActiveCard(activeCard);
    return activeCard;
}

}

#endif
