/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
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
#include "Traversal.h"

#include "CallbackResult.h"
#include "Node.h"
#include "NodeFilter.h"
#include <wtf/SetForScope.h>

namespace WebCore {

NodeIteratorBase::NodeIteratorBase(Node& rootNode, unsigned whatToShow, RefPtr<NodeFilter>&& nodeFilter)
    : m_root(rootNode)
    , m_filter(WTFMove(nodeFilter))
    , m_whatToShow(whatToShow)
{
}

// https://dom.spec.whatwg.org/#concept-node-filter
ExceptionOr<unsigned short> NodeIteratorBase::acceptNode(Node& node)
{
    if (m_isActive)
        return Exception { InvalidStateError, "Recursive filters are not allowed"_s };

    // The bit twiddling here is done to map DOM node types, which are given as integers from
    // 1 through 14, to whatToShow bit masks.
    if (!(((1 << (node.nodeType() - 1)) & m_whatToShow)))
        return NodeFilter::FILTER_SKIP;

    if (!m_filter)
        return NodeFilter::FILTER_ACCEPT;

    SetForScope<bool> isActive(m_isActive, true);
    auto callbackResult = m_filter->acceptNode(node);
    if (callbackResult.type() == CallbackResultType::ExceptionThrown)
        return Exception { ExistingExceptionError };
    return callbackResult.releaseReturnValue();
}

} // namespace WebCore
