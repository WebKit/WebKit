/*
 *  Copyright (C) 2014 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "GObjectNodeFilterCondition.h"

#include "WebKitDOMNodePrivate.h"
#include <WebCore/NodeFilter.h>

using namespace WebCore;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

GObjectNodeFilterCondition::~GObjectNodeFilterCondition()
{
    g_object_set_data(G_OBJECT(m_filter.get()), "webkit-core-node-filter", nullptr);
}

unsigned short GObjectNodeFilterCondition::acceptNode(Node& node) const
{
    return webkit_dom_node_filter_accept_node(m_filter.get(), WebKit::kit(&node));
}

} // namespace WebKit
G_GNUC_END_IGNORE_DEPRECATIONS;
