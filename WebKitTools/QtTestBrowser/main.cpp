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

#include "mainwindow.h"
#include "urlloader.h"
#include "utils.h"
#include "webinspector.h"
#include "webpage.h"
#include "webview.h"
#include "../../WebKit/qt/WebCoreSupport/DumpRenderTreeSupportQt.h"

#ifdef Q_WS_MAEMO_5
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#undef KeyPress
#endif

static const int gExitClickArea = 80;
static bool gUseGraphicsView = false;
static bool gUseCompositing = true;
static bool gCacheWebView = false;
static bool gShowFrameRate = false;
static QGraphicsView::ViewportUpdateMode gViewportUpdateMode = QGraphicsView::MinimalViewportUpdate;
static QUrl gInspectorUrl;
static bool gResizesToContents = false;
static bool gUseTiledBackingStore = false;

#if defined(Q_WS_MAEMO_5) || defined(Q_WS_S60)
static bool gUseFrameFlattening = true;
#else
static bool gUseFrameFlattening = false;
#endif

#if defined(QT_CONFIGURED_WITH_OPENGL)
static bool gUseQGLWidgetViewport = false;
#endif


class LauncherWindow : public MainWindow {
    Q_OBJECT

public:
    LauncherWindow(LauncherWindow* other = 0, bool shareScene = false);
    virtual ~LauncherWindow();

    virtual void keyPressEvent(QKeyEvent* event);
    void grabZoomKeys(bool grab);

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    void sendTouchEvent();
#endif

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

    void selectElements();

    void setTouchMocking(bool on);
    void toggleAcceleratedCompositing(bool toggle);
    void toggleTiledBackingStore(bool toggle);
    void toggleResizesToContents(bool toggle);

    void toggleWebGL(bool toggle);
    void initializeView(bool useGraphicsView = false);
    void toggleSpatialNavigation(bool b);
    void toggleFullScreenMode(bool enable);
    void showFPS(bool enable);
    void changeViewportUpdateMode(int mode);
    void toggleFrameFlattening(bool toggle);
    void toggleInterruptingJavaScriptEnabled(bool enable);

#if defined(QT_CONFIGURED_WITH_OPENGL)
    void toggleQGLWidgetViewport(bool enable);
#endif

    void showUserAgentDialog();

public slots:
    LauncherWindow* newWindow();
    LauncherWindow* cloneWindow();
    void updateFPS(int fps);

signals:
    void enteredFullScreenMode(bool on);

private:
    void createChrome();
    void applyZoom();

private:
    QVector<int> m_zoomLevels;
    int m_currentZoom;

    QWidget* m_view;
    WebInspector* m_inspector;

    QAction* m_formatMenuAction;
    QAction* m_flipAnimated;
    QAction* m_flipYAnimated;

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    QPropertyAnimation* m_zoomAnimation;
    QList<QTouchEvent::TouchPoint> m_touchPoints;
    bool m_touchMocking;
#endif

    void init(bool useGraphicsView = false);
    bool isGraphicsBased() const;
    void applyPrefs(LauncherWindow* other = 0);
};

LauncherWindow::LauncherWindow(LauncherWindow* other, bool shareScene)
    : MainWindow()
    , m_currentZoom(100)
    , m_view(0)
    , m_inspector(0)
    , m_formatMenuAction(0)
    , m_flipAnimated(0)
    , m_flipYAnimated(0)
#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    , m_zoomAnimation(0)
#endif
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
    setWindowState(Qt::WindowMaximized);
#else
    setWindowState(Qt::WindowNoState);
    resize(800, 600);
#endif

    initializeView(useGraphicsView);

    connect(page(), SIGNAL(loadStarted()), this, SLOT(loadStarted()));
    connect(page(), SIGNAL(loadFinished(bool)), this, SLOT(loadFinished()));
    connect(page(), SIGNAL(linkHovered(const QString&, const QString&, const QString&)),
            this, SLOT(showLinkHover(const QString&, const QString&)));
    connect(this, SIGNAL(enteredFullScreenMode(bool)), this, SLOT(toggleFullScreenMode(bool)));

    m_inspector = new WebInspector(splitter);
    if (!gInspectorUrl.isEmpty())
        m_inspector->setProperty("_q_inspectorUrl", gInspectorUrl);
    m_inspector->setPage(page());
    m_inspector->hide();
    connect(this, SIGNAL(destroyed()), m_inspector, SLOT(deleteLater()));

    // the zoom values are chosen to be like in Mozilla Firefox 3
    m_zoomLevels << 30 << 50 << 67 << 80 << 90;
    m_zoomLevels << 100;
    m_zoomLevels << 110 << 120 << 133 << 150 << 170 << 200 << 240 << 300;

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
    applySetting(QWebSettings::TiledBackingStoreEnabled, settings, other, gUseTiledBackingStore);
    applySetting(QWebSettings::WebGLEnabled, settings, other, false);
    applySetting(QWebSettings::FrameFlatteningEnabled, settings, other, gUseFrameFlattening);

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
#endif // QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)

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

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
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
#endif // QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)

    return false;
}

void LauncherWindow::loadStarted()
{
    m_view->setFocus(Qt::OtherFocusReason);
}

void LauncherWindow::loadFinished()
{
    QUrl url = page()->mainFrame()->url();
    setAddressUrl(url.toString(QUrl::RemoveUserInfo));
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

void LauncherWindow::zoomAnimationFinished()
{
    if (!isGraphicsBased())
        return;
    QGraphicsWebView* view = static_cast<WebViewGraphicsBased*>(m_view)->graphicsWebView();
    view->setTiledBackingStoreFrozen(false);
}

void LauncherWindow::applyZoom()
{
#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
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
            page()->mainFrame(), SLOT(print(QPrinter*)));
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
    m_touchMocking = on;
#endif
}

void LauncherWindow::toggleAcceleratedCompositing(bool toggle)
{
    gUseCompositing = toggle;
    page()->settings()->setAttribute(QWebSettings::AcceleratedCompositingEnabled, toggle);
}

void LauncherWindow::toggleTiledBackingStore(bool toggle)
{
    page()->settings()->setAttribute(QWebSettings::TiledBackingStoreEnabled, toggle);
}

void LauncherWindow::toggleResizesToContents(bool toggle)
{
    static_cast<WebViewGraphicsBased*>(m_view)->setResizesToContents(toggle);
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

        view->installEventFilter(this);

        m_view = view;
    } else {
        WebViewGraphicsBased* view = new WebViewGraphicsBased(splitter);
        view->setPage(page());

        if (m_flipAnimated)
            connect(m_flipAnimated, SIGNAL(triggered()), view, SLOT(animatedFlip()));

        if (m_flipYAnimated)
            connect(m_flipYAnimated, SIGNAL(triggered()), view, SLOT(animatedYFlip()));

        connect(view, SIGNAL(currentFPSUpdated(int)), this, SLOT(updateFPS(int)));

        view->installEventFilter(this);
        // The implementation of QAbstractScrollArea::eventFilter makes us need
        // to install the event filter also on the viewport of a QGraphicsView.
        view->viewport()->installEventFilter(this);

        m_view = view;
    }

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    m_touchMocking = false;
#endif
}

void LauncherWindow::toggleSpatialNavigation(bool b)
{
    page()->settings()->setAttribute(QWebSettings::SpatialNavigationEnabled, b);
}

void LauncherWindow::toggleFullScreenMode(bool enable)
{
    if (enable)
        setWindowState(Qt::WindowFullScreen);
    else {
#if defined(Q_WS_S60)
        setWindowState(Qt::WindowMaximized);
#else
        setWindowState(Qt::WindowNoState);
#endif
    }
}

void LauncherWindow::showFPS(bool enable)
{
    if (!isGraphicsBased())
        return;

    gShowFrameRate = enable;
    WebViewGraphicsBased* view = static_cast<WebViewGraphicsBased*>(m_view);
    view->setFrameRateMeasurementEnabled(enable);

    if (!enable) {
#if defined(Q_WS_MAEMO_5) && defined(Q_WS_S60)
        setWindowTitle("");
#else
        statusBar()->clearMessage();
#endif
    }
}

void LauncherWindow::changeViewportUpdateMode(int mode)
{
    gViewportUpdateMode = QGraphicsView::ViewportUpdateMode(mode);

    if (!isGraphicsBased())
        return;

    WebViewGraphicsBased* view = static_cast<WebViewGraphicsBased*>(m_view);
    view->setViewportUpdateMode(gViewportUpdateMode);
}

void LauncherWindow::toggleFrameFlattening(bool toggle)
{
    gUseFrameFlattening = toggle;
    page()->settings()->setAttribute(QWebSettings::FrameFlatteningEnabled, toggle);
}

void LauncherWindow::toggleInterruptingJavaScriptEnabled(bool enable)
{
    page()->setInterruptingJavaScriptEnabled(enable);
}

#if defined(QT_CONFIGURED_WITH_OPENGL)
void LauncherWindow::toggleQGLWidgetViewport(bool enable)
{
    if (!isGraphicsBased())
        return;

    gUseQGLWidgetViewport = enable;
    WebViewGraphicsBased* view = static_cast<WebViewGraphicsBased*>(m_view);

    view->setViewport(enable ? new QGLWidget() : 0);
}
#endif

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

    QComboBox* combo = new QComboBox(dialog);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
    combo->setEditable(true);
    combo->insertItems(0, items);
    layout->addWidget(combo);

    int index = combo->findText(page()->userAgentForUrl(QUrl()));
    combo->setCurrentIndex(index);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok
            | QDialogButtonBox::Cancel, Qt::Horizontal, dialog);
    connect(buttonBox, SIGNAL(accepted()), dialog, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), dialog, SLOT(reject()));
    layout->addWidget(buttonBox);

    if (dialog->exec() && !combo->currentText().isEmpty()) {
        page()->setUserAgent(combo->currentText());
        if (!items.contains(combo->currentText()))
            settings.setValue("CustomUserAgent", combo->currentText());
    }

    delete dialog;
}

LauncherWindow* LauncherWindow::newWindow()
{
    LauncherWindow* mw = new LauncherWindow(this, false);
    mw->show();
    return mw;
}

LauncherWindow* LauncherWindow::cloneWindow()
{
    LauncherWindow* mw = new LauncherWindow(this, true);
    mw->show();
    return mw;
}

void LauncherWindow::updateFPS(int fps)
{
    QString fpsStatusText = QString("Current FPS: %1").arg(fps);

#if defined(Q_WS_MAEMO_5) && defined(Q_WS_S60)
    setWindowTitle(fpsStatusText);
#else
    statusBar()->showMessage(fpsStatusText);
#endif
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
    m_formatMenuAction = menuBar()->addMenu(formatMenu);
    m_formatMenuAction->setVisible(false);
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

    QMenu* windowMenu = menuBar()->addMenu("&Window");
    QAction* toggleFullScreen = windowMenu->addAction("Toggle FullScreen", this, SIGNAL(enteredFullScreenMode(bool)));
    toggleFullScreen->setCheckable(true);
    toggleFullScreen->setChecked(false);

    // when exit fullscreen mode by clicking on the exit area (bottom right corner) we must
    // uncheck the Toggle FullScreen action
    toggleFullScreen->connect(this, SIGNAL(enteredFullScreenMode(bool)), SLOT(setChecked(bool)));

    QMenu* toolsMenu = menuBar()->addMenu("&Develop");
    toolsMenu->addAction("Select Elements...", this, SLOT(selectElements()));
    QAction* showInspectorAction = toolsMenu->addAction("Show Web Inspector", m_inspector, SLOT(setVisible(bool)), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_I));
    showInspectorAction->setCheckable(true);
    showInspectorAction->connect(m_inspector, SIGNAL(visibleChanged(bool)), SLOT(setChecked(bool)));

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

    QAction* toggleResizesToContents = graphicsViewMenu->addAction("Toggle Resizes To Contents Mode", this, SLOT(toggleResizesToContents(bool)));
    toggleResizesToContents->setCheckable(true);
    toggleResizesToContents->setChecked(false);
    toggleResizesToContents->setEnabled(isGraphicsBased());
    toggleResizesToContents->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));

    QAction* toggleTiledBackingStore = graphicsViewMenu->addAction("Toggle Tiled Backing Store", this, SLOT(toggleTiledBackingStore(bool)));
    toggleTiledBackingStore->setCheckable(true);
    toggleTiledBackingStore->setChecked(false);
    toggleTiledBackingStore->setEnabled(isGraphicsBased());
    toggleTiledBackingStore->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));

    QAction* spatialNavigationAction = toolsMenu->addAction("Toggle Spatial Navigation", this, SLOT(toggleSpatialNavigation(bool)));
    spatialNavigationAction->setCheckable(true);
    spatialNavigationAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));

    QAction* toggleFrameFlattening = toolsMenu->addAction("Toggle Frame Flattening", this, SLOT(toggleFrameFlattening(bool)));
    toggleFrameFlattening->setCheckable(true);
    toggleFrameFlattening->setChecked(settings->testAttribute(QWebSettings::FrameFlatteningEnabled));

    QAction* toggleInterruptingJavaScripteEnabled = toolsMenu->addAction("Enable interrupting js scripts", this, SLOT(toggleInterruptingJavaScriptEnabled(bool)));
    toggleInterruptingJavaScripteEnabled->setCheckable(true);
    toggleInterruptingJavaScripteEnabled->setChecked(false);

#if defined(QT_CONFIGURED_WITH_OPENGL)
    QAction* toggleQGLWidgetViewport = graphicsViewMenu->addAction("Toggle use of QGLWidget Viewport", this, SLOT(toggleQGLWidgetViewport(bool)));
    toggleQGLWidgetViewport->setCheckable(true);
    toggleQGLWidgetViewport->setChecked(gUseQGLWidgetViewport);
    toggleQGLWidgetViewport->setEnabled(isGraphicsBased());
    toggleQGLWidgetViewport->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));
#endif

    QAction* userAgentAction = toolsMenu->addAction("Change User Agent", this, SLOT(showUserAgentDialog()));
    userAgentAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_U));

    graphicsViewMenu->addSeparator();

    m_flipAnimated = graphicsViewMenu->addAction("Animated Flip");
    m_flipAnimated->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));
    m_flipAnimated->setEnabled(isGraphicsBased());

    m_flipYAnimated = graphicsViewMenu->addAction("Animated Y-Flip");
    m_flipYAnimated->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));
    m_flipYAnimated->setEnabled(isGraphicsBased());

    if (isGraphicsBased()) {
        WebViewGraphicsBased* view = static_cast<WebViewGraphicsBased*>(m_view);
        connect(m_flipAnimated, SIGNAL(triggered()), view, SLOT(animatedFlip()));
        connect(m_flipYAnimated, SIGNAL(triggered()), view, SLOT(animatedYFlip()));
    }

    graphicsViewMenu->addSeparator();

    QAction* cloneWindow = graphicsViewMenu->addAction("Clone Window", this, SLOT(cloneWindow()));
    cloneWindow->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));
    cloneWindow->setEnabled(isGraphicsBased());

    QAction* showFPS = graphicsViewMenu->addAction("Show FPS", this, SLOT(showFPS(bool)));
    showFPS->setCheckable(true);
    showFPS->setEnabled(isGraphicsBased());
    showFPS->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));
    showFPS->setChecked(gShowFrameRate);

    QMenu* viewportUpdateMenu = graphicsViewMenu->addMenu("Change Viewport Update Mode");
    viewportUpdateMenu->setEnabled(isGraphicsBased());
    viewportUpdateMenu->connect(toggleGraphicsView, SIGNAL(toggled(bool)), SLOT(setEnabled(bool)));

    QAction* fullUpdate = viewportUpdateMenu->addAction("FullViewportUpdate");
    fullUpdate->setCheckable(true);
    fullUpdate->setChecked((gViewportUpdateMode == QGraphicsView::FullViewportUpdate) ? true : false);

    QAction* minimalUpdate = viewportUpdateMenu->addAction("MinimalViewportUpdate");
    minimalUpdate->setCheckable(true);
    minimalUpdate->setChecked((gViewportUpdateMode == QGraphicsView::MinimalViewportUpdate) ? true : false);

    QAction* smartUpdate = viewportUpdateMenu->addAction("SmartViewportUpdate");
    smartUpdate->setCheckable(true);
    smartUpdate->setChecked((gViewportUpdateMode == QGraphicsView::SmartViewportUpdate) ? true : false);

    QAction* boundingRectUpdate = viewportUpdateMenu->addAction("BoundingRectViewportUpdate");
    boundingRectUpdate->setCheckable(true);
    boundingRectUpdate->setChecked((gViewportUpdateMode == QGraphicsView::BoundingRectViewportUpdate) ? true : false);

    QAction* noUpdate = viewportUpdateMenu->addAction("NoViewportUpdate");
    noUpdate->setCheckable(true);
    noUpdate->setChecked((gViewportUpdateMode == QGraphicsView::NoViewportUpdate) ? true : false);

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
    DumpRenderTreeSupportQt::garbageCollectorCollect();
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
    : QApplication(argc, argv, QApplication::GuiServer)
    , m_isRobotized(false)
{
    // To allow QWebInspector's configuration persistence
    setOrganizationName("Nokia");
    setApplicationName("QtTestBrowser");
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
    QString programName("QtTestBrowser");
    if (program.exists())
        programName = program.baseName();

    QList<QString> updateModes(enumToKeys(QGraphicsView::staticMetaObject,
            "ViewportUpdateMode", "ViewportUpdate"));

    if (args.contains("-help")) {
        qDebug() << "Usage:" << programName.toLatin1().data()
             << "[-graphicsbased]"
             << "[-no-compositing]"
             << QString("[-viewport-update-mode %1]").arg(formatKeys(updateModes)).toLatin1().data()
             << "[-cache-webview]"
             << "[-show-fps]"
             << "[-r list]"
             << "[-inspector-url location]"
             << "[-tiled-backing-store]"
             << "[-resizes-to-contents]"
             << "URLs";
        appQuit(0);
    }

    if (args.contains("-graphicsbased"))
        gUseGraphicsView = true;

    if (args.contains("-no-compositing")) {
        requiresGraphicsView("-no-compositing");
        gUseCompositing = false;
    }

    if (args.contains("-show-fps")) {
        requiresGraphicsView("-show-fps");
        gShowFrameRate = true;
    }

    if (args.contains("-cache-webview")) {
        requiresGraphicsView("-cache-webview");
        gCacheWebView = true;
    }

    if (args.contains("-tiled-backing-store")) {
        requiresGraphicsView("-tiled-backing-store");
        gUseTiledBackingStore = true;
    }

    if (args.contains("-resizes-to-contents")) {
        requiresGraphicsView("-resizes-to-contents");
        gResizesToContents = true;
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
        LauncherWindow* newWindow;
        if (!window)
            newWindow = window = new LauncherWindow();
        else
            newWindow = window->newWindow();

        newWindow->load(url);
    }

    window->show();
    return launcherMain(app);
}

#include "main.moc"
