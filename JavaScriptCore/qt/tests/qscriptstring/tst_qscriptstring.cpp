/*
    Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

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

#ifndef tst_qscriptstring_h
#define tst_qscriptstring_h

#include "qscriptengine.h"
#include "qscriptstring.h"
#include <QtCore/qhash.h>
#include <QtTest/QtTest>

class tst_QScriptString : public QObject {
    Q_OBJECT

public:
    tst_QScriptString();
    virtual ~tst_QScriptString();

private slots:
    void test();
    void hash();
    void toArrayIndex_data();
    void toArrayIndex();
};

tst_QScriptString::tst_QScriptString()
{
}

tst_QScriptString::~tst_QScriptString()
{
}

void tst_QScriptString::test()
{
    QScriptEngine eng;
    {
        QScriptString str;
        QVERIFY(!str.isValid());
        QVERIFY(str == str);
        QVERIFY(!(str != str));
        QVERIFY(str.toString().isNull());

        QScriptString str1(str);
        QVERIFY(!str1.isValid());

        QScriptString str2 = str;
        QVERIFY(!str2.isValid());

        QCOMPARE(str.toArrayIndex(), quint32(0xffffffff));
    }
    for (int x = 0; x < 2; ++x) {
        QString ciao = QString::fromLatin1("ciao");
        QScriptString str = eng.toStringHandle(ciao);
        QVERIFY(str.isValid());
        QVERIFY(str == str);
        QVERIFY(!(str != str));
        QCOMPARE(str.toString(), ciao);

        QScriptString str1(str);
        QCOMPARE(str, str1);

        QScriptString str2 = str;
        QCOMPARE(str, str2);

        QScriptString str3 = eng.toStringHandle(ciao);
        QVERIFY(str3.isValid());
        QCOMPARE(str, str3);

        eng.collectGarbage();

        QVERIFY(str.isValid());
        QCOMPARE(str.toString(), ciao);
        QVERIFY(str1.isValid());
        QCOMPARE(str1.toString(), ciao);
        QVERIFY(str2.isValid());
        QCOMPARE(str2.toString(), ciao);
        QVERIFY(str3.isValid());
        QCOMPARE(str3.toString(), ciao);
    }
    {
        QScriptEngine* eng2 = new QScriptEngine;
        QString one = QString::fromLatin1("one");
        QString two = QString::fromLatin1("two");
        QScriptString oneInterned = eng2->toStringHandle(one);
        QCOMPARE(oneInterned.toString(), one);
        QScriptString twoInterned = eng2->toStringHandle(two);
        QCOMPARE(twoInterned.toString(), two);
        QVERIFY(oneInterned != twoInterned);
        QVERIFY(!(oneInterned == twoInterned));

        delete eng2;
    }
}

void tst_QScriptString::hash()
{
    QScriptEngine engine;
    QHash<QScriptString, int> stringToInt;
    QScriptString foo = engine.toStringHandle("foo");

    QScriptString bar = engine.toStringHandle("bar");
    QVERIFY(!stringToInt.contains(foo));
    for (int i = 0; i < 1000000; ++i)
        stringToInt.insert(foo, 123);
    QCOMPARE(stringToInt.value(foo), 123);
    QVERIFY(!stringToInt.contains(bar));
    stringToInt.insert(bar, 456);
    QCOMPARE(stringToInt.value(bar), 456);
    QCOMPARE(stringToInt.value(foo), 123);
}

void tst_QScriptString::toArrayIndex_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("expectSuccess");
    QTest::addColumn<quint32>("expectedIndex");
    QTest::newRow("foo") << QString::fromLatin1("foo") << false << quint32(0xffffffff);
    QTest::newRow("empty") << QString::fromLatin1("") << false << quint32(0xffffffff);
    QTest::newRow("0") << QString::fromLatin1("0") << true << quint32(0);
    QTest::newRow("00") << QString::fromLatin1("00") << false << quint32(0xffffffff);
    QTest::newRow("1") << QString::fromLatin1("1") << true << quint32(1);
    QTest::newRow("123") << QString::fromLatin1("123") << true << quint32(123);
    QTest::newRow("-1") << QString::fromLatin1("-1") << false << quint32(0xffffffff);
    QTest::newRow("0a") << QString::fromLatin1("0a") << false << quint32(0xffffffff);
    QTest::newRow("0x1") << QString::fromLatin1("0x1") << false << quint32(0xffffffff);
    QTest::newRow("01") << QString::fromLatin1("01") << false << quint32(0xffffffff);
    QTest::newRow("101a") << QString::fromLatin1("101a") << false << quint32(0xffffffff);
    QTest::newRow("4294967294") << QString::fromLatin1("4294967294") << true << quint32(0xfffffffe);
    QTest::newRow("4294967295") << QString::fromLatin1("4294967295") << false << quint32(0xffffffff);
    QTest::newRow("11111111111") << QString::fromLatin1("11111111111") << false << quint32(0xffffffff);
    QTest::newRow("0.0") << QString::fromLatin1("0.0") << false << quint32(0xffffffff);
    QTest::newRow("1.0") << QString::fromLatin1("1.0") << false << quint32(0xffffffff);
    QTest::newRow("1.5") << QString::fromLatin1("1.5") << false << quint32(0xffffffff);
    QTest::newRow("1.") << QString::fromLatin1("1.") << false << quint32(0xffffffff);
    QTest::newRow(".1") << QString::fromLatin1(".1") << false << quint32(0xffffffff);
    QTest::newRow("1e0") << QString::fromLatin1("1e0") << false << quint32(0xffffffff);
}

void tst_QScriptString::toArrayIndex()
{
    QFETCH(QString, input);
    QFETCH(bool, expectSuccess);
    QFETCH(quint32, expectedIndex);
    QScriptEngine engine;
    for (int x = 0; x < 2; ++x) {
        bool isArrayIndex;
        bool* ptr = (!x) ? &isArrayIndex : (bool*)0;
        quint32 result = engine.toStringHandle(input).toArrayIndex(ptr);
        if (!x)
            QCOMPARE(isArrayIndex, expectSuccess);
        QCOMPARE(result, expectedIndex);
    }
}

QTEST_MAIN(tst_QScriptString)
#include "tst_qscriptstring.moc"

#endif // tst_qscriptstring_h
