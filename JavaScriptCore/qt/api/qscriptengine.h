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

#ifndef qscriptengine_h
#define qscriptengine_h

#include "qscriptprogram.h"
#include "qscriptstring.h"
#include "qscriptsyntaxcheckresult.h"
#include "qscriptvalue.h"
#include <QtCore/qobject.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>

class QDateTime;
class QScriptEnginePrivate;

// FIXME: Remove this once QScriptContext is properly defined.
typedef void QScriptContext;

// Internal typedef
typedef QExplicitlySharedDataPointer<QScriptEnginePrivate> QScriptEnginePtr;

class QScriptEngine : public QObject {
public:
    QScriptEngine();
    ~QScriptEngine();

    static QScriptSyntaxCheckResult checkSyntax(const QString& program);
    QScriptValue evaluate(const QString& program, const QString& fileName = QString(), int lineNumber = 1);
    QScriptValue evaluate(const QScriptProgram& program);

    bool hasUncaughtException() const;
    QScriptValue uncaughtException() const;
    void clearExceptions();
    int uncaughtExceptionLineNumber() const;
    QStringList uncaughtExceptionBacktrace() const;

    void collectGarbage();
    void reportAdditionalMemoryCost(int cost);

    QScriptString toStringHandle(const QString& str);
    QScriptValue toObject(const QScriptValue& value);

    QScriptValue nullValue();
    QScriptValue undefinedValue();

    typedef QScriptValue (*FunctionSignature)(QScriptContext *, QScriptEngine *);
    typedef QScriptValue (*FunctionWithArgSignature)(QScriptContext *, QScriptEngine *, void *);

    QScriptValue newFunction(FunctionSignature fun, int length = 0);
    QScriptValue newFunction(FunctionSignature fun, const QScriptValue& prototype, int length = 0);
    QScriptValue newFunction(FunctionWithArgSignature fun, void* arg);

    QScriptValue newObject();
    QScriptValue newArray(uint length = 0);
    QScriptValue newDate(qsreal value);
    QScriptValue newDate(const QDateTime& value);
    QScriptValue globalObject() const;
private:
    friend class QScriptEnginePrivate;

    QScriptEnginePtr d_ptr;
};

#endif
