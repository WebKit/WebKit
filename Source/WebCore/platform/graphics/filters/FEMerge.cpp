/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
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
#include "FEMerge.h"

#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FEMerge> FEMerge::create()
{
    return adoptRef(*new FEMerge());
}

FEMerge::FEMerge()
    : FilterEffect(FilterEffect::Type::FEMerge)
{
}

bool FEMerge::platformApplySoftware(const Filter&)
{
    unsigned size = numberOfEffectInputs();
    ASSERT(size > 0);

    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return false;

    GraphicsContext& filterContext = resultImage->context();
    for (unsigned i = 0; i < size; ++i) {
        FilterEffect* in = inputEffect(i);
        if (ImageBuffer* inBuffer = in->imageBufferResult())
            filterContext.drawImageBuffer(*inBuffer, drawingRegionOfInputImage(in->absolutePaintRect()));
    }

    return true;
}

TextStream& FEMerge::externalRepresentation(TextStream& ts, RepresentationType representation) const
{
    ts << indent << "[feMerge";
    FilterEffect::externalRepresentation(ts, representation);
    unsigned size = numberOfEffectInputs();
    ASSERT(size > 0);
    ts << " mergeNodes=\"" << size << "\"]\n";

    TextStream::IndentScope indentScope(ts);
    for (unsigned i = 0; i < size; ++i)
        inputEffect(i)->externalRepresentation(ts, representation);
    return ts;
}

} // namespace WebCore
