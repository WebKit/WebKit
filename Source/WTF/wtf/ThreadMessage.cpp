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
#include "ThreadMessage.h"

#if USE(PTHREADS)

#include <fcntl.h>
#include <unistd.h>

#include <wtf/DataLog.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/threads/Signals.h>

#if HAVE(MACH_EXCEPTIONS)
#include <mach/thread_act.h>
#endif

namespace WTF {

using Node = LocklessBag<ThreadMessageData*>::Node;

class ThreadMessageData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ThreadMessageData(const ThreadMessage& m)
        : ran(nullptr)
        , message(m)
    {
    }

    Atomic<Node*> ran;
    const ThreadMessage& message;
};

enum FileDescriptor {
    Read,
    Write,
    NumberOfFileDescriptors,
};

static int fileDescriptors[NumberOfFileDescriptors];
static const char* const magicByte = "d";


void initializeThreadMessages()
{
    int result = pipe(fileDescriptors);
    RELEASE_ASSERT(!result);

    int flags = fcntl(fileDescriptors[Write], F_GETFL);
    result = fcntl(fileDescriptors[Write], F_SETFL, flags | O_NONBLOCK | O_APPEND);
    flags = fcntl(fileDescriptors[Write], F_GETFL);
    RELEASE_ASSERT(result != -1);
    RELEASE_ASSERT((flags & O_NONBLOCK) && (flags & O_APPEND));

    flags = fcntl(fileDescriptors[Read], F_GETFL);
    result = fcntl(fileDescriptors[Read], F_SETFL, flags & ~O_NONBLOCK);
    flags = fcntl(fileDescriptors[Read], F_GETFL);
    RELEASE_ASSERT(result != -1);
    RELEASE_ASSERT(!(flags & O_NONBLOCK));
}

SUPPRESS_ASAN
static MessageStatus sendMessageUsingSignal(Thread& thread, const ThreadMessage& message)
{
    constexpr Signal signal = Signal::Usr;
    static std::once_flag once;
    std::call_once(once, [] {
        installSignalHandler(signal, [] (Signal, SigInfo&, PlatformRegisters& registers) {
            Thread* thread = Thread::currentMayBeNull();

            if (!thread) {
                dataLogLn("We somehow got a message on a thread that didn't have a WTF::Thread initialized, we probably deadlocked, halp.");
                return SignalAction::NotHandled;
            }

            // Node should be deleted in the sender thread. Deleting Nodes in signal handler causes dead lock.
            thread->threadMessages().consumeAllWithNode([&] (ThreadMessageData* data, Node* node) {
                data->message(registers);
                // By setting ran variable, (1) the sender acknowledges the completion and
                // (2) gets the Node to be deleted.
                data->ran.store(node);
            });

            while (write(fileDescriptors[Write], magicByte, 1) == -1)
                ASSERT(errno == EAGAIN);

            return SignalAction::Handled;
        });
    });


    // Since we are guarenteed not to return until we get a response from the other thread this is ok.
    ThreadMessageData data(message);

    thread.threadMessages().add(&data);
    bool result = thread.signal(toSystemSignal(signal));
    if (!result)
        return MessageStatus::ThreadExited;
    RELEASE_ASSERT(result);

    static StaticLock readLock;
    while (true) {
        LockHolder locker(readLock);
        constexpr size_t bufferSize = 16;
        char buffer[bufferSize];

        // It's always safe to clear the pipe because only one thread can ever block trying to read
        // from the pipe. Thus, each byte we clear from the pipe actually just corresponds to some task
        // that has already finished. We actively want to ensure that the pipe does not overfill because
        // otherwise our writers might spin trying to write.
        auto clearPipe = [&] {
            int flags = fcntl(fileDescriptors[Read], F_GETFL);
            ASSERT(!(flags & O_NONBLOCK));
            fcntl(fileDescriptors[Read], F_SETFL, flags | O_NONBLOCK);

            while (read(fileDescriptors[Read], buffer, bufferSize) != -1) { }
            ASSERT(errno == EAGAIN);

            fcntl(fileDescriptors[Read], F_SETFL, flags);
        };

        if (Node* node = data.ran.load()) {
            clearPipe();
            delete node;
            return MessageStatus::MessageRan;
        }

        int ret = read(fileDescriptors[Read], buffer, 1);
        UNUSED_PARAM(ret);
        ASSERT(buffer[0] == magicByte[0]);
        clearPipe();
    }
    RELEASE_ASSERT_NOT_REACHED();
}

#if HAVE(MACH_EXCEPTIONS)
static MessageStatus sendMessageUsingMach(Thread& thread, const ThreadMessage& message)
{
    static StaticLock messageLock;
    auto lockholder = holdLock(messageLock);

    auto result = thread.suspend();
    if (!result)
        return MessageStatus::ThreadExited;

#if CPU(X86)
    unsigned userCount = sizeof(PlatformRegisters) / sizeof(int);
    thread_state_flavor_t flavor = i386_THREAD_STATE;
#elif CPU(X86_64)
    unsigned userCount = x86_THREAD_STATE64_COUNT;
    thread_state_flavor_t flavor = x86_THREAD_STATE64;
#elif CPU(ARM)
    unsigned userCount = ARM_THREAD_STATE_COUNT;
    thread_state_flavor_t flavor = ARM_THREAD_STATE;
#elif CPU(ARM64)
    unsigned userCount = ARM_THREAD_STATE64_COUNT;
    thread_state_flavor_t flavor = ARM_THREAD_STATE64;
#else
#error Unknown Architecture
#endif

    PlatformRegisters registers;
    thread_state_t state = reinterpret_cast<thread_state_t>(&registers);
    thread_get_state(thread.machThread(), flavor, state, &userCount);

    message(registers);

    thread_set_state(thread.machThread(), flavor, state, userCount);

    thread.resume();
    return MessageStatus::MessageRan;
}

static bool useMach = false;
void deliverMessagesUsingMach()
{
    useMach = true;
}

#endif // HAVE(MACH_EXCEPTIONS)

MessageStatus sendMessageScoped(Thread& thread, const ThreadMessage& message)
{
#if HAVE(MACH_EXCEPTIONS)
    if (useMach)
        return sendMessageUsingMach(thread, message);
#endif
    return sendMessageUsingSignal(thread, message);
}


} // namespace WTF

#endif // USE(PTHREADS)
