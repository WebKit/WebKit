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


/****************************************************************************
*************** This file has been generated. DO NOT MODIFY! ****************
****************************************************************************/

#include "tst_qscriptvalue.h"


void tst_QScriptValue::isValid_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static QString isValid_array[] = {
    "QScriptValue(QScriptValue::UndefinedValue)",
    "QScriptValue(QScriptValue::NullValue)",
    "QScriptValue(true)",
    "QScriptValue(false)",
    "QScriptValue(int(122))",
    "QScriptValue(uint(124))",
    "QScriptValue(0)",
    "QScriptValue(0.0)",
    "QScriptValue(123.0)",
    "QScriptValue(6.37e-8)",
    "QScriptValue(-6.37e-8)",
    "QScriptValue(0x43211234)",
    "QScriptValue(0x10000)",
    "QScriptValue(0x10001)",
    "QScriptValue(qSNaN())",
    "QScriptValue(qQNaN())",
    "QScriptValue(qInf())",
    "QScriptValue(-qInf())",
    "QScriptValue(\"NaN\")",
    "QScriptValue(\"Infinity\")",
    "QScriptValue(\"-Infinity\")",
    "QScriptValue(\"ciao\")",
    "QScriptValue(QString::fromLatin1(\"ciao\"))",
    "QScriptValue(QString(\"\"))",
    "QScriptValue(QString())",
    "QScriptValue(QString(\"0\"))",
    "QScriptValue(QString(\"123\"))",
    "QScriptValue(QString(\"12.4\"))",
    "QScriptValue(0, QScriptValue::UndefinedValue)",
    "QScriptValue(0, QScriptValue::NullValue)",
    "QScriptValue(0, true)",
    "QScriptValue(0, false)",
    "QScriptValue(0, int(122))",
    "QScriptValue(0, uint(124))",
    "QScriptValue(0, 0)",
    "QScriptValue(0, 0.0)",
    "QScriptValue(0, 123.0)",
    "QScriptValue(0, 6.37e-8)",
    "QScriptValue(0, -6.37e-8)",
    "QScriptValue(0, 0x43211234)",
    "QScriptValue(0, 0x10000)",
    "QScriptValue(0, 0x10001)",
    "QScriptValue(0, qSNaN())",
    "QScriptValue(0, qQNaN())",
    "QScriptValue(0, qInf())",
    "QScriptValue(0, -qInf())",
    "QScriptValue(0, \"NaN\")",
    "QScriptValue(0, \"Infinity\")",
    "QScriptValue(0, \"-Infinity\")",
    "QScriptValue(0, \"ciao\")",
    "QScriptValue(0, QString::fromLatin1(\"ciao\"))",
    "QScriptValue(0, QString(\"\"))",
    "QScriptValue(0, QString())",
    "QScriptValue(0, QString(\"0\"))",
    "QScriptValue(0, QString(\"123\"))",
    "QScriptValue(0, QString(\"12.3\"))",
    "QScriptValue(engine, QScriptValue::UndefinedValue)",
    "QScriptValue(engine, QScriptValue::NullValue)",
    "QScriptValue(engine, true)",
    "QScriptValue(engine, false)",
    "QScriptValue(engine, int(122))",
    "QScriptValue(engine, uint(124))",
    "QScriptValue(engine, 0)",
    "QScriptValue(engine, 0.0)",
    "QScriptValue(engine, 123.0)",
    "QScriptValue(engine, 6.37e-8)",
    "QScriptValue(engine, -6.37e-8)",
    "QScriptValue(engine, 0x43211234)",
    "QScriptValue(engine, 0x10000)",
    "QScriptValue(engine, 0x10001)",
    "QScriptValue(engine, qSNaN())",
    "QScriptValue(engine, qQNaN())",
    "QScriptValue(engine, qInf())",
    "QScriptValue(engine, -qInf())",
    "QScriptValue(engine, \"NaN\")",
    "QScriptValue(engine, \"Infinity\")",
    "QScriptValue(engine, \"-Infinity\")",
    "QScriptValue(engine, \"ciao\")",
    "QScriptValue(engine, QString::fromLatin1(\"ciao\"))",
    "QScriptValue(engine, QString(\"\"))",
    "QScriptValue(engine, QString())",
    "QScriptValue(engine, QString(\"0\"))",
    "QScriptValue(engine, QString(\"123\"))",
    "QScriptValue(engine, QString(\"1.23\"))",
    "engine->evaluate(\"[]\")",
    "engine->evaluate(\"{}\")",
    "engine->evaluate(\"Object.prototype\")",
    "engine->evaluate(\"Date.prototype\")",
    "engine->evaluate(\"Array.prototype\")",
    "engine->evaluate(\"Function.prototype\")",
    "engine->evaluate(\"Error.prototype\")",
    "engine->evaluate(\"Object\")",
    "engine->evaluate(\"Array\")",
    "engine->evaluate(\"Number\")",
    "engine->evaluate(\"Function\")",
    "engine->evaluate(\"(function() { return 1; })\")",
    "engine->evaluate(\"(function() { return 'ciao'; })\")",
    "engine->evaluate(\"(function() { throw new Error('foo' })\")",
    "engine->evaluate(\"/foo/\")",
    "engine->evaluate(\"new Object()\")",
    "engine->evaluate(\"new Array()\")",
    "engine->evaluate(\"new Error()\")",
    "engine->evaluate(\"a = new Object( a.foo = 22; a.foo\")",
    "engine->evaluate(\"Undefined\")",
    "engine->evaluate(\"Null\")",
    "engine->evaluate(\"True\")",
    "engine->evaluate(\"False\")",
    "engine->evaluate(\"undefined\")",
    "engine->evaluate(\"null\")",
    "engine->evaluate(\"true\")",
    "engine->evaluate(\"false\")",
    "engine->evaluate(\"122\")",
    "engine->evaluate(\"124\")",
    "engine->evaluate(\"0\")",
    "engine->evaluate(\"0.0\")",
    "engine->evaluate(\"123.0\")",
    "engine->evaluate(\"6.37e-8\")",
    "engine->evaluate(\"-6.37e-8\")",
    "engine->evaluate(\"0x43211234\")",
    "engine->evaluate(\"0x10000\")",
    "engine->evaluate(\"0x10001\")",
    "engine->evaluate(\"NaN\")",
    "engine->evaluate(\"Infinity\")",
    "engine->evaluate(\"-Infinity\")",
    "engine->evaluate(\"'ciao'\")",
    "engine->evaluate(\"''\")",
    "engine->evaluate(\"'0'\")",
    "engine->evaluate(\"'123'\")",
    "engine->evaluate(\"'12.4'\")",
    "engine->nullValue()",
    "engine->undefinedValue()"};

void tst_QScriptValue::isValid_makeData(const char* expr)
{
    static QSet<QString> isValid;
    if (isValid.isEmpty()) {
        isValid.reserve(131);
        for (unsigned i = 0; i < 131; ++i)
            isValid.insert(isValid_array[i]);
    }
    newRow(expr) << isValid.contains(expr);
}

void tst_QScriptValue::isValid_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isValid(), expected);
    QCOMPARE(value.isValid(), expected);
}

DEFINE_TEST_FUNCTION(isValid)


void tst_QScriptValue::isBool_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static QString isBool_array[] = {
    "QScriptValue(true)",
    "QScriptValue(false)",
    "QScriptValue(0, true)",
    "QScriptValue(0, false)",
    "QScriptValue(engine, true)",
    "QScriptValue(engine, false)",
    "engine->evaluate(\"true\")",
    "engine->evaluate(\"false\")"};

void tst_QScriptValue::isBool_makeData(const char* expr)
{
    static QSet<QString> isBool;
    if (isBool.isEmpty()) {
        isBool.reserve(8);
        for (unsigned i = 0; i < 8; ++i)
            isBool.insert(isBool_array[i]);
    }
    newRow(expr) << isBool.contains(expr);
}

void tst_QScriptValue::isBool_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isBool(), expected);
    QCOMPARE(value.isBool(), expected);
}

DEFINE_TEST_FUNCTION(isBool)


void tst_QScriptValue::isBoolean_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static QString isBoolean_array[] = {
    "QScriptValue(true)",
    "QScriptValue(false)",
    "QScriptValue(0, true)",
    "QScriptValue(0, false)",
    "QScriptValue(engine, true)",
    "QScriptValue(engine, false)",
    "engine->evaluate(\"true\")",
    "engine->evaluate(\"false\")"};

void tst_QScriptValue::isBoolean_makeData(const char* expr)
{
    static QSet<QString> isBoolean;
    if (isBoolean.isEmpty()) {
        isBoolean.reserve(8);
        for (unsigned i = 0; i < 8; ++i)
            isBoolean.insert(isBoolean_array[i]);
    }
    newRow(expr) << isBoolean.contains(expr);
}

void tst_QScriptValue::isBoolean_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isBoolean(), expected);
    QCOMPARE(value.isBoolean(), expected);
}

DEFINE_TEST_FUNCTION(isBoolean)


void tst_QScriptValue::isNumber_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static QString isNumber_array[] = {
    "QScriptValue(int(122))",
    "QScriptValue(uint(124))",
    "QScriptValue(0)",
    "QScriptValue(0.0)",
    "QScriptValue(123.0)",
    "QScriptValue(6.37e-8)",
    "QScriptValue(-6.37e-8)",
    "QScriptValue(0x43211234)",
    "QScriptValue(0x10000)",
    "QScriptValue(0x10001)",
    "QScriptValue(qSNaN())",
    "QScriptValue(qQNaN())",
    "QScriptValue(qInf())",
    "QScriptValue(-qInf())",
    "QScriptValue(0, int(122))",
    "QScriptValue(0, uint(124))",
    "QScriptValue(0, 0)",
    "QScriptValue(0, 0.0)",
    "QScriptValue(0, 123.0)",
    "QScriptValue(0, 6.37e-8)",
    "QScriptValue(0, -6.37e-8)",
    "QScriptValue(0, 0x43211234)",
    "QScriptValue(0, 0x10000)",
    "QScriptValue(0, 0x10001)",
    "QScriptValue(0, qSNaN())",
    "QScriptValue(0, qQNaN())",
    "QScriptValue(0, qInf())",
    "QScriptValue(0, -qInf())",
    "QScriptValue(engine, int(122))",
    "QScriptValue(engine, uint(124))",
    "QScriptValue(engine, 0)",
    "QScriptValue(engine, 0.0)",
    "QScriptValue(engine, 123.0)",
    "QScriptValue(engine, 6.37e-8)",
    "QScriptValue(engine, -6.37e-8)",
    "QScriptValue(engine, 0x43211234)",
    "QScriptValue(engine, 0x10000)",
    "QScriptValue(engine, 0x10001)",
    "QScriptValue(engine, qSNaN())",
    "QScriptValue(engine, qQNaN())",
    "QScriptValue(engine, qInf())",
    "QScriptValue(engine, -qInf())",
    "engine->evaluate(\"122\")",
    "engine->evaluate(\"124\")",
    "engine->evaluate(\"0\")",
    "engine->evaluate(\"0.0\")",
    "engine->evaluate(\"123.0\")",
    "engine->evaluate(\"6.37e-8\")",
    "engine->evaluate(\"-6.37e-8\")",
    "engine->evaluate(\"0x43211234\")",
    "engine->evaluate(\"0x10000\")",
    "engine->evaluate(\"0x10001\")",
    "engine->evaluate(\"NaN\")",
    "engine->evaluate(\"Infinity\")",
    "engine->evaluate(\"-Infinity\")"};

void tst_QScriptValue::isNumber_makeData(const char* expr)
{
    static QSet<QString> isNumber;
    if (isNumber.isEmpty()) {
        isNumber.reserve(55);
        for (unsigned i = 0; i < 55; ++i)
            isNumber.insert(isNumber_array[i]);
    }
    newRow(expr) << isNumber.contains(expr);
}

void tst_QScriptValue::isNumber_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isNumber(), expected);
    QCOMPARE(value.isNumber(), expected);
}

DEFINE_TEST_FUNCTION(isNumber)


void tst_QScriptValue::isFunction_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static QString isFunction_array[] = {
    "engine->evaluate(\"Function.prototype\")",
    "engine->evaluate(\"Object\")",
    "engine->evaluate(\"Array\")",
    "engine->evaluate(\"Number\")",
    "engine->evaluate(\"Function\")",
    "engine->evaluate(\"(function() { return 1; })\")",
    "engine->evaluate(\"(function() { return 'ciao'; })\")",
    "engine->evaluate(\"/foo/\")"};

void tst_QScriptValue::isFunction_makeData(const char* expr)
{
    static QSet<QString> isFunction;
    if (isFunction.isEmpty()) {
        isFunction.reserve(8);
        for (unsigned i = 0; i < 8; ++i)
            isFunction.insert(isFunction_array[i]);
    }
    newRow(expr) << isFunction.contains(expr);
}

void tst_QScriptValue::isFunction_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isFunction(), expected);
    QCOMPARE(value.isFunction(), expected);
}

DEFINE_TEST_FUNCTION(isFunction)


void tst_QScriptValue::isNull_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static QString isNull_array[] = {
    "QScriptValue(QScriptValue::NullValue)",
    "QScriptValue(0, QScriptValue::NullValue)",
    "QScriptValue(engine, QScriptValue::NullValue)",
    "engine->evaluate(\"null\")",
    "engine->nullValue()"};

void tst_QScriptValue::isNull_makeData(const char* expr)
{
    static QSet<QString> isNull;
    if (isNull.isEmpty()) {
        isNull.reserve(5);
        for (unsigned i = 0; i < 5; ++i)
            isNull.insert(isNull_array[i]);
    }
    newRow(expr) << isNull.contains(expr);
}

void tst_QScriptValue::isNull_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isNull(), expected);
    QCOMPARE(value.isNull(), expected);
}

DEFINE_TEST_FUNCTION(isNull)


void tst_QScriptValue::isString_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static QString isString_array[] = {
    "QScriptValue(\"NaN\")",
    "QScriptValue(\"Infinity\")",
    "QScriptValue(\"-Infinity\")",
    "QScriptValue(\"ciao\")",
    "QScriptValue(QString::fromLatin1(\"ciao\"))",
    "QScriptValue(QString(\"\"))",
    "QScriptValue(QString())",
    "QScriptValue(QString(\"0\"))",
    "QScriptValue(QString(\"123\"))",
    "QScriptValue(QString(\"12.4\"))",
    "QScriptValue(0, \"NaN\")",
    "QScriptValue(0, \"Infinity\")",
    "QScriptValue(0, \"-Infinity\")",
    "QScriptValue(0, \"ciao\")",
    "QScriptValue(0, QString::fromLatin1(\"ciao\"))",
    "QScriptValue(0, QString(\"\"))",
    "QScriptValue(0, QString())",
    "QScriptValue(0, QString(\"0\"))",
    "QScriptValue(0, QString(\"123\"))",
    "QScriptValue(0, QString(\"12.3\"))",
    "QScriptValue(engine, \"NaN\")",
    "QScriptValue(engine, \"Infinity\")",
    "QScriptValue(engine, \"-Infinity\")",
    "QScriptValue(engine, \"ciao\")",
    "QScriptValue(engine, QString::fromLatin1(\"ciao\"))",
    "QScriptValue(engine, QString(\"\"))",
    "QScriptValue(engine, QString())",
    "QScriptValue(engine, QString(\"0\"))",
    "QScriptValue(engine, QString(\"123\"))",
    "QScriptValue(engine, QString(\"1.23\"))",
    "engine->evaluate(\"'ciao'\")",
    "engine->evaluate(\"''\")",
    "engine->evaluate(\"'0'\")",
    "engine->evaluate(\"'123'\")",
    "engine->evaluate(\"'12.4'\")"};

void tst_QScriptValue::isString_makeData(const char* expr)
{
    static QSet<QString> isString;
    if (isString.isEmpty()) {
        isString.reserve(35);
        for (unsigned i = 0; i < 35; ++i)
            isString.insert(isString_array[i]);
    }
    newRow(expr) << isString.contains(expr);
}

void tst_QScriptValue::isString_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isString(), expected);
    QCOMPARE(value.isString(), expected);
}

DEFINE_TEST_FUNCTION(isString)


void tst_QScriptValue::isUndefined_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static QString isUndefined_array[] = {
    "QScriptValue(QScriptValue::UndefinedValue)",
    "QScriptValue(0, QScriptValue::UndefinedValue)",
    "QScriptValue(engine, QScriptValue::UndefinedValue)",
    "engine->evaluate(\"{}\")",
    "engine->evaluate(\"undefined\")",
    "engine->undefinedValue()"};

void tst_QScriptValue::isUndefined_makeData(const char* expr)
{
    static QSet<QString> isUndefined;
    if (isUndefined.isEmpty()) {
        isUndefined.reserve(6);
        for (unsigned i = 0; i < 6; ++i)
            isUndefined.insert(isUndefined_array[i]);
    }
    newRow(expr) << isUndefined.contains(expr);
}

void tst_QScriptValue::isUndefined_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isUndefined(), expected);
    QCOMPARE(value.isUndefined(), expected);
}

DEFINE_TEST_FUNCTION(isUndefined)





void tst_QScriptValue::isObject_initData()
{
    QTest::addColumn<bool>("expected");
    initScriptValues();
}

static QString isObject_array[] = {
    "engine->evaluate(\"[]\")",
    "engine->evaluate(\"Object.prototype\")",
    "engine->evaluate(\"Date.prototype\")",
    "engine->evaluate(\"Array.prototype\")",
    "engine->evaluate(\"Function.prototype\")",
    "engine->evaluate(\"Error.prototype\")",
    "engine->evaluate(\"Object\")",
    "engine->evaluate(\"Array\")",
    "engine->evaluate(\"Number\")",
    "engine->evaluate(\"Function\")",
    "engine->evaluate(\"(function() { return 1; })\")",
    "engine->evaluate(\"(function() { return 'ciao'; })\")",
    "engine->evaluate(\"(function() { throw new Error('foo' })\")",
    "engine->evaluate(\"/foo/\")",
    "engine->evaluate(\"new Object()\")",
    "engine->evaluate(\"new Array()\")",
    "engine->evaluate(\"new Error()\")",
    "engine->evaluate(\"a = new Object( a.foo = 22; a.foo\")",
    "engine->evaluate(\"Undefined\")",
    "engine->evaluate(\"Null\")",
    "engine->evaluate(\"True\")",
    "engine->evaluate(\"False\")"};

void tst_QScriptValue::isObject_makeData(const char* expr)
{
    static QSet<QString> isObject;
    if (isObject.isEmpty()) {
        isObject.reserve(22);
        for (unsigned i = 0; i < 22; ++i)
            isObject.insert(isObject_array[i]);
    }
    newRow(expr) << isObject.contains(expr);
}

void tst_QScriptValue::isObject_test(const char*, const QScriptValue& value)
{
    QFETCH(bool, expected);
    QCOMPARE(value.isObject(), expected);
    QCOMPARE(value.isObject(), expected);
}

DEFINE_TEST_FUNCTION(isObject)
