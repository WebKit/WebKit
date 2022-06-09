/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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
#include "SourceGraphic.h"

#include "Filter.h"
#include "SourceGraphicSoftwareApplier.h"
#include <wtf/text/TextStream.h>

#if USE(CORE_IMAGE)
#include "SourceGraphicCoreImageApplier.h"
#endif

namespace WebCore {

Ref<SourceGraphic> SourceGraphic::create()
{
    return adoptRef(*new SourceGraphic());
}

SourceGraphic::SourceGraphic()
    : FilterEffect(FilterEffect::Type::SourceGraphic)
{
}

std::unique_ptr<FilterEffectApplier> SourceGraphic::createApplier(const Filter& filter) const
{
#if USE(CORE_IMAGE)
    // FIXME: return SourceGraphicCoreImageApplier.
    if (filter.renderingMode() == RenderingMode::Accelerated)
        return FilterEffectApplier::create<SourceGraphicCoreImageApplier>(*this);
#endif
    return FilterEffectApplier::create<SourceGraphicSoftwareApplier>(*this);
}

TextStream& SourceGraphic::externalRepresentation(TextStream& ts, FilterRepresentation) const
{
    ts << indent << "[SourceGraphic]\n";
    return ts;
}

} // namespace WebCore
