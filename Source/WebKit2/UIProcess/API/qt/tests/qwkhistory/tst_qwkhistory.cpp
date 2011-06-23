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

#include "util.h"
#include <QScopedPointer>
#include <QSignalSpy>
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsView>
#include <QtTest/QtTest>
#include <qgraphicswkview.h>
#include <qwkcontext.h>
#include <qwkhistory.h>
#include <qwkpage.h>

#define TEST_HISTORY_ACTION_RANGE 10

class TestHistoryItem {
public:
    TestHistoryItem(const QString& title, const QString& filename);

    QString title() const
    {
        return m_title;
    }

    QUrl url() const
    {
        return m_url;
    }

private:
    QString m_title;
    QUrl m_url;
};

TestHistoryItem::TestHistoryItem(const QString& title, const QString& filename)
    : m_title(title)
{
    if (!filename.isEmpty())
        m_url = QUrl::fromLocalFile(QLatin1String(TESTS_SOURCE_DIR) + QLatin1String("/html/") + filename);
}

class tst_QWKHistory : public QObject {
    Q_OBJECT
    
private slots:
    void initTestCase();
    void historyForwardBackTest_data();
    void historyForwardBackTest();
    
private:
    QScopedPointer<QGraphicsView> m_graphicsView;
    QScopedPointer<QWKContext> m_context;
    QScopedPointer<QGraphicsWKView> m_webView;

    QWKPage* m_page;
    QWKHistory* m_history;

    // Enumerate TestHistoryActions starting at 10 so that other integers
    // can be used at will to test random access
    enum TestHistoryActions {
        TestNone = TEST_HISTORY_ACTION_RANGE,
        TestLoad,
        TestBack,
        TestFwd
    };

    QScopedPointer<TestHistoryItem> m_testItemBlank;
    QScopedPointer<TestHistoryItem> m_testItemA;
    QScopedPointer<TestHistoryItem> m_testItemB;
    QScopedPointer<TestHistoryItem> m_testItemC;
    QScopedPointer<TestHistoryItem> m_testItemD;
};

void tst_QWKHistory::initTestCase()
{   
    QGraphicsScene* const scene = new QGraphicsScene(this);

    m_graphicsView.reset(new QGraphicsView(scene));

    m_context.reset(new QWKContext(this));
    m_webView.reset(new QGraphicsWKView(m_context.data()));
    scene->addItem(m_webView.data());

    m_testItemBlank.reset(new TestHistoryItem(QString(), QString()));
    m_testItemA.reset(new TestHistoryItem(QLatin1String("aTitle"), QLatin1String("a.htm")));
    m_testItemB.reset(new TestHistoryItem(QLatin1String("bTitle"), QLatin1String("b.htm")));
    m_testItemC.reset(new TestHistoryItem(QLatin1String("cTitle"), QLatin1String("c.htm")));
    m_testItemD.reset(new TestHistoryItem(QLatin1String("dTitle"), QLatin1String("d.htm")));

    m_page = m_webView->page();
    m_history = m_page->history();
}

Q_DECLARE_METATYPE(TestHistoryItem*)
Q_DECLARE_METATYPE(QList<TestHistoryItem*>)

void tst_QWKHistory::historyForwardBackTest_data()
{
    QTest::addColumn<int>("command");
    QTest::addColumn<QList<TestHistoryItem*> >("expectedBackList");
    QTest::addColumn<TestHistoryItem*>("testItem");
    QTest::addColumn<QList<TestHistoryItem*> >("expectedForwardList");
    QTest::addColumn<bool>("wait");

    QList<TestHistoryItem*> expectedBackList;
    QList<TestHistoryItem*> expectedForwardList;

    // Test the initial state with empty forward and back lists.
    QTest::newRow("[] () []") <<
        int(TestNone) << expectedBackList << m_testItemBlank.data() << expectedForwardList << false;

    // Test loading a few items and check that they appear in the back list.
    QTest::newRow("[] (a) []") <<
        int(TestLoad) << expectedBackList << m_testItemA.data() << expectedForwardList << true;

    expectedBackList << m_testItemA.data();
    QTest::newRow("[a] (b) []") <<
        int(TestLoad) << expectedBackList << m_testItemB.data() << expectedForwardList << true;

    expectedBackList << m_testItemB.data();
    QTest::newRow("[a b] (c) []") <<
        int(TestLoad) << expectedBackList << m_testItemC.data() << expectedForwardList << true;

    expectedBackList << m_testItemC.data();
    QTest::newRow("[a b c] (d) []") <<
        int(TestLoad) << expectedBackList << m_testItemD.data() << expectedForwardList << true;
    
    // Go back and verify that items now appear in the forward list.
    expectedForwardList << m_testItemD.data();
    QTest::newRow("[a b] (c) [d]") <<
        int(TestBack) << expectedBackList << expectedBackList.takeLast() << expectedForwardList << true;

    expectedForwardList.prepend(m_testItemC.data());
    QTest::newRow("[a] (b) [c d]") <<
        int(TestBack) << expectedBackList << expectedBackList.takeLast() << expectedForwardList << true;

    expectedForwardList.prepend(m_testItemB.data());
    QTest::newRow("(a) [b c d]") <<
        int(TestBack) << expectedBackList << expectedBackList.takeLast() << expectedForwardList << true;

    // Test random access and related edge cases
    QTest::newRow("random (a) [b c d]") <<
        0 << expectedBackList << m_testItemA.data() << expectedForwardList << true;
    expectedBackList << m_testItemA.data() << expectedForwardList.takeFirst();
    QTest::newRow("random [a b] (c) [d]") <<
        2 << expectedBackList << expectedForwardList.takeFirst() << expectedForwardList << true;

    QTest::newRow("random [a b] (c) [d]") <<
        2 << expectedBackList << m_testItemC.data() << expectedForwardList << false;

    expectedBackList << m_testItemC.data();
    QTest::newRow("random [a b c] (d)") <<
        1 << expectedBackList << expectedForwardList.takeFirst() << expectedForwardList << true;

    QTest::newRow("random [a b c] (d)") <<
        1 << expectedBackList << m_testItemD.data() << expectedForwardList << false;

    expectedForwardList << m_testItemB.data() << m_testItemC.data() << m_testItemD.data();
    expectedBackList.clear();
    QTest::newRow("random (a) [b c d]") <<
        -3 << expectedBackList << m_testItemA.data() << expectedForwardList << true;

    QTest::newRow("random (a) [b c d]") <<
        -1 << expectedBackList << m_testItemA.data() << expectedForwardList << false;

    // "Branch" the forward list away from where it was by loading a new item. Forward list
    // is now cleared, back list should be as expected.
    expectedBackList.clear();
    expectedForwardList.clear();
    expectedBackList << m_testItemA.data();
    QTest::newRow("[a] (d) []") <<
        int(TestLoad) << expectedBackList << m_testItemD.data() << expectedForwardList << true;

    // Attempt to go forward with nothing in the forward list. Ask the test not to wait for the load
    // (you won't get it).
    QTest::newRow("[a] (d) []") <<
        int(TestFwd) << expectedBackList << m_testItemD.data() << expectedForwardList << false;

    expectedForwardList << m_testItemD.data();
    QTest::newRow("[] (a) [d]") <<
        int(TestBack) << expectedBackList << expectedBackList.takeLast() << expectedForwardList << true;

    // Attempt to go backwards with nothing in the back list. Ask the test not to wait for the load
    // (you won't get it).
    QTest::newRow("[] (a) [d]") <<
        int(TestBack) << expectedBackList << m_testItemA.data() << expectedForwardList << false;
}

void tst_QWKHistory::historyForwardBackTest()
{
    QFETCH(int, command);
    QFETCH(QList<TestHistoryItem*>, expectedBackList);
    QFETCH(TestHistoryItem*, testItem);
    QFETCH(QList<TestHistoryItem*>, expectedForwardList);
    QFETCH(bool, wait);

    switch (command) {
    case TestNone:
        break;
    case TestLoad:
        m_page->load(testItem->url());
        break;
    case TestBack:
        m_page->triggerAction(QWKPage::Back);
        break;
    case TestFwd:
        m_page->triggerAction(QWKPage::Forward);
        break;
    default:
        // allow test data to pass in indices between defined enum range
        if ((command > -TEST_HISTORY_ACTION_RANGE) && (command < TEST_HISTORY_ACTION_RANGE))
            m_history->goToItemAt(command);
        else
            QFAIL("undefined test case action");
        break;
    }
    
    QSignalSpy spy(m_page, SIGNAL(loadFinished(bool)));
    if (wait) {
        QVERIFY(waitForSignal(m_page, SIGNAL(loadFinished(bool))));
        QCOMPARE(spy.count(), 1);
        QList<QVariant> arguments = spy.takeFirst();
        QVERIFY2(arguments.at(0).toBool(), "Could Not Load: QWKPage loadFinished signal returned false");
    } else
        QCOMPARE(spy.count(), 0);

    // Check that the forward, back, and overall history item counts match the expected data.
    QCOMPARE(m_history->backListCount(), expectedBackList.count());
    QCOMPARE(m_history->forwardListCount(), expectedForwardList.count());
    QCOMPARE(m_history->count(), expectedBackList.count() + expectedForwardList.count());

    // Check that the current item matches what is expected to be the current item.
    QWKHistoryItem currentItem = m_history->currentItem();
    QCOMPARE(testItem->title(), currentItem.title());
    QCOMPARE(testItem->url(), currentItem.url());

    // Check that title, url from the back list match expected.
    // Check that title, url using itemAt API match expected.
    QList<QWKHistoryItem> blist = m_history->backItems(10);
    for (int i = 0; i < expectedBackList.count(); i++) {
        // Compare backItems and itemAt APIs
        // (note itemAt is continuous with itemAt(0) == currentItem)
        QCOMPARE(expectedBackList.at(i)->title(),
                 blist.at(i).title());
        QCOMPARE(expectedBackList.at(i)->url(),
                 blist.at(i).url());
        QCOMPARE(expectedBackList.at(i)->title(),
                 m_history->itemAt(i-expectedBackList.count()).title());
        QCOMPARE(expectedBackList.at(i)->url(),
                 m_history->itemAt(i-expectedBackList.count()).url());
    }

    // Check that title, url from the forward list match expected.
    // Check that title, url using itemAt API match expected.
    QList<QWKHistoryItem> flist = m_history->forwardItems(10);
    for (int i = 0; i < expectedForwardList.count(); i++) {
        // Compare forwardItems and itemAt APIs
        // (note itemAt is continuous with itemAt(0) == currentItem)
        QCOMPARE(expectedForwardList.at(i)->title(),
                 flist.at(i).title());
        QCOMPARE(expectedForwardList.at(i)->url(),
                 flist.at(i).url());
        QCOMPARE(expectedForwardList.at(i)->title(),
                 m_history->itemAt(i+1).title());
        QCOMPARE(expectedForwardList.at(i)->url(),
                 m_history->itemAt(i+1).url());
    }

    // Verify that the backItem API gives expected title and URL or that they give empty/blank items when
    // the back list is empty
    QWKHistoryItem backItem = m_history->backItem();
    if (!expectedBackList.count()) {
        QCOMPARE(backItem.title(), QString());
        QCOMPARE(backItem.url(), QUrl());
        QCOMPARE(m_testItemBlank->title(),
                 m_history->itemAt(-1).title());
        QCOMPARE(m_testItemBlank->url(),
                 m_history->itemAt(-1).url());
    } else {
        QCOMPARE(expectedBackList.at(expectedBackList.count() - 1)->title(),
                 backItem.title());
        QCOMPARE(expectedBackList.at(expectedBackList.count() - 1)->url(),
                 backItem.url());
    }

    // Verify that the forwardItem API gives expected title and URL or that they give empty/blank items when
    // the forward list is empty
    QWKHistoryItem forwardItem = m_history->forwardItem();
    if (!expectedForwardList.count()) {
        QCOMPARE(forwardItem.title(), QString());
        QCOMPARE(forwardItem.url(), QUrl());
        QCOMPARE(m_testItemBlank->title(),
                 m_history->itemAt(1).title());
        QCOMPARE(m_testItemBlank->url(),
                 m_history->itemAt(1).url());
    } else {
        QCOMPARE(expectedForwardList.at(0)->title(),
                 forwardItem.title());
        QCOMPARE(expectedForwardList.at(0)->url(),
                 forwardItem.url());
    }
}

QTEST_MAIN(tst_QWKHistory)

#include "tst_qwkhistory.moc"
