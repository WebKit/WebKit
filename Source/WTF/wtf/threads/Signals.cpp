/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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

#if OS(UNIX)

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
#include <mach/thread_status.h>
#endif

#if OS(DARWIN)
#include <mach/vm_param.h>
#endif

#include <unistd.h>
#include <wtf/Atomics.h>
#include <wtf/DataLog.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/PlatformRegisters.h>
#include <wtf/Scope.h>
#include <wtf/ThreadGroup.h>
#include <wtf/Threading.h>
#include <wtf/TranslatedProcess.h>
#include <wtf/WTFConfig.h>

namespace WTF {

#if HAVE(MACH_EXCEPTIONS)
static exception_mask_t toMachMask(Signal);
#endif

void SignalHandlers::add(Signal signal, SignalHandler&& handler)
{
    Config::AssertNotFrozenScope assertScope;
    static Lock lock;
    Locker locker { lock };

    size_t signalIndex = static_cast<size_t>(signal);
    size_t nextFree = numberOfHandlers[signalIndex];
#if HAVE(MACH_EXCEPTIONS)
    if (signal != Signal::Usr)
        addedExceptions |= toMachMask(signal);
#endif
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

#if HAVE(MACH_EXCEPTIONS)
// You can read more about mach exceptions here:
// http://www.cs.cmu.edu/afs/cs/project/mach/public/doc/unpublished/exception.ps
// and the Mach interface Generator (MiG) here:
// http://www.cs.cmu.edu/afs/cs/project/mach/public/doc/unpublished/mig.ps

static constexpr size_t maxMessageSize = 1 * KB;

void startMachExceptionHandlerThread()
{
    static std::once_flag once;
    std::call_once(once, [] {
        Config::AssertNotFrozenScope assertScope;
        SignalHandlers& handlers = g_wtfConfig.signalHandlers;
        kern_return_t kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &handlers.exceptionPort);
        RELEASE_ASSERT(kr == KERN_SUCCESS);
        kr = mach_port_insert_right(mach_task_self(), handlers.exceptionPort, handlers.exceptionPort, MACH_MSG_TYPE_MAKE_SEND);
        RELEASE_ASSERT(kr == KERN_SUCCESS);

        dispatch_source_t source = dispatch_source_create(
            DISPATCH_SOURCE_TYPE_MACH_RECV, handlers.exceptionPort, 0, DISPATCH_TARGET_QUEUE_DEFAULT);
        RELEASE_ASSERT(source);

        dispatch_source_set_event_handler(source, ^{
            UNUSED_PARAM(source); // Capture a pointer to source in user space to silence the leaks tool.

            kern_return_t kr = mach_msg_server_once(
                mach_exc_server, maxMessageSize, handlers.exceptionPort, MACH_MSG_TIMEOUT_NONE);
            RELEASE_ASSERT(kr == KERN_SUCCESS);
        });

        // No need for a cancel handler because we never destroy exceptionPort.

        dispatch_resume(source);
    });
}

static Signal fromMachException(exception_type_t type)
{
    switch (type) {
    case EXC_BAD_ACCESS: return Signal::AccessFault;
    case EXC_BAD_INSTRUCTION: return Signal::IllegalInstruction;
    case EXC_ARITHMETIC: return Signal::FloatingPoint;
    case EXC_BREAKPOINT: return Signal::Breakpoint;
    default: break;
    }
    return Signal::Unknown;
}

static exception_mask_t toMachMask(Signal signal)
{
    switch (signal) {
    case Signal::AccessFault: return EXC_MASK_BAD_ACCESS;
    case Signal::IllegalInstruction: return EXC_MASK_BAD_INSTRUCTION;
    case Signal::FloatingPoint: return EXC_MASK_ARITHMETIC;
    case Signal::Breakpoint: return EXC_MASK_BREAKPOINT;
    default: break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

#if CPU(ARM64E) && OS(DARWIN)
inline ptrauth_generic_signature_t hashThreadState(const thread_state_t source)
{
    constexpr size_t threadStatePCPointerIndex = (offsetof(arm_unified_thread_state, ts_64) + offsetof(arm_thread_state64_t, __opaque_pc)) / sizeof(uintptr_t);
    constexpr size_t threadStateSizeInPointers = sizeof(arm_unified_thread_state) / sizeof(uintptr_t);

    ptrauth_generic_signature_t hash = 0;

    hash = ptrauth_sign_generic_data(hash, mach_thread_self());

    const uintptr_t* srcPtr = reinterpret_cast<const uintptr_t*>(source);

    // Exclude the __opaque_flags field which is reserved for OS use.
    // __opaque_flags is at the end of the payload.
    for (size_t i = 0; i < threadStateSizeInPointers - 1; ++i) {
        if (i != threadStatePCPointerIndex)
            hash = ptrauth_sign_generic_data(srcPtr[i], hash);
    }
    const uint32_t* cpsrPtr = reinterpret_cast<const uint32_t*>(&srcPtr[threadStateSizeInPointers - 1]);
    hash = ptrauth_sign_generic_data(static_cast<uint64_t>(*cpsrPtr), hash);
    
    return hash;
}
#endif

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

static kern_return_t runSignalHandlers(Signal &signal, PlatformRegisters& registers, bool &didHandle, mach_msg_type_number_t dataCount, mach_exception_data_t exceptionData)
{
    SigInfo info;
    SignalHandlers& handlers = g_wtfConfig.signalHandlers;
    if (signal == Signal::AccessFault) {
        ASSERT_UNUSED(dataCount, dataCount == 2);
        info.faultingAddress = reinterpret_cast<void*>(exceptionData[1]);
#if CPU(ADDRESS64)
        // If the faulting address is out of the range of any valid memory, we would
        // not have any reason to handle it. Just let the default handler take care of it.
        static constexpr unsigned validAddressBits = OS_CONSTANT(EFFECTIVE_ADDRESS_WIDTH);
        static constexpr uintptr_t invalidAddressMask = ~((1ull << validAddressBits) - 1);
        if (bitwise_cast<uintptr_t>(info.faultingAddress) & invalidAddressMask)
            return KERN_FAILURE;
#endif
    }

    handlers.forEachHandler(signal, [&] (const SignalHandler& handler) {
        SignalAction handlerResult = handler(signal, info, registers);
        didHandle |= handlerResult == SignalAction::Handled;
    });
    return KERN_SUCCESS;
}

#ifdef EXCEPTION_IDENTITY_PROTECTED

static thread_act_t threadIDGetThreadPort(uint64_t threadID)
{
    mach_msg_type_number_t threadCount;
    thread_act_array_t threads;
    thread_act_t thread = THREAD_NULL;

    task_threads(mach_task_self(), &threads, &threadCount);
    for (mach_msg_type_number_t i = 0; i < threadCount; i++) {
        if (thread != THREAD_NULL) {
            // clean up all the remaining mach thread ports
            ASSERT(thread != threads[i]);
            mach_port_deallocate(mach_task_self(), threads[i]);
            continue;
        }

        struct thread_identifier_info threadIdentifierInfo;
        unsigned infoCount = THREAD_IDENTIFIER_INFO_COUNT;
        kern_return_t kr = thread_info(threads[i], THREAD_IDENTIFIER_INFO,
            reinterpret_cast<thread_info_t>(&threadIdentifierInfo), &infoCount);

        if (kr == MACH_SEND_INVALID_DEST) {
            // ignore threads that have been destroyed
            continue;
        }
        if (kr != KERN_SUCCESS) {
            dataLogLn("thread_info failed due to ", mach_error_string(kr));
            continue;
        }

        if (threadIdentifierInfo.thread_id == threadID) {
            // we found the thread we are looking for
            thread = threads[i];
            continue;
        }
        // clean up mach thread ports
        mach_port_deallocate(mach_task_self(), threads[i]);
    }
    // clean up array
    vm_deallocate(mach_task_self(), (vm_address_t)threads,
        sizeof(threads[0]) * threadCount);
    return thread;
}

kern_return_t catch_mach_exception_raise_identity_protected(
    mach_port_t exceptionPort,
    uint64_t threadID,
    mach_port_t taskIDToken,
    exception_type_t exceptionType,
    mach_exception_data_t exceptionData,
    mach_msg_type_number_t dataCount)
{
    UNUSED_PARAM(taskIDToken);

    SignalHandlers& handlers = g_wtfConfig.signalHandlers;
    RELEASE_ASSERT(exceptionPort == handlers.exceptionPort);
    // If we wanted to distinguish between SIGBUS and SIGSEGV for EXC_BAD_ACCESS on Darwin we could do:
    // if (exceptionData[0] == KERN_INVALID_ADDRESS)
    //    signal = SIGSEGV;
    // else
    //    signal = SIGBUS;
    Signal signal = fromMachException(exceptionType);
    RELEASE_ASSERT(signal != Signal::Unknown);

    thread_act_t thread = threadIDGetThreadPort(threadID);
    auto clear = makeScopeExit([&] {
        mach_port_deallocate(mach_task_self(), thread);
    });


    mach_msg_type_number_t stateCount = 0;

#if CPU(X86_64) || CPU(X86)
    x86_thread_state_t state;
    int flavor = x86_THREAD_STATE;
    stateCount = x86_THREAD_STATE_COUNT;
#elif CPU(ARM64) || CPU(ARM)
    arm_unified_thread_state state;
    int flavor = ARM_THREAD_STATE;
    stateCount = ARM_UNIFIED_THREAD_STATE_COUNT;
#endif

    kern_return_t kr = thread_get_state(thread, flavor, (thread_state_t)&state, &stateCount);
    if (kr != KERN_SUCCESS) {
        dataLogLn("thread_get_state failed due to ", mach_error_string(kr));
        return kr;
    }

#if CPU(ARM64E) && OS(DARWIN)
    ptrauth_generic_signature_t inStateHash = hashThreadState((thread_state_t)&state);
#endif

#if CPU(X86_64)
    PlatformRegisters& registers = state.uts.ts64;
    RELEASE_ASSERT(state.tsh.flavor == x86_THREAD_STATE64);
#elif CPU(X86)
    PlatformRegisters& registers = state.uts.ts32;
    RELEASE_ASSERT(state.tsh.flavor == x86_THREAD_STATE32);
#elif CPU(ARM64)
    PlatformRegisters& registers = state.ts_64;
    RELEASE_ASSERT(state.ash.flavor == ARM_THREAD_STATE64);
#elif CPU(ARM)
    PlatformRegisters& registers = state.ts_32;
    RELEASE_ASSERT(state.ash.flavor == ARM_THREAD_STATE32);
#endif

    bool didHandle = false;
    kr = runSignalHandlers(signal, registers, didHandle, dataCount, exceptionData);
    if (kr != KERN_SUCCESS)
        return kr;

    if (didHandle) {
#if CPU(ARM64E) && OS(DARWIN)
        RELEASE_ASSERT(inStateHash == hashThreadState((thread_state_t)&state));
#endif
        kr = thread_set_state(thread, flavor, (thread_state_t)&state,
            stateCount);

        if (kr != KERN_SUCCESS)
            dataLogLn("thread_set_state failed due to ", mach_error_string(kr));

        return kr;
    }
    return KERN_FAILURE;
}

#endif // EXCEPTION_IDENTITY_PROTECTED

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
    SignalHandlers& handlers = g_wtfConfig.signalHandlers;
    RELEASE_ASSERT(port == handlers.exceptionPort);
    // If we wanted to distinguish between SIGBUS and SIGSEGV for EXC_BAD_ACCESS on Darwin we could do:
    // if (exceptionData[0] == KERN_INVALID_ADDRESS)
    //    signal = SIGSEGV;
    // else
    //    signal = SIGBUS;
    Signal signal = fromMachException(exceptionType);
    RELEASE_ASSERT(signal != Signal::Unknown);

#if CPU(ARM64E) && OS(DARWIN)
    ptrauth_generic_signature_t inStateHash = hashThreadState(inState);
#endif

    memcpy(outState, inState, inStateCount * sizeof(inState[0]));

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

    bool didHandle = false;
    kern_return_t kr = runSignalHandlers(signal, registers, didHandle, dataCount, exceptionData);
    if (kr != KERN_SUCCESS)
        return kr;

    if (didHandle) {
#if CPU(ARM64E) && OS(DARWIN)
        RELEASE_ASSERT(inStateHash == hashThreadState(outState));
#endif
        *outStateCount = inStateCount;
        return KERN_SUCCESS;
    }

    return KERN_FAILURE;
}

}; // extern "C"

void handleSignalsWithMach()
{
    Config::AssertNotFrozenScope assertScope;
    g_wtfConfig.signalHandlers.useMach = true;
}

static exception_mask_t activeExceptions;
inline void setExceptionPorts(const AbstractLocker& threadGroupLocker, Thread& thread)
{
    UNUSED_PARAM(threadGroupLocker);
    SignalHandlers& handlers = g_wtfConfig.signalHandlers;

#ifdef EXCEPTION_IDENTITY_PROTECTED
    exception_behavior_t newBehavior = MACH_EXCEPTION_CODES;
    if (WTF::isX86BinaryRunningOnARM()) {
        // If we are a translated process in rosetta, use the old exception style
        newBehavior |= EXCEPTION_STATE;
    } else {
        // Otherwise use the new style
        newBehavior |= EXCEPTION_IDENTITY_PROTECTED;
    }
#else
    exception_behavior_t newBehavior = EXCEPTION_STATE | MACH_EXCEPTION_CODES;
#endif // EXCEPTION_IDENTITY_PROTECTED

    kern_return_t result = thread_set_exception_ports(thread.machThread(), handlers.addedExceptions &activeExceptions, handlers.exceptionPort, newBehavior, MACHINE_THREAD_STATE);
    if (result != KERN_SUCCESS) {
        dataLogLn("thread set port failed due to ", mach_error_string(result));
        CRASH();
    }
}

static ThreadGroup& activeThreads()
{
    static LazyNeverDestroyed<Ref<ThreadGroup>> activeThreads;
    static std::once_flag initializeKey;
    std::call_once(initializeKey, [&] {
        Config::AssertNotFrozenScope assertScope;
        activeThreads.construct(ThreadGroup::create());
    });
    return activeThreads.get();
}

void registerThreadForMachExceptionHandling(Thread& thread)
{
    Locker locker { activeThreads().getLock() };
    if (activeThreads().add(locker, thread) == ThreadGroupAddResult::NewlyAdded)
        setExceptionPorts(locker, thread);
}

#endif // HAVE(MACH_EXCEPTIONS)

inline size_t offsetForSystemSignal(int sig)
{
    Signal signal = fromSystemSignal(sig);
    return static_cast<size_t>(signal) + (sig == SIGBUS);
}

static void jscSignalHandler(int, siginfo_t*, void*);

void addSignalHandler(Signal signal, SignalHandler&& handler)
{
    Config::AssertNotFrozenScope assertScope;
    SignalHandlers& handlers = g_wtfConfig.signalHandlers;
    ASSERT(signal < Signal::Unknown);
    ASSERT(!handlers.useMach || signal != Signal::Usr);

#if HAVE(MACH_EXCEPTIONS)
    if (handlers.useMach)
        startMachExceptionHandlerThread();
#endif

    static std::once_flag initializeOnceFlags[static_cast<size_t>(Signal::NumberOfSignals)];
    std::call_once(initializeOnceFlags[static_cast<size_t>(signal)], [&] {
        Config::AssertNotFrozenScope assertScope;
        if (!handlers.useMach) {
            struct sigaction action;
            action.sa_sigaction = jscSignalHandler;
            auto result = sigfillset(&action.sa_mask);
            RELEASE_ASSERT(!result);
            // Do not block this signal since it is used on non-Darwin systems to suspend and resume threads.
            RELEASE_ASSERT(g_wtfConfig.isThreadSuspendResumeSignalConfigured);
            result = sigdelset(&action.sa_mask, g_wtfConfig.sigThreadSuspendResume);
            RELEASE_ASSERT(!result);
            action.sa_flags = SA_SIGINFO;
            auto systemSignals = toSystemSignal(signal);
            result = sigaction(std::get<0>(systemSignals), &action, &handlers.oldActions[offsetForSystemSignal(std::get<0>(systemSignals))]);
            if (std::get<1>(systemSignals))
                result |= sigaction(*std::get<1>(systemSignals), &action, &handlers.oldActions[offsetForSystemSignal(*std::get<1>(systemSignals))]);
            RELEASE_ASSERT(!result);
        }
    });

    handlers.add(signal, WTFMove(handler));
}

void activateSignalHandlersFor(Signal signal)
{
    UNUSED_PARAM(signal);
#if HAVE(MACH_EXCEPTIONS)
    const SignalHandlers& handlers = g_wtfConfig.signalHandlers;
    ASSERT(signal < Signal::Unknown);
    ASSERT(!handlers.useMach || signal != Signal::Usr);

    Locker locker { activeThreads().getLock() };
    if (handlers.useMach) {
        activeExceptions |= toMachMask(signal);

        for (auto& thread : activeThreads().threads(locker))
            setExceptionPorts(locker, thread.get());
    }
#endif
}

void jscSignalHandler(int sig, siginfo_t* info, void* ucontext)
{
    Signal signal = fromSystemSignal(sig);
    SignalHandlers& handlers = g_wtfConfig.signalHandlers;

    auto restoreDefault = [&] {
        struct sigaction defaultAction;
        defaultAction.sa_handler = SIG_DFL;
        sigfillset(&defaultAction.sa_mask);
        defaultAction.sa_flags = 0;
        auto result = sigaction(sig, &defaultAction, nullptr);
        dataLogLnIf(result == -1, "Unable to restore the default handler while processing signal ", sig, " the process is probably deadlocked. (errno: ", errno, ")");
    };

    // This shouldn't happen but we might as well be careful.
    if (signal == Signal::Unknown) {
        dataLogLn("We somehow got called for an unknown signal ", sig, ", help.");
        restoreDefault();
        return;
    }

    SigInfo sigInfo;
    if (signal == Signal::AccessFault)
        sigInfo.faultingAddress = info->si_addr;

#if HAVE(MACHINE_CONTEXT)
    PlatformRegisters& registers = registersFromUContext(reinterpret_cast<ucontext_t*>(ucontext));
#else
    PlatformRegisters registers { };
#endif

    bool didHandle = false;
    bool restoreDefaultHandler = false;
    handlers.forEachHandler(signal, [&] (const SignalHandler& handler) {
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
    struct sigaction& oldAction = handlers.oldActions[static_cast<size_t>(oldActionIndex)];
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

void SignalHandlers::initialize()
{
#if HAVE(MACH_EXCEPTIONS)
    // In production configurations, this does not matter because signal handler
    // installations will always trigger this initialization. However, in debugging
    // configurations, we may end up disabling the use of all signal handlers but
    // we still need this to be initialized. Hence, we need to initialize it
    // eagerly to ensure that it is done before we freeze the WTF::Config.
    activeThreads();
#endif
}

} // namespace WTF

#endif // OS(UNIX)
