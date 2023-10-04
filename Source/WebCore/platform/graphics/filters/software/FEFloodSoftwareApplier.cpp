/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
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
#include "FEFloodSoftwareApplier.h"

#include "FEFlood.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"

namespace WebCore {

bool FEFloodSoftwareApplier::apply(const Filter&, const FilterImageVector&, FilterImage& result) const
{
    RefPtr resultImage = result.imageBuffer();
    if (!resultImage)
        return false;

    auto color = m_effect.floodColor().colorWithAlphaMultipliedBy(m_effect.floodOpacity());
    resultImage->context().fillRect(FloatRect(FloatPoint(), result.absoluteImageRect().size()), color);

    return true;
}

} // namespace WebCore
