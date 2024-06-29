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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <type_traits>
#include <wtf/DataLog.h>
#include <wtf/Threading.h>
#include <wtf/WTFConfig.h>
#if OS(UNIX)
#include <signal.h>
#else
#include <wtf/Gigacage.h>
#include <wtf/OSAllocator.h>
#endif

#if OS(UNIX)
class ReflectedThread : public Thread {
public:
    using Thread::m_mutex;
    using Thread::m_handle;
    using Thread::hasExited;
};

TEST(Signals, SignalsWorkOnExit)
{
    static bool handlerRan = false;
    addSignalHandler(Signal::Usr, [] (Signal signal, SigInfo&, PlatformRegisters&) -> SignalAction {
        RELEASE_ASSERT(signal == Signal::Usr);

        handlerRan = true;
        return SignalAction::Handled;
    });
    activateSignalHandlersFor(Signal::Usr);
    WTF::Config::finalize();

    Atomic<bool> receiverShouldKeepRunning(true);
    Ref<Thread> receiverThread = (Thread::create("ThreadMessage receiver"_s,
        [&receiverShouldKeepRunning] () {
            while (receiverShouldKeepRunning.load()) { }
    }));

    bool signalFired;
    {
        std::unique_lock<WordLock> locker(static_cast<ReflectedThread&>(receiverThread.get()).m_mutex);
        receiverShouldKeepRunning.store(false);
        EXPECT_FALSE(static_cast<ReflectedThread&>(receiverThread.get()).hasExited());
        sleep(1_s);
        signalFired = !pthread_kill(static_cast<ReflectedThread&>(receiverThread.get()).m_handle, SIGUSR2);
    }

    receiverThread->waitForCompletion();
    EXPECT_TRUE(handlerRan || !signalFired);
}
#else
TEST(Signals, SignalsAccessFault)
{
    static bool handlerRan = false;
    addSignalHandler(Signal::AccessFault, [] (Signal signal, SigInfo& sigInfo, PlatformRegisters& context) -> SignalAction {
        RELEASE_ASSERT(signal == Signal::AccessFault);

        handlerRan = true;
#if CPU(ARM)
        context.Pc += 2;
#elif CPU(X86)
        context.Eip += 3;
#elif CPU(X86_64)
        context.Rip += 3;
#else
#error Unsupported CPU
#endif

        return SignalAction::Handled;
    });
    activateSignalHandlersFor(Signal::AccessFault);
    WTF::Config::finalize();

    // Allocate a page of memory
    char* ptr = bitwise_cast<char*>(Gigacage::tryAllocateZeroedVirtualPages(Gigacage::Primitive, 4096));

    // Protect that page - 4096 bytes, not readable or writeable
    OSAllocator::protect(ptr, 4096, false, false);

    // Try and read, triggering an AccessFault
    dataLogLn("Reading from protected memory", static_cast<unsigned>(ptr[0]));

    EXPECT_TRUE(handlerRan);
}
#endif
