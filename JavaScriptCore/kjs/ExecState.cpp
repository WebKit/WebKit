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

#include "JSGlobalObject.h"
#include "JSFunction.h"
#include "JSString.h"
#include "ScopeChainMark.h"

namespace KJS {

ExecState::ExecState(JSGlobalObject* globalObject, JSObject* globalThisValue, ScopeChainNode* globalScopeChain)
    : m_globalObject(globalObject)
    , m_globalThisValue(globalThisValue)
    , m_exception(0)
    , m_globalData(globalObject->globalData())
    , m_prev(0)
    , m_registerFile(0)
    , m_scopeChain(globalScopeChain)
    , m_callFrameOffset(-1)
{
}

ExecState::ExecState(ExecState* exec, RegisterFile* registerFile, ScopeChainNode* scopeChain, int callFrameOffset)
    : m_globalObject(exec->m_globalObject)
    , m_globalThisValue(exec->m_globalThisValue)
    , m_exception(0)
    , m_globalData(exec->m_globalData)
    , m_prev(exec)
    , m_registerFile(registerFile)
    , m_scopeChain(scopeChain)
    , m_callFrameOffset(callFrameOffset)
{
    ASSERT(!exec->m_exception);
}

bool ExecState::isGlobalObject(JSObject* o) const
{
    return o->isGlobalObject();
}

} // namespace KJS
