/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#ifdef SVG_SUPPORT
#include "SVGPreserveAspectRatio.h"

#include "SVGParserUtilities.h"
#include "SVGSVGElement.h"

namespace WebCore {

SVGPreserveAspectRatio::SVGPreserveAspectRatio(const SVGStyledElement* context)
    : Shared<SVGPreserveAspectRatio>()
    , m_align(SVG_PRESERVEASPECTRATIO_XMIDYMID)
    , m_meetOrSlice(SVG_MEETORSLICE_MEET)
    , m_context(context)
{
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

static const UChar deferDesc[] =  {'d', 'e', 'f', 'e', 'r'};
static const UChar noneDesc[] =  {'n', 'o', 'n', 'e'};
static const UChar meetDesc[] =  {'m', 'e', 'e', 't'};
static const UChar sliceDesc[] =  {'s', 'l', 'i', 'c', 'e'};
static const UChar xMinYMinDesc[] =  {'x', 'M', 'i', 'n', 'Y', 'M', 'i', 'n'};
static const UChar xMinYMidDesc[] =  {'x', 'M', 'i', 'n', 'Y', 'M', 'i', 'd'};
static const UChar xMinYMaxDesc[] =  {'x', 'M', 'i', 'n', 'Y', 'M', 'a', 'x'};
static const UChar xMidYMinDesc[] =  {'x', 'M', 'i', 'd', 'Y', 'M', 'i', 'n'};
static const UChar xMidYMidDesc[] =  {'x', 'M', 'i', 'd', 'Y', 'M', 'i', 'd'};
static const UChar xMidYMaxDesc[] =  {'x', 'M', 'i', 'd', 'Y', 'M', 'a', 'x'};
static const UChar xMaxYMinDesc[] =  {'x', 'M', 'a', 'x', 'Y', 'M', 'i', 'n'};
static const UChar xMaxYMidDesc[] =  {'x', 'M', 'a', 'x', 'Y', 'M', 'i', 'd'};
static const UChar xMaxYMaxDesc[] =  {'x', 'M', 'a', 'x', 'Y', 'M', 'a', 'x'};

void SVGPreserveAspectRatio::parsePreserveAspectRatio(const String& string)
{
    // Spec: set the defaults
    SVGPreserveAspectRatioType align = SVG_PRESERVEASPECTRATIO_NONE;
    SVGMeetOrSliceType meetOrSlice = SVG_MEETORSLICE_MEET;

    const UChar* currParam = string.characters();
    const UChar* end = currParam + string.length();

    if (!skipOptionalSpaces(currParam, end))
        goto bail_out;

    if (*currParam == 'd') {
        if (!checkString(currParam, end, deferDesc, sizeof(deferDesc) / sizeof(UChar)))
            goto bail_out;
        skipOptionalSpaces(currParam, end);
    }

    if (*currParam == 'n') {
        if (!checkString(currParam, end, noneDesc, sizeof(noneDesc) / sizeof(UChar)))
            goto bail_out;
        skipOptionalSpaces(currParam, end);
        align = SVG_PRESERVEASPECTRATIO_NONE;
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
    }

    if (*currParam == 'm') {
        if (!checkString(currParam, end, meetDesc, sizeof(meetDesc) / sizeof(UChar)))
            goto bail_out;
        skipOptionalSpaces(currParam, end);
        meetOrSlice = SVG_MEETORSLICE_MEET;
    } else if (*currParam == 's') {
        if (!checkString(currParam, end, sliceDesc, sizeof(sliceDesc) / sizeof(UChar)))
            goto bail_out;
        skipOptionalSpaces(currParam, end);
        if (m_align != SVG_PRESERVEASPECTRATIO_NONE)
            meetOrSlice = SVG_MEETORSLICE_SLICE;    
    }

    if (end != currParam)
        goto bail_out; // FIXME: may need to print an error here

    setAlign(align);
    setMeetOrSlice(meetOrSlice);

    if (m_context)
        m_context->notifyAttributeChange();
    return;

bail_out:
    setAlign(SVG_PRESERVEASPECTRATIO_NONE);
    setMeetOrSlice(SVG_MEETORSLICE_MEET);

    if (m_context)
        m_context->notifyAttributeChange();
}

AffineTransform SVGPreserveAspectRatio::getCTM(float logicX, float logicY,
                                               float logicWidth, float logicHeight,
                                               float /*physX*/, float /*physY*/,
                                               float physWidth, float physHeight)
{
    AffineTransform temp;

    if (align() == SVG_PRESERVEASPECTRATIO_UNKNOWN)
        return temp;

    float vpar = logicWidth / logicHeight;
    float svgar = physWidth / physHeight;

    if (align() == SVG_PRESERVEASPECTRATIO_NONE) {
        temp.scale(physWidth / logicWidth, physHeight / logicHeight);
        temp.translate(-logicX, -logicY);
    } else if (vpar < svgar && (meetOrSlice() == SVG_MEETORSLICE_MEET) || vpar >= svgar && (meetOrSlice() == SVG_MEETORSLICE_SLICE)) {
        temp.scale(physHeight / logicHeight, physHeight / logicHeight);

        if (align() == SVG_PRESERVEASPECTRATIO_XMINYMIN || align() == SVG_PRESERVEASPECTRATIO_XMINYMID || align() == SVG_PRESERVEASPECTRATIO_XMINYMAX)
            temp.translate(-logicX, -logicY);
        else if (align() == SVG_PRESERVEASPECTRATIO_XMIDYMIN || align() == SVG_PRESERVEASPECTRATIO_XMIDYMID || align() == SVG_PRESERVEASPECTRATIO_XMIDYMAX)
            temp.translate(-logicX - (logicWidth - physWidth * logicHeight / physHeight) / 2, -logicY);
        else
            temp.translate(-logicX - (logicWidth - physWidth * logicHeight / physHeight), -logicY);
    } else {
        temp.scale(physWidth / logicWidth, physWidth / logicWidth);

        if (align() == SVG_PRESERVEASPECTRATIO_XMINYMIN || align() == SVG_PRESERVEASPECTRATIO_XMIDYMIN || align() == SVG_PRESERVEASPECTRATIO_XMAXYMIN)
            temp.translate(-logicX, -logicY);
        else if (align() == SVG_PRESERVEASPECTRATIO_XMINYMID || align() == SVG_PRESERVEASPECTRATIO_XMIDYMID || align() == SVG_PRESERVEASPECTRATIO_XMAXYMID)
            temp.translate(-logicX, -logicY - (logicHeight - physHeight * logicWidth / physWidth) / 2);
        else
            temp.translate(-logicX, -logicY - (logicHeight - physHeight * logicWidth / physWidth));
    }

    return temp;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

