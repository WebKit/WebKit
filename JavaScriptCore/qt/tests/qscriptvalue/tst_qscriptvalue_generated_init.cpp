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
    DEFINE_TEST_VALUE(engine->evaluate("(function() { throw new Error('foo' })"));
    DEFINE_TEST_VALUE(engine->evaluate("/foo/"));
    DEFINE_TEST_VALUE(engine->evaluate("new Object()"));
    DEFINE_TEST_VALUE(engine->evaluate("new Array()"));
    DEFINE_TEST_VALUE(engine->evaluate("new Error()"));
    DEFINE_TEST_VALUE(engine->evaluate("a = new Object( a.foo = 22; a.foo"));
    DEFINE_TEST_VALUE(engine->evaluate("Undefined"));
    DEFINE_TEST_VALUE(engine->evaluate("Null"));
    DEFINE_TEST_VALUE(engine->evaluate("True"));
    DEFINE_TEST_VALUE(engine->evaluate("False"));
    DEFINE_TEST_VALUE(engine->evaluate("undefined"));
    DEFINE_TEST_VALUE(engine->evaluate("null"));
    DEFINE_TEST_VALUE(engine->evaluate("true"));
    DEFINE_TEST_VALUE(engine->evaluate("false"));
    DEFINE_TEST_VALUE(engine->evaluate("122"));
    DEFINE_TEST_VALUE(engine->evaluate("124"));
    DEFINE_TEST_VALUE(engine->evaluate("0"));
    DEFINE_TEST_VALUE(engine->evaluate("0.0"));
    DEFINE_TEST_VALUE(engine->evaluate("123.0"));
    DEFINE_TEST_VALUE(engine->evaluate("6.37e-8"));
    DEFINE_TEST_VALUE(engine->evaluate("-6.37e-8"));
    DEFINE_TEST_VALUE(engine->evaluate("0x43211234"));
    DEFINE_TEST_VALUE(engine->evaluate("0x10000"));
    DEFINE_TEST_VALUE(engine->evaluate("0x10001"));
    DEFINE_TEST_VALUE(engine->evaluate("NaN"));
    DEFINE_TEST_VALUE(engine->evaluate("Infinity"));
    DEFINE_TEST_VALUE(engine->evaluate("-Infinity"));
    DEFINE_TEST_VALUE(engine->evaluate("'ciao'"));
    DEFINE_TEST_VALUE(engine->evaluate("''"));
    DEFINE_TEST_VALUE(engine->evaluate("'0'"));
    DEFINE_TEST_VALUE(engine->evaluate("'123'"));
    DEFINE_TEST_VALUE(engine->evaluate("'12.4'"));
    DEFINE_TEST_VALUE(engine->nullValue());
    DEFINE_TEST_VALUE(engine->undefinedValue());
}
