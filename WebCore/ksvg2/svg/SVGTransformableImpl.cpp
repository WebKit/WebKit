/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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
#if SVG_SUPPORT
#include <qregexp.h>
#include <qstringlist.h>
#include <kxmlcore/RefPtr.h>

#include <kdom/core/AttrImpl.h>

#include <kcanvas/KCanvas.h>
#include <kcanvas/RenderPath.h>

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGMatrixImpl.h"
#include "SVGTransformableImpl.h"
#include "SVGStyledElementImpl.h"
#include "SVGDOMImplementationImpl.h"
#include "SVGAnimatedTransformListImpl.h"
#include "ksvg.h"

using namespace KSVG;

SVGTransformableImpl::SVGTransformableImpl() : SVGLocatableImpl()
{
}

SVGTransformableImpl::~SVGTransformableImpl()
{
}

void SVGTransformableImpl::parseTransformAttribute(SVGTransformListImpl *list, const KDOM::AtomicString& transform)
{
    // Split string for handling 1 transform statement at a time
    QStringList subtransforms = QStringList::split(')', transform.qstring().simplifyWhiteSpace());
    QStringList::ConstIterator it = subtransforms.begin();
    QStringList::ConstIterator end = subtransforms.end();
    for (; it != end; ++it) {
        QStringList subtransform = QStringList::split('(', (*it));
        
        if (subtransform.count() < 2)
            break; // invalid transform, ignore.

        subtransform[0] = subtransform[0].stripWhiteSpace().lower();
        subtransform[1] = subtransform[1].simplifyWhiteSpace();
        QRegExp reg("([-]?\\d*\\.?\\d+(?:e[-]?\\d+)?)");

        int pos = 0;
        QStringList params;
        
        while (pos >= 0) {
            pos = reg.search(subtransform[1], pos);
            if (pos != -1) {
                params += reg.cap(1);
                pos += reg.matchedLength();
            }
        }
        
        if (params.count() < 1)
            break;

        if (subtransform[0].startsWith(";") || subtransform[0].startsWith(","))
            subtransform[0] = subtransform[0].right(subtransform[0].length() - 1);

        RefPtr<SVGTransformImpl> t(new SVGTransformImpl());

        if (subtransform[0] == "rotate") {
            if (params.count() == 3)
                t->setRotate(params[0].toDouble(),
                             params[1].toDouble(),
                              params[2].toDouble());
            else if (params.count() == 1)
                t->setRotate(params[0].toDouble(), 0, 0);
        } else if (subtransform[0] == "translate") {
            if (params.count() == 2)
                t->setTranslate(params[0].toDouble(), params[1].toDouble());
            else if (params.count() == 1) // Spec: if only one param given, assume 2nd param to be 0
                t->setTranslate(params[0].toDouble(), 0);
        } else if (subtransform[0] == "scale") {
            if (params.count() == 2)
                t->setScale(params[0].toDouble(), params[1].toDouble());
            else if (params.count() == 1) // Spec: if only one param given, assume uniform scaling
                t->setScale(params[0].toDouble(), params[0].toDouble());
        } else if (subtransform[0] == "skewx" && (params.count() == 1))
            t->setSkewX(params[0].toDouble());
        else if (subtransform[0] == "skewy" && (params.count() == 1))
            t->setSkewY(params[0].toDouble());
        else if (subtransform[0] == "matrix" && (params.count() == 6)) {
            SVGMatrixImpl *ret = new SVGMatrixImpl(params[0].toDouble(),
                                                   params[1].toDouble(),
                                                   params[2].toDouble(),
                                                   params[3].toDouble(),
                                                   params[4].toDouble(),
                                                   params[5].toDouble());
            t->setMatrix(ret);
        }
        
        if (t->type() == SVG_TRANSFORM_UNKNOWN)
            break; // failed to parse a valid transform, abort.
        
        list->appendItem(t.release().release());
    }
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

