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
#include "QtViewInterface.h"

#include "QtWebError.h"
#include "qquickwebpage.h"
#include "qquickwebpage_p.h"
#include "qquickwebview.h"
#include "qquickwebview_p.h"

#include <QtDeclarative/QDeclarativeEngine>
#include <QtDeclarative/QQuickView>
#include <QtGui/QDrag>
#include <QtGui/QGuiApplication>

namespace WebKit {

QtViewInterface::QtViewInterface(QQuickWebView* viewportView, QQuickWebPage* pageView)
    : m_viewportView(viewportView)
    , m_pageView(pageView)
{
    Q_ASSERT(m_viewportView);
    Q_ASSERT(m_pageView);
}

void QtViewInterface::didFindZoomableArea(const QPoint&, const QRect&)
{
}

QtSGUpdateQueue* QtViewInterface::sceneGraphUpdateQueue() const
{
    return &m_pageView->d->sgUpdateQueue;
}

void QtViewInterface::setViewNeedsDisplay(const QRect&)
{
    m_pageView->update();
}

QSize QtViewInterface::drawingAreaSize()
{
    return QSize(m_pageView->width(), m_pageView->height());
}

void QtViewInterface::contentSizeChanged(const QSize& newSize)
{
    m_viewportView->d_func()->contentSizeChanged(newSize);
}

void QtViewInterface::scrollPositionRequested(const QPoint& pos)
{
    m_viewportView->d_func()->scrollPositionRequested(pos);
}

bool QtViewInterface::isActive()
{
    // FIXME: The scene graph does not have the concept of being active or not when this was written.
    return true;
}

bool QtViewInterface::hasFocus()
{
    return m_pageView->hasFocus();
}

bool QtViewInterface::isVisible()
{
    return m_viewportView->isVisible() && m_pageView->isVisible();
}

void QtViewInterface::startDrag(Qt::DropActions supportedDropActions, const QImage& dragImage, QMimeData* data, QPoint* clientPosition, QPoint* globalPosition, Qt::DropAction* dropAction)
{
    QWindow* window = m_viewportView->canvas();
    if (!window)
        return;

    QDrag* drag = new QDrag(window);
    drag->setPixmap(QPixmap::fromImage(dragImage));
    drag->setMimeData(data);
    *dropAction = drag->exec(supportedDropActions);
    *globalPosition = QCursor::pos();
    *clientPosition = window->mapFromGlobal(*globalPosition);
}


void QtViewInterface::didChangeViewportProperties(const WebCore::ViewportArguments& args)
{
    m_viewportView->d_func()->didChangeViewportProperties(args);
}

void QtViewInterface::didChangeUrl(const QUrl& url)
{
    emit m_viewportView->urlChanged(url);
}

void QtViewInterface::didChangeTitle(const QString& newTitle)
{
    emit m_viewportView->titleChanged(newTitle);
}

void QtViewInterface::didChangeToolTip(const QString&)
{
    // There is not yet any UI defined for the tooltips for mobile so we ignore the change.
}

void QtViewInterface::didChangeStatusText(const QString& newMessage)
{
    emit m_viewportView->statusBarMessageChanged(newMessage);
}

void QtViewInterface::didChangeCursor(const QCursor& newCursor)
{
    // FIXME: This is a temporary fix until we get cursor support in QML items.
    QGuiApplication::setOverrideCursor(newCursor);
}

void QtViewInterface::loadDidBegin()
{
    emit m_viewportView->loadStarted();
}

void QtViewInterface::loadDidCommit()
{
    m_viewportView->d_func()->loadDidCommit();
}

void QtViewInterface::loadDidSucceed()
{
    emit m_viewportView->loadSucceeded();
}

void QtViewInterface::loadDidFail(const QtWebError& error)
{
    emit m_viewportView->loadFailed(static_cast<QQuickWebView::ErrorType>(error.type()), error.errorCode(), error.url());
}

void QtViewInterface::didChangeLoadProgress(int percentageLoaded)
{
    emit m_viewportView->loadProgressChanged(percentageLoaded);
}

void QtViewInterface::didMouseMoveOverElement(const QUrl& linkURL, const QString& linkTitle)
{
    if (linkURL == lastHoveredURL && linkTitle == lastHoveredTitle)
        return;
    lastHoveredURL = linkURL;
    lastHoveredTitle = linkTitle;
    emit m_viewportView->linkHovered(lastHoveredURL, lastHoveredTitle);
}

void QtViewInterface::showContextMenu(QSharedPointer<QMenu> menu)
{
    // Remove the active menu in case this function is called twice.
    if (activeMenu)
        activeMenu->hide();

    if (menu->isEmpty())
        return;

    QWindow* window = m_viewportView->canvas();
    if (!window)
        return;

    activeMenu = menu;

    activeMenu->window()->winId(); // Ensure that the menu has a window
    Q_ASSERT(activeMenu->window()->windowHandle());
    activeMenu->window()->windowHandle()->setTransientParent(window);

    QPoint menuPositionInScene = m_viewportView->mapToScene(menu->pos()).toPoint();
    menu->exec(window->mapToGlobal(menuPositionInScene));
    // The last function to get out of exec() clear the local copy.
    if (activeMenu == menu)
        activeMenu.clear();
}

void QtViewInterface::hideContextMenu()
{
    if (activeMenu)
        activeMenu->hide();
}

void QtViewInterface::runJavaScriptAlert(const QString& alertText)
{
    m_viewportView->d_func()->runJavaScriptAlert(alertText);
}

bool QtViewInterface::runJavaScriptConfirm(const QString& message)
{
    return m_viewportView->d_func()->runJavaScriptConfirm(message);
}

QString QtViewInterface::runJavaScriptPrompt(const QString& message, const QString& defaultValue, bool& ok)
{
    return m_viewportView->d_func()->runJavaScriptPrompt(message, defaultValue, ok);
}

void QtViewInterface::processDidCrash()
{
    // FIXME
}

void QtViewInterface::didRelaunchProcess()
{
    // FIXME
}

QJSEngine* QtViewInterface::engine()
{
    QQuickView* view = qobject_cast<QQuickView*>(m_pageView->canvas());
    if (view)
        return view->engine();
    return 0;
}


void QtViewInterface::downloadRequested(QWebDownloadItem* downloadItem)
{
    if (!downloadItem)
        return;

    QDeclarativeEngine::setObjectOwnership(downloadItem, QDeclarativeEngine::JavaScriptOwnership);
    emit m_viewportView->downloadRequested(downloadItem);
}

void QtViewInterface::chooseFiles(WKOpenPanelResultListenerRef listenerRef, const QStringList& selectedFileNames, QtViewInterface::FileChooserType type)
{
    m_viewportView->d_func()->chooseFiles(listenerRef, selectedFileNames, type);
}

}

