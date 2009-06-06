/*
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
#include "WMLDocument.h"

#include "Page.h"
#include "Tokenizer.h"
#include "WMLCardElement.h"
#include "WMLErrorHandling.h"
#include "WMLPageState.h"
#include "WMLTemplateElement.h"

namespace WebCore {

WMLDocument::WMLDocument(Frame* frame)
    : Document(frame, false) 
{
    clearXMLVersion();
}

WMLDocument::~WMLDocument()
{
}

void WMLDocument::finishedParsing()
{
    if (Tokenizer* tokenizer = this->tokenizer()) {
        if (!tokenizer->wellFormed()) {
            Document::finishedParsing();
            return;
        }
    }

    WMLPageState* wmlPageState = wmlPageStateForDocument(this);
    if (!wmlPageState->isDeckAccessible()) {
        reportWMLError(this, WMLErrorDeckNotAccessible);
        Document::finishedParsing();
        return;
    }

    // Remember that we'e successfully entered the deck
    wmlPageState->setNeedCheckDeckAccess(false);

    initialize();
    Document::finishedParsing();
}

void WMLDocument::initialize()
{
    // Notify the existance of templates to all cards of the current deck
    WMLTemplateElement::registerTemplatesInDocument(this);

    // Set destination card
    WMLCardElement* card = WMLCardElement::determineActiveCard(this);
    if (!card) {
        reportWMLError(this, WMLErrorNoCardInDocument);
        Document::finishedParsing();
        return;
    }

    // Handle deck-level task overrides
    card->handleDeckLevelTaskOverridesIfNeeded();

    // Handle card-level intrinsic event
    card->handleIntrinsicEventIfNeeded();
}

WMLPageState* wmlPageStateForDocument(Document* doc)
{
    ASSERT(doc);

    Page* page = doc->page();
    ASSERT(page);

    return page->wmlPageState();
}

}

#endif
