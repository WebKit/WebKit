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
#include "QtTouchViewInterface.h"

#include "QtWebError.h"
#include "qtouchwebpage.h"
#include "qtouchwebpage_p.h"
#include "qtouchwebview.h"
#include "qtouchwebview_p.h"

#include <QDeclarativeEngine>
#include <QQuickView>

namespace WebKit {

QtTouchViewInterface::QtTouchViewInterface(QTouchWebView* viewportView, QTouchWebPage* pageView)
    : m_viewportView(viewportView)
    , m_pageView(pageView)
{
    Q_ASSERT(m_viewportView);
    Q_ASSERT(m_pageView);
}

void QtTouchViewInterface::didFindZoomableArea(const QPoint&, const QRect&)
{
}

QtSGUpdateQueue* QtTouchViewInterface::sceneGraphUpdateQueue() const
{
    return &m_pageView->d->sgUpdateQueue;
}

void QtTouchViewInterface::setViewNeedsDisplay(const QRect&)
{
    m_pageView->update();
}

QSize QtTouchViewInterface::drawingAreaSize()
{
    return QSize(m_pageView->width(), m_pageView->height());
}

void QtTouchViewInterface::contentSizeChanged(const QSize& newSize)
{
    m_pageView->setWidth(newSize.width());
    m_pageView->setHeight(newSize.height());
}

void QtTouchViewInterface::scrollPositionRequested(const QPoint& pos)
{
    m_viewportView->d_func()->scrollPositionRequested(pos);
}

bool QtTouchViewInterface::isActive()
{
    // FIXME: The scene graph does not have the concept of being active or not when this was written.
    return true;
}

bool QtTouchViewInterface::hasFocus()
{
    return m_pageView->hasFocus();
}

bool QtTouchViewInterface::isVisible()
{
    return m_viewportView->isVisible() && m_pageView->isVisible();
}

void QtTouchViewInterface::startDrag(Qt::DropActions supportedDropActions, const QImage& dragImage, QMimeData* data, QPoint* clientPosition, QPoint* globalPosition, Qt::DropAction* dropAction)
{
    // QTouchWebView does not support drag and drop.
    Q_ASSERT(false);
}

void QtTouchViewInterface::didChangeViewportProperties(const WebCore::ViewportArguments& args)
{
    m_viewportView->d_func()->didChangeViewportProperties(args);
}

void QtTouchViewInterface::didChangeUrl(const QUrl& url)
{
    emit m_viewportView->urlChanged(url);
}

void QtTouchViewInterface::didChangeTitle(const QString& newTitle)
{
    emit m_viewportView->titleChanged(newTitle);
}

void QtTouchViewInterface::didChangeToolTip(const QString&)
{
    // There is not yet any UI defined for the tooltips for mobile so we ignore the change.
}

void QtTouchViewInterface::didChangeStatusText(const QString&)
{
    // There is not yet any UI defined for status text so we ignore the call.
}

void QtTouchViewInterface::didChangeCursor(const QCursor&)
{
    // The cursor is not visible on mobile, just ignore this message.
}

void QtTouchViewInterface::loadDidBegin()
{
    emit m_viewportView->loadStarted();
}

void QtTouchViewInterface::loadDidCommit()
{
    m_viewportView->d_func()->loadDidCommit();
}

void QtTouchViewInterface::loadDidSucceed()
{
    emit m_viewportView->loadSucceeded();
}

void QtTouchViewInterface::loadDidFail(const QtWebError& error)
{
    emit m_viewportView->loadFailed(static_cast<QBaseWebView::ErrorType>(error.type()), error.errorCode(), error.url());
}

void QtTouchViewInterface::didChangeLoadProgress(int percentageLoaded)
{
    emit m_viewportView->loadProgressChanged(percentageLoaded);
}

void QtTouchViewInterface::showContextMenu(QSharedPointer<QMenu>)
{
    // FIXME
}

void QtTouchViewInterface::hideContextMenu()
{
    // FIXME
}

void QtTouchViewInterface::runJavaScriptAlert(const QString&)
{
    // FIXME.
}

bool QtTouchViewInterface::runJavaScriptConfirm(const QString&)
{
    // FIXME.
    return true;
}

QString QtTouchViewInterface::runJavaScriptPrompt(const QString&, const QString& defaultValue, bool&)
{
    // FIXME.
    return defaultValue;
}

void QtTouchViewInterface::processDidCrash()
{
    // FIXME
}

void QtTouchViewInterface::didRelaunchProcess()
{
    // FIXME
}

QJSEngine* QtTouchViewInterface::engine()
{
    QQuickView* view = qobject_cast<QQuickView*>(m_pageView->canvas());
    if (view)
        return view->engine();
    return 0;
}

}
