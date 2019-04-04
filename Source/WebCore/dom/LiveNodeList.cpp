/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006-2008, 2010, 2013-2014 Apple Inc. All rights reserved.
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
#include "LiveNodeList.h"

#include "ClassCollection.h"
#include "Element.h"
#include "ElementTraversal.h"
#include "HTMLCollection.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LiveNodeList);

LiveNodeList::LiveNodeList(ContainerNode& ownerNode, NodeListInvalidationType invalidationType)
    : m_ownerNode(ownerNode)
    , m_invalidationType(invalidationType)
    , m_isRegisteredForInvalidationAtDocument(false)
{
    ASSERT(m_invalidationType == static_cast<unsigned>(invalidationType));
}

LiveNodeList::~LiveNodeList() = default;

ContainerNode& LiveNodeList::rootNode() const
{
    if (isRootedAtDocument() && ownerNode().isConnected())
        return ownerNode().document();

    return ownerNode();
}

} // namespace WebCore
