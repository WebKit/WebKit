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

#include "config.h"

#include "qscriptengine_p.h"

#include "qscriptprogram_p.h"
#include "qscriptvalue_p.h"

/*!
    Constructs a default QScriptEnginePrivate object, a new global context will be created.
    \internal
*/
QScriptEnginePrivate::QScriptEnginePrivate(const QScriptEngine* engine)
    : q_ptr(const_cast<QScriptEngine*>(engine))
    , m_context(JSGlobalContextCreate(0))
    , m_exception(0)
{
}

QScriptEnginePrivate::~QScriptEnginePrivate()
{
    if (m_exception)
        JSValueUnprotect(m_context, m_exception);
    JSGlobalContextRelease(m_context);
}

QScriptSyntaxCheckResultPrivate* QScriptEnginePrivate::checkSyntax(const QString& program)
{
    JSValueRef exception;
    JSStringRef source = QScriptConverter::toString(program);
    bool syntaxIsCorrect = JSCheckScriptSyntax(m_context, source, /* url */ 0, /* starting line */ 1, &exception);
    JSStringRelease(source);
    if (syntaxIsCorrect) {
        return new QScriptSyntaxCheckResultPrivate(this);
    }
    JSValueProtect(m_context, exception);
    return new QScriptSyntaxCheckResultPrivate(this, const_cast<JSObjectRef>(exception));
}

/*!
    Evaluates program and returns the result of the evaluation.
    \internal
*/
QScriptValuePrivate* QScriptEnginePrivate::evaluate(const QString& program, const QString& fileName, int lineNumber)
{
    JSStringRef script = QScriptConverter::toString(program);
    JSStringRef file = QScriptConverter::toString(fileName);
    QScriptValuePrivate* result = new QScriptValuePrivate(this, evaluate(script, file, lineNumber));
    JSStringRelease(script);
    JSStringRelease(file);
    return result;
}

/*!
    Evaluates program and returns the result of the evaluation.
    \internal
*/
QScriptValuePrivate* QScriptEnginePrivate::evaluate(const QScriptProgramPrivate* program)
{
    if (program->isNull())
        return new QScriptValuePrivate;
    return new QScriptValuePrivate(this, evaluate(*program, program->file(), program->line()));
}

QScriptValuePrivate* QScriptEnginePrivate::uncaughtException() const
{
    return m_exception ? new QScriptValuePrivate(this, m_exception) : new QScriptValuePrivate();
}

QScriptValuePrivate* QScriptEnginePrivate::newObject() const
{
    return new QScriptValuePrivate(this, JSObjectMake(m_context, /* jsClass */ 0, /* userData */ 0));
}

QScriptValuePrivate* QScriptEnginePrivate::newArray(uint length)
{
    JSValueRef exception = 0;
    JSObjectRef array = JSObjectMakeArray(m_context, /* argumentCount */ 0, /* arguments */ 0, &exception);

    if (!exception) {
        if (length > 0) {
            JSRetainPtr<JSStringRef> lengthRef(Adopt, JSStringCreateWithUTF8CString("length"));
            // array is an Array instance, so an exception should not occure here.
            JSObjectSetProperty(m_context, array, lengthRef.get(), JSValueMakeNumber(m_context, length), kJSPropertyAttributeNone, /* exception */ 0);
        }
    } else {
        setException(exception, NotNullException);
        return new QScriptValuePrivate();
    }

    return new QScriptValuePrivate(this, array);
}

QScriptValuePrivate* QScriptEnginePrivate::globalObject() const
{
    JSObjectRef globalObject = JSContextGetGlobalObject(m_context);
    return new QScriptValuePrivate(this, globalObject);
}
