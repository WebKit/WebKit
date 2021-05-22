/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/win/DbgHelperWin.h>

#include <mutex>

namespace WTF {

namespace DbgHelper {

// We are only calling these DbgHelp.dll functions in debug mode since the library is not threadsafe.
// It's possible for external code to call the library at the same time as WebKit and cause memory corruption.

#if !defined(NDEBUG)

static Lock callMutex;

static void initializeSymbols(HANDLE hProc)
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&]() {
        if (!SymInitialize(hProc, nullptr, TRUE))
            LOG_ERROR("Failed to initialze symbol information %d", GetLastError());
    });
}

bool SymFromAddress(HANDLE hProc, DWORD64 address, DWORD64* displacement, SYMBOL_INFO* symbolInfo)
{
    LockHolder lock(callMutex);
    initializeSymbols(hProc);

    bool success = ::SymFromAddr(hProc, address, displacement, symbolInfo);
    if (success)
        symbolInfo->Name[symbolInfo->NameLen] = '\0';
    return success;
}

#else

bool SymFromAddress(HANDLE, DWORD64, DWORD64*, SYMBOL_INFO*)
{
    return false;
}

#endif // !defined(NDEBUG)

} // namespace DbgHelper

} // namespace WTF
