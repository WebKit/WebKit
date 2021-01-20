/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTMLMenuItemElement.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "HTMLMenuElement.h"
#include "HTMLNames.h"
#include "Page.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLMenuItemElement);

using namespace HTMLNames;
    
inline HTMLMenuItemElement::HTMLMenuItemElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(menuitemTag));
}

Node::InsertedIntoAncestorResult HTMLMenuItemElement::insertedIntoAncestor(InsertionType type, ContainerNode& ancestor)
{
    auto result = HTMLElement::insertedIntoAncestor(type, ancestor);
    if (type.connectedToDocument) {
        if (auto* page = document().page()) {
            if (is<HTMLMenuElement>(ancestor) && downcast<HTMLMenuElement>(ancestor).isTouchBarMenu())
                page->chrome().client().didInsertMenuItemElement(*this);
        }
    }
    return result;
}

void HTMLMenuItemElement::removedFromAncestor(RemovalType type, ContainerNode& ancestor)
{
    HTMLElement::removedFromAncestor(type, ancestor);
    if (type.disconnectedFromDocument) {
        if (auto* page = document().page()) {
            if (is<HTMLMenuElement>(ancestor) && downcast<HTMLMenuElement>(ancestor).isTouchBarMenu())
                page->chrome().client().didRemoveMenuItemElement(*this);
        }
    }
}

Ref<HTMLMenuItemElement> HTMLMenuItemElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLMenuItemElement(tagName, document));
}

}
