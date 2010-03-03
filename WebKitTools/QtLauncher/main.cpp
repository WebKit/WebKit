/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
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

#include <QtGui>
#include <QtNetwork/QNetworkRequest>
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

#ifndef NDEBUG
void QWEBKIT_EXPORT qt_drt_garbageCollector_collect();
#endif


static bool gUseGraphicsView = false;
static bool gUseCompositing = false;
static bool gCacheWebView = false;
static bool gShowFrameRate = false;
static QGraphicsView::ViewportUpdateMode gViewportUpdateMode = QGraphicsView::MinimalViewportUpdate;
static QUrl gInspectorUrl;

class LauncherWindow : public MainWindow {
    Q_OBJECT

public:
    LauncherWindow(LauncherWindow* other = 0, bool shareScene = false);
    virtual ~LauncherWindow();

    virtual void keyPressEvent(QKeyEvent* event);
    void grabZoomKeys(bool grab);

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    void sendTouchEvent();
    bool eventFilter(QObject* obj, QEvent* event);
#endif

protected slots:
    void loadStarted();
    void loadFinished();

    void showLinkHover(const QString &link, const QString &toolTip);

    void zoomIn();
    void zoomOut();
    void resetZoom();
    void toggleZoomTextOnly(bool on);

    void print();
    void screenshot();

    void setEditable(bool on);

    /* void dumpPlugins() */
    void dumpHtml();

    void selectElements();

    void setTouchMocking(bool on);
    void toggleAcceleratedCompositing(bool toggle);
    void toggleWebGL(bool toggle);
    void initializeView(bool useGraphicsView = false);

public slots:
    void newWindow();
    void cloneWindow();

private:
    void createChrome();

private:
    QVector<int> zoomLevels;
    int currentZoom;

    QWidget* m_view;
    WebInspector* inspector;

    QAction* formatMenuAction;
    QAction* flipAnimated;
    QAction* flipYAnimated;

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    QList<QTouchEvent::TouchPoint> touchPoints;
    bool touchMocking;
#endif

    void init(bool useGraphicsView = false);
    bool isGraphicsBased() const;
    void applyPrefs(LauncherWindow* other = 0);
};

LauncherWindow::LauncherWindow(LauncherWindow* other, bool shareScene)
    : MainWindow()
    , currentZoom(100)
    , m_view(0)
    , inspector(0)
    , formatMenuAction(0)
    , flipAnimated(0)
    , flipYAnimated(0)
{
    if (other) {
        init(other->isGraphicsBased());
        applyPrefs(other);
        if (shareScene && other->isGraphicsBased()) {
            QGraphicsView* otherView = static_cast<QGraphicsView*>(other->m_view);
            static_cast<QGraphicsView*>(m_view)->setScene(otherView->scene());
        }
    } else {
        init(gUseGraphicsView);
        applyPrefs();
    }

    createChrome();
}

LauncherWindow::~LauncherWindow()
{
    grabZoomKeys(false);
}

void LauncherWindow::init(bool useGraphicsView)
{
    QSplitter* splitter = new QSplitter(Qt::Vertical, this);
    setCentralWidget(splitter);

#if defined(Q_WS_S60)
    showMaximized();
#else
    resize(800, 600);
#endif

    initializeView(useGraphicsView);

    connect(page(), SIGNAL(loadStarted()), this, SLOT(loadStarted()));
    connect(page(), SIGNAL(loadFinished(bool)), this, SLOT(loadFinished()));
    connect(page(), SIGNAL(linkHovered(const QString&, const QString&, const QString&)),
            this, SLOT(showLinkHover(const QString&, const QString&)));

    if (!gInspectorUrl.isEmpty())
      page()->settings()->setInspectorUrl(gInspectorUrl);

    inspector = new WebInspector(splitter);
    inspector->setPage(page());
    inspector->hide();
    connect(this, SIGNAL(destroyed()), inspector, SLOT(deleteLater()));

    // the zoom values are chosen to be like in Mozilla Firefox 3
    zoomLevels << 30 << 50 << 67 << 80 << 90;
    zoomLevels << 100;
    zoomLevels << 110 << 120 << 133 << 150 << 170 << 200 << 240 << 300;

    grabZoomKeys(true);
}

bool LauncherWindow::isGraphicsBased() const
{
    return bool(qobject_cast<QGraphicsView*>(m_view));
}

inline void applySetting(QWebSettings::WebAttribute type, QWebSettings* settings, QWebSettings* other, bool defaultValue)
{
    settings->setAttribute(type, other ? other->testAttribute(type) : defaultValue);
}

void LauncherWindow::applyPrefs(LauncherWindow* source)
{
    QWebSettings* other = source ? source->page()->settings() : 0;
    QWebSettings* settings = page()->settings();

    applySetting(QWebSettings::AcceleratedCompositingEnabled, settings, other, gUseCompositing);
    applySetting(QWebSettings::WebGLEnabled, settings, other, false);

    if (!isGraphicsBased())
        return;

    WebViewGraphicsBased* view = static_cast<WebViewGraphicsBased*>(m_view);
    WebViewGraphicsBased* otherView = source ? qobject_cast<WebViewGraphicsBased*>(source->m_view) : 0;

    view->setViewportUpdateMode(otherView ? otherView->viewportUpdateMode() : gViewportUpdateMode);
    view->setFrameRateMeasurementEnabled(otherView ? otherView->frameRateMeasurementEnabled() : gShowFrameRate);

    if (otherView)
        view->setItemCacheMode(otherView->itemCacheMode());
    else
        view->setItemCacheMode(gCacheWebView ? QGraphicsItem::DeviceCoordinateCache : QGraphicsItem::NoCache);
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

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
void LauncherWindow::sendTouchEvent()
{
    if (touchPoints.isEmpty())
        return;

    QEvent::Type type = QEvent::TouchUpdate;
    if (touchPoints.size() == 1) {
        if (touchPoints[0].state() == Qt::TouchPointReleased)
            type = QEvent::TouchEnd;
        else if (touchPoints[0].state() == Qt::TouchPointPressed)
            type = QEvent::TouchBegin;
    }

    QTouchEvent touchEv(type);
    touchEv.setTouchPoints(touchPoints);
    QCoreApplication::sendEvent(page(), &touchEv);

    // After sending the event, remove all touchpoints that were released
    if (touchPoints[0].state() == Qt::TouchPointReleased)
        touchPoints.removeAt(0);
    if (touchPoints.size() > 1 && touchPoints[1].state() == Qt::TouchPointReleased)
        touchPoints.removeAt(1);
}

bool LauncherWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (!touchMocking || obj != m_view)
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
        if (touchPoints.size() > 0 && !touchPoints[0].id())
            touchPoints[0] = touchPoint;
        else if (touchPoints.size() > 1 && !touchPoints[1].id())
            touchPoints[1] = touchPoint;
        else
            touchPoints.append(touchPoint);

        sendTouchEvent();
    } else if (event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_F
        && static_cast<QKeyEvent*>(event)->modifiers() == Qt::ControlModifier) {

        // If the keyboard point is already pressed, release it.
        // Otherwise create it and append to touchPoints.
        if (touchPoints.size() > 0 && touchPoints[0].id() == 1) {
            touchPoints[0].setState(Qt::TouchPointReleased);
            sendTouchEvent();
        } else if (touchPoints.size() > 1 && touchPoints[1].id() == 1) {
            touchPoints[1].setState(Qt::TouchPointReleased);
            sendTouchEvent();
        } else {
            QTouchEvent::TouchPoint touchPoint;
            touchPoint.setState(Qt::TouchPointPressed);
            touchPoint.setId(1);
            touchPoint.setScreenPos(QCursor::pos());
            touchPoint.setPos(m_view->mapFromGlobal(QCursor::pos()));
            touchPoint.setPressure(1);
            touchPoints.append(touchPoint);
            sendTouchEvent();

            // After sending the event, change the touchpoint state to stationary
            touchPoints.last().setState(Qt::TouchPointStationary);
        }
    }
    return false;
}
#endif // QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)

void LauncherWindow::loadStarted()
{
    m_view->setFocus(Qt::OtherFocusReason);
}

void LauncherWindow::loadFinished()
{
    QUrl url = page()->mainFrame()->url();
    setAddressUrl(url.toString());
    addCompleterEntry(url);
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

void LauncherWindow::zoomIn()
{
    int i = zoomLevels.indexOf(currentZoom);
    Q_ASSERT(i >= 0);
    if (i < zoomLevels.count() - 1)
        currentZoom = zoomLevels[i + 1];

    page()->mainFrame()->setZoomFactor(qreal(currentZoom) / 100.0);
}

void LauncherWindow::zoomOut()
{
    int i = zoomLevels.indexOf(currentZoom);
    Q_ASSERT(i >= 0);
    if (i > 0)
        currentZoom = zoomLevels[i - 1];

    page()->mainFrame()->setZoomFactor(qreal(currentZoom) / 100.0);
}

void LauncherWindow::resetZoom()
{
    currentZoom = 100;
    page()->mainFrame()->setZoomFactor(1.0);
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
            m_view, SLOT(print(QPrinter*)));
    dlg.exec();
#endif
}

void LauncherWindow::screenshot()
{
    QPixmap pixmap = QPixmap::grabWidget(m_view);
    QLabel* label = new QLabel;
    label->setAttribute(Qt::WA_DeleteOnClose);
    label->setWindowTitle("Screenshot - Preview");
    label->setPixmap(pixmap);
    label->show();

    QString fileName = QFileDialog::getSaveFileName(label, "Screenshot");
    if (!fileName.isEmpty()) {
        pixmap.save(fileName, "png");
        label->setWindowTitle(QString("Screenshot - Saved at %1").arg(fileName));
    }
}

void LauncherWindow::setEditable(bool on)
{
    page()->setContentEditable(on);
    formatMenuAction->setVisible(on);
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
}

void LauncherWindow::setTouchMocking(bool on)
{
#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    touchMocking = on;
#endif
}

void LauncherWindow::toggleAcceleratedCompositing(bool toggle)
{
    page()->settings()->setAttribute(QWebSettings::AcceleratedCompositingEnabled, toggle);
}

void LauncherWindow::toggleWebGL(bool toggle)
{
    page()->settings()->setAttribute(QWebSettings::WebGLEnabled, toggle);
}

void LauncherWindow::initializeView(bool useGraphicsView)
{
    delete m_view;

    QSplitter* splitter = static_cast<QSplitter*>(centralWidget());

    if (!useGraphicsView) {
        WebViewTraditional* view = new WebViewTraditional(splitter);
        view->setPage(page());
        m_view = view;
    } else {
        WebViewGraphicsBased* view = new WebViewGraphicsBased(splitter);
        view->setPage(page());

        if (flipAnimated)
            connect(flipAnimated, SIGNAL(triggered()), view, SLOT(animatedFlip()));

        if (flipYAnimated)
            connect(flipYAnimated, SIGNAL(triggered()), view, SLOT(animatedYFlip()));

        m_view = view;
    }

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    m_view->installEventFilter(this);
    touchMocking = false;
#endif
}

void LauncherWindow::newWindow()
{
    LauncherWindow* mw = new LauncherWindow(this, false);
    mw->show();
}

void LauncherWindow::cloneWindow()
{
    LauncherWindow* mw = new LauncherWindow(this, true);
    mw->show();
}

void LauncherWindow::createChrome()
{
    QMenu* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("New Window", this, SLOT(newWindow()), QKeySequence::New);
    fileMenu->addAction(tr("Open File..."), this, SLOT(openFile()), QKeySequence::Open);
    fileMenu->addAction("Close Window", this, SLOT(close()), QKeySequence::Close);
    fileMenu->addSeparator();
    fileMenu->addAction("Take Screen Shot...", this, SLOT(screenshot()));
    fileMenu->addAction(tr("Print..."), this, SLOT(print()), QKeySequence::Print);
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

    QMenu* formatMenu = new QMenu("F&ormat", this);
    formatMenuAction = menuBar()->addMenu(formatMenu);
    formatMenuAction->setVisible(false);
    formatMenu->addAction(page()->action(QWebPage::ToggleBold));
    formatMenu->addAction(page()->action(QWebPage::ToggleItalic));
    formatMenu->addAction(page()->action(QWebPage::ToggleUnderline));
    QMenu* writingMenu = formatMenu->addMenu(tr("Writing Direction"));
    writingMenu->addAction(page()->action(QWebPage::SetTextDirectionDefault));
    writingMenu->addAction(page()->action(QWebPage::SetTextDirectionLeftToRight));
    writingMenu->addAction(page()->action(QWebPage::SetTextDirectionRightToLeft));

    zoomIn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
    zoomOut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    resetZoom->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));

    QMenu* toolsMenu = menuBar()->addMenu("&Develop");
    toolsMenu->addAction("Select Elements...", this, SLOT(selectElements()));
    QAction* showInspectorAction = toolsMenu->addAction("Show Web Inspector", inspector, SLOT(setVisible(bool)), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_I));
    showInspectorAction->setCheckable(true);
    showInspectorAction->connect(inspector, SIGNAL(visibleChanged(bool)), SLOT(setChecked(bool)));

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    QAction* touchMockAction = toolsMenu->addAction("Toggle multitouch mocking", this, SLOT(setTouchMocking(bool)));
    touchMockAction->setCheckable(true);
    touchMockAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_T));
#endif

    QWebSettings* settings = page()->settings();

    QMenu* graphicsViewMenu = toolsMenu->addMenu("QGraphicsView");
    QAction* toggleGraphicsView = graphicsViewMenu->addAction("Toggle use of QGraphicsView", this, SLOT(initializeView(bool)));
    toggleGraphicsView->setCheckable(true);
    toggleGraphicsView->setChecked(isGraphicsBased());

    QAction* toggleWebGL = toolsMenu->addAction("Toggle WebGL", this, SLOT(toggleWebGL(bool)));
    toggleWebGL->setCheckable(true);
    toggleWebGL->setChecked(settings->testAttribute(QWebSettings::WebGLEnabled));

    QAction* toggleAcceleratedCompositing = graphicsViewMenu->addAction("Toggle Accelerated Compositing", this, SLOT(toggleAcceleratedCompositing(bool)));
    toggleAcceleratedCompositing->setCheckable(true);
    toggleAcceleratedCompositing->setChecked(settings->testAttribute(QWebSettings::AcceleratedCompositingEnabled));
    toggleAcceleratedCompositing->setEnabled(isGraphicsBased());
    toggleAcceleratedCompositing->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));

    graphicsViewMenu->addSeparator();

    flipAnimated = graphicsViewMenu->addAction("Animated Flip");
    flipAnimated->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));
    flipAnimated->setEnabled(isGraphicsBased());

    flipYAnimated = graphicsViewMenu->addAction("Animated Y-Flip");
    flipYAnimated->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));
    flipYAnimated->setEnabled(isGraphicsBased());

    if (isGraphicsBased()) {
        WebViewGraphicsBased* view = static_cast<WebViewGraphicsBased*>(m_view);
        connect(flipAnimated, SIGNAL(triggered()), view, SLOT(animatedFlip()));
        connect(flipYAnimated, SIGNAL(triggered()), view, SLOT(animatedYFlip()));
    }

    graphicsViewMenu->addSeparator();

    QAction* cloneWindow = graphicsViewMenu->addAction("Clone Window", this, SLOT(cloneWindow()));
    cloneWindow->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));
    cloneWindow->setEnabled(isGraphicsBased());
}

QWebPage* WebPage::createWindow(QWebPage::WebWindowType type)
{
    LauncherWindow* mw = new LauncherWindow;
    if (type == WebModalDialog)
        mw->setWindowModality(Qt::ApplicationModal);
    mw->show();
    return mw->page();
}

QObject* WebPage::createPlugin(const QString &classId, const QUrl&, const QStringList&, const QStringList&)
{
    if (classId == "alien_QLabel") {
        QLabel* l = new QLabel;
        l->winId();
        return l;
    }

#ifndef QT_NO_UITOOLS
    QUiLoader loader;
    return loader.createWidget(classId, view());
#else
    Q_UNUSED(classId);
    return 0;
#endif
}


int launcherMain(const QApplication& app)
{
#ifndef NDEBUG
    int retVal = app.exec();
    qt_drt_garbageCollector_collect();
    QWebSettings::clearMemoryCaches();
    return retVal;
#else
    return app.exec();
#endif
}

class LauncherApplication : public QApplication {
    Q_OBJECT

public:
    LauncherApplication(int& argc, char** argv);
    QStringList urls() const { return m_urls; }
    bool isRobotized() const { return m_isRobotized; }

private:
    void handleUserOptions();
    void applyDefaultSettings();

private:
    bool m_isRobotized;
    QStringList m_urls;
};

void LauncherApplication::applyDefaultSettings()
{
    QWebSettings::setMaximumPagesInCache(4);

    QWebSettings::setObjectCacheCapacities((16*1024*1024) / 8, (16*1024*1024) / 8, 16*1024*1024);

    QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled, true);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
    QWebSettings::enablePersistentStorage();
}

LauncherApplication::LauncherApplication(int& argc, char** argv)
    : QApplication(argc, argv)
    , m_isRobotized(false)
{
    // To allow QWebInspector's configuration persistence
    setOrganizationName("Nokia");
    setApplicationName("QtLauncher");
    setApplicationVersion("0.1");

    applyDefaultSettings();

    handleUserOptions();
}

static void requiresGraphicsView(const QString& option)
{
    if (gUseGraphicsView)
        return;
    appQuit(1, QString("%1 only works in combination with the -graphicsbased option").arg(option));
}

void LauncherApplication::handleUserOptions()
{
    QStringList args = arguments();
    QFileInfo program(args.at(0));
    QString programName("QtLauncher");
    if (program.exists())
        programName = program.baseName();

    QList<QString> updateModes(enumToKeys(QGraphicsView::staticMetaObject,
            "ViewportUpdateMode", "ViewportUpdate"));

    if (args.contains("-help")) {
        qDebug() << "Usage:" << programName.toLatin1().data()
             << "[-graphicsbased]"
             << "[-compositing]"
             << QString("[-viewport-update-mode %1]").arg(formatKeys(updateModes)).toLatin1().data()
             << "[-cache-webview]"
             << "[-show-fps]"
             << "[-r list]"
             << "[-inspector-url location]"
             << "URLs";
        appQuit(0);
    }

    if (args.contains("-graphicsbased"))
        gUseGraphicsView = true;

    if (args.contains("-compositing")) {
        requiresGraphicsView("-compositing");
        gUseCompositing = true;
    }

    if (args.contains("-show-fps")) {
        requiresGraphicsView("-show-fps");
        gShowFrameRate = true;
    }

    if (args.contains("-cache-webview")) {
        requiresGraphicsView("-cache-webview");
        gCacheWebView = true;
    }

    QString arg1("-viewport-update-mode");
    int modeIndex = args.indexOf(arg1);
    if (modeIndex != -1) {
        requiresGraphicsView(arg1);

        QString mode = takeOptionValue(&args, modeIndex);
        if (mode.isEmpty())
            appQuit(1, QString("%1 needs a value of one of [%2]").arg(arg1).arg(formatKeys(updateModes)));
        int idx = updateModes.indexOf(mode);
        if (idx == -1)
            appQuit(1, QString("%1 value has to be one of [%2]").arg(arg1).arg(formatKeys(updateModes)));

        gViewportUpdateMode = static_cast<QGraphicsView::ViewportUpdateMode>(idx);
    }

    QString inspectorUrlArg("-inspector-url");
    int inspectorUrlIndex = args.indexOf(inspectorUrlArg);
    if (inspectorUrlIndex != -1)
       gInspectorUrl = takeOptionValue(&args, inspectorUrlIndex);

    int robotIndex = args.indexOf("-r");
    if (robotIndex != -1) {
        QString listFile = takeOptionValue(&args, robotIndex);
        if (listFile.isEmpty())
            appQuit(1, "-r needs a list file to start in robotized mode");
        if (!QFile::exists(listFile))
            appQuit(1, "The list file supplied to -r does not exist.");

        m_isRobotized = true;
        m_urls = QStringList(listFile);
        return;
    }

    int lastArg = args.lastIndexOf(QRegExp("^-.*"));
    m_urls = (lastArg != -1) ? args.mid(++lastArg) : args.mid(1);
}


int main(int argc, char **argv)
{
    LauncherApplication app(argc, argv);

    if (app.isRobotized()) {
        LauncherWindow* window = new LauncherWindow();
        UrlLoader loader(window->page()->mainFrame(), app.urls().at(0));
        QObject::connect(window->page()->mainFrame(), SIGNAL(loadFinished(bool)), &loader, SLOT(loadNext()));
        loader.loadNext();
        window->show();
        return launcherMain(app);
    }

    QStringList urls = app.urls();

    if (urls.isEmpty()) {
        QString defaultUrl = QString("file://%1/%2").arg(QDir::homePath()).arg(QLatin1String("index.html"));
        if (QDir(defaultUrl).exists())
            urls.append(defaultUrl);
        else
            urls.append("");
    }

    LauncherWindow* window = 0;
    foreach (QString url, urls) {
        if (!window)
            window = new LauncherWindow();
        else
            window->newWindow();

        window->load(url);
    }

    window->show();
    return launcherMain(app);
}

#include "main.moc"
