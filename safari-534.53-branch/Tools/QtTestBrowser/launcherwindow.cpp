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

#include "launcherwindow.h"
#include "urlloader.h"

const int gExitClickArea = 80;
QVector<int> LauncherWindow::m_zoomLevels;

LauncherWindow::LauncherWindow(WindowOptions* data, QGraphicsScene* sharedScene)
    : MainWindow()
    , m_currentZoom(100)
    , m_urlLoader(0)
    , m_view(0)
    , m_inspector(0)
    , m_formatMenuAction(0)
    , m_zoomAnimation(0)
{
    if (data)
        m_windowOptions = *data;

    init();
    if (sharedScene && data->useGraphicsView)
        static_cast<QGraphicsView*>(m_view)->setScene(sharedScene);

    createChrome();
}

LauncherWindow::~LauncherWindow()
{
    grabZoomKeys(false);
    delete m_urlLoader;
}

void LauncherWindow::init()
{
    QSplitter* splitter = new QSplitter(Qt::Vertical, this);
    setCentralWidget(splitter);

    if (m_windowOptions.startMaximized)
        setWindowState(windowState() | Qt::WindowMaximized);
    else
        resize(800, 600);

    m_inspector = new WebInspector;
#ifndef QT_NO_PROPERTIES
    if (!m_windowOptions.inspectorUrl.isEmpty())
        m_inspector->setProperty("_q_inspectorUrl", m_windowOptions.inspectorUrl);
#endif
    connect(this, SIGNAL(destroyed()), m_inspector, SLOT(deleteLater()));

    // the zoom values are chosen to be like in Mozilla Firefox 3
    if (!m_zoomLevels.count()) {
        m_zoomLevels << 30 << 50 << 67 << 80 << 90;
        m_zoomLevels << 100;
        m_zoomLevels << 110 << 120 << 133 << 150 << 170 << 200 << 240 << 300;
    }

    grabZoomKeys(true);

    initializeView();
}

void LauncherWindow::initializeView()
{
    delete m_view;

    m_inputUrl = addressUrl();
    QUrl url = page()->mainFrame()->url();
    setPage(new WebPage(this));

    QSplitter* splitter = static_cast<QSplitter*>(centralWidget());

    if (!m_windowOptions.useGraphicsView) {
        WebViewTraditional* view = new WebViewTraditional(splitter);
        view->setPage(page());

        view->installEventFilter(this);

        m_view = view;
    } else {
        WebViewGraphicsBased* view = new WebViewGraphicsBased(splitter);
        m_view = view;
#if defined(QT_CONFIGURED_WITH_OPENGL)
        toggleQGLWidgetViewport(m_windowOptions.useQGLWidgetViewport);
#endif
        view->setPage(page());

        connect(view, SIGNAL(currentFPSUpdated(int)), this, SLOT(updateFPS(int)));

        view->installEventFilter(this);
        // The implementation of QAbstractScrollArea::eventFilter makes us need
        // to install the event filter also on the viewport of a QGraphicsView.
        view->viewport()->installEventFilter(this);
    }

    m_touchMocking = false;

    connect(page(), SIGNAL(loadStarted()), this, SLOT(loadStarted()));
    connect(page(), SIGNAL(loadFinished(bool)), this, SLOT(loadFinished()));
    connect(page(), SIGNAL(linkHovered(const QString&, const QString&, const QString&)),
            this, SLOT(showLinkHover(const QString&, const QString&)));
    connect(this, SIGNAL(enteredFullScreenMode(bool)), this, SLOT(toggleFullScreenMode(bool)));

    if (m_windowOptions.printLoadedUrls)
        connect(page()->mainFrame(), SIGNAL(urlChanged(QUrl)), this, SLOT(printURL(QUrl)));

    applyPrefs();

    splitter->addWidget(m_inspector);
    m_inspector->setPage(page());
    m_inspector->hide();

    if (m_windowOptions.remoteInspectorPort)
        page()->setProperty("_q_webInspectorServerPort", m_windowOptions.remoteInspectorPort);

    if (url.isValid())
        page()->mainFrame()->load(url);
    else  {
        setAddressUrl(m_inputUrl);
        m_inputUrl = QString();
    }
}

void LauncherWindow::applyPrefs()
{
    QWebSettings* settings = page()->settings();
    settings->setAttribute(QWebSettings::AcceleratedCompositingEnabled, m_windowOptions.useCompositing);
    settings->setAttribute(QWebSettings::TiledBackingStoreEnabled, m_windowOptions.useTiledBackingStore);
    settings->setAttribute(QWebSettings::FrameFlatteningEnabled, m_windowOptions.useFrameFlattening);
    settings->setAttribute(QWebSettings::WebGLEnabled, m_windowOptions.useWebGL);

    if (!isGraphicsBased())
        return;

    WebViewGraphicsBased* view = static_cast<WebViewGraphicsBased*>(m_view);
    view->setViewportUpdateMode(m_windowOptions.viewportUpdateMode);
    view->setFrameRateMeasurementEnabled(m_windowOptions.showFrameRate);
    view->setItemCacheMode(m_windowOptions.cacheWebView ? QGraphicsItem::DeviceCoordinateCache : QGraphicsItem::NoCache);

    if (m_windowOptions.resizesToContents)
        toggleResizesToContents(m_windowOptions.resizesToContents);
}

void LauncherWindow::createChrome()
{
#ifndef QT_NO_SHORTCUT
    QMenu* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("New Window", this, SLOT(newWindow()), QKeySequence::New);
    fileMenu->addAction(tr("Open File..."), this, SLOT(openFile()), QKeySequence::Open);
    fileMenu->addAction(tr("Open Location..."), this, SLOT(openLocation()), QKeySequence(Qt::CTRL | Qt::Key_L));
    fileMenu->addAction("Close Window", this, SLOT(close()), QKeySequence::Close);
    fileMenu->addSeparator();
    fileMenu->addAction("Take Screen Shot...", this, SLOT(screenshot()));
#ifndef QT_NO_PRINTER
    fileMenu->addAction(tr("Print..."), this, SLOT(print()), QKeySequence::Print);
#endif
    fileMenu->addSeparator();
    fileMenu->addAction("Quit", QApplication::instance(), SLOT(closeAllWindows()), QKeySequence(Qt::CTRL | Qt::Key_Q));

    QMenu* editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(page()->action(QWebPage::Undo));
    editMenu->addAction(page()->action(QWebPage::Redo));
    editMenu->addSeparator();
    editMenu->addAction(page()->action(QWebPage::Cut));
    editMenu->addAction(page()->action(QWebPage::Copy));
    editMenu->addAction(page()->action(QWebPage::Paste));
    editMenu->addSeparator();
    QAction* setEditable = editMenu->addAction("Set Editable", this, SLOT(setEditable(bool)));
    setEditable->setCheckable(true);

    QMenu* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(page()->action(QWebPage::Stop));
    viewMenu->addAction(page()->action(QWebPage::Reload));
    viewMenu->addSeparator();
    QAction* zoomIn = viewMenu->addAction("Zoom &In", this, SLOT(zoomIn()));
    QAction* zoomOut = viewMenu->addAction("Zoom &Out", this, SLOT(zoomOut()));
    QAction* resetZoom = viewMenu->addAction("Reset Zoom", this, SLOT(resetZoom()));
    QAction* zoomTextOnly = viewMenu->addAction("Zoom Text Only", this, SLOT(toggleZoomTextOnly(bool)));
    zoomTextOnly->setCheckable(true);
    zoomTextOnly->setChecked(false);
    viewMenu->addSeparator();
    viewMenu->addAction("Dump HTML", this, SLOT(dumpHtml()));
    // viewMenu->addAction("Dump plugins", this, SLOT(dumpPlugins()));

    zoomIn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
    zoomOut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    resetZoom->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));

    QMenu* formatMenu = new QMenu("F&ormat", this);
    m_formatMenuAction = menuBar()->addMenu(formatMenu);
    m_formatMenuAction->setVisible(false);
    formatMenu->addAction(page()->action(QWebPage::ToggleBold));
    formatMenu->addAction(page()->action(QWebPage::ToggleItalic));
    formatMenu->addAction(page()->action(QWebPage::ToggleUnderline));
    QMenu* writingMenu = formatMenu->addMenu(tr("Writing Direction"));
    writingMenu->addAction(page()->action(QWebPage::SetTextDirectionDefault));
    writingMenu->addAction(page()->action(QWebPage::SetTextDirectionLeftToRight));
    writingMenu->addAction(page()->action(QWebPage::SetTextDirectionRightToLeft));

    QMenu* windowMenu = menuBar()->addMenu("&Window");
    QAction* toggleFullScreen = windowMenu->addAction("Toggle FullScreen", this, SIGNAL(enteredFullScreenMode(bool)));
    toggleFullScreen->setShortcut(Qt::Key_F11);
    toggleFullScreen->setCheckable(true);
    toggleFullScreen->setChecked(false);
    // When exit fullscreen mode by clicking on the exit area (bottom right corner) we must
    // uncheck the Toggle FullScreen action.
    toggleFullScreen->connect(this, SIGNAL(enteredFullScreenMode(bool)), SLOT(setChecked(bool)));

    QWebSettings* settings = page()->settings();

    QMenu* toolsMenu = menuBar()->addMenu("&Develop");
    QMenu* graphicsViewMenu = toolsMenu->addMenu("QGraphicsView");
    QAction* toggleGraphicsView = graphicsViewMenu->addAction("Toggle use of QGraphicsView", this, SLOT(toggleWebView(bool)));
    toggleGraphicsView->setCheckable(true);
    toggleGraphicsView->setChecked(isGraphicsBased());

    QAction* toggleWebGL = toolsMenu->addAction("Toggle WebGL", this, SLOT(toggleWebGL(bool)));
    toggleWebGL->setCheckable(true);
    toggleWebGL->setChecked(settings->testAttribute(QWebSettings::WebGLEnabled));

    QAction* spatialNavigationAction = toolsMenu->addAction("Toggle Spatial Navigation", this, SLOT(toggleSpatialNavigation(bool)));
    spatialNavigationAction->setCheckable(true);
    spatialNavigationAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));

    QAction* toggleFrameFlattening = toolsMenu->addAction("Toggle Frame Flattening", this, SLOT(toggleFrameFlattening(bool)));
    toggleFrameFlattening->setCheckable(true);
    toggleFrameFlattening->setChecked(settings->testAttribute(QWebSettings::FrameFlatteningEnabled));

    QAction* touchMockAction = toolsMenu->addAction("Toggle touch mocking", this, SLOT(setTouchMocking(bool)));
    touchMockAction->setCheckable(true);
    touchMockAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_T));

    toolsMenu->addSeparator();

    QAction* toggleLocalStorage = toolsMenu->addAction("Enable Local Storage", this, SLOT(toggleLocalStorage(bool)));
    toggleLocalStorage->setCheckable(true);
    toggleLocalStorage->setChecked(m_windowOptions.useLocalStorage);

    QAction* toggleOfflineStorageDatabase = toolsMenu->addAction("Enable Offline Storage Database", this, SLOT(toggleOfflineStorageDatabase(bool)));
    toggleOfflineStorageDatabase->setCheckable(true);
    toggleOfflineStorageDatabase->setChecked(m_windowOptions.useOfflineStorageDatabase);

    QAction* toggleOfflineWebApplicationCache = toolsMenu->addAction("Enable Offline Web Application Cache", this, SLOT(toggleOfflineWebApplicationCache(bool)));
    toggleOfflineWebApplicationCache->setCheckable(true);
    toggleOfflineWebApplicationCache->setChecked(m_windowOptions.useOfflineWebApplicationCache);

    QAction* offlineStorageDefaultQuotaAction = toolsMenu->addAction("Set Offline Storage Default Quota Size", this, SLOT(setOfflineStorageDefaultQuota()));
    offlineStorageDefaultQuotaAction->setCheckable(true);
    offlineStorageDefaultQuotaAction->setChecked(m_windowOptions.offlineStorageDefaultQuotaSize);

    toolsMenu->addSeparator();

    QAction* userAgentAction = toolsMenu->addAction("Change User Agent", this, SLOT(showUserAgentDialog()));
    userAgentAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_U));

    toolsMenu->addAction("Select Elements...", this, SLOT(selectElements()));

    QAction* showInspectorAction = toolsMenu->addAction("Show Web Inspector", m_inspector, SLOT(setVisible(bool)), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_I));
    showInspectorAction->setCheckable(true);
    showInspectorAction->connect(m_inspector, SIGNAL(visibleChanged(bool)), SLOT(setChecked(bool)));
    toolsMenu->addSeparator();
    toolsMenu->addAction("Load URLs from file", this, SLOT(loadURLListFromFile()));

    // GraphicsView sub menu.
    QAction* toggleAcceleratedCompositing = graphicsViewMenu->addAction("Toggle Accelerated Compositing", this, SLOT(toggleAcceleratedCompositing(bool)));
    toggleAcceleratedCompositing->setCheckable(true);
    toggleAcceleratedCompositing->setChecked(settings->testAttribute(QWebSettings::AcceleratedCompositingEnabled));
    toggleAcceleratedCompositing->setEnabled(isGraphicsBased());
    toggleAcceleratedCompositing->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));

    QAction* toggleResizesToContents = graphicsViewMenu->addAction("Toggle Resizes To Contents Mode", this, SLOT(toggleResizesToContents(bool)));
    toggleResizesToContents->setCheckable(true);
    toggleResizesToContents->setChecked(m_windowOptions.resizesToContents);
    toggleResizesToContents->setEnabled(isGraphicsBased());
    toggleResizesToContents->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));

    QAction* toggleTiledBackingStore = graphicsViewMenu->addAction("Toggle Tiled Backing Store", this, SLOT(toggleTiledBackingStore(bool)));
    toggleTiledBackingStore->setCheckable(true);
    toggleTiledBackingStore->setChecked(m_windowOptions.useTiledBackingStore);
    toggleTiledBackingStore->setEnabled(isGraphicsBased());
    toggleTiledBackingStore->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));

#if defined(QT_CONFIGURED_WITH_OPENGL)
    QAction* toggleQGLWidgetViewport = graphicsViewMenu->addAction("Toggle use of QGLWidget Viewport", this, SLOT(toggleQGLWidgetViewport(bool)));
    toggleQGLWidgetViewport->setCheckable(true);
    toggleQGLWidgetViewport->setChecked(m_windowOptions.useQGLWidgetViewport);
    toggleQGLWidgetViewport->setEnabled(isGraphicsBased());
    toggleQGLWidgetViewport->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));
#endif

    QMenu* viewportUpdateMenu = graphicsViewMenu->addMenu("Change Viewport Update Mode");
    viewportUpdateMenu->setEnabled(isGraphicsBased());
    viewportUpdateMenu->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));

    QAction* fullUpdate = viewportUpdateMenu->addAction("FullViewportUpdate");
    fullUpdate->setCheckable(true);
    fullUpdate->setChecked((m_windowOptions.viewportUpdateMode == QGraphicsView::FullViewportUpdate) ? true : false);

    QAction* minimalUpdate = viewportUpdateMenu->addAction("MinimalViewportUpdate");
    minimalUpdate->setCheckable(true);
    minimalUpdate->setChecked((m_windowOptions.viewportUpdateMode == QGraphicsView::MinimalViewportUpdate) ? true : false);

    QAction* smartUpdate = viewportUpdateMenu->addAction("SmartViewportUpdate");
    smartUpdate->setCheckable(true);
    smartUpdate->setChecked((m_windowOptions.viewportUpdateMode == QGraphicsView::SmartViewportUpdate) ? true : false);

    QAction* boundingRectUpdate = viewportUpdateMenu->addAction("BoundingRectViewportUpdate");
    boundingRectUpdate->setCheckable(true);
    boundingRectUpdate->setChecked((m_windowOptions.viewportUpdateMode == QGraphicsView::BoundingRectViewportUpdate) ? true : false);

    QAction* noUpdate = viewportUpdateMenu->addAction("NoViewportUpdate");
    noUpdate->setCheckable(true);
    noUpdate->setChecked((m_windowOptions.viewportUpdateMode == QGraphicsView::NoViewportUpdate) ? true : false);

    QSignalMapper* signalMapper = new QSignalMapper(viewportUpdateMenu);
    signalMapper->setMapping(fullUpdate, QGraphicsView::FullViewportUpdate);
    signalMapper->setMapping(minimalUpdate, QGraphicsView::MinimalViewportUpdate);
    signalMapper->setMapping(smartUpdate, QGraphicsView::SmartViewportUpdate);
    signalMapper->setMapping(boundingRectUpdate, QGraphicsView::BoundingRectViewportUpdate);
    signalMapper->setMapping(noUpdate, QGraphicsView::NoViewportUpdate);

    connect(fullUpdate, SIGNAL(triggered()), signalMapper, SLOT(map()));
    connect(minimalUpdate, SIGNAL(triggered()), signalMapper, SLOT(map()));
    connect(smartUpdate, SIGNAL(triggered()), signalMapper, SLOT(map()));
    connect(boundingRectUpdate, SIGNAL(triggered()), signalMapper, SLOT(map()));
    connect(noUpdate, SIGNAL(triggered()), signalMapper, SLOT(map()));

    connect(signalMapper, SIGNAL(mapped(int)), this, SLOT(changeViewportUpdateMode(int)));

    QActionGroup* viewportUpdateModeActions = new QActionGroup(viewportUpdateMenu);
    viewportUpdateModeActions->addAction(fullUpdate);
    viewportUpdateModeActions->addAction(minimalUpdate);
    viewportUpdateModeActions->addAction(smartUpdate);
    viewportUpdateModeActions->addAction(boundingRectUpdate);
    viewportUpdateModeActions->addAction(noUpdate);

    graphicsViewMenu->addSeparator();

    QAction* flipAnimated = graphicsViewMenu->addAction("Animated Flip");
    flipAnimated->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));
    flipAnimated->setEnabled(isGraphicsBased());
    connect(flipAnimated, SIGNAL(triggered()), SLOT(animatedFlip()));

    QAction* flipYAnimated = graphicsViewMenu->addAction("Animated Y-Flip");
    flipYAnimated->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));
    flipYAnimated->setEnabled(isGraphicsBased());
    connect(flipYAnimated, SIGNAL(triggered()), SLOT(animatedYFlip()));

    QAction* cloneWindow = graphicsViewMenu->addAction("Clone Window", this, SLOT(cloneWindow()));
    cloneWindow->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));
    cloneWindow->setEnabled(isGraphicsBased());

    graphicsViewMenu->addSeparator();

    QAction* showFPS = graphicsViewMenu->addAction("Show FPS", this, SLOT(showFPS(bool)));
    showFPS->setCheckable(true);
    showFPS->setEnabled(isGraphicsBased());
    showFPS->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));
    showFPS->setChecked(m_windowOptions.showFrameRate);

    QMenu* settingsMenu = menuBar()->addMenu("&Settings");

    QAction* toggleAutoLoadImages = settingsMenu->addAction("Disable Auto Load Images", this, SLOT(toggleAutoLoadImages(bool)));
    toggleAutoLoadImages->setCheckable(true);
    toggleAutoLoadImages->setChecked(false);

    QAction* togglePlugins = settingsMenu->addAction("Disable Plugins", this, SLOT(togglePlugins(bool)));
    togglePlugins->setCheckable(true);
    togglePlugins->setChecked(false);

    QAction* toggleInterruptingJavaScripteEnabled = settingsMenu->addAction("Enable interrupting js scripts", this, SLOT(toggleInterruptingJavaScriptEnabled(bool)));
    toggleInterruptingJavaScripteEnabled->setCheckable(true);
    toggleInterruptingJavaScripteEnabled->setChecked(false);

    QAction* toggleJavascriptCanOpenWindows = settingsMenu->addAction("Enable js popup windows", this, SLOT(toggleJavascriptCanOpenWindows(bool)));
    toggleJavascriptCanOpenWindows->setCheckable(true);
    toggleJavascriptCanOpenWindows->setChecked(false);

#endif
}

bool LauncherWindow::isGraphicsBased() const
{
    return bool(qobject_cast<QGraphicsView*>(m_view));
}

void LauncherWindow::keyPressEvent(QKeyEvent* event)
{
#ifdef Q_WS_MAEMO_5
    switch (event->key()) {
    case Qt::Key_F7:
        zoomIn();
        event->accept();
        break;
    case Qt::Key_F8:
        zoomOut();
        event->accept();
        break;
    }
#endif
    MainWindow::keyPressEvent(event);
}

void LauncherWindow::grabZoomKeys(bool grab)
{
#ifdef Q_WS_MAEMO_5
    if (!winId()) {
        qWarning("Can't grab keys unless we have a window id");
        return;
    }

    Atom atom = XInternAtom(QX11Info::display(), "_HILDON_ZOOM_KEY_ATOM", False);
    if (!atom) {
        qWarning("Unable to obtain _HILDON_ZOOM_KEY_ATOM");
        return;
    }

    unsigned long val = (grab) ? 1 : 0;
    XChangeProperty(QX11Info::display(), winId(), atom, XA_INTEGER, 32, PropModeReplace, reinterpret_cast<unsigned char*>(&val), 1);
#endif
}

void LauncherWindow::sendTouchEvent()
{
    if (m_touchPoints.isEmpty())
        return;

    QEvent::Type type = QEvent::TouchUpdate;
    if (m_touchPoints.size() == 1) {
        if (m_touchPoints[0].state() == Qt::TouchPointReleased)
            type = QEvent::TouchEnd;
        else if (m_touchPoints[0].state() == Qt::TouchPointPressed)
            type = QEvent::TouchBegin;
    }

    QTouchEvent touchEv(type);
    touchEv.setTouchPoints(m_touchPoints);
    QCoreApplication::sendEvent(page(), &touchEv);

    // After sending the event, remove all touchpoints that were released
    if (m_touchPoints[0].state() == Qt::TouchPointReleased)
        m_touchPoints.removeAt(0);
    if (m_touchPoints.size() > 1 && m_touchPoints[1].state() == Qt::TouchPointReleased)
        m_touchPoints.removeAt(1);
}

bool LauncherWindow::eventFilter(QObject* obj, QEvent* event)
{
    // If click pos is the bottom right corner (square with size defined by gExitClickArea)
    // and the window is on FullScreen, the window must return to its original state.
    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent* ev = static_cast<QMouseEvent*>(event);
        if (windowState() == Qt::WindowFullScreen
            && ev->pos().x() > (width() - gExitClickArea)
            && ev->pos().y() > (height() - gExitClickArea)) {

            emit enteredFullScreenMode(false);
        }
    }

    if (!m_touchMocking)
        return QObject::eventFilter(obj, event);

    if (event->type() == QEvent::MouseButtonPress
        || event->type() == QEvent::MouseButtonRelease
        || event->type() == QEvent::MouseButtonDblClick
        || event->type() == QEvent::MouseMove) {

        QMouseEvent* ev = static_cast<QMouseEvent*>(event);
        if (ev->type() == QEvent::MouseMove
            && !(ev->buttons() & Qt::LeftButton))
            return false;

        QTouchEvent::TouchPoint touchPoint;
        touchPoint.setState(Qt::TouchPointMoved);
        if ((ev->type() == QEvent::MouseButtonPress
             || ev->type() == QEvent::MouseButtonDblClick))
            touchPoint.setState(Qt::TouchPointPressed);
        else if (ev->type() == QEvent::MouseButtonRelease)
            touchPoint.setState(Qt::TouchPointReleased);

        touchPoint.setId(0);
        touchPoint.setScreenPos(ev->globalPos());
        touchPoint.setPos(ev->pos());
        touchPoint.setPressure(1);

        // If the point already exists, update it. Otherwise create it.
        if (m_touchPoints.size() > 0 && !m_touchPoints[0].id())
            m_touchPoints[0] = touchPoint;
        else if (m_touchPoints.size() > 1 && !m_touchPoints[1].id())
            m_touchPoints[1] = touchPoint;
        else
            m_touchPoints.append(touchPoint);

        sendTouchEvent();
    } else if (event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_F
        && static_cast<QKeyEvent*>(event)->modifiers() == Qt::ControlModifier) {

        // If the keyboard point is already pressed, release it.
        // Otherwise create it and append to m_touchPoints.
        if (m_touchPoints.size() > 0 && m_touchPoints[0].id() == 1) {
            m_touchPoints[0].setState(Qt::TouchPointReleased);
            sendTouchEvent();
        } else if (m_touchPoints.size() > 1 && m_touchPoints[1].id() == 1) {
            m_touchPoints[1].setState(Qt::TouchPointReleased);
            sendTouchEvent();
        } else {
            QTouchEvent::TouchPoint touchPoint;
            touchPoint.setState(Qt::TouchPointPressed);
            touchPoint.setId(1);
            touchPoint.setScreenPos(QCursor::pos());
            touchPoint.setPos(m_view->mapFromGlobal(QCursor::pos()));
            touchPoint.setPressure(1);
            m_touchPoints.append(touchPoint);
            sendTouchEvent();

            // After sending the event, change the touchpoint state to stationary
            m_touchPoints.last().setState(Qt::TouchPointStationary);
        }
    }

    return false;
}

void LauncherWindow::loadStarted()
{
    m_view->setFocus(Qt::OtherFocusReason);
}

void LauncherWindow::loadFinished()
{
    QUrl url = page()->mainFrame()->url();
    addCompleterEntry(url);
    if (m_inputUrl.isEmpty())
        setAddressUrl(url.toString(QUrl::RemoveUserInfo));
    else {
        setAddressUrl(m_inputUrl);
        m_inputUrl = QString();
    }
}

void LauncherWindow::showLinkHover(const QString &link, const QString &toolTip)
{
#ifndef Q_WS_MAEMO_5
    statusBar()->showMessage(link);
#endif
#ifndef QT_NO_TOOLTIP
    if (!toolTip.isEmpty())
        QToolTip::showText(QCursor::pos(), toolTip);
#endif
}

void LauncherWindow::zoomAnimationFinished()
{
    if (!isGraphicsBased())
        return;
    QGraphicsWebView* view = static_cast<WebViewGraphicsBased*>(m_view)->graphicsWebView();
    view->setTiledBackingStoreFrozen(false);
}

void LauncherWindow::applyZoom()
{
#ifndef QT_NO_ANIMATION
    if (isGraphicsBased() && page()->settings()->testAttribute(QWebSettings::TiledBackingStoreEnabled)) {
        QGraphicsWebView* view = static_cast<WebViewGraphicsBased*>(m_view)->graphicsWebView();
        view->setTiledBackingStoreFrozen(true);
        if (!m_zoomAnimation) {
            m_zoomAnimation = new QPropertyAnimation(view, "scale");
            m_zoomAnimation->setStartValue(view->scale());
            connect(m_zoomAnimation, SIGNAL(finished()), this, SLOT(zoomAnimationFinished()));
        } else {
            m_zoomAnimation->stop();
            m_zoomAnimation->setStartValue(m_zoomAnimation->currentValue());
        }

        m_zoomAnimation->setDuration(300);
        m_zoomAnimation->setEndValue(qreal(m_currentZoom) / 100.);
        m_zoomAnimation->start();
        return;
    }
#endif
    page()->mainFrame()->setZoomFactor(qreal(m_currentZoom) / 100.0);
}

void LauncherWindow::zoomIn()
{
    int i = m_zoomLevels.indexOf(m_currentZoom);
    Q_ASSERT(i >= 0);
    if (i < m_zoomLevels.count() - 1)
        m_currentZoom = m_zoomLevels[i + 1];

    applyZoom();
}

void LauncherWindow::zoomOut()
{
    int i = m_zoomLevels.indexOf(m_currentZoom);
    Q_ASSERT(i >= 0);
    if (i > 0)
        m_currentZoom = m_zoomLevels[i - 1];

    applyZoom();
}

void LauncherWindow::resetZoom()
{
    m_currentZoom = 100;
    applyZoom();
}

void LauncherWindow::toggleZoomTextOnly(bool b)
{
    page()->settings()->setAttribute(QWebSettings::ZoomTextOnly, b);
}

void LauncherWindow::print()
{
#if !defined(QT_NO_PRINTER)
    QPrintPreviewDialog dlg(this);
    connect(&dlg, SIGNAL(paintRequested(QPrinter*)),
            page()->mainFrame(), SLOT(print(QPrinter*)));
    dlg.exec();
#endif
}

void LauncherWindow::screenshot()
{
    QPixmap pixmap = QPixmap::grabWidget(m_view);
    QLabel* label = 0;
#if !defined(Q_OS_SYMBIAN)
    label = new QLabel;
    label->setAttribute(Qt::WA_DeleteOnClose);
    label->setWindowTitle("Screenshot - Preview");
    label->setPixmap(pixmap);
    label->show();
#endif

#ifndef QT_NO_FILEDIALOG
    QString fileName = QFileDialog::getSaveFileName(label, "Screenshot");
    if (!fileName.isEmpty()) {
        pixmap.save(fileName, "png");
        if (label)
            label->setWindowTitle(QString("Screenshot - Saved at %1").arg(fileName));
    }
#endif

#if defined(QT_CONFIGURED_WITH_OPENGL)
    toggleQGLWidgetViewport(m_windowOptions.useQGLWidgetViewport);
#endif
}

void LauncherWindow::setEditable(bool on)
{
    page()->setContentEditable(on);
    m_formatMenuAction->setVisible(on);
}

/*
void LauncherWindow::dumpPlugins() {
    QList<QWebPluginInfo> plugins = QWebSettings::pluginDatabase()->plugins();
    foreach (const QWebPluginInfo plugin, plugins) {
        qDebug() << "Plugin:" << plugin.name();
        foreach (const QWebPluginInfo::MimeType mime, plugin.mimeTypes()) {
            qDebug() << "   " << mime.name;
        }
    }
}
*/

void LauncherWindow::dumpHtml()
{
    qDebug() << "HTML: " << page()->mainFrame()->toHtml();
}

void LauncherWindow::selectElements()
{
#ifndef QT_NO_INPUTDIALOG
    bool ok;
    QString str = QInputDialog::getText(this, "Select elements", "Choose elements",
                                        QLineEdit::Normal, "a", &ok);

    if (ok && !str.isEmpty()) {
        QWebElementCollection result =  page()->mainFrame()->findAllElements(str);
        foreach (QWebElement e, result)
            e.setStyleProperty("background-color", "yellow");
#ifndef Q_WS_MAEMO_5
        statusBar()->showMessage(QString("%1 element(s) selected").arg(result.count()), 5000);
#endif
    }
#endif
}

void LauncherWindow::setTouchMocking(bool on)
{
    m_touchMocking = on;
}

void LauncherWindow::toggleWebView(bool graphicsBased)
{
    m_windowOptions.useGraphicsView = graphicsBased;
    initializeView();
    menuBar()->clear();
    createChrome();
}

void LauncherWindow::toggleAcceleratedCompositing(bool toggle)
{
    m_windowOptions.useCompositing = toggle;
    page()->settings()->setAttribute(QWebSettings::AcceleratedCompositingEnabled, toggle);
}

void LauncherWindow::toggleTiledBackingStore(bool toggle)
{
    page()->settings()->setAttribute(QWebSettings::TiledBackingStoreEnabled, toggle);
}

void LauncherWindow::toggleResizesToContents(bool toggle)
{
    m_windowOptions.resizesToContents = toggle;
    static_cast<WebViewGraphicsBased*>(m_view)->setResizesToContents(toggle);
}

void LauncherWindow::toggleWebGL(bool toggle)
{
    m_windowOptions.useWebGL = toggle;
    page()->settings()->setAttribute(QWebSettings::WebGLEnabled, toggle);
}

void LauncherWindow::animatedFlip()
{
    qobject_cast<WebViewGraphicsBased*>(m_view)->animatedFlip();
}

void LauncherWindow::animatedYFlip()
{
    qobject_cast<WebViewGraphicsBased*>(m_view)->animatedYFlip();
}
void LauncherWindow::toggleSpatialNavigation(bool b)
{
    page()->settings()->setAttribute(QWebSettings::SpatialNavigationEnabled, b);
}

void LauncherWindow::toggleFullScreenMode(bool enable)
{
    bool alreadyEnabled = windowState() & Qt::WindowFullScreen;
    if (enable ^ alreadyEnabled)
        setWindowState(windowState() ^ Qt::WindowFullScreen);
}

void LauncherWindow::toggleFrameFlattening(bool toggle)
{
    m_windowOptions.useFrameFlattening = toggle;
    page()->settings()->setAttribute(QWebSettings::FrameFlatteningEnabled, toggle);
}

void LauncherWindow::toggleInterruptingJavaScriptEnabled(bool enable)
{
    page()->setInterruptingJavaScriptEnabled(enable);
}

void LauncherWindow::toggleJavascriptCanOpenWindows(bool enable)
{
    page()->settings()->setAttribute(QWebSettings::JavascriptCanOpenWindows, enable);
}

void LauncherWindow::toggleAutoLoadImages(bool enable)
{
    page()->settings()->setAttribute(QWebSettings::AutoLoadImages, !enable);
}

void LauncherWindow::togglePlugins(bool enable)
{
    page()->settings()->setAttribute(QWebSettings::PluginsEnabled, !enable);
}

#if defined(QT_CONFIGURED_WITH_OPENGL)
void LauncherWindow::toggleQGLWidgetViewport(bool enable)
{
    if (!isGraphicsBased())
        return;

    m_windowOptions.useQGLWidgetViewport = enable;
    WebViewGraphicsBased* view = static_cast<WebViewGraphicsBased*>(m_view);

    view->setViewport(enable ? new QGLWidget() : 0);
}
#endif

void LauncherWindow::changeViewportUpdateMode(int mode)
{
    m_windowOptions.viewportUpdateMode = QGraphicsView::ViewportUpdateMode(mode);

    if (!isGraphicsBased())
        return;

    WebViewGraphicsBased* view = static_cast<WebViewGraphicsBased*>(m_view);
    view->setViewportUpdateMode(m_windowOptions.viewportUpdateMode);
}

void LauncherWindow::showFPS(bool enable)
{
    if (!isGraphicsBased())
        return;

    m_windowOptions.showFrameRate = enable;
    WebViewGraphicsBased* view = static_cast<WebViewGraphicsBased*>(m_view);
    view->setFrameRateMeasurementEnabled(enable);

    if (!enable) {
#if defined(Q_WS_MAEMO_5) && defined(Q_OS_SYMBIAN)
        setWindowTitle("");
#else
        statusBar()->clearMessage();
#endif
    }
}

void LauncherWindow::showUserAgentDialog()
{
    QStringList items;
    QFile file(":/useragentlist.txt");
    if (file.open(QIODevice::ReadOnly)) {
         while (!file.atEnd())
            items << file.readLine().trimmed();
        file.close();
    }

    QSettings settings;
    QString customUserAgent = settings.value("CustomUserAgent").toString();
    if (!items.contains(customUserAgent) && !customUserAgent.isEmpty())
        items << customUserAgent;

    QDialog* dialog = new QDialog(this);
    dialog->resize(size().width() * 0.7, dialog->size().height());
    dialog->setMaximumHeight(dialog->size().height());
    dialog->setWindowTitle("Change User Agent");

    QVBoxLayout* layout = new QVBoxLayout(dialog);
    dialog->setLayout(layout);

#ifndef QT_NO_COMBOBOX
    QComboBox* combo = new QComboBox(dialog);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
    combo->setEditable(true);
    combo->insertItems(0, items);
    layout->addWidget(combo);

    int index = combo->findText(page()->userAgentForUrl(QUrl()));
    combo->setCurrentIndex(index);
#endif

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok
            | QDialogButtonBox::Cancel, Qt::Horizontal, dialog);
    connect(buttonBox, SIGNAL(accepted()), dialog, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), dialog, SLOT(reject()));
    layout->addWidget(buttonBox);

#ifndef QT_NO_COMBOBOX
    if (dialog->exec() && !combo->currentText().isEmpty()) {
        page()->setUserAgent(combo->currentText());
        if (!items.contains(combo->currentText()))
            settings.setValue("CustomUserAgent", combo->currentText());
    }
#endif

    delete dialog;
}

void LauncherWindow::loadURLListFromFile()
{
    QString selectedFile;
#ifndef QT_NO_FILEDIALOG
    selectedFile = QFileDialog::getOpenFileName(this, tr("Load URL list from file")
                                                       , QString(), tr("Text Files (*.txt);;All Files (*)"));
#endif
    if (selectedFile.isEmpty())
       return;

    m_urlLoader = new UrlLoader(this->page()->mainFrame(), selectedFile, 0, 0);
    m_urlLoader->loadNext();
}

void LauncherWindow::printURL(const QUrl& url)
{
    QTextStream output(stdout);
    output << "Loaded: " << url.toString() << endl;
}

void LauncherWindow::updateFPS(int fps)
{
    QString fpsStatusText = QString("Current FPS: %1").arg(fps);

#if defined(Q_WS_MAEMO_5) && defined(Q_OS_SYMBIAN)
    setWindowTitle(fpsStatusText);
#else
    statusBar()->showMessage(fpsStatusText);
#endif
}

void LauncherWindow::toggleLocalStorage(bool toggle)
{
    m_windowOptions.useLocalStorage = toggle;
    page()->settings()->setAttribute(QWebSettings::LocalStorageEnabled, toggle);
}

void LauncherWindow::toggleOfflineStorageDatabase(bool toggle)
{
    m_windowOptions.useOfflineStorageDatabase = toggle;
    page()->settings()->setAttribute(QWebSettings::OfflineStorageDatabaseEnabled, toggle);
}

void LauncherWindow::toggleOfflineWebApplicationCache(bool toggle)
{
    m_windowOptions.useOfflineWebApplicationCache = toggle;
    page()->settings()->setAttribute(QWebSettings::OfflineWebApplicationCacheEnabled, toggle);
}

void LauncherWindow::setOfflineStorageDefaultQuota()
{
    // For command line execution, quota size is taken from command line.   
    if (m_windowOptions.offlineStorageDefaultQuotaSize)
        page()->settings()->setOfflineStorageDefaultQuota(m_windowOptions.offlineStorageDefaultQuotaSize);
    else {
#ifndef QT_NO_INPUTDIALOG
        bool ok;
        // Maximum size is set to 25 * 1024 * 1024.
        int quotaSize = QInputDialog::getInt(this, "Offline Storage Default Quota Size" , "Quota Size", 0, 0, 26214400, 1, &ok);
        if (ok) 
            page()->settings()->setOfflineStorageDefaultQuota(quotaSize);
#endif
    }
}

LauncherWindow* LauncherWindow::newWindow()
{
    LauncherWindow* mw = new LauncherWindow(&m_windowOptions);
    mw->show();
    return mw;
}

LauncherWindow* LauncherWindow::cloneWindow()
{
    LauncherWindow* mw = new LauncherWindow(&m_windowOptions, qobject_cast<QGraphicsView*>(m_view)->scene());
    mw->show();
    return mw;
}

