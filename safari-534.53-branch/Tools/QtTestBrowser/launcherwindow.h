/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Girish Ramakrishnan <girish@forwardbias.in>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef launcherwindow_h
#define launcherwindow_h

#include <QtGui>
#include <QtNetwork/QNetworkRequest>

#if defined(QT_CONFIGURED_WITH_OPENGL)
#include <QtOpenGL/QGLWidget>
#endif

#if !defined(QT_NO_PRINTER)
#include <QPrintPreviewDialog>
#endif

#ifndef QT_NO_UITOOLS
#include <QtUiTools/QUiLoader>
#endif

#include <QDebug>

#include <cstdio>
#include <qevent.h>
#include <qwebelement.h>
#include <qwebframe.h>
#include <qwebinspector.h>
#include <qwebsettings.h>

#ifdef Q_WS_MAEMO_5
#include <qx11info_x11.h>
#endif

#include "DumpRenderTreeSupportQt.h"
#include "mainwindow.h"
#include "urlloader.h"
#include "utils.h"
#include "webinspector.h"
#include "webpage.h"
#include "webview.h"

#ifdef Q_WS_MAEMO_5
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#undef KeyPress
#endif

class QPropertyAnimation;

class WindowOptions {
public:
    WindowOptions()
        : useGraphicsView(false)
        , useCompositing(true)
        , useTiledBackingStore(false)
        , useWebGL(false)
#if defined(Q_WS_MAEMO_5) || defined(Q_OS_SYMBIAN)
        , useFrameFlattening(true)
#else
        , useFrameFlattening(false)
#endif
        , cacheWebView(false)
        , showFrameRate(false)
        , resizesToContents(false)
        , viewportUpdateMode(QGraphicsView::MinimalViewportUpdate)
        , useLocalStorage(false)
        , useOfflineStorageDatabase(false)
        , useOfflineWebApplicationCache(false)
        , offlineStorageDefaultQuotaSize(0)
#if defined(QT_CONFIGURED_WITH_OPENGL)
        , useQGLWidgetViewport(false)
#endif
#if defined(Q_WS_X11)
        , useTestFonts(false)
#endif
        , printLoadedUrls(false)
#if defined(Q_OS_SYMBIAN)
        , startMaximized(true)
#else
        , startMaximized(false)
#endif
    {
    }

    bool useGraphicsView;
    bool useCompositing;
    bool useTiledBackingStore;
    bool useWebGL;
    bool useFrameFlattening;
    bool cacheWebView;
    bool showFrameRate;
    bool resizesToContents;
    QGraphicsView::ViewportUpdateMode viewportUpdateMode;
    bool useLocalStorage;
    bool useOfflineStorageDatabase;
    bool useOfflineWebApplicationCache;
    quint64 offlineStorageDefaultQuotaSize;
#if defined(QT_CONFIGURED_WITH_OPENGL)
    bool useQGLWidgetViewport;
#endif
#if defined(Q_WS_X11)
    bool useTestFonts;
#endif
    bool printLoadedUrls;
    QUrl inspectorUrl;
    quint16 remoteInspectorPort;
    bool startMaximized;
};

class LauncherWindow : public MainWindow {
    Q_OBJECT

public:
    LauncherWindow(WindowOptions* data = 0, QGraphicsScene* sharedScene = 0);
    virtual ~LauncherWindow();

    virtual void keyPressEvent(QKeyEvent* event);
    void grabZoomKeys(bool grab);

    void sendTouchEvent();

    bool eventFilter(QObject* obj, QEvent* event);

protected slots:
    void loadStarted();
    void loadFinished();

    void showLinkHover(const QString &link, const QString &toolTip);

    void zoomIn();
    void zoomOut();
    void resetZoom();
    void toggleZoomTextOnly(bool on);
    void zoomAnimationFinished();

    void print();
    void screenshot();

    void setEditable(bool on);

    /* void dumpPlugins() */
    void dumpHtml();

    void loadURLListFromFile();

    void setTouchMocking(bool on);
    void toggleWebView(bool graphicsBased);
    void toggleAcceleratedCompositing(bool toggle);
    void toggleTiledBackingStore(bool toggle);
    void toggleResizesToContents(bool toggle);
    void toggleWebGL(bool toggle);
    void toggleSpatialNavigation(bool b);
    void toggleFullScreenMode(bool enable);
    void toggleFrameFlattening(bool toggle);
    void toggleInterruptingJavaScriptEnabled(bool enable);
    void toggleJavascriptCanOpenWindows(bool enable);
    void toggleAutoLoadImages(bool enable);
    void togglePlugins(bool enable);
    void toggleLocalStorage(bool toggle);
    void toggleOfflineStorageDatabase(bool toggle);
    void toggleOfflineWebApplicationCache(bool toggle);
    void setOfflineStorageDefaultQuota();

#if defined(QT_CONFIGURED_WITH_OPENGL)
    void toggleQGLWidgetViewport(bool enable);
#endif

    void changeViewportUpdateMode(int mode);
    void animatedFlip();
    void animatedYFlip();
    void selectElements();
    void showFPS(bool enable);
    void showUserAgentDialog();

    void printURL(const QUrl&);

public slots:
    LauncherWindow* newWindow();
    LauncherWindow* cloneWindow();
    void updateFPS(int fps);

signals:
    void enteredFullScreenMode(bool on);

private:
    void init();
    void initializeView();
    void createChrome();
    void applyPrefs();
    void applyZoom();

    bool isGraphicsBased() const;

private:
    static QVector<int> m_zoomLevels;
    int m_currentZoom;

    UrlLoader* m_urlLoader;

    QWidget* m_view;
    WebInspector* m_inspector;

    WindowOptions m_windowOptions;

    QAction* m_formatMenuAction;

    QPropertyAnimation* m_zoomAnimation;
    QList<QTouchEvent::TouchPoint> m_touchPoints;
    bool m_touchMocking;

    QString m_inputUrl;
};

#endif
