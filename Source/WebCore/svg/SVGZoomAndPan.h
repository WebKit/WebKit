/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGZoomAndPan_h
#define SVGZoomAndPan_h

#include "QualifiedName.h"
#include "SVGNames.h"

namespace WebCore {

enum SVGZoomAndPanType { SVGZoomAndPanUnknown, SVGZoomAndPanDisable, SVGZoomAndPanMagnify };

class SVGZoomAndPan {
public:
    // Forward declare enumerations in the W3C naming scheme, for IDL generation.
    enum {
        SVG_ZOOMANDPAN_UNKNOWN = SVGZoomAndPanUnknown,
        SVG_ZOOMANDPAN_DISABLE = SVGZoomAndPanDisable,
        SVG_ZOOMANDPAN_MAGNIFY = SVGZoomAndPanMagnify
    };

    static bool isKnownAttribute(const QualifiedName&);

    static SVGZoomAndPanType parseFromNumber(unsigned short);

    static bool parse(const UChar*& start, const UChar* end, SVGZoomAndPanType&);
    template<class DerivedClass> static void parseAttribute(DerivedClass&, const QualifiedName&, const AtomicString& value);

private:
    static SVGZoomAndPanType parseAttributeValue(const AtomicString&);
};

inline bool SVGZoomAndPan::isKnownAttribute(const QualifiedName& name)
{
    return name == SVGNames::zoomAndPanAttr;
}

inline SVGZoomAndPanType SVGZoomAndPan::parseFromNumber(unsigned short number)
{
    if (number > SVGZoomAndPanMagnify)
        return SVGZoomAndPanUnknown;
    return static_cast<SVGZoomAndPanType>(number);
}

template<class DerivedClass> void SVGZoomAndPan::parseAttribute(DerivedClass& element, const QualifiedName& name, const AtomicString& value)
{
    if (name == SVGNames::zoomAndPanAttr)
        element.setZoomAndPan(parseAttributeValue(value));
}

} // namespace WebCore

#endif // SVGZoomAndPan_h
