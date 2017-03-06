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
#include "WasmFaultSignalHandler.h"

#if ENABLE(WEBASSEMBLY)

#include "ExecutableAllocator.h"
#include "VM.h"
#include "WasmExceptionType.h"

#include <signal.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>

namespace JSC { namespace Wasm {


namespace {
static const bool verbose = false;
}

static struct sigaction oldSigBusHandler;
static struct sigaction oldSigSegvHandler;
static bool fastHandlerInstalled { false };
static StaticLock codeLocationsLock;
static LazyNeverDestroyed<HashSet<std::tuple<VM*, void*, void*>>> codeLocations; // (vm, start, end)

// FIXME: Clean up mcontext_t handling code since it is scattered in heap/, tools/ and wasm/.
// https://bugs.webkit.org/show_bug.cgi?id=169180
#if CPU(X86_64)

#if OS(DARWIN)
#define InstructionPointerGPR context->__ss.__rip
#define FirstArgumentGPR context->__ss.__rsi
#elif OS(FREEBSD)
#define InstructionPointerGPR context.mc_rip
#define FirstArgumentGPR context.mc_rsi
#elif defined(__GLIBC__)
#define InstructionPointerGPR context.gregs[REG_RIP]
#define FirstArgumentGPR context.gregs[REG_RSI]
#endif

#else

#if OS(DARWIN)
#define InstructionPointerGPR context->__ss.__pc
#define FirstArgumentGPR context->__ss.__x[1]
#elif OS(FREEBSD)
#define InstructionPointerGPR context.mc_gpregs.gp_elr
#define FirstArgumentGPR context.mc_gpregs.gp_x[1]
#elif defined(__GLIBC__)
#define InstructionPointerGPR context.pc
#define FirstArgumentGPR context.regs[1]
#endif

#endif

static void trapHandler(int signal, siginfo_t*, void* ucontext)
{
    mcontext_t& context = static_cast<ucontext_t*>(ucontext)->uc_mcontext;
    void* faultingInstruction = reinterpret_cast<void*>(InstructionPointerGPR);
    dataLogLnIf(verbose, "starting handler for fault at: ", RawPointer(faultingInstruction));

    dataLogLnIf(verbose, "JIT memory start: ", RawPointer(reinterpret_cast<void*>(startOfFixedExecutableMemoryPool)), " end: ", RawPointer(reinterpret_cast<void*>(endOfFixedExecutableMemoryPool)));
    // First we need to make sure we are in JIT code before we can aquire any locks. Otherwise,
    // we might have crashed in code that is already holding one of the locks we want to aquire.
    if (reinterpret_cast<void*>(startOfFixedExecutableMemoryPool) <= faultingInstruction
        && faultingInstruction < reinterpret_cast<void*>(endOfFixedExecutableMemoryPool)) {

        LockHolder locker(codeLocationsLock);
        for (auto range : codeLocations.get()) {
            VM* vm;
            void* start;
            void* end;
            std::tie(vm, start, end) = range;
            dataLogLnIf(verbose, "function start: ", RawPointer(start), " end: ", RawPointer(end));
            if (start <= faultingInstruction && faultingInstruction < end) {
                dataLogLnIf(verbose, "found match");
                MacroAssemblerCodeRef exceptionStub = vm->jitStubs->existingCTIStub(throwExceptionFromWasmThunkGenerator);
                // If for whatever reason we don't have a stub then we should just treat this like a regular crash.
                if (!exceptionStub)
                    break;
                dataLogLnIf(verbose, "found stub: ", RawPointer(exceptionStub.code().executableAddress()));
                FirstArgumentGPR = static_cast<uint64_t>(ExceptionType::OutOfBoundsMemoryAccess);
                InstructionPointerGPR = reinterpret_cast<uint64_t>(exceptionStub.code().executableAddress());
                return;
            }
        }
    }

    // Since we only use fast memory in processes we control, if we restore we will just fall back to the default handler.
    if (signal == SIGBUS)
        sigaction(signal, &oldSigBusHandler, nullptr);
    else
        sigaction(signal, &oldSigSegvHandler, nullptr);
}

void registerCode(VM& vm, void* start, void* end)
{
    if (!fastMemoryEnabled())
        return;
    LockHolder locker(codeLocationsLock);
    codeLocations->add(std::make_tuple(&vm, start, end));
}

void unregisterCode(VM& vm, void* start, void* end)
{
    if (!fastMemoryEnabled())
        return;
    LockHolder locker(codeLocationsLock);
    codeLocations->remove(std::make_tuple(&vm, start, end));
}

bool fastMemoryEnabled()
{
    return fastHandlerInstalled;
}

void enableFastMemory()
{
    static std::once_flag once;
    std::call_once(once, [] {
        if (!Options::useWebAssemblyFastMemory())
            return;

        struct sigaction action;

        action.sa_sigaction = trapHandler;
        sigfillset(&action.sa_mask);
        action.sa_flags = SA_SIGINFO;
        
        // Installing signal handlers fails when
        // 1. specificied sig is incorrect (invalid values or signal numbers which cannot be handled), or
        // 2. second or third parameter points incorrect pointers.
        // Thus, we must not fail in the following attempts.
        int ret = 0;
        ret = sigaction(SIGBUS, &action, &oldSigBusHandler);
        ASSERT_UNUSED(ret, !ret);

        ret = sigaction(SIGSEGV, &action, &oldSigSegvHandler);
        ASSERT_UNUSED(ret, !ret);

        codeLocations.construct();
        fastHandlerInstalled = true;
    });
}
    
} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)

