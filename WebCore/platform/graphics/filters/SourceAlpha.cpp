/*
 *  Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 *
 *  This library is free software; you can redistribute it and/or
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

#if ENABLE(FILTERS)
#include "SourceAlpha.h"

#include "Color.h"
#include "GraphicsContext.h"
#include "PlatformString.h"
#include "Filter.h"

#include <wtf/StdLibExtras.h>

namespace WebCore {

PassRefPtr<SourceAlpha> SourceAlpha::create()
{
    return adoptRef(new SourceAlpha);
}

const AtomicString& SourceAlpha::effectName()
{
    DEFINE_STATIC_LOCAL(const AtomicString, s_effectName, ("SourceAlpha"));
    return s_effectName;
}

FloatRect SourceAlpha::calculateEffectRect(Filter* filter)
{
    FloatRect clippedSourceRect = filter->sourceImageRect();
    if (filter->sourceImageRect().x() < filter->filterRegion().x())
        clippedSourceRect.setX(filter->filterRegion().x());
    if (filter->sourceImageRect().y() < filter->filterRegion().y())
        clippedSourceRect.setY(filter->filterRegion().y());
    setSubRegion(clippedSourceRect);
    return filter->filterRegion();
}

void SourceAlpha::apply(Filter* filter)
{
    GraphicsContext* filterContext = getEffectContext();
    if (!filterContext)
        return;

    setIsAlphaImage(true);

    FloatRect imageRect(FloatPoint(), filter->sourceImage()->image()->size());
    filterContext->save();
    filterContext->clipToImageBuffer(imageRect, filter->sourceImage());
    filterContext->fillRect(imageRect, Color::black);
    filterContext->restore();
}

void SourceAlpha::dump()
{
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
