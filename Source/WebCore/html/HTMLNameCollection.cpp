/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2011, 2012 Apple Inc. All rights reserved.
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
#include "HTMLNameCollection.h"

#include "Element.h"
#include "HTMLEmbedElement.h"
#include "HTMLFormElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLImageElement.h"
#include "HTMLNameCollectionInlines.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "NodeRareData.h"
#include "NodeTraversal.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

template HTMLNameCollection<WindowNameCollection>::~HTMLNameCollection();
template HTMLNameCollection<DocumentNameCollection>::~HTMLNameCollection();

WTF_MAKE_TZONE_ALLOCATED_IMPL(WindowNameCollection);
WTF_MAKE_TZONE_ALLOCATED_IMPL(DocumentNameCollection);

bool WindowNameCollection::elementMatchesIfNameAttributeMatch(const Element& element)
{
    return isAnyOf<HTMLEmbedElement, HTMLFormElement, HTMLImageElement, HTMLObjectElement>(element);
}

bool WindowNameCollection::elementMatches(const Element& element, const AtomString& name)
{
    // Find only images, forms, applets, embeds and objects by name, but anything by id.
    return (elementMatchesIfNameAttributeMatch(element) && element.getNameAttribute() == name)
        || element.getIdAttribute() == name;
}

static inline bool NODELETE isObjectElementForDocumentNameCollection(const Element& element)
{
    // This is used for supportedPropertyNames enumeration. All object elements
    // are included unconditionally to match other browsers' behavior.
    // The isExposed() check is applied later in elementMatches() when resolving
    // a specific name to actual elements.
    return is<HTMLObjectElement>(element);
}

bool DocumentNameCollection::elementMatchesIfIdAttributeMatch(const Element& element)
{
    // FIXME: We need to fix HTMLImageElement to update the hash map for us when the name attribute is removed.
    return isObjectElementForDocumentNameCollection(element)
        || (is<HTMLImageElement>(element) && element.hasName() && !element.getNameAttribute().isEmpty());
}

bool DocumentNameCollection::elementMatchesIfNameAttributeMatch(const Element& element)
{
    return isObjectElementForDocumentNameCollection(element)
        || isAnyOf<HTMLEmbedElement, HTMLFormElement, HTMLIFrameElement, HTMLImageElement>(element);
}

bool DocumentNameCollection::elementMatches(const Element& element, const AtomString& name)
{
    // https://html.spec.whatwg.org/multipage/dom.html#dom-document-nameditem
    // Only exposed object/embed elements should match when resolving a name.
    // Note: elementMatchesIfNameAttributeMatch/elementMatchesIfIdAttributeMatch intentionally
    // do not check isExposed() because they are used for supportedPropertyNames enumeration,
    // where browsers include all object elements unconditionally.
    if (auto* object = dynamicDowncast<HTMLObjectElement>(element))
        return object->isExposed() && (element.getNameAttribute() == name || element.getIdAttribute() == name);
    if (auto* embed = dynamicDowncast<HTMLEmbedElement>(element))
        return embed->isExposed() && element.getNameAttribute() == name;

    if (is<HTMLImageElement>(element)) {
        const auto& nameValue = element.getNameAttribute();
        return nameValue == name || (element.getIdAttribute() == name && !nameValue.isEmpty());
    }
    return isAnyOf<HTMLFormElement, HTMLIFrameElement>(element) && element.getNameAttribute() == name;
}

}
