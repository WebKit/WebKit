/*
    Copyright (C) 2007 Rob Buis <buis@kde.org>

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
#include "SVGViewSpec.h"

#include "Document.h"
#include "PlatformString.h"
#include "SVGParserUtilities.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGSVGElement.h"
#include "SVGTransformList.h"
#include "SVGTransformable.h"

namespace WebCore {

SVGViewSpec::SVGViewSpec(const SVGSVGElement* contextElement)
    : SVGFitToViewBox()
    , SVGZoomAndPan()
    , m_contextElement(contextElement)
    , m_viewBox(this, SVGNames::viewBoxAttr)
    , m_preserveAspectRatio(this, SVGNames::preserveAspectRatioAttr, SVGPreserveAspectRatio::create())
    , m_transform(SVGTransformList::create(SVGNames::transformAttr))
{
}

SVGViewSpec::~SVGViewSpec()
{
}

void SVGViewSpec::setTransform(const String& transform)
{
    SVGTransformable::parseTransformAttribute(m_transform.get(), transform);
}

void SVGViewSpec::setViewBoxString(const String& viewBox)
{
    float x, y, w, h;
    const UChar* c = viewBox.characters();
    const UChar* end = c + viewBox.length();
    if (!parseViewBox(m_contextElement->document(), c, end, x, y, w, h, false))
        return;
    setViewBoxBaseValue(FloatRect(x, y, w, h));
}

void SVGViewSpec::setPreserveAspectRatioString(const String& preserve)
{
    const UChar* c = preserve.characters();
    const UChar* end = c + preserve.length();
    preserveAspectRatioBaseValue()->parsePreserveAspectRatio(c, end);
}

void SVGViewSpec::setViewTargetString(const String& viewTargetString)
{
    m_viewTargetString = viewTargetString;
}

SVGElement* SVGViewSpec::viewTarget() const
{
    return static_cast<SVGElement*>(m_contextElement->document()->getElementById(m_viewTargetString));
}

static const UChar svgViewSpec[] = {'s', 'v', 'g', 'V', 'i', 'e', 'w'};
static const UChar viewBoxSpec[] = {'v', 'i', 'e', 'w', 'B', 'o', 'x'};
static const UChar preserveAspectRatioSpec[] = {'p', 'r', 'e', 's', 'e', 'r', 'v', 'e', 'A', 's', 'p', 'e', 'c', 't', 'R', 'a', 't', 'i', 'o'};
static const UChar transformSpec[] = {'t', 'r', 'a', 'n', 's', 'f', 'o', 'r', 'm'};
static const UChar zoomAndPanSpec[] = {'z', 'o', 'o', 'm', 'A', 'n', 'd', 'P', 'a', 'n'};
static const UChar viewTargetSpec[] =  {'v', 'i', 'e', 'w', 'T', 'a', 'r', 'g', 'e', 't'};

bool SVGViewSpec::parseViewSpec(const String& viewSpec)
{
    const UChar* currViewSpec = viewSpec.characters();
    const UChar* end = currViewSpec + viewSpec.length();

    if (currViewSpec >= end)
        return false;

    if (!skipString(currViewSpec, end, svgViewSpec, sizeof(svgViewSpec) / sizeof(UChar)))
        return false;

    if (currViewSpec >= end || *currViewSpec != '(' )
        return false;
    currViewSpec++;

    while (currViewSpec < end && *currViewSpec != ')') {
        if (*currViewSpec == 'v') {
            if (skipString(currViewSpec, end, viewBoxSpec, sizeof(viewBoxSpec) / sizeof(UChar))) {
                if (currViewSpec >= end || *currViewSpec != '(')
                    return false;
                currViewSpec++;
                float x, y, w, h;
                if (!parseViewBox(m_contextElement->document(), currViewSpec, end, x, y, w, h, false))
                    return false;
                setViewBoxBaseValue(FloatRect(x, y, w, h));
                if (currViewSpec >= end || *currViewSpec != ')')
                    return false;
                currViewSpec++;
            } else if (skipString(currViewSpec, end, viewTargetSpec, sizeof(viewTargetSpec) / sizeof(UChar))) {
                if (currViewSpec >= end || *currViewSpec != '(')
                    return false;
                const UChar* viewTargetStart = ++currViewSpec;
                while (currViewSpec < end && *currViewSpec != ')')
                    currViewSpec++;
                if (currViewSpec >= end)
                    return false;
                setViewTargetString(String(viewTargetStart, currViewSpec - viewTargetStart));
                currViewSpec++;
            } else
                return false;
        } else if (*currViewSpec == 'z') {
            if (!skipString(currViewSpec, end, zoomAndPanSpec, sizeof(zoomAndPanSpec) / sizeof(UChar)))
                return false;
            if (currViewSpec >= end || *currViewSpec != '(')
                return false;
            currViewSpec++;
            if (!parseZoomAndPan(currViewSpec, end))
                return false;
            if (currViewSpec >= end || *currViewSpec != ')')
                return false;
            currViewSpec++;
        } else if (*currViewSpec == 'p') {
            if (!skipString(currViewSpec, end, preserveAspectRatioSpec, sizeof(preserveAspectRatioSpec) / sizeof(UChar)))
                return false;
            if (currViewSpec >= end || *currViewSpec != '(')
                return false;
            currViewSpec++;
            if (!preserveAspectRatioBaseValue()->parsePreserveAspectRatio(currViewSpec, end, false))
                return false;
            if (currViewSpec >= end || *currViewSpec != ')')
                return false;
            currViewSpec++;
        } else if (*currViewSpec == 't') {
            if (!skipString(currViewSpec, end, transformSpec, sizeof(transformSpec) / sizeof(UChar)))
                return false;
            if (currViewSpec >= end || *currViewSpec != '(')
                return false;
            currViewSpec++;
            SVGTransformable::parseTransformAttribute(m_transform.get(), currViewSpec, end);
            if (currViewSpec >= end || *currViewSpec != ')')
                return false;
            currViewSpec++;
        } else
            return false;

        if (currViewSpec < end && *currViewSpec == ';')
            currViewSpec++;
    }
    
    if (currViewSpec >= end || *currViewSpec != ')')
        return false;

    return true;
}

}

#endif // ENABLE(SVG)
