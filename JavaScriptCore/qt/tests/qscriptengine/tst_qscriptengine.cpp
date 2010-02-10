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

#include "qscriptengine.h"
#include "qscriptvalue.h"
#include <QtTest/qtest.h>

class tst_QScriptEngine : public QObject {
    Q_OBJECT

public:
    tst_QScriptEngine() {}
    virtual ~tst_QScriptEngine() {}

public slots:
    void init() {}
    void cleanup() {}

private slots:
    void evaluate();
    void collectGarbage();
    void nullValue();
    void undefinedValue();
};

/* Evaluating a script that throw an unhandled exception should return an invalid value. */
void tst_QScriptEngine::evaluate()
{
    QScriptEngine engine;
    QVERIFY2(engine.evaluate("1+1").isValid(), "the expression should be evaluated and an valid result should be returned");
    QVERIFY2(engine.evaluate("ping").isValid(), "Script throwing an unhandled exception should return an exception value");
}

/* Test garbage collection, at least try to not crash. */
void tst_QScriptEngine::collectGarbage()
{
    QScriptEngine engine;
    QScriptValue foo = engine.evaluate("( function foo() {return 'pong';} )");
    QVERIFY(foo.isFunction());
    engine.collectGarbage();
    QCOMPARE(foo.call().toString(), QString::fromAscii("pong"));
}

void tst_QScriptEngine::nullValue()
{
    QScriptEngine engine;
    QScriptValue value = engine.nullValue();
    QVERIFY(value.isValid());
    QVERIFY(value.isNull());
}

void tst_QScriptEngine::undefinedValue()
{
    QScriptEngine engine;
    QScriptValue value = engine.undefinedValue();
    QVERIFY(value.isValid());
    QVERIFY(value.isUndefined());
}

QTEST_MAIN(tst_QScriptEngine)
#include "tst_qscriptengine.moc"
