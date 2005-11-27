// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * This file is part of the KDE libraries
 * Copyright (C) 2005 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "JSLock.h"

#include "collector.h"

namespace KJS {

#if KJS_MULTIPLE_THREADS

static pthread_once_t interpreterLockOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t interpreterLock;
static int interpreterLockCount = 0;

static void initializeJSLock()
{
  pthread_mutexattr_t attr;

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);

  pthread_mutex_init(&interpreterLock, &attr);
}

void JSLock::lock()
{
  pthread_once(&interpreterLockOnce, initializeJSLock);
  pthread_mutex_lock(&interpreterLock);
  interpreterLockCount++;
  Collector::registerThread();

  // FIXME: Hack-o-rama. To prevent construction of a global object with a null prototype (4342216),
  // we need to intialize our constants before the first object is constructed. InterpreterImp::lock()
  // is a good place to do this because you have to call it before doing any allocations. Once we change our 
  // implementation to use immediate values, we should remove this code.
  ConstantValues::initIfNeeded();
}

void JSLock::unlock()
{
  interpreterLockCount--;
  pthread_mutex_unlock(&interpreterLock);
}

#else

// If threading support is off, set the lock count to a constant value of 1 so assertions
// that the lock is held don't fail
const int interpreterLockCount = 1;

void JSLock::lock()
{
}

void JSLock::unlock()
{
}

#endif

int JSLock::lockCount()
{
    return interpreterLockCount;
}
        
JSLock::DropAllLocks::DropAllLocks()
{
    int lockCount = JSLock::lockCount();
    for (int i = 0; i < lockCount; i++) {
        JSLock::unlock();
    }
    m_lockCount = lockCount;
}

JSLock::DropAllLocks::~DropAllLocks()
{
    int lockCount = m_lockCount;
    for (int i = 0; i < lockCount; i++) {
        JSLock::lock();
    }
}

}
