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
#include "SVGTransformable.h"

#include "RegularExpression.h"
#include "AffineTransform.h"
#include "SVGNames.h"
#include "SVGStyledElement.h"
#include "SVGTransformList.h"

namespace WebCore {

SVGTransformable::SVGTransformable() : SVGLocatable()
{
}

SVGTransformable::~SVGTransformable()
{
}

AffineTransform SVGTransformable::getCTM(const SVGElement* element) const
{
    AffineTransform ctm = SVGLocatable::getCTM(element);
    return localMatrix() * ctm;
}

AffineTransform SVGTransformable::getScreenCTM(const SVGElement* element) const
{
    AffineTransform ctm = SVGLocatable::getScreenCTM(element);
    return localMatrix() * ctm;
}

bool SVGTransformable::parseTransformAttribute(SVGTransformList* list, const AtomicString& transform)
{
    // Split string for handling 1 transform statement at a time
    Vector<String> subtransforms = transform.domString().simplifyWhiteSpace().split(')');
    Vector<String>::const_iterator end = subtransforms.end();
    for (Vector<String>::const_iterator it = subtransforms.begin(); it != end; ++it) {
        Vector<String> subtransform = (*it).split('(');

        if (subtransform.size() < 2)
            return false; // invalid transform, ignore.

        subtransform[0] = subtransform[0].stripWhiteSpace().lower();
        subtransform[1] = subtransform[1].simplifyWhiteSpace();
        RegularExpression reg("([-]?\\d*\\.?\\d+(?:e[-]?\\d+)?)");

        int pos = 0;
        Vector<String> params;

        while (pos >= 0) {
            pos = reg.search(subtransform[1].deprecatedString(), pos);
            if (pos != -1) {
                params.append(reg.cap(1));
                pos += reg.matchedLength();
            }
        }
        int nparams = params.size();

        if (nparams < 1)
            return false; // invalid transform, ignore.

        if (subtransform[0].startsWith(";") || subtransform[0].startsWith(","))
            subtransform[0] = subtransform[0].substring(1).stripWhiteSpace();

        SVGTransform* t = new SVGTransform();

        if (subtransform[0] == "rotate" && (nparams == 1 || nparams == 3)) {
            if (nparams == 3)
                t->setRotate(params[0].toDouble(),
                             params[1].toDouble(),
                              params[2].toDouble());
            else
                t->setRotate(params[0].toDouble(), 0, 0);
        } else if (subtransform[0] == "translate" && (nparams == 1 || nparams == 2)) {
            if (nparams == 2)
                t->setTranslate(params[0].toDouble(), params[1].toDouble());
            else // Spec: if only one param given, assume 2nd param to be 0
                t->setTranslate(params[0].toDouble(), 0);
        } else if (subtransform[0] == "scale" && (nparams == 1 || nparams == 2)) {
            if (nparams == 2)
                t->setScale(params[0].toDouble(), params[1].toDouble());
            else // Spec: if only one param given, assume uniform scaling
                t->setScale(params[0].toDouble(), params[0].toDouble());
        } else if (subtransform[0] == "skewx" && nparams == 1)
            t->setSkewX(params[0].toDouble());
        else if (subtransform[0] == "skewy" && nparams == 1)
            t->setSkewY(params[0].toDouble());
        else if (subtransform[0] == "matrix" && nparams == 6) {
            AffineTransform ret(params[0].toDouble(),
                                params[1].toDouble(),
                                params[2].toDouble(),
                                params[3].toDouble(),
                                params[4].toDouble(),
                                params[5].toDouble());
            t->setMatrix(ret);
        } else {
            delete t;
            return false; // failed to parse a valid transform, abort.
        }

        ExceptionCode ec = 0;
        list->appendItem(t, ec);
    }

    return true;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

