/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
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
 *
 */

#include "config.h"
#include "Traversal.h"

#include "Node.h"
#include "NodeFilter.h"
#include <wtf/PassRefPtr.h>

namespace WebCore {

Traversal::Traversal(Node* rootNode, unsigned whatToShow, PassRefPtr<NodeFilter> nodeFilter, bool expandEntityReferences)
    : m_root(rootNode)
    , m_whatToShow(whatToShow)
    , m_filter(nodeFilter)
    , m_expandEntityReferences(expandEntityReferences)
{
}

Traversal::~Traversal()
{
}

short Traversal::acceptNode(Node* node) const
{
    // FIXME: If XML is implemented we have to check expandEntityRerefences in this function.
    // The bid twiddling here is done to map DOM node types, which are given as integers from
    // 1 through 12, to whatToShow bit masks.
    if (node && ((1 << (node->nodeType()-1)) & m_whatToShow))
        // cast to short silences "enumeral and non-enumeral types in return" warning
        return m_filter ? m_filter->acceptNode(node) : static_cast<short>(NodeFilter::FILTER_ACCEPT);
    return NodeFilter::FILTER_SKIP;
}

} // namespace WebCore
