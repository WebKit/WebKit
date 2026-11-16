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
 * \brief Win32 implementation of semaphore.
 *//*--------------------------------------------------------------------*/

#include "deSemaphore.h"

#if (DE_OS == DE_OS_WIN32 || DE_OS == DE_OS_WINCE)

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define WIN32_SEM_MAX_VALUE 0x7fffffff

DE_STATIC_ASSERT(sizeof(deSemaphore) >= sizeof(HANDLE));

deSemaphore deSemaphore_create(int initialValue, const deSemaphoreAttributes *attributes)
{
    HANDLE handle;

    DE_UNREF(attributes);

    handle = CreateSemaphore(NULL, initialValue, WIN32_SEM_MAX_VALUE, NULL);
    if (!handle)
        return 0;

    DE_ASSERT((deSemaphore)handle != 0);

    return (deSemaphore)handle;
}

void deSemaphore_destroy(deSemaphore semaphore)
{
    HANDLE handle = (HANDLE)semaphore;
    CloseHandle(handle);
}

void deSemaphore_increment(deSemaphore semaphore)
{
    HANDLE handle = (HANDLE)semaphore;
    BOOL ret      = ReleaseSemaphore(handle, 1, NULL);
    DE_ASSERT(ret);
    DE_UNREF(ret);
}

void deSemaphore_decrement(deSemaphore semaphore)
{
    HANDLE handle = (HANDLE)semaphore;
    DWORD ret     = WaitForSingleObject(handle, INFINITE);
    DE_ASSERT(ret == WAIT_OBJECT_0);
    DE_UNREF(ret);
}

bool deSemaphore_tryDecrement(deSemaphore semaphore)
{
    HANDLE handle = (HANDLE)semaphore;
    DWORD ret     = WaitForSingleObject(handle, 0);
    return (ret == WAIT_OBJECT_0);
}

#endif /* DE_OS */
