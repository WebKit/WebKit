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
#include "qweberror.h"

#include <QDeclarativeEngine>
#include <QSGView>

namespace WebKit {

TouchViewInterface::TouchViewInterface(QTouchWebView* viewportView, QTouchWebPage* pageView)
    : m_viewportView(viewportView)
    , m_pageView(pageView)
{
    Q_ASSERT(m_viewportView);
    Q_ASSERT(m_pageView);
}

void TouchViewInterface::didFindZoomableArea(const QPoint&, const QRect&)
{
}

SGUpdateQueue* TouchViewInterface::sceneGraphUpdateQueue() const
{
    return &m_pageView->d->sgUpdateQueue;
}

void TouchViewInterface::setViewNeedsDisplay(const QRect&)
{
}

QSize TouchViewInterface::drawingAreaSize()
{
    return QSize(m_pageView->width(), m_pageView->height());
}

void TouchViewInterface::contentSizeChanged(const QSize& newSize)
{
    m_pageView->setWidth(newSize.width());
    m_pageView->setHeight(newSize.height());
    m_viewportView->d->updateViewportConstraints();
}

bool TouchViewInterface::isActive()
{
    // FIXME: The scene graph does not have the concept of being active or not when this was written.
    return true;
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

void TouchViewInterface::didReceiveViewportArguments(const WebCore::ViewportArguments& args)
{
    m_viewportView->d->setViewportArguments(args);
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

void TouchViewInterface::loadDidCommit()
{
    m_viewportView->d->loadDidCommit();
}

void TouchViewInterface::loadDidSucceed()
{
    emit m_pageView->loadSucceeded();
}

void TouchViewInterface::loadDidFail(const QWebError& error)
{
    emit m_pageView->loadFailed(static_cast<QTouchWebPage::ErrorType>(error.type()), error.errorCode(), error.url());
}

void TouchViewInterface::didChangeLoadProgress(int percentageLoaded)
{
    emit m_pageView->loadProgressChanged(percentageLoaded);
}

void TouchViewInterface::showContextMenu(QSharedPointer<QMenu>)
{
    // FIXME
}

void TouchViewInterface::hideContextMenu()
{
    // FIXME
}

void TouchViewInterface::runJavaScriptAlert(const QString&)
{
    // FIXME.
}

bool TouchViewInterface::runJavaScriptConfirm(const QString&)
{
    // FIXME.
    return true;
}

QString TouchViewInterface::runJavaScriptPrompt(const QString&, const QString& defaultValue, bool&)
{
    // FIXME.
    return defaultValue;
}

void TouchViewInterface::processDidCrash()
{
    // FIXME
}

void TouchViewInterface::didRelaunchProcess()
{
    // FIXME
}

QJSEngine* TouchViewInterface::engine()
{
    QSGView* view = qobject_cast<QSGView*>(m_pageView->canvas());
    if (view)
        return view->engine();
    return 0;
}

}
