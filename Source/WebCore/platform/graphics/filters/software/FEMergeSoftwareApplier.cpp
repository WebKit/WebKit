/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) Apple Inc. 2021 All rights reserved.
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
#include "FEMergeSoftwareApplier.h"

#include "FEMerge.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"

namespace WebCore {

bool FEMergeSoftwareApplier::apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const
{
    ASSERT(inputs.size() == m_effect.numberOfEffectInputs());

    RefPtr resultImage = result.imageBuffer();
    if (!resultImage)
        return false;

    auto& filterContext = resultImage->context();

    for (auto& input : inputs) {
        RefPtr inputImage = input->imageBuffer();
        if (!inputImage)
            continue;

        auto inputImageRect = input->absoluteImageRectRelativeTo(result);
        filterContext.drawImageBuffer(*inputImage, inputImageRect);
    }

    return true;
}

} // namespace WebCore
