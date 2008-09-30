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
#include "JSString.h"
#include "ScopeChainMark.h"

namespace JSC {

ExecState::ExecState(JSGlobalObject* globalObject, Register* callFrame)
    : m_globalObject(globalObject)
    , m_exception(0)
    , m_globalData(globalObject->globalData())
    , m_callFrame(callFrame)
{
}

ExecState::ExecState(ExecState* exec, Register* callFrame)
    : m_globalObject(exec->m_globalObject)
    , m_exception(0)
    , m_globalData(exec->m_globalData)
    , m_callFrame(callFrame)
{
    ASSERT(!exec->m_exception);
}

} // namespace JSC
