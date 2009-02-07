/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGPreserveAspectRatio.h"

#include "TransformationMatrix.h"
#include "SVGParserUtilities.h"
#include "SVGSVGElement.h"

namespace WebCore {

SVGPreserveAspectRatio::SVGPreserveAspectRatio()
    : m_align(SVG_PRESERVEASPECTRATIO_XMIDYMID)
    , m_meetOrSlice(SVG_MEETORSLICE_MEET)
{
    // FIXME: Should the two values default to UNKNOWN instead?
}

SVGPreserveAspectRatio::~SVGPreserveAspectRatio()
{
}

void SVGPreserveAspectRatio::setAlign(unsigned short align)
{
    m_align = align;
}

unsigned short SVGPreserveAspectRatio::align() const
{
    return m_align;
}

void SVGPreserveAspectRatio::setMeetOrSlice(unsigned short meetOrSlice)
{
    m_meetOrSlice = meetOrSlice;
}

unsigned short SVGPreserveAspectRatio::meetOrSlice() const
{
    return m_meetOrSlice;
}

bool SVGPreserveAspectRatio::parsePreserveAspectRatio(const UChar*& currParam, const UChar* end, bool validate)
{
    SVGPreserveAspectRatioType align = SVG_PRESERVEASPECTRATIO_NONE;
    SVGMeetOrSliceType meetOrSlice = SVG_MEETORSLICE_MEET;
    bool ret = false;

    if (!skipOptionalSpaces(currParam, end))
        goto bail_out;

    if (*currParam == 'd') {
        if (!skipString(currParam, end, "defer"))
            goto bail_out;
        // FIXME: We just ignore the "defer" here.
        if (!skipOptionalSpaces(currParam, end))
            goto bail_out;
    }

    if (*currParam == 'n') {
        if (!skipString(currParam, end, "none"))
            goto bail_out;
        skipOptionalSpaces(currParam, end);
    } else if (*currParam == 'x') {
        if ((end - currParam) < 8)
            goto bail_out;
        if (currParam[1] != 'M' || currParam[4] != 'Y' || currParam[5] != 'M')
            goto bail_out;
        if (currParam[2] == 'i') {
            if (currParam[3] == 'n') {
                if (currParam[6] == 'i') {
                    if (currParam[7] == 'n')
                        align = SVG_PRESERVEASPECTRATIO_XMINYMIN;
                    else if (currParam[7] == 'd')
                        align = SVG_PRESERVEASPECTRATIO_XMINYMID;
                    else
                        goto bail_out;
                } else if (currParam[6] == 'a' && currParam[7] == 'x')
                     align = SVG_PRESERVEASPECTRATIO_XMINYMAX;
                else
                     goto bail_out;
             } else if (currParam[3] == 'd') {
                if (currParam[6] == 'i') {
                    if (currParam[7] == 'n')
                        align = SVG_PRESERVEASPECTRATIO_XMIDYMIN;
                    else if (currParam[7] == 'd')
                        align = SVG_PRESERVEASPECTRATIO_XMIDYMID;
                    else
                        goto bail_out;
                } else if (currParam[6] == 'a' && currParam[7] == 'x')
                    align = SVG_PRESERVEASPECTRATIO_XMIDYMAX;
                else
                    goto bail_out;
            } else
                goto bail_out;
        } else if (currParam[2] == 'a' && currParam[3] == 'x') {
            if (currParam[6] == 'i') {
                if (currParam[7] == 'n')
                    align = SVG_PRESERVEASPECTRATIO_XMAXYMIN;
                else if (currParam[7] == 'd')
                    align = SVG_PRESERVEASPECTRATIO_XMAXYMID;
                else
                    goto bail_out;
            } else if (currParam[6] == 'a' && currParam[7] == 'x')
                align = SVG_PRESERVEASPECTRATIO_XMAXYMAX;
            else
                goto bail_out;
        } else
            goto bail_out;
        currParam += 8;
        skipOptionalSpaces(currParam, end);
    } else
        goto bail_out;

    if (currParam < end) {
        if (*currParam == 'm') {
            if (!skipString(currParam, end, "meet"))
                goto bail_out;
            skipOptionalSpaces(currParam, end);
        } else if (*currParam == 's') {
            if (!skipString(currParam, end, "slice"))
                goto bail_out;
            skipOptionalSpaces(currParam, end);
            if (align != SVG_PRESERVEASPECTRATIO_NONE)
                meetOrSlice = SVG_MEETORSLICE_SLICE;    
        }
    }

    if (end != currParam && validate) {
bail_out:
        // FIXME: Should the two values be set to UNKNOWN instead?
        align = SVG_PRESERVEASPECTRATIO_NONE;
        meetOrSlice = SVG_MEETORSLICE_MEET;
    } else
        ret = true;

    if (m_align == align && m_meetOrSlice == meetOrSlice)
        return ret;

    m_align = align;
    m_meetOrSlice = meetOrSlice;
    return ret;
}

TransformationMatrix SVGPreserveAspectRatio::getCTM(double logicX, double logicY,
                                               double logicWidth, double logicHeight,
                                               double /*physX*/, double /*physY*/,
                                               double physWidth, double physHeight)
{
    TransformationMatrix temp;

    if (align() == SVG_PRESERVEASPECTRATIO_UNKNOWN)
        return temp;

    double vpar = logicWidth / logicHeight;
    double svgar = physWidth / physHeight;

    if (align() == SVG_PRESERVEASPECTRATIO_NONE) {
        temp.scaleNonUniform(physWidth / logicWidth, physHeight / logicHeight);
        temp.translate(-logicX, -logicY);
    } else if (vpar < svgar && (meetOrSlice() == SVG_MEETORSLICE_MEET) || vpar >= svgar && (meetOrSlice() == SVG_MEETORSLICE_SLICE)) {
        temp.scaleNonUniform(physHeight / logicHeight, physHeight / logicHeight);

        if (align() == SVG_PRESERVEASPECTRATIO_XMINYMIN || align() == SVG_PRESERVEASPECTRATIO_XMINYMID || align() == SVG_PRESERVEASPECTRATIO_XMINYMAX)
            temp.translate(-logicX, -logicY);
        else if (align() == SVG_PRESERVEASPECTRATIO_XMIDYMIN || align() == SVG_PRESERVEASPECTRATIO_XMIDYMID || align() == SVG_PRESERVEASPECTRATIO_XMIDYMAX)
            temp.translate(-logicX - (logicWidth - physWidth * logicHeight / physHeight) / 2, -logicY);
        else
            temp.translate(-logicX - (logicWidth - physWidth * logicHeight / physHeight), -logicY);
    } else {
        temp.scaleNonUniform(physWidth / logicWidth, physWidth / logicWidth);

        if (align() == SVG_PRESERVEASPECTRATIO_XMINYMIN || align() == SVG_PRESERVEASPECTRATIO_XMIDYMIN || align() == SVG_PRESERVEASPECTRATIO_XMAXYMIN)
            temp.translate(-logicX, -logicY);
        else if (align() == SVG_PRESERVEASPECTRATIO_XMINYMID || align() == SVG_PRESERVEASPECTRATIO_XMIDYMID || align() == SVG_PRESERVEASPECTRATIO_XMAXYMID)
            temp.translate(-logicX, -logicY - (logicHeight - physHeight * logicWidth / physWidth) / 2);
        else
            temp.translate(-logicX, -logicY - (logicHeight - physHeight * logicWidth / physWidth));
    }

    return temp;
}

String SVGPreserveAspectRatio::valueAsString() const
{
    String result;

    switch ((SVGPreserveAspectRatioType) align()) {
    default:
    case SVG_PRESERVEASPECTRATIO_NONE:
        result = "none";
        break;
    case SVG_PRESERVEASPECTRATIO_XMINYMIN:
        result = "xMinYMin";
        break;
    case SVG_PRESERVEASPECTRATIO_XMIDYMIN:
        result = "xMidYMin";
        break;
    case SVG_PRESERVEASPECTRATIO_XMAXYMIN:
        result = "xMaxYMin";
        break;
    case SVG_PRESERVEASPECTRATIO_XMINYMID:
        result = "xMinYMid";
        break;
    case SVG_PRESERVEASPECTRATIO_XMIDYMID:
        result = "xMidYMid";
        break;
    case SVG_PRESERVEASPECTRATIO_XMAXYMID:
        result = "xMaxYMid";
        break;
    case SVG_PRESERVEASPECTRATIO_XMINYMAX:
        result = "xMinYMax";
        break;
    case SVG_PRESERVEASPECTRATIO_XMIDYMAX:
        result = "xMidYMax";
        break;
    case SVG_PRESERVEASPECTRATIO_XMAXYMAX:
        result = "xMaxYMax";
        break;
    };

    switch ((SVGMeetOrSliceType) meetOrSlice()) {
    default:
    case SVG_MEETORSLICE_UNKNOWN:
        break;
    case SVG_MEETORSLICE_MEET:
        result += " meet";
        break;
    case SVG_MEETORSLICE_SLICE:
        result += " slice";
        break;
    };

    return result;
}

}

#endif // ENABLE(SVG)
