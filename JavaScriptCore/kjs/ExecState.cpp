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

namespace KJS {

static inline List* globalEmptyList()
{
    static List staticEmptyList;
    return &staticEmptyList;
}

// ECMA 10.2

// The constructor for the globalExec pseudo-ExecState
inline ExecState::ExecState(JSGlobalObject* globalObject)
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
    , m_variableObject(globalObject)
    , m_thisValue(globalObject)
    , m_iterationDepth(0)
    , m_switchDepth(0) 
    , m_codeType(GlobalCode)
{
    m_scopeChain.push(globalObject);
}

inline ExecState::ExecState(JSGlobalObject* globalObject, JSObject* /*thisObject*/, ProgramNode* programNode)
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
    , m_variableObject(globalObject)
    , m_thisValue(globalObject)
    , m_iterationDepth(0)
    , m_switchDepth(0) 
    , m_codeType(GlobalCode)
{
    // FIXME: This function ignores the "thisObject" parameter, which means that the API for evaluating
    // a script with a this object that's not the same as the global object is broken, and probably
    // has been for some time.
    ASSERT(m_scopeNode);
    m_scopeChain.push(globalObject);
}

inline ExecState::ExecState(JSGlobalObject* globalObject, EvalNode* evalNode, ExecState* callingExec)
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
    , m_scopeChain(callingExec->m_scopeChain)
    , m_variableObject(callingExec->m_variableObject)
    , m_thisValue(callingExec->m_thisValue)
    , m_iterationDepth(0)
    , m_switchDepth(0) 
    , m_codeType(EvalCode)
{    
    ASSERT(m_scopeNode);
}

inline ExecState::ExecState(JSGlobalObject* globalObject, JSObject* thisObject, 
         FunctionBodyNode* functionBodyNode, ExecState* callingExec,
         FunctionImp* func, const List& args)
    : m_globalObject(globalObject)
    , m_exception(0)
    , m_propertyNames(callingExec->m_propertyNames)
    , m_emptyList(callingExec->m_emptyList)
    , m_callingExec(callingExec)
    , m_scopeNode(functionBodyNode)
    , m_function(func)
    , m_arguments(&args)
    , m_scopeChain(func->scope())
    , m_thisValue(thisObject)
    , m_iterationDepth(0)
    , m_switchDepth(0) 
    , m_codeType(FunctionCode)
{
    ASSERT(m_scopeNode);

    ActivationImp* activation = globalObject->pushActivation(this);
    m_activation = activation;
    m_localStorage = &activation->localStorage();
    m_variableObject = activation;
    m_scopeChain.push(activation);
}

inline ExecState::~ExecState()
{
}

JSGlobalObject* ExecState::lexicalGlobalObject() const
{
    JSObject* object = m_scopeChain.bottom();
    if (object && object->isGlobalObject())
        return static_cast<JSGlobalObject*>(object);
    return m_globalObject;
}

void ExecState::markActiveExecStates() 
{
    ExecStateStack::const_iterator end = activeExecStates().end();
    for (ExecStateStack::const_iterator it = activeExecStates().begin(); it != end; ++it)
        (*it)->m_scopeChain.mark();
}

static inline ExecStateStack& inlineActiveExecStates()
{
    static ExecStateStack staticActiveExecStates;
    return staticActiveExecStates;
}

ExecStateStack& ExecState::activeExecStates()
{
    return inlineActiveExecStates();
}

GlobalExecState::GlobalExecState(JSGlobalObject* globalObject)
    : ExecState(globalObject)
{
}

GlobalExecState::~GlobalExecState()
{
}

InterpreterExecState::InterpreterExecState(JSGlobalObject* globalObject, JSObject* thisObject, ProgramNode* programNode)
    : ExecState(globalObject, thisObject, programNode)
{
    inlineActiveExecStates().append(this);
}

InterpreterExecState::~InterpreterExecState()
{
    ASSERT(inlineActiveExecStates().last() == this);
    inlineActiveExecStates().removeLast();
}

EvalExecState::EvalExecState(JSGlobalObject* globalObject, EvalNode* evalNode, ExecState* callingExec)
    : ExecState(globalObject, evalNode, callingExec)
{
    inlineActiveExecStates().append(this);
}

EvalExecState::~EvalExecState()
{
    ASSERT(inlineActiveExecStates().last() == this);
    inlineActiveExecStates().removeLast();
}

FunctionExecState::FunctionExecState(JSGlobalObject* globalObject, JSObject* thisObject, 
         FunctionBodyNode* functionBodyNode, ExecState* callingExec,
         FunctionImp* func, const List& args)
    : ExecState(globalObject, thisObject, functionBodyNode, callingExec, func, args)
{
    inlineActiveExecStates().append(this);
}

FunctionExecState::~FunctionExecState()
{
    ASSERT(inlineActiveExecStates().last() == this);
    inlineActiveExecStates().removeLast();

    if (m_activation->needsPop())
        m_globalObject->popActivation();
}

} // namespace KJS
