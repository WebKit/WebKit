/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#if ENABLE(SVG)
#include "SVGPathSegList.h"

#include "FloatConversion.h"
#include "FloatPoint.h"
#include "PathTraversalState.h"
#include "SVGPathSegClosePath.h"
#include "SVGPathSegMoveto.h"
#include "SVGPathSegLineto.h"
#include "SVGPathSegCurvetoCubic.h"

namespace WebCore {

SVGPathSegList::SVGPathSegList(const QualifiedName& attributeName)
    : SVGList<RefPtr<SVGPathSeg> >(attributeName)
{
}

SVGPathSegList::~SVGPathSegList()
{
}

unsigned SVGPathSegList::getPathSegAtLength(double length, ExceptionCode& ec)
{
    // FIXME : to be useful this will need to support non-normalized SVGPathSegLists
    int len = numberOfItems();
    // FIXME: Eventually this will likely move to a "path applier"-like model, until then PathTraversalState is less useful as we could just use locals
    PathTraversalState traversalState(PathTraversalState::TraversalSegmentAtLength);
    traversalState.m_desiredLength = narrowPrecisionToFloat(length);
    for (int i = 0; i < len; ++i) {
        SVGPathSeg* segment = getItem(i, ec).get();
        if (ec)
            return 0;
        float segmentLength = 0;
        switch (segment->pathSegType()) {
        case PathSegMoveToAbs:
        {
            SVGPathSegMovetoAbs* moveTo = static_cast<SVGPathSegMovetoAbs*>(segment);
            segmentLength = traversalState.moveTo(FloatPoint(moveTo->x(), moveTo->y()));
            break;
        }
        case PathSegLineToAbs:
        {
            SVGPathSegLinetoAbs* lineTo = static_cast<SVGPathSegLinetoAbs*>(segment);
            segmentLength = traversalState.lineTo(FloatPoint(lineTo->x(), lineTo->y()));
            break;
        }
        case PathSegCurveToCubicAbs:
        {
            SVGPathSegCurvetoCubicAbs* curveTo = static_cast<SVGPathSegCurvetoCubicAbs*>(segment);
            segmentLength = traversalState.cubicBezierTo(FloatPoint(curveTo->x1(), curveTo->y1()),
                                      FloatPoint(curveTo->x2(), curveTo->y2()),
                                      FloatPoint(curveTo->x(), curveTo->y()));
            break;
        }
        case PathSegClosePath:
            segmentLength = traversalState.closeSubpath();
            break;
        default:
            ASSERT(false); // FIXME: This only works with normalized/processed path data.
            break;
        }
        traversalState.m_totalLength += segmentLength;
        if ((traversalState.m_action == PathTraversalState::TraversalSegmentAtLength)
            && (traversalState.m_totalLength >= traversalState.m_desiredLength)) {
            return traversalState.m_segmentIndex;
        }
        traversalState.m_segmentIndex++;
    }

    // The SVG spec is unclear as to what to return when the distance is not on the path.
    // WebKit/Opera/FF all return the last path segment if the distance exceeds the actual path length:
    return traversalState.m_segmentIndex ? traversalState.m_segmentIndex - 1 : 0;
}

}

#endif // ENABLE(SVG)
