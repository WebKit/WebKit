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

#define DEFINE_TEST_VALUE(expr) m_values.insert(QString::fromLatin1(#expr), expr)

Q_DECLARE_METATYPE(QScriptValue*);
Q_DECLARE_METATYPE(QScriptValue);
typedef QPair<QString, QScriptValue> QPairQStringAndQScriptValue;
Q_DECLARE_METATYPE(QPairQStringAndQScriptValue);

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
    void getSetPrototype();
    void call();
    void ctor();
    void toObjectSimple();
    void getPropertySimple_data();
    void getPropertySimple();
    void setPropertySimple();
    void setProperty_data();
    void setProperty();
    void getSetProperty();
    void getPropertyResolveFlag();
    void propertyFlag_data();
    void propertyFlag();
    void globalObjectChanges();
    void assignAndCopyConstruct_data();
    void assignAndCopyConstruct();

    // Generated test functions.
    void isArray_data();
    void isArray();

    void isBool_data();
    void isBool();

    void isBoolean_data();
    void isBoolean();

    void isError_data();
    void isError();

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

    void equals_data();
    void equals();

    void strictlyEquals_data();
    void strictlyEquals();

    void instanceOf_data();
    void instanceOf();

private:
    // Generated function
    QPair<QString, QScriptValue> initScriptValues(uint idx);

    QScriptEngine* m_engine;
};

#endif // tst_qscriptvalue_h
