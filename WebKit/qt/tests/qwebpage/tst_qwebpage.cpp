/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/


#include <QtTest/QtTest>

#include <qwebpage.h>
#include <qwidget.h>
#include <qwebview.h>
#include <qwebframe.h>
#include <qwebhistory.h>
#include <qnetworkrequest.h>
#include <QDebug>
#include <QMenu>

// Will try to wait for the condition while allowing event processing
#define QTRY_COMPARE(__expr, __expected) \
    do { \
        const int __step = 50; \
        const int __timeout = 5000; \
        if ((__expr) != (__expected)) { \
            QTest::qWait(0); \
        } \
        for (int __i = 0; __i < __timeout && ((__expr) != (__expected)); __i+=__step) { \
            QTest::qWait(__step); \
        } \
        QCOMPARE(__expr, __expected); \
    } while(0)

//TESTED_CLASS=
//TESTED_FILES=

// Task 160192
/**
 * Starts an event loop that runs until the given signal is received.
 Optionally the event loop
 * can return earlier on a timeout.
 *
 * \return \p true if the requested signal was received
 *         \p false on timeout
 */
static bool waitForSignal(QObject* obj, const char* signal, int timeout = 0)
{
    QEventLoop loop;
    QObject::connect(obj, signal, &loop, SLOT(quit()));
    QTimer timer;
    QSignalSpy timeoutSpy(&timer, SIGNAL(timeout()));
    if (timeout > 0) {
        QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
        timer.setSingleShot(true);
        timer.start(timeout);
    }
    loop.exec();
    return timeoutSpy.isEmpty();
}

class tst_QWebPage : public QObject
{
    Q_OBJECT

public:
    tst_QWebPage();
    virtual ~tst_QWebPage();

public slots:
    void init();
    void cleanup();

private slots:
    void acceptNavigationRequest();
    void loadFinished();
    void acceptNavigationRequestWithNewWindow();
    void userStyleSheet();
    void modified();
    void contextMenuCrash();

private:


private:
    QWebView* m_view;
    QWebPage* m_page;
};

tst_QWebPage::tst_QWebPage()
{
}

tst_QWebPage::~tst_QWebPage()
{
}

void tst_QWebPage::init()
{
    m_view = new QWebView();
    m_page = m_view->page();
}

void tst_QWebPage::cleanup()
{
    delete m_view;
}

class NavigationRequestOverride : public QWebPage
{
public:
    NavigationRequestOverride(QWebView* parent, bool initialValue) : QWebPage(parent), m_acceptNavigationRequest(initialValue) {}

    bool m_acceptNavigationRequest;
protected:
    virtual bool acceptNavigationRequest(QWebFrame* frame, const QNetworkRequest &request, QWebPage::NavigationType type) {
        Q_UNUSED(frame);
        Q_UNUSED(request);
        Q_UNUSED(type);

        return m_acceptNavigationRequest;
    }
};

void tst_QWebPage::acceptNavigationRequest()
{
    QSignalSpy loadSpy(m_view, SIGNAL(loadFinished(bool)));

    NavigationRequestOverride* newPage = new NavigationRequestOverride(m_view, false);
    m_view->setPage(newPage);

    m_view->setHtml(QString("<html><body><form name='tstform' action='data:text/html,foo'method='get'>"
                            "<input type='text'><input type='submit'></form></body></html>"), QUrl());
    QTRY_COMPARE(loadSpy.count(), 1);

    m_view->page()->mainFrame()->evaluateJavaScript("tstform.submit();");

    newPage->m_acceptNavigationRequest = true;
    m_view->page()->mainFrame()->evaluateJavaScript("tstform.submit();");
    QTRY_COMPARE(loadSpy.count(), 2);

    QCOMPARE(m_view->page()->mainFrame()->toPlainText(), QString("foo?"));

    // Restore default page
    m_view->setPage(0);
}


void tst_QWebPage::loadFinished()
{
    QSignalSpy spyLoadStarted(m_view, SIGNAL(loadStarted()));
    QSignalSpy spyLoadFinished(m_view, SIGNAL(loadFinished(bool)));

    m_view->setHtml(QString("data:text/html,<frameset cols=\"25%,75%\"><frame src=\"data:text/html,"
                            "<head><meta http-equiv='refresh' content='1'></head>foo \">"
                            "<frame src=\"data:text/html,bar\"></frameset>"), QUrl());
    QTRY_COMPARE(spyLoadFinished.count(), 1);

    QTest::qWait(3000);

    QVERIFY(spyLoadStarted.count() > 1);
    QVERIFY(spyLoadFinished.count() > 1);

    spyLoadFinished.clear();

    m_view->setHtml(QString("data:text/html,<frameset cols=\"25%,75%\"><frame src=\"data:text/html,"
                            "foo \"><frame src=\"data:text/html,bar\"></frameset>"), QUrl());
    QTRY_COMPARE(spyLoadFinished.count(), 1);
    QCOMPARE(spyLoadFinished.count(), 1);
}

class TestPage : public QWebPage
{
public:
    TestPage(QObject* parent = 0) : QWebPage(parent) {}

    struct Navigation {
        QPointer<QWebFrame> frame;
        QNetworkRequest request;
        NavigationType type;
    };

    QList<Navigation> navigations;
    QList<QWebPage*> createdWindows;

    virtual bool acceptNavigationRequest(QWebFrame* frame, const QNetworkRequest &request, NavigationType type) {
        Navigation n;
        n.frame = frame;
        n.request = request;
        n.type = type;
        navigations.append(n);
        return true;
    }

    virtual QWebPage* createWindow(WebWindowType type) {
        QWebPage* page = new TestPage(this);
        createdWindows.append(page);
        return page;
    }
};

void tst_QWebPage::acceptNavigationRequestWithNewWindow()
{
    TestPage* page = new TestPage(m_view);
    page->settings()->setAttribute(QWebSettings::LinksIncludedInFocusChain, true);
    m_page = page;
    m_view->setPage(m_page);

    m_view->setUrl(QString("data:text/html,<a href=\"data:text/html,Reached\" target=\"_blank\">Click me</a>"));
    QVERIFY(::waitForSignal(m_view, SIGNAL(loadFinished(bool))));

    QFocusEvent fe(QEvent::FocusIn);
    m_page->event(&fe);

    QVERIFY(m_page->focusNextPrevChild(/*next*/ true));

    QKeyEvent keyEnter(QEvent::KeyPress, Qt::Key_Enter, Qt::NoModifier);
    m_page->event(&keyEnter);

    QCOMPARE(page->navigations.count(), 2);

    TestPage::Navigation n = page->navigations.at(1);
    QVERIFY(n.frame.isNull());
    QCOMPARE(n.request.url().toString(), QString("data:text/html,Reached"));
    QVERIFY(n.type == QWebPage::NavigationTypeLinkClicked);

    QCOMPARE(page->createdWindows.count(), 1);
}

class TestNetworkManager : public QNetworkAccessManager
{
public:
    TestNetworkManager(QObject* parent) : QNetworkAccessManager(parent) {}

    QList<QUrl> requestedUrls;

protected:
    virtual QNetworkReply* createRequest(Operation op, const QNetworkRequest &request, QIODevice* outgoingData) {
        requestedUrls.append(request.url());
        return QNetworkAccessManager::createRequest(op, request, outgoingData);
    }
};

void tst_QWebPage::userStyleSheet()
{
    TestNetworkManager* networkManager = new TestNetworkManager(m_page);
    m_page->setNetworkAccessManager(networkManager);
    networkManager->requestedUrls.clear();

    m_page->settings()->setUserStyleSheetUrl(QUrl("data:text/css,p { background-image: url('http://does.not/exist.png');}"));
    m_view->setHtml("<p>hello world</p>");
    QVERIFY(::waitForSignal(m_view, SIGNAL(loadFinished(bool))));

    QVERIFY(networkManager->requestedUrls.count() >= 2);
    QCOMPARE(networkManager->requestedUrls.at(0), QUrl("data:text/css,p { background-image: url('http://does.not/exist.png');}"));
    QCOMPARE(networkManager->requestedUrls.at(1), QUrl("http://does.not/exist.png"));
}

void tst_QWebPage::modified()
{
    m_page->mainFrame()->setUrl(QUrl("data:text/html,<body>blub"));
    QVERIFY(::waitForSignal(m_view, SIGNAL(loadFinished(bool))));

    m_page->mainFrame()->setUrl(QUrl("data:text/html,<body id=foo contenteditable>blah"));
    QVERIFY(::waitForSignal(m_view, SIGNAL(loadFinished(bool))));

    QVERIFY(!m_page->isModified());

//    m_page->mainFrame()->evaluateJavaScript("alert(document.getElementById('foo'))");
    m_page->mainFrame()->evaluateJavaScript("document.getElementById('foo').focus()");
    m_page->mainFrame()->evaluateJavaScript("document.execCommand('InsertText', true, 'Test');");

    QVERIFY(m_page->isModified());

    m_page->mainFrame()->evaluateJavaScript("document.execCommand('Undo', true);");

    QVERIFY(!m_page->isModified());

    m_page->mainFrame()->evaluateJavaScript("document.execCommand('Redo', true);");

    QVERIFY(m_page->isModified());

    QVERIFY(m_page->history()->canGoBack());
    QVERIFY(!m_page->history()->canGoForward());
    QCOMPARE(m_page->history()->count(), 2);
    QVERIFY(m_page->history()->backItem().isValid());
    QVERIFY(!m_page->history()->forwardItem().isValid());

    m_page->history()->back();
    QVERIFY(::waitForSignal(m_view, SIGNAL(loadFinished(bool))));

    QVERIFY(!m_page->history()->canGoBack());
    QVERIFY(m_page->history()->canGoForward());

    QVERIFY(!m_page->isModified());

    QVERIFY(m_page->history()->currentItemIndex() == 0);

    m_page->history()->setMaximumItemCount(3);
    QVERIFY(m_page->history()->maximumItemCount() == 3);

    QVariant variant("string test");
    m_page->history()->currentItem().setUserData(variant);
    QVERIFY(m_page->history()->currentItem().userData().toString() == "string test");

    m_page->mainFrame()->setUrl(QUrl("data:text/html,<body>This is second page"));
    m_page->mainFrame()->setUrl(QUrl("data:text/html,<body>This is third page"));
    QVERIFY(m_page->history()->count() == 2);
    m_page->mainFrame()->setUrl(QUrl("data:text/html,<body>This is fourth page"));
    QVERIFY(m_page->history()->count() == 2);
    m_page->mainFrame()->setUrl(QUrl("data:text/html,<body>This is fifth page"));
    QVERIFY(::waitForSignal(m_page->mainFrame(), SIGNAL(aboutToUpdateHistory(QWebHistoryItem*))));
}

void tst_QWebPage::contextMenuCrash()
{
    QWebView view;
    view.setHtml("<p>test");
    view.page()->updatePositionDependentActions(QPoint(0, 0));
    QMenu* contextMenu = 0;
    foreach (QObject* child, view.children()) {
        contextMenu = qobject_cast<QMenu*>(child);
        if (contextMenu)
            break;
    }
    QVERIFY(contextMenu);
    delete contextMenu;
}

QTEST_MAIN(tst_QWebPage)
#include "tst_qwebpage.moc"
