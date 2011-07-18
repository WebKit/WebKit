/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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
#include "TouchViewInterface.h"

#include "qtouchwebpage.h"
#include "qtouchwebpage_p.h"
#include "qtouchwebview.h"
#include "qtouchwebview_p.h"

namespace WebKit {

TouchViewInterface::TouchViewInterface(QTouchWebView* viewportView, QTouchWebPage* pageView)
    : m_viewportView(viewportView)
    , m_pageView(pageView)
    , m_pinchStartScale(1.f)
{
    Q_ASSERT(m_viewportView);
    Q_ASSERT(m_pageView);
}

void TouchViewInterface::panGestureStarted()
{
    // FIXME: suspend the Web engine (stop animated GIF, etc).
    // FIXME: initialize physics for panning (stop animation, etc).
}

void TouchViewInterface::panGestureRequestScroll(qreal deltaX, qreal deltaY)
{
    m_viewportView->d->scroll(deltaX, deltaY);
}

void TouchViewInterface::panGestureEnded()
{
    // FIXME: trigger physics engine for animation (the Web engine should be resumed after the animation.)
}

void TouchViewInterface::panGestureCancelled()
{
    // FIXME: reset physics.
    // FIXME: resume the Web engine.
}

void TouchViewInterface::pinchGestureStarted()
{
    // FIXME: suspend the engine.
    m_pinchStartScale = m_pageView->scale();
}

void TouchViewInterface::pinchGestureRequestUpdate(const QPointF& pinchCenterInPageViewCoordinate, qreal totalScaleFactor)
{
    // FIXME: it is a bit more complicated than that, changes of the center position should move the page even
    // if the zoom factor does not change. Both the zoom and the panning should be handled through the physics
    // engine.
    const qreal scale = m_pinchStartScale * totalScaleFactor;
    m_pageView->setTransformOriginPoint(pinchCenterInPageViewCoordinate);
    m_pageView->setScale(scale);
}

void TouchViewInterface::pinchGestureEnded()
{
    // FIXME: commit scale with the scale value in the valid range in order to get new tiles.
    // FIXME: animate the back zoom in the valid range.
    // FIXME: resume the engine after the animation.
    m_pageView->d->commitScaleChange();
}

void TouchViewInterface::setViewNeedsDisplay(const QRect& invalidatedRect)
{
    m_pageView->update(invalidatedRect);
}

QSize TouchViewInterface::drawingAreaSize()
{
    return m_pageView->size().toSize();
}

void TouchViewInterface::contentSizeChanged(const QSize& newSize)
{
    // FIXME: the viewport should take care of:
    // -resize the page
    // -change the zoom level if needed
    // -move the page back in viewport boundaries if needed
    // -update the viewport rect
    m_pageView->resize(newSize);
    m_viewportView->d->viewportRectUpdated();
}

bool TouchViewInterface::isActive()
{
    return m_pageView->isActive();
}

bool TouchViewInterface::hasFocus()
{
    return m_pageView->hasFocus();
}

bool TouchViewInterface::isVisible()
{
    return m_viewportView->isVisible() && m_pageView->isVisible();
}

void TouchViewInterface::startDrag(Qt::DropActions supportedDropActions, const QImage& dragImage, QMimeData* data, QPoint* clientPosition, QPoint* globalPosition, Qt::DropAction* dropAction)
{
    // QTouchWebView does not support drag and drop.
    Q_ASSERT(false);
}

void TouchViewInterface::didChangeUrl(const QUrl& url)
{
    emit m_pageView->urlChanged(url);
}

void TouchViewInterface::didChangeTitle(const QString& newTitle)
{
    emit m_pageView->titleChanged(newTitle);
}

void TouchViewInterface::didChangeToolTip(const QString&)
{
    // There is not yet any UI defined for the tooltips for mobile so we ignore the change.
}

void TouchViewInterface::didChangeStatusText(const QString&)
{
    // There is not yet any UI defined for status text so we ignore the call.
}

void TouchViewInterface::didChangeCursor(const QCursor&)
{
    // The cursor is not visible on mobile, just ignore this message.
}

void TouchViewInterface::loadDidBegin()
{
    emit m_pageView->loadStarted();
}

void TouchViewInterface::loadDidSucceed()
{
    emit m_pageView->loadSucceeded();
}

void TouchViewInterface::loadDidFail(const QWebError& error)
{
    emit m_pageView->loadFailed(error);
}

void TouchViewInterface::didChangeLoadProgress(int percentageLoaded)
{
    emit m_pageView->loadProgress(percentageLoaded);
}

void TouchViewInterface::showContextMenu(QSharedPointer<QMenu>)
{
    // FIXME
}

void TouchViewInterface::hideContextMenu()
{
    // FIXME
}

void TouchViewInterface::processDidCrash()
{
    // FIXME
}

void TouchViewInterface::didRelaunchProcess()
{
    // FIXME
}

}
