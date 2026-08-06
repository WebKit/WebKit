/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2026 Igalia S.L.
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

#if OS(LINUX)
#include <linux/sched/types.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <wtf/linux/HighPriorityThreads.h>
#endif

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
        thread1 = &Thread::currentSingleton();
        thread1->ref();
        return nullptr;
    }, nullptr);
    pthread_join(pthread, nullptr);

    pthread_create(&pthread, nullptr, [] (void*) -> void* {
        thread2 = &Thread::currentSingleton();
        thread2->ref();
        return nullptr;
    }, nullptr);
    pthread_join(pthread, nullptr);

    // Now create another thread using WTF. On OSX, it may have the same pthread handle
    // but should get a different RefPtr<Thread> if the previous RefPtr<Thread> is held.
    Thread::create("DumpRenderTree: test"_s, [] {
        EXPECT_TRUE(thread1 != &Thread::currentSingleton());
        EXPECT_TRUE(thread2 != &Thread::currentSingleton());
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

        thread = Thread::create("com.apple.WebKit.Test.ThreadThreadSafetyAnalysisAssertIsCurrentWorks"_s, [&] {
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


#if OS(LINUX)
namespace {

struct ObservedAttributes {
    int policy;
    int niceLevel;
    uint32_t minUtil;
};

static std::optional<ObservedAttributes> observeSchedulingAttributes(pid_t threadID)
{
    struct sched_attr attributes { };
    attributes.size = sizeof(attributes);
    if (syscall(SYS_sched_getattr, threadID, &attributes, sizeof(attributes), 0))
        return std::nullopt;
    return ObservedAttributes { static_cast<int>(attributes.sched_policy), attributes.sched_nice, attributes.sched_util_min };
}

// Runs a thread at the given QOS and reports the attributes the kernel gave it. The thread waits
// before exiting so that its identifier stays valid while we read them.
static std::optional<ObservedAttributes> attributesForQOS(Thread::QOS qos)
{
    BinarySemaphore running;
    BinarySemaphore mayExit;
    Ref thread = Thread::create("QOSProbe"_s, [&] {
        running.signal();
        mayExit.wait();
    }, ThreadType::Unknown, qos);

    running.wait();
    auto observed = observeSchedulingAttributes(thread->id());
    mayExit.signal();
    thread->waitForCompletion();
    return observed;
}

// The sysctl is only registered when the kernel was built with CONFIG_UCLAMP_TASK.
static bool kernelSupportsUtilizationClamp()
{
    return !access("/proc/sys/kernel/sched_util_clamp_min", R_OK);
}

} // namespace

TEST(WTF_Thread, SchedulingAttributesFollowQOS)
{
    struct Expectation {
        Thread::QOS qos;
        int policy;
        int niceLevel;
        uint32_t minUtil;
    };

    // UserInteractive is left out: HighPriorityThreads asks rtkit to lower its nice level
    // asynchronously, so its nice level is not ours to predict.
    static constexpr Expectation expectations[] = {
        { Thread::QOS::UserInitiated, SCHED_OTHER, 0, 1024 },
        { Thread::QOS::Default, SCHED_OTHER, 0, 204 },
        { Thread::QOS::Utility, SCHED_BATCH, 10, 0 },
        { Thread::QOS::Background, SCHED_IDLE, 19, 0 },
    };

    for (auto& expectation : expectations) {
        auto observed = attributesForQOS(expectation.qos);
        ASSERT_TRUE(observed.has_value());
        EXPECT_EQ(expectation.policy, observed->policy);
        // The kernel only applies sched_nice under SCHED_OTHER and SCHED_BATCH, so a SCHED_IDLE
        // thread keeps whatever nice level it had.
        if (expectation.policy != SCHED_IDLE)
            EXPECT_EQ(expectation.niceLevel, observed->niceLevel);
        if (kernelSupportsUtilizationClamp())
            EXPECT_EQ(expectation.minUtil, observed->minUtil);
    }
}

// A UserInteractive thread is registered with HighPriorityThreads, which drops its boost while no
// page is visible. Re-enabling has to put the utilization clamp back, which it can only do by
// applying the nice level and the clamp separately: rtkit owns the nice level, and asking for both
// at once fails as a unit when we lack the privilege for the nice level.
TEST(WTF_Thread, HighPriorityThreadsRestoresClampWhenReenabled)
{
    if (!kernelSupportsUtilizationClamp())
        return;

    BinarySemaphore running;
    BinarySemaphore mayExit;
    Ref thread = Thread::create("ClampProbe"_s, [&] {
        running.signal();
        mayExit.wait();
    }, ThreadType::Unknown, Thread::QOS::UserInteractive);
    running.wait();

    auto& highPriorityThreads = HighPriorityThreads::singleton();
    highPriorityThreads.setEnabled(false);
    auto demoted = observeSchedulingAttributes(thread->id());
    highPriorityThreads.setEnabled(true);
    auto restored = observeSchedulingAttributes(thread->id());

    mayExit.signal();
    thread->waitForCompletion();

    ASSERT_TRUE(demoted.has_value());
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(1024u * 20 / 100, demoted->minUtil);
    EXPECT_EQ(1024u, restored->minUtil);
}
#endif // OS(LINUX)

} // namespace TestWebKitAPI
