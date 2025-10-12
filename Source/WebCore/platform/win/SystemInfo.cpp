/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SystemInfo.h"

#include <windows.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

IGNORE_CLANG_WARNINGS_BEGIN("deprecated-declarations")

static std::tuple<int, int> windowsVersion()
{
    static bool initialized = false;
    static int majorVersion, minorVersion;

    if (!initialized) {
        initialized = true;
        OSVERSIONINFOEX versionInfo { };
        versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
        GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&versionInfo));
        majorVersion = versionInfo.dwMajorVersion;
        minorVersion = versionInfo.dwMinorVersion;
    }

    return { majorVersion, minorVersion };
}

IGNORE_CLANG_WARNINGS_END

static bool isWOW64()
{
    static bool initialized = false;
    static bool wow64 = false;

    if (!initialized) {
        initialized = true;
        HMODULE kernel32Module = GetModuleHandleA("kernel32.dll");
        if (!kernel32Module)
            return wow64;
        typedef BOOL (WINAPI* IsWow64ProcessFunc)(HANDLE, PBOOL);
        IsWow64ProcessFunc isWOW64Process = reinterpret_cast<IsWow64ProcessFunc>((void*)GetProcAddress(kernel32Module, "IsWow64Process"));
        if (isWOW64Process) {
            BOOL result = FALSE;
            wow64 = isWOW64Process(GetCurrentProcess(), &result) && result;
        }
    }

    return wow64;
}

static WORD processorArchitecture()
{
    static bool initialized = false;
    static WORD architecture = PROCESSOR_ARCHITECTURE_INTEL;

    if (!initialized) {
        initialized = true;
        SYSTEM_INFO systemInfo { };
        GetNativeSystemInfo(&systemInfo);
        architecture = systemInfo.wProcessorArchitecture;
    }
    return architecture;
}

static String architectureTokenForUAString()
{
    if (isWOW64())
        return "; WOW64"_s;
    if (processorArchitecture() == PROCESSOR_ARCHITECTURE_AMD64)
        return "; Win64; x64"_s;
    if (processorArchitecture() == PROCESSOR_ARCHITECTURE_IA64)
        return "; Win64; IA64"_s;
    return { };
}

String windowsVersionForUAString()
{
    auto [major, minor] = windowsVersion();
    return makeString("Windows NT "_s, major, '.', minor, architectureTokenForUAString());
}

} // namespace WebCore
