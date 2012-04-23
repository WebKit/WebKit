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
#include "QtWebPageSGNode.h"
#include "TransformationMatrix.h"
#include "WebLayerTreeRenderer.h"
#include "WebPageProxy.h"
#include "qquickwebpage_p_p.h"
#include "qquickwebview_p.h"
#include <QtQuick/QQuickCanvas>

using namespace WebKit;

QQuickWebPage::QQuickWebPage(QQuickWebView* viewportItem)
    : QQuickItem(viewportItem->contentItem())
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

void QQuickWebPagePrivate::paint(QPainter* painter)
{
    if (!webPageProxy->drawingArea())
        return;

    LayerTreeHostProxy* layerTreeHostProxy = webPageProxy->drawingArea()->layerTreeHostProxy();
    if (layerTreeHostProxy->layerTreeRenderer())
        layerTreeHostProxy->layerTreeRenderer()->paintToGraphicsContext(painter);
}

QSGNode* QQuickWebPage::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    if (!d->webPageProxy->drawingArea())
        return oldNode;

    LayerTreeHostProxy* layerTreeHostProxy = d->webPageProxy->drawingArea()->layerTreeHostProxy();
    WebLayerTreeRenderer* renderer = layerTreeHostProxy->layerTreeRenderer();

    QtWebPageSGNode* node = static_cast<QtWebPageSGNode*>(oldNode);
    if (!node)
        node = new QtWebPageSGNode();
    node->setRenderer(renderer);
    renderer->syncRemoteContent();

    node->setScale(d->contentsScale);
    QColor backgroundColor = d->webPageProxy->drawsTransparentBackground() ? Qt::transparent : Qt::white;
    QRectF backgroundRect(QPointF(0, 0), d->contentsSize);
    node->setBackground(backgroundRect, backgroundColor);

    return node;
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
    DrawingAreaProxy* drawingArea = webPageProxy->drawingArea();
    if (drawingArea && drawingArea->layerTreeHostProxy())
        drawingArea->layerTreeHostProxy()->setContentsSize(WebCore::FloatSize(contentsSize.width(), contentsSize.height()));
}

QQuickWebPagePrivate::~QQuickWebPagePrivate()
{
}

#include "moc_qquickwebpage_p.cpp"
