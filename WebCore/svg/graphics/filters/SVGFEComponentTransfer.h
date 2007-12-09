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

#ifndef SVGFEComponentTransfer_h
#define SVGFEComponentTransfer_h

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include <wtf/Vector.h>

#include "SVGFilterEffect.h"
#include "SVGFEDisplacementMap.h"

#if PLATFORM(CI)
#ifdef __OBJC__
@class CIImage;
@class CIFilter;
#else
class CIImage;
class CIFilter;
#endif
#endif

namespace WebCore {

enum SVGComponentTransferType {
    SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN  = 0,
    SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY = 1,
    SVG_FECOMPONENTTRANSFER_TYPE_TABLE    = 2,
    SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE = 3,
    SVG_FECOMPONENTTRANSFER_TYPE_LINEAR   = 4,
    SVG_FECOMPONENTTRANSFER_TYPE_GAMMA    = 5
};

struct SVGComponentTransferFunction {
    SVGComponentTransferFunction()
        : type(SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN)
        , slope(0.0f)
        , intercept(0.0f)
        , amplitude(0.0f)
        , exponent(0.0f)
        , offset(0.0f)
    {
    }

    SVGComponentTransferType type;

    float slope;
    float intercept;
    float amplitude;
    float exponent;
    float offset;

    Vector<float> tableValues;
};

class SVGFEComponentTransfer : public SVGFilterEffect {
public:
    SVGFEComponentTransfer(SVGResourceFilter*);

    SVGComponentTransferFunction redFunction() const;
    void setRedFunction(const SVGComponentTransferFunction&);

    SVGComponentTransferFunction greenFunction() const;
    void setGreenFunction(const SVGComponentTransferFunction&);

    SVGComponentTransferFunction blueFunction() const;
    void setBlueFunction(const SVGComponentTransferFunction&);

    SVGComponentTransferFunction alphaFunction() const;
    void setAlphaFunction(const SVGComponentTransferFunction&);

    virtual TextStream& externalRepresentation(TextStream&) const;

#if PLATFORM(CI)
    virtual CIFilter* getCIFilter(const FloatRect& bbox) const;

private:
    CIFilter* getFunctionFilter(SVGChannelSelectorType, CIImage* inputImage) const;
#endif

private:
    SVGComponentTransferFunction m_redFunc;
    SVGComponentTransferFunction m_greenFunc;
    SVGComponentTransferFunction m_blueFunc;
    SVGComponentTransferFunction m_alphaFunc;
};

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)

#endif // SVGFEComponentTransfer_h
