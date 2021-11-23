/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
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
#include "FETile.h"

#include "AffineTransform.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "Pattern.h"
#include "RenderTreeAsText.h"
#include "SVGRenderingContext.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FETile> FETile::create()
{
    return adoptRef(*new FETile());
}

FETile::FETile()
    : FilterEffect(FilterEffect::Type::FETile)
{
}

bool FETile::platformApplySoftware(const Filter& filter)
{
    FilterEffect* in = inputEffect(0);

    auto resultImage = imageBufferResult();
    auto inBuffer = in->imageBufferResult();
    if (!resultImage || !inBuffer)
        return false;

    setIsAlphaImage(in->isAlphaImage());

    // Source input needs more attention. It has the size of the filterRegion but gives the
    // size of the cutted sourceImage back. This is part of the specification and optimization.
    FloatRect tileRect = in->maxEffectRect();
    FloatPoint inMaxEffectLocation = tileRect.location();
    FloatPoint maxEffectLocation = maxEffectRect().location();
    if (in->filterType() == FilterEffect::Type::SourceGraphic || in->filterType() == FilterEffect::Type::SourceAlpha) {
        tileRect = filter.filterRegion();
        tileRect.scale(filter.filterScale());
    }

    auto tileImage = SVGRenderingContext::createImageBuffer(tileRect, tileRect, DestinationColorSpace::SRGB(), filter.renderingMode());
    if (!tileImage)
        return false;

    GraphicsContext& tileImageContext = tileImage->context();
    tileImageContext.translate(-inMaxEffectLocation.x(), -inMaxEffectLocation.y());
    tileImageContext.drawImageBuffer(*inBuffer, in->absolutePaintRect().location());

    auto tileImageCopy = ImageBuffer::sinkIntoNativeImage(WTFMove(tileImage));
    if (!tileImageCopy)
        return false;

    AffineTransform patternTransform;
    patternTransform.translate(inMaxEffectLocation - maxEffectLocation);

    auto pattern = Pattern::create(tileImageCopy.releaseNonNull(), { true, true, patternTransform });

    GraphicsContext& filterContext = resultImage->context();
    filterContext.setFillPattern(WTFMove(pattern));
    filterContext.fillRect(FloatRect(FloatPoint(), absolutePaintRect().size()));

    return true;
}

TextStream& FETile::externalRepresentation(TextStream& ts, RepresentationType representation) const
{
    ts << indent << "[feTile";
    FilterEffect::externalRepresentation(ts, representation);
    ts << "]\n";

    TextStream::IndentScope indentScope(ts);
    inputEffect(0)->externalRepresentation(ts, representation);

    return ts;
}

} // namespace WebCore
