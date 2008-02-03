/*
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 
    This file is part of the WebKit project
 
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
#include "SVGPathSegList.h"

#include "FloatPoint.h"
#include "Path.h"
#include "PathTraversalState.h"
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

unsigned SVGPathSegList::getPathSegAtLength(double)
{
    // FIXME : to be useful this will need to support non-normalized SVGPathSegLists
    ExceptionCode ec = 0;
    int len = numberOfItems();
    // FIXME: Eventually this will likely move to a "path applier"-like model, until then PathTraversalState is less useful as we could just use locals
    PathTraversalState traversalState(PathTraversalState::TraversalSegmentAtLength);
    for (int i = 0; i < len; ++i) {
        SVGPathSeg* segment = getItem(i, ec).get();
        float segmentLength = 0;
        switch (segment->pathSegType()) {
        case SVGPathSeg::PATHSEG_MOVETO_ABS:
        {
            SVGPathSegMovetoAbs* moveTo = static_cast<SVGPathSegMovetoAbs*>(segment);
            segmentLength = traversalState.moveTo(FloatPoint(moveTo->x(), moveTo->y()));
            break;
        }
        case SVGPathSeg::PATHSEG_LINETO_ABS:
        {
            SVGPathSegLinetoAbs* lineTo = static_cast<SVGPathSegLinetoAbs*>(segment);
            segmentLength = traversalState.lineTo(FloatPoint(lineTo->x(), lineTo->y()));
            break;
        }
        case SVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS:
        {
            SVGPathSegCurvetoCubicAbs* curveTo = static_cast<SVGPathSegCurvetoCubicAbs*>(segment);
            segmentLength = traversalState.cubicBezierTo(FloatPoint(curveTo->x1(), curveTo->y1()),
                                      FloatPoint(curveTo->x2(), curveTo->y2()),
                                      FloatPoint(curveTo->x(), curveTo->y()));
            break;
        }
        case SVGPathSeg::PATHSEG_CLOSEPATH:
            segmentLength = traversalState.closeSubpath();
            break;
        default:
            ASSERT(false); // FIXME: This only works with normalized/processed path data.
            break;
        }
        traversalState.m_totalLength += segmentLength;
        if ((traversalState.m_action == PathTraversalState::TraversalSegmentAtLength)
            && (traversalState.m_totalLength > traversalState.m_desiredLength)) {
            return traversalState.m_segmentIndex;
        }
        traversalState.m_segmentIndex++;
    }
    
    return 0; // The SVG spec is unclear as to what to return when the distance is not on the path    
}

Path SVGPathSegList::toPathData()
{
    // FIXME : This should also support non-normalized PathSegLists
    Path pathData;
    ExceptionCode ec = 0;
    int len = numberOfItems();
    for (int i = 0; i < len; ++i) {
        SVGPathSeg* segment = getItem(i, ec).get();;
        switch (segment->pathSegType())
        {
            case SVGPathSeg::PATHSEG_MOVETO_ABS:
            {
                SVGPathSegMovetoAbs* moveTo = static_cast<SVGPathSegMovetoAbs*>(segment);
                pathData.moveTo(FloatPoint(moveTo->x(), moveTo->y()));
                break;
            }
            case SVGPathSeg::PATHSEG_LINETO_ABS:
            {
                SVGPathSegLinetoAbs* lineTo = static_cast<SVGPathSegLinetoAbs*>(segment);
                pathData.addLineTo(FloatPoint(lineTo->x(), lineTo->y()));
                break;
            }
            case SVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS:
            {
                SVGPathSegCurvetoCubicAbs* curveTo = static_cast<SVGPathSegCurvetoCubicAbs*>(segment);
                pathData.addBezierCurveTo(FloatPoint(curveTo->x1(), curveTo->y1()),
                                          FloatPoint(curveTo->x2(), curveTo->y2()),
                                          FloatPoint(curveTo->x(), curveTo->y()));
                break;
            }
            case SVGPathSeg::PATHSEG_CLOSEPATH:
                pathData.closeSubpath();
                break;
            default:
                ASSERT(false); // FIXME: This only works with normalized/processed path data.
                break;
        }
    }
    
    return pathData;
}

}

#endif // ENABLE(SVG)
