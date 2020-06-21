/*
 * Copyright (C) 2007, 2010 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include "SVGViewSpec.h"

#include "Document.h"
#include "SVGElement.h"
#include "SVGFitToViewBox.h"
#include "SVGNames.h"
#include "SVGParserUtilities.h"
#include "SVGTransformList.h"
#include "SVGTransformable.h"

namespace WebCore {

SVGViewSpec::SVGViewSpec(SVGElement& contextElement)
    : SVGFitToViewBox(&contextElement, SVGPropertyAccess::ReadOnly)
    , m_contextElement(makeWeakPtr(contextElement))
    , m_transform(SVGTransformList::create(&contextElement, SVGPropertyAccess::ReadOnly))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::transformAttr, &SVGViewSpec::m_transform>();
    });
}

SVGElement* SVGViewSpec::viewTarget() const
{
    if (!m_contextElement)
        return nullptr;
    auto* element = m_contextElement->treeScope().getElementById(m_viewTargetString);
    if (!is<SVGElement>(element))
        return nullptr;
    return downcast<SVGElement>(element);
}

void SVGViewSpec::reset()
{
    m_viewTargetString = emptyString();
    m_transform->clearItems();
    SVGFitToViewBox::reset();
    SVGZoomAndPan::reset();
}

static const UChar svgViewSpec[] = {'s', 'v', 'g', 'V', 'i', 'e', 'w'};
static const UChar viewBoxSpec[] = {'v', 'i', 'e', 'w', 'B', 'o', 'x'};
static const UChar preserveAspectRatioSpec[] = {'p', 'r', 'e', 's', 'e', 'r', 'v', 'e', 'A', 's', 'p', 'e', 'c', 't', 'R', 'a', 't', 'i', 'o'};
static const UChar transformSpec[] = {'t', 'r', 'a', 'n', 's', 'f', 'o', 'r', 'm'};
static const UChar zoomAndPanSpec[] = {'z', 'o', 'o', 'm', 'A', 'n', 'd', 'P', 'a', 'n'};
static const UChar viewTargetSpec[] =  {'v', 'i', 'e', 'w', 'T', 'a', 'r', 'g', 'e', 't'};

bool SVGViewSpec::parseViewSpec(const String& viewSpec)
{
    auto upconvertedCharacters = StringView(viewSpec).upconvertedCharacters();
    const UChar* currViewSpec = upconvertedCharacters;
    const UChar* end = currViewSpec + viewSpec.length();

    if (currViewSpec >= end || !m_contextElement)
        return false;

    if (!skipString(currViewSpec, end, svgViewSpec, WTF_ARRAY_LENGTH(svgViewSpec)))
        return false;

    if (currViewSpec >= end || *currViewSpec != '(')
        return false;
    currViewSpec++;

    while (currViewSpec < end && *currViewSpec != ')') {
        if (*currViewSpec == 'v') {
            if (skipString(currViewSpec, end, viewBoxSpec, WTF_ARRAY_LENGTH(viewBoxSpec))) {
                if (currViewSpec >= end || *currViewSpec != '(')
                    return false;
                currViewSpec++;
                auto viewBox = SVGFitToViewBox::parseViewBox(currViewSpec, end, false);
                if (!viewBox)
                    return false;
                setViewBox(WTFMove(*viewBox));
                if (currViewSpec >= end || *currViewSpec != ')')
                    return false;
                currViewSpec++;
            } else if (skipString(currViewSpec, end, viewTargetSpec, WTF_ARRAY_LENGTH(viewTargetSpec))) {
                if (currViewSpec >= end || *currViewSpec != '(')
                    return false;
                const UChar* viewTargetStart = ++currViewSpec;
                while (currViewSpec < end && *currViewSpec != ')')
                    currViewSpec++;
                if (currViewSpec >= end)
                    return false;
                m_viewTargetString = String(viewTargetStart, currViewSpec - viewTargetStart);
                currViewSpec++;
            } else
                return false;
        } else if (*currViewSpec == 'z') {
            if (!skipString(currViewSpec, end, zoomAndPanSpec, WTF_ARRAY_LENGTH(zoomAndPanSpec)))
                return false;
            if (currViewSpec >= end || *currViewSpec != '(')
                return false;
            currViewSpec++;
            if (!SVGZoomAndPan::parseZoomAndPan(currViewSpec, end))
                return false;
            if (currViewSpec >= end || *currViewSpec != ')')
                return false;
            currViewSpec++;
        } else if (*currViewSpec == 'p') {
            if (!skipString(currViewSpec, end, preserveAspectRatioSpec, WTF_ARRAY_LENGTH(preserveAspectRatioSpec)))
                return false;
            if (currViewSpec >= end || *currViewSpec != '(')
                return false;
            currViewSpec++;
            SVGPreserveAspectRatioValue preserveAspectRatio;
            if (!preserveAspectRatio.parse(currViewSpec, end, false))
                return false;
            setPreserveAspectRatio(preserveAspectRatio);
            if (currViewSpec >= end || *currViewSpec != ')')
                return false;
            currViewSpec++;
        } else if (*currViewSpec == 't') {
            if (!skipString(currViewSpec, end, transformSpec, WTF_ARRAY_LENGTH(transformSpec)))
                return false;
            if (currViewSpec >= end || *currViewSpec != '(')
                return false;
            currViewSpec++;
            m_transform->parse(currViewSpec, end);
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
