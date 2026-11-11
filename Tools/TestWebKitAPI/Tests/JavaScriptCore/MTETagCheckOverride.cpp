/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if CPU(ARM64) && OS(DARWIN)

#include "Helpers/MTASanLauncher.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/VM.h>
#include <wtf/Threading.h>
#include <wtf/threads/BinarySemaphore.h>

// The GC scans stacks through untagged pointers. In a process spawned as MT-ASan, instrumented
// frames tag their stack granules, so the scan only survives if it runs under
// MTETagCheckOverrideScope. These tests tag the granules of a stack frame by hand and force a GC.
// Without the override the scan takes a synchronous tag-check fault and the test process crashes.
//
// In an ordinary process each test runs itself again in a child spawned as MT-ASan.

namespace TestWebKitAPI {

using JSC::HeapType;
using JSC::JSLock;
using JSC::JSLockHolder;
using JSC::VM;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

// A stack buffer whose granules carry a non-zero MTE tag, as an MT-ASan-instrumented frame would leave them.
class TaggedStackBuffer {
    WTF_MAKE_NONCOPYABLE(TaggedStackBuffer);
    WTF_FORBID_HEAP_ALLOCATION;
public:
    TaggedStackBuffer()
    {
        uintptr_t base = reinterpret_cast<uintptr_t>(m_buffer);
        for (size_t offset = 0; offset < sizeof(m_buffer); offset += granuleSize)
            setTag((base | testTag << 56) + offset);
    }

    ~TaggedStackBuffer()
    {
        // Give the granules back their tag so later untagged accesses to this stack don't fault.
        uintptr_t base = reinterpret_cast<uintptr_t>(m_buffer);
        for (size_t offset = 0; offset < sizeof(m_buffer); offset += granuleSize)
            setTag(base + offset);
    }

    // stg does nothing if MTE is off or the memory isn't MTE-backed, which would let the tests
    // pass without testing anything. Read every tag back with ldg and require it to match.
    bool tagsRoundTrip() const
    {
        uintptr_t base = reinterpret_cast<uintptr_t>(m_buffer);
        for (size_t offset = 0; offset < sizeof(m_buffer); offset += granuleSize) {
            uintptr_t loaded = base + offset;
            __asm__ volatile(".arch_extension memtag\nldg %0, [%0]" : "+r"(loaded) : : "memory");
            if ((loaded >> 56 & 0xf) != testTag)
                return false;
        }
        return true;
    }

private:
    static constexpr size_t granuleSize = 16;
    static constexpr uintptr_t testTag = 0x5;

    static ALWAYS_INLINE void setTag(uintptr_t pointer)
    {
        __asm__ volatile(".arch_extension memtag\nstg %0, [%0]" : : "r"(pointer) : "memory");
    }

    alignas(granuleSize) char m_buffer[256];
};

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

NEVER_INLINE static void collectWithTaggedFrame(VM& vm)
{
    TaggedStackBuffer buffer;
    ASSERT_TRUE(buffer.tagsRoundTrip());
    vm.heap.collectSync(JSC::CollectionScope::Full);
}

TEST(JavaScriptCore_MTETagCheckOverride, CurrentThreadTaggedStack)
{
    WTF::initializeMainThread();
    JSC::initialize();
    if (!isMTASanChild()) {
        runCurrentTestInMTASanChild();
        return;
    }

    RefPtr<VM> vm = VM::create(HeapType::Large);
    JSLockHolder locker(*vm);
    collectWithTaggedFrame(*vm);
}

// Another thread's stack is copied while it is suspended, which is a separate read of the tagged granules.
NEVER_INLINE static void waitWithTaggedFrame(VM& vm, BinarySemaphore& ready, BinarySemaphore& done)
{
    TaggedStackBuffer buffer;
    // Not ASSERT: returning early here would leave the main thread waiting on the semaphores.
    EXPECT_TRUE(buffer.tagsRoundTrip());
    // The thread stays registered with the heap, so its stack is still scanned.
    JSLock::DropAllLocks dropAllLocks(vm);
    ready.signal();
    done.wait();
}

TEST(JavaScriptCore_MTETagCheckOverride, OtherThreadTaggedStack)
{
    WTF::initializeMainThread();
    JSC::initialize();
    if (!isMTASanChild()) {
        runCurrentTestInMTASanChild();
        return;
    }

    RefPtr<VM> vm = VM::create(HeapType::Large);
    BinarySemaphore ready;
    BinarySemaphore done;
    auto worker = Thread::create("MTETagCheckOverride worker"_s, [&] {
        JSLockHolder locker(*vm);
        waitWithTaggedFrame(*vm, ready, done);
    });

    ready.wait();
    {
        JSLockHolder locker(*vm);
        vm->heap.collectSync(JSC::CollectionScope::Full);
    }
    done.signal();
    worker->waitForCompletion();
}

} // namespace TestWebKitAPI

#endif // CPU(ARM64) && OS(DARWIN)
