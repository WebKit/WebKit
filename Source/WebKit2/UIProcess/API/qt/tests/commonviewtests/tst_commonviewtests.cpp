/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)

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

#include <QScopedPointer>
#include <QSignalSpy>
#include <QtTest/QtTest>
#include "webviewabstraction.h"
#include "../util.h"

class tst_CommonViewTests : public QObject {
    Q_OBJECT
public:
    tst_CommonViewTests();

private slots:
    void init();
    void cleanup();

    void baseUrl();
    void loadEmptyUrl();
    void loadEmptyPageViewVisible();
    void loadEmptyPageViewHidden();
    void loadNonexistentFileUrl();
    void backAndForward();
    void reload();
    void stop();
    void loadProgress();

    void show();
private:
    QScopedPointer<WebViewAbstraction> viewAbstraction;
};

tst_CommonViewTests::tst_CommonViewTests()
{
    addQtWebProcessToPath();
}

void tst_CommonViewTests::init()
{
    viewAbstraction.reset(new WebViewAbstraction);
}

void tst_CommonViewTests::cleanup()
{
    viewAbstraction.reset();
}

void tst_CommonViewTests::baseUrl()
{
    // Test the url is in a well defined state when instanciating the view, but before loading anything.
    QUrl url;
    QVERIFY(viewAbstraction->url(url));
    QVERIFY(url.isEmpty());
}

void tst_CommonViewTests::loadEmptyUrl()
{
    viewAbstraction->load(QUrl());
    viewAbstraction->load(QUrl(QLatin1String("")));
}

void tst_CommonViewTests::loadEmptyPageViewVisible()
{
    viewAbstraction->show();
    loadEmptyPageViewHidden();
}

void tst_CommonViewTests::loadEmptyPageViewHidden()
{
    QSignalSpy loadStartedSpy(viewAbstraction.data(), SIGNAL(loadStarted()));

    viewAbstraction->load(QUrl::fromLocalFile(QLatin1String(TESTS_SOURCE_DIR "/html/basic_page.html")));
    QVERIFY(waitForSignal(viewAbstraction.data(), SIGNAL(loadSucceeded())));

    QCOMPARE(loadStartedSpy.size(), 1);
}

void tst_CommonViewTests::loadNonexistentFileUrl()
{
    QSignalSpy loadFailedSpy(viewAbstraction.data(), SIGNAL(loadStarted()));

    viewAbstraction->load(QUrl::fromLocalFile(QLatin1String(TESTS_SOURCE_DIR "/html/file_that_does_not_exist.html")));
    QVERIFY(waitForSignal(viewAbstraction.data(), SIGNAL(loadFailed(QJSValue))));

    QCOMPARE(loadFailedSpy.size(), 1);
}

void tst_CommonViewTests::backAndForward()
{
    viewAbstraction->load(QUrl::fromLocalFile(QLatin1String(TESTS_SOURCE_DIR "/html/basic_page.html")));
    QVERIFY(waitForSignal(viewAbstraction.data(), SIGNAL(loadSucceeded())));

    QUrl url;
    QVERIFY(viewAbstraction->url(url));
    QCOMPARE(url.path(), QLatin1String(TESTS_SOURCE_DIR "/html/basic_page.html"));

    viewAbstraction->load(QUrl::fromLocalFile(QLatin1String(TESTS_SOURCE_DIR "/html/basic_page2.html")));
    QVERIFY(waitForSignal(viewAbstraction.data(), SIGNAL(loadSucceeded())));

    QVERIFY(viewAbstraction->url(url));
    QCOMPARE(url.path(), QLatin1String(TESTS_SOURCE_DIR "/html/basic_page2.html"));

    viewAbstraction->triggerNavigationAction(QtWebKit::Back);
    QVERIFY(waitForSignal(viewAbstraction.data(), SIGNAL(loadSucceeded())));

    QVERIFY(viewAbstraction->url(url));
    QCOMPARE(url.path(), QLatin1String(TESTS_SOURCE_DIR "/html/basic_page.html"));

    viewAbstraction->triggerNavigationAction(QtWebKit::Forward);
    QVERIFY(waitForSignal(viewAbstraction.data(), SIGNAL(loadSucceeded())));

    QVERIFY(viewAbstraction->url(url));
    QCOMPARE(url.path(), QLatin1String(TESTS_SOURCE_DIR "/html/basic_page2.html"));
}

void tst_CommonViewTests::reload()
{
    viewAbstraction->load(QUrl::fromLocalFile(QLatin1String(TESTS_SOURCE_DIR "/html/basic_page.html")));
    QVERIFY(waitForSignal(viewAbstraction.data(), SIGNAL(loadSucceeded())));

    QUrl url;
    QVERIFY(viewAbstraction->url(url));
    QCOMPARE(url.path(), QLatin1String(TESTS_SOURCE_DIR "/html/basic_page.html"));

    viewAbstraction->triggerNavigationAction(QtWebKit::Reload);
    QVERIFY(waitForSignal(viewAbstraction.data(), SIGNAL(loadSucceeded())));

    QVERIFY(viewAbstraction->url(url));
    QCOMPARE(url.path(), QLatin1String(TESTS_SOURCE_DIR "/html/basic_page.html"));
}

void tst_CommonViewTests::stop()
{
    viewAbstraction->load(QUrl::fromLocalFile(QLatin1String(TESTS_SOURCE_DIR "/html/basic_page.html")));
    QVERIFY(waitForSignal(viewAbstraction.data(), SIGNAL(loadSucceeded())));

    QUrl url;
    QVERIFY(viewAbstraction->url(url));
    QCOMPARE(url.path(), QLatin1String(TESTS_SOURCE_DIR "/html/basic_page.html"));

    // FIXME: This test should be fleshed out. Right now it's just here to make sure we don't crash.
    viewAbstraction->triggerNavigationAction(QtWebKit::Stop);
}

void tst_CommonViewTests::loadProgress()
{
    QCOMPARE(viewAbstraction->loadProgress(), 0);

    viewAbstraction->load(QUrl::fromLocalFile(QLatin1String(TESTS_SOURCE_DIR "/html/basic_page.html")));
    QSignalSpy loadProgressChangedSpy(viewAbstraction.data(), SIGNAL(loadProgressChanged(int)));
    QVERIFY(waitForSignal(viewAbstraction.data(), SIGNAL(loadSucceeded())));

    QVERIFY(loadProgressChangedSpy.count() >= 1);

    QCOMPARE(viewAbstraction->loadProgress(), 100);
}

void tst_CommonViewTests::show()
{
    // This should not crash.
    viewAbstraction->show();
    QTest::qWait(200);
    viewAbstraction->hide();
}

QTEST_MAIN(tst_CommonViewTests)

#include "tst_commonviewtests.moc"

