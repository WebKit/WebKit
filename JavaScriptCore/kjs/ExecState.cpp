// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ExecState.h"

#include "Activation.h"
#include "JSGlobalObject.h"
#include "function.h"
#include "internal.h"
#include "scope_chain_mark.h"
#include "ExecStateInlines.h"

namespace KJS {

static inline List* globalEmptyList()
{
    static List staticEmptyList;
    return &staticEmptyList;
}

// ECMA 10.2

// The constructor for the globalExec pseudo-ExecState
inline ExecState::ExecState(JSGlobalObject* globalObject, JSObject* thisObject)
    : m_globalObject(globalObject)
    , m_exception(0)
    , m_propertyNames(CommonIdentifiers::shared())
    , m_emptyList(globalEmptyList())
    , m_callingExec(0)
    , m_scopeNode(0)
    , m_function(0)
    , m_arguments(0)
    , m_activation(0)
    , m_localStorage(&globalObject->localStorage())
    , m_inlineScopeChainNode(0, 0)
    , m_variableObject(globalObject)
    , m_thisValue(thisObject)
    , m_globalThisValue(thisObject)
    , m_iterationDepth(0)
    , m_switchDepth(0) 
    , m_codeType(GlobalCode)
{
    m_scopeChain.push(globalObject);
}

inline ExecState::ExecState(JSGlobalObject* globalObject, JSObject* thisObject, ProgramNode* programNode)
    : m_globalObject(globalObject)
    , m_exception(0)
    , m_propertyNames(CommonIdentifiers::shared())
    , m_emptyList(globalEmptyList())
    , m_callingExec(0)
    , m_scopeNode(programNode)
    , m_function(0)
    , m_arguments(0)
    , m_activation(0)
    , m_localStorage(&globalObject->localStorage())
    , m_inlineScopeChainNode(0, 0)
    , m_variableObject(globalObject)
    , m_thisValue(thisObject)
    , m_globalThisValue(thisObject)
    , m_iterationDepth(0)
    , m_switchDepth(0) 
    , m_codeType(GlobalCode)
{
    ASSERT(m_scopeNode);
    m_scopeChain.push(globalObject);
}

inline ExecState::ExecState(JSGlobalObject* globalObject, JSObject* thisObject, EvalNode* evalNode, ExecState* callingExec, const ScopeChain& scopeChain, JSVariableObject* variableObject)
    : m_globalObject(globalObject)
    , m_exception(0)
    , m_propertyNames(callingExec->m_propertyNames)
    , m_emptyList(callingExec->m_emptyList)
    , m_callingExec(callingExec)
    , m_scopeNode(evalNode)
    , m_function(0)
    , m_arguments(0)
    , m_activation(0)
    , m_localStorage(callingExec->m_localStorage)
    , m_scopeChain(scopeChain)
    , m_inlineScopeChainNode(0, 0)
    , m_variableObject(variableObject)
    , m_thisValue(thisObject)
    , m_globalThisValue(thisObject)
    , m_iterationDepth(0)
    , m_switchDepth(0) 
    , m_codeType(EvalCode)
{    
    ASSERT(m_scopeNode);
}

JSGlobalObject* ExecState::lexicalGlobalObject() const
{
    JSObject* object = m_scopeChain.bottom();
    if (object && object->isGlobalObject())
        return static_cast<JSGlobalObject*>(object);
    return m_globalObject;
}

GlobalExecState::GlobalExecState(JSGlobalObject* globalObject, JSObject* thisObject)
    : ExecState(globalObject, thisObject)
{
}

GlobalExecState::~GlobalExecState()
{
}

InterpreterExecState::InterpreterExecState(JSGlobalObject* globalObject, JSObject* thisObject, ProgramNode* programNode)
    : ExecState(globalObject, thisObject, programNode)
{
    m_globalObject->activeExecStates().append(this);
}

InterpreterExecState::~InterpreterExecState()
{
    ASSERT(m_globalObject->activeExecStates().last() == this);
    m_globalObject->activeExecStates().removeLast();
}

EvalExecState::EvalExecState(JSGlobalObject* globalObject, JSObject* thisObj, EvalNode* evalNode, ExecState* callingExec, const ScopeChain& scopeChain, JSVariableObject* variableObject)
    : ExecState(globalObject, thisObj, evalNode, callingExec, scopeChain, variableObject)
{
    m_globalObject->activeExecStates().append(this);
}

EvalExecState::~EvalExecState()
{
    ASSERT(m_globalObject->activeExecStates().last() == this);
    m_globalObject->activeExecStates().removeLast();
}
    
} // namespace KJS
