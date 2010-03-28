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
#include <QtCore/qobject.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>

class QScriptValue;
class QScriptEnginePrivate;

// Internal typedef
typedef QExplicitlySharedDataPointer<QScriptEnginePrivate> QScriptEnginePtr;

class QScriptEngine : public QObject {
public:
    QScriptEngine();
    ~QScriptEngine();

    static QScriptSyntaxCheckResult checkSyntax(const QString& program);
    QScriptValue evaluate(const QString& program, const QString& fileName = QString(), int lineNumber = 1);
    QScriptValue evaluate(const QScriptProgram& program);

    void collectGarbage();
    void reportAdditionalMemoryCost(int cost);

    QScriptString toStringHandle(const QString& str);

    QScriptValue nullValue();
    QScriptValue undefinedValue();
    QScriptValue globalObject() const;
private:
    friend class QScriptEnginePrivate;

    QScriptEnginePtr d_ptr;
};

#endif
