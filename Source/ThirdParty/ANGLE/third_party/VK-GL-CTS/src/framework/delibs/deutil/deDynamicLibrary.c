/*-------------------------------------------------------------------------
 * drawElements Utility Library
 * ----------------------------
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
 * \brief Dynamic link library abstraction.
 *//*--------------------------------------------------------------------*/

#include "deDynamicLibrary.h"
#include "deMemory.h"

#if (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_OSX) || (DE_OS == DE_OS_SYMBIAN) || \
    (DE_OS == DE_OS_IOS) || (DE_OS == DE_OS_QNX) || (DE_OS == DE_OS_FUCHSIA)
/* Posix implementation. */

#include <dlfcn.h>

struct deDynamicLibrary_s
{
    void *libHandle;
};

deDynamicLibrary *deDynamicLibrary_open(const char *fileName)
{
    deDynamicLibrary *library = (deDynamicLibrary *)deCalloc(sizeof(deDynamicLibrary));
    if (!library)
        return NULL;

    library->libHandle = dlopen(fileName, RTLD_LAZY);

    if (!library->libHandle)
    {
        deFree(library);
        return NULL;
    }

    return library;
}

void deDynamicLibrary_close(deDynamicLibrary *library)
{
    if (library && library->libHandle)
        dlclose(library->libHandle);
    deFree(library);
}

deFunctionPtr deDynamicLibrary_getFunction(const deDynamicLibrary *library, const char *symbolName)
{
    /* C forbids direct cast from object pointer to function pointer */
    union
    {
        deFunctionPtr funcPtr;
        void *objPtr;
    } ptr;

    DE_ASSERT(library && library->libHandle && symbolName);
    ptr.objPtr = dlsym(library->libHandle, symbolName);
    return ptr.funcPtr;
}

#elif (DE_OS == DE_OS_WIN32)
/* Win32 implementation. */

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>

struct deDynamicLibrary_s
{
    HINSTANCE handle;
};

deDynamicLibrary *deDynamicLibrary_open(const char *fileName)
{
    deDynamicLibrary *library = (deDynamicLibrary *)deCalloc(sizeof(deDynamicLibrary));
    if (!library)
        return NULL;

    library->handle = LoadLibrary(fileName);
    if (!library->handle)
    {
        deFree(library);
        return NULL;
    }

    return library;
}

void deDynamicLibrary_close(deDynamicLibrary *library)
{
    if (library && library->handle)
        FreeLibrary(library->handle);
    deFree(library);
}

deFunctionPtr deDynamicLibrary_getFunction(const deDynamicLibrary *library, const char *symbolName)
{
    DE_ASSERT(library && library->handle && symbolName);
    return (deFunctionPtr)GetProcAddress(library->handle, symbolName);
}

#else
#error deDynamicLibrary is not implemented on this OS
#endif
