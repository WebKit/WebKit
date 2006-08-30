/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric.seidel@kdemail.net>

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
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#ifdef SVG_SUPPORT
#include "RenderPath.h"

#include "GraphicsContext.h"
#include "RenderSVGContainer.h"
#include "KRenderingDevice.h"
#include "KRenderingFillPainter.h"
#include "KRenderingStrokePainter.h"
#include "SVGStyledElement.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class RenderPath::Private {
public:
    Path path;

    FloatRect fillBBox;
    FloatRect strokeBbox;
    AffineTransform matrix;
    IntRect absoluteBounds;
};        

// RenderPath
RenderPath::RenderPath(RenderStyle* style, SVGStyledElement* node)
    : RenderObject(node), d(new Private)
{
    ASSERT(style != 0);
}

RenderPath::~RenderPath()
{
    delete d;
}

AffineTransform RenderPath::localTransform() const
{
    return d->matrix;
}

void RenderPath::setLocalTransform(const AffineTransform &matrix)
{
    d->matrix = matrix;
}


FloatPoint RenderPath::mapAbsolutePointToLocal(const FloatPoint& point) const
{
    // FIXME: does it make sense to map incoming points with the inverse of the
    // absolute transform? 
    double localX;
    double localY;
    absoluteTransform().invert().map(point.x(), point.y(), &localX, &localY);
    return FloatPoint(localX, localY);
}

bool RenderPath::fillContains(const FloatPoint& point, bool requiresFill) const
{
    if (d->path.isEmpty())
        return false;

    if (requiresFill && !KSVGPainterFactory::fillPaintServer(style(), this))
        return false;

    return path().contains(mapAbsolutePointToLocal(point),
                           KSVGPainterFactory::fillPainter(style(), this).fillRule());
}

FloatRect RenderPath::relativeBBox(bool includeStroke) const
{
    if (d->path.isEmpty())
        return FloatRect();

    if (includeStroke) {
        if (d->strokeBbox.isEmpty())
            d->strokeBbox = strokeBBox();
        return d->strokeBbox;
    }

    if (d->fillBBox.isEmpty())
        d->fillBBox = path().boundingRect();
    return d->fillBBox;
}

void RenderPath::setPath(const Path& newPath)
{
    d->path = newPath;
    d->strokeBbox = FloatRect();
    d->fillBBox = FloatRect();
}

const Path& RenderPath::path() const
{
    return d->path;
}

void RenderPath::layout()
{
    // FIXME: Currently the DOM does all of the % length calculations, so we
    // pretend that one of the attributes of the element has changed on the DOM
    // to force the DOM object to update this render object with new aboslute position values.

    IntRect oldBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (selfNeedsLayout() && checkForRepaint)
        oldBounds = d->absoluteBounds;

    static_cast<SVGStyledElement*>(element())->notifyAttributeChange();

    d->absoluteBounds = getAbsoluteRepaintRect();

    setWidth(d->absoluteBounds.width());
    setHeight(d->absoluteBounds.height());

    if (selfNeedsLayout() && checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldBounds);
        
    setNeedsLayout(false);
}

IntRect RenderPath::getAbsoluteRepaintRect()
{
    FloatRect repaintRect = absoluteTransform().mapRect(relativeBBox(true));
    
    // Filters can expand the bounding box
    KCanvasFilter *filter = getFilterById(document(), style()->svgStyle()->filter().mid(1));
    if (filter)
        repaintRect.unite(filter->filterBBoxForItemBBox(repaintRect));
    
    if (!repaintRect.isEmpty())
        repaintRect.inflate(1); // inflate 1 pixel for antialiasing
    return enclosingIntRect(repaintRect);
}

bool RenderPath::requiresLayer()
{
    return false;
}

short RenderPath::lineHeight(bool b, bool isRootLineBox) const
{
    return static_cast<short>(relativeBBox(true).height());
}

short RenderPath::baselinePosition(bool b, bool isRootLineBox) const
{
    return static_cast<short>(relativeBBox(true).height());
}

void RenderPath::paint(PaintInfo &paintInfo, int parentX, int parentY)
{
    // No one should be transforming us via these.
    //ASSERT(parentX == 0);
    //ASSERT(parentY == 0);

    if (paintInfo.p->paintingDisabled() || (paintInfo.phase != PaintPhaseForeground) || style()->visibility() == HIDDEN || path().isEmpty())
        return;
    
    KRenderingDevice* device = renderingDevice();
    KRenderingDeviceContext *context = device->currentContext();
    bool shouldPopContext = false;
    if (context)
        paintInfo.p->save();
    else {
        // Need to set up KCanvas rendering if it hasn't already been done.
        context = paintInfo.p->createRenderingDeviceContext();
        device->pushContext(context);
        shouldPopContext = true;
    }

    context->concatCTM(localTransform());

    // setup to apply filters
    KCanvasFilter *filter = getFilterById(document(), style()->svgStyle()->filter().mid(1));
    if (filter) {
        filter->prepareFilter(relativeBBox(true));
        context = device->currentContext();
    }

    if (KCanvasClipper *clipper = getClipperById(document(), style()->svgStyle()->clipPath().mid(1)))
        clipper->applyClip(relativeBBox(true));

    if (KCanvasMasker *masker = getMaskerById(document(), style()->svgStyle()->maskElement().mid(1)))
        masker->applyMask(relativeBBox(true));

    context->clearPath();
    
    KRenderingPaintServer *fillPaintServer = KSVGPainterFactory::fillPaintServer(style(), this);
    if (fillPaintServer) {
        context->addPath(path());
        fillPaintServer->setActiveClient(this);
        fillPaintServer->draw(context, this, APPLY_TO_FILL);
    }
    KRenderingPaintServer *strokePaintServer = KSVGPainterFactory::strokePaintServer(style(), this);
    if (strokePaintServer) {
        context->addPath(path()); // path is cleared when filled.
        strokePaintServer->setActiveClient(this);
        strokePaintServer->draw(context, this, APPLY_TO_STROKE);
    }

    drawMarkersIfNeeded(paintInfo.p, paintInfo.r, path());

    // actually apply the filter
    if (filter)
        filter->applyFilter(relativeBBox(true));

    // restore drawing state
    if (!shouldPopContext)
        paintInfo.p->restore();
    else {
        device->popContext();
        delete context;
    }
}

void RenderPath::absoluteRects(DeprecatedValueList<IntRect>& rects, int _tx, int _ty)
{
    rects.append(getAbsoluteRepaintRect());
}

RenderPath::PointerEventsHitRules RenderPath::pointerEventsHitRules()
{
    PointerEventsHitRules hitRules;
    
    switch (style()->svgStyle()->pointerEvents())
    {
        case PE_VISIBLE_PAINTED:
            hitRules.requireVisible = true;
            hitRules.requireFill = true;
            hitRules.requireStroke = true;
            hitRules.canHitFill = true;
            hitRules.canHitStroke = true;
            break;
        case PE_VISIBLE_FILL:
            hitRules.requireVisible = true;
            hitRules.canHitFill = true;
            break;
        case PE_VISIBLE_STROKE:
            hitRules.requireVisible = true;
            hitRules.canHitStroke = true;
            break;
        case PE_VISIBLE:
            hitRules.requireVisible = true;
            hitRules.canHitFill = true;
            hitRules.canHitStroke = true;
            break;
        case PE_PAINTED:
            hitRules.requireFill = true;
            hitRules.requireStroke = true;
            hitRules.canHitFill = true;
            hitRules.canHitStroke = true;
            break;
        case PE_FILL:
            hitRules.canHitFill = true;
            break;
        case PE_STROKE:
            hitRules.canHitStroke = true;
            break;
        case PE_ALL:
            hitRules.canHitFill = true;
            hitRules.canHitStroke = true;
            break;
        case PE_NONE:
            // nothing to do here, defaults are all false.
            break;
    }
    return hitRules;
}

bool RenderPath::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    // We only draw in the forground phase, so we only hit-test then.
    if (hitTestAction != HitTestForeground)
        return false;
    PointerEventsHitRules hitRules = pointerEventsHitRules();
    
    bool isVisible = (style()->visibility() == VISIBLE);
    if (isVisible || !hitRules.requireVisible) {
        bool hasFill = (style()->svgStyle()->fillPaint() && style()->svgStyle()->fillPaint()->paintType() != SVG_PAINTTYPE_NONE);
        bool hasStroke = (style()->svgStyle()->strokePaint() && style()->svgStyle()->strokePaint()->paintType() != SVG_PAINTTYPE_NONE);
        FloatPoint hitPoint(_x,_y);
        if ((hitRules.canHitStroke && (hasStroke || !hitRules.requireStroke) && strokeContains(hitPoint, hitRules.requireStroke))
            || (hitRules.canHitFill && (hasFill || !hitRules.requireFill) && fillContains(hitPoint, hitRules.requireFill))) {
            setInnerNode(info);
            return true;
        }
    }
    return false;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

