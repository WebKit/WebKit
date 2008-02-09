/*
   Copyright (C) 2007 Eric Seidel <eric@webkit.org>

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

#if ENABLE(SVG_FONTS)
#include "SVGFontFaceUriElement.h"

#include "CSSFontFaceSrcValue.h"
#include "SVGFontFaceElement.h"
#include "SVGNames.h"
#include "XLinkNames.h"

namespace WebCore {
    
using namespace SVGNames;
    
SVGFontFaceUriElement::SVGFontFaceUriElement(const QualifiedName& tagName, Document* doc)
    : SVGElement(tagName, doc)
{
}

PassRefPtr<CSSFontFaceSrcValue> SVGFontFaceUriElement::srcValue() const
{
    RefPtr<CSSFontFaceSrcValue> src = new CSSFontFaceSrcValue(getAttribute(XLinkNames::hrefAttr), false);
    AtomicString value(getAttribute(formatAttr));
    src->setFormat(value.isEmpty() ? "svg" : value); // Default format
    return src.release();
}

void SVGFontFaceUriElement::childrenChanged(bool changedByParser)
{
    SVGElement::childrenChanged(changedByParser);

    if (!parentNode() || !parentNode()->hasTagName(font_face_srcTag))
        return;
    
    Node* grandParent = parentNode()->parentNode();
    if (grandParent && grandParent->hasTagName(font_faceTag))
        static_cast<SVGFontFaceElement*>(grandParent)->rebuildFontFace();
}

}

#endif // ENABLE(SVG_FONTS)
