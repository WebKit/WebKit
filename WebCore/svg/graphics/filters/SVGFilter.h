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

#ifndef SVGFilter_h
#define SVGFilter_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "Filter.h"
#include "FilterEffect.h"
#include "FloatRect.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

    class SVGFilter : public Filter {
    public:
        static PassRefPtr<SVGFilter> create(const FloatRect&, const FloatRect&, bool);

        bool effectBoundingBoxMode() { return m_effectBBoxMode; }

        FloatRect filterRegion() { return m_filterRect; }
        FloatRect sourceImageRect() { return m_itemBox; }
        void calculateEffectSubRegion(FilterEffect*);

    private:
        SVGFilter(const FloatRect& itemBox, const FloatRect& filterRect, bool effectBBoxMode);

        FloatRect m_itemBox;
        FloatRect m_filterRect;
        bool m_effectBBoxMode;
    };

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)

#endif // SVGFilter_h
