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

#pragma once

#include "WebKitDOMNodeFilter.h"
#include <WebCore/NodeFilterCondition.h>
#include <wtf/glib/GRefPtr.h>

namespace WebCore {
class Node;
}

namespace WebKit {

class GObjectNodeFilterCondition final : public WebCore::NodeFilterCondition {
public:
    static Ref<GObjectNodeFilterCondition> create(WebKitDOMNodeFilter* filter)
    {
        return adoptRef(*new GObjectNodeFilterCondition(filter));
    }

    unsigned short acceptNode(WebCore::Node&) const override;

private:
    GObjectNodeFilterCondition(WebKitDOMNodeFilter* filter)
        : m_filter(filter)
    {
    }
    ~GObjectNodeFilterCondition();

    GRefPtr<WebKitDOMNodeFilter> m_filter;
};

} // namespace WebKit
