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

#include "TouchViewInterface.h"
#include "qtouchwebpage.h"
#include "qtouchwebview.h"

namespace WebKit {

TouchViewInterface::TouchViewInterface(QTouchWebView* viewportView, QTouchWebPage* pageView)
    : m_viewportView(viewportView)
    , m_pageView(pageView)
{
    Q_ASSERT(m_viewportView);
    Q_ASSERT(m_pageView);
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
    m_pageView->resize(newSize);
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

#ifndef QT_NO_CURSOR
void TouchViewInterface::didChangeCursor(const QCursor&)
{
    // The cursor is not visible on mobile, just ignore this message.
}
#endif

void TouchViewInterface::loadDidBegin()
{
    emit m_pageView->loadStarted();
}

void TouchViewInterface::loadDidSucceed()
{
    emit m_pageView->loadSucceeded();
}

void TouchViewInterface::didChangeLoadProgress(int percentageLoaded)
{
    emit m_pageView->loadProgress(percentageLoaded);
}

void TouchViewInterface::showContextMenu(QSharedPointer<QMenu>)
{
    // TODO
}

void TouchViewInterface::hideContextMenu()
{
    // TODO
}

}
