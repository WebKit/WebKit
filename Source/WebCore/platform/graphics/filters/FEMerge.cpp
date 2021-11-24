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

#include "FilterEffectApplier.h"
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

// FIXME: Move the class FEMergeSoftwareApplier to separate source and header files.
class FEMergeSoftwareApplier : public FilterEffectConcreteApplier<FEMerge> {
    using Base = FilterEffectConcreteApplier<FEMerge>;

public:
    using Base::Base;

    bool apply(const Filter&, const FilterEffectVector& inputEffects) override;
};

bool FEMergeSoftwareApplier::apply(const Filter&, const FilterEffectVector& inputEffects)
{
    ASSERT(inputEffects.size() > 0);

    auto resultImage = m_effect.imageBufferResult();
    if (!resultImage)
        return false;

    auto& filterContext = resultImage->context();

    for (auto& in : inputEffects) {
        auto inBuffer = in->imageBufferResult();
        if (!inBuffer)
            continue;
        filterContext.drawImageBuffer(*inBuffer, m_effect.drawingRegionOfInputImage(in->absolutePaintRect()));
    }

    return true;
}

bool FEMerge::platformApplySoftware(const Filter& filter)
{
    return FEMergeSoftwareApplier(*this).apply(filter, inputEffects());
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
