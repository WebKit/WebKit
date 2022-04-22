/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include <wtf/threads/BinarySemaphore.h>

namespace TestWebKitAPI {

#if USE(PTHREADS)
TEST(WTF, ThreadingThreadIdentity)
{
    // http://webkit.org/b/32689
    static Thread* thread1;
    static Thread* thread2;

    // Imitate 'foreign' threads that are not created by WTF.
    pthread_t pthread;
    pthread_create(&pthread, nullptr, [] (void*) -> void* {
        thread1 = &Thread::current();
        thread1->ref();
        return nullptr;
    }, nullptr);
    pthread_join(pthread, nullptr);

    pthread_create(&pthread, nullptr, [] (void*) -> void* {
        thread2 = &Thread::current();
        thread2->ref();
        return nullptr;
    }, nullptr);
    pthread_join(pthread, nullptr);

    // Now create another thread using WTF. On OSX, it may have the same pthread handle
    // but should get a different RefPtr<Thread> if the previous RefPtr<Thread> is held.
    Thread::create("DumpRenderTree: test", [] {
        EXPECT_TRUE(thread1 != &Thread::current());
        EXPECT_TRUE(thread2 != &Thread::current());
        EXPECT_TRUE(thread1 != thread2);
        thread1->deref();
        thread2->deref();
    })->waitForCompletion();
}
#endif // USE(PTHREADS)

namespace {
struct AssertionTestHolder {
    RefPtr<Thread> thread;
    size_t counter WTF_GUARDED_BY_CAPABILITY(*thread) { 0 };
    size_t result { 0 }; // This is here to support the result assertion. The compiler doesn't allow us to obtain the `counter` otherwise.

    AssertionTestHolder()
    {
        BinarySemaphore memberInitialized;
        BinarySemaphore threadInitialized;

        thread = Thread::create("com.apple.WebKit.Test.ThreadThreadSafetyAnalysisAssertIsCurrentWorks", [&] {
            memberInitialized.wait(); // Wait for `AssertionTestHolder::thread` assignment to complete.
            threadInitialized.signal();
// Enable to see "writing variable 'counter' requires holding mutex 'thread' exclusively".
// #define TEST_COMPILE_FAILURE
#ifdef TEST_COMPILE_FAILURE
            testTaskThatFailsToCompile<int>();
#endif
            testTask();
            assertIsCurrent(*thread);
            computeResult();
        });
        memberInitialized.signal();
        threadInitialized.wait(); // Wait for the thread to stop using `memberInitialized`, as we will destroy it.
    }
    void testTask()
    {
        assertIsCurrent(*thread); // This is being tested.
        ++counter;
    }
    void computeResult() WTF_REQUIRES_CAPABILITY(*thread) // This is being tested.
    {
        result = ++counter;
    }
    template<typename T> void testTaskThatFailsToCompile()
    {
        ++counter;
    }
};
}

// Consider declaration `RefPtr<Thread> myThread`.
// This test tests that clients can use thread safety analysis to check that the thread is current
// by using `assertIsCurrent(*myThread);` to establish the assertion and WTF_GUARDED_BY_CAPABILITY, WTF_REQUIRES_BY_CAPABILITY
// declarations to let the compiler analyze the uses of the variables and functions with the declarations.
TEST(WTF_Thread, ThreadSafetyAnalysisAssertIsCurrentWorks)
{
    AssertionTestHolder holders[50];
    for (auto& holder : holders)
        holder.thread->waitForCompletion();
    for (auto& holder : holders)
        EXPECT_EQ(2u, holder.result);
}

} // namespace TestWebKitAPI
