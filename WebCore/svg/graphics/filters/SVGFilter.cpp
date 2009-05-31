/*
 *  Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 *
 *   This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  aint with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFilter.h"

namespace WebCore {

SVGFilter::SVGFilter(const FloatRect& itemBox, const FloatRect& filterRect, bool effectBBoxMode, bool filterBBoxMode)
    : Filter()
    , m_itemBox(itemBox)
    , m_filterRect(filterRect)
    , m_effectBBoxMode(effectBBoxMode)
    , m_filterBBoxMode(filterBBoxMode)
{
}

void SVGFilter::calculateEffectSubRegion(FilterEffect*)
{
}

PassRefPtr<SVGFilter> SVGFilter::create(const FloatRect& itemBox, const FloatRect& filterRect, bool effectBBoxMode, bool filterBBoxMode)
{
    return adoptRef(new SVGFilter(itemBox, filterRect, effectBBoxMode, filterBBoxMode));
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)
