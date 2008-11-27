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
#include "WMLErrorHandling.h"

#include "Document.h"
#include "XMLTokenizer.h"

namespace WebCore {

void reportWMLError(Document* doc, WMLErrorCode error)
{
    if (!doc || error == WMLErrorUnknown)
        return;

    XMLTokenizer* tokenizer = static_cast<XMLTokenizer*>(doc->tokenizer());
    if (!tokenizer)
        return;

    // Some errors are reported as result of an insertedIntoDocument() call.
    // If this happened, parsing has been stopped, and the document fragment
    // is wrapped in a XHTML error document. That means insertedIntoDocument()
    // will be called again - do NOT report the error twice, that would result
    // in an infinite error reporting loop.
    if (!tokenizer->wellFormed())
        return;

    const char* errorMessage = 0;
    switch (error) {
    case WMLErrorConflictingEventBinding:
        errorMessage = "Conflicting event bindings within an element.";
        break;
    case WMLErrorDeckNotAccessible:
        errorMessage = "Deck not accessible.";
        break;
    case WMLErrorDuplicatedDoElement:
        errorMessage = "At least two do elements share a name, which is not allowed.";
        break;
    case WMLErrorForbiddenTaskInAnchorElement:
        errorMessage = "Forbidden task contained in anchor element.";
        break;
    case WMLErrorInvalidColumnsNumberInTable:
        errorMessage = "A table contains an invalid number of columns.";
        break;
    case WMLErrorInvalidVariableName:
        errorMessage = "A variable name contains invalid characters.";
        break;
    case WMLErrorInvalidVariableReference:
        errorMessage = "A variable reference uses invalid syntax or is placed in an invalid location.";
        break;
    case WMLErrorMultipleAccessElements:
        errorMessage = "Only one access element is allowed in a deck.";
        break;
    case WMLErrorNoCardInDocument:
        errorMessage = "No card contained in document.";
        break;
    case WMLErrorMultipleTimerElements:
        errorMessage = "Only one access element is allowed in a card.";
        break;
    case WMLErrorUnknown: 
    default:
        ASSERT_NOT_REACHED();
        break;
    };

    tokenizer->handleError(XMLTokenizer::fatal, errorMessage, tokenizer->lineNumber(), tokenizer->columnNumber());
}

}

#endif
