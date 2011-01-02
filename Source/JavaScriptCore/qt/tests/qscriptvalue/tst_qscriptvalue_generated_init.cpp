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


QPair<QString, QScriptValue> tst_QScriptValue::initScriptValues(uint idx)
{
    QScriptEngine* engine = m_engine;
    switch (idx) {
    case 0: return QPair<QString, QScriptValue>("QScriptValue()", QScriptValue());
    case 1: return QPair<QString, QScriptValue>("QScriptValue(QScriptValue::UndefinedValue)", QScriptValue(QScriptValue::UndefinedValue));
    case 2: return QPair<QString, QScriptValue>("QScriptValue(QScriptValue::NullValue)", QScriptValue(QScriptValue::NullValue));
    case 3: return QPair<QString, QScriptValue>("QScriptValue(true)", QScriptValue(true));
    case 4: return QPair<QString, QScriptValue>("QScriptValue(false)", QScriptValue(false));
    case 5: return QPair<QString, QScriptValue>("QScriptValue(int(122))", QScriptValue(int(122)));
    case 6: return QPair<QString, QScriptValue>("QScriptValue(uint(124))", QScriptValue(uint(124)));
    case 7: return QPair<QString, QScriptValue>("QScriptValue(0)", QScriptValue(0));
    case 8: return QPair<QString, QScriptValue>("QScriptValue(0.0)", QScriptValue(0.0));
    case 9: return QPair<QString, QScriptValue>("QScriptValue(123.0)", QScriptValue(123.0));
    case 10: return QPair<QString, QScriptValue>("QScriptValue(6.37e-8)", QScriptValue(6.37e-8));
    case 11: return QPair<QString, QScriptValue>("QScriptValue(-6.37e-8)", QScriptValue(-6.37e-8));
    case 12: return QPair<QString, QScriptValue>("QScriptValue(0x43211234)", QScriptValue(0x43211234));
    case 13: return QPair<QString, QScriptValue>("QScriptValue(0x10000)", QScriptValue(0x10000));
    case 14: return QPair<QString, QScriptValue>("QScriptValue(0x10001)", QScriptValue(0x10001));
    case 15: return QPair<QString, QScriptValue>("QScriptValue(qSNaN())", QScriptValue(qSNaN()));
    case 16: return QPair<QString, QScriptValue>("QScriptValue(qQNaN())", QScriptValue(qQNaN()));
    case 17: return QPair<QString, QScriptValue>("QScriptValue(qInf())", QScriptValue(qInf()));
    case 18: return QPair<QString, QScriptValue>("QScriptValue(-qInf())", QScriptValue(-qInf()));
    case 19: return QPair<QString, QScriptValue>("QScriptValue(\"NaN\")", QScriptValue("NaN"));
    case 20: return QPair<QString, QScriptValue>("QScriptValue(\"Infinity\")", QScriptValue("Infinity"));
    case 21: return QPair<QString, QScriptValue>("QScriptValue(\"-Infinity\")", QScriptValue("-Infinity"));
    case 22: return QPair<QString, QScriptValue>("QScriptValue(\"ciao\")", QScriptValue("ciao"));
    case 23: return QPair<QString, QScriptValue>("QScriptValue(QString::fromLatin1(\"ciao\"))", QScriptValue(QString::fromLatin1("ciao")));
    case 24: return QPair<QString, QScriptValue>("QScriptValue(QString(\"\"))", QScriptValue(QString("")));
    case 25: return QPair<QString, QScriptValue>("QScriptValue(QString())", QScriptValue(QString()));
    case 26: return QPair<QString, QScriptValue>("QScriptValue(QString(\"0\"))", QScriptValue(QString("0")));
    case 27: return QPair<QString, QScriptValue>("QScriptValue(QString(\"123\"))", QScriptValue(QString("123")));
    case 28: return QPair<QString, QScriptValue>("QScriptValue(QString(\"12.4\"))", QScriptValue(QString("12.4")));
    case 29: return QPair<QString, QScriptValue>("QScriptValue(0, QScriptValue::UndefinedValue)", QScriptValue(0, QScriptValue::UndefinedValue));
    case 30: return QPair<QString, QScriptValue>("QScriptValue(0, QScriptValue::NullValue)", QScriptValue(0, QScriptValue::NullValue));
    case 31: return QPair<QString, QScriptValue>("QScriptValue(0, true)", QScriptValue(0, true));
    case 32: return QPair<QString, QScriptValue>("QScriptValue(0, false)", QScriptValue(0, false));
    case 33: return QPair<QString, QScriptValue>("QScriptValue(0, int(122))", QScriptValue(0, int(122)));
    case 34: return QPair<QString, QScriptValue>("QScriptValue(0, uint(124))", QScriptValue(0, uint(124)));
    case 35: return QPair<QString, QScriptValue>("QScriptValue(0, 0)", QScriptValue(0, 0));
    case 36: return QPair<QString, QScriptValue>("QScriptValue(0, 0.0)", QScriptValue(0, 0.0));
    case 37: return QPair<QString, QScriptValue>("QScriptValue(0, 123.0)", QScriptValue(0, 123.0));
    case 38: return QPair<QString, QScriptValue>("QScriptValue(0, 6.37e-8)", QScriptValue(0, 6.37e-8));
    case 39: return QPair<QString, QScriptValue>("QScriptValue(0, -6.37e-8)", QScriptValue(0, -6.37e-8));
    case 40: return QPair<QString, QScriptValue>("QScriptValue(0, 0x43211234)", QScriptValue(0, 0x43211234));
    case 41: return QPair<QString, QScriptValue>("QScriptValue(0, 0x10000)", QScriptValue(0, 0x10000));
    case 42: return QPair<QString, QScriptValue>("QScriptValue(0, 0x10001)", QScriptValue(0, 0x10001));
    case 43: return QPair<QString, QScriptValue>("QScriptValue(0, qSNaN())", QScriptValue(0, qSNaN()));
    case 44: return QPair<QString, QScriptValue>("QScriptValue(0, qQNaN())", QScriptValue(0, qQNaN()));
    case 45: return QPair<QString, QScriptValue>("QScriptValue(0, qInf())", QScriptValue(0, qInf()));
    case 46: return QPair<QString, QScriptValue>("QScriptValue(0, -qInf())", QScriptValue(0, -qInf()));
    case 47: return QPair<QString, QScriptValue>("QScriptValue(0, \"NaN\")", QScriptValue(0, "NaN"));
    case 48: return QPair<QString, QScriptValue>("QScriptValue(0, \"Infinity\")", QScriptValue(0, "Infinity"));
    case 49: return QPair<QString, QScriptValue>("QScriptValue(0, \"-Infinity\")", QScriptValue(0, "-Infinity"));
    case 50: return QPair<QString, QScriptValue>("QScriptValue(0, \"ciao\")", QScriptValue(0, "ciao"));
    case 51: return QPair<QString, QScriptValue>("QScriptValue(0, QString::fromLatin1(\"ciao\"))", QScriptValue(0, QString::fromLatin1("ciao")));
    case 52: return QPair<QString, QScriptValue>("QScriptValue(0, QString(\"\"))", QScriptValue(0, QString("")));
    case 53: return QPair<QString, QScriptValue>("QScriptValue(0, QString())", QScriptValue(0, QString()));
    case 54: return QPair<QString, QScriptValue>("QScriptValue(0, QString(\"0\"))", QScriptValue(0, QString("0")));
    case 55: return QPair<QString, QScriptValue>("QScriptValue(0, QString(\"123\"))", QScriptValue(0, QString("123")));
    case 56: return QPair<QString, QScriptValue>("QScriptValue(0, QString(\"12.3\"))", QScriptValue(0, QString("12.3")));
    case 57: return QPair<QString, QScriptValue>("QScriptValue(engine, QScriptValue::UndefinedValue)", QScriptValue(engine, QScriptValue::UndefinedValue));
    case 58: return QPair<QString, QScriptValue>("QScriptValue(engine, QScriptValue::NullValue)", QScriptValue(engine, QScriptValue::NullValue));
    case 59: return QPair<QString, QScriptValue>("QScriptValue(engine, true)", QScriptValue(engine, true));
    case 60: return QPair<QString, QScriptValue>("QScriptValue(engine, false)", QScriptValue(engine, false));
    case 61: return QPair<QString, QScriptValue>("QScriptValue(engine, int(122))", QScriptValue(engine, int(122)));
    case 62: return QPair<QString, QScriptValue>("QScriptValue(engine, uint(124))", QScriptValue(engine, uint(124)));
    case 63: return QPair<QString, QScriptValue>("QScriptValue(engine, 0)", QScriptValue(engine, 0));
    case 64: return QPair<QString, QScriptValue>("QScriptValue(engine, 0.0)", QScriptValue(engine, 0.0));
    case 65: return QPair<QString, QScriptValue>("QScriptValue(engine, 123.0)", QScriptValue(engine, 123.0));
    case 66: return QPair<QString, QScriptValue>("QScriptValue(engine, 6.37e-8)", QScriptValue(engine, 6.37e-8));
    case 67: return QPair<QString, QScriptValue>("QScriptValue(engine, -6.37e-8)", QScriptValue(engine, -6.37e-8));
    case 68: return QPair<QString, QScriptValue>("QScriptValue(engine, 0x43211234)", QScriptValue(engine, 0x43211234));
    case 69: return QPair<QString, QScriptValue>("QScriptValue(engine, 0x10000)", QScriptValue(engine, 0x10000));
    case 70: return QPair<QString, QScriptValue>("QScriptValue(engine, 0x10001)", QScriptValue(engine, 0x10001));
    case 71: return QPair<QString, QScriptValue>("QScriptValue(engine, qSNaN())", QScriptValue(engine, qSNaN()));
    case 72: return QPair<QString, QScriptValue>("QScriptValue(engine, qQNaN())", QScriptValue(engine, qQNaN()));
    case 73: return QPair<QString, QScriptValue>("QScriptValue(engine, qInf())", QScriptValue(engine, qInf()));
    case 74: return QPair<QString, QScriptValue>("QScriptValue(engine, -qInf())", QScriptValue(engine, -qInf()));
    case 75: return QPair<QString, QScriptValue>("QScriptValue(engine, \"NaN\")", QScriptValue(engine, "NaN"));
    case 76: return QPair<QString, QScriptValue>("QScriptValue(engine, \"Infinity\")", QScriptValue(engine, "Infinity"));
    case 77: return QPair<QString, QScriptValue>("QScriptValue(engine, \"-Infinity\")", QScriptValue(engine, "-Infinity"));
    case 78: return QPair<QString, QScriptValue>("QScriptValue(engine, \"ciao\")", QScriptValue(engine, "ciao"));
    case 79: return QPair<QString, QScriptValue>("QScriptValue(engine, QString::fromLatin1(\"ciao\"))", QScriptValue(engine, QString::fromLatin1("ciao")));
    case 80: return QPair<QString, QScriptValue>("QScriptValue(engine, QString(\"\"))", QScriptValue(engine, QString("")));
    case 81: return QPair<QString, QScriptValue>("QScriptValue(engine, QString())", QScriptValue(engine, QString()));
    case 82: return QPair<QString, QScriptValue>("QScriptValue(engine, QString(\"0\"))", QScriptValue(engine, QString("0")));
    case 83: return QPair<QString, QScriptValue>("QScriptValue(engine, QString(\"123\"))", QScriptValue(engine, QString("123")));
    case 84: return QPair<QString, QScriptValue>("QScriptValue(engine, QString(\"1.23\"))", QScriptValue(engine, QString("1.23")));
    case 85: return QPair<QString, QScriptValue>("engine->evaluate(\"[]\")", engine->evaluate("[]"));
    case 86: return QPair<QString, QScriptValue>("engine->evaluate(\"{}\")", engine->evaluate("{}"));
    case 87: return QPair<QString, QScriptValue>("engine->evaluate(\"Object.prototype\")", engine->evaluate("Object.prototype"));
    case 88: return QPair<QString, QScriptValue>("engine->evaluate(\"Date.prototype\")", engine->evaluate("Date.prototype"));
    case 89: return QPair<QString, QScriptValue>("engine->evaluate(\"Array.prototype\")", engine->evaluate("Array.prototype"));
    case 90: return QPair<QString, QScriptValue>("engine->evaluate(\"Function.prototype\")", engine->evaluate("Function.prototype"));
    case 91: return QPair<QString, QScriptValue>("engine->evaluate(\"Error.prototype\")", engine->evaluate("Error.prototype"));
    case 92: return QPair<QString, QScriptValue>("engine->evaluate(\"Object\")", engine->evaluate("Object"));
    case 93: return QPair<QString, QScriptValue>("engine->evaluate(\"Array\")", engine->evaluate("Array"));
    case 94: return QPair<QString, QScriptValue>("engine->evaluate(\"Number\")", engine->evaluate("Number"));
    case 95: return QPair<QString, QScriptValue>("engine->evaluate(\"Function\")", engine->evaluate("Function"));
    case 96: return QPair<QString, QScriptValue>("engine->evaluate(\"(function() { return 1; })\")", engine->evaluate("(function() { return 1; })"));
    case 97: return QPair<QString, QScriptValue>("engine->evaluate(\"(function() { return 'ciao'; })\")", engine->evaluate("(function() { return 'ciao'; })"));
    case 98: return QPair<QString, QScriptValue>("engine->evaluate(\"(function() { throw new Error('foo'); })\")", engine->evaluate("(function() { throw new Error('foo'); })"));
    case 99: return QPair<QString, QScriptValue>("engine->evaluate(\"/foo/\")", engine->evaluate("/foo/"));
    case 100: return QPair<QString, QScriptValue>("engine->evaluate(\"new Object()\")", engine->evaluate("new Object()"));
    case 101: return QPair<QString, QScriptValue>("engine->evaluate(\"new Array()\")", engine->evaluate("new Array()"));
    case 102: return QPair<QString, QScriptValue>("engine->evaluate(\"new Error()\")", engine->evaluate("new Error()"));
    case 103: return QPair<QString, QScriptValue>("engine->evaluate(\"a = new Object(); a.foo = 22; a.foo\")", engine->evaluate("a = new Object(); a.foo = 22; a.foo"));
    case 104: return QPair<QString, QScriptValue>("engine->evaluate(\"Undefined\")", engine->evaluate("Undefined"));
    case 105: return QPair<QString, QScriptValue>("engine->evaluate(\"Null\")", engine->evaluate("Null"));
    case 106: return QPair<QString, QScriptValue>("engine->evaluate(\"True\")", engine->evaluate("True"));
    case 107: return QPair<QString, QScriptValue>("engine->evaluate(\"False\")", engine->evaluate("False"));
    case 108: return QPair<QString, QScriptValue>("engine->evaluate(\"undefined\")", engine->evaluate("undefined"));
    case 109: return QPair<QString, QScriptValue>("engine->evaluate(\"null\")", engine->evaluate("null"));
    case 110: return QPair<QString, QScriptValue>("engine->evaluate(\"true\")", engine->evaluate("true"));
    case 111: return QPair<QString, QScriptValue>("engine->evaluate(\"false\")", engine->evaluate("false"));
    case 112: return QPair<QString, QScriptValue>("engine->evaluate(\"122\")", engine->evaluate("122"));
    case 113: return QPair<QString, QScriptValue>("engine->evaluate(\"124\")", engine->evaluate("124"));
    case 114: return QPair<QString, QScriptValue>("engine->evaluate(\"0\")", engine->evaluate("0"));
    case 115: return QPair<QString, QScriptValue>("engine->evaluate(\"0.0\")", engine->evaluate("0.0"));
    case 116: return QPair<QString, QScriptValue>("engine->evaluate(\"123.0\")", engine->evaluate("123.0"));
    case 117: return QPair<QString, QScriptValue>("engine->evaluate(\"6.37e-8\")", engine->evaluate("6.37e-8"));
    case 118: return QPair<QString, QScriptValue>("engine->evaluate(\"-6.37e-8\")", engine->evaluate("-6.37e-8"));
    case 119: return QPair<QString, QScriptValue>("engine->evaluate(\"0x43211234\")", engine->evaluate("0x43211234"));
    case 120: return QPair<QString, QScriptValue>("engine->evaluate(\"0x10000\")", engine->evaluate("0x10000"));
    case 121: return QPair<QString, QScriptValue>("engine->evaluate(\"0x10001\")", engine->evaluate("0x10001"));
    case 122: return QPair<QString, QScriptValue>("engine->evaluate(\"NaN\")", engine->evaluate("NaN"));
    case 123: return QPair<QString, QScriptValue>("engine->evaluate(\"Infinity\")", engine->evaluate("Infinity"));
    case 124: return QPair<QString, QScriptValue>("engine->evaluate(\"-Infinity\")", engine->evaluate("-Infinity"));
    case 125: return QPair<QString, QScriptValue>("engine->evaluate(\"'ciao'\")", engine->evaluate("'ciao'"));
    case 126: return QPair<QString, QScriptValue>("engine->evaluate(\"''\")", engine->evaluate("''"));
    case 127: return QPair<QString, QScriptValue>("engine->evaluate(\"'0'\")", engine->evaluate("'0'"));
    case 128: return QPair<QString, QScriptValue>("engine->evaluate(\"'123'\")", engine->evaluate("'123'"));
    case 129: return QPair<QString, QScriptValue>("engine->evaluate(\"'12.4'\")", engine->evaluate("'12.4'"));
    case 130: return QPair<QString, QScriptValue>("engine->nullValue()", engine->nullValue());
    case 131: return QPair<QString, QScriptValue>("engine->undefinedValue()", engine->undefinedValue());
    case 132: return QPair<QString, QScriptValue>("engine->newObject()", engine->newObject());
    case 133: return QPair<QString, QScriptValue>("engine->newArray()", engine->newArray());
    case 134: return QPair<QString, QScriptValue>("engine->newArray(10)", engine->newArray(10));
    }
    Q_ASSERT(false);
    return qMakePair(QString(), QScriptValue());
}
