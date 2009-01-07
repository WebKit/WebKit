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
#include <QLineEdit>
#include <QMenu>
#include <qwebsecurityorigin.h>
#include <qwebdatabase.h>
#include <QPushButton>

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
    void initTestCase();
    void cleanupTestCase();

    void acceptNavigationRequest();
    void loadFinished();
    void acceptNavigationRequestWithNewWindow();
    void userStyleSheet();
    void modified();
    void contextMenuCrash();
    void database();
    void createPlugin();
    void destroyPlugin();
    void createViewlessPlugin();

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

void tst_QWebPage::initTestCase()
{
}

void tst_QWebPage::cleanupTestCase()
{
    // clean up the database files we created
    QFile::remove("Databases.db");
    QDir::current().rmdir("http_www.myexample.com_0");
    QFile::remove("http_www.myexample.com_0.localstorage");
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

    virtual QWebPage* createWindow(WebWindowType) {
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
    QVERIFY(::waitForSignal(m_page, SIGNAL(saveFrameStateRequested(QWebFrame*, QWebHistoryItem*))));
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

void tst_QWebPage::database()
{
    QString path = QDir::currentPath();
    m_page->settings()->setOfflineStoragePath(path);
    QVERIFY(m_page->settings()->offlineStoragePath() == path);

    QWebSettings::setOfflineStorageDefaultQuota(1024 * 1024);
    QVERIFY(QWebSettings::offlineStorageDefaultQuota() == 1024 * 1024);

    m_page->settings()->setOfflineWebApplicationCachePath(path);
    QVERIFY(m_page->settings()->offlineWebApplicationCachePath() == path);

    m_page->settings()->setLocalStorageDatabasePath(path);
    QVERIFY(m_page->settings()->localStorageDatabasePath() == path);

    QString dbFileName = path + "Databases.db";

    if (QFile::exists(dbFileName))
        QFile::remove(dbFileName);

    qRegisterMetaType<QWebFrame*>("QWebFrame*");
    QSignalSpy spy(m_page, SIGNAL(databaseQuotaExceeded(QWebFrame *, QString)));
    m_view->setHtml(QString("<html><head><script>var db; db=openDatabase('testdb', '1.0', 'test database API', 50000); </script></head><body><div></div></body></html>"), QUrl("http://www.myexample.com"));
    QTRY_COMPARE(spy.count(), 1);
    m_page->mainFrame()->evaluateJavaScript("var db2; db2=openDatabase('testdb', '1.0', 'test database API', 50000);");
    QTRY_COMPARE(spy.count(),1);

    m_page->mainFrame()->evaluateJavaScript("localStorage.test='This is a test for local storage';");
    m_view->setHtml(QString("<html><body id='b'>text</body></html>"), QUrl("http://www.myexample.com"));

    QVariant s1 = m_page->mainFrame()->evaluateJavaScript("localStorage.test");
    QCOMPARE(s1.toString(), QString("This is a test for local storage"));

    m_page->mainFrame()->evaluateJavaScript("sessionStorage.test='This is a test for session storage';");
    m_view->setHtml(QString("<html><body id='b'>text</body></html>"), QUrl("http://www.myexample.com"));
    QVariant s2 = m_page->mainFrame()->evaluateJavaScript("sessionStorage.test");
    QCOMPARE(s2.toString(), QString("This is a test for session storage"));

    m_view->setHtml(QString("<html><head></head><body><div></div></body></html>"), QUrl("http://www.myexample.com"));
    m_page->mainFrame()->evaluateJavaScript("var db3; db3=openDatabase('testdb', '1.0', 'test database API', 50000);db3.transaction(function(tx) { tx.executeSql('CREATE TABLE IF NOT EXISTS Test (text TEXT)', []); }, function(tx, result) { }, function(tx, error) { });");
    QTest::qWait(200);

    QWebSecurityOrigin origin = m_page->mainFrame()->securityOrigin();
    QList<QWebDatabase> dbs = origin.databases();
    if (dbs.count() > 0) {
        QString fileName = dbs[0].fileName();
        QVERIFY(QFile::exists(fileName));
        QWebDatabase::removeDatabase(dbs[0]);
        QVERIFY(!QFile::exists(fileName));
    }
    QTest::qWait(1000);
}

class PluginPage : public QWebPage
{
public:
    PluginPage(QObject *parent = 0)
        : QWebPage(parent) {}

    struct CallInfo
    {
        CallInfo(const QString &c, const QUrl &u,
                 const QStringList &pn, const QStringList &pv,
                 QObject *r)
            : classid(c), url(u), paramNames(pn),
              paramValues(pv), returnValue(r)
            {}
        QString classid;
        QUrl url;
        QStringList paramNames;
        QStringList paramValues;
        QObject *returnValue;
    };

    QList<CallInfo> calls;

protected:
    virtual QObject *createPlugin(const QString &classid, const QUrl &url,
                                  const QStringList &paramNames,
                                  const QStringList &paramValues)
    {
        QObject *result = 0;
        if (classid == "pushbutton")
            result = new QPushButton();
        else if (classid == "lineedit")
            result = new QLineEdit();
        if (result)
            result->setObjectName(classid);
        calls.append(CallInfo(classid, url, paramNames, paramValues, result));
        return result;
    }
};

void tst_QWebPage::createPlugin()
{
    QSignalSpy loadSpy(m_view, SIGNAL(loadFinished(bool)));

    PluginPage* newPage = new PluginPage(m_view);
    m_view->setPage(newPage);

    // plugins not enabled by default, so the plugin shouldn't be loaded
    m_view->setHtml(QString("<html><body><object type='application/x-qt-plugin' classid='pushbutton' id='mybutton'/></body></html>"));
    QTRY_COMPARE(loadSpy.count(), 1);
    QCOMPARE(newPage->calls.count(), 0);

    m_view->settings()->setAttribute(QWebSettings::PluginsEnabled, true);

    // type has to be application/x-qt-plugin
    m_view->setHtml(QString("<html><body><object type='application/x-foobarbaz' classid='pushbutton' id='mybutton'/></body></html>"));
    QTRY_COMPARE(loadSpy.count(), 2);
    QCOMPARE(newPage->calls.count(), 0);

    m_view->setHtml(QString("<html><body><object type='application/x-qt-plugin' classid='pushbutton' id='mybutton'/></body></html>"));
    QTRY_COMPARE(loadSpy.count(), 3);
    QCOMPARE(newPage->calls.count(), 1);
    {
        PluginPage::CallInfo ci = newPage->calls.takeFirst();
        QCOMPARE(ci.classid, QString::fromLatin1("pushbutton"));
        QCOMPARE(ci.url, QUrl());
        QCOMPARE(ci.paramNames.count(), 3);
        QCOMPARE(ci.paramValues.count(), 3);
        QCOMPARE(ci.paramNames.at(0), QString::fromLatin1("type"));
        QCOMPARE(ci.paramValues.at(0), QString::fromLatin1("application/x-qt-plugin"));
        QCOMPARE(ci.paramNames.at(1), QString::fromLatin1("classid"));
        QCOMPARE(ci.paramValues.at(1), QString::fromLatin1("pushbutton"));
        QCOMPARE(ci.paramNames.at(2), QString::fromLatin1("id"));
        QCOMPARE(ci.paramValues.at(2), QString::fromLatin1("mybutton"));
        QVERIFY(ci.returnValue != 0);
        QVERIFY(ci.returnValue->inherits("QPushButton"));
    }
    // test JS bindings
    QCOMPARE(newPage->mainFrame()->evaluateJavaScript("document.getElementById('mybutton').toString()").toString(),
             QString::fromLatin1("[object HTMLObjectElement]"));
    QCOMPARE(newPage->mainFrame()->evaluateJavaScript("mybutton.toString()").toString(),
             QString::fromLatin1("[object HTMLObjectElement]"));
    QCOMPARE(newPage->mainFrame()->evaluateJavaScript("typeof mybutton.objectName").toString(),
             QString::fromLatin1("string"));
    QCOMPARE(newPage->mainFrame()->evaluateJavaScript("mybutton.objectName").toString(),
             QString::fromLatin1("pushbutton"));
    QCOMPARE(newPage->mainFrame()->evaluateJavaScript("typeof mybutton.clicked").toString(),
             QString::fromLatin1("function"));
    QCOMPARE(newPage->mainFrame()->evaluateJavaScript("mybutton.clicked.toString()").toString(),
             QString::fromLatin1("function clicked() {\n    [native code]\n}"));

    m_view->setHtml(QString("<html><body><table>"
                            "<tr><object type='application/x-qt-plugin' classid='lineedit' id='myedit'/></tr>"
                            "<tr><object type='application/x-qt-plugin' classid='pushbutton' id='mybutton'/></tr>"
                            "</table></body></html>"), QUrl("http://foo.bar.baz"));
    QTRY_COMPARE(loadSpy.count(), 4);
    QCOMPARE(newPage->calls.count(), 2);
    {
        PluginPage::CallInfo ci = newPage->calls.takeFirst();
        QCOMPARE(ci.classid, QString::fromLatin1("lineedit"));
        QCOMPARE(ci.url, QUrl());
        QCOMPARE(ci.paramNames.count(), 3);
        QCOMPARE(ci.paramValues.count(), 3);
        QCOMPARE(ci.paramNames.at(0), QString::fromLatin1("type"));
        QCOMPARE(ci.paramValues.at(0), QString::fromLatin1("application/x-qt-plugin"));
        QCOMPARE(ci.paramNames.at(1), QString::fromLatin1("classid"));
        QCOMPARE(ci.paramValues.at(1), QString::fromLatin1("lineedit"));
        QCOMPARE(ci.paramNames.at(2), QString::fromLatin1("id"));
        QCOMPARE(ci.paramValues.at(2), QString::fromLatin1("myedit"));
        QVERIFY(ci.returnValue != 0);
        QVERIFY(ci.returnValue->inherits("QLineEdit"));
    }
    {
        PluginPage::CallInfo ci = newPage->calls.takeFirst();
        QCOMPARE(ci.classid, QString::fromLatin1("pushbutton"));
        QCOMPARE(ci.url, QUrl());
        QCOMPARE(ci.paramNames.count(), 3);
        QCOMPARE(ci.paramValues.count(), 3);
        QCOMPARE(ci.paramNames.at(0), QString::fromLatin1("type"));
        QCOMPARE(ci.paramValues.at(0), QString::fromLatin1("application/x-qt-plugin"));
        QCOMPARE(ci.paramNames.at(1), QString::fromLatin1("classid"));
        QCOMPARE(ci.paramValues.at(1), QString::fromLatin1("pushbutton"));
        QCOMPARE(ci.paramNames.at(2), QString::fromLatin1("id"));
        QCOMPARE(ci.paramValues.at(2), QString::fromLatin1("mybutton"));
        QVERIFY(ci.returnValue != 0);
        QVERIFY(ci.returnValue->inherits("QPushButton"));
    }

    m_view->settings()->setAttribute(QWebSettings::PluginsEnabled, false);

    m_view->setHtml(QString("<html><body><object type='application/x-qt-plugin' classid='pushbutton' id='mybutton'/></body></html>"));
    QTRY_COMPARE(loadSpy.count(), 5);
    QCOMPARE(newPage->calls.count(), 0);
}

class PluginTrackedPage : public QWebPage
{
public:

    int count;
    QPointer<QWidget> widget;

    PluginTrackedPage(QWidget *parent = 0) : QWebPage(parent), count(0) {
       settings()->setAttribute(QWebSettings::PluginsEnabled, true);
    }

    virtual QObject* createPlugin(const QString&, const QUrl&, const QStringList&, const QStringList&) {
       count++;
       QWidget *w = new QWidget;
       widget = w;
       return w;
    }
};

void tst_QWebPage::destroyPlugin()
{
    PluginTrackedPage* page = new PluginTrackedPage(m_view);
    m_view->setPage(page);

    // we create the plugin, so the widget should be constructed
    QString content("<html><body><object type=\"application/x-qt-plugin\" classid=\"QProgressBar\"></object></body></html>");
    m_view->setHtml(content);
    QVERIFY(page->widget != 0);
    QCOMPARE(page->count, 1);

    // navigate away, the plugin widget should be destructed
    m_view->setHtml("<html><body>Hi</body></html>");
    QTestEventLoop::instance().enterLoop(1);
    QVERIFY(page->widget == 0);
}

void tst_QWebPage::createViewlessPlugin()
{
    PluginTrackedPage* page = new PluginTrackedPage;
    QString content("<html><body><object type=\"application/x-qt-plugin\" classid=\"QProgressBar\"></object></body></html>");
    page->mainFrame()->setHtml(content);
    QCOMPARE(page->count, 1);
    QVERIFY(page->widget != 0);
    delete page;
}

QTEST_MAIN(tst_QWebPage)
#include "tst_qwebpage.moc"
