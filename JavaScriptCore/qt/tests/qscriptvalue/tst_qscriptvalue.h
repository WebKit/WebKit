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

#ifndef tst_qscriptvalue_h
#define tst_qscriptvalue_h

#include "qscriptengine.h"
#include "qscriptvalue.h"
#include <QtCore/qnumeric.h>
#include <QtTest/qtest.h>

Q_DECLARE_METATYPE(QScriptValue*);
Q_DECLARE_METATYPE(QScriptValue);

class tst_QScriptValue : public QObject {
    Q_OBJECT

public:
    tst_QScriptValue();
    virtual ~tst_QScriptValue();

private slots:
    void toStringSimple_data();
    void toStringSimple();
    void copyConstructor_data();
    void copyConstructor();
    void assignOperator_data();
    void assignOperator();
    void dataSharing();
    void constructors_data();
    void constructors();
    void call();
    void ctor();

    // Generated test functions.
    void isBool_data();
    void isBool();

    void isBoolean_data();
    void isBoolean();

    void isNumber_data();
    void isNumber();

    void isFunction_data();
    void isFunction();

    void isNull_data();
    void isNull();

    void isObject_data();
    void isObject();

    void isString_data();
    void isString();

    void isUndefined_data();
    void isUndefined();

    void isValid_data();
    void isValid();

    void toString_data();
    void toString();

    void toNumber_data();
    void toNumber();

    void toBool_data();
    void toBool();

    void toBoolean_data();
    void toBoolean();

    void toInteger_data();
    void toInteger();

    void toInt32_data();
    void toInt32();

    void toUInt32_data();
    void toUInt32();

    void toUInt16_data();
    void toUInt16();

private:
    typedef void (tst_QScriptValue::*InitDataFunction)();
    typedef void (tst_QScriptValue::*DefineDataFunction)(const char*);
    void dataHelper(InitDataFunction init, DefineDataFunction define);
    QTestData& newRow(const char* tag);

    typedef void (tst_QScriptValue::*TestFunction)(const char*, const QScriptValue&);
    void testHelper(TestFunction fun);

    // Generated functions

    void initScriptValues();

    void isBool_initData();
    void isBool_makeData(const char* expr);
    void isBool_test(const char* expr, const QScriptValue& value);

    void isBoolean_initData();
    void isBoolean_makeData(const char* expr);
    void isBoolean_test(const char* expr, const QScriptValue& value);

    void isNumber_initData();
    void isNumber_makeData(const char* expr);
    void isNumber_test(const char* expr, const QScriptValue&);

    void isFunction_initData();
    void isFunction_makeData(const char* expr);
    void isFunction_test(const char* expr, const QScriptValue& value);

    void isNull_initData();
    void isNull_makeData(const char* expr);
    void isNull_test(const char* expr, const QScriptValue& value);

    void isObject_initData();
    void isObject_makeData(const char* expr);
    void isObject_test(const char* expr, const QScriptValue& value);

    void isString_initData();
    void isString_makeData(const char* expr);
    void isString_test(const char* expr, const QScriptValue& value);

    void isUndefined_initData();
    void isUndefined_makeData(const char* expr);
    void isUndefined_test(const char* expr, const QScriptValue& value);

    void isValid_initData();
    void isValid_makeData(const char* expr);
    void isValid_test(const char* expr, const QScriptValue& value);

    void toString_initData();
    void toString_makeData(const char*);
    void toString_test(const char*, const QScriptValue&);

    void toNumber_initData();
    void toNumber_makeData(const char*);
    void toNumber_test(const char*, const QScriptValue&);

    void toBool_initData();
    void toBool_makeData(const char*);
    void toBool_test(const char*, const QScriptValue&);

    void toBoolean_initData();
    void toBoolean_makeData(const char*);
    void toBoolean_test(const char*, const QScriptValue&);

    void toInteger_initData();
    void toInteger_makeData(const char*);
    void toInteger_test(const char*, const QScriptValue&);

    void toInt32_initData();
    void toInt32_makeData(const char*);
    void toInt32_test(const char*, const QScriptValue&);

    void toUInt32_initData();
    void toUInt32_makeData(const char*);
    void toUInt32_test(const char*, const QScriptValue&);

    void toUInt16_initData();
    void toUInt16_makeData(const char*);
    void toUInt16_test(const char*, const QScriptValue&);

private:
    QScriptEngine* engine;
    QHash<QString, QScriptValue> m_values;
    QString m_currentExpression;
};

#define DEFINE_TEST_FUNCTION(name) \
void tst_QScriptValue::name##_data() { dataHelper(&tst_QScriptValue::name##_initData, &tst_QScriptValue::name##_makeData); } \
void tst_QScriptValue::name() { testHelper(&tst_QScriptValue::name##_test); }



#endif // tst_qscriptvalue_h
