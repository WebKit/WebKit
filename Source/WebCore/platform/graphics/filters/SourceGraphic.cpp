/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2021-2022 Apple Inc.  All rights reserved.
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
#include "SourceGraphicSoftwareApplier.h"
#include <wtf/text/TextStream.h>

#if USE(CORE_IMAGE)
#include "SourceGraphicCoreImageApplier.h"
#endif

namespace WebCore {

Ref<SourceGraphic> SourceGraphic::create(DestinationColorSpace colorSpace)
{
    return adoptRef(*new SourceGraphic(colorSpace));
}

SourceGraphic::SourceGraphic(DestinationColorSpace colorSpace)
    : FilterEffect(FilterEffect::Type::SourceGraphic, colorSpace)
{
}

OptionSet<FilterRenderingMode> SourceGraphic::supportedFilterRenderingModes() const
{
    OptionSet<FilterRenderingMode> modes = FilterRenderingMode::Software;
#if USE(CORE_IMAGE)
    modes.add(FilterRenderingMode::Accelerated);
#endif
#if USE(GRAPHICS_CONTEXT_FILTERS)
    modes.add(FilterRenderingMode::GraphicsContext);
#endif
    return modes;
}

std::unique_ptr<FilterEffectApplier> SourceGraphic::createAcceleratedApplier() const
{
#if USE(CORE_IMAGE)
    return FilterEffectApplier::create<SourceGraphicCoreImageApplier>(*this);
#else
    return nullptr;
#endif
}

std::unique_ptr<FilterEffectApplier> SourceGraphic::createSoftwareApplier() const
{
    return FilterEffectApplier::create<SourceGraphicSoftwareApplier>(*this);
}

TextStream& SourceGraphic::externalRepresentation(TextStream& ts, FilterRepresentation) const
{
    ts << indent << "[SourceGraphic]\n";
    return ts;
}

} // namespace WebCore
