/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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
#include <mach/port.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach/thread_status.h>
#endif

#if OS(DARWIN)
#include <mach/vm_param.h>
#endif

#include <unistd.h>
#include <wtf/Atomics.h>
#include <wtf/CryptographicallyRandomNumber.h>
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

    ASSERT(signal < Signal::NumberOfSignals);
    ASSERT(!useMach || signal != Signal::Usr);
    RELEASE_ASSERT(initState == SignalHandlers::InitState::Initializing);

    size_t signalIndex = static_cast<size_t>(signal);
    size_t nextFree = numberOfHandlers[signalIndex];
#if HAVE(MACH_EXCEPTIONS)
    if (signal != Signal::Usr)
        addedExceptions |= toMachMask(signal);
#endif
    RELEASE_ASSERT(nextFree < maxNumberOfHandlers);
    SignalHandlerMemory* memory = &handlers[signalIndex][nextFree];
    new (memory) SignalHandler(WTFMove(handler));

    numberOfHandlers[signalIndex]++;
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

#if CPU(ARM64E) && HAVE(HARDENED_MACH_EXCEPTIONS)
// Our secret key which we use as random diversifier when signing our return PC in the handler callbacks.
// Be VERY careful to clear this before any web content is loaded.
static uint32_t secretSigningKey;

void* SignalHandlers::presignReturnPCForHandler(CodePtr<NoPtrTag> returnPC)
{
    ASSERT(initState < SignalHandlers::InitState::Finalized);
    uint64_t diversifier = ptrauth_blend_discriminator(reinterpret_cast<void*>(secretSigningKey), ptrauth_string_discriminator("pc"));
    return ptrauth_sign_unauthenticated(returnPC.untaggedPtr(), ptrauth_key_function_pointer, diversifier);
}
#endif

static constexpr size_t maxMessageSize = 1 * KB;

static void initMachExceptionHandlerThread()
{
    Config::AssertNotFrozenScope assertScope;
    SignalHandlers& handlers = g_wtfConfig.signalHandlers;
    RELEASE_ASSERT_WITH_MESSAGE(!handlers.exceptionPort, "Mach exception handler thread was already created");
    ASSERT(handlers.useMach);

    // We need this because some processes (e.g. WebKit's GPU process) don't allow signal handling in their
    // sandbox profiles. We don't use them there so there's no point in setting up a dispatch queue we're
    // never going to use.
    if (!handlers.addedExceptions)
        return;

    uint16_t flags = MPO_INSERT_SEND_RIGHT;

    // This provisional flag can be removed once macos sonoma is no longer supported
#ifdef MPO_PROVISIONAL_ID_PROT_OPTOUT
    flags |= MPO_PROVISIONAL_ID_PROT_OPTOUT;
#endif

#if CPU(ARM64) && HAVE(HARDENED_MACH_EXCEPTIONS)
    flags |= MPO_EXCEPTION_PORT;
#endif

    mach_port_options_t options { };
    options.flags = flags;

    kern_return_t kr = mach_port_construct(mach_task_self(), &options, 0, &handlers.exceptionPort);
    RELEASE_ASSERT(kr == KERN_SUCCESS);

#if CPU(ARM64) && HAVE(HARDENED_MACH_EXCEPTIONS)
#if !CPU(ARM64E)
    uint32_t secretSigningKey = 0;
#endif
    uint64_t exceptionsAllowed = handlers.addedExceptions;
    uint64_t behaviorsAllowed = EXCEPTION_STATE_IDENTITY_PROTECTED | MACH_EXCEPTION_CODES;
    uint64_t flavorsAllowed = MACHINE_THREAD_STATE;

    kr = task_register_hardened_exception_handler(current_task(), secretSigningKey, exceptionsAllowed,
        behaviorsAllowed, flavorsAllowed, handlers.exceptionPort);
    if (kr == KERN_SUCCESS)
        handlers.useHardenedHandler = true;
    else {
        dataLog("Failed to register hardened exception handler due to ", mach_error_string(kr));
        if (kr == KERN_DENIED)
            dataLog(" consider adding `task_register_hardened_exception_handler` and `thread_adopt_exception_handler` to your sandbox");
        dataLogLn();
    }

    // Clear the key since we no longer need it anymore and we don't want an attacker to find it.
    secretSigningKey = 0;
#endif

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

static kern_return_t runSignalHandlers(Signal signal, PlatformRegisters& registers, mach_msg_type_number_t dataCount, mach_exception_data_t exceptionData)
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

    bool didHandle = false;
    handlers.forEachHandler(signal, [&] (const SignalHandler& handler) {
        SignalAction handlerResult = handler(signal, info, registers);
        didHandle |= handlerResult == SignalAction::Handled;
    });
    return didHandle ? KERN_SUCCESS : KERN_FAILURE;
}

#if defined(EXCEPTION_STATE_IDENTITY_PROTECTED)

kern_return_t catch_mach_exception_raise_state_identity_protected(
    mach_port_t exceptionPort,
    uint64_t threadID,
    mach_port_t taskIDToken,
    exception_type_t exceptionType,
    mach_exception_data_t exceptionData,
    mach_msg_type_number_t dataCount,
    int* stateFlavor,
    const thread_state_t inState,
    mach_msg_type_number_t inStateCount,
    thread_state_t outState,
    mach_msg_type_number_t* outStateCount)
{
    UNUSED_PARAM(threadID);
    UNUSED_PARAM(taskIDToken);
    return catch_mach_exception_raise_state(exceptionPort, exceptionType, exceptionData,
        dataCount, stateFlavor, inState, inStateCount, outState, outStateCount);
}

#endif // defined(EXCEPTION_STATE_IDENTITY_PROTECTED)

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
    ASSERT(g_wtfConfig.isPermanentlyFrozen || g_wtfConfig.disabledFreezingForTesting);
    SignalHandlers& handlers = g_wtfConfig.signalHandlers;
    RELEASE_ASSERT(port == handlers.exceptionPort);
    // If we wanted to distinguish between SIGBUS and SIGSEGV for EXC_BAD_ACCESS on Darwin we could do:
    // if (exceptionData[0] == KERN_INVALID_ADDRESS)
    //    signal = SIGSEGV;
    // else
    //    signal = SIGBUS;
    Signal signal = fromMachException(exceptionType);
    RELEASE_ASSERT(signal != Signal::Unknown);

#if CPU(ARM64E) && HAVE(HARDENED_MACH_EXCEPTIONS)
    ASSERT_WITH_MESSAGE(!secretSigningKey, "The secret key should have been cleared before any exception handlers are run");
#endif

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

    kern_return_t kr = runSignalHandlers(signal, registers, dataCount, exceptionData);
    if (kr != KERN_SUCCESS)
        return kr;

#if CPU(ARM64E) && OS(DARWIN)
    RELEASE_ASSERT(inStateHash == hashThreadState(outState));
#endif
    *outStateCount = inStateCount;
    return KERN_SUCCESS;
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

#if CPU(ARM64) && HAVE(HARDENED_MACH_EXCEPTIONS)
    if (handlers.useHardenedHandler) {
        const exception_behavior_t newBehavior = MACH_EXCEPTION_CODES | EXCEPTION_STATE_IDENTITY_PROTECTED;
        kern_return_t result = thread_adopt_exception_handler(thread.machThread(), handlers.exceptionPort, handlers.addedExceptions & activeExceptions, newBehavior, MACHINE_THREAD_STATE);
        RELEASE_ASSERT(result == KERN_SUCCESS, result, handlers.exceptionPort, handlers.addedExceptions, activeExceptions);
        return;
    }
#endif // CPU(ARM64) && HAVE(HARDENED_MACH_EXCEPTIONS)
    const exception_behavior_t newBehavior = MACH_EXCEPTION_CODES | EXCEPTION_STATE;
    kern_return_t result = thread_set_exception_ports(thread.machThread(), handlers.addedExceptions & activeExceptions, handlers.exceptionPort, newBehavior, MACHINE_THREAD_STATE);
    RELEASE_ASSERT(result == KERN_SUCCESS, result, handlers.exceptionPort, handlers.addedExceptions, activeExceptions);
}

static ThreadGroup& activeThreads()
{
    static LazyNeverDestroyed<std::shared_ptr<ThreadGroup>> activeThreads;
    static std::once_flag initializeKey;
    std::call_once(initializeKey, [&] {
        activeThreads.construct(ThreadGroup::create());
    });
    return (*activeThreads.get());
}

void registerThreadForMachExceptionHandling(Thread& thread)
{
    const SignalHandlers& signalHandlers = g_wtfConfig.signalHandlers;
    RELEASE_ASSERT(signalHandlers.initState >= SignalHandlers::InitState::Initializing, signalHandlers.initState);
    if (!signalHandlers.useMach || !signalHandlers.addedExceptions)
        return;

    Locker locker { activeThreads().getLock() };
    if (activeThreads().add(locker, thread) == ThreadGroupAddResult::NewlyAdded)
        setExceptionPorts(locker, thread);
}

#endif // HAVE(MACH_EXCEPTIONS)

inline std::tuple<int, std::optional<int>> toSystemSignal(Signal signal)
{
    switch (signal) {
    case Signal::AccessFault: return std::make_tuple(SIGSEGV, SIGBUS);
    case Signal::IllegalInstruction: return std::make_tuple(SIGILL, std::nullopt);
    case Signal::Usr: return std::make_tuple(SIGUSR2, std::nullopt);
    case Signal::FloatingPoint: return std::make_tuple(SIGFPE, std::nullopt);
    case Signal::Breakpoint: return std::make_tuple(SIGTRAP, std::nullopt);
#if !OS(DARWIN)
    case Signal::Abort: return std::make_tuple(SIGABRT, std::nullopt);
#endif
    default: break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline Signal fromSystemSignal(int signal)
{
    switch (signal) {
    case SIGSEGV: return Signal::AccessFault;
    case SIGBUS: return Signal::AccessFault;
    case SIGFPE: return Signal::FloatingPoint;
    case SIGTRAP: return Signal::Breakpoint;
    case SIGILL: return Signal::IllegalInstruction;
    case SIGUSR2: return Signal::Usr;
#if !OS(DARWIN)
    case SIGABRT: return Signal::Abort;
#endif
    default: return Signal::Unknown;
    }
}

inline size_t offsetForSystemSignal(int sig)
{
    Signal signal = fromSystemSignal(sig);
    return static_cast<size_t>(signal) + (sig == SIGBUS);
}

void activateSignalHandlersFor(Signal signal)
{
    const SignalHandlers& handlers = g_wtfConfig.signalHandlers;
    RELEASE_ASSERT(handlers.initState >= SignalHandlers::InitState::Initializing);
    ASSERT_UNUSED(signal, signal < Signal::Unknown);

#if HAVE(MACH_EXCEPTIONS)
    if (handlers.useMach) {
        ASSERT(signal != Signal::Usr);
        Locker locker { activeThreads().getLock() };
        if (activeExceptions & toMachMask(signal))
            return;

        ASSERT(handlers.numberOfHandlers[static_cast<uint8_t>(signal)]);
        activeExceptions |= toMachMask(signal);
        // activeExceptions should be a subset of addedExceptions.
        ASSERT(!(activeExceptions & ~handlers.addedExceptions));
        for (auto& thread : activeThreads().threads(locker))
            setExceptionPorts(locker, thread.get());
        return;
    }
#endif
}

void addSignalHandler(Signal signal, SignalHandler&& handler)
{
    g_wtfConfig.signalHandlers.add(signal, WTFMove(handler));
}

static void jscSignalHandler(int sig, siginfo_t* info, void* ucontext)
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
    SignalHandlers& handlers = g_wtfConfig.signalHandlers;
    RELEASE_ASSERT(handlers.initState == SignalHandlers::InitState::Uninitialized);
    handlers.initState = SignalHandlers::InitState::Initializing;

#if CPU(ARM64E) && HAVE(HARDENED_MACH_EXCEPTIONS)
    // Set up our secret key which we use as random diversifier when signing our return PC in the handler callbacks.
    // According to the ARM64 ABI ptrauth_blend_discriminator can't take zero as the first argument.
    static_assert(__DARWIN_ARM_THREAD_STATE64_USER_DIVERSIFIER_MASK == 0xff000000);
    do {
        secretSigningKey = static_cast<uint32_t>(WTF::cryptographicallyRandomNumber<uint8_t>()) << 24;
    } while (!secretSigningKey);
    ASSERT(secretSigningKey == (secretSigningKey & __DARWIN_ARM_THREAD_STATE64_USER_DIVERSIFIER_MASK));
#endif
}

void SignalHandlers::finalize()
{
    Config::AssertNotFrozenScope assertScope;
    SignalHandlers& handlers = g_wtfConfig.signalHandlers;
    RELEASE_ASSERT(handlers.initState == SignalHandlers::InitState::Initializing);
    handlers.initState = SignalHandlers::InitState::Finalized;

#if HAVE(MACH_EXCEPTIONS)
    if (handlers.useMach)
        initMachExceptionHandlerThread();
#endif

    if (!handlers.useMach) {
        for (unsigned i = 0; i < numberOfSignals; ++i) {
            if (!handlers.numberOfHandlers[i])
                continue;

            Signal signal = static_cast<Signal>(i);
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
    }
}

} // namespace WTF

#endif // OS(UNIX)
