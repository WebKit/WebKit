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

#ifndef InRegionScrollableArea_h
#define InRegionScrollableArea_h

#include <BlackBerryPlatformPrimitives.h>
#include <interaction/ScrollViewBase.h>

namespace WebCore {
class RenderLayer;
}

namespace BlackBerry {
namespace WebKit {

class WebPagePrivate;

class InRegionScrollableArea : public Platform::ScrollViewBase {
public:

    InRegionScrollableArea();
    InRegionScrollableArea(WebPagePrivate*, WebCore::RenderLayer*);

    WebCore::RenderLayer* layer() const;

private:
    Platform::IntPoint calculateMinimumScrollPosition(const Platform::IntSize& viewportSize, float overscrollLimitFactor) const;
    Platform::IntPoint calculateMaximumScrollPosition(const Platform::IntSize& viewportSize, const Platform::IntSize& contentsSize, float overscrollLimitFactor) const;

    WebPagePrivate* m_webPage;
    WebCore::RenderLayer* m_layer;
};

}
}

#endif
