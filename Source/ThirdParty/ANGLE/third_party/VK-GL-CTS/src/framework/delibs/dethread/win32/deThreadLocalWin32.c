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
 * \brief Win32 implementation of thread-local storage.
 *//*--------------------------------------------------------------------*/

#include "deThreadLocal.h"

#if (DE_OS == DE_OS_WIN32 || DE_OS == DE_OS_WINCE)

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

DE_STATIC_ASSERT(sizeof(deThreadLocal) >= sizeof(DWORD));

deThreadLocal deThreadLocal_create(void)
{
    DWORD handle = TlsAlloc();
    if (handle == TLS_OUT_OF_INDEXES)
        return 0;
    return (deThreadLocal)handle;
}

void deThreadLocal_destroy(deThreadLocal threadLocal)
{
    DE_ASSERT(threadLocal != 0);
    TlsFree((DWORD)threadLocal);
}

void *deThreadLocal_get(deThreadLocal threadLocal)
{
    DE_ASSERT(threadLocal != 0);
    return TlsGetValue((DWORD)threadLocal);
}

void deThreadLocal_set(deThreadLocal threadLocal, void *value)
{
    DE_ASSERT(threadLocal != 0);
    TlsSetValue((DWORD)threadLocal, value);
}

#endif /* DE_OS */
