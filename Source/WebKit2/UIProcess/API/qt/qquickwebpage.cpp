/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "qquickwebpage_p.h"

#include "LayerTreeHostProxy.h"
#include "QtWebPageEventHandler.h"
#include "TransformationMatrix.h"
#include "qquickwebpage_p_p.h"
#include "qquickwebview_p.h"
#include <QPolygonF>
#include <QtQuick/QQuickCanvas>
#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGMaterial>
#include <private/qsgrendernode_p.h>

QQuickWebPage::QQuickWebPage(QQuickWebView* viewportItem)
    : QQuickItem(viewportItem)
    , d(new QQuickWebPagePrivate(this, viewportItem))
{
    setFlag(ItemHasContents);
    setClip(true);

    // We do the transform from the top left so the viewport can assume the position 0, 0
    // is always where rendering starts.
    setTransformOrigin(TopLeft);
}

QQuickWebPage::~QQuickWebPage()
{
    delete d;
}

QQuickWebPagePrivate::QQuickWebPagePrivate(QQuickWebPage* q, QQuickWebView* viewportItem)
    : q(q)
    , viewportItem(viewportItem)
    , webPageProxy(0)
    , paintingIsInitialized(false)
    , m_paintNode(0)
    , contentsScale(1)
{
}

void QQuickWebPagePrivate::initialize(WebKit::WebPageProxy* webPageProxy)
{
    this->webPageProxy = webPageProxy;
    eventHandler.reset(new QtWebPageEventHandler(toAPI(webPageProxy), q, viewportItem));
}

void QQuickWebPagePrivate::setDrawingAreaSize(const QSize& size)
{
    DrawingAreaProxy* drawingArea = webPageProxy->drawingArea();
    if (!drawingArea)
        return;
    drawingArea->setSize(WebCore::IntSize(size), WebCore::IntSize());
}

void QQuickWebPagePrivate::paint(QPainter* painter)
{
    if (webPageProxy->drawingArea())
        webPageProxy->drawingArea()->paintLayerTree(painter);
}

void QQuickWebPagePrivate::paintToCurrentGLContext(const QTransform& transform, float opacity, const QRectF& clipRect)
{
    if (!q->isVisible())
        return;

    if (!clipRect.isValid())
        return;

    DrawingAreaProxy* drawingArea = webPageProxy->drawingArea();
    if (!drawingArea)
        return;

    drawingArea->paintToCurrentGLContext(QTransform(transform).scale(contentsScale, contentsScale), opacity, clipRect);
}

struct PageProxyNode : public QSGRenderNode {
    PageProxyNode(QQuickWebPagePrivate* page)
        : m_pagePrivate(page)
    {
    }

    virtual StateFlags changedStates()
    {
        return StateFlags(StencilState) | ColorState | BlendState;
    }

    virtual void render(const RenderState&)
    {
        if (!m_pagePrivate)
            return;
        QTransform transform = matrix() ? matrix()->toTransform() : QTransform();

        // FIXME: Support non-rectangular clippings.
        m_pagePrivate->paintToCurrentGLContext(transform, inheritedOpacity(), clipRect());
    }

    ~PageProxyNode()
    {
        if (m_pagePrivate)
            m_pagePrivate->resetPaintNode();
    }

    QRectF clipRect() const
    {
        // Start with an invalid rect.
        QRectF resultRect(0, 0, -1, -1);

        for (const QSGClipNode* clip = clipList(); clip; clip = clip->clipList()) {
            QMatrix4x4 clipMatrix;
            if (clip->matrix())
                clipMatrix = *clip->matrix();
            QRectF currentClip;

            if (clip->isRectangular())
                currentClip = clipMatrix.mapRect(clip->clipRect());
            else {
                const QSGGeometry* geometry = clip->geometry();
                // Assume here that clipNode has only coordinate data.
                const QSGGeometry::Point2D* geometryPoints = geometry->vertexDataAsPoint2D();

                // Clip region should be at least triangle to make valid clip.
                if (geometry->vertexCount() < 3)
                    continue;

                QPolygonF polygon;

                for (int i = 0; i < geometry->vertexCount(); i++)
                    polygon.append(clipMatrix.map(QPoint(geometryPoints[i].x, geometryPoints[i].y)));
                currentClip = polygon.boundingRect();
            }

            if (currentClip.isEmpty())
                continue;

            if (resultRect.isValid())
                resultRect &= currentClip;
            else
                resultRect = currentClip;
        }

        return resultRect;
    }

    QQuickWebPagePrivate* m_pagePrivate;
};

QSGNode* QQuickWebPage::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    if (!(flags() & ItemHasContents)) {
        if (oldNode)
            delete oldNode;
        return 0;
    }

    PageProxyNode* proxyNode = static_cast<PageProxyNode*>(oldNode);
    if (!proxyNode) {
        proxyNode = new PageProxyNode(d);
        d->m_paintNode = proxyNode;
    }

    return proxyNode;
}

QtWebPageEventHandler* QQuickWebPage::eventHandler() const
{
    return d->eventHandler.data();
}

void QQuickWebPage::setContentsSize(const QSizeF& size)
{
    if (size.isEmpty() || d->contentsSize == size)
        return;

    d->contentsSize = size;
    d->updateSize();
    d->setDrawingAreaSize(d->contentsSize.toSize());
}

const QSizeF& QQuickWebPage::contentsSize() const
{
    return d->contentsSize;
}

void QQuickWebPage::setContentsScale(qreal scale)
{
    ASSERT(scale > 0);
    d->contentsScale = scale;
    d->updateSize();
}

qreal QQuickWebPage::contentsScale() const
{
    ASSERT(d->contentsScale > 0);
    return d->contentsScale;
}

QTransform QQuickWebPage::transformFromItem() const
{
    return transformToItem().inverted();
}

QTransform QQuickWebPage::transformToItem() const
{
    QPointF pos = d->viewportItem->pageItemPos();
    return QTransform(d->contentsScale, 0, 0, 0, d->contentsScale, 0, pos.x(), pos.y(), 1);
}

void QQuickWebPagePrivate::updateSize()
{
    QSizeF scaledSize = contentsSize * contentsScale;
    q->setSize(scaledSize);
    viewportItem->updateContentsSize(scaledSize);
}

void QQuickWebPagePrivate::resetPaintNode()
{
    m_paintNode = 0;
    DrawingAreaProxy* drawingArea = webPageProxy->drawingArea();
    if (drawingArea && drawingArea->layerTreeHostProxy())
        drawingArea->layerTreeHostProxy()->purgeGLResources();
}

QQuickWebPagePrivate::~QQuickWebPagePrivate()
{
    if (m_paintNode)
        static_cast<PageProxyNode*>(m_paintNode)->m_pagePrivate = 0;
}

#include "moc_qquickwebpage_p.cpp"
