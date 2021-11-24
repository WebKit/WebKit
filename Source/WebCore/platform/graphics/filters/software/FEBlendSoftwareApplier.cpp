/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
#include "FEBlendSoftwareApplier.h"

#include "FEBlend.h"
#include "FloatPoint.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"

namespace WebCore {

#if !HAVE(ARM_NEON_INTRINSICS)
bool FEBlendSoftwareApplier::apply(const Filter&, const FilterEffectVector& inputEffects)
{
    FilterEffect* in = inputEffects[0].get();
    FilterEffect* in2 = inputEffects[1].get();

    auto resultImage = m_effect.imageBufferResult();
    if (!resultImage)
        return false;

    auto imageBuffer = in->imageBufferResult();
    auto imageBuffer2 = in2->imageBufferResult();
    if (!imageBuffer || !imageBuffer2)
        return false;

    GraphicsContext& filterContext = resultImage->context();
    filterContext.drawImageBuffer(*imageBuffer2, m_effect.drawingRegionOfInputImage(in2->absolutePaintRect()));
    filterContext.drawImageBuffer(*imageBuffer, m_effect.drawingRegionOfInputImage(in->absolutePaintRect()), { { }, imageBuffer->logicalSize() }, { CompositeOperator::SourceOver, m_effect.blendMode() });
    return true;
}
#endif

} // namespace WebCore
