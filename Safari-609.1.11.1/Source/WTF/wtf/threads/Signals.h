/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if USE(PTHREADS) && HAVE(MACHINE_CONTEXT)

#include <signal.h>
#include <tuple>
#include <wtf/Function.h>
#include <wtf/Optional.h>
#include <wtf/PlatformRegisters.h>

namespace WTF {

// Note that SIGUSR1 is used in Pthread-based ports except for Darwin to suspend and resume threads.
enum class Signal {
    // Usr will always chain to any non-default handler install before us. Since there is no way to know
    // if a signal was intended exclusively for us.
    Usr,

    // These signals will only chain if we don't have a handler that can process them. If there is nothing
    // to chain to we restore the default handler and crash.
    Ill,
    BadAccess, // For posix this is both SIGSEGV and SIGBUS
    NumberOfSignals = BadAccess + 2, // BadAccess is really two signals.
    Unknown = NumberOfSignals
};

inline std::tuple<int, Optional<int>> toSystemSignal(Signal signal)
{
    switch (signal) {
    case Signal::BadAccess: return std::make_tuple(SIGSEGV, SIGBUS);
    case Signal::Ill: return std::make_tuple(SIGILL, WTF::nullopt);
    case Signal::Usr: return std::make_tuple(SIGILL, WTF::nullopt);
    default: break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline Signal fromSystemSignal(int signal)
{
    switch (signal) {
    case SIGSEGV: return Signal::BadAccess;
    case SIGBUS: return Signal::BadAccess;
    case SIGILL: return Signal::Ill;
    case SIGUSR2: return Signal::Usr;
    default: return Signal::Unknown;
    }
}

enum class SignalAction {
    Handled,
    NotHandled,
    ForceDefault
};

struct SigInfo {
    void* faultingAddress { 0 };
};

using SignalHandler = Function<SignalAction(Signal, SigInfo&, PlatformRegisters&)>;

// Call this method whenever you want to install a signal handler. It's ok to call this function lazily.
// Note: Your signal handler will be called every time the handler for the desired signal is called.
// Thus it is your responsibility to discern if the signal fired was yours.
// This function is currently a one way street i.e. once installed, a signal handler cannot be uninstalled.
WTF_EXPORT_PRIVATE void installSignalHandler(Signal, SignalHandler&&);


#if HAVE(MACH_EXCEPTIONS)
class Thread;
void registerThreadForMachExceptionHandling(Thread&);

void handleSignalsWithMach();
#endif // HAVE(MACH_EXCEPTIONS)

} // namespace WTF

#if HAVE(MACH_EXCEPTIONS)
using WTF::registerThreadForMachExceptionHandling;
using WTF::handleSignalsWithMach;
#endif // HAVE(MACH_EXCEPTIONS)

using WTF::Signal;
using WTF::SigInfo;
using WTF::toSystemSignal;
using WTF::fromSystemSignal;
using WTF::SignalAction;
using WTF::installSignalHandler;

#endif // USE(PTHREADS) && HAVE(MACHINE_CONTEXT)
