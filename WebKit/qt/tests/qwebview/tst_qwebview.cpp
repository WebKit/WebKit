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
    void guessUrlFromString_data();
    void guessUrlFromString();
    void getWebKitVersion();

    void reusePage_data();
    void reusePage();
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

void tst_QWebView::guessUrlFromString_data()
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<QUrl>("guessUrlFromString");

    // Null
    QTest::newRow("null") << QString() << QUrl();

    // File
    QDirIterator it(QDir::homePath());
    QString fileString;
    int c = 0;
    while (it.hasNext()) {
        it.next();
        QTest::newRow(QString("file-%1").arg(c++).toLatin1()) << it.filePath() << QUrl::fromLocalFile(it.filePath());
    }

    // basic latin1
    QTest::newRow("unicode-0") << QString::fromUtf8("å.com/") << QUrl::fromEncoded(QString::fromUtf8("http://å.com/").toUtf8(), QUrl::TolerantMode);
    // unicode
    QTest::newRow("unicode-1") << QString::fromUtf8("λ.com/") << QUrl::fromEncoded(QString::fromUtf8("http://λ.com/").toUtf8(), QUrl::TolerantMode);

    // no scheme
    QTest::newRow("add scheme-0") << "webkit.org" << QUrl("http://webkit.org");
    QTest::newRow("add scheme-1") << "www.webkit.org" << QUrl("http://www.webkit.org");
    QTest::newRow("add scheme-2") << "ftp.webkit.org" << QUrl("ftp://ftp.webkit.org");
    QTest::newRow("add scheme-3") << "webkit" << QUrl("webkit");

    // QUrl's tolerant parser should already handle this
    QTest::newRow("not-encoded-0") << "http://webkit.org/test page.html" << QUrl("http://webkit.org/test%20page.html");

    // Make sure the :80, i.e. port doesn't screw anything up
    QUrl portUrl("http://webkit.org");
    portUrl.setPort(80);
    QTest::newRow("port-0") << "webkit.org:80" << portUrl;
    QTest::newRow("port-1") << "http://webkit.org:80" << portUrl;

    // mailto doesn't have a ://, but is valid
    QUrl mailto("ben@meyerhome.net");
    mailto.setScheme("mailto");
    QTest::newRow("mailto") << "mailto:ben@meyerhome.net" << mailto;

    // misc
    QTest::newRow("localhost-0") << "localhost" << QUrl("http://localhost");
    QTest::newRow("localhost-1") << "localhost:80" << QUrl("http://localhost:80");
    QTest::newRow("spaces-0") << "  http://webkit.org/test page.html " << QUrl("http://webkit.org/test%20page.html");

    // FYI: The scheme in the resulting url user
    QUrl authUrl("user:pass@domain.com");
    QTest::newRow("misc-1") << "user:pass@domain.com" << authUrl;
}

// public static QUrl guessUrlFromString(QString const& string)
void tst_QWebView::guessUrlFromString()
{
    QFETCH(QString, string);
    QFETCH(QUrl, guessUrlFromString);

    QUrl url = QWebView::guessUrlFromString(string);
    QCOMPARE(url, guessUrlFromString);
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
    QDir::setCurrent(SRCDIR);

    QFETCH(QString, html);
    QWebView* view1 = new QWebView;
    QPointer<QWebPage> page = new QWebPage;
    view1->setPage(page);
    page->settings()->setAttribute(QWebSettings::PluginsEnabled, true);
    QWebFrame* mainFrame = page->mainFrame();
    mainFrame->setHtml(html, QUrl::fromLocalFile(QDir::currentPath()));
    if (html.contains("</embed>")) {
        // some reasonable time for the PluginStream to feed test.swf to flash and start painting
        QTest::qWait(2000);
    }

    view1->show();
    QTest::qWait(2000);
    delete view1;
    QVERIFY(page != 0); // deleting view must not have deleted the page, since it's not a child of view

    QWebView *view2 = new QWebView;
    view2->setPage(page);
    view2->show(); // in Windowless mode, you should still be able to see the plugin here
    QTest::qWait(2000);
    delete view2;

    delete page; // must not crash

    QDir::setCurrent(QApplication::applicationDirPath());
}

QTEST_MAIN(tst_QWebView)
#include "tst_qwebview.moc"

