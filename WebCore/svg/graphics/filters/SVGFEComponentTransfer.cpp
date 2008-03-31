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

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFEComponentTransfer.h"
#include "TextStream.h"

namespace WebCore {

SVGFEComponentTransfer::SVGFEComponentTransfer(SVGResourceFilter* filter)
    : SVGFilterEffect(filter)
{
}

SVGComponentTransferFunction SVGFEComponentTransfer::redFunction() const
{
    return m_redFunc;
}

void SVGFEComponentTransfer::setRedFunction(const SVGComponentTransferFunction& func)
{
    m_redFunc = func;
}

SVGComponentTransferFunction SVGFEComponentTransfer::greenFunction() const
{
    return m_greenFunc;
}

void SVGFEComponentTransfer::setGreenFunction(const SVGComponentTransferFunction& func)
{
    m_greenFunc = func;
}

SVGComponentTransferFunction SVGFEComponentTransfer::blueFunction() const
{
    return m_blueFunc;
}

void SVGFEComponentTransfer::setBlueFunction(const SVGComponentTransferFunction& func)
{
    m_blueFunc = func;
}

SVGComponentTransferFunction SVGFEComponentTransfer::alphaFunction() const
{
    return m_alphaFunc;
}

void SVGFEComponentTransfer::setAlphaFunction(const SVGComponentTransferFunction& func)
{
    m_alphaFunc = func;
}

static TextStream& operator<<(TextStream& ts, SVGComponentTransferType t)
{
    switch (t)
    {
        case SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN:
            ts << "UNKNOWN"; break;
        case SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY:
            ts << "IDENTITY"; break;
        case SVG_FECOMPONENTTRANSFER_TYPE_TABLE:
            ts << "TABLE"; break;
        case SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE:
            ts << "DISCRETE"; break;
        case SVG_FECOMPONENTTRANSFER_TYPE_LINEAR:
            ts << "LINEAR"; break;
        case SVG_FECOMPONENTTRANSFER_TYPE_GAMMA:
            ts << "GAMMA"; break;
    }
    return ts;
}

static TextStream& operator<<(TextStream& ts, const SVGComponentTransferFunction &func)
{
    ts << "[type=" << func.type << "]";
    switch (func.type) {
        case SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN:
        case SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY:
            break;
        case SVG_FECOMPONENTTRANSFER_TYPE_TABLE:
        case SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE:
        {
            ts << " [table values=";
            Vector<float>::const_iterator itr=func.tableValues.begin();
            if (itr != func.tableValues.end()) {
                ts << *itr++;
                for (; itr!=func.tableValues.end(); itr++) {
                    ts << " " << *itr;
                }
            }
            ts << "]";
            break;
        }
        case SVG_FECOMPONENTTRANSFER_TYPE_LINEAR:
            ts << " [slope=" << func.slope << "]"
               << " [intercept=" << func.intercept << "]";
            break;
        case SVG_FECOMPONENTTRANSFER_TYPE_GAMMA:
            ts << " [amplitude=" << func.amplitude << "]"
               << " [exponent=" << func.exponent << "]"
               << " [offset=" << func.offset << "]";
            break;
    }
    return ts;
}

TextStream& SVGFEComponentTransfer::externalRepresentation(TextStream& ts) const
{
    ts << "[type=COMPONENT-TRANSFER] ";
    SVGFilterEffect::externalRepresentation(ts);
    ts << " [red func=" << redFunction() << "]"
        << " [green func=" << greenFunction() << "]"
        << " [blue func=" << blueFunction() << "]"
        << " [alpha func=" << alphaFunction() << "]";
    return ts;
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
