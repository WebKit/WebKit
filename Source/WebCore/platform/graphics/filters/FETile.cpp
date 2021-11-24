/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
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
#include "FETile.h"

#include "FETileSoftwareApplier.h"
#include "GraphicsContext.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FETile> FETile::create()
{
    return adoptRef(*new FETile());
}

FETile::FETile()
    : FilterEffect(FilterEffect::Type::FETile)
{
}

bool FETile::platformApplySoftware(const Filter& filter)
{
    return FETileSoftwareApplier(*this).apply(filter, inputEffects());
}

TextStream& FETile::externalRepresentation(TextStream& ts, RepresentationType representation) const
{
    ts << indent << "[feTile";
    FilterEffect::externalRepresentation(ts, representation);
    ts << "]\n";

    TextStream::IndentScope indentScope(ts);
    inputEffect(0)->externalRepresentation(ts, representation);

    return ts;
}

} // namespace WebCore
