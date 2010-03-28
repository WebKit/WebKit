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
#include "qscriptprogram.h"
#include "qscriptsyntaxcheckresult.h"
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
    void globalObject();
    void evaluate();
    void collectGarbage();
    void reportAdditionalMemoryCost();
    void nullValue();
    void undefinedValue();
    void evaluateProgram();
    void checkSyntax_data();
    void checkSyntax();
};

/* Evaluating a script that throw an unhandled exception should return an invalid value. */
void tst_QScriptEngine::evaluate()
{
    QScriptEngine engine;
    QVERIFY2(engine.evaluate("1+1").isValid(), "the expression should be evaluated and an valid result should be returned");
    QVERIFY2(engine.evaluate("ping").isValid(), "Script throwing an unhandled exception should return an exception value");
}

void tst_QScriptEngine::globalObject()
{
    QScriptEngine engine;
    QScriptValue global = engine.globalObject();
    QScriptValue self = engine.evaluate("this");
    QVERIFY(global.isObject());
    QVERIFY(engine.globalObject().equals(engine.evaluate("this")));
    QEXPECT_FAIL("", "strictlyEquals is broken - bug 36600 in bugs.webkit.org", Continue);
    QVERIFY(engine.globalObject().strictlyEquals(self));
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

void tst_QScriptEngine::reportAdditionalMemoryCost()
{
    // There isn't any easy way to test the responsiveness of the GC;
    // just try to call the function a few times with various sizes.
    QScriptEngine eng;
    for (int i = 0; i < 100; ++i) {
        eng.reportAdditionalMemoryCost(0);
        eng.reportAdditionalMemoryCost(10);
        eng.reportAdditionalMemoryCost(1000);
        eng.reportAdditionalMemoryCost(10000);
        eng.reportAdditionalMemoryCost(100000);
        eng.reportAdditionalMemoryCost(1000000);
        eng.reportAdditionalMemoryCost(10000000);
        eng.reportAdditionalMemoryCost(-1);
        eng.reportAdditionalMemoryCost(-1000);
        QScriptValue obj = eng.evaluate("new Object");
        eng.collectGarbage();
    }
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

void tst_QScriptEngine::evaluateProgram()
{
    QScriptEngine eng;
    {
        QString code("1 + 2");
        QString fileName("hello.js");
        int lineNumber = 123;
        QScriptProgram program(code, fileName, lineNumber);
        QVERIFY(!program.isNull());
        QCOMPARE(program.sourceCode(), code);
        QCOMPARE(program.fileName(), fileName);
        QCOMPARE(program.firstLineNumber(), lineNumber);

        QScriptValue expected = eng.evaluate(code);
        for (int x = 0; x < 10; ++x) {
            QScriptValue ret = eng.evaluate(program);
            QVERIFY(ret.equals(expected));
        }

        // operator=
        QScriptProgram sameProgram = program;
        QVERIFY(sameProgram == program);
        QVERIFY(eng.evaluate(sameProgram).equals(expected));

        // copy constructor
        QScriptProgram sameProgram2(program);
        QVERIFY(sameProgram2 == program);
        QVERIFY(eng.evaluate(sameProgram2).equals(expected));

        QScriptProgram differentProgram("2 + 3");
        QVERIFY(differentProgram != program);
        QVERIFY(!eng.evaluate(differentProgram).equals(expected));
    }

    // Program that accesses variable in the scope
    {
        QScriptProgram program("a");
        QVERIFY(!program.isNull());
        {
            QScriptValue ret = eng.evaluate(program);
            QVERIFY(ret.isError());
            QCOMPARE(ret.toString(), QString::fromLatin1("ReferenceError: Can't find variable: a"));
        }
        {
            QScriptValue ret = eng.evaluate(program);
            QVERIFY(ret.isError());
        }
        eng.evaluate("a = 456");
        {
            QScriptValue ret = eng.evaluate(program);
            QVERIFY(!ret.isError());
            QCOMPARE(ret.toNumber(), 456.0);
        }
    }

    // Program that creates closure
    {
        QScriptProgram program("(function() { var count = 0; return function() { return count++; }; })");
        QVERIFY(!program.isNull());
        QScriptValue createCounter = eng.evaluate(program);
        QVERIFY(createCounter.isFunction());
        QScriptValue counter = createCounter.call();
        QVERIFY(counter.isFunction());
        {
            QScriptValue ret = counter.call();
            QVERIFY(ret.isNumber());
        }
        QScriptValue counter2 = createCounter.call();
        QVERIFY(counter2.isFunction());
        QVERIFY(!counter2.equals(counter));
        {
            QScriptValue ret = counter2.call();
            QVERIFY(ret.isNumber());
        }
    }

    // Same program run in different engines
    {
        QString code("1 + 2");
        QScriptProgram program(code);
        QVERIFY(!program.isNull());
        double expected = eng.evaluate(program).toNumber();
        for (int x = 0; x < 2; ++x) {
            QScriptEngine eng2;
            for (int y = 0; y < 2; ++y) {
                double ret = eng2.evaluate(program).toNumber();
                QCOMPARE(ret, expected);
            }
        }
    }

    // No program
    {
        QScriptProgram program;
        QVERIFY(program.isNull());
        QScriptValue ret = eng.evaluate(program);
        QVERIFY(!ret.isValid());
    }
}

void tst_QScriptEngine::checkSyntax_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<int>("expectedState");
    QTest::addColumn<int>("errorLineNumber");
    QTest::addColumn<int>("errorColumnNumber");
    QTest::addColumn<QString>("errorMessage");

    QTest::newRow("0")
        << QString("0") << int(QScriptSyntaxCheckResult::Valid)
        << -1 << -1 << "";
    QTest::newRow("if (")
        << QString("if (\n") << int(QScriptSyntaxCheckResult::Intermediate)
        << 1 << 4 << "";
    QTest::newRow("if else")
        << QString("\nif else") << int(QScriptSyntaxCheckResult::Error)
        << 2 << 4 << "SyntaxError: Parse error";
    QTest::newRow("{if}")
            << QString("{\n{\nif\n}\n") << int(QScriptSyntaxCheckResult::Error)
        << 4 << 1 << "SyntaxError: Parse error";
    QTest::newRow("foo[")
        << QString("foo[") << int(QScriptSyntaxCheckResult::Error)
        << 1 << 4 << "SyntaxError: Parse error";
    QTest::newRow("foo['bar']")
        << QString("foo['bar']") << int(QScriptSyntaxCheckResult::Valid)
        << -1 << -1 << "";

    QTest::newRow("/*")
        << QString("/*") << int(QScriptSyntaxCheckResult::Intermediate)
        << 1 << 1 << "Unclosed comment at end of file";
    QTest::newRow("/*\nMy comment")
        << QString("/*\nMy comment") << int(QScriptSyntaxCheckResult::Intermediate)
        << 1 << 1 << "Unclosed comment at end of file";
    QTest::newRow("/*\nMy comment */\nfoo = 10")
        << QString("/*\nMy comment */\nfoo = 10") << int(QScriptSyntaxCheckResult::Valid)
        << -1 << -1 << "";
    QTest::newRow("foo = 10 /*")
        << QString("foo = 10 /*") << int(QScriptSyntaxCheckResult::Intermediate)
        << -1 << -1 << "";
    QTest::newRow("foo = 10; /*")
        << QString("foo = 10; /*") << int(QScriptSyntaxCheckResult::Intermediate)
        << 1 << 11 << "Expected `end of file'";
    QTest::newRow("foo = 10 /* My comment */")
        << QString("foo = 10 /* My comment */") << int(QScriptSyntaxCheckResult::Valid)
        << -1 << -1 << "";

    QTest::newRow("/=/")
        << QString("/=/") << int(QScriptSyntaxCheckResult::Valid) << -1 << -1 << "";
    QTest::newRow("/=/g")
        << QString("/=/g") << int(QScriptSyntaxCheckResult::Valid) << -1 << -1 << "";
    QTest::newRow("/a/")
        << QString("/a/") << int(QScriptSyntaxCheckResult::Valid) << -1 << -1 << "";
    QTest::newRow("/a/g")
        << QString("/a/g") << int(QScriptSyntaxCheckResult::Valid) << -1 << -1 << "";
}

void tst_QScriptEngine::checkSyntax()
{
    QFETCH(QString, code);
    QFETCH(int, expectedState);
    QFETCH(int, errorLineNumber);
    QFETCH(int, errorColumnNumber);
    QFETCH(QString, errorMessage);

    QScriptSyntaxCheckResult result = QScriptEngine::checkSyntax(code);

    // assignment
    {
        QScriptSyntaxCheckResult copy = result;
        QCOMPARE(copy.state(), result.state());
        QCOMPARE(copy.errorLineNumber(), result.errorLineNumber());
        QCOMPARE(copy.errorColumnNumber(), result.errorColumnNumber());
        QCOMPARE(copy.errorMessage(), result.errorMessage());
    }
    {
        QScriptSyntaxCheckResult copy(result);
        QCOMPARE(copy.state(), result.state());
        QCOMPARE(copy.errorLineNumber(), result.errorLineNumber());
        QCOMPARE(copy.errorColumnNumber(), result.errorColumnNumber());
        QCOMPARE(copy.errorMessage(), result.errorMessage());
    }

    if (expectedState == QScriptSyntaxCheckResult::Intermediate)
        QEXPECT_FAIL("", "QScriptSyntaxCheckResult::state() doesn't return the Intermediate state", Abort);
    QCOMPARE(result.state(), QScriptSyntaxCheckResult::State(expectedState));
    QCOMPARE(result.errorLineNumber(), errorLineNumber);
    if (expectedState != QScriptSyntaxCheckResult::Valid && errorColumnNumber != 1)
            QEXPECT_FAIL("", "QScriptSyntaxCheckResult::errorColumnNumber() doesn't return correct value", Continue);
    QCOMPARE(result.errorColumnNumber(), errorColumnNumber);
    QCOMPARE(result.errorMessage(), errorMessage);
}


QTEST_MAIN(tst_QScriptEngine)
#include "tst_qscriptengine.moc"
