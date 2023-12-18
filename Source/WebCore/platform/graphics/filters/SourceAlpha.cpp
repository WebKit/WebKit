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
#include "SourceAlpha.h"

#include "ImageBuffer.h"
#include "SourceAlphaSoftwareApplier.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<SourceAlpha> SourceAlpha::create(const DestinationColorSpace& colorSpace)
{
    return adoptRef(*new SourceAlpha(colorSpace));
}

SourceAlpha::SourceAlpha(DestinationColorSpace colorSpace)
    : FilterEffect(FilterEffect::Type::SourceAlpha, colorSpace)
{
}

std::unique_ptr<FilterEffectApplier> SourceAlpha::createSoftwareApplier() const
{
    return FilterEffectApplier::create<SourceAlphaSoftwareApplier>(*this);
}

TextStream& SourceAlpha::externalRepresentation(TextStream& ts, FilterRepresentation) const
{
    ts << indent << "[SourceAlpha]\n";
    return ts;
}

} // namespace WebCore
