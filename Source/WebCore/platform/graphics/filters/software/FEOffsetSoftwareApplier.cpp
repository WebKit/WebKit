/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2021 Apple Inc.  All rights reserved.
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
#include "FEOffsetSoftwareApplier.h"

#include "FEOffset.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"

namespace WebCore {

bool FEOffsetSoftwareApplier::apply(const Filter& filter, const FilterEffectVector& inputEffects)
{
    FilterEffect* in = inputEffects[0].get();

    auto resultImage = m_effect.imageBufferResult();
    auto inBuffer = in->imageBufferResult();
    if (!resultImage || !inBuffer)
        return false;

    m_effect.setIsAlphaImage(in->isAlphaImage());

    FloatRect drawingRegion = m_effect.drawingRegionOfInputImage(in->absolutePaintRect());
    drawingRegion.move(filter.scaledByFilterScale({ m_effect.dx(), m_effect.dy() }));
    resultImage->context().drawImageBuffer(*inBuffer, drawingRegion);

    return true;
}

} // namespace WebCore
