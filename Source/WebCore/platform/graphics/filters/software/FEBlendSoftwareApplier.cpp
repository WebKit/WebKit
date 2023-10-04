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

#if !HAVE(ARM_NEON_INTRINSICS)

#include "FEBlend.h"
#include "FloatPoint.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"

namespace WebCore {

bool FEBlendSoftwareApplier::apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const
{
    auto& input = inputs[0].get();
    auto& input2 = inputs[1].get();

    RefPtr resultImage = result.imageBuffer();
    if (!resultImage)
        return false;

    RefPtr inputImage = input.imageBuffer();
    RefPtr inputImage2 = input2.imageBuffer();
    if (!inputImage || !inputImage2)
        return false;

    auto& filterContext = resultImage->context();
    auto inputImageRect = input.absoluteImageRectRelativeTo(result);
    auto inputImageRect2 = input2.absoluteImageRectRelativeTo(result);

    filterContext.drawImageBuffer(*inputImage2, inputImageRect2);
    filterContext.drawImageBuffer(*inputImage, inputImageRect, { { }, inputImage->logicalSize() }, { CompositeOperator::SourceOver, m_effect.blendMode() });
    return true;
}

} // namespace WebCore

#endif // !HAVE(ARM_NEON_INTRINSICS)
