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
#include "qtouchwebpage.h"

#include "QtTouchWebPageProxy.h"
#include "TransformationMatrix.h"
#include "qtouchwebpage_p.h"
#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QQuickCanvas>
#include <QSGEngine>
#include <QUrl>

QTouchWebPage::QTouchWebPage(QQuickItem* parent)
    : QQuickItem(parent)
    , d(new QTouchWebPagePrivate(this))
{
    setFlag(ItemHasContents);

    // We do the transform from the top left so the viewport can assume the position 0, 0
    // is always where rendering starts.
    setTransformOrigin(TopLeft);
    initSceneGraphConnections();
}

void QTouchWebPage::initSceneGraphConnections()
{
    if (d->paintingIsInitialized)
        return;
    if (!canvas())
        return;
    d->paintingIsInitialized = true;
    if (sceneGraphEngine())
        d->_q_onSceneGraphInitialized();
    else
        connect(canvas(), SIGNAL(sceneGraphInitialized()), this, SLOT(_q_onSceneGraphInitialized()));
}

QTouchWebPage::~QTouchWebPage()
{
    delete d;
}

/*! \reimp
*/
bool QTouchWebPage::event(QEvent* ev)
{
    if (d->pageProxy->handleEvent(ev))
        return true;
    return QQuickItem::event(ev);
}

void QTouchWebPage::keyPressEvent(QKeyEvent* event)
{
    this->event(event);
}

void QTouchWebPage::keyReleaseEvent(QKeyEvent* event)
{
    this->event(event);
}

void QTouchWebPage::inputMethodEvent(QInputMethodEvent* event)
{
    this->event(event);
}

void QTouchWebPage::focusInEvent(QFocusEvent* event)
{
    this->event(event);
}

void QTouchWebPage::focusOutEvent(QFocusEvent* event)
{
    this->event(event);
}

void QTouchWebPage::touchEvent(QTouchEvent* event)
{
    this->event(event);
}

void QTouchWebPage::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        d->pageProxy->setDrawingAreaSize(newGeometry.size().toSize());
}

QTouchWebPagePrivate::QTouchWebPagePrivate(QTouchWebPage* view)
    : q(view)
    , pageProxy(0)
    , navigationController(0)
    , sgUpdateQueue(view)
    , paintingIsInitialized(false)
{
}

void QTouchWebPagePrivate::setPageProxy(QtWebPageProxy* pageProxy)
{
    ASSERT(!this->pageProxy);
    ASSERT(pageProxy);
    this->pageProxy = pageProxy;

}

static float computeEffectiveOpacity(const QQuickItem* item)
{
    if (!item)
        return 1.0;

    float opacity = item->opacity();
    if (opacity < 0.01)
        return 0;

    return opacity * computeEffectiveOpacity(item->parentItem());
}

void QTouchWebPagePrivate::paintToCurrentGLContext()
{
    if (!q->isVisible())
        return;

    QTransform transform = q->itemTransform(0, 0);
    float opacity = computeEffectiveOpacity(q);
    QRectF clipRect = q->parentItem()->mapRectToScene(q->parentItem()->boundingRect());

    glEnable(GL_SCISSOR_TEST);
    const int left = clipRect.left();
    const int width = clipRect.width();
    const int bottom = q->canvas()->height() - (clipRect.bottom() + 1);
    const int height = clipRect.height();

    glScissor(left, bottom, width, height);

    touchPageProxy()->renderToCurrentGLContext(transform, opacity);
    glDisable(GL_SCISSOR_TEST);
}

void QTouchWebPagePrivate::_q_onAfterSceneRender()
{
    // TODO: Allow painting before the scene or in the middle of the scene with an FBO.
    paintToCurrentGLContext();
}

void QTouchWebPagePrivate::_q_onSceneGraphInitialized()
{
    QSGEngine* engine = q->sceneGraphEngine();
    QObject::connect(engine, SIGNAL(afterRendering()), q, SLOT(_q_onAfterSceneRender()), Qt::DirectConnection);
}

void QTouchWebPage::itemChange(ItemChange change, const ItemChangeData &data)
{
    if (change == ItemSceneChange)
        initSceneGraphConnections();
    QQuickItem::itemChange(change, data);
}

#include "moc_qtouchwebpage.cpp"
