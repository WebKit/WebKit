/*
 * Copyright (C) 2003, 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// The vprintf_stderr_common function triggers this error in the Mac build.
// Feel free to remove this pragma if this file builds on Mac.
// According to http://gcc.gnu.org/onlinedocs/gcc-4.2.1/gcc/Diagnostic-Pragmas.html#Diagnostic-Pragmas
// we need to place this directive before any data or functions are defined.
#pragma GCC diagnostic ignored "-Wmissing-format-attribute"

#include "config.h"
#include "Assertions.h"

#include "OwnArrayPtr.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#if PLATFORM(MAC)
#include <CoreFoundation/CFString.h>
#include <asl.h>
#endif

#if COMPILER(MSVC) && !OS(WINCE)
#include <crtdbg.h>
#endif

#if OS(WINDOWS)
#include <windows.h>
#endif

#if OS(DARWIN) || OS(LINUX)
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#endif

#if PLATFORM(BLACKBERRY)
#include <BlackBerryPlatformLog.h>
#endif

extern "C" {

WTF_ATTRIBUTE_PRINTF(1, 0)
static void vprintf_stderr_common(const char* format, va_list args)
{
#if PLATFORM(MAC)
    if (strstr(format, "%@")) {
        CFStringRef cfFormat = CFStringCreateWithCString(NULL, format, kCFStringEncodingUTF8);
        CFStringRef str = CFStringCreateWithFormatAndArguments(NULL, NULL, cfFormat, args);

        int length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8);
        char* buffer = (char*)malloc(length + 1);

        CFStringGetCString(str, buffer, length, kCFStringEncodingUTF8);

#if !defined(BUILDING_ON_SNOW_LEOPARD) && !defined(BUILDING_ON_LION)
        asl_log(0, 0, ASL_LEVEL_NOTICE, "%s", buffer);
#endif
        fputs(buffer, stderr);

        free(buffer);
        CFRelease(str);
        CFRelease(cfFormat);
        return;
    }

#if !defined(BUILDING_ON_SNOW_LEOPARD) && !defined(BUILDING_ON_LION)
    va_list copyOfArgs;
    va_copy(copyOfArgs, args);
    asl_vlog(0, 0, ASL_LEVEL_NOTICE, format, copyOfArgs);
    va_end(copyOfArgs);
#endif

    // Fall through to write to stderr in the same manner as other platforms.

#elif PLATFORM(BLACKBERRY)
    BlackBerry::Platform::logV(BlackBerry::Platform::LogLevelInfo, format, args);
#elif HAVE(ISDEBUGGERPRESENT)
    if (IsDebuggerPresent()) {
        size_t size = 1024;

        do {
            char* buffer = (char*)malloc(size);

            if (buffer == NULL)
                break;

            if (_vsnprintf(buffer, size, format, args) != -1) {
#if OS(WINCE)
                // WinCE only supports wide chars
                wchar_t* wideBuffer = (wchar_t*)malloc(size * sizeof(wchar_t));
                if (wideBuffer == NULL)
                    break;
                for (unsigned int i = 0; i < size; ++i) {
                    if (!(wideBuffer[i] = buffer[i]))
                        break;
                }
                OutputDebugStringW(wideBuffer);
                free(wideBuffer);
#else
                OutputDebugStringA(buffer);
#endif
                free(buffer);
                break;
            }

            free(buffer);
            size *= 2;
        } while (size > 1024);
    }
#endif
    vfprintf(stderr, format, args);
}

#if COMPILER(CLANG) || (COMPILER(GCC) && GCC_VERSION_AT_LEAST(4, 6, 0))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif

static void vprintf_stderr_with_prefix(const char* prefix, const char* format, va_list args)
{
    size_t prefixLength = strlen(prefix);
    size_t formatLength = strlen(format);
    OwnArrayPtr<char> formatWithPrefix = adoptArrayPtr(new char[prefixLength + formatLength + 1]);
    memcpy(formatWithPrefix.get(), prefix, prefixLength);
    memcpy(formatWithPrefix.get() + prefixLength, format, formatLength);
    formatWithPrefix[prefixLength + formatLength] = 0;

    vprintf_stderr_common(formatWithPrefix.get(), args);
}

static void vprintf_stderr_with_trailing_newline(const char* format, va_list args)
{
    size_t formatLength = strlen(format);
    if (formatLength && format[formatLength - 1] == '\n') {
        vprintf_stderr_common(format, args);
        return;
    }

    OwnArrayPtr<char> formatWithNewline = adoptArrayPtr(new char[formatLength + 2]);
    memcpy(formatWithNewline.get(), format, formatLength);
    formatWithNewline[formatLength] = '\n';
    formatWithNewline[formatLength + 1] = 0;

    vprintf_stderr_common(formatWithNewline.get(), args);
}

#if COMPILER(CLANG) || (COMPILER(GCC) && GCC_VERSION_AT_LEAST(4, 6, 0))
#pragma GCC diagnostic pop
#endif

WTF_ATTRIBUTE_PRINTF(1, 2)
static void printf_stderr_common(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf_stderr_common(format, args);
    va_end(args);
}

static void printCallSite(const char* file, int line, const char* function)
{
#if OS(WINDOWS) && !OS(WINCE) && defined(_DEBUG)
    _CrtDbgReport(_CRT_WARN, file, line, NULL, "%s\n", function);
#else
    // By using this format, which matches the format used by MSVC for compiler errors, developers
    // using Visual Studio can double-click the file/line number in the Output Window to have the
    // editor navigate to that line of code. It seems fine for other developers, too.
    printf_stderr_common("%s(%d) : %s\n", file, line, function);
#endif
}

void WTFReportAssertionFailure(const char* file, int line, const char* function, const char* assertion)
{
    if (assertion)
        printf_stderr_common("ASSERTION FAILED: %s\n", assertion);
    else
        printf_stderr_common("SHOULD NEVER BE REACHED\n");
    printCallSite(file, line, function);
}

void WTFReportAssertionFailureWithMessage(const char* file, int line, const char* function, const char* assertion, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf_stderr_with_prefix("ASSERTION FAILED: ", format, args);
    va_end(args);
    printf_stderr_common("\n%s\n", assertion);
    printCallSite(file, line, function);
}

void WTFReportArgumentAssertionFailure(const char* file, int line, const char* function, const char* argName, const char* assertion)
{
    printf_stderr_common("ARGUMENT BAD: %s, %s\n", argName, assertion);
    printCallSite(file, line, function);
}

void WTFGetBacktrace(void** stack, int* size)
{
#if OS(DARWIN) || OS(LINUX)
    *size = backtrace(stack, *size);
#elif OS(WINDOWS) && !OS(WINCE)
    // The CaptureStackBackTrace function is available in XP, but it is not defined
    // in the Windows Server 2003 R2 Platform SDK. So, we'll grab the function
    // through GetProcAddress.
    typedef WORD (NTAPI* RtlCaptureStackBackTraceFunc)(DWORD, DWORD, PVOID*, PDWORD);
    HMODULE kernel32 = ::GetModuleHandleW(L"Kernel32.dll");
    if (!kernel32) {
        *size = 0;
        return;
    }
    RtlCaptureStackBackTraceFunc captureStackBackTraceFunc = reinterpret_cast<RtlCaptureStackBackTraceFunc>(
        ::GetProcAddress(kernel32, "RtlCaptureStackBackTrace"));
    if (captureStackBackTraceFunc)
        *size = captureStackBackTraceFunc(0, *size, stack, 0);
    else
        *size = 0;
#else
    *size = 0;
#endif
}

void WTFReportBacktrace()
{
    static const int framesToShow = 31;
    static const int framesToSkip = 2;
    void* samples[framesToShow + framesToSkip];
    int frames = framesToShow + framesToSkip;

    WTFGetBacktrace(samples, &frames);

    for (int i = framesToSkip; i < frames; ++i) {
        const char* mangledName = 0;
        char* cxaDemangled = 0;

#if !PLATFORM(GTK) && !PLATFORM(QT) && (OS(DARWIN) || OS(LINUX))
        Dl_info info;
        if (dladdr(samples[i], &info) && info.dli_sname)
            mangledName = info.dli_sname;
        if (mangledName)
            cxaDemangled = abi::__cxa_demangle(mangledName, 0, 0, 0);
#endif
        const int frameNumber = i - framesToSkip + 1;
        if (mangledName || cxaDemangled)
            printf_stderr_common("%-3d %p %s\n", frameNumber, samples[i], cxaDemangled ? cxaDemangled : mangledName);
        else
            printf_stderr_common("%-3d %p\n", frameNumber, samples[i]);
        free(cxaDemangled);
    }
}

void WTFReportFatalError(const char* file, int line, const char* function, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf_stderr_with_prefix("FATAL ERROR: ", format, args);
    va_end(args);
    printf_stderr_common("\n");
    printCallSite(file, line, function);
}

void WTFReportError(const char* file, int line, const char* function, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf_stderr_with_prefix("ERROR: ", format, args);
    va_end(args);
    printf_stderr_common("\n");
    printCallSite(file, line, function);
}

void WTFLog(WTFLogChannel* channel, const char* format, ...)
{
    if (channel->state != WTFLogChannelOn)
        return;

    va_list args;
    va_start(args, format);
    vprintf_stderr_with_trailing_newline(format, args);
    va_end(args);
}

void WTFLogVerbose(const char* file, int line, const char* function, WTFLogChannel* channel, const char* format, ...)
{
    if (channel->state != WTFLogChannelOn)
        return;

    va_list args;
    va_start(args, format);
    vprintf_stderr_with_trailing_newline(format, args);
    va_end(args);

    printCallSite(file, line, function);
}

} // extern "C"
