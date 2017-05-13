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

#include "config.h"
#include "Signals.h"

#if USE(PTHREADS)

#include <cstdio>
#include <mutex>
#include <signal.h>
#include <wtf/Atomics.h>
#include <wtf/DataLog.h>
#include <wtf/LocklessBag.h>
#include <wtf/NeverDestroyed.h>

namespace WTF {

static LazyNeverDestroyed<LocklessBag<SignalHandler>> handlers[static_cast<size_t>(Signal::NumberOfSignals)] = { };
static struct sigaction oldActions[static_cast<size_t>(Signal::NumberOfSignals)];
static std::once_flag initializeOnceFlags[static_cast<size_t>(Signal::NumberOfSignals)];

static void jscSignalHandler(int sig, siginfo_t* info, void* mcontext)
{
    Signal signal = fromSystemSignal(sig);

    auto restoreDefault = [&] {
        struct sigaction defaultAction;
        defaultAction.sa_handler = SIG_DFL;
        sigfillset(&defaultAction.sa_mask);
        defaultAction.sa_flags = 0;
        auto result = sigaction(sig, &defaultAction, nullptr);
        dataLogLnIf(result == -1, "Unable to restore the default handler while proccessing signal ", sig, " the process is probably deadlocked. (errno: ", strerror(errno), ")");
    };

    // This shouldn't happen but we might as well be careful.
    if (signal == Signal::Unknown) {
        dataLogLn("We somehow got called for an unknown signal ", sig, ", halp.");
        restoreDefault();
        return;
    }

    bool didHandle = false;
    bool restoreDefaultHandler = false;
    handlers[static_cast<size_t>(signal)]->iterate([&] (const SignalHandler& handler) {
        switch (handler(sig, info, mcontext)) {
        case SignalAction::Handled:
            didHandle = true;
            break;
        case SignalAction::ForceDefault:
            restoreDefaultHandler = true;
            break;
        default:
            break;
        }
    });

    if (restoreDefaultHandler) {
        restoreDefault();
        return;
    }

    struct sigaction& oldAction = oldActions[static_cast<size_t>(signal)];
    if (signal == Signal::Usr) {
        if (oldAction.sa_sigaction)
            oldAction.sa_sigaction(sig, info, mcontext);
        return;
    }

    if (!didHandle) {
        if (oldAction.sa_sigaction) {
            oldAction.sa_sigaction(sig, info, mcontext);
            return;
        }

        restoreDefault();
        return;
    }
}

void installSignalHandler(Signal signal, SignalHandler&& handler)
{
    ASSERT(signal < Signal::Unknown);
    std::call_once(initializeOnceFlags[static_cast<size_t>(signal)], [&] {
        handlers[static_cast<size_t>(signal)].construct();

        struct sigaction action;
        action.sa_sigaction = jscSignalHandler;
        auto result = sigfillset(&action.sa_mask);
        RELEASE_ASSERT(!result);
        action.sa_flags = SA_SIGINFO;
        result = sigaction(toSystemSignal(signal), &action, &oldActions[static_cast<size_t>(signal)]);
        RELEASE_ASSERT(!result);
    });

    handlers[static_cast<size_t>(signal)]->add(WTFMove(handler));
}

} // namespace WTF

#endif // USE(PTHREADS)
