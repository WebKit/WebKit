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
#include "mainwindow.h"
#include <qevent.h>
#include <qwebelement.h>
#include <qwebframe.h>
#include <qwebinspector.h>
#include <qwebsettings.h>
#include "urlloader.h"
#include "utils.h"
#include "webinspector.h"
#include "webpage.h"
#include "webview.h"

#ifndef NDEBUG
void QWEBKIT_EXPORT qt_drt_garbageCollector_collect();
#endif

class LauncherWindow : public MainWindow {
    Q_OBJECT

public:
    LauncherWindow(QString url = QString())
        : MainWindow(url)
        , currentZoom(100)
    {
        QSplitter* splitter = new QSplitter(Qt::Vertical, this);
        setCentralWidget(splitter);

        view = new WebViewTraditional(splitter);
        view->setPage(page());

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
        view->installEventFilter(this);
        touchMocking = false;
#endif

        connect(page(), SIGNAL(loadStarted()), this, SLOT(loadStarted()));
        connect(page(), SIGNAL(loadFinished(bool)), this, SLOT(loadFinished()));
        connect(page(), SIGNAL(linkHovered(const QString&, const QString&, const QString&)),
                this, SLOT(showLinkHover(const QString&, const QString&)));

        inspector = new WebInspector(splitter);
        inspector->setPage(page());
        inspector->hide();
        connect(this, SIGNAL(destroyed()), inspector, SLOT(deleteLater()));

        setupUI();

        // the zoom values are chosen to be like in Mozilla Firefox 3
        zoomLevels << 30 << 50 << 67 << 80 << 90;
        zoomLevels << 100;
        zoomLevels << 110 << 120 << 133 << 150 << 170 << 200 << 240 << 300;

        load(url);
    }

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    void sendTouchEvent()
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

    bool eventFilter(QObject* obj, QEvent* event)
    {
        if (!touchMocking || obj != view)
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
                touchPoint.setPos(view->mapFromGlobal(QCursor::pos()));
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

    QWebView* webView() const
    {
        return view;
    }

protected slots:
    void loadStarted()
    {
        view->setFocus(Qt::OtherFocusReason);
    }

    void loadFinished()
    {
        QUrl url = page()->mainFrame()->url();
        setAddressUrl(url.toString());
        addCompleterEntry(url);
    }

    void showLinkHover(const QString &link, const QString &toolTip)
    {
        statusBar()->showMessage(link);
#ifndef QT_NO_TOOLTIP
        if (!toolTip.isEmpty())
            QToolTip::showText(QCursor::pos(), toolTip);
#endif
    }

    void zoomIn()
    {
        int i = zoomLevels.indexOf(currentZoom);
        Q_ASSERT(i >= 0);
        if (i < zoomLevels.count() - 1)
            currentZoom = zoomLevels[i + 1];

        page()->mainFrame()->setZoomFactor(qreal(currentZoom) / 100.0);
    }

    void zoomOut()
    {
        int i = zoomLevels.indexOf(currentZoom);
        Q_ASSERT(i >= 0);
        if (i > 0)
            currentZoom = zoomLevels[i - 1];

        page()->mainFrame()->setZoomFactor(qreal(currentZoom) / 100.0);
    }

    void resetZoom()
    {
       currentZoom = 100;
       page()->mainFrame()->setZoomFactor(1.0);
    }

    void toggleZoomTextOnly(bool b)
    {
        page()->settings()->setAttribute(QWebSettings::ZoomTextOnly, b);
    }

    void print()
    {
#if !defined(QT_NO_PRINTER)
        QPrintPreviewDialog dlg(this);
        connect(&dlg, SIGNAL(paintRequested(QPrinter *)),
                view, SLOT(print(QPrinter *)));
        dlg.exec();
#endif
    }

    void screenshot()
    {
        QPixmap pixmap = QPixmap::grabWidget(view);
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

    void setEditable(bool on)
    {
        view->page()->setContentEditable(on);
        formatMenuAction->setVisible(on);
    }

    /*
    void dumpPlugins() {
        QList<QWebPluginInfo> plugins = QWebSettings::pluginDatabase()->plugins();
        foreach (const QWebPluginInfo plugin, plugins) {
            qDebug() << "Plugin:" << plugin.name();
            foreach (const QWebPluginInfo::MimeType mime, plugin.mimeTypes()) {
                qDebug() << "   " << mime.name;
            }
        }
    }
    */

    void dumpHtml()
    {
        qDebug() << "HTML: " << page()->mainFrame()->toHtml();
    }

    void selectElements()
    {
        bool ok;
        QString str = QInputDialog::getText(this, "Select elements", "Choose elements",
                                            QLineEdit::Normal, "a", &ok);

        if (ok && !str.isEmpty()) {
            QWebElementCollection result =  page()->mainFrame()->findAllElements(str);
            foreach (QWebElement e, result)
                e.setStyleProperty("background-color", "yellow");
            statusBar()->showMessage(QString("%1 element(s) selected").arg(result.count()), 5000);
        }
    }

    void setTouchMocking(bool on)
    {
#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
        touchMocking = on;
#endif
    }

public slots:

    void newWindow(const QString& url = QString())
    {
        LauncherWindow* mw = new LauncherWindow(url);
        mw->show();
    }

private:

    QVector<int> zoomLevels;
    int currentZoom;

    // create the status bar, tool bar & menu
    void setupUI()
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
        viewMenu->addAction(view->pageAction(QWebPage::Stop));
        viewMenu->addAction(view->pageAction(QWebPage::Reload));
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
    }

private:
    QWebView* view;
    WebInspector* inspector;

    QAction* formatMenuAction;

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    QList<QTouchEvent::TouchPoint> touchPoints;
    bool touchMocking;
#endif
};


QWebPage* WebPage::createWindow(QWebPage::WebWindowType)
{
    LauncherWindow* mw = new LauncherWindow;
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


#include "main.moc"

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

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QString defaultUrl = QString("file://%1/%2").arg(QDir::homePath()).arg(QLatin1String("index.html"));

    QWebSettings::setMaximumPagesInCache(4);

    app.setApplicationName("QtLauncher");
    app.setApplicationVersion("0.1");

    QWebSettings::setObjectCacheCapacities((16*1024*1024) / 8, (16*1024*1024) / 8, 16*1024*1024);

    QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled, true);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
    QWebSettings::enablePersistentStorage();

    // To allow QWebInspector's configuration persistence
    QCoreApplication::setOrganizationName("Nokia");
    QCoreApplication::setApplicationName("QtLauncher");

    const QStringList args = app.arguments();

    if (args.contains(QLatin1String("-r"))) {
        // robotized
        QString listFile = args.at(2);
        if (!(args.count() == 3) && QFile::exists(listFile)) {
            qDebug() << "Usage: QtLauncher -r listfile";
            exit(0);
        }
        LauncherWindow* window = new LauncherWindow;
        QWebView* view = window->webView();
        UrlLoader loader(view->page()->mainFrame(), listFile);
        QObject::connect(view->page()->mainFrame(), SIGNAL(loadFinished(bool)), &loader, SLOT(loadNext()));
        loader.loadNext();
        window->show();
        launcherMain(app);
    } else {
        LauncherWindow* window = 0;

        // Look though the args for something we can open
        for (int i = 1; i < args.count(); i++) {
            if (!args.at(i).startsWith("-")) {
                if (!window)
                    window = new LauncherWindow(args.at(i));
                else
                    window->newWindow(args.at(i));
            }
        }

        // If not, just open the default URL
        if (!window)
            window = new LauncherWindow(defaultUrl);

        window->show();
        launcherMain(app);
    }
}
