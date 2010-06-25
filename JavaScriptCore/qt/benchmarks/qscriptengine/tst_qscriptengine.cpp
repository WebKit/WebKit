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
#include "qscriptvalue.h"
#include <qtest.h>

class tst_QScriptEngine : public QObject {
    Q_OBJECT

private slots:
    void checkSyntax_data();
    void checkSyntax();
    void constructor();
    void evaluateString_data();
    void evaluateString();
    void evaluateProgram_data();
    void evaluateProgram();
    void newObject();
    void nullValue();
    void undefinedValue();
    void globalObject();
    void toStringHandle();
};

void tst_QScriptEngine::checkSyntax_data()
{
    evaluateString_data();
}

void tst_QScriptEngine::checkSyntax()
{
    QFETCH(QString, code);
    QScriptEngine engine;
    QBENCHMARK {
        engine.checkSyntax(code);
    }
}

void tst_QScriptEngine::constructor()
{
    QBENCHMARK {
        QScriptEngine engine;
    }
}

void tst_QScriptEngine::evaluateString_data()
{
    QTest::addColumn<QString>("code");
    QTest::newRow("empty script") << QString::fromLatin1("");
    QTest::newRow("number literal") << QString::fromLatin1("123");
    QTest::newRow("string literal") << QString::fromLatin1("'ciao'");
    QTest::newRow("regexp literal") << QString::fromLatin1("/foo/gim");
    QTest::newRow("null literal") << QString::fromLatin1("null");
    QTest::newRow("undefined literal") << QString::fromLatin1("undefined");
    QTest::newRow("empty object literal") << QString::fromLatin1("{}");
    QTest::newRow("this") << QString::fromLatin1("this");
}

void tst_QScriptEngine::evaluateString()
{
    QFETCH(QString, code);
    QScriptEngine engine;
    QBENCHMARK {
        engine.evaluate(code);
    }
}

void tst_QScriptEngine::evaluateProgram_data()
{
    evaluateString_data();
}

void tst_QScriptEngine::evaluateProgram()
{
    QFETCH(QString, code);
    QScriptEngine engine;
    QScriptProgram program(code);
    QBENCHMARK {
        engine.evaluate(program);
    }
}

void tst_QScriptEngine::newObject()
{
    QScriptEngine engine;
    QBENCHMARK {
        engine.newObject();
    }
}

void tst_QScriptEngine::nullValue()
{
    QScriptEngine engine;
    QBENCHMARK {
        engine.nullValue();
    }
}

void tst_QScriptEngine::undefinedValue()
{
    QScriptEngine engine;
    QBENCHMARK {
        engine.undefinedValue();
    }
}

void tst_QScriptEngine::globalObject()
{
    QScriptEngine engine;
    QBENCHMARK {
        engine.globalObject();
    }
}

void tst_QScriptEngine::toStringHandle()
{
    QScriptEngine engine;
    QString str = QString::fromLatin1("foobarbaz");
    QBENCHMARK {
        engine.toStringHandle(str);
    }
}

QTEST_MAIN(tst_QScriptEngine)
#include "tst_qscriptengine.moc"
