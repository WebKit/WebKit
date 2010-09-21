/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2009 Brent Fulgham <bfulgham@webkit.org>
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
 */

#ifndef ImageBufferFilter_h
#define ImageBufferFilter_h

#if ENABLE(FILTERS)
#include "Filter.h"
#include "FilterEffect.h"
#include "FloatRect.h"
#include "FloatSize.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class ImageBufferFilter : public Filter {
public:
    static PassRefPtr<ImageBufferFilter> create();

    virtual FloatRect filterRegion() const { return FloatRect(); }
    virtual FloatRect sourceImageRect() const { return FloatRect(); }

    // SVG specific
    virtual bool effectBoundingBoxMode() const { return false; }

    virtual FloatSize maxImageSize() const { return FloatSize(); }
    virtual void calculateEffectSubRegion(FilterEffect*) { }

private:
    ImageBufferFilter();
};

} // namespace WebCore

#endif // ENABLE(FILTERS)

#endif // ImageBufferFilter_h
