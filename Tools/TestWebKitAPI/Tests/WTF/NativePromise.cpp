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
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>
#include <wtf/threads/BinarySemaphore.h>

using namespace WTF;

namespace TestWebKitAPI {

using TestPromise = NativePromise<int, double, false>;
using TestPromiseExcl = NativePromise<int, double, true /* exclusive */>;

class WorkQueueWithShutdown : public WorkQueue {
public:
    static Ref<WorkQueueWithShutdown> create(const char* name) { return adoptRef(*new WorkQueueWithShutdown(name)); }
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
    WorkQueueWithShutdown(const char* name)
        : WorkQueue(name, QOS::Default)
    {
    }
    std::atomic<bool> m_shutdown { false };
    BinarySemaphore m_semaphore;
};

class AutoWorkQueue {
public:
    AutoWorkQueue()
        : m_workQueue(WorkQueueWithShutdown::create("com.apple.WebKit.Test.simple"))
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
    RefCountedProducer()
        : producer(__func__)
    {
    }
    Ref<TestPromise> promise() { return producer; }
    typename TestPromise::Producer producer;
};

class DelayedResolveOrReject final : public ThreadSafeRefCounted<DelayedResolveOrReject> {
public:
    DelayedResolveOrReject(WorkQueue& workQueue, RefPtr<RefCountedProducer> producer, TestPromise::Result&& result, int iterations)
        : m_producer(producer)
        , m_iterations(iterations)
        , m_workQueue(workQueue)
        , m_result(WTFMove(result))
    {
    }

    void dispatch()
    {
        m_workQueue->dispatch([strongThis = RefPtr { this }] {
            strongThis->run();
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
            m_producer->producer.resolveOrReject(m_result, __func__);
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

static std::function<void()> doFail()
{
    return [] {
        EXPECT_TRUE(false);
    };
}

static auto doFailAndReject(const char* location)
{
    return [location] {
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

TEST(NativePromise, BasicResolve)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        TestPromise::createAndResolve(42, __func__)->then(queue, __func__,
            [queue](int resolveValue) {
                EXPECT_EQ(resolveValue, 42);
                queue->beginShutdown();
            }, doFail());
    });
}

TEST(NativePromise, BasicReject)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        TestPromise::createAndReject(42.0, __func__)->then(queue, __func__,
            doFail(),
            [queue](int rejectValue) {
                EXPECT_EQ(rejectValue, 42.0);
                queue->beginShutdown();
            });
    });
}

TEST(NativePromise, BasicResolveOrRejectResolved)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        TestPromise::createAndResolve(42, __func__)->whenSettled(queue, __func__,
            [queue](const TestPromise::Result& result) {
                EXPECT_TRUE(result);
                EXPECT_EQ(result.value(), 42);
                queue->beginShutdown();
            });
    });
}

TEST(NativePromise, BasicResolveOrRejectRejected)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        TestPromise::createAndReject(42.0, __func__)->whenSettled(queue, __func__,
            [queue](const TestPromise::Result& result) {
                EXPECT_TRUE(!result);
                EXPECT_EQ(result.error(), 42.0);
                queue->beginShutdown();
            });
    });
}

TEST(NativePromise, GenericPromise)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        GenericPromise::createAndResolve(__func__)->then(queue, __func__,
            []() {
                EXPECT_TRUE(true);
            },
            doFail());
        GenericPromise::createAndResolve(__func__)->whenSettled(queue, __func__,
            [](GenericPromise::Result result) {
                EXPECT_TRUE(result);
            });

        GenericPromise::createAndReject(123, __func__)->whenSettled(queue, __func__,
            [](GenericPromise::Result result) {
                EXPECT_TRUE(!result);
                EXPECT_EQ(result.error(), 123);
            });

        GenericPromise::Producer p1(__func__);
        p1.resolve(__func__);
        p1.then(queue, __func__,
            []() {
                EXPECT_TRUE(true);
            },
            doFail());

        GenericPromise::Producer p2(__func__);
        p2.reject(123, __func__);
        p2.then(queue, __func__,
            []() {
                EXPECT_TRUE(true);
            },
            [](int value) {
                EXPECT_EQ(value, 123);
            });

        GenericPromise::Producer p3(__func__);
        NativePromiseRequest<GenericPromise> request;

        // Note that if you're not interested in the result you can provide two Function<void()> to then()
        p3.then(queue, __func__, doFail(), doFail()).track(request);
        p3.resolve(__func__);

        // We are no longer interested by the result of the promise. We disconnect the request holder.
        // doFail() above will never be called.
        request.disconnect();

        // Note that if you're not interested in the result you can also provide one Function<void()> with whenSettled()
        GenericPromise::Producer p4(__func__);
        p4.whenSettled(queue, __func__, []() {
        });
        p4.resolve(__func__);

        // You can mix & match promise types and chain them together.
        // Producer also accepts syntax using operator-> for consistency with a consumer's promise.
        GenericPromise::Producer p5(__func__);
        using MyPromise = NativePromise<int, int, true>;
        p5->whenSettled(queue, __func__,
            [](GenericPromise::Result result) {
                EXPECT_TRUE(result.has_value());
                return MyPromise::createAndResolve(1, __func__);
            })->whenSettled(queue, __func__,
                [queue](MyPromise::Result result) {
                    static_assert(std::is_same_v<MyPromise::Result::value_type, int>, "The type received is the same as the last promise returned");
                    EXPECT_TRUE(result.has_value());
                    EXPECT_EQ(result.value(), 1);
                    queue->beginShutdown();
                });
        p5->resolve(__func__);
    });
}

TEST(NativePromise, PromiseRequest)
{
    // We declare the Request holder before using the runLoop to ensure it stays in scope for the entire run.
    // ASSERTION FAILED: !m_request
    using MyPromise = NativePromise<bool, bool, true>;
    NativePromiseRequest<MyPromise> request1;

    runInCurrentRunLoop([&](auto& runLoop) {
        MyPromise::Producer p1(__func__);
        p1.resolve(true, __func__);

        p1.whenSettled(runLoop, __func__,
            [&](MyPromise::Result&& result) {
                EXPECT_TRUE(result.has_value());
                EXPECT_TRUE(result.value());
                request1.complete();
            }).track(request1);
    });

    // PromiseRequest allows to use capture by reference or pointer to ref-counted object and ensure the
    // lifetime of the object.
    runInCurrentRunLoop([&](auto& runLoop) {
        NativePromiseRequest<GenericPromise> request2;
        bool objectToShare = true;
        GenericPromise::Producer p2(__func__);
        p2.whenSettled(runLoop, __func__,
            [&objectToShare](GenericPromise::Result&&) mutable {
                // It would be normally unsafe to access `objectToShare` as it went out of scope.
                // but this function will never run as we've disconnected the ThenCommand.
                objectToShare = false;
            }).track(request2);
        request2.disconnect();
        p2.resolve(__func__);
        EXPECT_TRUE(objectToShare);
    });
}

// Ensure that callbacks aren't run when request holder is disconnected after the promise was resolved and then() called.
TEST(NativePromise, PromiseRequestDisconnected1)
{
    runInCurrentRunLoop([](auto& runLoop) {
        NativePromiseRequest<TestPromise> request;

        TestPromise::Producer p(__func__);
        p.then(runLoop, __func__, doFail(), doFail()).track(request);

        p.resolve(1, __func__);
        request.disconnect();
    });
}

// Ensure that callbacks aren't run when request holder is disconnected even if promise was resolved first.
TEST(NativePromise, PromiseRequestDisconnected2)
{
    runInCurrentRunLoop([](auto& runLoop) {
        NativePromiseRequest<TestPromise> request;

        TestPromise::Producer p(__func__);
        p->resolve(1, __func__);

        p->then(runLoop, __func__, doFail(), doFail())->track(request);

        request.disconnect();
    });
}
TEST(NativePromise, AsyncResolve)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        auto producer = adoptRef(new RefCountedProducer());
        auto p = producer->promise();

        // Kick off three racing tasks, and make sure we get the one that finishes
        // earliest.
        auto a = adoptRef(new DelayedResolveOrReject(queue, producer, TestPromise::Result(32), 10));
        auto b = adoptRef(new DelayedResolveOrReject(queue, producer, TestPromise::Result(42), 5));
        auto c = adoptRef(new DelayedResolveOrReject(queue, producer, TestPromise::Error(32.0), 7));

        a->dispatch();
        b->dispatch();
        c->dispatch();

        p->then(queue, __func__,
            [queue, a, b, c](int resolveValue) {
                EXPECT_EQ(resolveValue, 42);
                a->cancel();
                b->cancel();
                c->cancel();
                queue->beginShutdown();
            },
            doFail());
    });
}

TEST(NativePromise, CompletionPromises)
{
    std::atomic<bool> invokedPass { false };
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue, &invokedPass] {
        TestPromise::createAndResolve(40, __func__)->then(queue, __func__,
            [](int val) {
                return TestPromise::createAndResolve(val + 10, __func__);
            },
            doFailAndReject(__func__))->then(queue, __func__,
                [&invokedPass](int val) {
                    invokedPass = true;
                    return TestPromise::createAndResolve(val, __func__);
                },
                doFailAndReject(__func__))->then(queue, __func__,
                    [queue](int val) {
                        auto producer = adoptRef(new RefCountedProducer());
                        auto p = producer->promise();

                        auto resolver = adoptRef(new DelayedResolveOrReject(queue, producer, TestPromise::Result(val - 8), 10));
                        resolver->dispatch();
                        return p;
                    },
                    doFailAndReject(__func__))->then(queue, __func__,
                        [](int val) {
                            return TestPromise::createAndReject(double(val - 42) + 42.0, __func__);
                        },
                        doFailAndReject(__func__))->then(queue, __func__,
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
        void resolveOrRejectWithNothing()
        {
            EXPECT_TRUE(true);
        }
        void resolveOrRejectWithResult(const TestPromise::Result& result)
        {
            EXPECT_TRUE(result.has_value());
            EXPECT_EQ(result.value(), 1);
        }
    };
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue, myClass = MyClass::create()] {
        TestPromise::createAndResolve(1, __func__)->then(queue, __func__, myClass.get(), &MyClass::resolveWithNothing, &MyClass::rejectWithNothing);
        TestPromise::createAndReject(2.0, __func__)->then(queue, __func__, myClass.get(), &MyClass::resolveWithValue, &MyClass::rejectWithValue);
        TestPromise::createAndResolve(3, __func__)->whenSettled(queue, __func__, myClass.get(), &MyClass::resolveOrRejectWithNothing);
        TestPromise::createAndResolve(1, __func__)->whenSettled(queue, __func__, myClass.get(), &MyClass::resolveOrRejectWithResult);
        queue->dispatch([queue] {
            queue->beginShutdown();
        });
    });
}

static Ref<GenericPromise> myMethod()
{
    assertIsCurrent(RunLoop::main());
    // You would normally do some work here.
    return GenericPromise::createAndResolve(__func__);
}

TEST(NativePromise, InvokeAsync)
{
    bool done = false;
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue, &done] {
        invokeAsync(RunLoop::main(), __func__, &myMethod)->whenSettled(queue, __func__,
            [queue, &done](GenericPromise::Result result) {
                EXPECT_TRUE(result.has_value());
                queue->beginShutdown();
                done = true;
            });
    });

    Util::run(&done);
}

static Ref<GenericPromise> myMethodReturningThenCommand()
{
    assertIsCurrent(RunLoop::main());
    // You would normally do some work here.
    return GenericPromise::createAndResolve(__func__)->whenSettled(RunLoop::main(), __func__,
        [](GenericPromise::Result result) {
            return GenericPromise::createAndResolveOrReject(WTFMove(result), __func__);
        });
}

TEST(NativePromise, InvokeAsyncAutoConversion)
{
    // Ensure that there's no need to cast NativePromise::then() result
    bool done = false;
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue, &done] {
        invokeAsync(RunLoop::main(), __func__, &myMethodReturningThenCommand)->whenSettled(queue, __func__,
            [queue, &done](GenericPromise::Result result) {
                EXPECT_TRUE(result.has_value());
                queue->beginShutdown();
                done = true;
            });
    });

    Util::run(&done);
}

static Ref<GenericPromise> myMethodReturningProducer()
{
    assertIsCurrent(RunLoop::main());
    // You would normally do some work here.
    return GenericPromise::createAndResolve(__func__)->whenSettled(RunLoop::main(), __func__,
        [](GenericPromise::Result result) {
            GenericPromise::Producer producer(__func__);
            producer.resolveOrReject(WTFMove(result), __func__);
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
        invokeAsync(RunLoop::main(), __func__, &myMethodReturningProducer)->whenSettled(queue, __func__,
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
        promises.append(TestPromise::createAndResolve(22, __func__));
        promises.append(TestPromise::createAndResolve(32, __func__));
        promises.append(TestPromise::createAndResolve(42, __func__));

        TestPromise::all(queue, promises)->then(queue, __func__,
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

TEST(NativePromise, PromiseAllResolveAsync)
{
    AutoWorkQueue awq;
    auto queue = awq.queue();
    queue->dispatch([queue] {
        Vector<Ref<TestPromise>> promises;
        promises.append(invokeAsync(queue, __func__, [] {
            return TestPromise::createAndResolve(22, __func__);
        }));
        promises.append(invokeAsync(queue, __func__, [] {
            return TestPromise::createAndResolve(32, __func__);
        }));
        promises.append(invokeAsync(queue, __func__, [] {
            return TestPromise::createAndResolve(42, __func__);
        }));

        TestPromise::all(queue, promises)->then(queue, __func__,
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
        promises.append(TestPromise::createAndResolve(22, __func__));
        promises.append(TestPromise::createAndReject(32.0, __func__));
        promises.append(TestPromise::createAndResolve(42, __func__));
        // Ensure that more than one rejection doesn't cause a crash
        promises.append(TestPromise::createAndReject(52.0, __func__));

        TestPromise::all(queue, promises)->then(queue, __func__,
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
        promises.append(invokeAsync(queue, __func__, [] {
            return TestPromise::createAndResolve(22, __func__);
        }));
        promises.append(invokeAsync(queue, __func__, [] {
            return TestPromise::createAndReject(32.0, __func__);
        }));
        promises.append(invokeAsync(queue, __func__, [] {
            return TestPromise::createAndResolve(42, __func__);
        }));
        // Ensure that more than one rejection doesn't cause a crash
        promises.append(invokeAsync(queue, __func__, [] {
            return TestPromise::createAndReject(52.0, __func__);
        }));

        TestPromise::all(queue, promises)->then(queue, __func__,
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
        promises.append(TestPromise::createAndResolve(22, __func__));
        promises.append(TestPromise::createAndReject(32.0, __func__));
        promises.append(TestPromise::createAndResolve(42, __func__));
        promises.append(TestPromise::createAndReject(52.0, __func__));

        TestPromise::allSettled(queue, promises)->then(
            queue, __func__,
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
        promises.append(invokeAsync(queue, __func__, [] {
            return TestPromise::createAndResolve(22, __func__);
        }));
        promises.append(invokeAsync(queue, __func__, [] {
            return TestPromise::createAndReject(32.0, __func__);
        }));
        promises.append(invokeAsync(queue, __func__, [] {
            return TestPromise::createAndResolve(42, __func__);
        }));
        promises.append(invokeAsync(queue, __func__, [] {
            return TestPromise::createAndReject(52.0, __func__);
        }));

        TestPromise::allSettled(queue, promises)->then(queue, __func__,
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

TEST(NativePromise, Chaining)
{
    // We declare this variable before |awq| to ensure the destructor is run after |holder.disconnect()|.
    NativePromiseRequest<TestPromise> holder;

    AutoWorkQueue awq;
    auto queue = awq.queue();

    queue->dispatch([queue, &holder] {
        auto p = TestPromise::createAndResolve(42, __func__);
        const size_t kIterations = 100;
        for (size_t i = 0; i < kIterations; ++i) {
            p = p->then(queue, __func__,
                [](int val) {
                    EXPECT_EQ(val, 42);
                    return TestPromise::createAndResolve(val, __func__);
                },
                [](double val) {
                    return TestPromise::createAndReject(val, __func__);
                });

            if (i == kIterations / 2) {
                p->then(queue, __func__,
                    [queue, &holder] {
                        holder.disconnect();
                        queue->beginShutdown();
                    },
                    doFail());
            }
        }
        // We will hit the assertion if we don't disconnect the leaf Request
        // in the promise chain.
        p->whenSettled(queue, __func__, [] { })->track(holder);
    });
}

TEST(NativePromise, MoveOnlyType)
{
    using MyPromise = NativePromise<std::unique_ptr<int>, bool, true>;

    AutoWorkQueue awq;
    auto queue = awq.queue();

    MyPromise::createAndResolve(std::make_unique<int>(87), __func__)->then(queue, __func__,
        [](std::unique_ptr<int> val) {
            EXPECT_EQ(87, *val);
        },
        [] {
            EXPECT_TRUE(false);
        });

    MyPromise::createAndResolve(std::make_unique<int>(87), __func__)->whenSettled(queue, __func__,
        [queue](MyPromise::Result&& val) {
            EXPECT_TRUE(val.has_value());
            EXPECT_EQ(87, *(val.value()));
            queue->beginShutdown();
        });
}

TEST(NativePromise, HeterogeneousChaining)
{
    using Promise1 = NativePromise<std::unique_ptr<char>, bool, true>;
    using Promise2 = NativePromise<std::unique_ptr<int>, bool, true>;

    NativePromiseRequest<Promise2> holder;

    AutoWorkQueue awq;
    auto queue = awq.queue();

    queue->dispatch([queue, &holder] {
        Promise1::createAndResolve(std::make_unique<char>(0), __func__)->whenSettled(queue, __func__,
            [&holder] {
                holder.disconnect();
                return Promise2::createAndResolve(std::make_unique<int>(0), __func__);
            })->whenSettled(queue, __func__,
                [] {
                    // Shouldn't be called for we've disconnected the request.
                    EXPECT_FALSE(true);
                })->track(holder);
    });

    Promise1::createAndResolve(std::make_unique<char>(87), __func__)->then(queue, __func__,
        [](std::unique_ptr<char> val) {
            EXPECT_EQ(87, *val);
            return Promise2::createAndResolve(std::make_unique<int>(94), __func__);
        },
        [] {
            return Promise2::createAndResolve(std::make_unique<int>(95), __func__);
        })->then(queue, __func__,
            [](std::unique_ptr<int> val) {
                EXPECT_EQ(94, *val);
            },
            doFail());

    Promise1::createAndResolve(std::make_unique<char>(87), __func__)->whenSettled(queue, __func__,
        [](Promise1::Result&& result) {
            EXPECT_EQ(87, *(result.value()));
            return Promise2::createAndResolve(std::make_unique<int>(94), __func__);
        })->whenSettled(queue, __func__,
            [queue](Promise2::Result&& result) {
                EXPECT_EQ(94, *(result.value()));
            });

    // Chaining promises of different types, even if returned from within callback
    TestPromise::createAndResolve(1, __func__)->whenSettled(queue, __func__,
        [queue] {
            return TestPromiseExcl::createAndResolve(2, __func__)->whenSettled(queue, __func__, [] {
                return TestPromise::createAndResolve(3, __func__);
            });
        })->whenSettled(queue, __func__,
            [queue](TestPromise::Result result) {
                EXPECT_TRUE(result.has_value());
                EXPECT_EQ(3, result.value());
            });

    TestPromise::createAndResolve(1, __func__)->whenSettled(queue, __func__,
        [queue] {
            return TestPromiseExcl::createAndResolve(2, __func__)->whenSettled(queue, __func__, [] {
                return GenericPromise::createAndResolve(__func__);
            });
        })->whenSettled(queue, __func__,
            [queue](GenericPromise::Result result) {
                EXPECT_TRUE(result.has_value());
            });

    TestPromise::createAndResolve(1, __func__)->whenSettled(queue, __func__,
        [queue] {
            return TestPromiseExcl::createAndResolve(2, __func__)->whenSettled(queue, __func__, [] {
                return GenericPromise::createAndReject(1, __func__);
            });
        })->whenSettled(queue, __func__,
            [queue](GenericPromise::Result result) {
                EXPECT_FALSE(result.has_value());
                EXPECT_EQ(1, result.error());
                queue->beginShutdown();
            });
}

TEST(NativePromise, RunLoop)
{
    runInCurrentRunLoop([](auto& runLoop) {
        TestPromise::createAndResolve(42, __func__)->then(runLoop, __func__,
            [](int resolveValue) {
                EXPECT_EQ(resolveValue, 42);
            },
            doFail());
    });
}

TEST(NativePromise, ImplicitConversionWithForwardPreviousReturn)
{
    runInCurrentRunLoop([](auto& runLoop) {
        TestPromise::Producer p(__func__);
        Ref<TestPromise> promise = p->whenSettled(runLoop, __func__,
            [](const TestPromise::Result& result) {
                return TestPromise::createAndResolveOrReject(result, __func__);
            });
        promise->whenSettled(runLoop, __func__, [&](const TestPromise::Result& result) {
            EXPECT_TRUE(!result.has_value());
            EXPECT_FALSE(promise->isResolved());
        });
        p.resolve(1, __func__);
        Ref<TestPromise> originalPromise = p;
        EXPECT_TRUE(originalPromise->isResolved());
        // This could be written as EXPECT_TRUE(static_cast<Ref<TestPromise>>(p)->isResolved());
        // but MSVC errors on it.
        EXPECT_FALSE(promise->isResolved()); // The callback hasn't been run yet.
    });
}

TEST(NativePromise, ChainTo)
{
    runInCurrentRunLoop([&, producer2 = TestPromise::Producer(__func__)](auto& runLoop) mutable {
        auto promise1 = TestPromise::createAndResolve(42, __func__);
        producer2->then(runLoop, __func__,
            [&](int resolveValue) { EXPECT_EQ(resolveValue, 42); },
            doFail());

        // As promise1 is already resolved, it will automatically resolve/reject producer2 with its resolved/reject value.
        promise1->chainTo(WTFMove(producer2), __func__);
    });

    runInCurrentRunLoop([&, producer2 = TestPromise::Producer(__func__), producer1 = TestPromise::Producer(__func__)](auto& runLoop) mutable {
        producer2->then(runLoop, __func__,
            [&](int resolveValue) { EXPECT_EQ(resolveValue, 42); },
            doFail());

        // When producer1 is resolved, it will automatically resolve/reject producer2 with the resolved/reject value.
        producer1->chainTo(WTFMove(producer2), __func__);
        producer1.resolve(42, __func__);
    });
}

} // namespace TestWebKitAPI
