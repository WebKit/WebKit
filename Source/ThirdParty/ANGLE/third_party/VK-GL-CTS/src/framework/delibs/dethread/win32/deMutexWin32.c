/*-------------------------------------------------------------------------
 * drawElements Thread Library
 * ---------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Win32 implementation of mutex.
 *//*--------------------------------------------------------------------*/

#include "deMutex.h"

#if (DE_OS == DE_OS_WIN32 || DE_OS == DE_OS_WINCE)

#include "deMemory.h"

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

/* Critical section objects are more lightweight than mutexes on Win32. */
#define USE_CRITICAL_SECTION 1

#if defined(USE_CRITICAL_SECTION)

enum
{
    CRITICAL_SECTION_SPIN_COUNT = 2048
};

DE_STATIC_ASSERT(sizeof(deMutex) >= sizeof(CRITICAL_SECTION *));

deMutex deMutex_create(const deMutexAttributes *attributes)
{
    CRITICAL_SECTION *criticalSection = (CRITICAL_SECTION *)deMalloc(sizeof(CRITICAL_SECTION));
    if (!criticalSection)
        return 0;

    DE_UNREF(attributes);
    /* \note [2012-11-05 pyry] Critical sections are always recursive. */

    if (!InitializeCriticalSectionAndSpinCount(criticalSection, CRITICAL_SECTION_SPIN_COUNT))
    {
        deFree(criticalSection);
        return 0;
    }

    return (deMutex)criticalSection;
}

void deMutex_destroy(deMutex mutex)
{
    DeleteCriticalSection((CRITICAL_SECTION *)mutex);
    deFree((CRITICAL_SECTION *)mutex);
}

void deMutex_lock(deMutex mutex)
{
    EnterCriticalSection((CRITICAL_SECTION *)mutex);
}

void deMutex_unlock(deMutex mutex)
{
    LeaveCriticalSection((CRITICAL_SECTION *)mutex);
}

bool deMutex_tryLock(deMutex mutex)
{
    return TryEnterCriticalSection((CRITICAL_SECTION *)mutex) == TRUE;
}

#else

DE_STATIC_ASSERT(sizeof(deMutex) >= sizeof(HANDLE));

deMutex deMutex_create(const deMutexAttributes *attributes)
{
    HANDLE handle = NULL;

    DE_UNREF(attributes);
    /* \note [2009-11-12 pyry] Created mutex is always recursive. */

    handle = CreateMutex(NULL, FALSE, NULL);
    return (deMutex)handle;
}

void deMutex_destroy(deMutex mutex)
{
    HANDLE handle = (HANDLE)mutex;
    CloseHandle(handle);
}

void deMutex_lock(deMutex mutex)
{
    HANDLE handle = (HANDLE)mutex;
    DWORD ret     = WaitForSingleObject(handle, INFINITE);
    DE_ASSERT(ret == WAIT_OBJECT_0);
}

void deMutex_unlock(deMutex mutex)
{
    HANDLE handle = (HANDLE)mutex;
    BOOL ret      = ReleaseMutex(handle);
    DE_ASSERT(ret == TRUE);
}

bool deMutex_tryLock(deMutex mutex)
{
    HANDLE handle = (HANDLE)mutex;
    DWORD ret     = WaitForSingleObject(handle, 0);
    return (ret == WAIT_OBJECT_0);
}

#endif /* USE_CRITICAL_SECTION */

#endif /* DE_OS */
