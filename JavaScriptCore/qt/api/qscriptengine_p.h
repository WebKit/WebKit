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

#ifndef qscriptengine_p_h
#define qscriptengine_p_h

#include "qscriptconverter_p.h"
#include "qscriptengine.h"
#include "qscriptstring_p.h"
#include "qscriptsyntaxcheckresult_p.h"
#include "qscriptvalue.h"
#include <JavaScriptCore/JavaScript.h>
#include <JSBasePrivate.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>

class QScriptEngine;
class QScriptSyntaxCheckResultPrivate;

class QScriptEnginePrivate : public QSharedData {
public:
    static QScriptEnginePrivate* get(const QScriptEngine* q) { Q_ASSERT(q); return q->d_ptr.data(); }
    static QScriptEngine* get(const QScriptEnginePrivate* d) { Q_ASSERT(d); return d->q_ptr; }

    QScriptEnginePrivate(const QScriptEngine*);
    ~QScriptEnginePrivate();

    QScriptSyntaxCheckResultPrivate* checkSyntax(const QString& program);
    QScriptValuePrivate* evaluate(const QString& program, const QString& fileName, int lineNumber);
    QScriptValuePrivate* evaluate(const QScriptProgramPrivate* program);
    inline JSValueRef evaluate(JSStringRef program, JSStringRef fileName, int lineNumber);

    inline void collectGarbage();
    inline void reportAdditionalMemoryCost(int cost);

    inline JSValueRef makeJSValue(double number) const;
    inline JSValueRef makeJSValue(int number) const;
    inline JSValueRef makeJSValue(uint number) const;
    inline JSValueRef makeJSValue(const QString& string) const;
    inline JSValueRef makeJSValue(bool number) const;
    inline JSValueRef makeJSValue(QScriptValue::SpecialValue value) const;

    QScriptValuePrivate* globalObject() const;

    inline QScriptStringPrivate* toStringHandle(const QString& str) const;

    inline JSGlobalContextRef context() const;
private:
    QScriptEngine* q_ptr;
    JSGlobalContextRef m_context;
};


/*!
  Evaluates given JavaScript program and returns result of the evaluation.
  \attention this function doesn't take ownership of the parameters.
  \internal
*/
JSValueRef QScriptEnginePrivate::evaluate(JSStringRef program, JSStringRef fileName, int lineNumber)
{
    JSValueRef exception;
    JSValueRef result = JSEvaluateScript(m_context, program, /* Global Object */ 0, fileName, lineNumber, &exception);
    if (!result)
        return exception; // returns an exception
    return result;
}

void QScriptEnginePrivate::collectGarbage()
{
    JSGarbageCollect(m_context);
}

void QScriptEnginePrivate::reportAdditionalMemoryCost(int cost)
{
    if (cost > 0)
        JSReportExtraMemoryCost(m_context, cost);
}

JSValueRef QScriptEnginePrivate::makeJSValue(double number) const
{
    return JSValueMakeNumber(m_context, number);
}

JSValueRef QScriptEnginePrivate::makeJSValue(int number) const
{
    return JSValueMakeNumber(m_context, number);
}

JSValueRef QScriptEnginePrivate::makeJSValue(uint number) const
{
    return JSValueMakeNumber(m_context, number);
}

JSValueRef QScriptEnginePrivate::makeJSValue(const QString& string) const
{
    JSStringRef tmp = QScriptConverter::toString(string);
    JSValueRef result = JSValueMakeString(m_context, tmp);
    JSStringRelease(tmp);
    return result;
}

JSValueRef QScriptEnginePrivate::makeJSValue(bool value) const
{
    return JSValueMakeBoolean(m_context, value);
}

JSValueRef QScriptEnginePrivate::makeJSValue(QScriptValue::SpecialValue value) const
{
    if (value == QScriptValue::NullValue)
        return JSValueMakeNull(m_context);
    return JSValueMakeUndefined(m_context);
}

QScriptStringPrivate* QScriptEnginePrivate::toStringHandle(const QString& str) const
{
    return new QScriptStringPrivate(str);
}

JSGlobalContextRef QScriptEnginePrivate::context() const
{
    return m_context;
}

#endif
