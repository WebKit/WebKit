/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "ChildNodeList.h"
#include "Node.h"

using namespace WebCore;

namespace WebCore {

ChildNodeList::ChildNodeList( Node *n )
    : NodeList(n)
{
}

unsigned ChildNodeList::length() const
{
    if (isLengthCacheValid)
        return cachedLength;

    unsigned len = 0;
    Node *n;
    for(n = rootNode->firstChild(); n != 0; n = n->nextSibling())
        len++;

    cachedLength = len;
    isLengthCacheValid = true;

    return len;
}

Node *ChildNodeList::item ( unsigned index ) const
{
    unsigned int pos = 0;
    Node *n = rootNode->firstChild();

    if (isItemCacheValid) {
        if (index == lastItemOffset) {
            return lastItem;
        } else if (index > lastItemOffset) {
            n = lastItem;
            pos = lastItemOffset;
        }
    }

    while (n && pos < index) {
        n = n->nextSibling();
        pos++;
    }

    if (n) {
        lastItem = n;
        lastItemOffset = pos;
        isItemCacheValid = true;
        return n;
    }

    return 0;
}

bool ChildNodeList::nodeMatches(Node *testNode) const
{
    return testNode->parentNode() == rootNode;
}

}
