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

#include "qscriptengine.h"
#include "qscriptstring.h"
#include "qscriptvalue.h"
#include <qtest.h>

Q_DECLARE_METATYPE(QScriptValue);

class tst_QScriptValue : public QObject {
    Q_OBJECT

public:
    tst_QScriptValue()
        : m_engine(0)
    {}

    ~tst_QScriptValue()
    {
        if (m_engine)
            delete m_engine;
    }

private slots:
    void values_data();

    void ctorBool();
    void ctorReal();
    void ctorNumber();
    void ctorQString();
    void ctorCString();
    void ctorSpecial();
    void ctorQScriptValue();

    void isValid_data();
    void isValid();
    void isBool_data();
    void isBool();
    void isNumber_data();
    void isNumber();
    void isFunction_data();
    void isFunction();
    void isNull_data();
    void isNull();
    void isString_data();
    void isString();
    void isUndefined_data();
    void isUndefined();
    void isObject_data();
    void isObject();
    void isError_data();
    void isError();

    void toString_data();
    void toString();
    void toNumber_data();
    void toNumber();
    void toBool_data();
    void toBool();
    void toInteger_data();
    void toInteger();
    void toInt32_data();
    void toInt32();
    void toUInt32_data();
    void toUInt32();
    void toUInt16_data();
    void toUInt16();
    void toObject_data();
    void toObject();

    void equals_data();
    void equals();
    void strictlyEquals_data();
    void strictlyEquals();
    void instanceOf_data();
    void instanceOf();

private:
    QScriptEngine* m_engine;
};

void tst_QScriptValue::values_data()
{
    if (m_engine)
        delete m_engine;
    m_engine = new QScriptEngine;

    QTest::addColumn<QScriptValue>("value");

    QTest::newRow("invalid") << QScriptValue();

    QTest::newRow("cbool") << QScriptValue(true);
    QTest::newRow("cnumber") << QScriptValue(1234);
    QTest::newRow("cstring") << QScriptValue("abc");
    QTest::newRow("cnull") << QScriptValue(QScriptValue::NullValue);
    QTest::newRow("cundefined") << QScriptValue(QScriptValue::UndefinedValue);

    QTest::newRow("jsbool") << m_engine->evaluate("true");
    QTest::newRow("jsnumber") << m_engine->evaluate("12345");
    QTest::newRow("jsstring") << m_engine->evaluate("'go'");
    QTest::newRow("jsfunction") << m_engine->evaluate("(function {})");
    QTest::newRow("jsnull") << m_engine->nullValue();
    QTest::newRow("jsundefined") << m_engine->undefinedValue();
    QTest::newRow("jsobject") << m_engine->newObject();
    QTest::newRow("jserror") << m_engine->evaluate("new Error()");
}

void tst_QScriptValue::ctorBool()
{
    QBENCHMARK {
        QScriptValue(true);
    }
}

void tst_QScriptValue::ctorReal()
{
    QBENCHMARK {
        QScriptValue(12.3);
    }
}

void tst_QScriptValue::ctorNumber()
{
    QBENCHMARK {
        QScriptValue(123);
    }
}

void tst_QScriptValue::ctorQString()
{
    QString str = QString::fromLatin1("ciao");
    QBENCHMARK {
        QScriptValue(str);
    }
}

void tst_QScriptValue::ctorCString()
{
    QBENCHMARK {
        QScriptValue("Pong!");
    }
}

void tst_QScriptValue::ctorSpecial()
{
    QBENCHMARK {
        (void)QScriptValue(QScriptValue::NullValue);
    }
}

void tst_QScriptValue::ctorQScriptValue()
{
    QScriptValue tmp(1234);
    QBENCHMARK {
        QScriptValue(tmp);
    }
}

void tst_QScriptValue::isValid_data()
{
    values_data();
}

void tst_QScriptValue::isValid()
{
    QFETCH(QScriptValue, value);
    QBENCHMARK {
        value.isValid();
    }
}

void tst_QScriptValue::isBool_data()
{
    values_data();
}

void tst_QScriptValue::isBool()
{
    QFETCH(QScriptValue, value);
    QBENCHMARK {
        value.isBool();
    }
}

void tst_QScriptValue::isNumber_data()
{
    values_data();
}

void tst_QScriptValue::isNumber()
{
    QFETCH(QScriptValue, value);
    QBENCHMARK {
        value.isNumber();
    }
}

void tst_QScriptValue::isFunction_data()
{
    values_data();
}

void tst_QScriptValue::isFunction()
{
    QFETCH(QScriptValue, value);
    QBENCHMARK {
        value.isFunction();
    }
}

void tst_QScriptValue::isNull_data()
{
    values_data();
}

void tst_QScriptValue::isNull()
{
    QFETCH(QScriptValue, value);
    QBENCHMARK {
        value.isNull();
    }
}

void tst_QScriptValue::isString_data()
{
    values_data();
}

void tst_QScriptValue::isString()
{
    QFETCH(QScriptValue, value);
    QBENCHMARK {
        value.isString();
    }
}

void tst_QScriptValue::isUndefined_data()
{
    values_data();
}

void tst_QScriptValue::isUndefined()
{
    QFETCH(QScriptValue, value);
    QBENCHMARK {
        value.isUndefined();
    }
}

void tst_QScriptValue::isObject_data()
{
    values_data();
}

void tst_QScriptValue::isObject()
{
    QFETCH(QScriptValue, value);
    QBENCHMARK {
        value.isObject();
    }
}

void tst_QScriptValue::isError_data()
{
    values_data();
}

void tst_QScriptValue::isError()
{
    QFETCH(QScriptValue, value);
    QBENCHMARK {
        value.isError();
    }
}

void tst_QScriptValue::toString_data()
{
    values_data();
}

void tst_QScriptValue::toString()
{
    QFETCH(QScriptValue, value);
    QBENCHMARK {
        value.toString();
    }
}

void tst_QScriptValue::toNumber_data()
{
    values_data();
}

void tst_QScriptValue::toNumber()
{
    QFETCH(QScriptValue, value);
    QBENCHMARK {
        value.toNumber();
    }
}

void tst_QScriptValue::toBool_data()
{
    values_data();
}

void tst_QScriptValue::toBool()
{
    QFETCH(QScriptValue, value);
    QBENCHMARK {
        value.toBool();
    }
}

void tst_QScriptValue::toInteger_data()
{
    values_data();
}

void tst_QScriptValue::toInteger()
{
    QFETCH(QScriptValue, value);
    QBENCHMARK {
        value.toInteger();
    }
}

void tst_QScriptValue::toInt32_data()
{
    values_data();
}

void tst_QScriptValue::toInt32()
{
    QFETCH(QScriptValue, value);
    QBENCHMARK {
        value.toInt32();
    }
}

void tst_QScriptValue::toUInt32_data()
{
    values_data();
}

void tst_QScriptValue::toUInt32()
{
    QFETCH(QScriptValue, value);
    QBENCHMARK {
        value.toUInt32();
    }
}

void tst_QScriptValue::toUInt16_data()
{
    values_data();
}

void tst_QScriptValue::toUInt16()
{
    QFETCH(QScriptValue, value);
    QBENCHMARK {
        value.toUInt16();
    }
}

void tst_QScriptValue::toObject_data()
{
    values_data();
}

void tst_QScriptValue::toObject()
{
    QFETCH(QScriptValue, value);
    QBENCHMARK {
        value.toObject();
    }
}

void tst_QScriptValue::equals_data()
{
    values_data();
}

void tst_QScriptValue::equals()
{
    QFETCH(QScriptValue, value);
    static QScriptValue previous;
    QBENCHMARK {
        value.equals(previous);
    }
    previous = value;
}

void tst_QScriptValue::strictlyEquals_data()
{
    values_data();
}

void tst_QScriptValue::strictlyEquals()
{
    QFETCH(QScriptValue, value);
    static QScriptValue previous;
    QBENCHMARK {
        value.strictlyEquals(previous);
    }
    previous = value;
}

void tst_QScriptValue::instanceOf_data()
{
    values_data();
}

void tst_QScriptValue::instanceOf()
{
    QFETCH(QScriptValue, value);
    static QScriptValue object = m_engine->newObject();
    QBENCHMARK {
        value.instanceOf(object);
    }
}

QTEST_MAIN(tst_QScriptValue)
#include "tst_qscriptvalue.moc"
