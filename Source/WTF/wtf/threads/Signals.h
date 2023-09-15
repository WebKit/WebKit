/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/PlatformRegisters.h>

#if OS(UNIX)

#include <signal.h>
#include <tuple>

#if HAVE(MACH_EXCEPTIONS)
#include <mach/exception_types.h>
#endif
#endif // OS(UNIX)

namespace WTF {

// Note that SIGUSR1 is used in Pthread-based ports except for Darwin to suspend and resume threads.
enum class Signal {
#if OS(UNIX)
    // Usr will always chain to any non-default handler install before us. Since there is no way to know
    // if a signal was intended exclusively for us.
    Usr,

    // These signals will only chain if we don't have a handler that can process them. If there is nothing
    // to chain to we restore the default handler and crash.
#if !OS(DARWIN)
    Abort,
#endif
    FloatingPoint,
    Breakpoint, // Be VERY careful with this, installing a handler for this will break lldb/gdb.
    IllegalInstruction,
    AccessFault, // For posix this is both SIGSEGV and SIGBUS
    NumberOfSignals = AccessFault + 2, // AccessFault is really two signals.
    Unknown = NumberOfSignals
#else
    FloatingPoint,
    IllegalInstruction,
    AccessFault,
    NumberOfSignals = AccessFault + 1,
    Unknown = NumberOfSignals
#endif
};

enum class SignalAction {
    Handled,
    NotHandled,
    ForceDefault
};

struct SigInfo {
    void* faultingAddress { 0 };
};

using SignalHandler = Function<SignalAction(Signal, SigInfo&, PlatformRegisters&)>;
using SignalHandlerMemory = std::aligned_storage<sizeof(SignalHandler), std::alignment_of<SignalHandler>::value>::type;

struct SignalHandlers {
    static void initialize();

    void add(Signal, SignalHandler&&);
    template<typename Func>
    void forEachHandler(Signal, const Func&) const;

    static constexpr size_t numberOfSignals = static_cast<size_t>(Signal::NumberOfSignals);
    static constexpr size_t maxNumberOfHandlers = 4;

    static_assert(numberOfSignals < std::numeric_limits<uint8_t>::max());

#if HAVE(MACH_EXCEPTIONS)
    mach_port_t exceptionPort;
    exception_mask_t addedExceptions;
    bool useMach;

    enum class InitState : uint8_t {
        Uninitialized = 0,
        InitializedHandlerThread,
        AddedHandlers
    };
    InitState initState;
#else
    static constexpr bool useMach = false;
#endif
    uint8_t numberOfHandlers[numberOfSignals];
    SignalHandlerMemory handlers[numberOfSignals][maxNumberOfHandlers];

#if OS(UNIX)
    struct sigaction oldActions[numberOfSignals];
#endif
};

// Call this method whenever you want to add a signal handler. This function needs to be called
// before g_wtfConfig is frozen. After the g_wtfConfig is frozen, no additional signal handlers may
// be installed. Any attempt to do so will trigger a crash.
// Note: Your signal handler will be called every time the handler for the desired signal is called.
// Thus it is your responsibility to discern if the signal fired was yours.
// These functions are a one way street i.e. once installed, a signal handler cannot be uninstalled
// and once commited they can't be turned off.
WTF_EXPORT_PRIVATE void addSignalHandler(Signal, SignalHandler&&);
WTF_EXPORT_PRIVATE void activateSignalHandlersFor(Signal);


#if HAVE(MACH_EXCEPTIONS)
class Thread;
void registerThreadForMachExceptionHandling(Thread&);
WTF_EXPORT_PRIVATE void initMachExceptionHandlerThread(bool);
inline void initializeSignalHandling() { initMachExceptionHandlerThread(true); }
inline void disableSignalHandling() { initMachExceptionHandlerThread(false); }

void handleSignalsWithMach();
#else
inline void initializeSignalHandling() { }
inline void disableSignalHandling() { }
#endif // HAVE(MACH_EXCEPTIONS)

} // namespace WTF

#if HAVE(MACH_EXCEPTIONS)
using WTF::registerThreadForMachExceptionHandling;
using WTF::handleSignalsWithMach;
#endif // HAVE(MACH_EXCEPTIONS)

using WTF::Signal;
using WTF::SigInfo;
using WTF::SignalAction;
using WTF::SignalHandler;
using WTF::addSignalHandler;
using WTF::activateSignalHandlersFor;
using WTF::initializeSignalHandling;
using WTF::disableSignalHandling;
