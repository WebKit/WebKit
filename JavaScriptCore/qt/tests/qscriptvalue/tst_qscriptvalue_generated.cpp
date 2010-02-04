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

#include "tst_qscriptvalue.h"

#define DEFINE_TEST_VALUE(expr) m_values.insert(QString::fromLatin1(#expr), expr)

void tst_QScriptValue::initScriptValues()
{
    m_values.clear();
    if (engine)
        delete engine;
    engine = new QScriptEngine;
    DEFINE_TEST_VALUE(QScriptValue());
    DEFINE_TEST_VALUE(QScriptValue(QScriptValue::UndefinedValue));
    DEFINE_TEST_VALUE(QScriptValue(QScriptValue::NullValue));
    DEFINE_TEST_VALUE(QScriptValue(true));
    DEFINE_TEST_VALUE(QScriptValue(false));
    DEFINE_TEST_VALUE(QScriptValue(int(122)));
    DEFINE_TEST_VALUE(QScriptValue(uint(124)));
    DEFINE_TEST_VALUE(QScriptValue(0));
    DEFINE_TEST_VALUE(QScriptValue(0.0));
    DEFINE_TEST_VALUE(QScriptValue(123.0));
    DEFINE_TEST_VALUE(QScriptValue(6.37e-8));
    DEFINE_TEST_VALUE(QScriptValue(-6.37e-8));
    DEFINE_TEST_VALUE(QScriptValue(0x43211234));
    DEFINE_TEST_VALUE(QScriptValue(0x10000));
    DEFINE_TEST_VALUE(QScriptValue(0x10001));
    DEFINE_TEST_VALUE(QScriptValue(qSNaN()));
    DEFINE_TEST_VALUE(QScriptValue(qQNaN()));
    DEFINE_TEST_VALUE(QScriptValue(qInf()));
    DEFINE_TEST_VALUE(QScriptValue(-qInf()));
    DEFINE_TEST_VALUE(QScriptValue("NaN"));
    DEFINE_TEST_VALUE(QScriptValue("Infinity"));
    DEFINE_TEST_VALUE(QScriptValue("-Infinity"));
    DEFINE_TEST_VALUE(QScriptValue("ciao"));
    DEFINE_TEST_VALUE(QScriptValue(QString::fromLatin1("ciao")));
    DEFINE_TEST_VALUE(QScriptValue(QString("")));
    DEFINE_TEST_VALUE(QScriptValue(QString()));
    DEFINE_TEST_VALUE(QScriptValue(QString("0")));
    DEFINE_TEST_VALUE(QScriptValue(QString("123")));
    DEFINE_TEST_VALUE(QScriptValue(QString("12.4")));
    DEFINE_TEST_VALUE(QScriptValue(0, QScriptValue::UndefinedValue));
    DEFINE_TEST_VALUE(QScriptValue(0, QScriptValue::NullValue));
    DEFINE_TEST_VALUE(QScriptValue(0, true));
    DEFINE_TEST_VALUE(QScriptValue(0, false));
    DEFINE_TEST_VALUE(QScriptValue(0, int(122)));
    DEFINE_TEST_VALUE(QScriptValue(0, uint(124)));
    DEFINE_TEST_VALUE(QScriptValue(0, 0));
    DEFINE_TEST_VALUE(QScriptValue(0, 0.0));
    DEFINE_TEST_VALUE(QScriptValue(0, 123.0));
    DEFINE_TEST_VALUE(QScriptValue(0, 6.37e-8));
    DEFINE_TEST_VALUE(QScriptValue(0, -6.37e-8));
    DEFINE_TEST_VALUE(QScriptValue(0, 0x43211234));
    DEFINE_TEST_VALUE(QScriptValue(0, 0x10000));
    DEFINE_TEST_VALUE(QScriptValue(0, 0x10001));
    DEFINE_TEST_VALUE(QScriptValue(0, qSNaN()));
    DEFINE_TEST_VALUE(QScriptValue(0, qQNaN()));
    DEFINE_TEST_VALUE(QScriptValue(0, qInf()));
    DEFINE_TEST_VALUE(QScriptValue(0, -qInf()));
    DEFINE_TEST_VALUE(QScriptValue(0, "NaN"));
    DEFINE_TEST_VALUE(QScriptValue(0, "Infinity"));
    DEFINE_TEST_VALUE(QScriptValue(0, "-Infinity"));
    DEFINE_TEST_VALUE(QScriptValue(0, "ciao"));
    DEFINE_TEST_VALUE(QScriptValue(0, QString::fromLatin1("ciao")));
    DEFINE_TEST_VALUE(QScriptValue(0, QString("")));
    DEFINE_TEST_VALUE(QScriptValue(0, QString()));
    DEFINE_TEST_VALUE(QScriptValue(0, QString("0")));
    DEFINE_TEST_VALUE(QScriptValue(0, QString("123")));
    DEFINE_TEST_VALUE(QScriptValue(0, QString("12.3")));
    DEFINE_TEST_VALUE(QScriptValue(engine, QScriptValue::UndefinedValue));
    DEFINE_TEST_VALUE(QScriptValue(engine, QScriptValue::NullValue));
    DEFINE_TEST_VALUE(QScriptValue(engine, true));
    DEFINE_TEST_VALUE(QScriptValue(engine, false));
    DEFINE_TEST_VALUE(QScriptValue(engine, int(122)));
    DEFINE_TEST_VALUE(QScriptValue(engine, uint(124)));
    DEFINE_TEST_VALUE(QScriptValue(engine, 0));
    DEFINE_TEST_VALUE(QScriptValue(engine, 0.0));
    DEFINE_TEST_VALUE(QScriptValue(engine, 123.0));
    DEFINE_TEST_VALUE(QScriptValue(engine, 6.37e-8));
    DEFINE_TEST_VALUE(QScriptValue(engine, -6.37e-8));
    DEFINE_TEST_VALUE(QScriptValue(engine, 0x43211234));
    DEFINE_TEST_VALUE(QScriptValue(engine, 0x10000));
    DEFINE_TEST_VALUE(QScriptValue(engine, 0x10001));
    DEFINE_TEST_VALUE(QScriptValue(engine, qSNaN()));
    DEFINE_TEST_VALUE(QScriptValue(engine, qQNaN()));
    DEFINE_TEST_VALUE(QScriptValue(engine, qInf()));
    DEFINE_TEST_VALUE(QScriptValue(engine, -qInf()));
    DEFINE_TEST_VALUE(QScriptValue(engine, "NaN"));
    DEFINE_TEST_VALUE(QScriptValue(engine, "Infinity"));
    DEFINE_TEST_VALUE(QScriptValue(engine, "-Infinity"));
    DEFINE_TEST_VALUE(QScriptValue(engine, "ciao"));
    DEFINE_TEST_VALUE(QScriptValue(engine, QString::fromLatin1("ciao")));
    DEFINE_TEST_VALUE(QScriptValue(engine, QString("")));
    DEFINE_TEST_VALUE(QScriptValue(engine, QString()));
    DEFINE_TEST_VALUE(QScriptValue(engine, QString("0")));
    DEFINE_TEST_VALUE(QScriptValue(engine, QString("123")));
    DEFINE_TEST_VALUE(QScriptValue(engine, QString("1.23")));
    DEFINE_TEST_VALUE(engine->evaluate("[]"));
    DEFINE_TEST_VALUE(engine->evaluate("{}"));
    DEFINE_TEST_VALUE(engine->evaluate("Object.prototype"));
    DEFINE_TEST_VALUE(engine->evaluate("Date.prototype"));
    DEFINE_TEST_VALUE(engine->evaluate("Array.prototype"));
    DEFINE_TEST_VALUE(engine->evaluate("Function.prototype"));
    DEFINE_TEST_VALUE(engine->evaluate("Error.prototype"));
    DEFINE_TEST_VALUE(engine->evaluate("Object"));
    DEFINE_TEST_VALUE(engine->evaluate("Array"));
    DEFINE_TEST_VALUE(engine->evaluate("Number"));
    DEFINE_TEST_VALUE(engine->evaluate("Function"));
    DEFINE_TEST_VALUE(engine->evaluate("(function() { return 1; })"));
    DEFINE_TEST_VALUE(engine->evaluate("(function() { return 'ciao'; })"));
    DEFINE_TEST_VALUE(engine->evaluate("(function() { throw new Error('foo'); })"));
    DEFINE_TEST_VALUE(engine->evaluate("/foo/"));
    DEFINE_TEST_VALUE(engine->evaluate("new Object()"));
    DEFINE_TEST_VALUE(engine->evaluate("new Array()"));
    DEFINE_TEST_VALUE(engine->evaluate("new Error()"));
}


void tst_QScriptValue::isValid_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

void tst_QScriptValue::isValid_makeData(const char* expr)
{
    static QSet<QString> isValid;
    if (isValid.isEmpty()) {
        isValid << "QScriptValue(QScriptValue::UndefinedValue)"
                << "QScriptValue(QScriptValue::NullValue)"
                << "QScriptValue(true)"
                << "QScriptValue(false)"
                << "QScriptValue(int(122))"
                << "QScriptValue(uint(124))"
                << "QScriptValue(0)"
                << "QScriptValue(0.0)"
                << "QScriptValue(123.0)"
                << "QScriptValue(6.37e-8)"
                << "QScriptValue(-6.37e-8)"
                << "QScriptValue(0x43211234)"
                << "QScriptValue(0x10000)"
                << "QScriptValue(0x10001)"
                << "QScriptValue(qSNaN())"
                << "QScriptValue(qQNaN())"
                << "QScriptValue(qInf())"
                << "QScriptValue(-qInf())"
                << "QScriptValue(\"NaN\")"
                << "QScriptValue(\"Infinity\")"
                << "QScriptValue(\"-Infinity\")"
                << "QScriptValue(\"ciao\")"
                << "QScriptValue(QString::fromLatin1(\"ciao\"))"
                << "QScriptValue(QString(\"\"))"
                << "QScriptValue(QString())"
                << "QScriptValue(QString(\"0\"))"
                << "QScriptValue(QString(\"123\"))"
                << "QScriptValue(QString(\"12.4\"))"
                << "QScriptValue(0, QScriptValue::UndefinedValue)"
                << "QScriptValue(0, QScriptValue::NullValue)"
                << "QScriptValue(0, true)"
                << "QScriptValue(0, false)"
                << "QScriptValue(0, int(122))"
                << "QScriptValue(0, uint(124))"
                << "QScriptValue(0, 0)"
                << "QScriptValue(0, 0.0)"
                << "QScriptValue(0, 123.0)"
                << "QScriptValue(0, 6.37e-8)"
                << "QScriptValue(0, -6.37e-8)"
                << "QScriptValue(0, 0x43211234)"
                << "QScriptValue(0, 0x10000)"
                << "QScriptValue(0, 0x10001)"
                << "QScriptValue(0, qSNaN())"
                << "QScriptValue(0, qQNaN())"
                << "QScriptValue(0, qInf())"
                << "QScriptValue(0, -qInf())"
                << "QScriptValue(0, \"NaN\")"
                << "QScriptValue(0, \"Infinity\")"
                << "QScriptValue(0, \"-Infinity\")"
                << "QScriptValue(0, \"ciao\")"
                << "QScriptValue(0, QString::fromLatin1(\"ciao\"))"
                << "QScriptValue(0, QString(\"\"))"
                << "QScriptValue(0, QString())"
                << "QScriptValue(0, QString(\"0\"))"
                << "QScriptValue(0, QString(\"123\"))"
                << "QScriptValue(0, QString(\"12.3\"))"
                << "QScriptValue(engine, QScriptValue::UndefinedValue)"
                << "QScriptValue(engine, QScriptValue::NullValue)"
                << "QScriptValue(engine, true)"
                << "QScriptValue(engine, false)"
                << "QScriptValue(engine, int(122))"
                << "QScriptValue(engine, uint(124))"
                << "QScriptValue(engine, 0)"
                << "QScriptValue(engine, 0.0)"
                << "QScriptValue(engine, 123.0)"
                << "QScriptValue(engine, 6.37e-8)"
                << "QScriptValue(engine, -6.37e-8)"
                << "QScriptValue(engine, 0x43211234)"
                << "QScriptValue(engine, 0x10000)"
                << "QScriptValue(engine, 0x10001)"
                << "QScriptValue(engine, qSNaN())"
                << "QScriptValue(engine, qQNaN())"
                << "QScriptValue(engine, qInf())"
                << "QScriptValue(engine, -qInf())"
                << "QScriptValue(engine, \"NaN\")"
                << "QScriptValue(engine, \"Infinity\")"
                << "QScriptValue(engine, \"-Infinity\")"
                << "QScriptValue(engine, \"ciao\")"
                << "QScriptValue(engine, QString::fromLatin1(\"ciao\"))"
                << "QScriptValue(engine, QString(\"\"))"
                << "QScriptValue(engine, QString())"
                << "QScriptValue(engine, QString(\"0\"))"
                << "QScriptValue(engine, QString(\"123\"))"
                << "QScriptValue(engine, QString(\"1.23\"))"
                << "engine->evaluate(\"[]\")"
                << "engine->evaluate(\"{}\")"
                << "engine->evaluate(\"Object.prototype\")"
                << "engine->evaluate(\"Date.prototype\")"
                << "engine->evaluate(\"Array.prototype\")"
                << "engine->evaluate(\"Function.prototype\")"
                << "engine->evaluate(\"Error.prototype\")"
                << "engine->evaluate(\"Object\")"
                << "engine->evaluate(\"Array\")"
                << "engine->evaluate(\"Number\")"
                << "engine->evaluate(\"Function\")"
                << "engine->evaluate(\"(function() { return 1; })\")"
                << "engine->evaluate(\"(function() { return 'ciao'; })\")"
                << "engine->evaluate(\"(function() { throw new Error('foo'); })\")"
                << "engine->evaluate(\"/foo/\")"
                << "engine->evaluate(\"new Object()\")"
                << "engine->evaluate(\"new Array()\")"
                << "engine->evaluate(\"new Error()\")";
    }
    newRow(expr) << isValid.contains(expr);
}

void tst_QScriptValue::isValid_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isValid(), expected);
}

DEFINE_TEST_FUNCTION(isValid)


void tst_QScriptValue::isBool_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

void tst_QScriptValue::isBool_makeData(const char* expr)
{
    static QSet<QString> isBool;
    if (isBool.isEmpty()) {
        isBool << "QScriptValue(true)"
                << "QScriptValue(false)"
                << "QScriptValue(0, true)"
                << "QScriptValue(0, false)"
                << "QScriptValue(engine, true)"
                << "QScriptValue(engine, false)";
    }
    newRow(expr) << isBool.contains(expr);
}

void tst_QScriptValue::isBool_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isBool(), expected);
}

DEFINE_TEST_FUNCTION(isBool)


void tst_QScriptValue::isBoolean_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

void tst_QScriptValue::isBoolean_makeData(const char* expr)
{
    static QSet<QString> isBoolean;
    if (isBoolean.isEmpty()) {
        isBoolean << "QScriptValue(true)"
                << "QScriptValue(false)"
                << "QScriptValue(0, true)"
                << "QScriptValue(0, false)"
                << "QScriptValue(engine, true)"
                << "QScriptValue(engine, false)";
    }
    newRow(expr) << isBoolean.contains(expr);
}

void tst_QScriptValue::isBoolean_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isBoolean(), expected);
}

DEFINE_TEST_FUNCTION(isBoolean)


void tst_QScriptValue::isFunction_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

void tst_QScriptValue::isFunction_makeData(const char* expr)
{
    static QSet<QString> isFunction;
    if (isFunction.isEmpty()) {
        isFunction << "engine->evaluate(\"Function.prototype\")"
                << "engine->evaluate(\"Object\")"
                << "engine->evaluate(\"Array\")"
                << "engine->evaluate(\"Number\")"
                << "engine->evaluate(\"Function\")"
                << "engine->evaluate(\"(function() { return 1; })\")"
                << "engine->evaluate(\"(function() { return 'ciao'; })\")"
                << "engine->evaluate(\"(function() { throw new Error('foo'); })\")"
                << "engine->evaluate(\"/foo/\")";
    }
    newRow(expr) << isFunction.contains(expr);
}

void tst_QScriptValue::isFunction_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isFunction(), expected);
}

DEFINE_TEST_FUNCTION(isFunction)


void tst_QScriptValue::isNull_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

void tst_QScriptValue::isNull_makeData(const char* expr)
{
    static QSet<QString> isNull;
    if (isNull.isEmpty()) {
        isNull << "QScriptValue(QScriptValue::NullValue)"
                << "QScriptValue(0, QScriptValue::NullValue)"
                << "QScriptValue(engine, QScriptValue::NullValue)";
    }
    newRow(expr) << isNull.contains(expr);
}

void tst_QScriptValue::isNull_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isNull(), expected);
}

DEFINE_TEST_FUNCTION(isNull)


void tst_QScriptValue::isString_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

void tst_QScriptValue::isString_makeData(const char* expr)
{
    static QSet<QString> isString;
    if (isString.isEmpty()) {
        isString << "QScriptValue(\"NaN\")"
                << "QScriptValue(\"Infinity\")"
                << "QScriptValue(\"-Infinity\")"
                << "QScriptValue(\"ciao\")"
                << "QScriptValue(QString::fromLatin1(\"ciao\"))"
                << "QScriptValue(QString(\"\"))"
                << "QScriptValue(QString())"
                << "QScriptValue(QString(\"0\"))"
                << "QScriptValue(QString(\"123\"))"
                << "QScriptValue(QString(\"12.4\"))"
                << "QScriptValue(0, \"NaN\")"
                << "QScriptValue(0, \"Infinity\")"
                << "QScriptValue(0, \"-Infinity\")"
                << "QScriptValue(0, \"ciao\")"
                << "QScriptValue(0, QString::fromLatin1(\"ciao\"))"
                << "QScriptValue(0, QString(\"\"))"
                << "QScriptValue(0, QString())"
                << "QScriptValue(0, QString(\"0\"))"
                << "QScriptValue(0, QString(\"123\"))"
                << "QScriptValue(0, QString(\"12.3\"))"
                << "QScriptValue(engine, \"NaN\")"
                << "QScriptValue(engine, \"Infinity\")"
                << "QScriptValue(engine, \"-Infinity\")"
                << "QScriptValue(engine, \"ciao\")"
                << "QScriptValue(engine, QString::fromLatin1(\"ciao\"))"
                << "QScriptValue(engine, QString(\"\"))"
                << "QScriptValue(engine, QString())"
                << "QScriptValue(engine, QString(\"0\"))"
                << "QScriptValue(engine, QString(\"123\"))"
                << "QScriptValue(engine, QString(\"1.23\"))";
    }
    newRow(expr) << isString.contains(expr);
}

void tst_QScriptValue::isString_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isString(), expected);
}

DEFINE_TEST_FUNCTION(isString)


void tst_QScriptValue::isUndefined_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

void tst_QScriptValue::isUndefined_makeData(const char* expr)
{
    static QSet<QString> isUndefined;
    if (isUndefined.isEmpty()) {
        isUndefined << "QScriptValue(QScriptValue::UndefinedValue)"
                << "QScriptValue(0, QScriptValue::UndefinedValue)"
                << "QScriptValue(engine, QScriptValue::UndefinedValue)"
                << "engine->evaluate(\"{}\")";
    }
    newRow(expr) << isUndefined.contains(expr);
}

void tst_QScriptValue::isUndefined_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isUndefined(), expected);
}

DEFINE_TEST_FUNCTION(isUndefined)

void tst_QScriptValue::isObject_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

void tst_QScriptValue::isObject_makeData(const char* expr)
{
    static QSet<QString> isObject;
    if (isObject.isEmpty()) {
        isObject << "engine->evaluate(\"[]\")"
                << "engine->evaluate(\"Object.prototype\")"
                << "engine->evaluate(\"Date.prototype\")"
                << "engine->evaluate(\"Array.prototype\")"
                << "engine->evaluate(\"Function.prototype\")"
                << "engine->evaluate(\"Error.prototype\")"
                << "engine->evaluate(\"Object\")"
                << "engine->evaluate(\"Array\")"
                << "engine->evaluate(\"Number\")"
                << "engine->evaluate(\"Function\")"
                << "engine->evaluate(\"(function() { return 1; })\")"
                << "engine->evaluate(\"(function() { return 'ciao'; })\")"
                << "engine->evaluate(\"(function() { throw new Error('foo'); })\")"
                << "engine->evaluate(\"/foo/\")"
                << "engine->evaluate(\"new Object()\")"
                << "engine->evaluate(\"new Array()\")"
                << "engine->evaluate(\"new Error()\")";
    }
    newRow(expr) << isObject.contains(expr);
}

void tst_QScriptValue::isObject_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isObject(), expected);
}

DEFINE_TEST_FUNCTION(isObject)


