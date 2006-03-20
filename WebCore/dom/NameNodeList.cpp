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
#include "NameNodeList.h"
#include "dom_elementimpl.h"

#include "HTMLNames.h"

using namespace WebCore;

namespace WebCore {

using namespace HTMLNames;

NameNodeList::NameNodeList(Node *n, const String &t )
  : NodeList(n), nodeName(t)
{
}

unsigned NameNodeList::length() const
{
    return recursiveLength();
}

Node *NameNodeList::item ( unsigned index ) const
{
    return recursiveItem( index );
}

bool NameNodeList::nodeMatches(Node *testNode) const
{
    return static_cast<Element *>(testNode)->getAttribute(nameAttr) == nodeName;
}

}
