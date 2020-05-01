/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "config.h"
#include "SourceGraphic.h"

#include "Filter.h"
#include "GraphicsContext.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

Ref<SourceGraphic> SourceGraphic::create(Filter& filter)
{
    return adoptRef(*new SourceGraphic(filter));
}

const AtomString& SourceGraphic::effectName()
{
    static MainThreadNeverDestroyed<const AtomString> s_effectName("SourceGraphic", AtomString::ConstructFromLiteral);
    return s_effectName;
}

void SourceGraphic::determineAbsolutePaintRect()
{
    Filter& filter = this->filter();
    FloatRect paintRect = filter.sourceImageRect();
    paintRect.scale(filter.filterResolution().width(), filter.filterResolution().height());
    setAbsolutePaintRect(enclosingIntRect(paintRect));
}

void SourceGraphic::platformApplySoftware()
{
    Filter& filter = this->filter();

    ImageBuffer* resultImage = createImageBufferResult();
    ImageBuffer* sourceImage = filter.sourceImage();
    if (!resultImage || !sourceImage)
        return;

    resultImage->context().drawImageBuffer(*sourceImage, IntPoint());
}

TextStream& SourceGraphic::externalRepresentation(TextStream& ts, RepresentationType) const
{
    ts << indent << "[SourceGraphic]\n";
    return ts;
}

} // namespace WebCore
