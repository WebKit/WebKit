/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
#include "SVGFontFaceSrcElement.h"

#include "CSSFontFaceSrcValue.h"
#include "CSSValueList.h"
#include "ElementChildIteratorInlines.h"
#include "SVGElementTypeHelpers.h"
#include "SVGFontFaceElement.h"
#include "SVGFontFaceNameElement.h"
#include "SVGFontFaceUriElement.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFontFaceSrcElement);

using namespace SVGNames;
    
inline SVGFontFaceSrcElement::SVGFontFaceSrcElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(font_face_srcTag));
}

Ref<SVGFontFaceSrcElement> SVGFontFaceSrcElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFontFaceSrcElement(tagName, document));
}

Ref<CSSValueList> SVGFontFaceSrcElement::createSrcValue() const
{
    CSSValueListBuilder list;
    for (auto& child : childrenOfType<SVGElement>(*this)) {
        if (auto* element = dynamicDowncast<SVGFontFaceUriElement>(child)) {
            if (auto srcValue = element->createSrcValue(); !srcValue->isEmpty())
                list.append(WTFMove(srcValue));
        } else if (auto* element = dynamicDowncast<SVGFontFaceNameElement>(child)) {
            if (auto srcValue = element->createSrcValue(); !srcValue->isEmpty())
                list.append(WTFMove(srcValue));
        }
    }
    return CSSValueList::createCommaSeparated(WTFMove(list));
}

void SVGFontFaceSrcElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);
    if (is<SVGFontFaceElement>(parentNode()))
        downcast<SVGFontFaceElement>(*parentNode()).rebuildFontFace();
}

}
