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

#ifndef QtViewInterface_h
#define QtViewInterface_h

#include <QMenu>
#include <QtCore/QSharedPointer>
#include <QtCore/QSize>
#include <WebKit2/WKBase.h>

class QWebError;

QT_BEGIN_NAMESPACE
class QCursor;
class QGraphicsWidget;
class QImage;
class QJSEngine;
class QMimeData;
class QPoint;
class QRect;
class QUrl;
class QWidget;
QT_END_NAMESPACE

namespace WebCore {
class ViewportArguments;
}

namespace WebKit {

class QtViewInterface {
public:
    enum FileChooserType {
        SingleFileSelection,
        MultipleFilesSelection
    };

    virtual void setViewNeedsDisplay(const QRect&) = 0;

    virtual QSize drawingAreaSize() = 0;
    virtual void contentSizeChanged(const QSize&) = 0;

    virtual bool isActive() = 0;
    virtual bool hasFocus() = 0;
    virtual bool isVisible() = 0;

    virtual void startDrag(Qt::DropActions supportedDropActions, const QImage& dragImage, QMimeData*, QPoint* clientPosition, QPoint* globalPosition, Qt::DropAction*) = 0;
    virtual void didReceiveViewportArguments(const WebCore::ViewportArguments&) = 0;

    virtual void didFindZoomableArea(const QPoint&, const QRect&) = 0;

    virtual void didChangeUrl(const QUrl&) = 0;
    virtual void didChangeTitle(const QString&) = 0;
    virtual void didChangeToolTip(const QString&) = 0;
    virtual void didChangeStatusText(const QString&) = 0;
    virtual void didChangeCursor(const QCursor&) = 0;
    virtual void loadDidBegin() = 0;
    virtual void loadDidCommit() = 0;
    virtual void loadDidSucceed() = 0;
    virtual void loadDidFail(const QWebError&) = 0;
    virtual void didChangeLoadProgress(int) = 0;

    virtual void showContextMenu(QSharedPointer<QMenu>) = 0;
    virtual void hideContextMenu() = 0;

    virtual void runJavaScriptAlert(const QString&) = 0;
    virtual bool runJavaScriptConfirm(const QString&) = 0;
    virtual QString runJavaScriptPrompt(const QString&, const QString& defaultValue, bool& ok) = 0;

    virtual void processDidCrash() = 0;
    virtual void didRelaunchProcess() = 0;

    virtual QJSEngine* engine() = 0;

    virtual void chooseFiles(WKOpenPanelResultListenerRef, const QStringList& selectedFileNames, FileChooserType) = 0;

    virtual void didMouseMoveOverElement(const QUrl&, const QString&) = 0;
};

}

#endif // QtViewInterface_h
