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
#include "qscriptoriginalglobalobject_p.h"
#include "qscriptstring_p.h"
#include "qscriptsyntaxcheckresult_p.h"
#include "qscriptvalue.h"
#include <JavaScriptCore/JavaScript.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JSBasePrivate.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

class QScriptEngine;
class QScriptSyntaxCheckResultPrivate;

class QScriptEnginePrivate : public QSharedData {
public:
    static QScriptEnginePrivate* get(const QScriptEngine* q) { Q_ASSERT(q); return q->d_ptr.data(); }
    static QScriptEngine* get(const QScriptEnginePrivate* d) { Q_ASSERT(d); return d->q_ptr; }

    QScriptEnginePrivate(const QScriptEngine*);
    ~QScriptEnginePrivate();

    enum SetExceptionFlag {
        IgnoreNullException = 0x01,
        NotNullException = 0x02,
    };

    QScriptSyntaxCheckResultPrivate* checkSyntax(const QString& program);
    QScriptValuePrivate* evaluate(const QString& program, const QString& fileName, int lineNumber);
    QScriptValuePrivate* evaluate(const QScriptProgramPrivate* program);
    inline JSValueRef evaluate(JSStringRef program, JSStringRef fileName, int lineNumber);

    inline bool hasUncaughtException() const;
    QScriptValuePrivate* uncaughtException() const;
    inline void clearExceptions();
    inline void setException(JSValueRef exception, const /* SetExceptionFlags */ unsigned flags = IgnoreNullException);
    inline int uncaughtExceptionLineNumber() const;
    inline QStringList uncaughtExceptionBacktrace() const;

    inline void collectGarbage();
    inline void reportAdditionalMemoryCost(int cost);

    inline JSValueRef makeJSValue(double number) const;
    inline JSValueRef makeJSValue(int number) const;
    inline JSValueRef makeJSValue(uint number) const;
    inline JSValueRef makeJSValue(const QString& string) const;
    inline JSValueRef makeJSValue(bool number) const;
    inline JSValueRef makeJSValue(QScriptValue::SpecialValue value) const;

    QScriptValuePrivate* newFunction(QScriptEngine::FunctionSignature fun, QScriptValuePrivate* prototype, int length);
    QScriptValuePrivate* newFunction(QScriptEngine::FunctionWithArgSignature fun, void* arg);
    QScriptValuePrivate* newFunction(JSObjectRef funObject, QScriptValuePrivate* prototype);

    QScriptValuePrivate* newObject() const;
    QScriptValuePrivate* newArray(uint length);
    QScriptValuePrivate* newDate(qsreal value);
    QScriptValuePrivate* globalObject() const;

    inline QScriptStringPrivate* toStringHandle(const QString& str) const;

    inline operator JSGlobalContextRef() const;

    inline bool isDate(JSValueRef value) const;
    inline bool isArray(JSValueRef value) const;
    inline bool isError(JSValueRef value) const;
    inline bool objectHasOwnProperty(JSObjectRef object, JSStringRef property) const;
    inline QVector<JSStringRef> objectGetOwnPropertyNames(JSObjectRef object) const;

private:
    QScriptEngine* q_ptr;
    JSGlobalContextRef m_context;
    JSValueRef m_exception;

    QScriptOriginalGlobalObject m_originalGlobalObject;

    JSClassRef m_nativeFunctionClass;
    JSClassRef m_nativeFunctionWithArgClass;
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
    if (!result) {
        setException(exception, NotNullException);
        return exception; // returns an exception
    }
    clearExceptions();
    return result;
}

bool QScriptEnginePrivate::hasUncaughtException() const
{
    return m_exception;
}

void QScriptEnginePrivate::clearExceptions()
{
    if (m_exception)
        JSValueUnprotect(m_context, m_exception);
    m_exception = 0;
}

void QScriptEnginePrivate::setException(JSValueRef exception, const /* SetExceptionFlags */ unsigned flags)
{
    if (!((flags & NotNullException) || exception))
        return;
    Q_ASSERT(exception);

    if (m_exception)
        JSValueUnprotect(m_context, m_exception);
    JSValueProtect(m_context, exception);
    m_exception = exception;
}

int QScriptEnginePrivate::uncaughtExceptionLineNumber() const
{
    if (!hasUncaughtException() || !JSValueIsObject(m_context, m_exception))
        return -1;

    JSValueRef exception = 0;
    JSRetainPtr<JSStringRef> lineNumberPropertyName(Adopt, QScriptConverter::toString("line"));
    JSValueRef lineNumber = JSObjectGetProperty(m_context, const_cast<JSObjectRef>(m_exception), lineNumberPropertyName.get(), &exception);
    int result = JSValueToNumber(m_context, lineNumber, &exception);
    return exception ? -1 : result;
}

QStringList QScriptEnginePrivate::uncaughtExceptionBacktrace() const
{
    if (!hasUncaughtException() || !JSValueIsObject(m_context, m_exception))
        return QStringList();

    JSValueRef exception = 0;
    JSRetainPtr<JSStringRef> fileNamePropertyName(Adopt, QScriptConverter::toString("sourceURL"));
    JSRetainPtr<JSStringRef> lineNumberPropertyName(Adopt, QScriptConverter::toString("line"));
    JSValueRef jsFileName = JSObjectGetProperty(m_context, const_cast<JSObjectRef>(m_exception), fileNamePropertyName.get(), &exception);
    JSValueRef jsLineNumber = JSObjectGetProperty(m_context, const_cast<JSObjectRef>(m_exception), lineNumberPropertyName.get(), &exception);
    JSRetainPtr<JSStringRef> fileName(Adopt, JSValueToStringCopy(m_context, jsFileName, &exception));
    int lineNumber = JSValueToNumber(m_context, jsLineNumber, &exception);
    return QStringList(QString::fromLatin1("<anonymous>()@%0:%1")
            .arg(QScriptConverter::toString(fileName.get()))
            .arg(QScriptConverter::toString(exception ? -1 : lineNumber)));
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

QScriptEnginePrivate::operator JSGlobalContextRef() const
{
    Q_ASSERT(this);
    return m_context;
}

bool QScriptEnginePrivate::isDate(JSValueRef value) const
{
    return m_originalGlobalObject.isDate(value);
}

bool QScriptEnginePrivate::isArray(JSValueRef value) const
{
    return m_originalGlobalObject.isArray(value);
}

bool QScriptEnginePrivate::isError(JSValueRef value) const
{
    return m_originalGlobalObject.isError(value);
}

inline bool QScriptEnginePrivate::objectHasOwnProperty(JSObjectRef object, JSStringRef property) const
{
    // FIXME We need a JSC C API function for this.
    return m_originalGlobalObject.objectHasOwnProperty(object, property);
}

inline QVector<JSStringRef> QScriptEnginePrivate::objectGetOwnPropertyNames(JSObjectRef object) const
{
    // FIXME We can't use C API function JSObjectGetPropertyNames as it returns only enumerable properties.
    return m_originalGlobalObject.objectGetOwnPropertyNames(object);
}

#endif
