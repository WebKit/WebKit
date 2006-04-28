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
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#if SVG_SUPPORT
#include "KCanvasRenderingStyle.h"
#include "KRenderingDevice.h"
#include "KCanvasContainer.h"
#include "SVGStyledElement.h"
#include "GraphicsContext.h"

namespace WebCore {

class KCanvasContainer::Private
{
public:
    Private() : drawsContents(true), slice(false) { }    
    ~Private() { }

    bool drawsContents : 1;
    bool slice : 1;
    QMatrix matrix;
    
    FloatRect viewport;
    FloatRect viewBox;
    KCAlign align;
};

KCanvasContainer::KCanvasContainer(SVGStyledElement *node)
: RenderContainer(node), d(new Private())
{
    setReplaced(true);
}

KCanvasContainer::~KCanvasContainer()
{
    delete d;
}

bool KCanvasContainer::drawsContents() const
{
    return d->drawsContents;
}

void KCanvasContainer::setDrawsContents(bool drawsContents)
{
    d->drawsContents = drawsContents;
}

QMatrix KCanvasContainer::localTransform() const
{
    return d->matrix;
}

void KCanvasContainer::setLocalTransform(const QMatrix &matrix)
{
    d->matrix = matrix;
}

bool KCanvasContainer::canHaveChildren() const
{
    return true;
}
    
bool KCanvasContainer::requiresLayer()
{
    return false;
}

short KCanvasContainer::lineHeight(bool b, bool isRootLineBox) const
{
    return height() + marginTop() + marginBottom();
}

short KCanvasContainer::baselinePosition(bool b, bool isRootLineBox) const
{
    return height() + marginTop() + marginBottom();
}

void KCanvasContainer::calcMinMaxWidth()
{
    KHTMLAssert(!minMaxKnown());
    m_minWidth = m_maxWidth = 0;
    setMinMaxKnown();
}

void KCanvasContainer::layout()
{
    KHTMLAssert(needsLayout());
    KHTMLAssert(minMaxKnown());

    IntRect oldBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint)
        oldBounds = getAbsoluteRepaintRect();

    calcWidth();
    calcHeight();

    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldBounds);
        
    RenderContainer::layout();
}

void KCanvasContainer::paint(PaintInfo &paintInfo, int parentX, int parentY)
{
    if (paintInfo.p->paintingDisabled())
        return;
    
    // No one should be transforming us via these.
    //ASSERT(d->x == 0);
    //ASSERT(d->y == 0);
        
    if (shouldPaintBackgroundOrBorder() && (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection)) 
        paintBoxDecorations(paintInfo, parentX, parentY);
    
    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(paintInfo.p, parentX, parentY, width(), height(), style());
    
    if (paintInfo.phase != WebCore::PaintPhaseForeground || !drawsContents() || style()->visibility() == HIDDEN)
        return;
    
    KCanvasFilter *filter = getFilterById(document(), style()->svgStyle()->filter().mid(1));
    if (!firstChild() && !filter)
        return; // Spec: groups w/o children still may render filter content.
    
    KRenderingDevice* device = renderingDevice();
    KRenderingDeviceContext *deviceContext = device->currentContext();
    bool shouldPopContext = false;
    if (!deviceContext) {
        // I only need to setup for KCanvas rendering if it hasn't already been done.
        deviceContext = paintInfo.p->createRenderingDeviceContext();
        device->pushContext(deviceContext);
        shouldPopContext = true;
    } else
        paintInfo.p->save();
    
    if (parentX != 0 || parentY != 0) {
        // Translate from parent offsets (khtml) to a relative transform (ksvg2/kcanvas)
        deviceContext->concatCTM(QMatrix().translate(parentX, parentY));
        parentX = parentY = 0;
    }
    
    if (!viewport().isEmpty())
        deviceContext->concatCTM(QMatrix().translate(viewport().x(), viewport().y()));
    
    if (!localTransform().isIdentity())
        deviceContext->concatCTM(localTransform());
    
    if (KCanvasClipper *clipper = getClipperById(document(), style()->svgStyle()->clipPath().mid(1)))
        clipper->applyClip(relativeBBox(true));

    if (KCanvasMasker *masker = getMaskerById(document(), style()->svgStyle()->maskElement().mid(1)))
        masker->applyMask(relativeBBox(true));

    float opacity = style()->opacity();
    if (opacity < 1.0f)
        paintInfo.p->beginTransparencyLayer(opacity);

    if (filter)
        filter->prepareFilter(relativeBBox(true));
    
    if (!viewBox().isEmpty())
        deviceContext->concatCTM(viewportTransform());
    
    RenderContainer::paint(paintInfo, 0, 0);
    
    if (filter)
        filter->applyFilter(relativeBBox(true));
    
    if (opacity < 1.0f)
        paintInfo.p->endTransparencyLayer();
    
    // restore drawing state
    if (shouldPopContext) {
        device->popContext();
        delete deviceContext;
    } else
        paintInfo.p->restore();
}

void KCanvasContainer::setViewport(const FloatRect& viewport)
{
    d->viewport = viewport;
}

FloatRect KCanvasContainer::viewport() const
{
   return d->viewport;
}

void KCanvasContainer::setViewBox(const FloatRect& viewBox)
{
    d->viewBox = viewBox;
}

FloatRect KCanvasContainer::viewBox() const
{
    return d->viewBox;
}

void KCanvasContainer::setAlign(KCAlign align)
{
    d->align = align;
}

KCAlign KCanvasContainer::align() const
{
    return d->align;
}

QMatrix KCanvasContainer::viewportTransform() const
{
    if (!viewBox().isEmpty()) {
        FloatRect viewportRect = viewport();
        if (!parent()->isKCanvasContainer())
            viewportRect = FloatRect(viewport().x(), viewport().y(), width(), height());
        return getAspectRatio(viewBox(), viewportRect).qmatrix();
    }
    return QMatrix();
}

IntRect KCanvasContainer::getAbsoluteRepaintRect()
{
    IntRect repaintRect;
    
    for (RenderObject *current = firstChild(); current != 0; current = current->nextSibling())
        repaintRect.unite(current->getAbsoluteRepaintRect());
    
    // Filters can expand the bounding box
    KCanvasFilter *filter = getFilterById(document(), style()->svgStyle()->filter().mid(1));
    if (filter)
        repaintRect.unite(enclosingIntRect(filter->filterBBoxForItemBBox(repaintRect)));

    return repaintRect;
}

QMatrix KCanvasContainer::absoluteTransform() const
{
    return viewportTransform() * RenderContainer::absoluteTransform();
}

bool KCanvasContainer::fillContains(const FloatPoint &p) const
{
    RenderObject *current = firstChild();
    while (current != 0) {
        if (current->isRenderPath() && static_cast<RenderPath *>(current)->fillContains(p))
            return true;
        current = current->nextSibling();
    }

    return false;
}

bool KCanvasContainer::strokeContains(const FloatPoint &p) const
{
    RenderObject *current = firstChild();
    while (current != 0) {
        if (current->isRenderPath() && static_cast<RenderPath *>(current)->strokeContains(p))
            return true;
        current = current->nextSibling();
    }

    return false;
}

FloatRect KCanvasContainer::relativeBBox(bool includeStroke) const
{
    FloatRect rect;
    
    RenderObject *current = firstChild();
    for (; current != 0; current = current->nextSibling()) {
        FloatRect childBBox = current->relativeBBox(includeStroke);
        FloatRect mappedBBox = current->localTransform().mapRect(childBBox);
        rect.unite(mappedBBox);
    }

    return rect;
}

void KCanvasContainer::setSlice(bool slice)
{
    d->slice = slice;
}

bool KCanvasContainer::slice() const
{
    return d->slice;
}

KCanvasMatrix KCanvasContainer::getAspectRatio(const FloatRect& logical, const FloatRect& physical) const
{
    KCanvasMatrix temp;

    float logicX = logical.x();
    float logicY = logical.y();
    float logicWidth = logical.width();
    float logicHeight = logical.height();
    float physWidth = physical.width();
    float physHeight = physical.height();

    float vpar = logicWidth / logicHeight;
    float svgar = physWidth / physHeight;

    if (align() == ALIGN_NONE) {
        temp.scale(physWidth / logicWidth, physHeight / logicHeight);
        temp.translate(-logicX, -logicY);
    } else if ((vpar < svgar && !slice()) || (vpar >= svgar && slice())) {
        temp.scale(physHeight / logicHeight, physHeight / logicHeight);

        if (align() == ALIGN_XMINYMIN || align() == ALIGN_XMINYMID || align() == ALIGN_XMINYMAX)
            temp.translate(-logicX, -logicY);
        else if (align() == ALIGN_XMIDYMIN || align() == ALIGN_XMIDYMID || align() == ALIGN_XMIDYMAX)
            temp.translate(-logicX - (logicWidth - physWidth * logicHeight / physHeight) / 2, -logicY);
        else
            temp.translate(-logicX - (logicWidth - physWidth * logicHeight / physHeight), -logicY);
    } else {
        temp.scale(physWidth / logicWidth, physWidth / logicWidth);

        if (align() == ALIGN_XMINYMIN || align() == ALIGN_XMIDYMIN || align() == ALIGN_XMAXYMIN)
            temp.translate(-logicX, -logicY);
        else if (align() == ALIGN_XMINYMID || align() == ALIGN_XMIDYMID || align() == ALIGN_XMAXYMID)
            temp.translate(-logicX, -logicY - (logicHeight - physHeight * logicWidth / physWidth) / 2);
        else
            temp.translate(-logicX, -logicY - (logicHeight - physHeight * logicWidth / physWidth));
    }

    return temp;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

