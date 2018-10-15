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
#include <wtf/threads/Signals.h>

#if USE(PTHREADS) && HAVE(MACHINE_CONTEXT)

#if HAVE(MACH_EXCEPTIONS)
extern "C" {
#include "MachExceptionsServer.h"
};
#endif

#include <cstdio>
#include <mutex>
#include <signal.h>

#if HAVE(MACH_EXCEPTIONS)
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <mach/thread_act.h>
#endif

#include <wtf/Atomics.h>
#include <wtf/DataLog.h>
#include <wtf/LocklessBag.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ThreadGroup.h>
#include <wtf/ThreadMessage.h>
#include <wtf/Threading.h>


namespace WTF {

    
static LazyNeverDestroyed<LocklessBag<SignalHandler>> handlers[static_cast<size_t>(Signal::NumberOfSignals)] = { };
static std::once_flag initializeOnceFlags[static_cast<size_t>(Signal::NumberOfSignals)];
static struct sigaction oldActions[static_cast<size_t>(Signal::NumberOfSignals)];

#if HAVE(MACH_EXCEPTIONS)
// You can read more about mach exceptions here:
// http://www.cs.cmu.edu/afs/cs/project/mach/public/doc/unpublished/exception.ps
// and the Mach interface Generator (MiG) here:
// http://www.cs.cmu.edu/afs/cs/project/mach/public/doc/unpublished/mig.ps

static mach_port_t exceptionPort;
static constexpr size_t maxMessageSize = 1 * KB;

static void startMachExceptionHandlerThread()
{
    static std::once_flag once;
    std::call_once(once, [] {
        kern_return_t kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &exceptionPort);
        RELEASE_ASSERT(kr == KERN_SUCCESS);
        kr = mach_port_insert_right(mach_task_self(), exceptionPort, exceptionPort, MACH_MSG_TYPE_MAKE_SEND);
        RELEASE_ASSERT(kr == KERN_SUCCESS);

        dispatch_source_t source = dispatch_source_create(
            DISPATCH_SOURCE_TYPE_MACH_RECV, exceptionPort, 0, DISPATCH_TARGET_QUEUE_DEFAULT);
        RELEASE_ASSERT(source);

        dispatch_source_set_event_handler(source, ^{
            UNUSED_PARAM(source); // Capture a pointer to source in user space to silence the leaks tool.

            kern_return_t kr = mach_msg_server_once(
                mach_exc_server, maxMessageSize, exceptionPort, MACH_MSG_TIMEOUT_NONE);
            RELEASE_ASSERT(kr == KERN_SUCCESS);
        });

        // No need for a cancel handler because we never destroy exceptionPort.

        dispatch_resume(source);
    });
}

static Signal fromMachException(exception_type_t type)
{
    switch (type) {
    case EXC_BAD_ACCESS: return Signal::BadAccess;
    case EXC_BAD_INSTRUCTION: return Signal::Ill;
    default: break;
    }
    return Signal::Unknown;
}

static exception_mask_t toMachMask(Signal signal)
{
    switch (signal) {
    case Signal::BadAccess: return EXC_MASK_BAD_ACCESS;
    case Signal::Ill: return EXC_MASK_BAD_INSTRUCTION;
    default: break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

extern "C" {

// We need to implement stubs for catch_mach_exception_raise and catch_mach_exception_raise_state_identity.
// The MiG generated file will fail to link otherwise, even though we don't use the functions. Only the
// catch_mach_exception_raise_state function should be called because we pass EXCEPTION_STATE to
// thread_set_exception_ports.
kern_return_t catch_mach_exception_raise(mach_port_t, mach_port_t, mach_port_t, exception_type_t, mach_exception_data_t, mach_msg_type_number_t)
{
    dataLogLn("We should not have called catch_exception_raise(), please file a bug at bugs.webkit.org");
    return KERN_FAILURE;
}

kern_return_t catch_mach_exception_raise_state_identity(mach_port_t, mach_port_t, mach_port_t, exception_type_t, mach_exception_data_t, mach_msg_type_number_t, int*, thread_state_t, mach_msg_type_number_t, thread_state_t,  mach_msg_type_number_t*)
{
    dataLogLn("We should not have called catch_mach_exception_raise_state_identity, please file a bug at bugs.webkit.org");
    return KERN_FAILURE;
}

kern_return_t catch_mach_exception_raise_state(
    mach_port_t port,
    exception_type_t exceptionType,
    const mach_exception_data_t exceptionData,
    mach_msg_type_number_t dataCount,
    int* stateFlavor,
    const thread_state_t inState,
    mach_msg_type_number_t inStateCount,
    thread_state_t outState,
    mach_msg_type_number_t* outStateCount)
{
    RELEASE_ASSERT(port == exceptionPort);
    // If we wanted to distinguish between SIGBUS and SIGSEGV for EXC_BAD_ACCESS on Darwin we could do:
    // if (exceptionData[0] == KERN_INVALID_ADDRESS)
    //    signal = SIGSEGV;
    // else
    //    signal = SIGBUS;
    Signal signal = fromMachException(exceptionType);
    RELEASE_ASSERT(signal != Signal::Unknown);

    memcpy(outState, inState, inStateCount * sizeof(inState[0]));
    *outStateCount = inStateCount;

#if CPU(X86_64)
    RELEASE_ASSERT(*stateFlavor == x86_THREAD_STATE);
    PlatformRegisters& registers = reinterpret_cast<x86_thread_state_t*>(outState)->uts.ts64;
#elif CPU(X86)
    RELEASE_ASSERT(*stateFlavor == x86_THREAD_STATE);
    PlatformRegisters& registers = reinterpret_cast<x86_thread_state_t*>(outState)->uts.ts32;
#elif CPU(ARM64)
    RELEASE_ASSERT(*stateFlavor == ARM_THREAD_STATE);
    PlatformRegisters& registers = reinterpret_cast<arm_unified_thread_state*>(outState)->ts_64;
#elif CPU(ARM)
    RELEASE_ASSERT(*stateFlavor == ARM_THREAD_STATE);
    PlatformRegisters& registers = reinterpret_cast<arm_unified_thread_state*>(outState)->ts_32;
#endif

    SigInfo info;
    if (signal == Signal::BadAccess) {
        ASSERT_UNUSED(dataCount, dataCount == 2);
        info.faultingAddress = reinterpret_cast<void*>(exceptionData[1]);
    }

    bool didHandle = false;
    handlers[static_cast<size_t>(signal)]->iterate([&] (const SignalHandler& handler) {
        SignalAction handlerResult = handler(signal, info, registers);
        didHandle |= handlerResult == SignalAction::Handled;
    });

    if (didHandle)
        return KERN_SUCCESS;
    return KERN_FAILURE;
}

};

static bool useMach { false };
void handleSignalsWithMach()
{
    useMach = true;
}


exception_mask_t activeExceptions { 0 };

inline void setExceptionPorts(const AbstractLocker& threadGroupLocker, Thread& thread)
{
    UNUSED_PARAM(threadGroupLocker);
    kern_return_t result = thread_set_exception_ports(thread.machThread(), activeExceptions, exceptionPort, EXCEPTION_STATE | MACH_EXCEPTION_CODES, MACHINE_THREAD_STATE);
    if (result != KERN_SUCCESS) {
        dataLogLn("thread set port failed due to ", mach_error_string(result));
        CRASH();
    }
}

static ThreadGroup& activeThreads()
{
    static std::once_flag initializeKey;
    static ThreadGroup* activeThreadsPtr = nullptr;
    std::call_once(initializeKey, [&] {
        static NeverDestroyed<std::shared_ptr<ThreadGroup>> activeThreads { ThreadGroup::create() };
        activeThreadsPtr = activeThreads.get().get();
    });
    return *activeThreadsPtr;
}

void registerThreadForMachExceptionHandling(Thread& thread)
{
    auto locker = holdLock(activeThreads().getLock());
    if (activeThreads().add(locker, thread) == ThreadGroupAddResult::NewlyAdded)
        setExceptionPorts(locker, thread);
}

#else
static constexpr bool useMach = false;
#endif // HAVE(MACH_EXCEPTIONS)


inline size_t offsetForSystemSignal(int sig)
{
    Signal signal = fromSystemSignal(sig);
    return static_cast<size_t>(signal) + (sig == SIGBUS);
}

static void jscSignalHandler(int, siginfo_t*, void*);

void installSignalHandler(Signal signal, SignalHandler&& handler)
{
    ASSERT(signal < Signal::Unknown);
#if HAVE(MACH_EXCEPTIONS)
    ASSERT(!useMach || signal != Signal::Usr);

    if (useMach)
        startMachExceptionHandlerThread();
#endif

    std::call_once(initializeOnceFlags[static_cast<size_t>(signal)], [&] {
        handlers[static_cast<size_t>(signal)].construct();

        if (!useMach) {
            struct sigaction action;
            action.sa_sigaction = jscSignalHandler;
            auto result = sigfillset(&action.sa_mask);
            RELEASE_ASSERT(!result);
            action.sa_flags = SA_SIGINFO;
            auto systemSignals = toSystemSignal(signal);
            result = sigaction(std::get<0>(systemSignals), &action, &oldActions[offsetForSystemSignal(std::get<0>(systemSignals))]);
            if (std::get<1>(systemSignals))
                result |= sigaction(*std::get<1>(systemSignals), &action, &oldActions[offsetForSystemSignal(*std::get<1>(systemSignals))]);
            RELEASE_ASSERT(!result);
        }

    });

    handlers[static_cast<size_t>(signal)]->add(WTFMove(handler));

#if HAVE(MACH_EXCEPTIONS)
    auto locker = holdLock(activeThreads().getLock());
    if (useMach) {
        activeExceptions |= toMachMask(signal);

        for (auto& thread : activeThreads().threads(locker))
            setExceptionPorts(locker, thread.get());
    }
#endif
}

void jscSignalHandler(int sig, siginfo_t* info, void* ucontext)
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

    SigInfo sigInfo;
    if (signal == Signal::BadAccess)
        sigInfo.faultingAddress = info->si_addr;

    PlatformRegisters& registers = registersFromUContext(reinterpret_cast<ucontext_t*>(ucontext));

    bool didHandle = false;
    bool restoreDefaultHandler = false;
    handlers[static_cast<size_t>(signal)]->iterate([&] (const SignalHandler& handler) {
        switch (handler(signal, sigInfo, registers)) {
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

    unsigned oldActionIndex = static_cast<size_t>(signal) + (sig == SIGBUS);
    struct sigaction& oldAction = oldActions[static_cast<size_t>(oldActionIndex)];
    if (signal == Signal::Usr) {
        if (oldAction.sa_sigaction)
            oldAction.sa_sigaction(sig, info, ucontext);
        return;
    }

    if (!didHandle) {
        if (oldAction.sa_sigaction) {
            oldAction.sa_sigaction(sig, info, ucontext);
            return;
        }

        restoreDefault();
        return;
    }
}

} // namespace WTF

#endif // USE(PTHREADS) && HAVE(MACHINE_CONTEXT)
