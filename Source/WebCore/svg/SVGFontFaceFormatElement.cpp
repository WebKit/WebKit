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
#include "SVGFontFaceFormatElement.h"

#include "SVGElementTypeHelpers.h"
#include "SVGFontFaceElement.h"
#include "SVGNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFontFaceFormatElement);
    
using namespace SVGNames;
    
inline SVGFontFaceFormatElement::SVGFontFaceFormatElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
{
    ASSERT(hasTagName(font_face_formatTag));
}

Ref<SVGFontFaceFormatElement> SVGFontFaceFormatElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFontFaceFormatElement(tagName, document));
}

void SVGFontFaceFormatElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);

    if (!parentNode() || !parentNode()->hasTagName(font_face_uriTag))
        return;
    
    RefPtr ancestor = parentNode()->parentNode();
    if (!ancestor || !ancestor->hasTagName(font_face_srcTag))
        return;
    
    ancestor = ancestor->parentNode();
    if (ancestor && ancestor->hasTagName(font_faceTag))
        downcast<SVGFontFaceElement>(*ancestor).rebuildFontFace();
}

}
