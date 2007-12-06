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
ExecState::ExecState(JSGlobalObject* globalObject, JSObject* thisV, 
                     FunctionBodyNode* currentBody, CodeType type, ExecState* callingExec, ExecState* currentExec,
                     FunctionImp* func, const List* args)
    : m_globalObject(globalObject)
    , m_exception(0)
    , m_propertyNames(CommonIdentifiers::shared())
    , m_callingExec(callingExec)
    , m_savedExec(currentExec)
    , m_currentBody(currentBody)
    , m_function(func)
    , m_arguments(args)
    , m_iterationDepth(0)
    , m_switchDepth(0) 
    , m_codeType(type)
{
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
        if (m_callingExec) {
            m_scopeChain = m_callingExec->scopeChain();
            m_variable = m_callingExec->variableObject();
            m_thisVal = m_callingExec->thisValue();
            break;
        } // else same as GlobalCode
    case GlobalCode:
        m_scopeChain.push(globalObject);
        m_thisVal = globalObject;
        break;
    case FunctionCode:
        m_scopeChain = func->scope();
        m_scopeChain.push(m_activation);
        m_variable = m_activation; // TODO: DontDelete ? (ECMA 10.2.3)
        m_thisVal = thisV;
        break;
    }

    if (currentBody)
        m_globalObject->setCurrentExec(this);
}

ExecState::~ExecState()
{
    m_globalObject->setCurrentExec(m_savedExec);
}

void ExecState::mark()
{
    for (ExecState* exec = this; exec; exec = exec->m_callingExec)
        exec->m_scopeChain.mark();
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
    
void ExecState::updateLocalStorage() 
{
    m_localStorageBuffer = static_cast<ActivationImp*>(m_activation)->localStorage().data(); 
}

} // namespace KJS
