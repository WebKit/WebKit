/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
#include "FEBlend.h"

#include "FEBlendNEON.h"
#include "Filter.h"
#include "FloatPoint.h"
#include "GraphicsContext.h"
#include <JavaScriptCore/Uint8ClampedArray.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

FEBlend::FEBlend(Filter& filter, BlendMode mode)
    : FilterEffect(filter)
    , m_mode(mode)
{
}

Ref<FEBlend> FEBlend::create(Filter& filter, BlendMode mode)
{
    return adoptRef(*new FEBlend(filter, mode));
}

bool FEBlend::setBlendMode(BlendMode mode)
{
    if (m_mode == mode)
        return false;
    m_mode = mode;
    return true;
}

#if !HAVE(ARM_NEON_INTRINSICS)
void FEBlend::platformApplySoftware()
{
    FilterEffect* in = inputEffect(0);
    FilterEffect* in2 = inputEffect(1);

    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return;
    GraphicsContext& filterContext = resultImage->context();

    ImageBuffer* imageBuffer = in->imageBufferResult();
    ImageBuffer* imageBuffer2 = in2->imageBufferResult();
    if (!imageBuffer || !imageBuffer2)
        return;

    filterContext.drawImageBuffer(*imageBuffer2, drawingRegionOfInputImage(in2->absolutePaintRect()));
    filterContext.drawImageBuffer(*imageBuffer, drawingRegionOfInputImage(in->absolutePaintRect()), IntRect(IntPoint(), imageBuffer->logicalSize()), { CompositeOperator::SourceOver, m_mode });
}
#endif

TextStream& FEBlend::externalRepresentation(TextStream& ts, RepresentationType representation) const
{
    ts << indent << "[feBlend";
    FilterEffect::externalRepresentation(ts, representation);
    ts << " mode=\"" << (m_mode == BlendMode::Normal ? "normal" : compositeOperatorName(CompositeOperator::SourceOver, m_mode)) << "\"]\n";

    TextStream::IndentScope indentScope(ts);
    inputEffect(0)->externalRepresentation(ts, representation);
    inputEffect(1)->externalRepresentation(ts, representation);
    return ts;
}

} // namespace WebCore
