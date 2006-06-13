// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2006 Apple Computer, Inc.
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

#include "context.h"

namespace KJS {

// ECMA 10.2
Context::Context(JSObject* glob, Interpreter* interpreter, JSObject* thisV, 
                 FunctionBodyNode* currentBody, CodeType type, Context* callingCon, 
                 FunctionImp* func, const List* args)
    : m_interpreter(interpreter)
    , m_currentBody(currentBody)
    , m_function(func)
    , m_arguments(args)
    , m_iterationDepth(0)
    , m_switchDepth(0) 
{
    m_codeType = type;
    m_callingContext = callingCon;
    
    // create and initialize activation object (ECMA 10.1.6)
    if (type == FunctionCode || type == AnonymousCode ) {
        m_activation = new ActivationImp(func, *args);
        m_variable = m_activation;
    } else {
        m_activation = 0;
        m_variable = glob;
    }
    
    // ECMA 10.2
    switch(type) {
    case EvalCode:
        if (m_callingContext) {
            scope = m_callingContext->scopeChain();
            m_variable = m_callingContext->variableObject();
            m_thisVal = m_callingContext->thisValue();
            break;
        } // else same as GlobalCode
    case GlobalCode:
        scope.clear();
        scope.push(glob);
        m_thisVal = static_cast<JSObject*>(glob);
        break;
    case FunctionCode:
    case AnonymousCode:
        if (type == FunctionCode) {
            scope = func->scope();
            scope.push(m_activation);
        } else {
            scope.clear();
            scope.push(glob);
            scope.push(m_activation);
        }
        m_variable = m_activation; // TODO: DontDelete ? (ECMA 10.2.3)
        m_thisVal = thisV;
        break;
    }
    
    m_interpreter->setContext(this);
}

Context::~Context()
{
    m_interpreter->setContext(m_callingContext);
}

void Context::mark()
{
    for (Context* context = this; context; context = context->m_callingContext)
        context->scope.mark();
}

} // namespace KJS
