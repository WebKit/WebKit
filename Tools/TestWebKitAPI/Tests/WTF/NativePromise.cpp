/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Portions are Copyright (C) 2017-2021 Mozilla Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/NativePromise.h>

#include "Test.h"
#include "Utilities.h"
#include <wtf/Atomics.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/Ref.h>
#include <wtf/RefCountedFixedVector.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>
#include <wtf/threads/BinarySemaphore.h>

using namespace WTF;

namespace TestWebKitAPI {

using TestPromise = NativePromise<int, double, PromiseOption::Default | PromiseOption::NonExclusive>;
using TestPromiseExcl = NativePromise<int, double>;

class WorkQueueWithShutdown : public WorkQueue {
public:
    static Ref<WorkQueueWithShutdown> create(ASCIILiteral name) { return adoptRef(*new WorkQueueWithShutdown(name)); }
    void beginShutdown()
    {
        dispatch([this, strong = Ref { *this }] {
            m_shutdown = true;
            m_semaphore.signal();
        });
    }
    void waitUntilShutdown()
    {
        while (!m_shutdown)
            m_semaphore.wait();
    }

private:
    WorkQueueWithShutdown(ASCIILiteral name)
        : WorkQueue(name, QOS::Default)
    {
    }
    std::atomic<bool> m_shutdown { false };
    BinarySemaphore m_semaphore;
};

class AutoWorkQueue {
public:
    AutoWorkQueue()
        : m_workQueue(WorkQueueWithShutdown::create("com.apple.WebKit.Test.simple"_s))
    {
    }

    Ref<WorkQueueWithShutdown> queue() { return m_workQueue; }

    ~AutoWorkQueue()
    {
        m_workQueue->waitUntilShutdown();
    }

private:
    Ref<WorkQueueWithShutdown> m_workQueue;
};

struct RefCountedProducer final : public ThreadSafeRefCounted<RefCountedProducer> {
    Ref<TestPromise> promise() { return producer; }
    typename TestPromise::Producer producer;
};

class DelayedSettle final : public ThreadSafeRefCounted<DelayedSettle> {
public:
    DelayedSettle(WorkQueue& workQueue, RefPtr<RefCountedProducer> producer, TestPromise::Result&& result, int iterations)
        : m_producer(producer)
        , m_iterations(iterations)
        , m_workQueue(workQueue)
        , m_result(WTFMove(result))
    {
    }

    void dispatch()
    {
        m_workQueue->dispatch([protectedThis = RefPtr { this }] {
            protectedThis->run();
        });
    }

    void run()
    {
        assertIsCurrent(m_workQueue);

        Locker lock { m_lock };
        if (!m_producer) {
            // Canceled.
            return;
        }

        if (!--m_iterations) {
            m_producer->producer.settle(m_result);
            return;
        }

        dispatch();
    }

    void cancel()
    {
        Locker lock { m_lock };
        m_producer = nullptr;
    }

private:
    Lock m_lock;
    RefPtr<RefCountedProducer> m_producer WTF_GUARDED_BY_LOCK(m_lock);
    int m_iterations WTF_GUARDED_BY_LOCK(m_lock);
    Ref<WorkQueue> m_workQueue;
    const TestPromise::Result m_result;
};

static auto doFail()
{
    return [] {
        EXPECT_TRUE(false);
    };
}

static auto doFailAndReject(Logger::LogSiteIdentifier location = Logger::LogSiteIdentifier(__builtin_FUNCTION(), nullptr))
{
    return [location = WTFMove(location)] {
        EXPECT_TRUE(false);
        return TestPromise::createAndReject(0.0, location);
    };
}

static void runInCurrentRunLoop(Function<void(RunLoop&)>&& function)
{
    WTF::initializeMainThread();
    auto& runLoop = RunLoop::current();

    function(runLoop);

    bool done = false;
    runLoop.dispatch([&] {
        done = true;
    });
    while (!done)
        runLoop.cycle();
}

static void runInCurrentRunLoopUntilDone(Function<void(RunLoop&, bool&)>&& function)
{
    WTF::initializeMainThread();
    auto& runLoop = RunLoop::current();

    bool done = false;

    function(runLoop, done);

    while (!done)
        runLoop.cycle();
}

// Basis usage, create a resolved promise, then on a different workqueue.
TEST(NativePromise, BasicResolve)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        TestPromise::createAndResolve(42)->then(queue,
            [queue](int resolveValue) {
                EXPECT_EQ(resolveValue, 42);
                queue->beginShutdown();
            }, doFail());
    });
}

// Basis usage, create a rejected promise, then on a different workqueue.
TEST(NativePromise, BasicReject)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        TestPromise::createAndReject(42.0)->then(queue,
            doFail(),
            [queue](int rejectValue) {
                EXPECT_EQ(rejectValue, 42.0);
                queue->beginShutdown();
            });
    });
}

// Basis usage, create a resolved promise, whenSettled on a different workqueue.
TEST(NativePromise, BasicSettleResolved)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        TestPromise::createAndResolve(42)->whenSettled(queue,
            [queue](const TestPromise::Result& result) {
                EXPECT_TRUE(result);
                EXPECT_EQ(result.value(), 42);
                queue->beginShutdown();
            });
    });
}

// Basis usage, create a rejected promise, whenSettled on a different workqueue.
TEST(NativePromise, BasicSettleRejected)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        TestPromise::createAndReject(42.0)->whenSettled(queue,
            [queue](const TestPromise::Result& result) {
                EXPECT_TRUE(!result);
                EXPECT_EQ(result.error(), 42.0);
                queue->beginShutdown();
            });
    });
}

// Example use with a GenericPromise which is a NativePromise<void, void>
//
TEST(NativePromise, GenericPromise)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        GenericPromise::createAndResolve()->then(queue,
            []() {
                EXPECT_TRUE(true);
            },
            doFail());

        GenericPromise::createAndResolve()->whenSettled(queue,
            [](GenericPromise::Result result) {
                EXPECT_TRUE(result);
            });

        GenericPromise::createAndReject()->whenSettled(queue,
            [](GenericPromise::Result result) {
                EXPECT_TRUE(!result);
            });

        GenericPromise::Producer producer1;
        // A producer is directly convertible into the promise it can settle.
        Ref<GenericPromise> promise1 = producer1;
        producer1.resolve();
        promise1->then(queue,
            []() {
                EXPECT_TRUE(true);
            },
            doFail());

        GenericPromise::Producer producer2;
        Ref<GenericPromise> promise2 = producer2;
        producer2.reject();
        promise2->then(queue,
            doFail(),
            []() {
                EXPECT_TRUE(true);
            });

        GenericPromise::Producer producer3;
        Ref<GenericPromise> promise3 = producer3;
        NativePromiseRequest request3;

        // Note that if you're not interested in the result you can provide two Function<void()> to then()
        promise3->then(queue, doFail(), doFail()).track(request3);
        producer3.resolve();

        // We are no longer interested by the result of the promise. We disconnect the request holder.
        // doFail() above will never be called.
        request3.disconnect();

        // Note that if you're not interested in the result you can also provide one Function<void()> with whenSettled()
        GenericPromise::Producer producer4;
        Ref<GenericPromise> promise4 = producer4;

        promise4->whenSettled(queue, []() {
        });
        producer4.resolve();

        // You can mix & match promise types and chain them together.
        // Producer also accepts syntax using operator-> for consistency with a consumer's promise.
        GenericPromise::Producer producer5;
        using MyPromise = NativePromise<int, int>;
        producer5->whenSettled(queue,
            [](GenericPromise::Result result) {
                EXPECT_TRUE(result.has_value());
                return MyPromise::createAndResolve(1);
            })->whenSettled(queue,
                [queue](MyPromise::Result result) {
                    static_assert(std::is_same_v<MyPromise::Result::value_type, int>, "The type received is the same as the last promise returned");
                    EXPECT_TRUE(result.has_value());
                    EXPECT_EQ(result.value(), 1);
                    queue->beginShutdown();
                });
        producer5->resolve();
    });
}

TEST(NativePromise, CallbacksWithAuto)
{
    using MyPromiseNonExclusive = NativePromise<bool, bool, PromiseOption::Default | PromiseOption::NonExclusive>;
    runInCurrentRunLoop([&](auto& runLoop) {
        MyPromiseNonExclusive::Producer producer;
        Ref<MyPromiseNonExclusive> promise = producer;
        promise->then(runLoop, [] (auto val) {
            EXPECT_TRUE(val);
        }, [] (auto val) {
            EXPECT_TRUE(val);
        });
        promise->then(runLoop, [] (bool val) {
            EXPECT_TRUE(val);
        }, [] (auto val) {
            EXPECT_TRUE(val);
        });
        promise->then(runLoop, [] (auto val) {
            EXPECT_TRUE(val);
        }, [] (bool val) {
            EXPECT_TRUE(val);
        });
        promise->then(runLoop, [] {
            EXPECT_TRUE(true);
        }, [] (auto val) {
            EXPECT_TRUE(val);
        });
        promise->then(runLoop, [] (auto val) {
            EXPECT_TRUE(val);
        }, [] {
            EXPECT_TRUE(true);
        });
        promise->whenSettled(runLoop, [] (auto val) {
            EXPECT_TRUE(val);
            EXPECT_TRUE(val.value());
        });
        promise->whenSettled(runLoop, [] (const auto val) {
            EXPECT_TRUE(val);
            EXPECT_TRUE(val.value());
        });
        promise->whenSettled(runLoop, [] (const auto& val) {
            EXPECT_TRUE(val);
            EXPECT_TRUE(val.value());
        });
        promise->whenSettled(runLoop, [] (const MyPromiseNonExclusive::Result& val) {
            EXPECT_TRUE(val);
            EXPECT_TRUE(val.value());
        });
        promise->whenSettled(runLoop, [] (MyPromiseNonExclusive::Result val) {
            EXPECT_TRUE(val);
            EXPECT_TRUE(val.value());
        });
        promise->whenSettled(runLoop, [] {
        });
        producer.resolve(true);
    });

    using MyPromise = NativePromise<bool, bool>;
    runInCurrentRunLoop([&](auto& runLoop) {
        MyPromise::createAndResolve(true)->whenSettled(runLoop, [] (auto&& val) {
            EXPECT_TRUE(val);
            EXPECT_TRUE(val.value());
        });
        MyPromise::createAndResolve(true)->whenSettled(runLoop, [] (auto val) {
            EXPECT_TRUE(val);
            EXPECT_TRUE(val.value());
        });
        MyPromise::createAndResolve(true)->whenSettled(runLoop, [] (const auto val) {
            EXPECT_TRUE(val);
            EXPECT_TRUE(val.value());
        });
        MyPromise::createAndResolve(true)->whenSettled(runLoop, [] (const auto& val) {
            EXPECT_TRUE(val);
            EXPECT_TRUE(val.value());
        });
        MyPromise::createAndResolve(true)->whenSettled(runLoop, [] (MyPromise::Result&& val) {
            EXPECT_TRUE(val);
            EXPECT_TRUE(val.value());
        });
        MyPromise::createAndResolve(true)->whenSettled(runLoop, [] () {
            EXPECT_TRUE(true);
        });
    });
}

TEST(NativePromise, PromiseRequest)
{
    // We declare the Request holder before using the runLoop to ensure it stays in scope for the entire run.
    // ASSERTION FAILED: !m_request
    using MyPromise = NativePromise<bool, bool>;
    NativePromiseRequest request1;

    runInCurrentRunLoop([&](auto& runLoop) {
        MyPromise::Producer producer1;
        Ref<MyPromise> promise1 = producer1;
        producer1.resolve(true);

        promise1->whenSettled(runLoop,
            [&](MyPromise::Result&& result) {
                EXPECT_TRUE(result.has_value());
                EXPECT_TRUE(result.value());
                EXPECT_TRUE(!!request1);
                request1.complete();
                EXPECT_FALSE(!!request1);
            }).track(request1);
    });

    // PromiseRequest allows to use capture by reference or pointer to ref-counted object and ensure the
    // lifetime of the object.
    bool objectToShare = true;
    runInCurrentRunLoop([&](auto& runLoop) {
        NativePromiseRequest request2;
        GenericPromise::Producer producer2;
        Ref<GenericPromise> promise2 = producer2;
        promise2->whenSettled(runLoop,
            [&objectToShare](GenericPromise::Result&&) mutable {
                // It would be normally unsafe to access `objectToShare` as it went out of scope.
                // but this function will never run as we've disconnected the ThenCommand.
                objectToShare = false;
            }).track(request2);
        EXPECT_TRUE(!!request2);
        request2.disconnect();
        EXPECT_FALSE(!!request2);
        producer2.resolve();
        EXPECT_TRUE(objectToShare);
    });
    EXPECT_TRUE(objectToShare);
}

// Ensure that callbacks aren't run when request holder is disconnected after the promise was resolved and then() called.
TEST(NativePromise, PromiseRequestDisconnected1)
{
    runInCurrentRunLoop([](auto& runLoop) {
        NativePromiseRequest request;

        TestPromise::Producer producer;
        Ref<TestPromise> promise = producer;
        promise->then(runLoop, doFail(), doFail()).track(request);

        producer.resolve(1);
        request.disconnect();
    });
}

// Ensure that callbacks aren't run when request holder is disconnected even if promise was resolved first.
TEST(NativePromise, PromiseRequestDisconnected2)
{
    runInCurrentRunLoop([](auto& runLoop) {
        NativePromiseRequest request;

        TestPromise::Producer producer;
        Ref<TestPromise> promise = producer;
        producer.resolve(1);

        promise->then(runLoop, doFail(), doFail())->track(request);

        request.disconnect();
    });
}

TEST(NativePromise, AsyncResolve)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        auto producer = adoptRef(new RefCountedProducer());
        auto promise = producer->promise();

        // Kick off three racing tasks, and make sure we get the one that finishes
        // earliest.
        auto delayedSettleTask1 = adoptRef(new DelayedSettle(queue, producer, TestPromise::Result(32), 10));
        auto delayedSettleTask2 = adoptRef(new DelayedSettle(queue, producer, TestPromise::Result(42), 5));
        auto delayedSettleTask3 = adoptRef(new DelayedSettle(queue, producer, TestPromise::Error(32.0), 7));

        delayedSettleTask1->dispatch();
        delayedSettleTask2->dispatch();
        delayedSettleTask3->dispatch();

        promise->then(queue,
            [queue, delayedSettleTask1, delayedSettleTask2, delayedSettleTask3](int resolveValue) {
                EXPECT_EQ(resolveValue, 42);
                delayedSettleTask1->cancel();
                delayedSettleTask2->cancel();
                delayedSettleTask3->cancel();
                queue->beginShutdown();
            },
            doFail());
    });
}

TEST(NativePromise, CompletionPromises)
{
    bool invokedPass { false }; // Only ever accessed on the WorkQueue
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue, &invokedPass] {
        TestPromise::createAndResolve(40)->then(queue,
            [](int val) {
                return TestPromise::createAndResolve(val + 10);
            },
            doFailAndReject())->then(queue,
                [&invokedPass](int val) {
                    invokedPass = true;
                    return TestPromise::createAndResolve(val);
                },
                doFailAndReject())->then(queue,
                    [queue](int val) {
                        auto producer = adoptRef(new RefCountedProducer());
                        auto p = producer->promise();

                        auto resolver = adoptRef(new DelayedSettle(queue, producer, TestPromise::Result(val - 8), 10));
                        resolver->dispatch();
                        return p;
                    },
                    doFailAndReject())->then(queue,
                        [](int val) {
                            return TestPromise::createAndReject(double(val - 42) + 42.0);
                        },
                        doFailAndReject())->then(queue,
                            doFail(),
                            [queue, &invokedPass](double val) {
                                EXPECT_EQ(val, 42.0);
                                EXPECT_TRUE((bool)invokedPass);
                                queue->beginShutdown();
                            });
    });
}

TEST(NativePromise, UsingMethods)
{
    class MyClass : public ThreadSafeRefCounted<MyClass> {
    public:
        static Ref<MyClass> create() { return adoptRef(*new MyClass()); }
        void resolveWithNothing()
        {
            EXPECT_TRUE(true);
        }
        void rejectWithNothing()
        {
            EXPECT_TRUE(false);
        }
        void resolveWithValue(int value)
        {
            EXPECT_EQ(value, 1);
        }
        void rejectWithValue(double value)
        {
            EXPECT_EQ(value, 2.0);
        }
        void settleWithNothing()
        {
            EXPECT_TRUE(true);
        }
        void settleWithResult(const TestPromise::Result& result)
        {
            EXPECT_TRUE(result.has_value());
            EXPECT_EQ(result.value(), 1);
        }
    };
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue, myClass = MyClass::create()] {
        TestPromise::createAndResolve(1)->then(queue, myClass.get(), &MyClass::resolveWithNothing, &MyClass::rejectWithNothing);
        TestPromise::createAndReject(2.0)->then(queue, myClass.get(), &MyClass::resolveWithValue, &MyClass::rejectWithValue);
        TestPromise::createAndResolve(3)->whenSettled(queue, myClass.get(), &MyClass::settleWithNothing);
        TestPromise::createAndResolve(1)->whenSettled(queue, myClass.get(), &MyClass::settleWithResult);
        queue->dispatch([queue] {
            queue->beginShutdown();
        });
    });
}

static Ref<GenericPromise> myMethod()
{
    assertIsCurrent(RunLoop::main());
    // You would normally do some work here.
    return GenericPromise::createAndResolve();
}

TEST(NativePromise, InvokeAsync)
{
    bool done = false;
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue, &done] {
        invokeAsync(RunLoop::main(), &myMethod)->whenSettled(queue,
            [queue, &done](GenericPromise::Result result) {
                EXPECT_TRUE(result.has_value());
                queue->beginShutdown();
                done = true;
            });
    });

    Util::run(&done);
}

// A chained promise (that is promise->whenSettled([] { return GenericPromise }) is itself a promise
static Ref<GenericPromise> myMethodReturningThenCommandWithPromise()
{
    assertIsCurrent(RunLoop::main());
    // You would normally do some work here.
    return GenericPromise::createAndResolve()->whenSettled(RunLoop::main(),
        [](GenericPromise::Result result) {
            return GenericPromise::createAndSettle(WTFMove(result));
        });
}

static Ref<GenericPromise> myMethodReturningThenCommandWithVoid()
{
    assertIsCurrent(RunLoop::main());
    // You would normally do some work here.
    return GenericPromise::createAndReject()->whenSettled(RunLoop::main(),
        [](GenericPromise::Result result) {
            EXPECT_FALSE(result.has_value());
        });
}

TEST(NativePromise, InvokeAsyncAutoConversion)
{
    // Ensure that there's no need to cast NativePromise::then() result
    bool done = false;
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        invokeAsync(RunLoop::main(), &myMethodReturningThenCommandWithPromise)->whenSettled(queue,
            [](GenericPromise::Result result) {
                EXPECT_TRUE(result.has_value());
            });
    });
    queue->dispatch([queue, &done] {
        invokeAsync(RunLoop::main(), &myMethodReturningThenCommandWithVoid)->whenSettled(queue,
            [queue, &done](GenericPromise::Result result) {
                EXPECT_TRUE(result.has_value());
                queue->beginShutdown();
                done = true;
            });
    });

    Util::run(&done);
}

TEST(NativePromise, InvokeAsyncWithExpected)
{
    runInCurrentRunLoopUntilDone([](auto& runLoop, bool& done) {
        auto asyncMethodWithExpected = [] {
            return Expected<int, long> { 1 };
        };

        invokeAsync(runLoop, WTFMove(asyncMethodWithExpected))->whenSettled(runLoop, [&](auto&& result) {
            EXPECT_TRUE(!!result);
            EXPECT_EQ(result.value(), 1L);
            done = true;
        });
    });
}

TEST(NativePromise, InvokeAsyncWithVoid)
{
    runInCurrentRunLoopUntilDone([](auto& runLoop, bool& done) {
        invokeAsync(runLoop, [] {
            EXPECT_TRUE(true);
        })->whenSettled(runLoop, [&](auto&& result) {
            EXPECT_TRUE(!!result);
            static_assert(std::is_same_v<std::remove_reference_t<decltype(result)>, GenericPromise::Result>, "We must be getting a GenericPromise");
            static_assert(std::is_void_v<typename std::remove_reference_t<decltype(result)>::value_type>);
            done = true;
        });
    });
}

static Ref<GenericPromise> myMethodReturningProducer()
{
    assertIsCurrent(RunLoop::main());
    // You would normally do some work here.
    return GenericPromise::createAndResolve()->whenSettled(RunLoop::main(),
        [](GenericPromise::Result result) {
            GenericPromise::Producer producer;
            producer.settle(WTFMove(result));
            return producer;
        });
}

TEST(NativePromise, InvokeAsyncAutoConversionWithProducer)
{
    // Ensure that there's no need to cast NativePromise::whenSettled() result
    bool done = false;
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue, &done] {
        invokeAsync(RunLoop::main(), &myMethodReturningProducer)->whenSettled(queue,
            [queue, &done](GenericPromise::Result result) {
                EXPECT_TRUE(result.has_value());
                queue->beginShutdown();
                done = true;
            });
    });

    Util::run(&done);
}

TEST(NativePromise, PromiseAllResolve)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        Vector<Ref<TestPromise>> promises;
        promises.append(TestPromise::createAndResolve(22));
        promises.append(TestPromise::createAndResolve(32));
        promises.append(TestPromise::createAndResolve(42));

        TestPromise::all(promises)->then(queue,
            [queue](const Vector<int>& resolveValues) {
                EXPECT_EQ(resolveValues.size(), 3UL);
                EXPECT_EQ(resolveValues[0], 22);
                EXPECT_EQ(resolveValues[1], 32);
                EXPECT_EQ(resolveValues[2], 42);
                queue->beginShutdown();
            },
            doFail());
    });
}

TEST(NativePromise, PromiseVoidAllResolve)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        Vector<Ref<GenericPromise>> promises;
        promises.append(GenericPromise::createAndResolve());
        promises.append(GenericPromise::createAndResolve());
        promises.append(GenericPromise::createAndResolve());

        GenericPromise::all(promises)->then(queue,
            [] () {
                EXPECT_TRUE(true);
            },
            doFail());

        GenericPromise::all(Vector<Ref<GenericPromise>>(10, [](size_t) {
            return GenericPromise::createAndResolve();
        }))->then(queue,
            [queue] () {
                queue->beginShutdown();
            },
            doFail());
    });
}

TEST(NativePromise, PromiseAllResolveAsync)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        Vector<Ref<TestPromise>> promises;
        promises.append(invokeAsync(queue, [] {
            return TestPromise::createAndResolve(22);
        }));
        promises.append(invokeAsync(queue, [] {
            return TestPromise::createAndResolve(32);
        }));
        promises.append(invokeAsync(queue, [] {
            return TestPromise::createAndResolve(42);
        }));

        TestPromise::all(promises)->then(queue,
            [queue](const Vector<int>& resolveValues) {
                EXPECT_EQ(resolveValues.size(), 3UL);
                EXPECT_EQ(resolveValues[0], 22);
                EXPECT_EQ(resolveValues[1], 32);
                EXPECT_EQ(resolveValues[2], 42);
                queue->beginShutdown();
            },
            doFail());
    });
}

TEST(NativePromise, PromiseAllReject)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        Vector<Ref<TestPromise>> promises;
        promises.append(TestPromise::createAndResolve(22));
        promises.append(TestPromise::createAndReject(32.0));
        promises.append(TestPromise::createAndResolve(42));
        // Ensure that more than one rejection doesn't cause a crash
        promises.append(TestPromise::createAndReject(52.0));

        TestPromise::all(promises)->then(queue,
            doFail(),
            [queue](float rejectValue) {
                EXPECT_EQ(rejectValue, 32.0);
                queue->beginShutdown();
            });
    });
}

TEST(NativePromise, PromiseAllRejectAsync)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        Vector<Ref<TestPromise>> promises;
        promises.append(invokeAsync(queue, [] {
            return TestPromise::createAndResolve(22);
        }));
        promises.append(invokeAsync(queue, [] {
            return TestPromise::createAndReject(32.0);
        }));
        promises.append(invokeAsync(queue, [] {
            return TestPromise::createAndResolve(42);
        }));
        // Ensure that more than one rejection doesn't cause a crash
        promises.append(invokeAsync(queue, [] {
            return TestPromise::createAndReject(52.0);
        }));

        TestPromise::all(promises)->then(queue,
            doFail(),
            [queue](float rejectValue) {
                EXPECT_EQ(rejectValue, 32.0);
                queue->beginShutdown();
            });
    });
}

TEST(NativePromise, PromiseAllSettled)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        Vector<Ref<TestPromise>> promises;
        promises.append(TestPromise::createAndResolve(22));
        promises.append(TestPromise::createAndReject(32.0));
        promises.append(TestPromise::createAndResolve(42));
        promises.append(TestPromise::createAndReject(52.0));

        TestPromise::allSettled(promises)->then(
            queue,
            [queue](const TestPromise::AllSettledPromiseType::ResolveValueType& resolveValues) {
                EXPECT_EQ(resolveValues.size(), 4UL);
                EXPECT_TRUE(resolveValues[0]);
                EXPECT_EQ(resolveValues[0].value(), 22);
                EXPECT_FALSE(resolveValues[1]);
                EXPECT_EQ(resolveValues[1].error(), 32.0);
                EXPECT_TRUE(resolveValues[2]);
                EXPECT_EQ(resolveValues[2].value(), 42);
                EXPECT_FALSE(resolveValues[3]);
                EXPECT_EQ(resolveValues[3].error(), 52.0);
                queue->beginShutdown();
            },
            doFail());
    });
}

TEST(NativePromise, PromiseAllSettledAsync)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();

    queue->dispatch([queue] {
        Vector<Ref<TestPromise>> promises;
        promises.append(invokeAsync(queue, [] {
            return TestPromise::createAndResolve(22);
        }));
        promises.append(invokeAsync(queue, [] {
            return TestPromise::createAndReject(32.0);
        }));
        promises.append(invokeAsync(queue, [] {
            return TestPromise::createAndResolve(42);
        }));
        promises.append(invokeAsync(queue, [] {
            return TestPromise::createAndReject(52.0);
        }));

        TestPromise::allSettled(promises)->then(queue,
            [queue](const TestPromise::AllSettledPromiseType::ResolveValueType& resolveValues) {
                EXPECT_EQ(resolveValues.size(), 4UL);
                EXPECT_TRUE(resolveValues[0].has_value());
                EXPECT_EQ(resolveValues[0].value(), 22);
                EXPECT_FALSE(resolveValues[1].has_value());
                EXPECT_EQ(resolveValues[1].error(), 32.0);
                EXPECT_TRUE(resolveValues[2].has_value());
                EXPECT_EQ(resolveValues[2], 42);
                EXPECT_FALSE(resolveValues[3].has_value());
                EXPECT_EQ(resolveValues[3].error(), 52.0);
                queue->beginShutdown();
            },
            doFail());
    });
}

// Chain 100 promises, and disconnect the chain after the 50th resolve.
TEST(NativePromise, Chaining)
{
    // We declare this variable before |awq| to ensure the destructor is run after |holder.disconnect()|.
    NativePromiseRequest holder;

    AutoWorkQueue awq;
    auto queue = awq.queue();

    queue->dispatch([queue, &holder] {
        auto promise = TestPromise::createAndResolve(42);
        const size_t kIterations = 100;
        for (size_t i = 0; i < kIterations; ++i) {
            promise = promise->then(queue,
                [](int val) {
                    EXPECT_EQ(val, 42);
                    return TestPromise::createAndResolve(val);
                },
                [](double val) {
                    return TestPromise::createAndReject(val);
                });

            if (i == kIterations / 2) {
                promise->then(queue,
                    [queue, &holder] {
                        holder.disconnect();
                        queue->beginShutdown();
                    },
                    doFail());
            }
        }
        // We will hit the assertion if we don't disconnect the leaf Request
        // in the promise chain.
        promise->whenSettled(queue, [] { })->track(holder);
    });
}

TEST(NativePromise, MoveOnlyType)
{
    using MyPromise = NativePromise<std::unique_ptr<int>, std::unique_ptr<int>>;

    AutoWorkQueue awq;
    auto queue = awq.queue();

    MyPromise::createAndResolve(makeUniqueWithoutFastMallocCheck<int>(87))->then(queue,
        [](std::unique_ptr<int> val) {
            EXPECT_EQ(87, *val);
        },
        [] {
            EXPECT_TRUE(false);
        });

    MyPromise::createAndResolve(makeUniqueWithoutFastMallocCheck<int>(87))->whenSettled(queue,
        [queue](MyPromise::Result&& val) {
            EXPECT_TRUE(val.has_value());
            EXPECT_EQ(87, *(val.value()));
        });

    MyPromise::createAndReject(makeUniqueWithoutFastMallocCheck<int>(87))->whenSettled(queue,
        [queue](MyPromise::Result&& val) {
            EXPECT_FALSE(val.has_value());
            EXPECT_EQ(87, *(val.error()));
            queue->beginShutdown();
        });
}

// A WTFString can be directly returned by a producer. More generically, so long as an object implements an `isolatedCopy()` method, it will be automatically called.
TEST(NativePromise, WTFString)
{
    using MyPromise = NativePromise<String, String>;

    AutoWorkQueue awq2;
    auto queue2 = awq2.queue();

    AutoWorkQueue awq;
    auto queue = awq.queue();

    MyPromise::createAndResolve("hello"_s)->then(queue,
        [](String&& val) {
            EXPECT_EQ(String("hello"_s), val);
        },
        [] {
            EXPECT_TRUE(false);
        });

    MyPromise::createAndResolve("hello"_s)->whenSettled(queue,
        [queue](MyPromise::Result&& val) {
            EXPECT_TRUE(val.has_value());
            EXPECT_EQ(String("hello"_s), val.value());
            EXPECT_TRUE(val.value().isSafeToSendToAnotherThread());
        });

    MyPromise::createAndReject("error"_s)->whenSettled(queue,
        [queue](MyPromise::Result&& val) {
            EXPECT_FALSE(val.has_value());
            EXPECT_EQ(String("error"_s), val.error());
            EXPECT_TRUE(val.error().isSafeToSendToAnotherThread());
        });

    MyPromise::createAndResolve(String("hello"_s))->then(queue,
        [](String&& val) {
            EXPECT_EQ(String("hello"_s), val);
            EXPECT_TRUE(val.isSafeToSendToAnotherThread());
        },
        [] {
            EXPECT_TRUE(false);
        });

    // Check that we can receive the value by const reference too.
    MyPromise::createAndResolve(String("hello"_s))->then(queue,
        [](const String& val) {
            EXPECT_EQ(String("hello"_s), val);
            EXPECT_TRUE(val.isSafeToSendToAnotherThread());
        },
        [] {
            EXPECT_TRUE(false);
        });

    // Can pass object implecitly convertible to ResolveValueType
    MyPromise::createAndResolve(AtomString("hello"_s))->then(queue,
        [](String&& val) {
            EXPECT_EQ(String("hello"_s), val);
            EXPECT_TRUE(val.isSafeToSendToAnotherThread());
        },
        [] {
            EXPECT_TRUE(false);
        });

    MyPromise::createAndResolve(AtomString("hello"_s))->then(queue,
        [](const String& val) {
            EXPECT_EQ(String("hello"_s), val);
            EXPECT_TRUE(val.isSafeToSendToAnotherThread());
        },
        [] {
            EXPECT_TRUE(false);
        });

    MyPromise::createAndResolve(AtomString("hello"_s))->then(queue,
        [](String val) {
            EXPECT_EQ(String("hello"_s), val);
            EXPECT_TRUE(val.isSafeToSendToAnotherThread());
        },
        [] {
            EXPECT_TRUE(false);
        });

    MyPromise::createAndResolve(String("hello"_s))->whenSettled(queue,
        [](MyPromise::Result&& val) {
            EXPECT_TRUE(val.has_value());
            EXPECT_TRUE(val.value().isSafeToSendToAnotherThread());
            EXPECT_EQ(String("hello"_s), val.value());
        });

    MyPromise::createAndResolve(String("hello"_s))->whenSettled(queue,
        [](MyPromise::Result val) {
            EXPECT_TRUE(val.has_value());
            EXPECT_TRUE(val.value().isSafeToSendToAnotherThread());
            EXPECT_EQ(String("hello"_s), val.value());
        });

    MyPromise::createAndResolve(String("hello"_s))->whenSettled(queue,
        [](const MyPromise::Result& val) {
            EXPECT_TRUE(val.has_value());
            EXPECT_TRUE(val.value().isSafeToSendToAnotherThread());
            EXPECT_EQ(String("hello"_s), val.value());
        });

    MyPromise::createAndReject(String("error"_s))->whenSettled(queue,
        [](MyPromise::Result&& val) {
            EXPECT_FALSE(val.has_value());
            EXPECT_TRUE(val.error().isSafeToSendToAnotherThread());
            EXPECT_EQ(String("error"_s), val.error());
        });

    MyPromise::createAndResolve(AtomString("hello"_s))->whenSettled(queue,
        [](MyPromise::Result&& val) {
            EXPECT_TRUE(val.has_value());
            EXPECT_TRUE(val.value().isSafeToSendToAnotherThread());
            EXPECT_EQ(String("hello"_s), val.value());
            return MyPromise::createAndSettle(WTFMove(val));
        })->whenSettled(queue2,
            [](MyPromise::Result val) {
                EXPECT_TRUE(val.has_value());
                EXPECT_TRUE(val.value().isSafeToSendToAnotherThread());
                EXPECT_EQ(String("hello"_s), val.value());
            });

    MyPromise::createAndResolve(AtomString("hello"_s))->whenSettled(queue,
        [queue](MyPromise::Result val) {
            EXPECT_TRUE(val.has_value());
            EXPECT_TRUE(val.value().isSafeToSendToAnotherThread());
            EXPECT_EQ(String("hello"_s), val.value());
            queue->beginShutdown();
            // Don't move the result to make sure we get a new isolatedCopy.
            return MyPromise::createAndSettle(val);
        })->whenSettled(queue2,
            [queue2](MyPromise::Result val) {
                EXPECT_TRUE(val.has_value());
                EXPECT_TRUE(val.value().isSafeToSendToAnotherThread());
                EXPECT_EQ(String("hello"_s), val.value());
                queue2->beginShutdown();
            });
}

TEST(NativePromise, WTFStringWithDelayedResolve)
{
    using MyPromise = NativePromise<String, String>;

    // The following steps runs strictly serially.
    // 1. We create a promise on the main thread.
    // 2. Dispatch a task that will `whenSettled()` on that promise on WorkQueue2.
    // 3. We resolve on the main thread the promise with an AtomString.
    // 4. Resolver will be called on WorkQueue1 and check that the string content is correct and the string created on the main thread was safely moved.

    AutoWorkQueue awq1;
    awq1.queue();
    auto queue1 = awq1.queue();
    MyPromise::Producer producer;
    bool hasRun = false; // AutoWorkQueue guarantees that there can't be concurrent accesses to hasRun.
    {
        AutoWorkQueue awq2;
        auto queue2 = awq2.queue();
        queue2->dispatch([queue1, queue2, promise = Ref<MyPromise> { producer }, &hasRun] {
            promise->whenSettled(queue1,
                [queue1](MyPromise::Result val) {
                    EXPECT_TRUE(val.has_value());
                    EXPECT_TRUE(val.value().isSafeToSendToAnotherThread());
                    EXPECT_EQ(String("hello"_s), val.value());
                    queue1->beginShutdown();
                });
            hasRun = true;
            queue2->beginShutdown();
        });
    }
    EXPECT_TRUE(hasRun);
    producer.resolve(AtomString("hello"_s));
}

TEST(NativePromise, NonExclusiveWithCrossThreadCopy)
{
    int resolution = 0;
    {
        AutoWorkQueue awq;
        auto queue = awq.queue();
        // If you replace PromiseOption::WithCrossThreadCopy with PromiseOption::WithoutCrossThreadCopy, this test will crash due to the AtomString being deleted on the target queue.
        using MyPromise = NativePromise<Expected<String, AtomString>, bool, PromiseOption::NonExclusive | PromiseOption::WithCrossThreadCopy>;
        static_assert(CrossThreadCopier<Expected<String, AtomString>>::IsNeeded);
        MyPromise::Producer producer;
        Ref<MyPromise> promise = producer;
        promise->whenSettled(queue, [&resolution] (const MyPromise::Result& val) {
            EXPECT_TRUE(val.has_value());
            EXPECT_TRUE(val.value().has_value());
            EXPECT_TRUE(val.value().value().isSafeToSendToAnotherThread());
            EXPECT_EQ(String("that worked"_s), val.value().value());
            resolution++;
        });
        promise->whenSettled(queue, [&resolution] (MyPromise::Result val) {
            EXPECT_TRUE(val.has_value());
            EXPECT_TRUE(val.value().has_value());
            // Being a non-exclusive promise, the value is passed by const reference, so we copied the object in val.
            EXPECT_TRUE(!val.value().value().isSafeToSendToAnotherThread());
            EXPECT_EQ(String("that worked"_s), val.value().value());
            resolution++;
        });
        promise->whenSettled(queue, [queue, &resolution] (const MyPromise::Result& val) {
            EXPECT_TRUE(val.has_value());
            EXPECT_TRUE(val.value().has_value());
            // The previous whenSettled() has run already, and the object was derefed.
            EXPECT_TRUE(val.value().value().isSafeToSendToAnotherThread());
            EXPECT_EQ(String("that worked"_s), val.value().value());
            resolution++;
            queue->beginShutdown();
        });
        producer.resolve(String(AtomString("that worked"_s)));
    }
    EXPECT_EQ(3, resolution);
}

TEST(NativePromise, WithCrossThreadCopyType)
{
    using MyPromiseWithString = NativePromise<String, AtomString>;
    static_assert(MyPromiseWithString::WithAutomaticCrossThreadCopy);
    static_assert(CrossThreadCopier<typename MyPromiseWithString::RejectValueType>::IsNeeded);
    // Check that if making a NativePromise with an AtomString, you actually get a String
    static_assert(std::is_same_v<typename MyPromiseWithString::RejectValueType, String>);

    using MyPromiseWithoutString = NativePromise<int, bool>;
    static_assert(!MyPromiseWithoutString::WithAutomaticCrossThreadCopy);

    using MyPromiseWithArrayOfString = NativePromise<Vector<String>, bool>;
    static_assert(MyPromiseWithArrayOfString::WithAutomaticCrossThreadCopy);

    using MyNonExclusivePromise = NativePromise<Vector<int>, bool, PromiseOption::Default | PromiseOption::NonExclusive>;
    // No need for crossThreadProxy for a Vector not containing a type with isolatedCopy() method.
    static_assert(!MyNonExclusivePromise::WithAutomaticCrossThreadCopy);
}

TEST(NativePromise, ExpectedWithString)
{
    using MyPromise = NativePromise<Expected<String, String>, int>;

    AutoWorkQueue awq;
    auto queue = awq.queue();

    MyPromise::createAndResolve(String("hello"_s))->then(queue,
        [](MyPromise::ResolveValueType&& val) {
            EXPECT_TRUE(val.has_value());
            EXPECT_EQ(String("hello"_s), val.value());
            EXPECT_TRUE(val.value().isSafeToSendToAnotherThread());
        },
        [] {
            EXPECT_TRUE(false);
        });

    MyPromise::createAndResolve(String("hello"_s))->whenSettled(queue,
        [queue](MyPromise::Result&& val) {
            EXPECT_TRUE(val.has_value());
            EXPECT_EQ(String("hello"_s), val.value());
            EXPECT_TRUE(val.value().value().isSafeToSendToAnotherThread());
        });

    Expected<String, String> error = Unexpected<String>("error"_s);
    MyPromise::createAndResolve(WTFMove(error))->whenSettled(queue,
        [queue](MyPromise::Result&& val) {
            EXPECT_TRUE(val.has_value());
            EXPECT_FALSE(val.value().has_value());
            EXPECT_EQ(String("error"_s), val.value().error());
            EXPECT_TRUE(val.value().error().isSafeToSendToAnotherThread());
            queue->beginShutdown();
        });
}

TEST(NativePromise, HeterogeneousChaining)
{
    using Promise1 = NativePromise<std::unique_ptr<char>, bool>;
    using Promise2 = NativePromise<std::unique_ptr<int>, bool>;

    NativePromiseRequest holder;

    AutoWorkQueue awq;
    auto queue = awq.queue();

    queue->dispatch([queue, &holder] {
        Promise1::createAndResolve(makeUniqueWithoutFastMallocCheck<char>(0))->whenSettled(queue,
            [&holder] {
                holder.disconnect();
                return Promise2::createAndResolve(makeUniqueWithoutFastMallocCheck<int>(0));
            })->whenSettled(queue,
                [] {
                    // Shouldn't be called for we've disconnected the request.
                    EXPECT_FALSE(true);
                })->track(holder);
    });

    Promise1::createAndResolve(makeUniqueWithoutFastMallocCheck<char>(87))->then(queue,
        [](std::unique_ptr<char> val) {
            EXPECT_EQ(87, *val);
            return Promise2::createAndResolve(makeUniqueWithoutFastMallocCheck<int>(94));
        },
        [] {
            return Promise2::createAndResolve(makeUniqueWithoutFastMallocCheck<int>(95));
        })->then(queue,
            [](std::unique_ptr<int> val) {
                EXPECT_EQ(94, *val);
            },
            doFail());

    Promise1::createAndResolve(makeUniqueWithoutFastMallocCheck<char>(87))->whenSettled(queue,
        [](Promise1::Result&& result) {
            EXPECT_EQ(87, *(result.value()));
            return Promise2::createAndResolve(makeUniqueWithoutFastMallocCheck<int>(94));
        })->whenSettled(queue,
            [queue](Promise2::Result&& result) {
                EXPECT_EQ(94, *(result.value()));
            });

    // Chaining promises of different types, even if returned from within callback
    TestPromise::createAndResolve(1)->whenSettled(queue,
        [queue] {
            return TestPromiseExcl::createAndResolve(2)->whenSettled(queue, [] {
                return TestPromise::createAndResolve(3);
            });
        })->whenSettled(queue,
            [queue](TestPromise::Result result) {
                EXPECT_TRUE(result.has_value());
                EXPECT_EQ(3, result.value());
            });

    TestPromise::createAndResolve(1)->whenSettled(queue,
        [queue] {
            return TestPromiseExcl::createAndResolve(2)->whenSettled(queue, [] {
                return GenericPromise::createAndResolve();
            });
        })->whenSettled(queue,
            [queue](GenericPromise::Result result) {
                EXPECT_TRUE(result.has_value());
            });

    TestPromise::createAndResolve(1)->whenSettled(queue,
        [queue] {
            return TestPromiseExcl::createAndResolve(2)->whenSettled(queue, [] {
                return GenericPromise::createAndReject();
            });
        })->whenSettled(queue,
            [queue](GenericPromise::Result result) {
                EXPECT_FALSE(result.has_value());
                queue->beginShutdown();
            });
}

TEST(NativePromise, RunLoop)
{
    runInCurrentRunLoop([](auto& runLoop) {
        TestPromise::createAndResolve(42)->then(runLoop,
            [](int resolveValue) {
                EXPECT_EQ(resolveValue, 42);
            },
            doFail());
    });
}

TEST(NativePromise, ImplicitConversionWithForwardPreviousReturn)
{
    runInCurrentRunLoopUntilDone([](auto& runLoop, bool& done) {
        TestPromise::Producer producer;
        Ref<TestPromise> promise = producer;
        promise = promise->whenSettled(runLoop,
            [](const TestPromise::Result& result) {
                return TestPromise::createAndSettle(result);
            });
        promise->whenSettled(runLoop, [promise, &done](const TestPromise::Result& result) {
            EXPECT_TRUE(result.has_value());
            EXPECT_TRUE(promise->isResolved());
            done = true;
        });
        producer.resolve(1);
        Ref<TestPromise> originalPromise = producer;
        EXPECT_TRUE(originalPromise->isResolved());
        // This could be written as EXPECT_TRUE(static_cast<Ref<TestPromise>>(p)->isResolved());
        // but MSVC errors on it.
        EXPECT_FALSE(promise->isResolved()); // The callback hasn't been run yet.
    });
}

TEST(NativePromise, ChainTo)
{
    using VectorPromise = NativePromise<Ref<RefCountedFixedVector<int>>, bool, PromiseOption::Default | PromiseOption::NonExclusive>;
    auto resultVector = RefCountedFixedVector<int>::createFromVector(Vector<int>(5, 42));
    runInCurrentRunLoop([&, producer1 = VectorPromise::Producer(), producer2 = VectorPromise::Producer()](auto& runLoop) mutable {
        auto promise = VectorPromise::createAndResolve(resultVector);
        producer1->then(runLoop,
            [&](const auto& resolveValue) { EXPECT_EQ(resolveValue.get(), RefCountedFixedVector<int>::createFromVector(Vector<int>(5, 42)).get()); },
            doFail());
        producer2->then(runLoop,
            [&](const auto& resolveValue) { EXPECT_EQ(resolveValue.get(), RefCountedFixedVector<int>::createFromVector(Vector<int>(5, 42)).get()); },
            doFail());

        // As promise1 is already resolved, it will automatically resolve/reject producer1 and producer2 with its resolved/reject value.
        promise->chainTo(WTFMove(producer1));
        promise->chainTo(WTFMove(producer2));
    });

    runInCurrentRunLoop([&, producer1 = VectorPromise::Producer(), producer2 = VectorPromise::Producer()](auto& runLoop) mutable {
        producer2->then(runLoop,
            [&](const auto& resolveValue) { EXPECT_EQ(resolveValue.get(), RefCountedFixedVector<int>::createFromVector(Vector<int>(5, 42)).get()); },
            doFail());

        // When producer1 is resolved, it will automatically settle producer2 with the resolved/reject value.
        producer1->chainTo(WTFMove(producer2));
        VectorPromise::createAndResolve(resultVector)->chainTo(WTFMove(producer1));
    });

    runInCurrentRunLoop([&, producer1 = VectorPromise::Producer()](auto& runLoop) mutable {
        auto promise = VectorPromise::createAndResolve(resultVector);
        producer1->then(runLoop,
            [&](auto&& resolveValue) { EXPECT_EQ(resolveValue.get(), RefCountedFixedVector<int>::createFromVector(Vector<int>(5, 42)).get()); },
            doFail());

        // As promise1 is already resolved, it will automatically resolve/reject producer1 with its resolved/reject value.
        promise->chainTo(WTFMove(producer1));
    });
}

TEST(NativePromise, ChainToNonMovable)
{
    using VectorPromise = NativePromise<std::unique_ptr<Vector<int>>, bool, PromiseOption::Default | PromiseOption::NonExclusive>;
    runInCurrentRunLoop([&, producer1 = VectorPromise::Producer(), producer2 = VectorPromise::Producer()](auto& runLoop) mutable {
        auto promise = VectorPromise::createAndResolve(makeUnique<Vector<int>>(5, 42));
        producer1->then(runLoop,
            [&](const auto& resolveValue) { EXPECT_EQ(*resolveValue, Vector<int>(5, 42)); },
            doFail());
        producer2->then(runLoop,
            [&](const auto& resolveValue) { EXPECT_EQ(*resolveValue, Vector<int>(5, 42)); },
            doFail());

        // As promise1 is already resolved, it will automatically resolve/reject producer1 and producer2 with its resolved/reject value.
        promise->chainTo(WTFMove(producer1));
        promise->chainTo(WTFMove(producer2));
    });

    runInCurrentRunLoop([&, producer1 = VectorPromise::Producer(), producer2 = VectorPromise::Producer()](auto& runLoop) mutable {
        producer2->then(runLoop,
            [&](const auto& resolveValue) { EXPECT_EQ(*resolveValue, Vector<int>(5, 42)); },
            doFail());

        // When producer1 is resolved, it will automatically settle producer2 with the resolved/reject value.
        producer1->chainTo(WTFMove(producer2));
        VectorPromise::createAndResolve(makeUnique<Vector<int>>(5, 42))->chainTo(WTFMove(producer1));
    });

    runInCurrentRunLoop([&, producer1 = VectorPromise::Producer()](auto& runLoop) mutable {
        auto promise = VectorPromise::createAndResolve(makeUnique<Vector<int>>(5, 42));
        producer1->then(runLoop,
            [&](auto&& resolveValue) { EXPECT_EQ(*resolveValue, Vector<int>(5, 42)); },
            doFail());

        // As promise1 is already resolved, it will automatically resolve/reject producer1 with its resolved/reject value.
        promise->chainTo(WTFMove(producer1));
    });
}

TEST(NativePromise, RunSynchronouslyOnTarget)
{
    // Check that the callback is executed immediately when the promise is resolved.
    runInCurrentRunLoop([](auto& runLoop) {
        GenericPromise::Producer producer(PromiseDispatchMode::RunSynchronouslyOnTarget);
        int result = 0;
        producer.resolve();
        producer->whenSettled(runLoop, [&] {
            result = 42;
        });
        EXPECT_EQ(result, 42);
    });
    // Check that the callback is executed immediately when using chained promise and that itself is set to RunSynchronouslyOnTarget
    runInCurrentRunLoop([](auto& runLoop) {
        GenericPromise::Producer producer(PromiseDispatchMode::RunSynchronouslyOnTarget);
        int result = 0;
        producer->whenSettled(runLoop, [&] {
            // We need to configure this promise too as RunSynchronouslyOnTarget
            GenericPromise::Producer producer(PromiseDispatchMode::RunSynchronouslyOnTarget);
            producer.resolve();
            return producer;
        })->whenSettled(runLoop, [&] {
            result = 42;
        });
        producer.resolve();
        EXPECT_EQ(result, 42);
    });
    // Check that the callback will still run on the proper target queue, even if RunSynchronouslyOnTarget is set.
    runInCurrentRunLoop([](auto& runLoop) {
        GenericPromise::Producer producer(PromiseDispatchMode::RunSynchronouslyOnTarget);
        Ref<GenericPromise> promise = producer;
        int result = 0;
        producer.resolve();
        {
            AutoWorkQueue awq;
            promise->whenSettled(awq.queue().get(), [&, queue = awq.queue()] {
                assertIsCurrent(queue);
                result = 42;
                queue->beginShutdown();
            });
        }
        EXPECT_EQ(result, 42);
    });
}

// Test that you can convert a NativePromise of different types (exclusive vs non-exclusive)
TEST(NativePromise, PromiseConversion)
{
    using MyPromise = NativePromise<void, int>;
    using MyNonExclusivePromise = NativePromise<void, int, PromiseOption::Default | PromiseOption::NonExclusive>;
    using MyNonExclusiveLongPromise = NativePromise<void, long, PromiseOption::Default | PromiseOption::NonExclusive>;
    runInCurrentRunLoop([](auto& runLoop) {
        auto nonExclusivePromise = MyNonExclusivePromise::createAndResolve();
        Ref<MyPromise> promise1 = nonExclusivePromise.get();
        promise1->whenSettled(runLoop, [](auto&& result) {
            EXPECT_TRUE(!!result);
        });
        Ref<MyPromise> promise2 = nonExclusivePromise.get();
        promise2->whenSettled(runLoop, [](auto&& result) {
            EXPECT_TRUE(!!result);
        });

        auto promise3 = nonExclusivePromise->convert<GenericPromise>();
        static_assert(std::is_same_v<Ref<GenericPromise>, decltype(promise3)>);
        promise3->whenSettled(runLoop, [](auto&& result) {
            EXPECT_TRUE(!!result);
        });

        // Converting a promise taking void as reject.
        auto promise4 = MyNonExclusiveLongPromise::createAndReject(1)->convert<GenericPromise>();
        static_assert(std::is_same_v<Ref<GenericPromise>, decltype(promise4)>);
        promise4->whenSettled(runLoop, [](auto&& result) {
            EXPECT_TRUE(!result);
        });

        // Converting a promise with different type.
        using IntPromise = NativePromise<int, int>;
        using DoublePromise = NativePromise<double, double>;

        auto promise5 = IntPromise::createAndResolve(1)->convert<DoublePromise>();
        static_assert(std::is_same_v<Ref<DoublePromise>, decltype(promise5)>);
        promise5->whenSettled(runLoop, [](auto&& result) {
            static_assert(std::is_same_v<double, std::remove_reference_t<decltype(result.value())>>);
            EXPECT_TRUE(result.has_value());
        });

        // Can convert to promise taking a data/void mix
        using DoubleVoidPromise = NativePromise<double, void>;
        auto promise6 = IntPromise::createAndReject(1)->convert<DoubleVoidPromise>();
        promise6->whenSettled(runLoop, [](auto&& result) {
            EXPECT_TRUE(!result);
        });
    });
}

TEST(NativePromise, MismatchChainTo)
{
    using MyPromise = NativePromise<void, int>;
    using MyNonExclusivePromise = NativePromise<void, int, PromiseOption::Default | PromiseOption::NonExclusive>;
    runInCurrentRunLoopUntilDone([](auto& runLoop, bool& done) {

        // Chaining resolved promise from producer
        auto nonExclusivePromise = MyNonExclusivePromise::createAndResolve();
        MyPromise::Producer producer1;
        producer1->whenSettled(runLoop, [](auto&& result) {
            EXPECT_TRUE(!!result);
        });
        nonExclusivePromise->chainTo(WTFMove(producer1));

        // Chaining promise, not yet resolved
        MyNonExclusivePromise::Producer producer2;
        auto promise2 = producer2.promise();
        MyPromise::Producer producer3;
        auto promise3 = producer3.promise();
        promise3->whenSettled(runLoop, [](auto&& result) {
            EXPECT_TRUE(!!result);
        });
        producer2->chainTo(WTFMove(producer3));
        EXPECT_FALSE(promise2->isSettled());
        EXPECT_FALSE(promise3->isSettled());
        producer2->resolve();
        // Resolving one producer synchronously resolve the other.
        EXPECT_TRUE(promise2->isSettled());
        EXPECT_TRUE(promise3->isSettled());

        // Chaining promise from convertible type
        using IntPromise = NativePromise<int, int>;
        using LongPromise = NativePromise<long, long>;
        auto intPromise1 = IntPromise::createAndResolve(1);
        LongPromise::Producer longPromiseProducer1;
        auto longPromise1 = longPromiseProducer1.promise();
        intPromise1->chainTo(WTFMove(longPromiseProducer1));
        longPromise1->whenSettled(runLoop, [](auto&& result) {
            EXPECT_TRUE(!!result);
            EXPECT_EQ(*result, long(1));
        });

        // Chaining non-exclusive promise to exclusive, check the end result is movable.
        using IntPromiseNonExcl = NativePromise<int, int, PromiseOption::Default | PromiseOption::NonExclusive>;
        auto intPromise2 = IntPromiseNonExcl::createAndResolve(1);
        LongPromise::Producer longPromiseProducer2;
        auto longPromise2 = longPromiseProducer2.promise();
        intPromise2->chainTo(WTFMove(longPromiseProducer2));
        longPromise2->whenSettled(runLoop, [](auto&& result) {
            using NonRefQualifiedType = typename std::remove_reference<decltype(result)>::type;
            static_assert(!std::is_const<NonRefQualifiedType>::value, "result is const qualified.");
            EXPECT_TRUE(!!result);
            EXPECT_EQ(*result, long(1));
        });
        intPromise2->whenSettled(runLoop, [](auto result) {
            EXPECT_TRUE(!!result);
            EXPECT_EQ(*result, 1);
        });

        // chaining ThenCommand
        auto intPromise3 = IntPromise::createAndResolve(1);
        LongPromise::Producer longPromiseProducer3;
        auto longPromise3 = longPromiseProducer3.promise();
        longPromise3->whenSettled(runLoop, [&](auto&& result) mutable {
            EXPECT_TRUE(!!result);
            EXPECT_EQ(*result, long(2));
            done = true;
        });
        intPromise3->whenSettled(runLoop, [](auto&& result) {
            EXPECT_TRUE(!!result);
            EXPECT_EQ(*result, long(1));
            return IntPromise::createAndResolve(2);
        })->chainTo(WTFMove(longPromiseProducer3));

        auto intPromise4 = IntPromise::createAndResolve(1);
        LongPromise::Producer longPromiseProducer4;
        auto longPromise4 = longPromiseProducer4.promise();
        intPromise4->whenSettled(runLoop, [](auto&& result) {
            EXPECT_TRUE(!!result);
            EXPECT_EQ(*result, long(1));
            return IntPromise::createAndReject(2);
        })->chainTo(WTFMove(longPromiseProducer4));
        longPromise4->whenSettled(runLoop, [&](auto&& result) mutable {
            EXPECT_TRUE(!result);
            EXPECT_EQ(result.error(), long(2));
            done = true;
        });
    });
}

TEST(NativePromise, MismatchChainToVoidPromise)
{
    runInCurrentRunLoopUntilDone([](auto& runLoop, bool& done) {
        // chaining with void promise
        using IntPromise = NativePromise<int, int>;
        using LongPromise = NativePromise<long, long>;
        auto intPromise1 = IntPromise::createAndResolve(1);
        LongPromise::Producer longPromiseProducer1;
        auto longPromise1 = longPromiseProducer1.promise();
        intPromise1->chainTo(WTFMove(longPromiseProducer1));
        longPromise1->whenSettled(runLoop, [](auto&& result) {
            EXPECT_TRUE(!!result);
            EXPECT_EQ(*result, long(1));
        });

        auto intPromise2 = IntPromise::createAndResolve(1);
        GenericPromise::Producer genericPromiseProducer1;
        genericPromiseProducer1->whenSettled(runLoop, [](auto&& result) {
            EXPECT_TRUE(!!result);
        });
        intPromise2->chainTo(WTFMove(genericPromiseProducer1));

        auto intPromise3 = IntPromise::createAndReject(1);
        GenericPromise::Producer genericPromiseProducer2;
        genericPromiseProducer2->whenSettled(runLoop, [&](auto&& result) {
            EXPECT_TRUE(!result);
            done = true;
        });
        intPromise3->chainTo(WTFMove(genericPromiseProducer2));
    });
}

TEST(NativePromise, CreateSettledPromise)
{
    runInCurrentRunLoopUntilDone([](auto& runLoop, bool& done) {
        using MyExpected = Expected<int, long>;
        createSettledPromise(MyExpected { makeUnexpected<long>(1) })->whenSettled(runLoop, [](auto&& result) {
            EXPECT_TRUE(!result);
            EXPECT_EQ(result.error(), 1L);
        });
        createSettledPromise(MyExpected { 1 })->whenSettled(runLoop, [&](auto&& result) {
            EXPECT_TRUE(!!result);
            EXPECT_EQ(result.value(), 1);
            done = true;
        });
    });
}

TEST(NativePromise, DisconnectNotOwnedInstance)
{
    GenericPromise::Producer producer;
    auto request = makeUnique<NativePromiseRequest>();
    WeakPtr weakRequest { *request };
    producer->whenSettled(RunLoop::main(), [request = WTFMove(request)] (auto&& result) mutable {
        request->complete();
        EXPECT_TRUE(false);
    })->track(*weakRequest);
    weakRequest->disconnect();
    EXPECT_FALSE(!!weakRequest);
    producer.resolve();
}

TEST(NativePromise, AutoRejectProducer)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        RefPtr<GenericPromise> promise1;
        {
            GenericPromise::AutoRejectProducer producer;
            promise1 = producer.promise();
        }
        promise1->then(queue, doFail(), [] {
            EXPECT_TRUE(true);
        });

        RefPtr<NativePromise<int, int>> promise2;
        {
            NativePromise<int, int>::AutoRejectProducer producer(-1);
            promise2 = producer.promise();
        }
        promise2->then(queue, doFail(), [](auto result) {
            EXPECT_EQ(result, -1);
        });

        // Check that AutoRejectProducer is usable with non-copyable type.
        RefPtr<NativePromise<int, std::unique_ptr<int>>> promise3;
        {
            NativePromise<int, std::unique_ptr<int>>::AutoRejectProducer producer(makeUniqueWithoutFastMallocCheck<int>(-1));
            promise3 = producer.promise();
        }
        promise3->then(queue, doFail(), [](auto&& result) {
            EXPECT_EQ(*result, -1);
        });

        RefPtr<NativePromise<int, std::unique_ptr<int>>> promise4;
        {
            NativePromise<int, std::unique_ptr<int>>::AutoRejectProducer producer(makeUniqueWithoutFastMallocCheck<int>(-2));
            promise4 = producer.promise();
            producer.setDefaultReject(makeUniqueWithoutFastMallocCheck<int>(-1));
        }
        promise4->then(queue, doFail(), [queue](auto&& result) {
            EXPECT_EQ(*result, -1);
            queue->beginShutdown();
        });
    });
}

TEST(NativePromise, ResolveWithFunction)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();

    // Test settled with expected value, before whenSettled.
    NativePromise<int, int>::Producer producer1;
    producer1.settleWithFunction([queue]() -> NativePromise<int, int>::Result {
        assertIsCurrent(queue);
        return 1;
    });
    producer1.promise()->whenSettled(queue, [](auto result) {
        EXPECT_TRUE(!!result);
        EXPECT_EQ(*result, 1);
    });

    // Test settled with rejected value, before whenSettled.
    NativePromise<int, int>::Producer producer2;
    producer2.settleWithFunction([queue]() -> NativePromise<int, int>::Result {
        assertIsCurrent(queue);
        return makeUnexpected(2);
    });
    producer2.promise()->whenSettled(queue, [](auto result) {
        EXPECT_FALSE(!!result);
        EXPECT_EQ(result.error(), 2);
    });

    // Test settled with expected value, after whenSettled.
    NativePromise<int, int>::Producer producer3;
    producer3.promise()->whenSettled(queue, [](auto result) {
        EXPECT_TRUE(!!result);
        EXPECT_EQ(*result, 1);
    });
    producer3.settleWithFunction([queue]() -> NativePromise<int, int>::Result {
        assertIsCurrent(queue);
        return 1;
    });

    // Test settled with rejected value, after whenSettled.
    NativePromise<int, int>::Producer producer4;
    producer4.promise()->whenSettled(queue, [](auto result) {
        EXPECT_FALSE(!!result);
        EXPECT_EQ(result.error(), 2);
    });
    producer4.settleWithFunction([queue]() -> NativePromise<int, int>::Result {
        assertIsCurrent(queue);
        return makeUnexpected(2);
    });

    // Test with settle(Function) syntax
    NativePromise<int, int>::Producer producer5;
    producer5.settle([queue]() -> NativePromise<int, int>::Result {
        assertIsCurrent(queue);
        return 1;
    });
    producer5.promise()->whenSettled(queue, [queue](auto result) {
        EXPECT_TRUE(!!result);
        EXPECT_EQ(*result, 1);
        queue->beginShutdown();
    });
}

// Example:
// Consider a PhotoProducer class that can take a photo and returns an image and its mimetype.
// The PhotoProducer uses some system framework that takes a completion handler which will receive the photo once taken.
// The PhotoProducer uses its own WorkQueue to perform the work so that it won't block the thread it's called on.
// We want the PhotoProducer to be able to be called on any threads.

// This would be the system framework.
struct AVCaptureMethod {
    static void captureImage(std::function<void(std::vector<uint8_t>&&, std::string&&)>&& handler)
    {
        handler({ 1, 2, 3, 4, 5 }, "image/jpeg");
    }
};

struct PhotoSettings { };

// Needed until we get c++23 moveonly_function
template<class F>
auto makeMoveableFunction(F&& f)
{
    return [sharedPtr = std::make_shared<std::decay_t<F>>(std::forward<F>(f))] (auto&&... args) {
        return std::invoke(*sharedPtr, decltype(args)(args)...);
    };
}

class PhotoProducer : public ThreadSafeRefCounted<PhotoProducer> {
public:
    using PhotoPromise = NativePromise<std::pair<Vector<uint8_t>, String>, int>;
    static Ref<PhotoProducer> create(const PhotoSettings& settings) { return adoptRef(*new PhotoProducer(settings)); }

    Ref<PhotoPromise> takePhoto() const
    {
        // This can be called on any threads.
        // It uses invokeAsync which returns a NativePromise that will be settled when the promise returned by the method will itself be settled.
        // (the invokeAsync promise is "chained" to the promise returned by `takePhotoImpl()`)
        return invokeAsync(m_generatePhotoQueue, [protectedThis = Ref { *this }] {
            assertIsCurrent(protectedThis->m_generatePhotoQueue);
            return protectedThis->takePhotoImpl();
        });
    }
private:
    explicit PhotoProducer(const PhotoSettings& settings)
        : m_generatePhotoQueue(WorkQueue::create("takePhoto queue"_s))
    {
    }

    Ref<PhotoPromise> takePhotoImpl() const
    {
        PhotoPromise::Producer producer;
        Ref<PhotoPromise> promise = producer;

        AVCaptureMethod::captureImage(makeMoveableFunction([producer = WTFMove(producer)] (std::vector<uint8_t>&& image, std::string&& mimeType) {
            // Note that you can resolve a NativePromise on any threads. Unlike with a CompletionHandler it is not the responsibility of the producer to resolve the promise
            // on a particular thread.
            // The consumer specifies the thread on which it wants to be called back.
            producer.resolve(std::make_pair<Vector<uint8_t>, String>(std::span { image }, std::span<const char> { mimeType }));
        }));

        // Return the promise which the producer will resolve at a later stage.
        return promise;
    }
    Ref<WorkQueue> m_generatePhotoQueue;
};

TEST(NativePromise, TakePhotoExample)
{
    AutoWorkQueue awk;
    auto queue = awk.queue();
    queue->dispatch([queue] {
        auto photoProducer = PhotoProducer::create(PhotoSettings { });
        photoProducer->takePhoto()->whenSettled(queue, [queue] (PhotoProducer::PhotoPromise::Result&& result) mutable {
            static_assert(std::is_same_v<decltype(result.value()), std::pair<Vector<uint8_t>, String>&>);
            if (result)
                EXPECT_EQ(result.value().second, "image/jpeg"_s);
            else
                EXPECT_TRUE(false); // Got an unexpected error.
            queue->beginShutdown();
        });
    });
}

} // namespace TestWebKitAPI
