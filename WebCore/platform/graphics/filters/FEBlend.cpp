/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(FILTERS)
#include "FEBlend.h"

#include "Filter.h"

namespace WebCore {

FEBlend::FEBlend(FilterEffect* in, FilterEffect* in2, BlendModeType mode)
    : FilterEffect()
    , m_in(in)
    , m_in2(in2)
    , m_mode(mode)
{
}

PassRefPtr<FEBlend> FEBlend::create(FilterEffect* in, FilterEffect* in2, BlendModeType mode)
{
    return adoptRef(new FEBlend(in, in2, mode));
}

FilterEffect* FEBlend::in2() const
{
    return m_in2.get();
}

void FEBlend::setIn2(FilterEffect* in2)
{
    m_in2 = in2;
}

BlendModeType FEBlend::blendMode() const
{
    return m_mode;
}

void FEBlend::setBlendMode(BlendModeType mode)
{
    m_mode = mode;
}

void FEBlend::apply(Filter*)
{
}

void FEBlend::dump()
{
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
