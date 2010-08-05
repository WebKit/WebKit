/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2009 Torch Mobile Inc.
    Copyright (C) 2009 Girish Ramakrishnan <girish@forwardbias.in>

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

#include <qtest.h>
#include "../util.h"

#include <qpainter.h>
#include <qwebview.h>
#include <qwebpage.h>
#include <qnetworkrequest.h>
#include <qdiriterator.h>
#include <qwebkitversion.h>
#include <qwebframe.h>

class tst_QWebView : public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void renderHints();
    void getWebKitVersion();

    void reusePage_data();
    void reusePage();
    void microFocusCoordinates();
    void focusInputTypes();

    void crashTests();
};

class WebView : public QWebView
{
    Q_OBJECT

public:
    void fireMouseClick(QPoint point) {
        QMouseEvent presEv(QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent relEv(QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QWebView::mousePressEvent(&presEv);
        QWebView::mousePressEvent(&relEv);
    }

};

// This will be called before the first test function is executed.
// It is only called once.
void tst_QWebView::initTestCase()
{
}

// This will be called after the last test function is executed.
// It is only called once.
void tst_QWebView::cleanupTestCase()
{
}

// This will be called before each test function is executed.
void tst_QWebView::init()
{
}

// This will be called after every test function.
void tst_QWebView::cleanup()
{
}

void tst_QWebView::renderHints()
{
    QWebView webView;

    // default is only text antialiasing
    QVERIFY(!(webView.renderHints() & QPainter::Antialiasing));
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(!(webView.renderHints() & QPainter::SmoothPixmapTransform));
    QVERIFY(!(webView.renderHints() & QPainter::HighQualityAntialiasing));

    webView.setRenderHint(QPainter::Antialiasing, true);
    QVERIFY(webView.renderHints() & QPainter::Antialiasing);
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(!(webView.renderHints() & QPainter::SmoothPixmapTransform));
    QVERIFY(!(webView.renderHints() & QPainter::HighQualityAntialiasing));

    webView.setRenderHint(QPainter::Antialiasing, false);
    QVERIFY(!(webView.renderHints() & QPainter::Antialiasing));
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(!(webView.renderHints() & QPainter::SmoothPixmapTransform));
    QVERIFY(!(webView.renderHints() & QPainter::HighQualityAntialiasing));

    webView.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QVERIFY(!(webView.renderHints() & QPainter::Antialiasing));
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(webView.renderHints() & QPainter::SmoothPixmapTransform);
    QVERIFY(!(webView.renderHints() & QPainter::HighQualityAntialiasing));

    webView.setRenderHint(QPainter::SmoothPixmapTransform, false);
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(!(webView.renderHints() & QPainter::SmoothPixmapTransform));
    QVERIFY(!(webView.renderHints() & QPainter::HighQualityAntialiasing));
}

void tst_QWebView::getWebKitVersion()
{
    QVERIFY(qWebKitVersion().toDouble() > 0);
}

void tst_QWebView::reusePage_data()
{
    QTest::addColumn<QString>("html");
    QTest::newRow("WithoutPlugin") << "<html><body id='b'>text</body></html>";
    QTest::newRow("WindowedPlugin") << QString("<html><body id='b'>text<embed src='resources/test.swf'></embed></body></html>");
    QTest::newRow("WindowlessPlugin") << QString("<html><body id='b'>text<embed src='resources/test.swf' wmode=\"transparent\"></embed></body></html>");
}

void tst_QWebView::reusePage()
{
    if (!QDir(TESTS_SOURCE_DIR).exists())
        QSKIP(QString("This test requires access to resources found in '%1'").arg(TESTS_SOURCE_DIR).toLatin1().constData(), SkipAll);

    QDir::setCurrent(TESTS_SOURCE_DIR);

    QFETCH(QString, html);
    QWebView* view1 = new QWebView;
    QPointer<QWebPage> page = new QWebPage;
    view1->setPage(page);
    page->settings()->setAttribute(QWebSettings::PluginsEnabled, true);
    QWebFrame* mainFrame = page->mainFrame();
    mainFrame->setHtml(html, QUrl::fromLocalFile(TESTS_SOURCE_DIR));
    if (html.contains("</embed>")) {
        // some reasonable time for the PluginStream to feed test.swf to flash and start painting
        waitForSignal(view1, SIGNAL(loadFinished(bool)), 2000);
    }

    view1->show();
#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    QTest::qWaitForWindowShown(view1);
#else
    QTest::qWait(2000); 
#endif
    delete view1;
    QVERIFY(page != 0); // deleting view must not have deleted the page, since it's not a child of view

    QWebView *view2 = new QWebView;
    view2->setPage(page);
    view2->show(); // in Windowless mode, you should still be able to see the plugin here
#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    QTest::qWaitForWindowShown(view2);
#else
    QTest::qWait(2000); 
#endif
    delete view2;

    delete page; // must not crash

    QDir::setCurrent(QApplication::applicationDirPath());
}

// Class used in crashTests
class WebViewCrashTest : public QObject {
    Q_OBJECT
    QWebView* m_view;
public:
    bool m_executed;


    WebViewCrashTest(QWebView* view)
      : m_view(view)
      , m_executed(false)
    {
        view->connect(view, SIGNAL(loadProgress(int)), this, SLOT(loading(int)));
    }

private slots:
    void loading(int progress)
    {
        if (progress >= 20 && progress < 90) {
            QVERIFY(!m_executed);
            m_view->stop();
            m_executed = true;
        }
    }
};


// Should not crash.
void tst_QWebView::crashTests()
{
    // Test if loading can be stopped in loadProgress handler without crash.
    // Test page should have frames.
    QWebView view;
    WebViewCrashTest tester(&view);
    QUrl url("qrc:///resources/index.html");
    view.load(url);
    QTRY_VERIFY(tester.m_executed); // If fail it means that the test wasn't executed.
}

void tst_QWebView::microFocusCoordinates()
{
    QWebPage* page = new QWebPage;
    QWebView* webView = new QWebView;
    webView->setPage( page );

    page->mainFrame()->setHtml("<html><body>" \
        "<input type='text' id='input1' style='font--family: serif' value='' maxlength='20'/><br>" \
        "<canvas id='canvas1' width='500' height='500'/>" \
        "<input type='password'/><br>" \
        "<canvas id='canvas2' width='500' height='500'/>" \
        "</body></html>");

    page->mainFrame()->setFocus();

    QVariant initialMicroFocus = page->inputMethodQuery(Qt::ImMicroFocus);
    QVERIFY(initialMicroFocus.isValid());

    page->mainFrame()->scroll(0,50);

    QVariant currentMicroFocus = page->inputMethodQuery(Qt::ImMicroFocus);
    QVERIFY(currentMicroFocus.isValid());

    QCOMPARE(initialMicroFocus.toRect().translated(QPoint(0,-50)), currentMicroFocus.toRect());
}

void tst_QWebView::focusInputTypes()
{
    QWebPage* page = new QWebPage;
    WebView* webView = new WebView;
    webView->setPage( page );

    QCoreApplication::processEvents();
    QUrl url("qrc:///resources/input_types.html");
    page->mainFrame()->load(url);
    page->mainFrame()->setFocus();

    QVERIFY(waitForSignal(page, SIGNAL(loadFinished(bool))));

    // 'text' type
    webView->fireMouseClick(QPoint(20, 10));
#if defined(Q_WS_MAEMO_5) || defined(Q_WS_MAEMO_6) || defined(Q_OS_SYMBIAN)
    QVERIFY(webView->inputMethodHints() & Qt::ImhNoAutoUppercase);
    QVERIFY(webView->inputMethodHints() & Qt::ImhNoPredictiveText);
#else
    QVERIFY(webView->inputMethodHints() == Qt::ImhNone);
#endif
    QVERIFY(webView->testAttribute(Qt::WA_InputMethodEnabled));

    // 'password' field
    webView->fireMouseClick(QPoint(20, 60));
    QVERIFY(webView->inputMethodHints() == Qt::ImhHiddenText);
    QVERIFY(webView->testAttribute(Qt::WA_InputMethodEnabled));

    // 'tel' field
    webView->fireMouseClick(QPoint(20, 110));
    QVERIFY(webView->inputMethodHints() == Qt::ImhDialableCharactersOnly);
    QVERIFY(webView->testAttribute(Qt::WA_InputMethodEnabled));

    // 'number' field
    webView->fireMouseClick(QPoint(20, 160));
    QVERIFY(webView->inputMethodHints() == Qt::ImhDigitsOnly);
    QVERIFY(webView->testAttribute(Qt::WA_InputMethodEnabled));

    // 'email' field
    webView->fireMouseClick(QPoint(20, 210));
    QVERIFY(webView->inputMethodHints() == Qt::ImhEmailCharactersOnly);
    QVERIFY(webView->testAttribute(Qt::WA_InputMethodEnabled));

    // 'url' field
    webView->fireMouseClick(QPoint(20, 260));
    QVERIFY(webView->inputMethodHints() == Qt::ImhUrlCharactersOnly);
    QVERIFY(webView->testAttribute(Qt::WA_InputMethodEnabled));

    // 'password' field
    webView->fireMouseClick(QPoint(20, 60));
    QVERIFY(webView->inputMethodHints() == Qt::ImhHiddenText);
    QVERIFY(webView->testAttribute(Qt::WA_InputMethodEnabled));

    // 'text' type
    webView->fireMouseClick(QPoint(20, 10));
#if defined(Q_WS_MAEMO_5) || defined(Q_WS_MAEMO_6) || defined(Q_OS_SYMBIAN)
    QVERIFY(webView->inputMethodHints() & Qt::ImhNoAutoUppercase);
    QVERIFY(webView->inputMethodHints() & Qt::ImhNoPredictiveText);
#else
    QVERIFY(webView->inputMethodHints() == Qt::ImhNone);
#endif
    QVERIFY(webView->testAttribute(Qt::WA_InputMethodEnabled));

    // 'password' field
    webView->fireMouseClick(QPoint(20, 60));
    QVERIFY(webView->inputMethodHints() == Qt::ImhHiddenText);
    QVERIFY(webView->testAttribute(Qt::WA_InputMethodEnabled));

    qWarning("clicking on text area");
    // 'text area' field
    webView->fireMouseClick(QPoint(20, 320));
    QVERIFY(webView->inputMethodHints() == Qt::ImhNone);
    QVERIFY(webView->testAttribute(Qt::WA_InputMethodEnabled));

    delete webView;

}

QTEST_MAIN(tst_QWebView)
#include "tst_qwebview.moc"

