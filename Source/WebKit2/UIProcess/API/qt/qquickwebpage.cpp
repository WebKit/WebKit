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

#include "QtWebPageEventHandler.h"
#include "QtWebPageProxy.h"
#include "TransformationMatrix.h"
#include "qquickwebpage_p_p.h"
#include "qquickwebview_p.h"
#include <QtCore/QUrl>
#include <QtDeclarative/QQuickCanvas>
#include <QtDeclarative/QSGEngine>

QQuickWebPage::QQuickWebPage(QQuickItem* parent)
    : QQuickItem(parent)
    , d(new QQuickWebPagePrivate(this))
{
    setFlag(ItemHasContents);

    // We do the transform from the top left so the viewport can assume the position 0, 0
    // is always where rendering starts.
    setTransformOrigin(TopLeft);
    d->initializeSceneGraphConnections();
}

QQuickWebPage::~QQuickWebPage()
{
    delete d;
}

QtSGUpdateQueue *QQuickWebPage::sceneGraphUpdateQueue() const
{
    return &d->sgUpdateQueue;
}

void QQuickWebPage::keyPressEvent(QKeyEvent* event)
{
    this->event(event);
}

void QQuickWebPage::keyReleaseEvent(QKeyEvent* event)
{
    this->event(event);
}

void QQuickWebPage::inputMethodEvent(QInputMethodEvent* event)
{
    this->event(event);
}

void QQuickWebPage::focusInEvent(QFocusEvent* event)
{
    this->event(event);
}

void QQuickWebPage::focusOutEvent(QFocusEvent* event)
{
    this->event(event);
}

void QQuickWebPage::mousePressEvent(QMouseEvent* event)
{
    forceActiveFocus();
    this->event(event);
}

void QQuickWebPage::mouseMoveEvent(QMouseEvent* event)
{
    this->event(event);
}

void QQuickWebPage::mouseReleaseEvent(QMouseEvent* event)
{
    this->event(event);
}

void QQuickWebPage::mouseDoubleClickEvent(QMouseEvent* event)
{
    this->event(event);
}

void QQuickWebPage::wheelEvent(QWheelEvent* event)
{
    this->event(event);
}

void QQuickWebPage::hoverEnterEvent(QHoverEvent* event)
{
    this->event(event);
}

void QQuickWebPage::hoverMoveEvent(QHoverEvent* event)
{
    this->event(event);
}

void QQuickWebPage::hoverLeaveEvent(QHoverEvent* event)
{
    this->event(event);
}

void QQuickWebPage::dragMoveEvent(QDragMoveEvent* event)
{
    this->event(event);
}

void QQuickWebPage::dragEnterEvent(QDragEnterEvent* event)
{
    this->event(event);
}

void QQuickWebPage::dragLeaveEvent(QDragLeaveEvent* event)
{
    this->event(event);
}

void QQuickWebPage::dropEvent(QDropEvent* event)
{
    this->event(event);
}

void QQuickWebPage::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        d->pageProxy->setDrawingAreaSize(newGeometry.size().toSize());
}

bool QQuickWebPage::event(QEvent* ev)
{
    if (d->pageProxy->eventHandler()->handleEvent(ev))
        return true;
    if (ev->type() == QEvent::InputMethod)
        return false; // This is necessary to avoid an endless loop in connection with QQuickItem::event().
    return QQuickItem::event(ev);
}

void QQuickWebPage::touchEvent(QTouchEvent* event)
{
    this->event(event);
}

void QQuickWebPage::itemChange(ItemChange change, const ItemChangeData& data)
{
    if (change == ItemSceneChange)
        d->initializeSceneGraphConnections();
    QQuickItem::itemChange(change, data);
}

QQuickWebPagePrivate::QQuickWebPagePrivate(QQuickWebPage* view)
    : q(view)
    , pageProxy(0)
    , sgUpdateQueue(view)
    , paintingIsInitialized(false)
{
}

void QQuickWebPagePrivate::initializeSceneGraphConnections()
{
    if (paintingIsInitialized)
        return;
    if (!q->canvas())
        return;
    paintingIsInitialized = true;
    if (q->sceneGraphEngine())
        _q_onSceneGraphInitialized();
    else
        QObject::connect(q->canvas(), SIGNAL(sceneGraphInitialized()), q, SLOT(_q_onSceneGraphInitialized()));
}

void QQuickWebPagePrivate::setPageProxy(QtWebPageProxy* pageProxy)
{
    ASSERT(!this->pageProxy);
    ASSERT(pageProxy);
    this->pageProxy = pageProxy;
}

static float computeEffectiveOpacity(const QQuickItem* item)
{
    if (!item)
        return 1;

    float opacity = item->opacity();
    if (opacity < 0.01)
        return 0;

    return opacity * computeEffectiveOpacity(item->parentItem());
}

void QQuickWebPagePrivate::paintToCurrentGLContext()
{
    if (!q->isVisible())
        return;

    QTransform transform = q->itemTransform(0, 0);

    float opacity = computeEffectiveOpacity(q);
    QRectF clipRect = q->parentItem()->mapRectToScene(q->parentItem()->boundingRect());

    if (!clipRect.isValid())
        return;

    // Make sure that no GL error code stays from previous QT operations.
    glGetError();

    glEnable(GL_SCISSOR_TEST);
    ASSERT(!glGetError());
    const int left = clipRect.left();
    const int width = clipRect.width();
    const int bottom = q->canvas()->height() - (clipRect.bottom() + 1);
    const int height = clipRect.height();

    glScissor(left, bottom, width, height);
    ASSERT(!glGetError());

    pageProxy->renderToCurrentGLContext(transform, opacity);
    glDisable(GL_SCISSOR_TEST);
    ASSERT(!glGetError());
}

void QQuickWebPagePrivate::_q_onAfterSceneRender()
{
    // TODO: Allow painting before the scene or in the middle of the scene with an FBO.
    paintToCurrentGLContext();
}

void QQuickWebPagePrivate::_q_onSceneGraphInitialized()
{
    QSGEngine* engine = q->sceneGraphEngine();
    QObject::connect(engine, SIGNAL(afterRendering()), q, SLOT(_q_onAfterSceneRender()), Qt::DirectConnection);
}

#include "moc_qquickwebpage_p.cpp"
