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

#ifndef qscriptfunction_p_h
#define qscriptfunction_p_h

#include "config.h"

#include "qscriptengine.h"
#include "qscriptvalue_p.h"

extern JSClassDefinition qt_NativeFunctionClass;
extern JSClassDefinition qt_NativeFunctionWithArgClass;

struct QNativeFunctionData
{
    QNativeFunctionData(QScriptEnginePrivate* engine, QScriptEngine::FunctionSignature fun)
        : engine(engine)
        , fun(fun)
    {
    }

    QScriptEnginePrivate* engine;
    QScriptEngine::FunctionSignature fun;
};

struct QNativeFunctionWithArgData
{
    QNativeFunctionWithArgData(QScriptEnginePrivate* engine, QScriptEngine::FunctionWithArgSignature fun, void* arg)
        : engine(engine)
        , fun(fun)
        , arg(arg)
    {
    }

    QScriptEnginePrivate* engine;
    QScriptEngine::FunctionWithArgSignature fun;
    void* arg;
};

#endif
