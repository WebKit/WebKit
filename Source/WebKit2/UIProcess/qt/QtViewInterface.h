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
#include <QtCore/QUrl>
#include <WebKit2/WKBase.h>

class QQuickWebPage;
class QQuickWebView;
class QtWebError;
class QWebDownloadItem;

QT_BEGIN_NAMESPACE
class QCursor;
class QGraphicsWidget;
class QImage;
class QJSEngine;
class QMimeData;
class QPoint;
class QRect;
class QWidget;
QT_END_NAMESPACE

namespace WebCore {
class ViewportArguments;
}

namespace WebKit {

class QtSGUpdateQueue;

class QtViewInterface {
public:
    enum FileChooserType {
        SingleFileSelection,
        MultipleFilesSelection
    };

    QtViewInterface(QQuickWebView* viewportView);

    virtual void scrollPositionRequested(const QPoint& pos);

    virtual bool isActive();
    virtual bool isVisible();

    virtual void startDrag(Qt::DropActions supportedDropActions, const QImage& dragImage, QMimeData*, QPoint* clientPosition, QPoint* globalPosition, Qt::DropAction*);

    virtual void didFinishFirstNonEmptyLayout();
    virtual void didChangeContentsSize(const QSize&);
    virtual void didChangeViewportProperties(const WebCore::ViewportArguments&);

    virtual void didChangeUrl(const QUrl&);
    virtual void didChangeTitle(const QString&);
    virtual void didChangeToolTip(const QString&);
    virtual void didChangeStatusText(const QString&);
    virtual void didChangeCursor(const QCursor&);
    virtual void loadDidBegin();
    virtual void loadDidCommit();
    virtual void loadDidSucceed();
    virtual void loadDidFail(const QtWebError&);
    virtual void didChangeLoadProgress(int);

    virtual void showContextMenu(QSharedPointer<QMenu>);
    virtual void hideContextMenu();

    virtual void runJavaScriptAlert(const QString&);
    virtual bool runJavaScriptConfirm(const QString&);
    virtual QString runJavaScriptPrompt(const QString&, const QString& defaultValue, bool& ok);

    virtual void processDidCrash(const QUrl&);
    virtual void didRelaunchProcess();

    virtual void chooseFiles(WKOpenPanelResultListenerRef, const QStringList& selectedFileNames, FileChooserType);

    virtual void didMouseMoveOverElement(const QUrl&, const QString&);

    virtual void downloadRequested(QWebDownloadItem*);

private:
    QQuickWebView* const m_viewportView;

    QSharedPointer<QMenu> activeMenu;
    QUrl lastHoveredURL;
    QString lastHoveredTitle;
};

}

#endif // QtViewInterface_h
