/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2007, 2014, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
 */

#include "config.h"
#include "TagCollection.h"

#include "NodeRareData.h"

namespace WebCore {

TagCollection::TagCollection(ContainerNode& rootNode, const AtomicString& namespaceURI, const AtomicString& localName)
    : CachedHTMLCollection<TagCollection, CollectionTypeTraits<ByTag>::traversalType>(rootNode, ByTag)
    , m_namespaceURI(namespaceURI)
    , m_localName(localName)
{
    ASSERT(m_namespaceURI.isNull() || !m_namespaceURI.isEmpty());
}

TagCollection::~TagCollection()
{
    if (m_namespaceURI == starAtom)
        ownerNode().nodeLists()->removeCachedCollection(this, m_localName);
    else
        ownerNode().nodeLists()->removeCachedCollectionWithQualifiedName(*this, m_namespaceURI, m_localName);
}

HTMLTagCollection::HTMLTagCollection(ContainerNode& rootNode, const AtomicString& localName)
    : CachedHTMLCollection<HTMLTagCollection, CollectionTypeTraits<ByHTMLTag>::traversalType>(rootNode, ByHTMLTag)
    , m_localName(localName)
    , m_loweredLocalName(localName.convertToASCIILowercase())
{
}

HTMLTagCollection::~HTMLTagCollection()
{
    ownerNode().nodeLists()->removeCachedCollection(this, m_localName);
}

} // namespace WebCore
