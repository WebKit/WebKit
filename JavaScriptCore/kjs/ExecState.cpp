// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007 Apple Inc. All rights reserved.
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

ExecState::ExecState(JSGlobalObject* globalObject, JSObject* /*thisObject*/, ProgramNode* programNode)
    : m_globalObject(globalObject)
    , m_exception(0)
    , m_propertyNames(CommonIdentifiers::shared())
    , m_emptyList(globalEmptyList())
    , m_callingExec(0)
    , m_savedExec(0)
    , m_scopeNode(programNode)
    , m_function(0)
    , m_arguments(0)
    , m_activation(0)
    , m_localStorage(&globalObject->localStorage())
    , m_variableObject(globalObject)
    , m_thisVal(globalObject)
    , m_iterationDepth(0)
    , m_switchDepth(0) 
    , m_codeType(GlobalCode)
{
    // FIXME: This function ignores the "thisObject" parameter, which means that the API for evaluating
    // a script with a this object that's not the same as the global object is broken, and probably
    // has been for some time.
    m_scopeChain.push(globalObject);
    if (programNode)
        globalObject->setCurrentExec(this);
}

ExecState::ExecState(JSGlobalObject* globalObject, EvalNode* evalNode, ExecState* callingExec)
    : m_globalObject(globalObject)
    , m_exception(0)
    , m_propertyNames(callingExec->m_propertyNames)
    , m_emptyList(callingExec->m_emptyList)
    , m_callingExec(callingExec)
    , m_savedExec(globalObject->currentExec())
    , m_scopeNode(evalNode)
    , m_function(0)
    , m_arguments(0)
    , m_activation(0)
    , m_localStorage(callingExec->m_localStorage)
    , m_scopeChain(callingExec->m_scopeChain)
    , m_variableObject(callingExec->m_variableObject)
    , m_thisVal(callingExec->m_thisVal)
    , m_iterationDepth(0)
    , m_switchDepth(0) 
    , m_codeType(EvalCode)
{    
    globalObject->setCurrentExec(this);
}

ExecState::ExecState(JSGlobalObject* globalObject, JSObject* thisObject, 
                     FunctionBodyNode* functionBodyNode, ExecState* callingExec,
                     FunctionImp* func, const List& args)
    : m_globalObject(globalObject)
    , m_exception(0)
    , m_propertyNames(callingExec->m_propertyNames)
    , m_emptyList(callingExec->m_emptyList)
    , m_callingExec(callingExec)
    , m_savedExec(globalObject->currentExec())
    , m_scopeNode(functionBodyNode)
    , m_function(func)
    , m_arguments(&args)
    , m_scopeChain(func->scope())
    , m_thisVal(thisObject)
    , m_iterationDepth(0)
    , m_switchDepth(0) 
    , m_codeType(FunctionCode)
{
    ActivationImp* activation = globalObject->pushActivation(this);
    m_activation = activation;
    m_localStorage = &activation->localStorage();
    m_variableObject = activation;
    m_scopeChain.push(activation);
    globalObject->setCurrentExec(this);
}

ExecState::~ExecState()
{
    if (m_codeType == FunctionCode && m_activation->needsPop())
        m_globalObject->popActivation();
    
    m_globalObject->setCurrentExec(m_savedExec);
}

void ExecState::mark()
{
    for (ExecState* exec = this; exec; exec = exec->m_callingExec)
        exec->m_scopeChain.mark();

    // FIXME: It is surprising that this code is necessary, since at first
    // glance it seems that all ActivationImps should be in a ScopeChain.
    // However, <http://bugs.webkit.org/show_bug.cgi?id=16871> proves that is
    // not the case.
    if (m_activation && m_activation->isOnStack())
        m_activation->markChildren();
}

JSGlobalObject* ExecState::lexicalGlobalObject() const
{
    if (scopeChain().isEmpty())
        return dynamicGlobalObject();
    
    JSObject* object = scopeChain().bottom();
    if (object && object->isGlobalObject())
        return static_cast<JSGlobalObject*>(object);

    return dynamicGlobalObject();
}

} // namespace KJS
