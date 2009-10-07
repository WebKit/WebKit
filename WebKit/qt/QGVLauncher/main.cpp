/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2009 Kenneth Christiansen <kenneth@webkit.org>
 * Copyright (C) 2009 Antonio Gomes <antonio.gomes@openbossa.org>
 * Copyright (C) 2009 Girish Ramakrishnan <girish@forwardbias.in>
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

#include <QDebug>
#include <QFile>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsWidget>
#include <QNetworkRequest>
#include <QTextStream>
#include <QVector>
#include <QtGui>
#include <QtNetwork/QNetworkProxy>
#include <cstdio>
#include <qwebelement.h>
#include <qwebframe.h>
#include <qgraphicswebview.h>
#include <qwebpage.h>
#include <qwebsettings.h>
#include <qwebview.h>

class WebPage : public QWebPage {
    Q_OBJECT

public:
    WebPage(QWidget* parent = 0) : QWebPage(parent)
    {
        applyProxy();
    }
    virtual QWebPage* createWindow(QWebPage::WebWindowType);

private:
    void applyProxy();
};

class MainView : public QGraphicsView {
    Q_OBJECT

public:
    MainView(QWidget* parent) : QGraphicsView(parent), m_mainWidget(0)
    {
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

    void setMainWidget(QGraphicsWidget* widget)
    {
        QRectF rect(QRect(QPoint(0, 0), size()));
        widget->setGeometry(rect);
        m_mainWidget = widget;
    }

    void resizeEvent(QResizeEvent* event)
    {
        QGraphicsView::resizeEvent(event);
        if (!m_mainWidget)
            return;
        QRectF rect(QPoint(0, 0), event->size());
        m_mainWidget->setGeometry(rect);
    }

public slots:
    void flip()
    {
#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
        QSizeF center = m_mainWidget->boundingRect().size() / 2;
        QPointF centerPoint = QPointF(center.width(), center.height());
        m_mainWidget->setTransformOriginPoint(centerPoint);

        m_mainWidget->setRotation(m_mainWidget->rotation() ? 0 : 180);
#endif
    }

    void animatedFlip()
    {
#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
        QSizeF center = m_mainWidget->boundingRect().size() / 2;
        QPointF centerPoint = QPointF(center.width(), center.height());
        m_mainWidget->setTransformOriginPoint(centerPoint);

        QPropertyAnimation* animation = new QPropertyAnimation(m_mainWidget, "rotation", this);
        animation->setDuration(1000);
        animation->setStartValue(m_mainWidget->rotation());
        animation->setEndValue(m_mainWidget->rotation() + 180);
        animation->start(QAbstractAnimation::DeleteWhenStopped);
#endif
    }
private:
    QGraphicsWidget* m_mainWidget;
};

class SharedScene : public QSharedData {
public:
    SharedScene()
    {
        m_scene = new QGraphicsScene;

        m_item = new QGraphicsWebView;
        m_item->setPage(new WebPage());

        m_scene->addItem(m_item);
        m_scene->setActiveWindow(m_item);
    }

    ~SharedScene()
    {
        delete m_item;
        delete m_scene;
    }

    QGraphicsScene* scene() const { return m_scene; }
    QGraphicsWebView* webView() const { return m_item; }

private:
    QGraphicsScene* m_scene;
    QGraphicsWebView* m_item;
};


class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QExplicitlySharedDataPointer<SharedScene> other)
        : QMainWindow(), view(new MainView(this)), scene(other)
    {
        init();
    }

    MainWindow()
        : QMainWindow(), view(new MainView(this)), scene(new SharedScene())
    {
        init();
    }

    void init()
    {
        setAttribute(Qt::WA_DeleteOnClose);

        view->setScene(scene->scene());
        view->setFrameShape(QFrame::NoFrame);
        view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setCentralWidget(view);

        view->setMainWidget(scene->webView());

        connect(scene->webView(), SIGNAL(loadFinished()), this, SLOT(loadFinished()));
        connect(scene->webView(), SIGNAL(titleChanged(const QString&)), this, SLOT(setWindowTitle(const QString&)));
        connect(scene->webView()->page(), SIGNAL(windowCloseRequested()), this, SLOT(close()));

        resize(640, 480);
        buildUI();
    }

    void load(const QString& url)
    {
        QUrl deducedUrl = guessUrlFromString(url);
        if (!deducedUrl.isValid())
            deducedUrl = QUrl("http://" + url + "/");

        urlEdit->setText(deducedUrl.toEncoded());
        scene->webView()->load(deducedUrl);
        scene->webView()->setFocus(Qt::OtherFocusReason);
    }

    QUrl guessUrlFromString(const QString& string)
    {
        QString input(string);
        QFileInfo fi(input);
        if (fi.exists() && fi.isRelative())
            input = fi.absoluteFilePath();

        return QWebView::guessUrlFromString(input);
    }

    QWebPage* page() const
    {
        return scene->webView()->page();
    }

protected slots:
    void changeLocation()
    {
        load(urlEdit->text());
    }

    void loadFinished()
    {
        QUrl url = scene->webView()->url();
        urlEdit->setText(url.toString());

        QUrl::FormattingOptions opts;
        opts |= QUrl::RemoveScheme;
        opts |= QUrl::RemoveUserInfo;
        opts |= QUrl::StripTrailingSlash;
        QString s = url.toString(opts);
        s = s.mid(2);
        if (s.isEmpty())
            return;
        //FIXME: something missing here
    }

public slots:
    void newWindow(const QString &url = QString())
    {
        MainWindow* mw = new MainWindow();
        mw->load(url);
        mw->show();
    }

    void clone()
    {
        MainWindow* mw = new MainWindow(scene);
        mw->show();
    }

    void flip()
    {
        view->flip();
    }

    void animatedFlip()
    {
        view->animatedFlip();
    }
private:
    void buildUI()
    {
        QWebPage* page = scene->webView()->page();
        urlEdit = new QLineEdit(this);
        urlEdit->setSizePolicy(QSizePolicy::Expanding, urlEdit->sizePolicy().verticalPolicy());
        connect(urlEdit, SIGNAL(returnPressed()), SLOT(changeLocation()));

        QToolBar* bar = addToolBar("Navigation");
        bar->addAction(page->action(QWebPage::Back));
        bar->addAction(page->action(QWebPage::Forward));
        bar->addAction(page->action(QWebPage::Reload));
        bar->addAction(page->action(QWebPage::Stop));
        bar->addWidget(urlEdit);

        QMenu* fileMenu = menuBar()->addMenu("&File");
        fileMenu->addAction("New Window", this, SLOT(newWindow()));
        fileMenu->addAction("Clone view", this, SLOT(clone()));
        fileMenu->addAction("Close", this, SLOT(close()));

        QMenu* viewMenu = menuBar()->addMenu("&View");
        viewMenu->addAction(page->action(QWebPage::Stop));
        viewMenu->addAction(page->action(QWebPage::Reload));

        QMenu* fxMenu = menuBar()->addMenu("&Effects");
        fxMenu->addAction("Flip", this, SLOT(flip()));
        fxMenu->addAction("Animated Flip", this, SLOT(animatedFlip()));
    }

private:
    MainView* view;
    QExplicitlySharedDataPointer<SharedScene> scene;

    QLineEdit* urlEdit;
};

QWebPage* WebPage::createWindow(QWebPage::WebWindowType)
{
    MainWindow* mw = new MainWindow;
    mw->show();
    return mw->page();
}

void WebPage::applyProxy()
{
    QUrl proxyUrl = QWebView::guessUrlFromString(qgetenv("http_proxy"));

    if (proxyUrl.isValid() && !proxyUrl.host().isEmpty()) {
        int proxyPort = (proxyUrl.port() > 0) ? proxyUrl.port() : 8080;
        networkAccessManager()->setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, proxyUrl.host(), proxyPort));
    }
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QString url = QString("file://%1/%2").arg(QDir::homePath()).arg(QLatin1String("index.html"));

    app.setApplicationName("GQVLauncher");

    QWebSettings::setObjectCacheCapacities((16 * 1024 * 1024) / 8, (16 * 1024 * 1024) / 8, 16 * 1024 * 1024);
    QWebSettings::setMaximumPagesInCache(4);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled, true);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::DeveloperExtrasEnabled, true);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::LocalStorageEnabled, true);

    const QStringList args = app.arguments();
    if (args.count() > 1)
        url = args.at(1);

    MainWindow* window = new MainWindow;
    window->load(url);

    for (int i = 2; i < args.count(); i++)
        window->newWindow(args.at(i));

    window->show();
    return app.exec();
}

#include "main.moc"
