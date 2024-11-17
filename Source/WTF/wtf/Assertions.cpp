/*
 * Copyright (C) 2003-2024 Apple Inc.  All rights reserved.
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
#include <wtf/StackTrace.h>
#include <wtf/WTFConfig.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

#if USE(CF)
#include <CoreFoundation/CFString.h>
#endif // USE(CF)

#if OS(WINDOWS)
#include <crtdbg.h>
#include <windows.h>
#endif

#if OS(DARWIN)
#include <sys/sysctl.h>
#include <unistd.h>
#endif

#if !RELEASE_LOG_DISABLED && !USE(OS_LOG)
#include <wtf/StringPrintStream.h>
#endif

#if PLATFORM(COCOA)
#import <wtf/spi/cocoa/OSLogSPI.h>
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

WTF_ATTRIBUTE_PRINTF(1, 0)
static String createWithFormatAndArguments(const char* format, va_list args)
{
    va_list argsCopy;
    va_copy(argsCopy, args);

ALLOW_NONLITERAL_FORMAT_BEGIN

#if USE(CF)
    if (strstr(format, "%@")) {
        auto cfFormat = adoptCF(CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, format, kCFStringEncodingUTF8, kCFAllocatorNull));
        auto result = adoptCF(CFStringCreateWithFormatAndArguments(kCFAllocatorDefault, nullptr, cfFormat.get(), args));
        va_end(argsCopy);
        return result.get();
    }
#endif

    // Do the format once to get the length.
#if OS(WINDOWS)
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
    buffer.grow(result + 1);

    // Now do the formatting again, guaranteed to fit.
    vsnprintf(buffer.data(), buffer.size(), format, argsCopy);
    va_end(argsCopy);

ALLOW_NONLITERAL_FORMAT_END

    return StringImpl::create(buffer.subspan(0, buffer.size() - 1));
}

#if PLATFORM(COCOA)
void disableForwardingVPrintfStdErrToOSLog()
{
    g_wtfConfig.disableForwardingVPrintfStdErrToOSLog = true;
}
#endif

} // namespace WTF

extern "C" {

#if PLATFORM(COCOA)
static os_log_t webkitSubsystemForGenericOSLog()
{
    static dispatch_once_t once;
    static os_log_t subsystem;
    dispatch_once(&once, ^{
        subsystem = os_log_create(LOG_CHANNEL_WEBKIT_SUBSYSTEM, "Generic");
    });
    return subsystem;
}
#endif

static void logToStderr([[maybe_unused]] WTFLogChannel* channel, const char* buffer)
{
#if PLATFORM(COCOA)
    os_log(channel ? channel->osLogChannel : webkitSubsystemForGenericOSLog(), "%s", buffer);
#endif
    fputs(buffer, stderr);
}

WTF_ATTRIBUTE_PRINTF(2, 0)
static void vprintf_stderr_common([[maybe_unused]] WTFLogChannel* channel, const char* format, va_list args)
{
#if USE(CF)
    if (strstr(format, "%@")) {
        auto cfFormat = adoptCF(CFStringCreateWithCStringNoCopy(nullptr, format, kCFStringEncodingUTF8, kCFAllocatorNull));

ALLOW_NONLITERAL_FORMAT_BEGIN
        auto str = adoptCF(CFStringCreateWithFormatAndArguments(nullptr, nullptr, cfFormat.get(), args));
ALLOW_NONLITERAL_FORMAT_END
        CFIndex length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str.get()), kCFStringEncodingUTF8);
        constexpr unsigned InitialBufferSize { 256 };
        Vector<char, InitialBufferSize> buffer(length + 1);

        CFStringGetCString(str.get(), buffer.data(), length, kCFStringEncodingUTF8);

        logToStderr(channel, buffer.data());
        return;
    }

#if PLATFORM(COCOA)
    if (!g_wtfConfig.disableForwardingVPrintfStdErrToOSLog) {
        os_log_t osLogChannel = channel ? channel->osLogChannel : webkitSubsystemForGenericOSLog();
        va_list copyOfArgs;
        va_copy(copyOfArgs, args);
        os_log_with_args(osLogChannel, OS_LOG_TYPE_DEFAULT, format, copyOfArgs, __builtin_return_address(0));
        va_end(copyOfArgs);
    }
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

WTF_ATTRIBUTE_PRINTF(2, 0)
static void vprintf_stderr_with_prefix(const char* prefix, const char* format, va_list args)
{
    size_t prefixLength = strlen(prefix);
    size_t formatLength = strlen(format);
    Vector<char> formatWithPrefix(prefixLength + formatLength + 1);
    memcpy(formatWithPrefix.data(), prefix, prefixLength);
    memcpy(formatWithPrefix.data() + prefixLength, format, formatLength);
    formatWithPrefix[prefixLength + formatLength] = 0;

ALLOW_NONLITERAL_FORMAT_BEGIN
    vprintf_stderr_common(nullptr, formatWithPrefix.data(), args);
ALLOW_NONLITERAL_FORMAT_END
}

WTF_ATTRIBUTE_PRINTF(2, 0)
static void vprintf_stderr_with_trailing_newline(WTFLogChannel* channel, const char* format, va_list args)
{
    size_t formatLength = strlen(format);
    if (formatLength && format[formatLength - 1] == '\n') {
        vprintf_stderr_common(channel, format, args);
        return;
    }

    Vector<char> formatWithNewline(formatLength + 2);
    memcpy(formatWithNewline.data(), format, formatLength);
    formatWithNewline[formatLength] = '\n';
    formatWithNewline[formatLength + 1] = 0;

ALLOW_NONLITERAL_FORMAT_BEGIN
    vprintf_stderr_common(channel, formatWithNewline.data(), args);
ALLOW_NONLITERAL_FORMAT_END
}

WTF_ATTRIBUTE_PRINTF(1, 2)
static void printf_stderr_common(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf_stderr_common(nullptr, format, args);
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

class CrashLogPrintStream final : public PrintStream {
public:
    WTF_ATTRIBUTE_PRINTF(2, 0)
    void vprintf(const char* format, va_list argList) final
    {
        vprintf_stderr_common(nullptr, format, argList);
    }
};

void WTFReportBacktraceWithPrefix(const char* prefix)
{
    CrashLogPrintStream out;
    WTFReportBacktraceWithPrefixAndPrintStream(out, prefix);
}

static constexpr int kDefaultFramesToShow = 31;
static constexpr int kDefaultFramesToSkip = 2;

void WTFReportBacktraceWithStackDepth(int framesToShow)
{
    WTFReportBacktraceWithPrefixAndStackDepth("", framesToShow);
}

void WTFReportBacktraceWithPrefixAndStackDepth(const char* prefix, int framesToShow)
{
    int frames = framesToShow + kDefaultFramesToSkip;
    Vector<void*> samples;
    samples.resize(frames);

    WTFGetBacktrace(samples.data(), &frames);
    CrashLogPrintStream out;
    if (frames > kDefaultFramesToSkip)
        WTFPrintBacktraceWithPrefixAndPrintStream(out, samples.subspan(kDefaultFramesToSkip, framesToShow), prefix);
    else
        out.print("%sno stacktrace available", prefix);
}

void WTFReportBacktraceWithPrefixAndPrintStream(PrintStream& out, const char* prefix)
{
    std::array<void*, kDefaultFramesToShow + kDefaultFramesToSkip> samples;
    int frames = samples.size();

    WTFGetBacktrace(samples.data(), &frames);
    if (frames > kDefaultFramesToSkip)
        WTFPrintBacktraceWithPrefixAndPrintStream(out, std::span { samples }.subspan(kDefaultFramesToSkip, frames - kDefaultFramesToSkip), prefix);
    else
        out.print("%sno stacktrace available", prefix);
}

void WTFReportBacktrace()
{
    std::array<void*, kDefaultFramesToShow + kDefaultFramesToSkip> samples;
    int frames = kDefaultFramesToShow + kDefaultFramesToSkip;

    WTFGetBacktrace(samples.data(), &frames);
    if (frames > kDefaultFramesToSkip)
        WTFPrintBacktrace(std::span { samples }.subspan(kDefaultFramesToSkip, frames - kDefaultFramesToSkip));
    else
        CrashLogPrintStream { }.print("no stacktrace available");
}

void WTFPrintBacktraceWithPrefixAndPrintStream(PrintStream& out, std::span<void* const> stack, const char* prefix)
{
    out.print(StackTracePrinter { stack, prefix });
}

void WTFPrintBacktrace(std::span<void* const> stack)
{
    CrashLogPrintStream out;
    WTFPrintBacktraceWithPrefixAndPrintStream(out, stack, "");
}

#if !defined(NDEBUG) || !(OS(DARWIN) || PLATFORM(PLAYSTATION))
void WTFCrash()
{
#if ASAN_ENABLED
    __builtin_trap();
#elif OS(DARWIN) && CPU(ARM64)
    WTFBreakpointTrap();
    __builtin_trap(); // Suppress NO_RETURN_DUE_TO_CRASH warning.
#else
    *(int *)(uintptr_t)0xbbadbeef = 0;
    // More reliable, but doesn't say BBADBEEF.
    __builtin_trap();
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
#endif // !defined(NDEBUG) || !(OS(DARWIN) || PLATFORM(PLAYSTATION))

#if ENABLE(CONJECTURE_ASSERT)
int wtfConjectureAssertIsEnabled = 0;

void WTFCrashDueToConjectureAssert(const char* file, int line, const char* function, const char* assertion)
{
    printf_stderr_common("CONJECTURE ASSERTION FAILED: %s\n", assertion);
    printCallSite(file, line, function);
    WTFReportBacktrace();
    CRASH();
}
#endif

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
    WTF_MAKE_FAST_ALLOCATED;
public:
    void accumulate(const String&);
    void resetAccumulatedLogs();
    String getAndResetAccumulatedLogs();

private:
    Lock accumulatorLock;
    StringBuilder loggingAccumulator WTF_GUARDED_BY_LOCK(accumulatorLock);
};

void WTFLoggingAccumulator::accumulate(const String& log)
{
    Locker locker { accumulatorLock };
    loggingAccumulator.append(log);
}

void WTFLoggingAccumulator::resetAccumulatedLogs()
{
    Locker locker { accumulatorLock };
    loggingAccumulator.clear();
}

String WTFLoggingAccumulator::getAndResetAccumulatedLogs()
{
    Locker locker { accumulatorLock };
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
    return channel->level >= level && channel->state != WTFLogChannelState::Off;
}

void WTFLogWithLevel(WTFLogChannel* channel, WTFLogLevel level, const char* format, ...)
{
    if (level != WTFLogLevel::Always && level > channel->level)
        return;

    if (channel->level != WTFLogLevel::Always && channel->state == WTFLogChannelState::Off)
        return;

    va_list args;
    va_start(args, format);

ALLOW_NONLITERAL_FORMAT_BEGIN
    WTFLog(channel, format, args);
ALLOW_NONLITERAL_FORMAT_END

    va_end(args);
}

WTF_ATTRIBUTE_PRINTF(2, 0)
static void WTFLogVaList(WTFLogChannel* channel, const char* format, va_list args)
{
    if (channel->state == WTFLogChannelState::Off)
        return;

    if (channel->state == WTFLogChannelState::On) {
        vprintf_stderr_with_trailing_newline(channel, format, args);
        return;
    }

    ASSERT(channel->state == WTFLogChannelState::OnWithAccumulation);

ALLOW_NONLITERAL_FORMAT_BEGIN
    String loggingString = WTF::createWithFormatAndArguments(format, args);
ALLOW_NONLITERAL_FORMAT_END

    if (!loggingString.endsWith('\n'))
        loggingString = makeString(loggingString, '\n');

    loggingAccumulator().accumulate(loggingString);

    logToStderr(channel, loggingString.utf8().data());
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
    if (channel->state != WTFLogChannelState::On)
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
    vprintf_stderr_with_trailing_newline(nullptr, format, args);
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

    return nullptr;
}

static void setStateOfAllChannels(WTFLogChannel* channels[], size_t channelCount, WTFLogChannelState state)
{
    for (size_t i = 0; i < channelCount; ++i)
        channels[i]->state = state;
}

void WTFInitializeLogChannelStatesFromString(WTFLogChannel* channels[], size_t count, const char* logLevel)
{
#if USE(OS_LOG) && !RELEASE_LOG_DISABLED
    for (size_t i = 0; i < count; ++i) {
        WTFLogChannel* channel = channels[i];
        channel->osLogChannel = os_log_create(channel->subsystem, channel->name);
    }
#endif

    for (auto logLevelComponent : StringView::fromLatin1(logLevel).split(',')) {
        auto componentInfo = logLevelComponent.split('=');
        auto it = componentInfo.begin();
        if (it == componentInfo.end())
            continue;

        auto component = (*it).trim(isUnicodeCompatibleASCIIWhitespace<UChar>);

        WTFLogChannelState logChannelState = WTFLogChannelState::On;
        if (component.startsWith('-')) {
            logChannelState = WTFLogChannelState::Off;
            component = component.substring(1);
        }

        if (equalLettersIgnoringASCIICase(component, "all"_s)) {
            setStateOfAllChannels(channels, count, logChannelState);
            continue;
        }

        WTFLogLevel logChannelLevel = WTFLogLevel::Error;
        if (++it != componentInfo.end()) {
            auto level = (*it).trim(isUnicodeCompatibleASCIIWhitespace<UChar>);
            if (equalLettersIgnoringASCIICase(level, "error"_s))
                logChannelLevel = WTFLogLevel::Error;
            else if (equalLettersIgnoringASCIICase(level, "warning"_s))
                logChannelLevel = WTFLogLevel::Warning;
            else if (equalLettersIgnoringASCIICase(level, "info"_s))
                logChannelLevel = WTFLogLevel::Info;
            else if (equalLettersIgnoringASCIICase(level, "debug"_s))
                logChannelLevel = WTFLogLevel::Debug;
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

} // extern "C"

#if (OS(DARWIN) || PLATFORM(PLAYSTATION)) && (CPU(X86_64) || CPU(ARM64))

void WTFCrashWithInfoImpl(int, const char*, const char*, int, uint64_t reason, uint64_t misc1, uint64_t misc2, uint64_t misc3, uint64_t misc4, uint64_t misc5, uint64_t misc6)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    register uint64_t misc2GPR asm(CRASH_GPR2) = misc2;
    register uint64_t misc3GPR asm(CRASH_GPR3) = misc3;
    register uint64_t misc4GPR asm(CRASH_GPR4) = misc4;
    register uint64_t misc5GPR asm(CRASH_GPR5) = misc5;
    register uint64_t misc6GPR asm(CRASH_GPR6) = misc6;
    __asm__ volatile (WTF_FATAL_CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR), "r"(misc2GPR), "r"(misc3GPR), "r"(misc4GPR), "r"(misc5GPR), "r"(misc6GPR));
    __builtin_unreachable();
}

void WTFCrashWithInfoImpl(int, const char*, const char*, int, uint64_t reason, uint64_t misc1, uint64_t misc2, uint64_t misc3, uint64_t misc4, uint64_t misc5)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    register uint64_t misc2GPR asm(CRASH_GPR2) = misc2;
    register uint64_t misc3GPR asm(CRASH_GPR3) = misc3;
    register uint64_t misc4GPR asm(CRASH_GPR4) = misc4;
    register uint64_t misc5GPR asm(CRASH_GPR5) = misc5;
    __asm__ volatile (WTF_FATAL_CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR), "r"(misc2GPR), "r"(misc3GPR), "r"(misc4GPR), "r"(misc5GPR));
    __builtin_unreachable();
}

void WTFCrashWithInfoImpl(int, const char*, const char*, int, uint64_t reason, uint64_t misc1, uint64_t misc2, uint64_t misc3, uint64_t misc4)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    register uint64_t misc2GPR asm(CRASH_GPR2) = misc2;
    register uint64_t misc3GPR asm(CRASH_GPR3) = misc3;
    register uint64_t misc4GPR asm(CRASH_GPR4) = misc4;
    __asm__ volatile (WTF_FATAL_CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR), "r"(misc2GPR), "r"(misc3GPR), "r"(misc4GPR));
    __builtin_unreachable();
}

void WTFCrashWithInfoImpl(int, const char*, const char*, int, uint64_t reason, uint64_t misc1, uint64_t misc2, uint64_t misc3)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    register uint64_t misc2GPR asm(CRASH_GPR2) = misc2;
    register uint64_t misc3GPR asm(CRASH_GPR3) = misc3;
    __asm__ volatile (WTF_FATAL_CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR), "r"(misc2GPR), "r"(misc3GPR));
    __builtin_unreachable();
}

void WTFCrashWithInfoImpl(int, const char*, const char*, int, uint64_t reason, uint64_t misc1, uint64_t misc2)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    register uint64_t misc2GPR asm(CRASH_GPR2) = misc2;
    __asm__ volatile (WTF_FATAL_CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR), "r"(misc2GPR));
    __builtin_unreachable();
}

void WTFCrashWithInfoImpl(int, const char*, const char*, int, uint64_t reason, uint64_t misc1)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    register uint64_t misc1GPR asm(CRASH_GPR1) = misc1;
    __asm__ volatile (WTF_FATAL_CRASH_INST : : "r"(reasonGPR), "r"(misc1GPR));
    __builtin_unreachable();
}

void WTFCrashWithInfoImpl(int, const char*, const char*, int, uint64_t reason)
{
    register uint64_t reasonGPR asm(CRASH_GPR0) = reason;
    __asm__ volatile (WTF_FATAL_CRASH_INST : : "r"(reasonGPR));
    __builtin_unreachable();
}

#else

void WTFCrashWithInfoImpl(int, const char*, const char*, int, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) { CRASH(); }
void WTFCrashWithInfoImpl(int, const char*, const char*, int, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) { CRASH(); }
void WTFCrashWithInfoImpl(int, const char*, const char*, int, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) { CRASH(); }
void WTFCrashWithInfoImpl(int, const char*, const char*, int, uint64_t, uint64_t, uint64_t, uint64_t) { CRASH(); }
void WTFCrashWithInfoImpl(int, const char*, const char*, int, uint64_t, uint64_t, uint64_t) { CRASH(); }
void WTFCrashWithInfoImpl(int, const char*, const char*, int, uint64_t, uint64_t) { CRASH(); }
void WTFCrashWithInfoImpl(int, const char*, const char*, int, uint64_t) { CRASH(); }

#endif // (OS(DARWIN) || PLATFORM(PLAYSTATION)) && (CPU(X64_64) || CPU(ARM64))

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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
