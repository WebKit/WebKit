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

void tst_QScriptValue::isNumber_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

void tst_QScriptValue::isNumber_makeData(const char* expr)
{
    static QSet<QString> isNumber;
    if (isNumber.isEmpty()) {
        isNumber << "QScriptValue(int(122))"
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
                << "QScriptValue(engine, -qInf())";
    }
    newRow(expr) << isNumber.contains(expr);
}

void tst_QScriptValue::isNumber_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isNumber(), expected);
}

DEFINE_TEST_FUNCTION(isNumber)


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

void tst_QScriptValue::toString_initData()
{
    QTest::addColumn<QString>("expected");
    initScriptValues();
}

void tst_QScriptValue::toString_makeData(const char* expr)
{
    static QHash<QString, QString> toString;
    if (toString.isEmpty()) {
        toString.insert("QScriptValue()", "");
        toString.insert("QScriptValue(QScriptValue::UndefinedValue)", "undefined");
        toString.insert("QScriptValue(QScriptValue::NullValue)", "null");
        toString.insert("QScriptValue(true)", "true");
        toString.insert("QScriptValue(false)", "false");
        toString.insert("QScriptValue(int(122))", "122");
        toString.insert("QScriptValue(uint(124))", "124");
        toString.insert("QScriptValue(0)", "0");
        toString.insert("QScriptValue(0.0)", "0");
        toString.insert("QScriptValue(123.0)", "123");
        toString.insert("QScriptValue(6.37e-8)", "6.37e-8");
        toString.insert("QScriptValue(-6.37e-8)", "-6.37e-8");
        toString.insert("QScriptValue(0x43211234)", "1126240820");
        toString.insert("QScriptValue(0x10000)", "65536");
        toString.insert("QScriptValue(0x10001)", "65537");
        toString.insert("QScriptValue(qSNaN())", "NaN");
        toString.insert("QScriptValue(qQNaN())", "NaN");
        toString.insert("QScriptValue(qInf())", "Infinity");
        toString.insert("QScriptValue(-qInf())", "-Infinity");
        toString.insert("QScriptValue(\"NaN\")", "NaN");
        toString.insert("QScriptValue(\"Infinity\")", "Infinity");
        toString.insert("QScriptValue(\"-Infinity\")", "-Infinity");
        toString.insert("QScriptValue(\"ciao\")", "ciao");
        toString.insert("QScriptValue(QString::fromLatin1(\"ciao\"))", "ciao");
        toString.insert("QScriptValue(QString(\"\"))", "");
        toString.insert("QScriptValue(QString())", "");
        toString.insert("QScriptValue(QString(\"0\"))", "0");
        toString.insert("QScriptValue(QString(\"123\"))", "123");
        toString.insert("QScriptValue(QString(\"12.4\"))", "12.4");
        toString.insert("QScriptValue(0, QScriptValue::UndefinedValue)", "undefined");
        toString.insert("QScriptValue(0, QScriptValue::NullValue)", "null");
        toString.insert("QScriptValue(0, true)", "true");
        toString.insert("QScriptValue(0, false)", "false");
        toString.insert("QScriptValue(0, int(122))", "122");
        toString.insert("QScriptValue(0, uint(124))", "124");
        toString.insert("QScriptValue(0, 0)", "0");
        toString.insert("QScriptValue(0, 0.0)", "0");
        toString.insert("QScriptValue(0, 123.0)", "123");
        toString.insert("QScriptValue(0, 6.37e-8)", "6.37e-8");
        toString.insert("QScriptValue(0, -6.37e-8)", "-6.37e-8");
        toString.insert("QScriptValue(0, 0x43211234)", "1126240820");
        toString.insert("QScriptValue(0, 0x10000)", "65536");
        toString.insert("QScriptValue(0, 0x10001)", "65537");
        toString.insert("QScriptValue(0, qSNaN())", "NaN");
        toString.insert("QScriptValue(0, qQNaN())", "NaN");
        toString.insert("QScriptValue(0, qInf())", "Infinity");
        toString.insert("QScriptValue(0, -qInf())", "-Infinity");
        toString.insert("QScriptValue(0, \"NaN\")", "NaN");
        toString.insert("QScriptValue(0, \"Infinity\")", "Infinity");
        toString.insert("QScriptValue(0, \"-Infinity\")", "-Infinity");
        toString.insert("QScriptValue(0, \"ciao\")", "ciao");
        toString.insert("QScriptValue(0, QString::fromLatin1(\"ciao\"))", "ciao");
        toString.insert("QScriptValue(0, QString(\"\"))", "");
        toString.insert("QScriptValue(0, QString())", "");
        toString.insert("QScriptValue(0, QString(\"0\"))", "0");
        toString.insert("QScriptValue(0, QString(\"123\"))", "123");
        toString.insert("QScriptValue(0, QString(\"12.3\"))", "12.3");
        toString.insert("QScriptValue(engine, QScriptValue::UndefinedValue)", "undefined");
        toString.insert("QScriptValue(engine, QScriptValue::NullValue)", "null");
        toString.insert("QScriptValue(engine, true)", "true");
        toString.insert("QScriptValue(engine, false)", "false");
        toString.insert("QScriptValue(engine, int(122))", "122");
        toString.insert("QScriptValue(engine, uint(124))", "124");
        toString.insert("QScriptValue(engine, 0)", "0");
        toString.insert("QScriptValue(engine, 0.0)", "0");
        toString.insert("QScriptValue(engine, 123.0)", "123");
        toString.insert("QScriptValue(engine, 6.37e-8)", "6.37e-8");
        toString.insert("QScriptValue(engine, -6.37e-8)", "-6.37e-8");
        toString.insert("QScriptValue(engine, 0x43211234)", "1126240820");
        toString.insert("QScriptValue(engine, 0x10000)", "65536");
        toString.insert("QScriptValue(engine, 0x10001)", "65537");
        toString.insert("QScriptValue(engine, qSNaN())", "NaN");
        toString.insert("QScriptValue(engine, qQNaN())", "NaN");
        toString.insert("QScriptValue(engine, qInf())", "Infinity");
        toString.insert("QScriptValue(engine, -qInf())", "-Infinity");
        toString.insert("QScriptValue(engine, \"NaN\")", "NaN");
        toString.insert("QScriptValue(engine, \"Infinity\")", "Infinity");
        toString.insert("QScriptValue(engine, \"-Infinity\")", "-Infinity");
        toString.insert("QScriptValue(engine, \"ciao\")", "ciao");
        toString.insert("QScriptValue(engine, QString::fromLatin1(\"ciao\"))", "ciao");
        toString.insert("QScriptValue(engine, QString(\"\"))", "");
        toString.insert("QScriptValue(engine, QString())", "");
        toString.insert("QScriptValue(engine, QString(\"0\"))", "0");
        toString.insert("QScriptValue(engine, QString(\"123\"))", "123");
        toString.insert("QScriptValue(engine, QString(\"1.23\"))", "1.23");
        toString.insert("engine->evaluate(\"[]\")", "");
        toString.insert("engine->evaluate(\"{}\")", "undefined");
        toString.insert("engine->evaluate(\"Object.prototype\")", "[object Object]");
        toString.insert("engine->evaluate(\"Date.prototype\")", "Invalid Date");
        toString.insert("engine->evaluate(\"Array.prototype\")", "");
        toString.insert("engine->evaluate(\"Function.prototype\")", "function () {\n    [native code]\n}");
        toString.insert("engine->evaluate(\"Error.prototype\")", "Error: Unknown error");
        toString.insert("engine->evaluate(\"Object\")", "function Object() {\n    [native code]\n}");
        toString.insert("engine->evaluate(\"Array\")", "function Array() {\n    [native code]\n}");
        toString.insert("engine->evaluate(\"Number\")", "function Number() {\n    [native code]\n}");
        toString.insert("engine->evaluate(\"Function\")", "function Function() {\n    [native code]\n}");
        toString.insert("engine->evaluate(\"(function() { return 1; })\")", "function () { return 1; }");
        toString.insert("engine->evaluate(\"(function() { return 'ciao'; })\")", "function () { return 'ciao'; }");
        toString.insert("engine->evaluate(\"(function() { throw new Error('foo'); })\")", "function () { throw new Error('foo'); }");
        toString.insert("engine->evaluate(\"/foo/\")", "/foo/");
        toString.insert("engine->evaluate(\"new Object()\")", "[object Object]");
        toString.insert("engine->evaluate(\"new Array()\")", "");
        toString.insert("engine->evaluate(\"new Error()\")", "Error: Unknown error");
    }
    newRow(expr) << toString.value(expr);
}

void tst_QScriptValue::toString_test(const char*, const QScriptValue& value)
{
    QFETCH(QString, expected);
    QCOMPARE(value.toString(), expected);
}

DEFINE_TEST_FUNCTION(toString)

void tst_QScriptValue::toNumber_initData()
{
    QTest::addColumn<qsreal>("expected");
    initScriptValues();
}

void tst_QScriptValue::toNumber_makeData(const char* expr)
{
    static QHash<QString, qsreal> toNumber;
    if (toNumber.isEmpty()) {
        toNumber.insert("QScriptValue()", 0);
        toNumber.insert("QScriptValue(QScriptValue::UndefinedValue)", qQNaN());
        toNumber.insert("QScriptValue(QScriptValue::NullValue)", 0);
        toNumber.insert("QScriptValue(true)", 1);
        toNumber.insert("QScriptValue(false)", 0);
        toNumber.insert("QScriptValue(int(122))", 122);
        toNumber.insert("QScriptValue(uint(124))", 124);
        toNumber.insert("QScriptValue(0)", 0);
        toNumber.insert("QScriptValue(0.0)", 0);
        toNumber.insert("QScriptValue(123.0)", 123);
        toNumber.insert("QScriptValue(6.37e-8)", 6.369999999999999e-08);
        toNumber.insert("QScriptValue(-6.37e-8)", -6.369999999999999e-08);
        toNumber.insert("QScriptValue(0x43211234)", 1126240820);
        toNumber.insert("QScriptValue(0x10000)", 65536);
        toNumber.insert("QScriptValue(0x10001)", 65537);
        toNumber.insert("QScriptValue(qSNaN())", qQNaN());
        toNumber.insert("QScriptValue(qQNaN())", qQNaN());
        toNumber.insert("QScriptValue(qInf())", qInf());
        toNumber.insert("QScriptValue(-qInf())", qInf());
        toNumber.insert("QScriptValue(\"NaN\")", qQNaN());
        toNumber.insert("QScriptValue(\"Infinity\")", qInf());
        toNumber.insert("QScriptValue(\"-Infinity\")", qInf());
        toNumber.insert("QScriptValue(\"ciao\")", qQNaN());
        toNumber.insert("QScriptValue(QString::fromLatin1(\"ciao\"))", qQNaN());
        toNumber.insert("QScriptValue(QString(\"\"))", 0);
        toNumber.insert("QScriptValue(QString())", 0);
        toNumber.insert("QScriptValue(QString(\"0\"))", 0);
        toNumber.insert("QScriptValue(QString(\"123\"))", 123);
        toNumber.insert("QScriptValue(QString(\"12.4\"))", 12.4);
        toNumber.insert("QScriptValue(0, QScriptValue::UndefinedValue)", qQNaN());
        toNumber.insert("QScriptValue(0, QScriptValue::NullValue)", 0);
        toNumber.insert("QScriptValue(0, true)", 1);
        toNumber.insert("QScriptValue(0, false)", 0);
        toNumber.insert("QScriptValue(0, int(122))", 122);
        toNumber.insert("QScriptValue(0, uint(124))", 124);
        toNumber.insert("QScriptValue(0, 0)", 0);
        toNumber.insert("QScriptValue(0, 0.0)", 0);
        toNumber.insert("QScriptValue(0, 123.0)", 123);
        toNumber.insert("QScriptValue(0, 6.37e-8)", 6.369999999999999e-08);
        toNumber.insert("QScriptValue(0, -6.37e-8)", -6.369999999999999e-08);
        toNumber.insert("QScriptValue(0, 0x43211234)", 1126240820);
        toNumber.insert("QScriptValue(0, 0x10000)", 65536);
        toNumber.insert("QScriptValue(0, 0x10001)", 65537);
        toNumber.insert("QScriptValue(0, qSNaN())", qQNaN());
        toNumber.insert("QScriptValue(0, qQNaN())", qQNaN());
        toNumber.insert("QScriptValue(0, qInf())", qInf());
        toNumber.insert("QScriptValue(0, -qInf())", qInf());
        toNumber.insert("QScriptValue(0, \"NaN\")", qQNaN());
        toNumber.insert("QScriptValue(0, \"Infinity\")", qInf());
        toNumber.insert("QScriptValue(0, \"-Infinity\")", qInf());
        toNumber.insert("QScriptValue(0, \"ciao\")", qQNaN());
        toNumber.insert("QScriptValue(0, QString::fromLatin1(\"ciao\"))", qQNaN());
        toNumber.insert("QScriptValue(0, QString(\"\"))", 0);
        toNumber.insert("QScriptValue(0, QString())", 0);
        toNumber.insert("QScriptValue(0, QString(\"0\"))", 0);
        toNumber.insert("QScriptValue(0, QString(\"123\"))", 123);
        toNumber.insert("QScriptValue(0, QString(\"12.3\"))", 12.3);
        toNumber.insert("QScriptValue(engine, QScriptValue::UndefinedValue)", qQNaN());
        toNumber.insert("QScriptValue(engine, QScriptValue::NullValue)", 0);
        toNumber.insert("QScriptValue(engine, true)", 1);
        toNumber.insert("QScriptValue(engine, false)", 0);
        toNumber.insert("QScriptValue(engine, int(122))", 122);
        toNumber.insert("QScriptValue(engine, uint(124))", 124);
        toNumber.insert("QScriptValue(engine, 0)", 0);
        toNumber.insert("QScriptValue(engine, 0.0)", 0);
        toNumber.insert("QScriptValue(engine, 123.0)", 123);
        toNumber.insert("QScriptValue(engine, 6.37e-8)", 6.369999999999999e-08);
        toNumber.insert("QScriptValue(engine, -6.37e-8)", -6.369999999999999e-08);
        toNumber.insert("QScriptValue(engine, 0x43211234)", 1126240820);
        toNumber.insert("QScriptValue(engine, 0x10000)", 65536);
        toNumber.insert("QScriptValue(engine, 0x10001)", 65537);
        toNumber.insert("QScriptValue(engine, qSNaN())", qQNaN());
        toNumber.insert("QScriptValue(engine, qQNaN())", qQNaN());
        toNumber.insert("QScriptValue(engine, qInf())", qInf());
        toNumber.insert("QScriptValue(engine, -qInf())", qInf());
        toNumber.insert("QScriptValue(engine, \"NaN\")", qQNaN());
        toNumber.insert("QScriptValue(engine, \"Infinity\")", qInf());
        toNumber.insert("QScriptValue(engine, \"-Infinity\")", qInf());
        toNumber.insert("QScriptValue(engine, \"ciao\")", qQNaN());
        toNumber.insert("QScriptValue(engine, QString::fromLatin1(\"ciao\"))", qQNaN());
        toNumber.insert("QScriptValue(engine, QString(\"\"))", 0);
        toNumber.insert("QScriptValue(engine, QString())", 0);
        toNumber.insert("QScriptValue(engine, QString(\"0\"))", 0);
        toNumber.insert("QScriptValue(engine, QString(\"123\"))", 123);
        toNumber.insert("QScriptValue(engine, QString(\"1.23\"))", 1.23);
        toNumber.insert("engine->evaluate(\"[]\")", 0);
        toNumber.insert("engine->evaluate(\"{}\")", qQNaN());
        toNumber.insert("engine->evaluate(\"Object.prototype\")", qQNaN());
        toNumber.insert("engine->evaluate(\"Date.prototype\")", qQNaN());
        toNumber.insert("engine->evaluate(\"Array.prototype\")", 0);
        toNumber.insert("engine->evaluate(\"Function.prototype\")", qQNaN());
        toNumber.insert("engine->evaluate(\"Error.prototype\")", qQNaN());
        toNumber.insert("engine->evaluate(\"Object\")", qQNaN());
        toNumber.insert("engine->evaluate(\"Array\")", qQNaN());
        toNumber.insert("engine->evaluate(\"Number\")", qQNaN());
        toNumber.insert("engine->evaluate(\"Function\")", qQNaN());
        toNumber.insert("engine->evaluate(\"(function() { return 1; })\")", qQNaN());
        toNumber.insert("engine->evaluate(\"(function() { return 'ciao'; })\")", qQNaN());
        toNumber.insert("engine->evaluate(\"(function() { throw new Error('foo'); })\")", qQNaN());
        toNumber.insert("engine->evaluate(\"/foo/\")", qQNaN());
        toNumber.insert("engine->evaluate(\"new Object()\")", qQNaN());
        toNumber.insert("engine->evaluate(\"new Array()\")", 0);
        toNumber.insert("engine->evaluate(\"new Error()\")", qQNaN());
    }
    newRow(expr) << toNumber.value(expr);
}

void tst_QScriptValue::toNumber_test(const char*, const QScriptValue& value)
{
    QFETCH(qsreal, expected);
    if (qIsNaN(expected)) {
        QVERIFY(qIsNaN(value.toNumber()));
        return;
    }
    if (qIsInf(expected)) {
        QVERIFY(qIsInf(value.toNumber()));
        return;
    }
    QCOMPARE(value.toNumber(), expected);
}

DEFINE_TEST_FUNCTION(toNumber)


void tst_QScriptValue::toBool_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

void tst_QScriptValue::toBool_makeData(const char* expr)
{
    static QHash<QString, bool> toBool;
    if (toBool.isEmpty()) {
        toBool.insert("QScriptValue()", false);
        toBool.insert("QScriptValue(QScriptValue::UndefinedValue)", false);
        toBool.insert("QScriptValue(QScriptValue::NullValue)", false);
        toBool.insert("QScriptValue(true)", true);
        toBool.insert("QScriptValue(false)", false);
        toBool.insert("QScriptValue(int(122))", true);
        toBool.insert("QScriptValue(uint(124))", true);
        toBool.insert("QScriptValue(0)", false);
        toBool.insert("QScriptValue(0.0)", false);
        toBool.insert("QScriptValue(123.0)", true);
        toBool.insert("QScriptValue(6.37e-8)", true);
        toBool.insert("QScriptValue(-6.37e-8)", true);
        toBool.insert("QScriptValue(0x43211234)", true);
        toBool.insert("QScriptValue(0x10000)", true);
        toBool.insert("QScriptValue(0x10001)", true);
        toBool.insert("QScriptValue(qSNaN())", false);
        toBool.insert("QScriptValue(qQNaN())", false);
        toBool.insert("QScriptValue(qInf())", true);
        toBool.insert("QScriptValue(-qInf())", true);
        toBool.insert("QScriptValue(\"NaN\")", true);
        toBool.insert("QScriptValue(\"Infinity\")", true);
        toBool.insert("QScriptValue(\"-Infinity\")", true);
        toBool.insert("QScriptValue(\"ciao\")", true);
        toBool.insert("QScriptValue(QString::fromLatin1(\"ciao\"))", true);
        toBool.insert("QScriptValue(QString(\"\"))", false);
        toBool.insert("QScriptValue(QString())", false);
        toBool.insert("QScriptValue(QString(\"0\"))", true);
        toBool.insert("QScriptValue(QString(\"123\"))", true);
        toBool.insert("QScriptValue(QString(\"12.4\"))", true);
        toBool.insert("QScriptValue(0, QScriptValue::UndefinedValue)", false);
        toBool.insert("QScriptValue(0, QScriptValue::NullValue)", false);
        toBool.insert("QScriptValue(0, true)", true);
        toBool.insert("QScriptValue(0, false)", false);
        toBool.insert("QScriptValue(0, int(122))", true);
        toBool.insert("QScriptValue(0, uint(124))", true);
        toBool.insert("QScriptValue(0, 0)", false);
        toBool.insert("QScriptValue(0, 0.0)", false);
        toBool.insert("QScriptValue(0, 123.0)", true);
        toBool.insert("QScriptValue(0, 6.37e-8)", true);
        toBool.insert("QScriptValue(0, -6.37e-8)", true);
        toBool.insert("QScriptValue(0, 0x43211234)", true);
        toBool.insert("QScriptValue(0, 0x10000)", true);
        toBool.insert("QScriptValue(0, 0x10001)", true);
        toBool.insert("QScriptValue(0, qSNaN())", false);
        toBool.insert("QScriptValue(0, qQNaN())", false);
        toBool.insert("QScriptValue(0, qInf())", true);
        toBool.insert("QScriptValue(0, -qInf())", true);
        toBool.insert("QScriptValue(0, \"NaN\")", true);
        toBool.insert("QScriptValue(0, \"Infinity\")", true);
        toBool.insert("QScriptValue(0, \"-Infinity\")", true);
        toBool.insert("QScriptValue(0, \"ciao\")", true);
        toBool.insert("QScriptValue(0, QString::fromLatin1(\"ciao\"))", true);
        toBool.insert("QScriptValue(0, QString(\"\"))", false);
        toBool.insert("QScriptValue(0, QString())", false);
        toBool.insert("QScriptValue(0, QString(\"0\"))", true);
        toBool.insert("QScriptValue(0, QString(\"123\"))", true);
        toBool.insert("QScriptValue(0, QString(\"12.3\"))", true);
        toBool.insert("QScriptValue(engine, QScriptValue::UndefinedValue)", false);
        toBool.insert("QScriptValue(engine, QScriptValue::NullValue)", false);
        toBool.insert("QScriptValue(engine, true)", true);
        toBool.insert("QScriptValue(engine, false)", false);
        toBool.insert("QScriptValue(engine, int(122))", true);
        toBool.insert("QScriptValue(engine, uint(124))", true);
        toBool.insert("QScriptValue(engine, 0)", false);
        toBool.insert("QScriptValue(engine, 0.0)", false);
        toBool.insert("QScriptValue(engine, 123.0)", true);
        toBool.insert("QScriptValue(engine, 6.37e-8)", true);
        toBool.insert("QScriptValue(engine, -6.37e-8)", true);
        toBool.insert("QScriptValue(engine, 0x43211234)", true);
        toBool.insert("QScriptValue(engine, 0x10000)", true);
        toBool.insert("QScriptValue(engine, 0x10001)", true);
        toBool.insert("QScriptValue(engine, qSNaN())", false);
        toBool.insert("QScriptValue(engine, qQNaN())", false);
        toBool.insert("QScriptValue(engine, qInf())", true);
        toBool.insert("QScriptValue(engine, -qInf())", true);
        toBool.insert("QScriptValue(engine, \"NaN\")", true);
        toBool.insert("QScriptValue(engine, \"Infinity\")", true);
        toBool.insert("QScriptValue(engine, \"-Infinity\")", true);
        toBool.insert("QScriptValue(engine, \"ciao\")", true);
        toBool.insert("QScriptValue(engine, QString::fromLatin1(\"ciao\"))", true);
        toBool.insert("QScriptValue(engine, QString(\"\"))", false);
        toBool.insert("QScriptValue(engine, QString())", false);
        toBool.insert("QScriptValue(engine, QString(\"0\"))", true);
        toBool.insert("QScriptValue(engine, QString(\"123\"))", true);
        toBool.insert("QScriptValue(engine, QString(\"1.23\"))", true);
        toBool.insert("engine->evaluate(\"[]\")", true);
        toBool.insert("engine->evaluate(\"{}\")", false);
        toBool.insert("engine->evaluate(\"Object.prototype\")", true);
        toBool.insert("engine->evaluate(\"Date.prototype\")", true);
        toBool.insert("engine->evaluate(\"Array.prototype\")", true);
        toBool.insert("engine->evaluate(\"Function.prototype\")", true);
        toBool.insert("engine->evaluate(\"Error.prototype\")", true);
        toBool.insert("engine->evaluate(\"Object\")", true);
        toBool.insert("engine->evaluate(\"Array\")", true);
        toBool.insert("engine->evaluate(\"Number\")", true);
        toBool.insert("engine->evaluate(\"Function\")", true);
        toBool.insert("engine->evaluate(\"(function() { return 1; })\")", true);
        toBool.insert("engine->evaluate(\"(function() { return 'ciao'; })\")", true);
        toBool.insert("engine->evaluate(\"(function() { throw new Error('foo'); })\")", true);
        toBool.insert("engine->evaluate(\"/foo/\")", true);
        toBool.insert("engine->evaluate(\"new Object()\")", true);
        toBool.insert("engine->evaluate(\"new Array()\")", true);
        toBool.insert("engine->evaluate(\"new Error()\")", true);
    }
    newRow(expr) << toBool.value(expr);
}

void tst_QScriptValue::toBool_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.toBool(), expected);
}

DEFINE_TEST_FUNCTION(toBool)


void tst_QScriptValue::toBoolean_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

void tst_QScriptValue::toBoolean_makeData(const char* expr)
{
    static QHash<QString, bool> toBoolean;
    if (toBoolean.isEmpty()) {
        toBoolean.insert("QScriptValue()", false);
        toBoolean.insert("QScriptValue(QScriptValue::UndefinedValue)", false);
        toBoolean.insert("QScriptValue(QScriptValue::NullValue)", false);
        toBoolean.insert("QScriptValue(true)", true);
        toBoolean.insert("QScriptValue(false)", false);
        toBoolean.insert("QScriptValue(int(122))", true);
        toBoolean.insert("QScriptValue(uint(124))", true);
        toBoolean.insert("QScriptValue(0)", false);
        toBoolean.insert("QScriptValue(0.0)", false);
        toBoolean.insert("QScriptValue(123.0)", true);
        toBoolean.insert("QScriptValue(6.37e-8)", true);
        toBoolean.insert("QScriptValue(-6.37e-8)", true);
        toBoolean.insert("QScriptValue(0x43211234)", true);
        toBoolean.insert("QScriptValue(0x10000)", true);
        toBoolean.insert("QScriptValue(0x10001)", true);
        toBoolean.insert("QScriptValue(qSNaN())", false);
        toBoolean.insert("QScriptValue(qQNaN())", false);
        toBoolean.insert("QScriptValue(qInf())", true);
        toBoolean.insert("QScriptValue(-qInf())", true);
        toBoolean.insert("QScriptValue(\"NaN\")", true);
        toBoolean.insert("QScriptValue(\"Infinity\")", true);
        toBoolean.insert("QScriptValue(\"-Infinity\")", true);
        toBoolean.insert("QScriptValue(\"ciao\")", true);
        toBoolean.insert("QScriptValue(QString::fromLatin1(\"ciao\"))", true);
        toBoolean.insert("QScriptValue(QString(\"\"))", false);
        toBoolean.insert("QScriptValue(QString())", false);
        toBoolean.insert("QScriptValue(QString(\"0\"))", true);
        toBoolean.insert("QScriptValue(QString(\"123\"))", true);
        toBoolean.insert("QScriptValue(QString(\"12.4\"))", true);
        toBoolean.insert("QScriptValue(0, QScriptValue::UndefinedValue)", false);
        toBoolean.insert("QScriptValue(0, QScriptValue::NullValue)", false);
        toBoolean.insert("QScriptValue(0, true)", true);
        toBoolean.insert("QScriptValue(0, false)", false);
        toBoolean.insert("QScriptValue(0, int(122))", true);
        toBoolean.insert("QScriptValue(0, uint(124))", true);
        toBoolean.insert("QScriptValue(0, 0)", false);
        toBoolean.insert("QScriptValue(0, 0.0)", false);
        toBoolean.insert("QScriptValue(0, 123.0)", true);
        toBoolean.insert("QScriptValue(0, 6.37e-8)", true);
        toBoolean.insert("QScriptValue(0, -6.37e-8)", true);
        toBoolean.insert("QScriptValue(0, 0x43211234)", true);
        toBoolean.insert("QScriptValue(0, 0x10000)", true);
        toBoolean.insert("QScriptValue(0, 0x10001)", true);
        toBoolean.insert("QScriptValue(0, qSNaN())", false);
        toBoolean.insert("QScriptValue(0, qQNaN())", false);
        toBoolean.insert("QScriptValue(0, qInf())", true);
        toBoolean.insert("QScriptValue(0, -qInf())", true);
        toBoolean.insert("QScriptValue(0, \"NaN\")", true);
        toBoolean.insert("QScriptValue(0, \"Infinity\")", true);
        toBoolean.insert("QScriptValue(0, \"-Infinity\")", true);
        toBoolean.insert("QScriptValue(0, \"ciao\")", true);
        toBoolean.insert("QScriptValue(0, QString::fromLatin1(\"ciao\"))", true);
        toBoolean.insert("QScriptValue(0, QString(\"\"))", false);
        toBoolean.insert("QScriptValue(0, QString())", false);
        toBoolean.insert("QScriptValue(0, QString(\"0\"))", true);
        toBoolean.insert("QScriptValue(0, QString(\"123\"))", true);
        toBoolean.insert("QScriptValue(0, QString(\"12.3\"))", true);
        toBoolean.insert("QScriptValue(engine, QScriptValue::UndefinedValue)", false);
        toBoolean.insert("QScriptValue(engine, QScriptValue::NullValue)", false);
        toBoolean.insert("QScriptValue(engine, true)", true);
        toBoolean.insert("QScriptValue(engine, false)", false);
        toBoolean.insert("QScriptValue(engine, int(122))", true);
        toBoolean.insert("QScriptValue(engine, uint(124))", true);
        toBoolean.insert("QScriptValue(engine, 0)", false);
        toBoolean.insert("QScriptValue(engine, 0.0)", false);
        toBoolean.insert("QScriptValue(engine, 123.0)", true);
        toBoolean.insert("QScriptValue(engine, 6.37e-8)", true);
        toBoolean.insert("QScriptValue(engine, -6.37e-8)", true);
        toBoolean.insert("QScriptValue(engine, 0x43211234)", true);
        toBoolean.insert("QScriptValue(engine, 0x10000)", true);
        toBoolean.insert("QScriptValue(engine, 0x10001)", true);
        toBoolean.insert("QScriptValue(engine, qSNaN())", false);
        toBoolean.insert("QScriptValue(engine, qQNaN())", false);
        toBoolean.insert("QScriptValue(engine, qInf())", true);
        toBoolean.insert("QScriptValue(engine, -qInf())", true);
        toBoolean.insert("QScriptValue(engine, \"NaN\")", true);
        toBoolean.insert("QScriptValue(engine, \"Infinity\")", true);
        toBoolean.insert("QScriptValue(engine, \"-Infinity\")", true);
        toBoolean.insert("QScriptValue(engine, \"ciao\")", true);
        toBoolean.insert("QScriptValue(engine, QString::fromLatin1(\"ciao\"))", true);
        toBoolean.insert("QScriptValue(engine, QString(\"\"))", false);
        toBoolean.insert("QScriptValue(engine, QString())", false);
        toBoolean.insert("QScriptValue(engine, QString(\"0\"))", true);
        toBoolean.insert("QScriptValue(engine, QString(\"123\"))", true);
        toBoolean.insert("QScriptValue(engine, QString(\"1.23\"))", true);
        toBoolean.insert("engine->evaluate(\"[]\")", true);
        toBoolean.insert("engine->evaluate(\"{}\")", false);
        toBoolean.insert("engine->evaluate(\"Object.prototype\")", true);
        toBoolean.insert("engine->evaluate(\"Date.prototype\")", true);
        toBoolean.insert("engine->evaluate(\"Array.prototype\")", true);
        toBoolean.insert("engine->evaluate(\"Function.prototype\")", true);
        toBoolean.insert("engine->evaluate(\"Error.prototype\")", true);
        toBoolean.insert("engine->evaluate(\"Object\")", true);
        toBoolean.insert("engine->evaluate(\"Array\")", true);
        toBoolean.insert("engine->evaluate(\"Number\")", true);
        toBoolean.insert("engine->evaluate(\"Function\")", true);
        toBoolean.insert("engine->evaluate(\"(function() { return 1; })\")", true);
        toBoolean.insert("engine->evaluate(\"(function() { return 'ciao'; })\")", true);
        toBoolean.insert("engine->evaluate(\"(function() { throw new Error('foo'); })\")", true);
        toBoolean.insert("engine->evaluate(\"/foo/\")", true);
        toBoolean.insert("engine->evaluate(\"new Object()\")", true);
        toBoolean.insert("engine->evaluate(\"new Array()\")", true);
        toBoolean.insert("engine->evaluate(\"new Error()\")", true);
    }
    newRow(expr) << toBoolean.value(expr);
}

void tst_QScriptValue::toBoolean_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.toBoolean(), expected);
}

DEFINE_TEST_FUNCTION(toBoolean)


void tst_QScriptValue::toInteger_initData()
{
    QTest::addColumn<qsreal>("expected");
    initScriptValues();
}

void tst_QScriptValue::toInteger_makeData(const char* expr)
{
    static QHash<QString, qsreal> toInteger;
    if (toInteger.isEmpty()) {
        toInteger.insert("QScriptValue()", 0);
        toInteger.insert("QScriptValue(QScriptValue::UndefinedValue)", 0);
        toInteger.insert("QScriptValue(QScriptValue::NullValue)", 0);
        toInteger.insert("QScriptValue(true)", 1);
        toInteger.insert("QScriptValue(false)", 0);
        toInteger.insert("QScriptValue(int(122))", 122);
        toInteger.insert("QScriptValue(uint(124))", 124);
        toInteger.insert("QScriptValue(0)", 0);
        toInteger.insert("QScriptValue(0.0)", 0);
        toInteger.insert("QScriptValue(123.0)", 123);
        toInteger.insert("QScriptValue(6.37e-8)", 0);
        toInteger.insert("QScriptValue(-6.37e-8)", 0);
        toInteger.insert("QScriptValue(0x43211234)", 1126240820);
        toInteger.insert("QScriptValue(0x10000)", 65536);
        toInteger.insert("QScriptValue(0x10001)", 65537);
        toInteger.insert("QScriptValue(qSNaN())", 0);
        toInteger.insert("QScriptValue(qQNaN())", 0);
        toInteger.insert("QScriptValue(qInf())", qInf());
        toInteger.insert("QScriptValue(-qInf())", qInf());
        toInteger.insert("QScriptValue(\"NaN\")", 0);
        toInteger.insert("QScriptValue(\"Infinity\")", qInf());
        toInteger.insert("QScriptValue(\"-Infinity\")", qInf());
        toInteger.insert("QScriptValue(\"ciao\")", 0);
        toInteger.insert("QScriptValue(QString::fromLatin1(\"ciao\"))", 0);
        toInteger.insert("QScriptValue(QString(\"\"))", 0);
        toInteger.insert("QScriptValue(QString())", 0);
        toInteger.insert("QScriptValue(QString(\"0\"))", 0);
        toInteger.insert("QScriptValue(QString(\"123\"))", 123);
        toInteger.insert("QScriptValue(QString(\"12.4\"))", 12);
        toInteger.insert("QScriptValue(0, QScriptValue::UndefinedValue)", 0);
        toInteger.insert("QScriptValue(0, QScriptValue::NullValue)", 0);
        toInteger.insert("QScriptValue(0, true)", 1);
        toInteger.insert("QScriptValue(0, false)", 0);
        toInteger.insert("QScriptValue(0, int(122))", 122);
        toInteger.insert("QScriptValue(0, uint(124))", 124);
        toInteger.insert("QScriptValue(0, 0)", 0);
        toInteger.insert("QScriptValue(0, 0.0)", 0);
        toInteger.insert("QScriptValue(0, 123.0)", 123);
        toInteger.insert("QScriptValue(0, 6.37e-8)", 0);
        toInteger.insert("QScriptValue(0, -6.37e-8)", 0);
        toInteger.insert("QScriptValue(0, 0x43211234)", 1126240820);
        toInteger.insert("QScriptValue(0, 0x10000)", 65536);
        toInteger.insert("QScriptValue(0, 0x10001)", 65537);
        toInteger.insert("QScriptValue(0, qSNaN())", 0);
        toInteger.insert("QScriptValue(0, qQNaN())", 0);
        toInteger.insert("QScriptValue(0, qInf())", qInf());
        toInteger.insert("QScriptValue(0, -qInf())", qInf());
        toInteger.insert("QScriptValue(0, \"NaN\")", 0);
        toInteger.insert("QScriptValue(0, \"Infinity\")", qInf());
        toInteger.insert("QScriptValue(0, \"-Infinity\")", qInf());
        toInteger.insert("QScriptValue(0, \"ciao\")", 0);
        toInteger.insert("QScriptValue(0, QString::fromLatin1(\"ciao\"))", 0);
        toInteger.insert("QScriptValue(0, QString(\"\"))", 0);
        toInteger.insert("QScriptValue(0, QString())", 0);
        toInteger.insert("QScriptValue(0, QString(\"0\"))", 0);
        toInteger.insert("QScriptValue(0, QString(\"123\"))", 123);
        toInteger.insert("QScriptValue(0, QString(\"12.3\"))", 12);
        toInteger.insert("QScriptValue(engine, QScriptValue::UndefinedValue)", 0);
        toInteger.insert("QScriptValue(engine, QScriptValue::NullValue)", 0);
        toInteger.insert("QScriptValue(engine, true)", 1);
        toInteger.insert("QScriptValue(engine, false)", 0);
        toInteger.insert("QScriptValue(engine, int(122))", 122);
        toInteger.insert("QScriptValue(engine, uint(124))", 124);
        toInteger.insert("QScriptValue(engine, 0)", 0);
        toInteger.insert("QScriptValue(engine, 0.0)", 0);
        toInteger.insert("QScriptValue(engine, 123.0)", 123);
        toInteger.insert("QScriptValue(engine, 6.37e-8)", 0);
        toInteger.insert("QScriptValue(engine, -6.37e-8)", 0);
        toInteger.insert("QScriptValue(engine, 0x43211234)", 1126240820);
        toInteger.insert("QScriptValue(engine, 0x10000)", 65536);
        toInteger.insert("QScriptValue(engine, 0x10001)", 65537);
        toInteger.insert("QScriptValue(engine, qSNaN())", 0);
        toInteger.insert("QScriptValue(engine, qQNaN())", 0);
        toInteger.insert("QScriptValue(engine, qInf())", qInf());
        toInteger.insert("QScriptValue(engine, -qInf())", qInf());
        toInteger.insert("QScriptValue(engine, \"NaN\")", 0);
        toInteger.insert("QScriptValue(engine, \"Infinity\")", qInf());
        toInteger.insert("QScriptValue(engine, \"-Infinity\")", qInf());
        toInteger.insert("QScriptValue(engine, \"ciao\")", 0);
        toInteger.insert("QScriptValue(engine, QString::fromLatin1(\"ciao\"))", 0);
        toInteger.insert("QScriptValue(engine, QString(\"\"))", 0);
        toInteger.insert("QScriptValue(engine, QString())", 0);
        toInteger.insert("QScriptValue(engine, QString(\"0\"))", 0);
        toInteger.insert("QScriptValue(engine, QString(\"123\"))", 123);
        toInteger.insert("QScriptValue(engine, QString(\"1.23\"))", 1);
        toInteger.insert("engine->evaluate(\"[]\")", 0);
        toInteger.insert("engine->evaluate(\"{}\")", 0);
        toInteger.insert("engine->evaluate(\"Object.prototype\")", 0);
        toInteger.insert("engine->evaluate(\"Date.prototype\")", 0);
        toInteger.insert("engine->evaluate(\"Array.prototype\")", 0);
        toInteger.insert("engine->evaluate(\"Function.prototype\")", 0);
        toInteger.insert("engine->evaluate(\"Error.prototype\")", 0);
        toInteger.insert("engine->evaluate(\"Object\")", 0);
        toInteger.insert("engine->evaluate(\"Array\")", 0);
        toInteger.insert("engine->evaluate(\"Number\")", 0);
        toInteger.insert("engine->evaluate(\"Function\")", 0);
        toInteger.insert("engine->evaluate(\"(function() { return 1; })\")", 0);
        toInteger.insert("engine->evaluate(\"(function() { return 'ciao'; })\")", 0);
        toInteger.insert("engine->evaluate(\"(function() { throw new Error('foo'); })\")", 0);
        toInteger.insert("engine->evaluate(\"/foo/\")", 0);
        toInteger.insert("engine->evaluate(\"new Object()\")", 0);
        toInteger.insert("engine->evaluate(\"new Array()\")", 0);
        toInteger.insert("engine->evaluate(\"new Error()\")", 0);
    }
    newRow(expr) << toInteger.value(expr);
}

void tst_QScriptValue::toInteger_test(const char*, const QScriptValue& value)
{
    QFETCH(qsreal, expected);
    if (qIsInf(expected)) {
        QVERIFY(qIsInf(value.toInteger()));
        return;
    }
    QCOMPARE(value.toInteger(), expected);
}

DEFINE_TEST_FUNCTION(toInteger)


void tst_QScriptValue::toInt32_initData()
{
    QTest::addColumn<qint32>("expected");
    initScriptValues();
}

void tst_QScriptValue::toInt32_makeData(const char* expr)
{
    static QHash<QString, qint32> toInt32;
    if (toInt32.isEmpty()) {
        toInt32.insert("QScriptValue()", 0);
        toInt32.insert("QScriptValue(QScriptValue::UndefinedValue)", 0);
        toInt32.insert("QScriptValue(QScriptValue::NullValue)", 0);
        toInt32.insert("QScriptValue(true)", 1);
        toInt32.insert("QScriptValue(false)", 0);
        toInt32.insert("QScriptValue(int(122))", 122);
        toInt32.insert("QScriptValue(uint(124))", 124);
        toInt32.insert("QScriptValue(0)", 0);
        toInt32.insert("QScriptValue(0.0)", 0);
        toInt32.insert("QScriptValue(123.0)", 123);
        toInt32.insert("QScriptValue(6.37e-8)", 0);
        toInt32.insert("QScriptValue(-6.37e-8)", 0);
        toInt32.insert("QScriptValue(0x43211234)", 1126240820);
        toInt32.insert("QScriptValue(0x10000)", 65536);
        toInt32.insert("QScriptValue(0x10001)", 65537);
        toInt32.insert("QScriptValue(qSNaN())", 0);
        toInt32.insert("QScriptValue(qQNaN())", 0);
        toInt32.insert("QScriptValue(qInf())", 0);
        toInt32.insert("QScriptValue(-qInf())", 0);
        toInt32.insert("QScriptValue(\"NaN\")", 0);
        toInt32.insert("QScriptValue(\"Infinity\")", 0);
        toInt32.insert("QScriptValue(\"-Infinity\")", 0);
        toInt32.insert("QScriptValue(\"ciao\")", 0);
        toInt32.insert("QScriptValue(QString::fromLatin1(\"ciao\"))", 0);
        toInt32.insert("QScriptValue(QString(\"\"))", 0);
        toInt32.insert("QScriptValue(QString())", 0);
        toInt32.insert("QScriptValue(QString(\"0\"))", 0);
        toInt32.insert("QScriptValue(QString(\"123\"))", 123);
        toInt32.insert("QScriptValue(QString(\"12.4\"))", 12);
        toInt32.insert("QScriptValue(0, QScriptValue::UndefinedValue)", 0);
        toInt32.insert("QScriptValue(0, QScriptValue::NullValue)", 0);
        toInt32.insert("QScriptValue(0, true)", 1);
        toInt32.insert("QScriptValue(0, false)", 0);
        toInt32.insert("QScriptValue(0, int(122))", 122);
        toInt32.insert("QScriptValue(0, uint(124))", 124);
        toInt32.insert("QScriptValue(0, 0)", 0);
        toInt32.insert("QScriptValue(0, 0.0)", 0);
        toInt32.insert("QScriptValue(0, 123.0)", 123);
        toInt32.insert("QScriptValue(0, 6.37e-8)", 0);
        toInt32.insert("QScriptValue(0, -6.37e-8)", 0);
        toInt32.insert("QScriptValue(0, 0x43211234)", 1126240820);
        toInt32.insert("QScriptValue(0, 0x10000)", 65536);
        toInt32.insert("QScriptValue(0, 0x10001)", 65537);
        toInt32.insert("QScriptValue(0, qSNaN())", 0);
        toInt32.insert("QScriptValue(0, qQNaN())", 0);
        toInt32.insert("QScriptValue(0, qInf())", 0);
        toInt32.insert("QScriptValue(0, -qInf())", 0);
        toInt32.insert("QScriptValue(0, \"NaN\")", 0);
        toInt32.insert("QScriptValue(0, \"Infinity\")", 0);
        toInt32.insert("QScriptValue(0, \"-Infinity\")", 0);
        toInt32.insert("QScriptValue(0, \"ciao\")", 0);
        toInt32.insert("QScriptValue(0, QString::fromLatin1(\"ciao\"))", 0);
        toInt32.insert("QScriptValue(0, QString(\"\"))", 0);
        toInt32.insert("QScriptValue(0, QString())", 0);
        toInt32.insert("QScriptValue(0, QString(\"0\"))", 0);
        toInt32.insert("QScriptValue(0, QString(\"123\"))", 123);
        toInt32.insert("QScriptValue(0, QString(\"12.3\"))", 12);
        toInt32.insert("QScriptValue(engine, QScriptValue::UndefinedValue)", 0);
        toInt32.insert("QScriptValue(engine, QScriptValue::NullValue)", 0);
        toInt32.insert("QScriptValue(engine, true)", 1);
        toInt32.insert("QScriptValue(engine, false)", 0);
        toInt32.insert("QScriptValue(engine, int(122))", 122);
        toInt32.insert("QScriptValue(engine, uint(124))", 124);
        toInt32.insert("QScriptValue(engine, 0)", 0);
        toInt32.insert("QScriptValue(engine, 0.0)", 0);
        toInt32.insert("QScriptValue(engine, 123.0)", 123);
        toInt32.insert("QScriptValue(engine, 6.37e-8)", 0);
        toInt32.insert("QScriptValue(engine, -6.37e-8)", 0);
        toInt32.insert("QScriptValue(engine, 0x43211234)", 1126240820);
        toInt32.insert("QScriptValue(engine, 0x10000)", 65536);
        toInt32.insert("QScriptValue(engine, 0x10001)", 65537);
        toInt32.insert("QScriptValue(engine, qSNaN())", 0);
        toInt32.insert("QScriptValue(engine, qQNaN())", 0);
        toInt32.insert("QScriptValue(engine, qInf())", 0);
        toInt32.insert("QScriptValue(engine, -qInf())", 0);
        toInt32.insert("QScriptValue(engine, \"NaN\")", 0);
        toInt32.insert("QScriptValue(engine, \"Infinity\")", 0);
        toInt32.insert("QScriptValue(engine, \"-Infinity\")", 0);
        toInt32.insert("QScriptValue(engine, \"ciao\")", 0);
        toInt32.insert("QScriptValue(engine, QString::fromLatin1(\"ciao\"))", 0);
        toInt32.insert("QScriptValue(engine, QString(\"\"))", 0);
        toInt32.insert("QScriptValue(engine, QString())", 0);
        toInt32.insert("QScriptValue(engine, QString(\"0\"))", 0);
        toInt32.insert("QScriptValue(engine, QString(\"123\"))", 123);
        toInt32.insert("QScriptValue(engine, QString(\"1.23\"))", 1);
        toInt32.insert("engine->evaluate(\"[]\")", 0);
        toInt32.insert("engine->evaluate(\"{}\")", 0);
        toInt32.insert("engine->evaluate(\"Object.prototype\")", 0);
        toInt32.insert("engine->evaluate(\"Date.prototype\")", 0);
        toInt32.insert("engine->evaluate(\"Array.prototype\")", 0);
        toInt32.insert("engine->evaluate(\"Function.prototype\")", 0);
        toInt32.insert("engine->evaluate(\"Error.prototype\")", 0);
        toInt32.insert("engine->evaluate(\"Object\")", 0);
        toInt32.insert("engine->evaluate(\"Array\")", 0);
        toInt32.insert("engine->evaluate(\"Number\")", 0);
        toInt32.insert("engine->evaluate(\"Function\")", 0);
        toInt32.insert("engine->evaluate(\"(function() { return 1; })\")", 0);
        toInt32.insert("engine->evaluate(\"(function() { return 'ciao'; })\")", 0);
        toInt32.insert("engine->evaluate(\"(function() { throw new Error('foo'); })\")", 0);
        toInt32.insert("engine->evaluate(\"/foo/\")", 0);
        toInt32.insert("engine->evaluate(\"new Object()\")", 0);
        toInt32.insert("engine->evaluate(\"new Array()\")", 0);
        toInt32.insert("engine->evaluate(\"new Error()\")", 0);
    }
    newRow(expr) << toInt32.value(expr);
}

void tst_QScriptValue::toInt32_test(const char*, const QScriptValue& value)
{
    QFETCH(qint32, expected);
    QCOMPARE(value.toInt32(), expected);
}

DEFINE_TEST_FUNCTION(toInt32)


void tst_QScriptValue::toUInt32_initData()
{
    QTest::addColumn<quint32>("expected");
    initScriptValues();
}

void tst_QScriptValue::toUInt32_makeData(const char* expr)
{
    static QHash<QString, quint32> toUInt32;
    if (toUInt32.isEmpty()) {
        toUInt32.insert("QScriptValue()", 0);
        toUInt32.insert("QScriptValue(QScriptValue::UndefinedValue)", 0);
        toUInt32.insert("QScriptValue(QScriptValue::NullValue)", 0);
        toUInt32.insert("QScriptValue(true)", 1);
        toUInt32.insert("QScriptValue(false)", 0);
        toUInt32.insert("QScriptValue(int(122))", 122);
        toUInt32.insert("QScriptValue(uint(124))", 124);
        toUInt32.insert("QScriptValue(0)", 0);
        toUInt32.insert("QScriptValue(0.0)", 0);
        toUInt32.insert("QScriptValue(123.0)", 123);
        toUInt32.insert("QScriptValue(6.37e-8)", 0);
        toUInt32.insert("QScriptValue(-6.37e-8)", 0);
        toUInt32.insert("QScriptValue(0x43211234)", 1126240820);
        toUInt32.insert("QScriptValue(0x10000)", 65536);
        toUInt32.insert("QScriptValue(0x10001)", 65537);
        toUInt32.insert("QScriptValue(qSNaN())", 0);
        toUInt32.insert("QScriptValue(qQNaN())", 0);
        toUInt32.insert("QScriptValue(qInf())", 0);
        toUInt32.insert("QScriptValue(-qInf())", 0);
        toUInt32.insert("QScriptValue(\"NaN\")", 0);
        toUInt32.insert("QScriptValue(\"Infinity\")", 0);
        toUInt32.insert("QScriptValue(\"-Infinity\")", 0);
        toUInt32.insert("QScriptValue(\"ciao\")", 0);
        toUInt32.insert("QScriptValue(QString::fromLatin1(\"ciao\"))", 0);
        toUInt32.insert("QScriptValue(QString(\"\"))", 0);
        toUInt32.insert("QScriptValue(QString())", 0);
        toUInt32.insert("QScriptValue(QString(\"0\"))", 0);
        toUInt32.insert("QScriptValue(QString(\"123\"))", 123);
        toUInt32.insert("QScriptValue(QString(\"12.4\"))", 12);
        toUInt32.insert("QScriptValue(0, QScriptValue::UndefinedValue)", 0);
        toUInt32.insert("QScriptValue(0, QScriptValue::NullValue)", 0);
        toUInt32.insert("QScriptValue(0, true)", 1);
        toUInt32.insert("QScriptValue(0, false)", 0);
        toUInt32.insert("QScriptValue(0, int(122))", 122);
        toUInt32.insert("QScriptValue(0, uint(124))", 124);
        toUInt32.insert("QScriptValue(0, 0)", 0);
        toUInt32.insert("QScriptValue(0, 0.0)", 0);
        toUInt32.insert("QScriptValue(0, 123.0)", 123);
        toUInt32.insert("QScriptValue(0, 6.37e-8)", 0);
        toUInt32.insert("QScriptValue(0, -6.37e-8)", 0);
        toUInt32.insert("QScriptValue(0, 0x43211234)", 1126240820);
        toUInt32.insert("QScriptValue(0, 0x10000)", 65536);
        toUInt32.insert("QScriptValue(0, 0x10001)", 65537);
        toUInt32.insert("QScriptValue(0, qSNaN())", 0);
        toUInt32.insert("QScriptValue(0, qQNaN())", 0);
        toUInt32.insert("QScriptValue(0, qInf())", 0);
        toUInt32.insert("QScriptValue(0, -qInf())", 0);
        toUInt32.insert("QScriptValue(0, \"NaN\")", 0);
        toUInt32.insert("QScriptValue(0, \"Infinity\")", 0);
        toUInt32.insert("QScriptValue(0, \"-Infinity\")", 0);
        toUInt32.insert("QScriptValue(0, \"ciao\")", 0);
        toUInt32.insert("QScriptValue(0, QString::fromLatin1(\"ciao\"))", 0);
        toUInt32.insert("QScriptValue(0, QString(\"\"))", 0);
        toUInt32.insert("QScriptValue(0, QString())", 0);
        toUInt32.insert("QScriptValue(0, QString(\"0\"))", 0);
        toUInt32.insert("QScriptValue(0, QString(\"123\"))", 123);
        toUInt32.insert("QScriptValue(0, QString(\"12.3\"))", 12);
        toUInt32.insert("QScriptValue(engine, QScriptValue::UndefinedValue)", 0);
        toUInt32.insert("QScriptValue(engine, QScriptValue::NullValue)", 0);
        toUInt32.insert("QScriptValue(engine, true)", 1);
        toUInt32.insert("QScriptValue(engine, false)", 0);
        toUInt32.insert("QScriptValue(engine, int(122))", 122);
        toUInt32.insert("QScriptValue(engine, uint(124))", 124);
        toUInt32.insert("QScriptValue(engine, 0)", 0);
        toUInt32.insert("QScriptValue(engine, 0.0)", 0);
        toUInt32.insert("QScriptValue(engine, 123.0)", 123);
        toUInt32.insert("QScriptValue(engine, 6.37e-8)", 0);
        toUInt32.insert("QScriptValue(engine, -6.37e-8)", 0);
        toUInt32.insert("QScriptValue(engine, 0x43211234)", 1126240820);
        toUInt32.insert("QScriptValue(engine, 0x10000)", 65536);
        toUInt32.insert("QScriptValue(engine, 0x10001)", 65537);
        toUInt32.insert("QScriptValue(engine, qSNaN())", 0);
        toUInt32.insert("QScriptValue(engine, qQNaN())", 0);
        toUInt32.insert("QScriptValue(engine, qInf())", 0);
        toUInt32.insert("QScriptValue(engine, -qInf())", 0);
        toUInt32.insert("QScriptValue(engine, \"NaN\")", 0);
        toUInt32.insert("QScriptValue(engine, \"Infinity\")", 0);
        toUInt32.insert("QScriptValue(engine, \"-Infinity\")", 0);
        toUInt32.insert("QScriptValue(engine, \"ciao\")", 0);
        toUInt32.insert("QScriptValue(engine, QString::fromLatin1(\"ciao\"))", 0);
        toUInt32.insert("QScriptValue(engine, QString(\"\"))", 0);
        toUInt32.insert("QScriptValue(engine, QString())", 0);
        toUInt32.insert("QScriptValue(engine, QString(\"0\"))", 0);
        toUInt32.insert("QScriptValue(engine, QString(\"123\"))", 123);
        toUInt32.insert("QScriptValue(engine, QString(\"1.23\"))", 1);
        toUInt32.insert("engine->evaluate(\"[]\")", 0);
        toUInt32.insert("engine->evaluate(\"{}\")", 0);
        toUInt32.insert("engine->evaluate(\"Object.prototype\")", 0);
        toUInt32.insert("engine->evaluate(\"Date.prototype\")", 0);
        toUInt32.insert("engine->evaluate(\"Array.prototype\")", 0);
        toUInt32.insert("engine->evaluate(\"Function.prototype\")", 0);
        toUInt32.insert("engine->evaluate(\"Error.prototype\")", 0);
        toUInt32.insert("engine->evaluate(\"Object\")", 0);
        toUInt32.insert("engine->evaluate(\"Array\")", 0);
        toUInt32.insert("engine->evaluate(\"Number\")", 0);
        toUInt32.insert("engine->evaluate(\"Function\")", 0);
        toUInt32.insert("engine->evaluate(\"(function() { return 1; })\")", 0);
        toUInt32.insert("engine->evaluate(\"(function() { return 'ciao'; })\")", 0);
        toUInt32.insert("engine->evaluate(\"(function() { throw new Error('foo'); })\")", 0);
        toUInt32.insert("engine->evaluate(\"/foo/\")", 0);
        toUInt32.insert("engine->evaluate(\"new Object()\")", 0);
        toUInt32.insert("engine->evaluate(\"new Array()\")", 0);
        toUInt32.insert("engine->evaluate(\"new Error()\")", 0);
    }
    newRow(expr) << toUInt32.value(expr);
}

void tst_QScriptValue::toUInt32_test(const char*, const QScriptValue& value)
{
    QFETCH(quint32, expected);
    QCOMPARE(value.toUInt32(), expected);
}

DEFINE_TEST_FUNCTION(toUInt32)


void tst_QScriptValue::toUInt16_initData()
{
    QTest::addColumn<quint16>("expected");
    initScriptValues();
}

void tst_QScriptValue::toUInt16_makeData(const char* expr)
{
    static QHash<QString, quint16> toUInt16;
    if (toUInt16.isEmpty()) {
        toUInt16.insert("QScriptValue()", 0);
        toUInt16.insert("QScriptValue(QScriptValue::UndefinedValue)", 0);
        toUInt16.insert("QScriptValue(QScriptValue::NullValue)", 0);
        toUInt16.insert("QScriptValue(true)", 1);
        toUInt16.insert("QScriptValue(false)", 0);
        toUInt16.insert("QScriptValue(int(122))", 122);
        toUInt16.insert("QScriptValue(uint(124))", 124);
        toUInt16.insert("QScriptValue(0)", 0);
        toUInt16.insert("QScriptValue(0.0)", 0);
        toUInt16.insert("QScriptValue(123.0)", 123);
        toUInt16.insert("QScriptValue(6.37e-8)", 0);
        toUInt16.insert("QScriptValue(-6.37e-8)", 0);
        toUInt16.insert("QScriptValue(0x43211234)", 4660);
        toUInt16.insert("QScriptValue(0x10000)", 0);
        toUInt16.insert("QScriptValue(0x10001)", 1);
        toUInt16.insert("QScriptValue(qSNaN())", 0);
        toUInt16.insert("QScriptValue(qQNaN())", 0);
        toUInt16.insert("QScriptValue(qInf())", 0);
        toUInt16.insert("QScriptValue(-qInf())", 0);
        toUInt16.insert("QScriptValue(\"NaN\")", 0);
        toUInt16.insert("QScriptValue(\"Infinity\")", 0);
        toUInt16.insert("QScriptValue(\"-Infinity\")", 0);
        toUInt16.insert("QScriptValue(\"ciao\")", 0);
        toUInt16.insert("QScriptValue(QString::fromLatin1(\"ciao\"))", 0);
        toUInt16.insert("QScriptValue(QString(\"\"))", 0);
        toUInt16.insert("QScriptValue(QString())", 0);
        toUInt16.insert("QScriptValue(QString(\"0\"))", 0);
        toUInt16.insert("QScriptValue(QString(\"123\"))", 123);
        toUInt16.insert("QScriptValue(QString(\"12.4\"))", 12);
        toUInt16.insert("QScriptValue(0, QScriptValue::UndefinedValue)", 0);
        toUInt16.insert("QScriptValue(0, QScriptValue::NullValue)", 0);
        toUInt16.insert("QScriptValue(0, true)", 1);
        toUInt16.insert("QScriptValue(0, false)", 0);
        toUInt16.insert("QScriptValue(0, int(122))", 122);
        toUInt16.insert("QScriptValue(0, uint(124))", 124);
        toUInt16.insert("QScriptValue(0, 0)", 0);
        toUInt16.insert("QScriptValue(0, 0.0)", 0);
        toUInt16.insert("QScriptValue(0, 123.0)", 123);
        toUInt16.insert("QScriptValue(0, 6.37e-8)", 0);
        toUInt16.insert("QScriptValue(0, -6.37e-8)", 0);
        toUInt16.insert("QScriptValue(0, 0x43211234)", 4660);
        toUInt16.insert("QScriptValue(0, 0x10000)", 0);
        toUInt16.insert("QScriptValue(0, 0x10001)", 1);
        toUInt16.insert("QScriptValue(0, qSNaN())", 0);
        toUInt16.insert("QScriptValue(0, qQNaN())", 0);
        toUInt16.insert("QScriptValue(0, qInf())", 0);
        toUInt16.insert("QScriptValue(0, -qInf())", 0);
        toUInt16.insert("QScriptValue(0, \"NaN\")", 0);
        toUInt16.insert("QScriptValue(0, \"Infinity\")", 0);
        toUInt16.insert("QScriptValue(0, \"-Infinity\")", 0);
        toUInt16.insert("QScriptValue(0, \"ciao\")", 0);
        toUInt16.insert("QScriptValue(0, QString::fromLatin1(\"ciao\"))", 0);
        toUInt16.insert("QScriptValue(0, QString(\"\"))", 0);
        toUInt16.insert("QScriptValue(0, QString())", 0);
        toUInt16.insert("QScriptValue(0, QString(\"0\"))", 0);
        toUInt16.insert("QScriptValue(0, QString(\"123\"))", 123);
        toUInt16.insert("QScriptValue(0, QString(\"12.3\"))", 12);
        toUInt16.insert("QScriptValue(engine, QScriptValue::UndefinedValue)", 0);
        toUInt16.insert("QScriptValue(engine, QScriptValue::NullValue)", 0);
        toUInt16.insert("QScriptValue(engine, true)", 1);
        toUInt16.insert("QScriptValue(engine, false)", 0);
        toUInt16.insert("QScriptValue(engine, int(122))", 122);
        toUInt16.insert("QScriptValue(engine, uint(124))", 124);
        toUInt16.insert("QScriptValue(engine, 0)", 0);
        toUInt16.insert("QScriptValue(engine, 0.0)", 0);
        toUInt16.insert("QScriptValue(engine, 123.0)", 123);
        toUInt16.insert("QScriptValue(engine, 6.37e-8)", 0);
        toUInt16.insert("QScriptValue(engine, -6.37e-8)", 0);
        toUInt16.insert("QScriptValue(engine, 0x43211234)", 4660);
        toUInt16.insert("QScriptValue(engine, 0x10000)", 0);
        toUInt16.insert("QScriptValue(engine, 0x10001)", 1);
        toUInt16.insert("QScriptValue(engine, qSNaN())", 0);
        toUInt16.insert("QScriptValue(engine, qQNaN())", 0);
        toUInt16.insert("QScriptValue(engine, qInf())", 0);
        toUInt16.insert("QScriptValue(engine, -qInf())", 0);
        toUInt16.insert("QScriptValue(engine, \"NaN\")", 0);
        toUInt16.insert("QScriptValue(engine, \"Infinity\")", 0);
        toUInt16.insert("QScriptValue(engine, \"-Infinity\")", 0);
        toUInt16.insert("QScriptValue(engine, \"ciao\")", 0);
        toUInt16.insert("QScriptValue(engine, QString::fromLatin1(\"ciao\"))", 0);
        toUInt16.insert("QScriptValue(engine, QString(\"\"))", 0);
        toUInt16.insert("QScriptValue(engine, QString())", 0);
        toUInt16.insert("QScriptValue(engine, QString(\"0\"))", 0);
        toUInt16.insert("QScriptValue(engine, QString(\"123\"))", 123);
        toUInt16.insert("QScriptValue(engine, QString(\"1.23\"))", 1);
        toUInt16.insert("engine->evaluate(\"[]\")", 0);
        toUInt16.insert("engine->evaluate(\"{}\")", 0);
        toUInt16.insert("engine->evaluate(\"Object.prototype\")", 0);
        toUInt16.insert("engine->evaluate(\"Date.prototype\")", 0);
        toUInt16.insert("engine->evaluate(\"Array.prototype\")", 0);
        toUInt16.insert("engine->evaluate(\"Function.prototype\")", 0);
        toUInt16.insert("engine->evaluate(\"Error.prototype\")", 0);
        toUInt16.insert("engine->evaluate(\"Object\")", 0);
        toUInt16.insert("engine->evaluate(\"Array\")", 0);
        toUInt16.insert("engine->evaluate(\"Number\")", 0);
        toUInt16.insert("engine->evaluate(\"Function\")", 0);
        toUInt16.insert("engine->evaluate(\"(function() { return 1; })\")", 0);
        toUInt16.insert("engine->evaluate(\"(function() { return 'ciao'; })\")", 0);
        toUInt16.insert("engine->evaluate(\"(function() { throw new Error('foo'); })\")", 0);
        toUInt16.insert("engine->evaluate(\"/foo/\")", 0);
        toUInt16.insert("engine->evaluate(\"new Object()\")", 0);
        toUInt16.insert("engine->evaluate(\"new Array()\")", 0);
        toUInt16.insert("engine->evaluate(\"new Error()\")", 0);
    }
    newRow(expr) << toUInt16.value(expr);
}

void tst_QScriptValue::toUInt16_test(const char*, const QScriptValue& value)
{
    QFETCH(quint16, expected);
    QCOMPARE(value.toUInt16(), expected);
}

DEFINE_TEST_FUNCTION(toUInt16)
