/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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
#include "FEBlend.h"

#include "FEBlendNeonApplier.h"
#include "FEBlendSoftwareApplier.h"
#include "ImageBuffer.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<FEBlend> FEBlend::create(BlendMode mode)
{
    return adoptRef(*new FEBlend(mode));
}

FEBlend::FEBlend(BlendMode mode)
    : FilterEffect(FilterEffect::Type::FEBlend)
    , m_mode(mode)
{
}

bool FEBlend::operator==(const FEBlend& other) const
{
    return FilterEffect::operator==(other) && m_mode == other.m_mode;
}

bool FEBlend::setBlendMode(BlendMode mode)
{
    if (m_mode == mode)
        return false;
    m_mode = mode;
    return true;
}

std::unique_ptr<FilterEffectApplier> FEBlend::createSoftwareApplier() const
{
#if HAVE(ARM_NEON_INTRINSICS)
    return FilterEffectApplier::create<FEBlendNeonApplier>(*this);
#else
    return FilterEffectApplier::create<FEBlendSoftwareApplier>(*this);
#endif
}

TextStream& FEBlend::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    ts << indent << "[feBlend";
    FilterEffect::externalRepresentation(ts, representation);

    ts << " mode=\"" << (m_mode == BlendMode::Normal ? "normal"_s : compositeOperatorName(CompositeOperator::SourceOver, m_mode));

    ts << "\"]\n";
    return ts;
}

} // namespace WebCore
