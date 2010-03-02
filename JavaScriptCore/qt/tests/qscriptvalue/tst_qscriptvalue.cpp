/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

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

#include "tst_qscriptvalue.h"
#include <QtCore/qnumeric.h>

tst_QScriptValue::tst_QScriptValue()
    : engine(0)
{
}

tst_QScriptValue::~tst_QScriptValue()
{
    delete engine;
}

void tst_QScriptValue::dataHelper(InitDataFunction init, DefineDataFunction define)
{
    QTest::addColumn<QString>("__expression__");
    (this->*init)();
    QHash<QString, QScriptValue>::const_iterator it;
    for (it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
        m_currentExpression = it.key();
        (this->*define)(it.key().toLatin1());
    }
    m_currentExpression = QString();
}

QTestData& tst_QScriptValue::newRow(const char* tag)
{
    return QTest::newRow(tag) << m_currentExpression;
}

void tst_QScriptValue::testHelper(TestFunction fun)
{
    QFETCH(QString, __expression__);
    QScriptValue value = m_values.value(__expression__);
    (this->*fun)(__expression__.toLatin1(), value);
}


void tst_QScriptValue::ctor()
{
    QScriptEngine eng;
    {
        QScriptValue v;
        QCOMPARE(v.isValid(), false);
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    {
        QScriptValue v(&eng, QScriptValue::UndefinedValue);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isUndefined(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.engine(), &eng);
    }
    {
        QScriptValue v(&eng, QScriptValue::NullValue);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNull(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.engine(), &eng);
    }
    {
        QScriptValue v(&eng, false);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isBoolean(), true);
        QCOMPARE(v.isBool(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toBoolean(), false);
        QCOMPARE(v.engine(), &eng);
    }
    {
        QScriptValue v(&eng, int(1));
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), &eng);
    }
    {
        QScriptValue v(int(0x43211234));
        QVERIFY(v.isNumber());
        QCOMPARE(v.toInt32(), 0x43211234);
    }
    {
        QScriptValue v(&eng, uint(1));
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), &eng);
    }
    {
        QScriptValue v(uint(0x43211234));
        QVERIFY(v.isNumber());
        QCOMPARE(v.toUInt32(), uint(0x43211234));
    }
    {
        QScriptValue v(&eng, 1.0);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), &eng);
    }
    {
        QScriptValue v(12345678910.5);
        QVERIFY(v.isNumber());
        QCOMPARE(v.toNumber(), 12345678910.5);
    }
    {
        QScriptValue v(&eng, "ciao");
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isString(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toString(), QLatin1String("ciao"));
        QCOMPARE(v.engine(), &eng);
    }
    {
        QScriptValue v(&eng, QString("ciao"));
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isString(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toString(), QLatin1String("ciao"));
        QCOMPARE(v.engine(), &eng);
    }
    // copy constructor, operator=
    {
        QScriptValue v(&eng, 1.0);
        QScriptValue v2(v);
        QCOMPARE(v2.strictlyEquals(v), true);
        QCOMPARE(v2.engine(), &eng);

        QScriptValue v3(v);
        QCOMPARE(v3.strictlyEquals(v), true);
        QCOMPARE(v3.strictlyEquals(v2), true);
        QCOMPARE(v3.engine(), &eng);

        QScriptValue v4(&eng, 2.0);
        QCOMPARE(v4.strictlyEquals(v), false);
        v3 = v4;
        QCOMPARE(v3.strictlyEquals(v), false);
        QCOMPARE(v3.strictlyEquals(v4), true);

        v2 = QScriptValue();
        QCOMPARE(v2.strictlyEquals(v), false);
        QCOMPARE(v.toNumber(), 1.0);

        QScriptValue v5(v);
        QCOMPARE(v5.strictlyEquals(v), true);
        v = QScriptValue();
        QCOMPARE(v5.strictlyEquals(v), false);
        QCOMPARE(v5.toNumber(), 1.0);
    }

    // constructors that take no engine argument
    {
        QScriptValue v(QScriptValue::UndefinedValue);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isUndefined(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    {
        QScriptValue v(QScriptValue::NullValue);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNull(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    {
        QScriptValue v(false);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isBoolean(), true);
        QCOMPARE(v.isBool(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toBoolean(), false);
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    {
        QScriptValue v(int(1));
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    {
        QScriptValue v(uint(1));
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    {
        QScriptValue v(1.0);
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isNumber(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toNumber(), 1.0);
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    {
        QScriptValue v("ciao");
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isString(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toString(), QLatin1String("ciao"));
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    {
        QScriptValue v(QString("ciao"));
        QCOMPARE(v.isValid(), true);
        QCOMPARE(v.isString(), true);
        QCOMPARE(v.isObject(), false);
        QCOMPARE(v.toString(), QLatin1String("ciao"));
        QCOMPARE(v.engine(), (QScriptEngine*)0);
    }
    // copy constructor, operator=
    {
        QScriptValue v(1.0);
        QScriptValue v2(v);
        QCOMPARE(v2.strictlyEquals(v), true);
        QCOMPARE(v2.engine(), (QScriptEngine*)0);

        QScriptValue v3(v);
        QCOMPARE(v3.strictlyEquals(v), true);
        QCOMPARE(v3.strictlyEquals(v2), true);
        QCOMPARE(v3.engine(), (QScriptEngine*)0);

        QScriptValue v4(2.0);
        QCOMPARE(v4.strictlyEquals(v), false);
        v3 = v4;
        QCOMPARE(v3.strictlyEquals(v), false);
        QCOMPARE(v3.strictlyEquals(v4), true);

        v2 = QScriptValue();
        QCOMPARE(v2.strictlyEquals(v), false);
        QCOMPARE(v.toNumber(), 1.0);

        QScriptValue v5(v);
        QCOMPARE(v5.strictlyEquals(v), true);
        v = QScriptValue();
        QCOMPARE(v5.strictlyEquals(v), false);
        QCOMPARE(v5.toNumber(), 1.0);
    }

    // 0 engine
    QVERIFY(QScriptValue(0, QScriptValue::UndefinedValue).isUndefined());
    QVERIFY(QScriptValue(0, QScriptValue::NullValue).isNull());
    QVERIFY(QScriptValue(0, false).isBool());
    QVERIFY(QScriptValue(0, int(1)).isNumber());
    QVERIFY(QScriptValue(0, uint(1)).isNumber());
    QVERIFY(QScriptValue(0, 1.0).isNumber());
    QVERIFY(QScriptValue(0, "ciao").isString());
    QVERIFY(QScriptValue(0, QString("ciao")).isString());
}

void tst_QScriptValue::toStringSimple_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("result");

    QTest::newRow("string") << QString::fromAscii("'hello'") << QString::fromAscii("hello");
    QTest::newRow("string utf") << QString::fromUtf8("'ąśćżźółńę'") << QString::fromUtf8("ąśćżźółńę");
    QTest::newRow("expression") << QString::fromAscii("1 + 4") << QString::fromAscii("5");
    QTest::newRow("null") << QString::fromAscii("null") << QString::fromAscii("null");
    QTest::newRow("boolean") << QString::fromAscii("false") << QString::fromAscii("false");
    QTest::newRow("undefined") << QString::fromAscii("undefined") << QString::fromAscii("undefined");
    QTest::newRow("object") << QString::fromAscii("new Object") << QString::fromAscii("[object Object]");
}

/* Test conversion to string from different JSC types */
void tst_QScriptValue::toStringSimple()
{
    QFETCH(QString, code);
    QFETCH(QString, result);

    QScriptEngine engine;
    QCOMPARE(engine.evaluate(code).toString(), result);
}

void tst_QScriptValue::copyConstructor_data()
{
    QScriptEngine engine;
    QScriptValue nnumber(123);
    QScriptValue nstring("ping");
    QScriptValue number(engine.evaluate("1"));
    QScriptValue string(engine.evaluate("'foo'"));
    QScriptValue object(engine.evaluate("new Object"));
    QScriptValue undefined(engine.evaluate("undefined"));
    QScriptValue null(engine.evaluate("null"));

    QTest::addColumn<QScriptValue>("value");
    QTest::addColumn<QString>("result");

    QTest::newRow("native number") << nnumber << QString::number(123);
    QTest::newRow("native string") << nstring << QString("ping");
    QTest::newRow("number") << number << QString::fromAscii("1");
    QTest::newRow("string") << string << QString::fromAscii("foo");
    QTest::newRow("object") << object << QString::fromAscii("[object Object]");
    QTest::newRow("undefined") << undefined << QString::fromAscii("undefined");
    QTest::newRow("null") << null << QString::fromAscii("null");
}

void tst_QScriptValue::copyConstructor()
{
    QFETCH(QScriptValue, value);
    QFETCH(QString, result);

    QVERIFY(value.isValid());
    QScriptValue tmp(value);
    QVERIFY(tmp.isValid());
    QCOMPARE(tmp.toString(), result);
}

void tst_QScriptValue::assignOperator_data()
{
    copyConstructor_data();
}

void tst_QScriptValue::assignOperator()
{
    QFETCH(QScriptValue, value);
    QFETCH(QString, result);

    QScriptValue tmp;
    tmp = value;
    QVERIFY(tmp.isValid());
    QCOMPARE(tmp.toString(), result);
}

/* Test internal data sharing between a diffrenet QScriptValue. */
void tst_QScriptValue::dataSharing()
{
    QScriptEngine engine;
    QScriptValue v1;
    QScriptValue v2(v1);

    v1 = engine.evaluate("1"); // v1 == 1 ; v2 invalid.
    QVERIFY(v1.isValid());
    QVERIFY(!v2.isValid());

    v2 = v1; // v1 == 1; v2 == 1.
    QVERIFY(v1.isValid());
    QVERIFY(v2.isValid());

    v1 = engine.evaluate("obj = new Date"); // v1 == [object Date] ; v2 == 1.
    QVERIFY(v1.isValid());
    QVERIFY(v2.isValid());
    QVERIFY(v2.toString() != v1.toString());

    // TODO add object manipulation (v1 and v2 point to the same object).
}

void tst_QScriptValue::constructors_data()
{
    QScriptEngine engine;

    QTest::addColumn<QScriptValue>("value");
    QTest::addColumn<QString>("string");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<bool>("object");

    QTest::newRow("invalid") << QScriptValue() << QString() << false << false;
    QTest::newRow("number") << QScriptValue(-21) << QString::number(-21) << true << false;
    QTest::newRow("bool") << QScriptValue(true) << QString::fromAscii("true") << true << false;
    QTest::newRow("double") << QScriptValue(21.12) << QString::number(21.12) << true << false;
    QTest::newRow("string") << QScriptValue("AlaMaKota") << QString::fromAscii("AlaMaKota") << true << false;
    QTest::newRow("object") << engine.evaluate("new Object") << QString::fromAscii("[object Object]") << true << true;
    QTest::newRow("null") << QScriptValue(QScriptValue::NullValue)<< QString::fromAscii("null") << true << false;
    QTest::newRow("undef") << QScriptValue(QScriptValue::UndefinedValue)<< QString::fromAscii("undefined") << true << false;
}

void tst_QScriptValue::constructors()
{
    QFETCH(QScriptValue, value);
    QFETCH(QString, string);
    QFETCH(bool, valid);
    QFETCH(bool, object);

    QCOMPARE(value.isValid(), valid);
    QCOMPARE(value.toString(), string);
    QCOMPARE(value.isObject(), object);
}

void tst_QScriptValue::call()
{
    QScriptEngine engine;
    QScriptValue ping = engine.evaluate("( function() {return 'ping';} )");
    QScriptValue incr = engine.evaluate("( function(i) {return i + 1;} )");
    QScriptValue one(1);
    QScriptValue five(5);
    QScriptValue result;

    QVERIFY(one.isValid());
    QVERIFY(five.isValid());

    QVERIFY(ping.isValid());
    QVERIFY(ping.isFunction());
    result = ping.call();
    QVERIFY(result.isValid());
    QCOMPARE(result.toString(), QString::fromUtf8("ping"));

    QVERIFY(incr.isValid());
    QVERIFY(incr.isFunction());
    result = incr.call(QScriptValue(), QScriptValueList() << one);
    QVERIFY(result.isValid());
    QCOMPARE(result.toString(), QString("2"));

    QCOMPARE(incr.call(QScriptValue(), QScriptValueList() << five).toString(), QString::fromAscii("6"));

    QVERIFY(incr.call().isValid()); // Exception.
}


QTEST_MAIN(tst_QScriptValue)
