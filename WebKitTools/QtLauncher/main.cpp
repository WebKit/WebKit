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
#include "urlloader.h"
#include "utils.h"
#include "webinspector.h"
#include "webpage.h"
#include "webview.h"

#ifndef NDEBUG
void QWEBKIT_EXPORT qt_drt_garbageCollector_collect();
#endif


class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QString url = QString()): currentZoom(100)
    {
        setAttribute(Qt::WA_DeleteOnClose);
#if QT_VERSION >= QT_VERSION_CHECK(4, 5, 0)
        if (qgetenv("QTLAUNCHER_USE_ARGB_VISUALS").toInt() == 1)
            setAttribute(Qt::WA_TranslucentBackground);
#endif

        QSplitter* splitter = new QSplitter(Qt::Vertical, this);
        setCentralWidget(splitter);

        view = new WebViewTraditional(splitter);
        WebPage* page = new WebPage(view);
        view->setPage(page);

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
        view->installEventFilter(this);
        touchMocking = false;
#endif

        connect(view, SIGNAL(loadFinished(bool)),
                this, SLOT(loadFinished()));
        connect(view, SIGNAL(titleChanged(const QString&)),
                this, SLOT(setWindowTitle(const QString&)));
        connect(view->page(), SIGNAL(linkHovered(const QString&, const QString&, const QString &)),
                this, SLOT(showLinkHover(const QString&, const QString&)));
        connect(view->page(), SIGNAL(windowCloseRequested()), this, SLOT(close()));

        inspector = new WebInspector(splitter);
        inspector->setPage(page);
        inspector->hide();
        connect(this, SIGNAL(destroyed()), inspector, SLOT(deleteLater()));

        setupUI();

        QFileInfo fi(url);
        if (fi.exists() && fi.isRelative())
            url = fi.absoluteFilePath();

        QUrl qurl = urlFromUserInput(url);
        if (qurl.scheme().isEmpty())
            qurl = QUrl("http://" + url + "/");
        if (qurl.isValid()) {
            urlEdit->setText(qurl.toString());
            view->load(qurl);
        }

        // the zoom values are chosen to be like in Mozilla Firefox 3
        zoomLevels << 30 << 50 << 67 << 80 << 90;
        zoomLevels << 100;
        zoomLevels << 110 << 120 << 133 << 150 << 170 << 200 << 240 << 300;
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
        QCoreApplication::sendEvent(view->page(), &touchEv);

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

    QWebPage* webPage() const
    {
        return view->page();
    }

    QWebView* webView() const
    {
        return view;
    }

protected slots:

    void openFile()
    {
        static const QString filter("HTML Files (*.htm *.html);;Text Files (*.txt);;Image Files (*.gif *.jpg *.png);;All Files (*)");

        QFileDialog fileDialog(this, tr("Open"), QString(), filter);
        fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
        fileDialog.setFileMode(QFileDialog::ExistingFile);
        fileDialog.setOptions(QFileDialog::ReadOnly);

        if (fileDialog.exec()) {
            QString selectedFile = fileDialog.selectedFiles()[0];
            if (!selectedFile.isEmpty())
                loadURL(QUrl::fromLocalFile(selectedFile));
        }
    }

    void changeLocation()
    {
        QString string = urlEdit->text();
        QUrl url = urlFromUserInput(string);
        if (url.scheme().isEmpty())
            url = QUrl("http://" + string + "/");
        loadURL(url);
    }

    void loadFinished()
    {
        urlEdit->setText(view->url().toString());

        QUrl::FormattingOptions opts;
        opts |= QUrl::RemoveScheme;
        opts |= QUrl::RemoveUserInfo;
        opts |= QUrl::StripTrailingSlash;
        QString s = view->url().toString(opts);
        s = s.mid(2);
        if (s.isEmpty())
            return;

        if (!urlList.contains(s))
            urlList += s;
        urlModel.setStringList(urlList);
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

        view->setZoomFactor(qreal(currentZoom) / 100.0);
    }

    void zoomOut()
    {
        int i = zoomLevels.indexOf(currentZoom);
        Q_ASSERT(i >= 0);
        if (i > 0)
            currentZoom = zoomLevels[i - 1];

        view->setZoomFactor(qreal(currentZoom) / 100.0);
    }

    void resetZoom()
    {
       currentZoom = 100;
       view->setZoomFactor(1.0);
    }

    void toggleZoomTextOnly(bool b)
    {
        view->page()->settings()->setAttribute(QWebSettings::ZoomTextOnly, b);
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
        qDebug() << "HTML: " << view->page()->mainFrame()->toHtml();
    }

    void selectElements()
    {
        bool ok;
        QString str = QInputDialog::getText(this, "Select elements", "Choose elements",
                                            QLineEdit::Normal, "a", &ok);

        if (ok && !str.isEmpty()) {
            QWebElementCollection result =  view->page()->mainFrame()->findAllElements(str);
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

    void newWindow(const QString &url = QString())
    {
        MainWindow* mw = new MainWindow(url);
        mw->show();
    }

private:

    QVector<int> zoomLevels;
    int currentZoom;

    void loadURL(const QUrl& url)
    {
        if (!url.isValid())
            return;
    
        urlEdit->setText(url.toString());
        view->load(url);
        view->setFocus(Qt::OtherFocusReason);
    }

    // create the status bar, tool bar & menu
    void setupUI()
    {
        progress = new QProgressBar(this);
        progress->setRange(0, 100);
        progress->setMinimumSize(100, 20);
        progress->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        progress->hide();
        statusBar()->addPermanentWidget(progress);

        connect(view, SIGNAL(loadProgress(int)), progress, SLOT(show()));
        connect(view, SIGNAL(loadProgress(int)), progress, SLOT(setValue(int)));
        connect(view, SIGNAL(loadFinished(bool)), progress, SLOT(hide()));

        urlEdit = new QLineEdit(this);
        urlEdit->setSizePolicy(QSizePolicy::Expanding, urlEdit->sizePolicy().verticalPolicy());
        connect(urlEdit, SIGNAL(returnPressed()),
                SLOT(changeLocation()));
        QCompleter* completer = new QCompleter(this);
        urlEdit->setCompleter(completer);
        completer->setModel(&urlModel);

        QToolBar* bar = addToolBar("Navigation");
        bar->addAction(view->pageAction(QWebPage::Back));
        bar->addAction(view->pageAction(QWebPage::Forward));
        bar->addAction(view->pageAction(QWebPage::Reload));
        bar->addAction(view->pageAction(QWebPage::Stop));
        bar->addWidget(urlEdit);

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
        editMenu->addAction(view->pageAction(QWebPage::Undo));
        editMenu->addAction(view->pageAction(QWebPage::Redo));
        editMenu->addSeparator();
        editMenu->addAction(view->pageAction(QWebPage::Cut));
        editMenu->addAction(view->pageAction(QWebPage::Copy));
        editMenu->addAction(view->pageAction(QWebPage::Paste));
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
        //viewMenu->addAction("Dump plugins", this, SLOT(dumpPlugins()));

        QMenu* formatMenu = new QMenu("F&ormat", this);
        formatMenuAction = menuBar()->addMenu(formatMenu);
        formatMenuAction->setVisible(false);
        formatMenu->addAction(view->pageAction(QWebPage::ToggleBold));
        formatMenu->addAction(view->pageAction(QWebPage::ToggleItalic));
        formatMenu->addAction(view->pageAction(QWebPage::ToggleUnderline));
        QMenu* writingMenu = formatMenu->addMenu(tr("Writing Direction"));
        writingMenu->addAction(view->pageAction(QWebPage::SetTextDirectionDefault));
        writingMenu->addAction(view->pageAction(QWebPage::SetTextDirectionLeftToRight));
        writingMenu->addAction(view->pageAction(QWebPage::SetTextDirectionRightToLeft));

        view->pageAction(QWebPage::Back)->setShortcut(QKeySequence::Back);
        view->pageAction(QWebPage::Stop)->setShortcut(Qt::Key_Escape);
        view->pageAction(QWebPage::Forward)->setShortcut(QKeySequence::Forward);
        view->pageAction(QWebPage::Reload)->setShortcut(QKeySequence::Refresh);
        view->pageAction(QWebPage::Undo)->setShortcut(QKeySequence::Undo);
        view->pageAction(QWebPage::Redo)->setShortcut(QKeySequence::Redo);
        view->pageAction(QWebPage::Cut)->setShortcut(QKeySequence::Cut);
        view->pageAction(QWebPage::Copy)->setShortcut(QKeySequence::Copy);
        view->pageAction(QWebPage::Paste)->setShortcut(QKeySequence::Paste);
        zoomIn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
        zoomOut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
        resetZoom->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
        view->pageAction(QWebPage::ToggleBold)->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
        view->pageAction(QWebPage::ToggleItalic)->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
        view->pageAction(QWebPage::ToggleUnderline)->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_U));

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

    QWebView* view;
    QLineEdit* urlEdit;
    QProgressBar* progress;
    WebInspector* inspector;

    QAction* formatMenuAction;

    QStringList urlList;
    QStringListModel urlModel;

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    QList<QTouchEvent::TouchPoint> touchPoints;
    bool touchMocking;
#endif
};


QWebPage* WebPage::createWindow(QWebPage::WebWindowType)
{
    MainWindow* mw = new MainWindow;
    mw->show();
    return mw->webPage();
}

QObject* WebPage::createPlugin(const QString &classId, const QUrl &url, const QStringList &paramNames, const QStringList &paramValues)
{
    Q_UNUSED(url);
    Q_UNUSED(paramNames);
    Q_UNUSED(paramValues);

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
        MainWindow* window = new MainWindow;
        QWebView* view = window->webView();
        UrlLoader loader(view->page()->mainFrame(), listFile);
        QObject::connect(view->page()->mainFrame(), SIGNAL(loadFinished(bool)), &loader, SLOT(loadNext()));
        loader.loadNext();
        window->show();
        launcherMain(app);
    } else {
        MainWindow* window = 0;

        // Look though the args for something we can open
        for (int i = 1; i < args.count(); i++) {
            if (!args.at(i).startsWith("-")) {
                if (!window)
                    window = new MainWindow(args.at(i));
                else
                    window->newWindow(args.at(i));
            }
        }

        // If not, just open the default URL
        if (!window)
            window = new MainWindow(defaultUrl);

        window->show();
        launcherMain(app);
    }
}
