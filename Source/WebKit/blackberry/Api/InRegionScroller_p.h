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

#ifndef InRegionScroller_p_h
#define InRegionScroller_p_h

#include "IntSize.h"
#include "IntPoint.h"

#include <interaction/ScrollViewBase.h>
#include <vector>

namespace WebCore {
class Frame;
class Node;
class RenderObject;
class RenderLayer;
}

namespace BlackBerry {
namespace WebKit {

class InRegionScrollableArea;
class WebPagePrivate;

class InRegionScrollerPrivate {
public:
    InRegionScrollerPrivate(WebPagePrivate*);

    void setNode(WebCore::Node*);
    WebCore::Node* node() const;
    void reset();

    bool canScroll() const;
    bool hasNode() const;

    bool scrollBy(const Platform::IntSize& delta);

    bool setScrollPositionCompositingThread(unsigned camouflagedLayer, const WebCore::IntPoint& scrollPosition);
    bool setScrollPositionWebKitThread(unsigned camouflagedLayer, const WebCore::IntPoint& scrollPosition);

    void calculateInRegionScrollableAreasForPoint(const WebCore::IntPoint&);
    const std::vector<Platform::ScrollViewBase*>& activeInRegionScrollableAreas() const;

    WebPagePrivate* m_webPage;

private:
    bool setLayerScrollPosition(WebCore::RenderLayer*, const WebCore::IntPoint& scrollPosition);

    void pushBackInRegionScrollable(InRegionScrollableArea*);

    // Obsolete codepath.
    bool scrollNodeRecursively(WebCore::Node*, const WebCore::IntSize& delta);
    bool scrollRenderer(WebCore::RenderObject*, const WebCore::IntSize& delta);
    void adjustScrollDelta(const WebCore::IntPoint& maxOffset, const WebCore::IntPoint& currentOffset, WebCore::IntSize& delta) const;

    RefPtr<WebCore::Node> m_inRegionScrollStartingNode;
    std::vector<Platform::ScrollViewBase*> m_activeInRegionScrollableAreas;
};

}
}

#endif
