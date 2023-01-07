/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "HTMLMenuElement.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "ElementChildIterator.h"
#include "HTMLMenuItemElement.h"
#include "HTMLNames.h"
#include "Page.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLMenuElement);

using namespace HTMLNames;

inline HTMLMenuElement::HTMLMenuElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(menuTag));
}

Node::InsertedIntoAncestorResult HTMLMenuElement::insertedIntoAncestor(InsertionType type, ContainerNode& ancestor)
{
    auto result = HTMLElement::insertedIntoAncestor(type, ancestor);
    if (type.connectedToDocument && document().settings().menuItemElementEnabled() && m_isTouchBarMenu) {
        if (auto* page = document().page())
            page->chrome().client().didInsertMenuElement(*this);
    }
    return result;
}

void HTMLMenuElement::removedFromAncestor(RemovalType type, ContainerNode& ancestor)
{
    HTMLElement::removedFromAncestor(type, ancestor);
    if (type.disconnectedFromDocument && document().settings().menuItemElementEnabled() && m_isTouchBarMenu) {
        if (auto* page = document().page())
            page->chrome().client().didRemoveMenuElement(*this);
    }
}

void HTMLMenuElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name != typeAttr || !document().settings().menuItemElementEnabled()) {
        HTMLElement::parseAttribute(name, value);
        return;
    }
    bool wasTouchBarMenu = m_isTouchBarMenu;
    m_isTouchBarMenu = equalLettersIgnoringASCIICase(value, "touchbar"_s);
    if (!wasTouchBarMenu && m_isTouchBarMenu) {
        if (auto* page = document().page()) {
            page->chrome().client().didInsertMenuElement(*this);
            for (auto& child : childrenOfType<Element>(*this))
                page->chrome().client().didInsertMenuItemElement(downcast<HTMLMenuItemElement>(child));
        }
    } else if (wasTouchBarMenu && !m_isTouchBarMenu) {
        if (auto* page = document().page())
            page->chrome().client().didRemoveMenuElement(*this);
    }
}

Ref<HTMLMenuElement> HTMLMenuElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLMenuElement(tagName, document));
}

}
