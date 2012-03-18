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
#include "WebLayerTreeRenderer.h"
#include "qquickwebpage_p_p.h"
#include "qquickwebview_p.h"
#include <QPolygonF>
#include <QtQuick/QQuickCanvas>
#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGMaterial>
#include <QtQuick/QSGSimpleRectNode>
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

class ContentsSGNode : public QSGRenderNode {
public:
    ContentsSGNode(PassRefPtr<WebLayerTreeRenderer> renderer)
        : m_renderer(renderer)
        , m_scale(1)
    {
    }

    virtual StateFlags changedStates()
    {
        return StateFlags(StencilState) | ColorState | BlendState;
    }

    virtual void render(const RenderState&)
    {
        QMatrix4x4 renderMatrix = matrix() ? *matrix() : QMatrix4x4();

        // Have to apply render scale manualy because it is not applied on page item.
        // http://trac.webkit.org/changeset/104450
        renderMatrix.scale(m_scale);

        // FIXME: Support non-rectangular clippings.
        layerTreeRenderer()->paintToCurrentGLContext(renderMatrix, inheritedOpacity(), clipRect());
    }

    ~ContentsSGNode()
    {
        layerTreeRenderer()->purgeGLResources();
    }

    WebLayerTreeRenderer* layerTreeRenderer() const { return m_renderer.get(); }
    void setScale(float scale) { m_scale = scale; }

private:
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

    RefPtr<WebLayerTreeRenderer> m_renderer;
    float m_scale;
};

class BackgroundSGNode : public QSGSimpleRectNode {
public:
    BackgroundSGNode()
        : m_contentsNode(0)
    {
    }

    ContentsSGNode* contentsNode(PassRefPtr<WebLayerTreeRenderer> renderer)
    {
        if (m_contentsNode && m_contentsNode->layerTreeRenderer() == renderer)
            return m_contentsNode;

        delete m_contentsNode;

        m_contentsNode = new ContentsSGNode(renderer);
        appendChildNode(m_contentsNode);
        return m_contentsNode;
    }

private:
    ContentsSGNode* m_contentsNode;
};

QSGNode* QQuickWebPage::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    if (!d->webPageProxy->drawingArea())
        return oldNode;

    LayerTreeHostProxy* layerTreeHostProxy = d->webPageProxy->drawingArea()->layerTreeHostProxy();
    WebLayerTreeRenderer* renderer = layerTreeHostProxy->layerTreeRenderer();

    BackgroundSGNode* backgroundNode = static_cast<BackgroundSGNode*>(oldNode);
    if (!backgroundNode)
        backgroundNode = new BackgroundSGNode();
    ContentsSGNode* contentsNode = backgroundNode->contentsNode(renderer);
    renderer->syncRemoteContent();

    contentsNode->setScale(d->contentsScale);

    QColor backgroundColor = d->webPageProxy->drawsTransparentBackground() ? Qt::transparent : Qt::white;
    QRectF backgroundRect(0, 0, d->contentsSize.width() * d->contentsScale, d->contentsSize.height() * d->contentsScale);
    backgroundNode->setColor(backgroundColor);
    backgroundNode->setRect(backgroundRect);

    return backgroundNode;
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

QQuickWebPagePrivate::~QQuickWebPagePrivate()
{
}

#include "moc_qquickwebpage_p.cpp"
