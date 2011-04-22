/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Module.h"

#include <WebCore/DelayLoadedModulesEnumerator.h>
#include <WebCore/ImportedFunctionsEnumerator.h>
#include <WebCore/ImportedModulesEnumerator.h>
#include <shlwapi.h>

using namespace WebCore;

namespace WebKit {

bool Module::load()
{
    ASSERT(!::PathIsRelativeW(m_path.charactersWithNullTermination()));
    m_module = ::LoadLibraryExW(m_path.charactersWithNullTermination(), 0, LOAD_WITH_ALTERED_SEARCH_PATH);
    return m_module;
}

void Module::unload()
{
    if (!m_module)
        return;
    ::FreeLibrary(m_module);
    m_module = 0;
}

static void memcpyToReadOnlyMemory(void* destination, const void* source, size_t size)
{
    DWORD originalProtection;
    if (!::VirtualProtect(destination, size, PAGE_READWRITE, &originalProtection))
        return;

    memcpy(destination, source, size);

    ::VirtualProtect(destination, size, originalProtection, &originalProtection);
}

static const void* const* findFunctionPointerAddress(ImportedModulesEnumeratorBase& modules, const char* importDLLName, const char* importFunctionName)
{
    for (; !modules.isAtEnd(); modules.next()) {
        if (_stricmp(importDLLName, modules.currentModuleName()))
            continue;

        for (ImportedFunctionsEnumerator functions = modules.functionsEnumerator(); !functions.isAtEnd(); functions.next()) {
            const char* currentFunctionName = functions.currentFunctionName();
            if (!currentFunctionName || strcmp(importFunctionName, currentFunctionName))
                continue;

            return functions.addressOfCurrentFunctionPointer();
        }

        break;
    }

    return 0;
}

static const void* const* findFunctionPointerAddress(HMODULE module, const char* importDLLName, const char* importFunctionName)
{
    PEImage image(module);

    ImportedModulesEnumerator importedModules(image);
    if (const void* const* functionPointerAddress = findFunctionPointerAddress(importedModules, importDLLName, importFunctionName))
        return functionPointerAddress;

    DelayLoadedModulesEnumerator delayLoadedModules(image);
    return findFunctionPointerAddress(delayLoadedModules, importDLLName, importFunctionName);
}

void Module::installIATHook(const char* importDLLName, const char* importFunctionName, const void* hookFunction)
{
    if (!m_module)
        return;

    // The Import Address Table (IAT) contains one function pointer for each function imported by
    // this module. When code in this module calls that function, the function pointer from the IAT
    // is used. We find that function pointer and overwrite it with hookFunction so that
    // hookFunction will be called instead.

    const void* const* functionPointerAddress = findFunctionPointerAddress(m_module, importDLLName, importFunctionName);
    if (!functionPointerAddress || *functionPointerAddress == hookFunction)
        return;

    memcpyToReadOnlyMemory(const_cast<const void**>(functionPointerAddress), &hookFunction, sizeof(hookFunction));
}

void* Module::platformFunctionPointer(const char* functionName) const
{
    if (!m_module)
        return 0;

    return ::GetProcAddress(m_module, functionName);
}

} // namespace WebKit
