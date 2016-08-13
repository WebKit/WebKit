/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2007, 2014-2016 Apple Inc. All rights reserved.
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

static inline AtomicString makeQualifiedName(const String& prefix, const String& localName)
{
    if (LIKELY(prefix.isNull()))
        return localName;
    return prefix + ':' + localName;
}

static inline void splitQualifiedName(const String& qualifiedName, AtomicString& prefix, AtomicString& localName)
{
    size_t index = qualifiedName.find(':');
    if (UNLIKELY(index == notFound))
        localName = qualifiedName;
    else {
        prefix = qualifiedName.substring(0, index);
        localName = qualifiedName.substring(index + 1);
    }
    ASSERT(makeQualifiedName(prefix, localName) == qualifiedName);
}

TagCollectionNS::TagCollectionNS(ContainerNode& rootNode, const AtomicString& namespaceURI, const AtomicString& localName)
    : CachedHTMLCollection<TagCollectionNS, CollectionTypeTraits<ByTag>::traversalType>(rootNode, ByTag)
    , m_namespaceURI(namespaceURI)
    , m_localName(localName)
{
    ASSERT(m_namespaceURI.isNull() || !m_namespaceURI.isEmpty());
}

TagCollectionNS::~TagCollectionNS()
{
    ownerNode().nodeLists()->removeCachedTagCollectionNS(*this, m_namespaceURI, m_localName);
}

TagCollection::TagCollection(ContainerNode& rootNode, const AtomicString& qualifiedName)
    : CachedHTMLCollection<TagCollection, CollectionTypeTraits<ByTag>::traversalType>(rootNode, ByTag)
{
    ASSERT(qualifiedName != starAtom);
    splitQualifiedName(qualifiedName, m_prefix, m_localName);
}

TagCollection::~TagCollection()
{
    ownerNode().nodeLists()->removeCachedCollection(this, makeQualifiedName(m_prefix, m_localName));
}

HTMLTagCollection::HTMLTagCollection(ContainerNode& rootNode, const AtomicString& qualifiedName)
    : CachedHTMLCollection<HTMLTagCollection, CollectionTypeTraits<ByHTMLTag>::traversalType>(rootNode, ByHTMLTag)
{
    ASSERT(qualifiedName != starAtom);
    splitQualifiedName(qualifiedName, m_prefix, m_localName);
    m_loweredPrefix = m_prefix.convertToASCIILowercase();
    m_loweredLocalName = m_localName.convertToASCIILowercase();
}

HTMLTagCollection::~HTMLTagCollection()
{
    ownerNode().nodeLists()->removeCachedCollection(this, makeQualifiedName(m_prefix, m_localName));
}

} // namespace WebCore
