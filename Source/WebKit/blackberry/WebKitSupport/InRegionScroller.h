/*
 * Copyright (C) 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef InRegionScroller_h
#define InRegionScroller_h

#include "IntRect.h"

#include <interaction/ScrollViewBase.h>
#include <vector>

namespace WebCore {
class Frame;
class Node;
class RenderObject;
}

namespace BlackBerry {
namespace WebKit {

class WebPagePrivate;

class InRegionScroller {

public:
    InRegionScroller(WebPagePrivate*);

    void setNode(WebCore::Node*);
    WebCore::Node* node() const;
    void reset();

    bool canScroll() const;
    bool hasNode() const;

    bool scrollBy(const Platform::IntSize& delta);

    std::vector<Platform::ScrollViewBase*> inRegionScrollableAreasForPoint(const WebCore::IntPoint&);
private:

    bool scrollNodeRecursively(WebCore::Node*, const WebCore::IntSize& delta);
    bool scrollRenderer(WebCore::RenderObject*, const WebCore::IntSize& delta);

    void adjustScrollDelta(const WebCore::IntPoint& maxOffset, const WebCore::IntPoint& currentOffset, WebCore::IntSize& delta) const;

    RefPtr<WebCore::Node> m_inRegionScrollStartingNode;
    WebPagePrivate* m_webPage;
};

}
}

#endif
