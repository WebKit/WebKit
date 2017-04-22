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

#if USE(PTHREADS)

#include <signal.h>
#include <wtf/Function.h>

namespace WTF {

// Note that SIGUSR1 is used in Pthread-based ports except for Darwin to suspend and resume threads.
enum class Signal {
    // Usr will always chain to any non-default handler install before us. Since there is no way to know
    // if a signal was intended exclusively for us.
    Usr,

    // These signals will only chain if we don't have a handler that can process them. If there is nothing
    // to chain to we restore the default handler and crash.
    Trap,
    Ill,
    SegV,
    Bus,
    NumberOfSignals,
    Unknown = NumberOfSignals
};

inline int toSystemSignal(Signal signal)
{
    switch (signal) {
    case Signal::SegV: return SIGSEGV;
    case Signal::Bus: return SIGBUS;
    case Signal::Ill: return SIGILL;
    case Signal::Usr: return SIGUSR2;
    case Signal::Trap: return SIGTRAP;
    default: break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline Signal fromSystemSignal(int signal)
{
    switch (signal) {
    case SIGSEGV: return Signal::SegV;
    case SIGBUS: return Signal::Bus;
    case SIGILL: return Signal::Ill;
    case SIGUSR2: return Signal::Usr;
    case SIGTRAP: return Signal::Trap;
    default: return Signal::Unknown;
    }
}

enum class SignalAction {
    Handled,
    NotHandled,
    ForceDefault
};

using SignalHandler = Function<SignalAction(int, siginfo_t*, void*)>;

// Call this method whenever you want to install a signal handler. It's ok to call this function lazily.
// Note: Your signal handler will be called every time the handler for the desired signal is called.
// Thus it is your responsibility to discern if the signal fired was yours.
// This function is currently a one way street i.e. once installed, a signal handler cannot be uninstalled.
void installSignalHandler(Signal, SignalHandler&&);

} // namespace WTF

using WTF::Signal;
using WTF::toSystemSignal;
using WTF::fromSystemSignal;
using WTF::SignalAction;
using WTF::installSignalHandler;

#endif // USE(PTHREADS)
