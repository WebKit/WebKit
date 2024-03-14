/*
 * Copyright (C) 2023 Ian Grunert <ian.grunert@gmail.com>
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
#include <wtf/threads/Signals.h>

#if OS(WINDOWS)

#include <cstdio>
#include <mutex>
#include <signal.h>
#include <winnt.h>

#include <wtf/Atomics.h>
#include <wtf/DataLog.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/PlatformRegisters.h>
#include <wtf/ThreadGroup.h>
#include <wtf/Threading.h>
#include <wtf/WTFConfig.h>

namespace WTF {

void SignalHandlers::add(Signal signal, SignalHandler&& handler)
{
    Config::AssertNotFrozenScope assertScope;
    static Lock lock;
    Locker locker { lock };

    size_t signalIndex = static_cast<size_t>(signal);
    size_t nextFree = numberOfHandlers[signalIndex];
    RELEASE_ASSERT(nextFree < maxNumberOfHandlers);
    SignalHandlerMemory* memory = &handlers[signalIndex][nextFree];
    new (memory) SignalHandler(WTFMove(handler));

    // We deliberately do not want to increment the count until after we've
    // fully initialized the memory. This way, forEachHandler() won't see a
    // partially initialized handler.
    storeStoreFence();
    numberOfHandlers[signalIndex]++;
    loadLoadFence();
}

template<typename Func>
inline void SignalHandlers::forEachHandler(Signal signal, const Func& func) const
{
    size_t signalIndex = static_cast<size_t>(signal);
    size_t handlerIndex = numberOfHandlers[signalIndex];
    while (handlerIndex--) {
        auto* memory = const_cast<SignalHandlerMemory*>(&handlers[signalIndex][handlerIndex]);
        const SignalHandler& handler = *bitwise_cast<SignalHandler*>(memory);
        func(handler);
    }
}

inline Signal fromSystemException(DWORD signal)
{
    switch (signal) {
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_UNDERFLOW:
        return Signal::FloatingPoint;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return Signal::IllegalInstruction;
    case EXCEPTION_ACCESS_VIOLATION:
        return Signal::AccessFault;
    default:
        return Signal::Unknown;
    }
}

LONG WINAPI vectoredHandler(struct _EXCEPTION_POINTERS *exceptionInfo)
{
    Signal signal = fromSystemException(exceptionInfo->ExceptionRecord->ExceptionCode);
    SignalHandlers& handlers = g_wtfConfig.signalHandlers;

    SigInfo sigInfo;
    if (signal == Signal::AccessFault) {
        // The second array element specifies the virtual address of the inaccessible data.
        // https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-exception_record
        sigInfo.faultingAddress = reinterpret_cast<void*>(exceptionInfo->ExceptionRecord->ExceptionInformation[1]);
    }

    PlatformRegisters& registers = *(exceptionInfo->ContextRecord);

    long result = EXCEPTION_EXECUTE_HANDLER;
    handlers.forEachHandler(signal, [&] (const SignalHandler& handler) {
        switch (handler(signal, sigInfo, registers)) {
        case SignalAction::Handled:
            result = EXCEPTION_CONTINUE_EXECUTION;
            break;
        default:
            break;
        }
    });

    return result;
}

void addSignalHandler(Signal signal, SignalHandler&& handler)
{
    Config::AssertNotFrozenScope assertScope;
    SignalHandlers& handlers = g_wtfConfig.signalHandlers;
    ASSERT(signal < Signal::Unknown);

    static std::once_flag initializeOnceFlags[static_cast<size_t>(Signal::NumberOfSignals)];
    std::call_once(initializeOnceFlags[static_cast<size_t>(signal)], [&] {
        Config::AssertNotFrozenScope assertScope;
        AddVectoredExceptionHandler(1, vectoredHandler);
    });

    handlers.add(signal, WTFMove(handler));
}

void activateSignalHandlersFor(Signal signal)
{
    UNUSED_PARAM(signal);
}

void SignalHandlers::initialize()
{
    // noop
}

} // namespace WTF

#endif // OS(WINDOWS)
