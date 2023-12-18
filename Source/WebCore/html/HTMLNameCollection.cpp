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
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

template HTMLNameCollection<WindowNameCollection, CollectionTraversalType::Descendants>::~HTMLNameCollection();
template HTMLNameCollection<DocumentNameCollection, CollectionTraversalType::Descendants>::~HTMLNameCollection();

WTF_MAKE_ISO_ALLOCATED_IMPL(WindowNameCollection);
WTF_MAKE_ISO_ALLOCATED_IMPL(DocumentNameCollection);

bool WindowNameCollection::elementMatchesIfNameAttributeMatch(const Element& element)
{
    return is<HTMLEmbedElement>(element)
        || is<HTMLFormElement>(element)
        || is<HTMLImageElement>(element)
        || is<HTMLObjectElement>(element);
}

bool WindowNameCollection::elementMatches(const Element& element, const AtomString& name)
{
    // Find only images, forms, applets, embeds and objects by name, but anything by id.
    return (elementMatchesIfNameAttributeMatch(element) && element.getNameAttribute() == name)
        || element.getIdAttribute() == name;
}

static inline bool isObjectElementForDocumentNameCollection(const Element& element)
{
    auto* objectElement = dynamicDowncast<HTMLObjectElement>(element);
    return objectElement && objectElement->isExposed();
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
        || is<HTMLEmbedElement>(element)
        || is<HTMLFormElement>(element)
        || is<HTMLIFrameElement>(element)
        || is<HTMLImageElement>(element);
}

bool DocumentNameCollection::elementMatches(const Element& element, const AtomString& name)
{
    // Find images, forms, applets, embeds, objects and iframes by name, applets and object by id, and images by id
    // but only if they have a name attribute (this very strange rule matches IE).
    return (elementMatchesIfNameAttributeMatch(element) && element.getNameAttribute() == name)
        || (elementMatchesIfIdAttributeMatch(element) && element.getIdAttribute() == name);
}

}
