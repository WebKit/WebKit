/*
 * Copyright (C) 2003-2017 Apple Inc.  All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2011 University of Szeged. All rights reserved.
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

// The vprintf_stderr_common function triggers this error in the Mac build.
// Feel free to remove this pragma if this file builds on Mac.
// According to http://gcc.gnu.org/onlinedocs/gcc-4.2.1/gcc/Diagnostic-Pragmas.html#Diagnostic-Pragmas
// we need to place this directive before any data or functions are defined.
#pragma GCC diagnostic ignored "-Wmissing-format-attribute"

#include "config.h"
#include <wtf/Assertions.h>

#include <mutex>
#include <stdio.h>
#include <string.h>
#include <wtf/Compiler.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/LoggingAccumulator.h>
#include <wtf/PrintStream.h>
#include <wtf/RetainPtr.h>
#include <wtf/StackTrace.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

#if HAVE(SIGNAL_H)
#include <signal.h>
#endif

#if USE(CF)
#include <CoreFoundation/CFString.h>
#if PLATFORM(COCOA)
#define USE_APPLE_SYSTEM_LOG 1
#include <asl.h>
#endif
#endif // USE(CF)

#if COMPILER(MSVC)
#include <crtdbg.h>
#endif

#if OS(WINDOWS)
#include <windows.h>
#endif

#if OS(DARWIN)
#include <sys/sysctl.h>
#include <unistd.h>
#endif

namespace WTF {

WTF_ATTRIBUTE_PRINTF(1, 0) static String createWithFormatAndArguments(const char* format, va_list args)
{
    va_list argsCopy;
    va_copy(argsCopy, args);

    ALLOW_NONLITERAL_FORMAT_BEGIN

#if USE(CF) && !OS(WINDOWS)
    if (strstr(format, "%@")) {
        auto cfFormat = adoptCF(CFStringCreateWithCString(kCFAllocatorDefault, format, kCFStringEncodingUTF8));
        auto result = adoptCF(CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, nullptr, cfFormat.get(), args));
        va_end(argsCopy);
        return result.get();
    }
#endif

    // Do the format once to get the length.
#if COMPILER(MSVC)
    int result = _vscprintf(format, args);
#else
    char ch;
    int result = vsnprintf(&ch, 1, format, args);
#endif

    if (!result) {
        va_end(argsCopy);
        return emptyString();
    }
    if (result < 0) {
        va_end(argsCopy);
        return { };
    }

    Vector<char, 256> buffer;
    unsigned length = result;
    buffer.grow(length + 1);

    // Now do the formatting again, guaranteed to fit.
    vsnprintf(buffer.data(), buffer.size(), format, argsCopy);
    va_end(argsCopy);

    ALLOW_NONLITERAL_FORMAT_END

    return StringImpl::create(reinterpret_cast<const LChar*>(buffer.data()), length);
}

}

extern "C" {

static void logToStderr(const char* buffer)
{
#if USE(APPLE_SYSTEM_LOG)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    asl_log(0, 0, ASL_LEVEL_NOTICE, "%s", buffer);
    ALLOW_DEPRECATED_DECLARATIONS_END
#endif
    fputs(buffer, stderr);
}

WTF_ATTRIBUTE_PRINTF(1, 0)
static void vprintf_stderr_common(const char* format, va_list args)
{
#if USE(CF) && !OS(WINDOWS)
    if (strstr(format, "%@")) {
        auto cfFormat = adoptCF(CFStringCreateWithCString(nullptr, format, kCFStringEncodingUTF8));

        ALLOW_NONLITERAL_FORMAT_BEGIN
        auto str = adoptCF(CFStringCreateWithFormatAndArguments(nullptr, nullptr, cfFormat.get(), args));
        ALLOW_NONLITERAL_FORMAT_END
        CFIndex length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str.get()), kCFStringEncodingUTF8);
        constexpr unsigned InitialBufferSize { 256 };
        Vector<char, InitialBufferSize> buffer(length + 1);

        CFStringGetCString(str.get(), buffer.data(), length, kCFStringEncodingUTF8);

        logToStderr(buffer.data());
        return;
    }

#if USE(APPLE_SYSTEM_LOG)
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    va_list copyOfArgs;
    va_copy(copyOfArgs, args);
    asl_vlog(0, 0, ASL_LEVEL_NOTICE, format, copyOfArgs);
    va_end(copyOfArgs);
    ALLOW_DEPRECATED_DECLARATIONS_END
#endif

    // Fall through to write to stderr in the same manner as other platforms.

#elif HAVE(ISDEBUGGERPRESENT)
    if (IsDebuggerPresent()) {
        size_t size = 1024;
        Vector<char> buffer(size);
        do {
            buffer.grow(size);
            if (vsnprintf(buffer.data(), size, format, args) != -1) {
                OutputDebugStringA(buffer.data());
                break;
            }
            size *= 2;
        } while (size > 1024);
    }
#endif
    vfprintf(stderr, format, args);
}

ALLOW_NONLITERAL_FORMAT_BEGIN

static void vprintf_stderr_with_prefix(const char* prefix, const char* format, va_list args)
{
    size_t prefixLength = strlen(prefix);
    size_t formatLength = strlen(format);
    Vector<char> formatWithPrefix(prefixLength + formatLength + 1);
    memcpy(formatWithPrefix.data(), prefix, prefixLength);
    memcpy(formatWithPrefix.data() + prefixLength, format, formatLength);
    formatWithPrefix[prefixLength + formatLength] = 0;

    vprintf_stderr_common(formatWithPrefix.data(), args);
}

static void vprintf_stderr_with_trailing_newline(const char* format, va_list args)
{
    size_t formatLength = strlen(format);
    if (formatLength && format[formatLength - 1] == '\n') {
        vprintf_stderr_common(format, args);
        return;
    }

    Vector<char> formatWithNewline(formatLength + 2);
    memcpy(formatWithNewline.data(), format, formatLength);
    formatWithNewline[formatLength] = '\n';
    formatWithNewline[formatLength + 1] = 0;

    vprintf_stderr_common(formatWithNewline.data(), args);
}

ALLOW_NONLITERAL_FORMAT_END

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
#if OS(WINDOWS) && defined(_DEBUG)
    _CrtDbgReport(_CRT_WARN, file, line, NULL, "%s\n", function);
#else
    // By using this format, which matches the format used by MSVC for compiler errors, developers
    // using Visual Studio can double-click the file/line number in the Output Window to have the
    // editor navigate to that line of code. It seems fine for other developers, too.
    printf_stderr_common("%s(%d) : %s\n", file, line, function);
#endif
}

void WTFReportNotImplementedYet(const char* file, int line, const char* function)
{
    printf_stderr_common("NOT IMPLEMENTED YET\n");
    printCallSite(file, line, function);
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

class CrashLogPrintStream : public PrintStream {
public:
    WTF_ATTRIBUTE_PRINTF(2, 0)
    void vprintf(const char* format, va_list argList) override
    {
        vprintf_stderr_common(format, argList);
    }
};

void WTFReportBacktrace()
{
    static const int framesToShow = 31;
    static const int framesToSkip = 2;
    void* samples[framesToShow + framesToSkip];
    int frames = framesToShow + framesToSkip;

    WTFGetBacktrace(samples, &frames);
    WTFPrintBacktrace(samples + framesToSkip, frames - framesToSkip);
}

void WTFPrintBacktrace(void** stack, int size)
{
    CrashLogPrintStream out;
    StackTrace stackTrace(stack, size);
    out.print(stackTrace);
}

#if !defined(NDEBUG) || !OS(DARWIN)
void WTFCrash()
{
    WTFReportBacktrace();
#if ASAN_ENABLED
    __builtin_trap();
#else
    *(int *)(uintptr_t)0xbbadbeef = 0;
    // More reliable, but doesn't say BBADBEEF.
#if COMPILER(GCC_COMPATIBLE)
    __builtin_trap();
#else
    ((void(*)())0)();
#endif // COMPILER(GCC_COMPATIBLE)
#endif // ASAN_ENABLED
}
#else
// We need to keep WTFCrash() around (even on non-debug OS(DARWIN) builds) as a workaround
// for presently shipping (circa early 2016) SafariForWebKitDevelopment binaries which still
// expects to link to it.
void WTFCrash()
{
    CRASH();
}
#endif // !defined(NDEBUG) || !OS(DARWIN)

void WTFCrashWithSecurityImplication()
{
    CRASH();
}

bool WTFIsDebuggerAttached()
{
#if OS(DARWIN)
    struct kinfo_proc info;
    int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
    size_t size = sizeof(info);
    if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), &info, &size, nullptr, 0) == -1)
        return false;
    return info.kp_proc.p_flag & P_TRACED;
#else
    return false;
#endif
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

class WTFLoggingAccumulator {
public:
    void accumulate(const String&);
    void resetAccumulatedLogs();
    String getAndResetAccumulatedLogs();

private:
    Lock accumulatorLock;
    StringBuilder loggingAccumulator;
};

void WTFLoggingAccumulator::accumulate(const String& log)
{
    Locker<Lock> locker(accumulatorLock);
    loggingAccumulator.append(log);
}

void WTFLoggingAccumulator::resetAccumulatedLogs()
{
    Locker<Lock> locker(accumulatorLock);
    loggingAccumulator.clear();
}

String WTFLoggingAccumulator::getAndResetAccumulatedLogs()
{
    Locker<Lock> locker(accumulatorLock);
    String result = loggingAccumulator.toString();
    loggingAccumulator.clear();
    return result;
}

static WTFLoggingAccumulator& loggingAccumulator()
{
    static WTFLoggingAccumulator* accumulator;
    static std::once_flag initializeAccumulatorOnce;
    std::call_once(initializeAccumulatorOnce, [] {
        accumulator = new WTFLoggingAccumulator;
    });

    return *accumulator;
}

void WTFSetLogChannelLevel(WTFLogChannel* channel, WTFLogLevel level)
{
    channel->level = level;
}

bool WTFWillLogWithLevel(WTFLogChannel* channel, WTFLogLevel level)
{
    return channel->level >= level && channel->state != WTFLogChannelOff;
}

void WTFLogWithLevel(WTFLogChannel* channel, WTFLogLevel level, const char* format, ...)
{
    if (level != WTFLogLevelAlways && level > channel->level)
        return;

    if (channel->level != WTFLogLevelAlways && channel->state == WTFLogChannelOff)
        return;

    va_list args;
    va_start(args, format);

    ALLOW_NONLITERAL_FORMAT_BEGIN
    WTFLog(channel, format, args);
    ALLOW_NONLITERAL_FORMAT_END

    va_end(args);
}

static void WTFLogVaList(WTFLogChannel* channel, const char* format, va_list args)
{
    if (channel->state == WTFLogChannelOff)
        return;

    if (channel->state == WTFLogChannelOn) {
        vprintf_stderr_with_trailing_newline(format, args);
        return;
    }

    ASSERT(channel->state == WTFLogChannelOnWithAccumulation);

    ALLOW_NONLITERAL_FORMAT_BEGIN
    String loggingString = WTF::createWithFormatAndArguments(format, args);
    ALLOW_NONLITERAL_FORMAT_END

    if (!loggingString.endsWith('\n'))
        loggingString.append('\n');

    loggingAccumulator().accumulate(loggingString);

    logToStderr(loggingString.utf8().data());
}

void WTFLog(WTFLogChannel* channel, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    WTFLogVaList(channel, format, args);

    va_end(args);
}

void WTFLogVerbose(const char* file, int line, const char* function, WTFLogChannel* channel, const char* format, ...)
{
    if (channel->state != WTFLogChannelOn)
        return;

    va_list args;
    va_start(args, format);

    ALLOW_NONLITERAL_FORMAT_BEGIN
    WTFLogVaList(channel, format, args);
    ALLOW_NONLITERAL_FORMAT_END

    va_end(args);

    printCallSite(file, line, function);
}

void WTFLogAlwaysV(const char* format, va_list args)
{
    vprintf_stderr_with_trailing_newline(format, args);
}

void WTFLogAlways(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    WTFLogAlwaysV(format, args);
    va_end(args);
}

void WTFLogAlwaysAndCrash(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    WTFLogAlwaysV(format, args);
    va_end(args);
    CRASH();
}

WTFLogChannel* WTFLogChannelByName(WTFLogChannel* channels[], size_t count, const char* name)
{
    for (size_t i = 0; i < count; ++i) {
        WTFLogChannel* channel = channels[i];
        if (equalIgnoringASCIICase(name, channel->name))
            return channel;
    }

    return 0;
}

static void setStateOfAllChannels(WTFLogChannel* channels[], size_t channelCount, WTFLogChannelState state)
{
    for (size_t i = 0; i < channelCount; ++i)
        channels[i]->state = state;
}

void WTFInitializeLogChannelStatesFromString(WTFLogChannel* channels[], size_t count, const char* logLevel)
{
#if !RELEASE_LOG_DISABLED
    for (size_t i = 0; i < count; ++i) {
        WTFLogChannel* channel = channels[i];
        channel->osLogChannel = os_log_create(channel->subsystem, channel->name);
    }
#endif

    for (auto& logLevelComponent : String(logLevel).split(',')) {
        Vector<String> componentInfo = logLevelComponent.split('=');
        String component = componentInfo[0].stripWhiteSpace();

        WTFLogChannelState logChannelState = WTFLogChannelOn;
        if (component.startsWith('-')) {
            logChannelState = WTFLogChannelOff;
            component = component.substring(1);
        }

        if (equalLettersIgnoringASCIICase(component, "all")) {
            setStateOfAllChannels(channels, count, logChannelState);
            continue;
        }

        WTFLogLevel logChannelLevel = WTFLogLevelError;
        if (componentInfo.size() > 1) {
            String level = componentInfo[1].stripWhiteSpace();
            if (equalLettersIgnoringASCIICase(level, "error"))
                logChannelLevel = WTFLogLevelError;
            else if (equalLettersIgnoringASCIICase(level, "warning"))
                logChannelLevel = WTFLogLevelWarning;
            else if (equalLettersIgnoringASCIICase(level, "info"))
                logChannelLevel = WTFLogLevelInfo;
            else if (equalLettersIgnoringASCIICase(level, "debug"))
                logChannelLevel = WTFLogLevelDebug;
            else
                WTFLogAlways("Unknown logging level: %s", level.utf8().data());
        }

        if (WTFLogChannel* channel = WTFLogChannelByName(channels, count, component.utf8().data())) {
            channel->state = logChannelState;
            channel->level = logChannelLevel;
        } else
            WTFLogAlways("Unknown logging channel: %s", component.utf8().data());
    }
}

#if !RELEASE_LOG_DISABLED
void WTFReleaseLogStackTrace(WTFLogChannel* channel)
{
    auto stackTrace = WTF::StackTrace::captureStackTrace(30, 0);
    if (stackTrace && stackTrace->stack()) {
        auto stack = stackTrace->stack();
        for (int frameNumber = 1; frameNumber < stackTrace->size(); ++frameNumber) {
            auto stackFrame = stack[frameNumber];
            auto demangled = WTF::StackTrace::demangle(stackFrame);
            if (demangled && demangled->demangledName())
                os_log(channel->osLogChannel, "%-3d %p %{public}s", frameNumber, stackFrame, demangled->demangledName());
            else if (demangled && demangled->mangledName())
                os_log(channel->osLogChannel, "%-3d %p %{public}s", frameNumber, stackFrame, demangled->mangledName());
            else
                os_log(channel->osLogChannel, "%-3d %p", frameNumber, stackFrame);
        }
    }
}
#endif

} // extern "C"

#if OS(DARWIN) && (CPU(X86_64) || CPU(ARM64))
#if CPU(X86_64)

#define CRASH_INST "int3"

// This ordering was chosen to be consistent with JSC's JIT asserts. We probably shouldn't change this ordering
// since it would make tooling crash reports much harder. If, for whatever reason, we decide to change the ordering
// here we should update the abortWithuint64_t functions.
#define CRASH_GPR0 "r11"
#define CRASH_GPR1 "r10"
#define CRASH_GPR2 "r9"
#define CRASH_GPR3 "r8"
#define CRASH_GPR4 "r15"
#define CRASH_GPR5 "r14"
#define CRASH_GPR6 "r13"

#elif CPU(ARM64) // CPU(X86_64)

#define CRASH_INST "brk #0"

// See comment above on the ordering.
#define CRASH_GPR0 "x16"
#define CRASH_GPR1 "x17"
#define CRASH_GPR2 "x18"
#define CRASH_GPR3 "x19"
#define CRASH_GPR4 "x20"
#define CRASH_GPR5 "x21"
#define CRASH_GPR6 "x22"

#endif // CPU(ARM64)

void WTFCrashWithInfo(int, const char*, const char*, int, uint64_t reason, uint64_t misc1, uint64_t misc2, uint64_t misc3, uint64_t misc4, uint64_t misc5, uint64_t misc6)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    register uint64_t misc2GPR asm(CRASH_GPR2) = misc2;
    register uint64_t misc3GPR asm(CRASH_GPR3) = misc3;
    register uint64_t misc4GPR asm(CRASH_GPR4) = misc4;
    register uint64_t misc5GPR asm(CRASH_GPR5) = misc5;
    register uint64_t misc6GPR asm(CRASH_GPR6) = misc6;
    __asm__ volatile (CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR), "r"(misc2GPR), "r"(misc3GPR), "r"(misc4GPR), "r"(misc5GPR), "r"(misc6GPR));
    __builtin_unreachable();
}

void WTFCrashWithInfo(int, const char*, const char*, int, uint64_t reason, uint64_t misc1, uint64_t misc2, uint64_t misc3, uint64_t misc4, uint64_t misc5)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    register uint64_t misc2GPR asm(CRASH_GPR2) = misc2;
    register uint64_t misc3GPR asm(CRASH_GPR3) = misc3;
    register uint64_t misc4GPR asm(CRASH_GPR4) = misc4;
    register uint64_t misc5GPR asm(CRASH_GPR5) = misc5;
    __asm__ volatile (CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR), "r"(misc2GPR), "r"(misc3GPR), "r"(misc4GPR), "r"(misc5GPR));
    __builtin_unreachable();
}

void WTFCrashWithInfo(int, const char*, const char*, int, uint64_t reason, uint64_t misc1, uint64_t misc2, uint64_t misc3, uint64_t misc4)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    register uint64_t misc2GPR asm(CRASH_GPR2) = misc2;
    register uint64_t misc3GPR asm(CRASH_GPR3) = misc3;
    register uint64_t misc4GPR asm(CRASH_GPR4) = misc4;
    __asm__ volatile (CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR), "r"(misc2GPR), "r"(misc3GPR), "r"(misc4GPR));
    __builtin_unreachable();
}

void WTFCrashWithInfo(int, const char*, const char*, int, uint64_t reason, uint64_t misc1, uint64_t misc2, uint64_t misc3)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    register uint64_t misc2GPR asm(CRASH_GPR2) = misc2;
    register uint64_t misc3GPR asm(CRASH_GPR3) = misc3;
    __asm__ volatile (CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR), "r"(misc2GPR), "r"(misc3GPR));
    __builtin_unreachable();
}

void WTFCrashWithInfo(int, const char*, const char*, int, uint64_t reason, uint64_t misc1, uint64_t misc2)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    register uint64_t misc2GPR asm(CRASH_GPR2) = misc2;
    __asm__ volatile (CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR), "r"(misc2GPR));
    __builtin_unreachable();
}

void WTFCrashWithInfo(int, const char*, const char*, int, uint64_t reason, uint64_t misc1)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    __asm__ volatile (CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR));
    __builtin_unreachable();
}

void WTFCrashWithInfo(int, const char*, const char*, int, uint64_t reason)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    __asm__ volatile (CRASH_INST : : "r"(reasonGPR));
    __builtin_unreachable();
}

#else

void WTFCrashWithInfo(int, const char*, const char*, int, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) { CRASH(); }
void WTFCrashWithInfo(int, const char*, const char*, int, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) { CRASH(); }
void WTFCrashWithInfo(int, const char*, const char*, int, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) { CRASH(); }
void WTFCrashWithInfo(int, const char*, const char*, int, uint64_t, uint64_t, uint64_t, uint64_t) { CRASH(); }
void WTFCrashWithInfo(int, const char*, const char*, int, uint64_t, uint64_t, uint64_t) { CRASH(); }
void WTFCrashWithInfo(int, const char*, const char*, int, uint64_t, uint64_t) { CRASH(); }
void WTFCrashWithInfo(int, const char*, const char*, int, uint64_t) { CRASH(); }

#endif // OS(DARWIN) && (CPU(X64_64) || CPU(ARM64))

namespace WTF {

void resetAccumulatedLogs()
{
    loggingAccumulator().resetAccumulatedLogs();
}

String getAndResetAccumulatedLogs()
{
    return loggingAccumulator().getAndResetAccumulatedLogs();
}

} // namespace WTF
