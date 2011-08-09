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

#ifndef ViewInterface_h
#define ViewInterface_h

#include <QtCore/QSharedPointer>
#include <QtCore/QSize>
#include <QtGui/QMenu>

class QWebError;

QT_BEGIN_NAMESPACE
class QCursor;
class QGraphicsWidget;
class QImage;
class QMimeData;
class QPoint;
class QRect;
class QUrl;
class QWidget;
QT_END_NAMESPACE

namespace WebKit {

class ViewInterface
{
public:
    virtual void setViewNeedsDisplay(const QRect&) = 0;

    virtual QSize drawingAreaSize() = 0;
    virtual void contentSizeChanged(const QSize&) = 0;

    virtual bool isActive() = 0;
    virtual bool hasFocus() = 0;
    virtual bool isVisible() = 0;

    virtual void startDrag(Qt::DropActions supportedDropActions, const QImage& dragImage, QMimeData* data, QPoint* clientPosition, QPoint* globalPosition, Qt::DropAction* dropAction) = 0;
    virtual void didFindZoomableArea(const QPoint&, const QRect&) = 0;

    virtual void didChangeUrl(const QUrl&) = 0;
    virtual void didChangeTitle(const QString&) = 0;
    virtual void didChangeToolTip(const QString&) = 0;
    virtual void didChangeStatusText(const QString&) = 0;
    virtual void didChangeCursor(const QCursor&) = 0;
    virtual void loadDidBegin() = 0;
    virtual void loadDidSucceed() = 0;
    virtual void loadDidFail(const QWebError&) = 0;
    virtual void didChangeLoadProgress(int) = 0;

    virtual void showContextMenu(QSharedPointer<QMenu>) = 0;
    virtual void hideContextMenu() = 0;

    virtual void processDidCrash() = 0;
    virtual void didRelaunchProcess() = 0;
};

}

#endif /* ViewInterface_h */
