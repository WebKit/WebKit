/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA 
 *
 */

#include "config.h"
#include "JSLock.h"

#include "collector.h"
#include "ExecState.h"

#if USE(MULTIPLE_THREADS)
#include <pthread.h>
#endif

namespace KJS {

JSLock::JSLock(ExecState*)
    : m_lockingForReal(false)
{
}

// If threading support is off, set the lock count to a constant value of 1 so assertions
// that the lock is held don't fail
intptr_t JSLock::lockCount()
{
    return 1;
}

bool JSLock::currentThreadIsHoldingLock()
{
    return true;
}

void JSLock::lock(bool)
{
}

void JSLock::unlock(bool)
{
}

void JSLock::lock(ExecState*)
{
}

void JSLock::unlock(ExecState*)
{
}

JSLock::DropAllLocks::DropAllLocks(ExecState*)
{
}

JSLock::DropAllLocks::DropAllLocks(bool)
{
}

JSLock::DropAllLocks::~DropAllLocks()
{
}

} // namespace KJS
