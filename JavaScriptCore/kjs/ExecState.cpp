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

#include "JSGlobalObject.h"
#include "function.h"
#include "internal.h"

namespace KJS {


// ECMA 10.2
ExecState::ExecState(Interpreter* interpreter, JSGlobalObject* globalObject, JSObject* thisV, 
                     FunctionBodyNode* currentBody, CodeType type, ExecState* callingExec, 
                     FunctionImp* func, const List* args)
    : m_interpreter(interpreter)
    , m_exception(0)
    , m_propertyNames(CommonIdentifiers::shared())
    , m_savedExecState(interpreter->currentExec())
    , m_currentBody(currentBody)
    , m_function(func)
    , m_arguments(args)
    , m_iterationDepth(0)
    , m_switchDepth(0) 
{
    m_codeType = type;
    m_callingExecState = callingExec;
    
    // create and initialize activation object (ECMA 10.1.6)
    if (type == FunctionCode) {
        m_activation = new ActivationImp(this);
        m_variable = m_activation;
    } else {
        m_activation = 0;
        m_variable = globalObject;
    }
    
    // ECMA 10.2
    switch(type) {
    case EvalCode:
        if (m_callingExecState) {
            m_scopeChain = m_callingExecState->scopeChain();
            m_variable = m_callingExecState->variableObject();
            m_thisVal = m_callingExecState->thisValue();
            break;
        } // else same as GlobalCode
    case GlobalCode:
        if (globalObject)
            setGlobalObject(globalObject);
        break;
    case FunctionCode:
        m_scopeChain = func->scope();
        m_scopeChain.push(m_activation);
        m_variable = m_activation; // TODO: DontDelete ? (ECMA 10.2.3)
        m_thisVal = thisV;
        break;
    }

    if (currentBody)
        m_interpreter->setCurrentExec(this);
}

ExecState::~ExecState()
{
    m_interpreter->setCurrentExec(m_savedExecState);
}

void ExecState::markChildren(MarkStack& stack)
{
    for (ExecState* exec = this; exec; exec = exec->m_callingExecState)
        exec->m_scopeChain.markChildren(stack);
}

void ExecState::setGlobalObject(JSGlobalObject* globalObject)
{
    m_scopeChain.clear();
    m_scopeChain.push(globalObject);
    m_thisVal = static_cast<JSObject*>(globalObject);
}

Interpreter* ExecState::lexicalInterpreter() const
{
    if (scopeChain().isEmpty())
        return dynamicInterpreter();
    
    JSObject* object = scopeChain().bottom();
    if (object && object->isGlobalObject())
        return static_cast<JSGlobalObject*>(object)->interpreter();

    return dynamicInterpreter();
}
    
void ExecState::updateLocalStorage() 
{
    m_localStorageBuffer = static_cast<ActivationImp*>(m_activation)->localStorage().data(); 
}

} // namespace KJS
