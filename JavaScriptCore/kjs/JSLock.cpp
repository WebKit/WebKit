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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA 
 *
 */

#include "config.h"
#include "JSLock.h"

#include "collector.h"
#if USE(MULTIPLE_THREADS)
#include <pthread.h>
#endif

namespace KJS {

#if USE(MULTIPLE_THREADS)

// Acquire this mutex before accessing lock-related data.
static pthread_mutex_t JSMutex = PTHREAD_MUTEX_INITIALIZER;

// Thread-specific key that tells whether a thread holds the JSMutex.
pthread_key_t didLockJSMutex;

// Lock nesting count.
static int JSLockCount;

static void createDidLockJSMutex()
{
    pthread_key_create(&didLockJSMutex, 0);
}
pthread_once_t createDidLockJSMutexOnce = PTHREAD_ONCE_INIT;

void JSLock::lock()
{
    pthread_once(&createDidLockJSMutexOnce, createDidLockJSMutex);

    if (!pthread_getspecific(didLockJSMutex)) {
        int result;
        result = pthread_mutex_lock(&JSMutex);
        ASSERT(!result);
        pthread_setspecific(didLockJSMutex, &didLockJSMutex);
    }
    ++JSLockCount;
}

void JSLock::unlock()
{
    ASSERT(JSLockCount);
    ASSERT(!!pthread_getspecific(didLockJSMutex));

    --JSLockCount;
    if (!JSLockCount) {
        pthread_setspecific(didLockJSMutex, 0);
        int result;
        result = pthread_mutex_unlock(&JSMutex);
        ASSERT(!result);
    }
}

bool JSLock::currentThreadIsHoldingLock()
{
    pthread_once(&createDidLockJSMutexOnce, createDidLockJSMutex);
    return !!pthread_getspecific(didLockJSMutex);
}

void JSLock::registerThread()
{
    Collector::registerThread();
}

JSLock::DropAllLocks::DropAllLocks()
    : m_lockCount(0)
{
    pthread_once(&createDidLockJSMutexOnce, createDidLockJSMutex);

    m_lockCount = !!pthread_getspecific(didLockJSMutex) ? JSLock::lockCount() : 0;
    for (int i = 0; i < m_lockCount; i++)
        JSLock::unlock();
}

JSLock::DropAllLocks::~DropAllLocks()
{
    for (int i = 0; i < m_lockCount; i++)
        JSLock::lock();
    m_lockCount = 0;
}

#else

// If threading support is off, set the lock count to a constant value of 1 so assertions
// that the lock is held don't fail
const int JSLockCount = 1;

bool JSLock::currentThreadIsHoldingLock()
{
    return true;
}

void JSLock::lock()
{
}

void JSLock::unlock()
{
}

void JSLock::registerThread()
{
}

JSLock::DropAllLocks::DropAllLocks()
{
}

JSLock::DropAllLocks::~DropAllLocks()
{
}

#endif // USE(MULTIPLE_THREADS)

int JSLock::lockCount()
{
    return JSLockCount;
}

}
