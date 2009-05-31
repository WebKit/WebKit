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
#include "FEComponentTransfer.h"

#include "Filter.h"

namespace WebCore {

FEComponentTransfer::FEComponentTransfer(FilterEffect* in, const ComponentTransferFunction& redFunc, 
    const ComponentTransferFunction& greenFunc, const ComponentTransferFunction& blueFunc, const ComponentTransferFunction& alphaFunc)
    : FilterEffect()
    , m_in(in)
    , m_redFunc(redFunc)
    , m_greenFunc(greenFunc)
    , m_blueFunc(blueFunc)
    , m_alphaFunc(alphaFunc)
{
}

PassRefPtr<FEComponentTransfer> FEComponentTransfer::create(FilterEffect* in, const ComponentTransferFunction& redFunc, 
    const ComponentTransferFunction& greenFunc, const ComponentTransferFunction& blueFunc, const ComponentTransferFunction& alphaFunc)
{
    return adoptRef(new FEComponentTransfer(in, redFunc, greenFunc, blueFunc, alphaFunc));
}

ComponentTransferFunction FEComponentTransfer::redFunction() const
{
    return m_redFunc;
}

void FEComponentTransfer::setRedFunction(const ComponentTransferFunction& func)
{
    m_redFunc = func;
}

ComponentTransferFunction FEComponentTransfer::greenFunction() const
{
    return m_greenFunc;
}

void FEComponentTransfer::setGreenFunction(const ComponentTransferFunction& func)
{
    m_greenFunc = func;
}

ComponentTransferFunction FEComponentTransfer::blueFunction() const
{
    return m_blueFunc;
}

void FEComponentTransfer::setBlueFunction(const ComponentTransferFunction& func)
{
    m_blueFunc = func;
}

ComponentTransferFunction FEComponentTransfer::alphaFunction() const
{
    return m_alphaFunc;
}

void FEComponentTransfer::setAlphaFunction(const ComponentTransferFunction& func)
{
    m_alphaFunc = func;
}

void FEComponentTransfer::apply(Filter*)
{
}

void FEComponentTransfer::dump()
{
}

} // namespace WebCore

#endif // ENABLE(FILTERS)
