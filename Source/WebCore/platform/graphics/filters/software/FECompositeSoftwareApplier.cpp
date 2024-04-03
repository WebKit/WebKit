/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2021-2023 Apple Inc.  All rights reserved.
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
#include "FECompositeSoftwareApplier.h"

#include "FEComposite.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "PixelBuffer.h"
#include <wtf/MathExtras.h>

namespace WebCore {

FECompositeSoftwareApplier::FECompositeSoftwareApplier(const FEComposite& effect)
    : Base(effect)
{
    ASSERT(m_effect.operation() != CompositeOperationType::FECOMPOSITE_OPERATOR_ARITHMETIC);
}

bool FECompositeSoftwareApplier::apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const
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

    switch (m_effect.operation()) {
    case CompositeOperationType::FECOMPOSITE_OPERATOR_UNKNOWN:
        return false;

    case CompositeOperationType::FECOMPOSITE_OPERATOR_OVER:
        filterContext.drawImageBuffer(*inputImage2, inputImageRect2);
        filterContext.drawImageBuffer(*inputImage, inputImageRect);
        break;

    case CompositeOperationType::FECOMPOSITE_OPERATOR_IN: {
        // Applies only to the intersected region.
        IntRect destinationRect = input.absoluteImageRect();
        destinationRect.intersect(input2.absoluteImageRect());
        destinationRect.intersect(result.absoluteImageRect());
        if (destinationRect.isEmpty())
            break;
        IntRect adjustedDestinationRect = destinationRect - result.absoluteImageRect().location();
        IntRect sourceRect = destinationRect - input.absoluteImageRect().location();
        IntRect source2Rect = destinationRect - input2.absoluteImageRect().location();
        filterContext.drawImageBuffer(*inputImage2, FloatRect(adjustedDestinationRect), FloatRect(source2Rect));
        filterContext.drawImageBuffer(*inputImage, FloatRect(adjustedDestinationRect), FloatRect(sourceRect), { CompositeOperator::SourceIn });
        break;
    }

    case CompositeOperationType::FECOMPOSITE_OPERATOR_OUT:
        filterContext.drawImageBuffer(*inputImage, inputImageRect);
        filterContext.drawImageBuffer(*inputImage2, inputImageRect2, { { }, inputImage2->logicalSize() }, { CompositeOperator::DestinationOut });
        break;

    case CompositeOperationType::FECOMPOSITE_OPERATOR_ATOP:
        filterContext.drawImageBuffer(*inputImage2, inputImageRect2);
        filterContext.drawImageBuffer(*inputImage, inputImageRect, { { }, inputImage->logicalSize() }, { CompositeOperator::SourceAtop });
        break;

    case CompositeOperationType::FECOMPOSITE_OPERATOR_XOR:
        filterContext.drawImageBuffer(*inputImage2, inputImageRect2);
        filterContext.drawImageBuffer(*inputImage, inputImageRect, { { }, inputImage->logicalSize() }, { CompositeOperator::XOR });
        break;

    case CompositeOperationType::FECOMPOSITE_OPERATOR_ARITHMETIC:
        ASSERT_NOT_REACHED();
        return false;

    case CompositeOperationType::FECOMPOSITE_OPERATOR_LIGHTER:
        filterContext.drawImageBuffer(*inputImage2, inputImageRect2);
        filterContext.drawImageBuffer(*inputImage, inputImageRect, { { }, inputImage->logicalSize() }, { CompositeOperator::PlusLighter });
        break;
    }

    return true;
}

} // namespace WebCore
