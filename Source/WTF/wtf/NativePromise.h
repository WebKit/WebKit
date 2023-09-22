/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

// NativePromise is a C++ Promise implementation based on Mozilla's MozPromise.

#pragma once

#if ASSERT_ENABLED
#include <atomic>
#endif
#include <type_traits>
#include <utility>
#include <wtf/Assertions.h>
#include <wtf/Expected.h>
#include <wtf/FunctionDispatcher.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TypeTraits.h>
#include <wtf/Unexpected.h>
#include <wtf/Vector.h>

namespace WTF {

/*
 * A promise manages an asynchronous request that may or may not be able to be fulfilled immediately.
 * When an API returns a promise, the consumer may attach callbacks to be invoked (asynchronously, on a specified thread)
 * when the request is either completed (resolved) or cannot be completed (rejected).
 *
 * A NativePromise object is thread safe, and may be ->then()ed on any thread.
 * The then() call accepts either a resolve and reject callback, while whenSettled() accepts a resolveOrReject one.
 *
 * NativePromise::then() and NativePromise::whenSettled() returns a NativePromise::Request object. This request can be either:
 * 1- Converted back to a NativePromise which will be resolved or rejected once the resolve/reject callbacks are run.
 *    This new NativePromise can be then()ed again to chain multiple operations.
 * 2- Be tracked using a NativePromiseRequest: this allows the caller to cancel the delivery of the resolve/reject result if it has not already occurred.
 *    (call to NativePromiseRequest::disconnect() must be done on the target thread to avoid thread safety issues).
 *
 * When IsExclusive is true:
 *  - The NativePromise performs run-time assertions that there is at most one call to either then(...) or chainTo(...).
 *  - Move semantics are used when passing arguments
 *  - The resolved or rejected object will be deleted on the target thread.
 *  - The ResolveValueType and RejectValueType must be moveable (e.g have a move constructor). Compilation will fail otherwise.
 * Otherwise:
 * - values are passed to the resolve/reject callbacks through either const references or pointers.
 * - the resolve or reject object will be deleted on the last SerialFunctionDispatcher that got used.
 *
 * A typical workflow would be as follow:
 * If the work is to be done immediately:
 * From the producer side:
 *  - Do the work
 *  - return a resolved or rejected promise via NativePromise::createAndResolve or NativePromise::createAndReject
 * From the consumer side:
 *  - call the method returning a promise
 *  - then()/whenSettled() on the promise to set the actions to run once the promise has settled.
 *
 * If the work is to be done at a later stage:
 * From the producer side:
 *  - Allocate a NativePromise::Producer (via NativePromise::Producer::create() and return it to the consumer has a Ref<NativePromise>
 *  - Do the work
 *  - Once the work has been completed, either resolve or reject the NativePromise::Producer object.
 * From the consumer side:
 *  - call the method returning a promise
 *  - then() on the promise to set the actions to run once the promise has settled.
 *
 * In either case (immediate or later resolution) using a NativePromiseRequest:
 *  - track the promise
 *  - cancel the delivery of the resolve/reject result and prevent callbacks to be run.
 *
 * By disconnecting the NativePromiseRequest (via NativePromiseRequest::disconnect(), the then()/whenSettled() callbacks will not be run.
 *
 * For now, care should be taken by the Producer to only return an object that is usable on the target's queue (don't return an AtomString for example)
 *
 * Examples:
 * 1. Basic usage. methodA runs on the main thread, methodB must run on a WorkQueue, and expects a std::unique<int>.
 *    methodA calls methodB for asynchronous work and will perform some work once methodB is done.
 *
 *    static Ref<GenericPromise> methodB(std_unique<int>&& arg)
 *    {
 *        assertIsCurrent(workQueue);
 *        // Do something with arg and once done return a resolved promise.
 *        if (all_ok)
 *            return GenericPromise::createAndResolve(__func__);
 *        else
 *            return GenericPromise::createAndReject(-1, __func__);
 *    }
 *
 *    static void methodA()
 *    {
 *        assertIsMainThread();
 *        auto arg = std::make_unique<int>(20);
 *        // invokeAsync returns a promise of same type as what the function returns,
 *        // and it will be resolved or rejected when the original promise is settled.
 *        invokeAsync(workQueue, __func__, [arg = WTFMove(arg)] () mutable { return methodB(WTFMove(arg); })
 *        ->then(RunLoop::main(), __func__,
 *            []() {
 *                assertIsMainThread();
 *                // Method succeeded
 *            }, [](int) {
 *                assertIsMainThread();
 *                // Method failed
 *            });
 *    }
 *
 * 2. Using lambdas
 *    auto p = MyAsyncMethod(); // MyAsyncMethod returns a Ref<NativePromise>, and perform some work on some thread.
 *    p->then(RunLoop::main(), __func__, [] (NativePromise::Result&& result) {
 *        assertIsMainThread();
 *        if (result) {
 *            auto resolveValue = WTFMove(result.value());
 *        } else {
 *            auto rejectValue = WTFMove(result.error());
 *        }
 *    }
 *
 * 3. Using a NativePromiseRequest
 *    NativePromiseRequest<GenericPromise> request;
 *
 *    GenericPromise::Producer p(__func__);
 *    // Note that if you're not interested in the result you can provide a Function<void()>
 *    p->then(RunLoop::main(), __func__,
 *            [] { CRASH("resolve callback won't be run"); },
 *            [] { CRASH("reject callback won't be run"); })
 *      ->track(request);
 *
 *    // We resolve the promise.
 *    p.resolve(__func__);
 *
 *    // We are no longer interested by the result of the promise. We disconnect the request holder.
 *    request.disconnect();
 *
 * 4. Chaining promises of different types
 *    auto p = MyAsyncMethod(); // MyAsyncMethod returns a Ref<MyNativePromise>, and perform some work on some thread.
 *    auto p2 = p->then(RunLoop::main(), __func__, [] (MyNativePromise::ResolveValueType val) {
 *            assertIsMainThread();
 *            if (val)
 *                return MyOtherPromise::createAndResolve(val, __func__);
 *            return MyOtherPromise::createAndReject(val, __func__);
 *        }, [] (MyOtherPromise::RejectValueType val) {
 *            return MyOtherPromise::createAndReject(val, __func__);
 *        }) // The type returned by then() is of the last PromiseType returned in the chain.
 *        ->whenSettled(RunLoop::main(), __func__, [] (const MyOtherPromise::Result&) -> void {
 *            // do something else
 *        });
 *
 * For additional examples on how to use NativePromise, refer to NativePromise.cpp API tests.
 */

class NativePromiseBase : public ThreadSafeRefCounted<NativePromiseBase>  {
public:
    virtual void assertIsDead() = 0;
    virtual ~NativePromiseBase() = default;
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    WTF_EXPORT_PRIVATE static WTFLogChannel* logChannel();
    template<typename... Args>
    static void log(const char* format, Args... arguments)
    {
        ALLOW_NONLITERAL_FORMAT_BEGIN
        WTFLogWithLevel(logChannel(), WTFLogLevel::Debug, format, arguments...);
        ALLOW_NONLITERAL_FORMAT_END
    }
#endif
};

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
#define PROMISE_LOG(x, ...) NativePromiseBase::log(x, __VA_ARGS__)
#else
#define PROMISE_LOG(x, ...) ((void)0)
#endif

class ConvertibleToNativePromise { };

template<typename T>
class NativePromiseRequest;

enum class PromiseDispatchMode : uint8_t {
    Default, // ResolveRejectCallbacks will be dispatched on the target thread.
    RunSynchronouslyOnTarget, // ResolveRejectCallbacks will be run synchronously if target thread is current.

};

template<typename ResolveValueT, typename RejectValueT, bool IsExclusive>
class NativePromise final : public NativePromiseBase, public ConvertibleToNativePromise {
public:
    using Result = Expected<ResolveValueT, RejectValueT>;
    using Error = Unexpected<RejectValueT>;
    using ResolveValueType = ResolveValueT;
    using RejectValueType = RejectValueT;

    // used by IsConvertibleToNativePromise to determine how to cast the result.
    using PromiseType = NativePromise;

    // We split the functionalities from a "Producer" that can create and resolve/reject a promise and a "Consumer"
    // that will then()/whenSettled() on such promise.
    class Producer;

    // Request is the object returned by NativePromise::then()/whenSettled is used by NativePromiseRequest holder to track/disconnect.
    class Request : public ThreadSafeRefCounted<Request> {
    public:
        virtual ~Request() = default;
        virtual void disconnect() = 0;
    };

    virtual ~NativePromise()
    {
        PROMISE_LOG("%s destroying NativePromise:%p]", m_creationSite, this);
        assertIsDead();
#if ASSERT_ENABLED
        Locker lock { m_lock };
        ASSERT(!isNothing());
        ASSERT(m_thenCallbacks.isEmpty());
        ASSERT(m_chainedPromises.isEmpty());
#endif
    }

private:
    template <typename VisibleType>
    class AliasedRefPtr {
    public:
        template <typename RefCountedType>
        AliasedRefPtr(RefCountedType* ptr)
            : m_ptr(ptr)
            , m_ptrDeref([](VisibleType* d) {
                static_cast<RefCountedType*>(d)->deref();
            })
        {
            static_assert(HasRefCountMethods<RefCountedType>::value, "Must be used with a RefCounted object");
            if (ptr)
                ptr->ref();
        }

        AliasedRefPtr(AliasedRefPtr&& other)
            : m_ptr(std::exchange(other.m_ptr, nullptr))
            , m_ptrDeref(other.m_ptrDeref)
        {
        }

        virtual ~AliasedRefPtr()
        {
            if (m_ptr)
                m_ptrDeref(m_ptr);
        }
        AliasedRefPtr& operator=(AliasedRefPtr&& other)
        {
            if (&other == this)
                return *this;
            m_ptr = std::exchange(other.m_ptr, nullptr);
            m_ptrDeref = other.m_ptrDeref;
            return *this;
        }

        VisibleType* operator->() { return m_ptr; }
        VisibleType& operator*() const { return *m_ptr; }
    private:
        VisibleType* m_ptr;
        using PtrDeref = void (*)(VisibleType*);
        const PtrDeref m_ptrDeref;
    };
    using ManagedSerialFunctionDispatcher = AliasedRefPtr<SerialFunctionDispatcher>;

    // Return a |T&&| to enable move when IsExclusive is true or a |const T&| to enforce copy otherwise.
    template<typename T, typename R = std::conditional_t<IsExclusive, T&&, const T&>>
    static R maybeMove(T& aX)
    {
        return static_cast<R>(aX);
    }

public:
    template<typename ResolveValueType_, typename = std::enable_if<!std::is_void_v<ResolveValueT>>>
    static Ref<NativePromise> createAndResolve(ResolveValueType_&& resolveValue, const char* resolveSite)
    {
        auto p = adoptRef(*new NativePromise(resolveSite));
        p->resolve(std::forward<ResolveValueType_>(resolveValue), resolveSite);
        return p;
    }

    template<typename = std::enable_if<std::is_void_v<ResolveValueT>>>
    static Ref<NativePromise> createAndResolve(const char* resolveSite)
    {
        auto p = adoptRef(*new NativePromise(resolveSite));
        p->resolve(resolveSite);
        return p;
    }

    template<typename RejectValueType_>
    static Ref<NativePromise> createAndReject(RejectValueType_&& rejectValue, const char* rejectSite)
    {
        auto p = adoptRef(*new NativePromise(rejectSite));
        p->reject(std::forward<RejectValueType_>(rejectValue), rejectSite);
        return p;
    }

    template<typename ResultType_>
    static Ref<NativePromise> createAndResolveOrReject(ResultType_&& result, const char* site)
    {
        auto p = adoptRef(*new NativePromise(site));
        p->resolveOrReject(std::forward<ResultType_>(result), site);
        return p;
    }

    using AllPromiseType = NativePromise<Vector<ResolveValueType>, RejectValueType, IsExclusive>;
    using AllSettledPromiseType = NativePromise<Vector<Result>, bool, IsExclusive>;

private:
    friend class Producer;
    template<typename ResolveValueType_, typename = std::enable_if<!std::is_void_v<ResolveValueT>>>
    void resolve(ResolveValueType_&& resolveValue, const char* resolveSite)
    {
        static_assert(std::is_convertible_v<ResolveValueType_, ResolveValueT>, "resolve() argument must be implicitly convertible to NativePromise's ResolveValueT");
        Locker lock { m_lock };
#if LOG_DISABLED && RELEASE_LOG_DISABLED
        UNUSED_PARAM(resolveSite);
#endif
        PROMISE_LOG("%s resolving NativePromise (%p created at %s)", resolveSite, this, m_creationSite);
        ASSERT(isNothing());
        m_result = std::forward<ResolveValueType_>(resolveValue);
        dispatchAll(lock);
    }

    template<typename = std::enable_if<std::is_void_v<ResolveValueT>>>
    void resolve(const char* resolveSite)
    {
        Locker lock { m_lock };
#if LOG_DISABLED && RELEASE_LOG_DISABLED
        UNUSED_PARAM(resolveSite);
#endif
        PROMISE_LOG("%s resolving NativePromise (%p created at %s)", resolveSite, this, m_creationSite);
        ASSERT(isNothing());
        m_result = Result { };
        dispatchAll(lock);
    }

    template<typename RejectValueType_>
    void reject(RejectValueType_&& rejectValue, const char* rejectSite)
    {
        static_assert(std::is_convertible_v<RejectValueType_, RejectValueT>, "reject() argument must be implicitly convertible to NativePromise's RejectValueT");
        Locker lock { m_lock };
#if LOG_DISABLED && RELEASE_LOG_DISABLED
        UNUSED_PARAM(rejectSite);
#endif
        PROMISE_LOG("%s rejecting NativePromise (%p created at %s)", rejectSite, this, m_creationSite);
        ASSERT(isNothing());
        m_result = Unexpected<RejectValueT>(std::forward<RejectValueType_>(rejectValue));
        dispatchAll(lock);
    }

    template<typename ResolveOrRejectValue_>
    void resolveOrReject(ResolveOrRejectValue_&& result, const char* site)
    {
        Locker lock { m_lock };
        ASSERT(isNothing());
#if LOG_DISABLED && RELEASE_LOG_DISABLED
        UNUSED_PARAM(site);
#endif
        PROMISE_LOG("%s resolveOrRejecting NativePromise (%p created at %s)", site, this, m_creationSite);
        m_result = std::forward<ResolveOrRejectValue_>(result);
        dispatchAll(lock);
    }

    void setDispatchMode(PromiseDispatchMode dispatchMode, const char* site)
    {
        static_assert(IsExclusive, "setDispatchMode can only be used with exclusive promises");
        Locker lock { m_lock };
#if LOG_DISABLED && RELEASE_LOG_DISABLED
        UNUSED_PARAM(site);
#endif
        PROMISE_LOG("%s runSynchronouslyOnTarget NativePromise (%p created at %s)", site, this, m_creationSite);
        ASSERT(isNothing(), "A Promise must not have been already resolved or rejected to set dispatch state");
        m_dispatchMode = dispatchMode;
    }

    // We can't move the Result object with non-exclusive promise.
    using ResultParam = std::conditional_t<IsExclusive, Result&&, const Result&>;

    class AllPromiseProducer : public ThreadSafeRefCounted<AllPromiseProducer> {
    public:
        explicit AllPromiseProducer(size_t dependentPromisesCount)
            : m_producer(makeUnique<typename AllPromiseType::Producer>(__func__))
            , m_outstandingPromises(dependentPromisesCount)
        {
            ASSERT(dependentPromisesCount);
            m_resolveValues.resize(dependentPromisesCount);
        }

        template<typename ResolveValueType_>
        void resolve(size_t index, ResolveValueType_&& resolveValue)
        {
            if (!m_producer) {
                // Already resolved or rejected.
                return;
            }

            m_resolveValues[index] = std::forward<ResolveValueType_>(resolveValue);
            if (!--m_outstandingPromises) {
                Vector<ResolveValueType> resolveValues;
                resolveValues.reserveInitialCapacity(m_resolveValues.size());
                for (auto&& resolveValue : m_resolveValues)
                    resolveValues.append(WTFMove(resolveValue.value()));

                m_producer->resolve(WTFMove(resolveValues), __func__);
                m_producer = nullptr;
                m_resolveValues.clear();
            }
        }

        template<typename RejectValueType_>
        void reject(RejectValueType_&& rejectValue)
        {
            if (!m_producer) {
                // Already resolved or rejected.
                return;
            }

            m_producer->reject(std::forward<RejectValueType_>(rejectValue), __func__);
            m_producer = nullptr;
            m_resolveValues.clear();
        }

        Ref<AllPromiseType> promise() { return static_cast<Ref<AllPromiseType>>(*m_producer); }

    private:
        Vector<std::optional<ResolveValueType>> m_resolveValues;
        std::unique_ptr<typename AllPromiseType::Producer> m_producer;
        size_t m_outstandingPromises;
    };

    class AllSettledPromiseProducer : public ThreadSafeRefCounted<AllSettledPromiseProducer> {
    public:
        explicit AllSettledPromiseProducer(size_t dependentPromisesCount)
            : m_producer(makeUnique<typename AllSettledPromiseType::Producer>(__func__))
            , m_outstandingPromises(dependentPromisesCount)
        {
            ASSERT(dependentPromisesCount);
            m_results.resize(dependentPromisesCount);
        }

        void settle(size_t index, ResultParam result)
        {
            if (!m_producer) {
                // Already settled.
                return;
            }

            m_results[index].emplace(maybeMove(result));
            if (!--m_outstandingPromises) {
                Vector<Result> results;
                results.reserveInitialCapacity(m_results.size());
                for (auto&& result : m_results)
                    results.append(WTFMove(result.value()));

                m_producer->resolve(WTFMove(results), __func__);
                m_producer = nullptr;
                m_results.clear();
            }
        }

        Ref<AllSettledPromiseType> promise() { return static_cast<Ref<AllSettledPromiseType>>(*m_producer); }

    private:
        Vector<std::optional<Result>> m_results;
        std::unique_ptr<typename AllSettledPromiseType::Producer> m_producer;
        size_t m_outstandingPromises;
    };

public:
    template <class Dispatcher>
    static Ref<AllPromiseType> all(Dispatcher& targetQueue, Vector<Ref<NativePromise>>& promises)
    {
        static_assert(LooksLikeRCSerialDispatcher<typename RemoveSmartPointer<Dispatcher>::type>::value, "Must be used with a RefCounted SerialFunctionDispatcher");
        if (promises.isEmpty())
            return AllPromiseType::createAndResolve(Vector<ResolveValueType>(), __func__);

        auto producer = adoptRef(new AllPromiseProducer(promises.size()));
        auto promise = producer->promise();
        for (size_t i = 0; i < promises.size(); ++i) {
            promises[i]->whenSettled(targetQueue, __func__, [producer, i] (ResultParam result) {
                if (result)
                    producer->resolve(i, maybeMove(result.value()));
                else
                    producer->reject(maybeMove(result.error()));
            });
        }
        return promise;
    }

    template <class Dispatcher>
    static Ref<AllSettledPromiseType> allSettled(Dispatcher& targetQueue, Vector<Ref<NativePromise>>& promises)
    {
        static_assert(LooksLikeRCSerialDispatcher<typename RemoveSmartPointer<Dispatcher>::type>::value, "Must be used with a RefCounted SerialFunctionDispatcher");
        if (promises.isEmpty())
            return AllSettledPromiseType::createAndResolve(Vector<Result>(), __func__);

        auto producer = adoptRef(new AllSettledPromiseProducer(promises.size()));
        auto promise = producer->promise();
        for (size_t i = 0; i < promises.size(); ++i) {
            promises[i]->whenSettled(targetQueue, __func__, [producer, i] (ResultParam result) {
                producer->settle(i, maybeMove(result));
            });
        }
        return promise;
    }

private:
    explicit NativePromise(const char* creationSite)
        : m_creationSite(creationSite)
    {
        PROMISE_LOG("%s creating NativePromise:%p", m_creationSite, this);
    }

    class ThenCallbackBase : public Request {

    public:
        ThenCallbackBase(ManagedSerialFunctionDispatcher&& targetQueue, const char* callSite)
            : m_targetQueue(WTFMove(targetQueue))
            , m_callSite(callSite)
        {
        }

        void assertIsDead()
        {
            // Ensure that there are no pending (that is either not disconnected or the completion promise itself is pending)
#if ASSERT_ENABLED
            if (auto p = completionPromise())
                p->assertIsDead();
            else
                ASSERT(m_disconnected);
#endif
        }

        void dispatch(NativePromise& promise, Locker<Lock>& lock)
        {
            assertIsHeld(promise.m_lock);

            ASSERT(!promise.isNothing());

            if (UNLIKELY(promise.m_dispatchMode == PromiseDispatchMode::RunSynchronouslyOnTarget) && m_targetQueue->isCurrent()) {
                PROMISE_LOG("%s synchronous then() call made from %s [promise:%p, callback:%p]", promise.m_result ? "Resolving" : "Rejecting", m_callSite, &promise, this);
                if (m_disconnected) {
                    PROMISE_LOG("ThenCallback disconnected aborting [this:%p, callSite:%s]", this, m_callSite);
                    return;
                }
                {
                    // Holding the lock is unnecessary while running the resolve/reject callback and we don't want to hold the lock for too long.
                    DropLockForScope unlocker(lock);
                    processResult(promise.result());
                }
                return;
            }
            m_targetQueue->dispatch([this, strongThis = Ref { *this }, promise = Ref { promise }, operation = *promise.m_result ? "Resolving" : "Rejecting"] () mutable {
                PROMISE_LOG("%s then() call made from %s [promise:%p, callback:%p]", operation, m_callSite, &promise, this);
                if (m_disconnected) {
                    PROMISE_LOG("ThenCallback disconnected aborting [this:%p, callSite:%s]", this, m_callSite);
                    return;
                }
                processResult(promise->result());
            });
        }

        void disconnect() override
        {
            assertIsCurrent(*m_targetQueue);

            m_disconnected = true;
        }

    protected:
        virtual void processResult(Result&) = 0;
        ManagedSerialFunctionDispatcher m_targetQueue;
        const char* m_callSite;

#if ASSERT_ENABLED
        virtual RefPtr<NativePromiseBase> completionPromise() = 0;
        // In a debug build, m_disconnected is checked in the destructor
        // Otherwise it is only ever modified and read on the target queue.
        std::atomic<bool> m_disconnected { false };
#else
        bool m_disconnected { false };
#endif
    };

    template<bool IsChaining, typename ReturnPromiseType_>
    class ThenCallback : public ThenCallbackBase {
    public:
        static constexpr bool SupportChaining = IsChaining;
        // We could have the method return void if SupportChaining is false, it would however make it difficult to identify usage errors.
        // Returning a NativePromise by default allows to have a more user friendly static_assert instead.
        using ReturnPromiseType = std::conditional_t<IsChaining, ReturnPromiseType_, NativePromise>;
        using CallBackType = std::conditional_t<IsChaining, Function<Ref<ReturnPromiseType_>(ResultParam)>, Function<void(ResultParam)>>;

        ThenCallback(ManagedSerialFunctionDispatcher&& targetQueue, CallBackType&& function, const char* callSite)
            : ThenCallbackBase(WTFMove(targetQueue), callSite)
            , m_resolveOrRejectFunction(WTFMove(function))
        {
        }

        void disconnect() override
        {
            assertIsCurrent(*ThenCallbackBase::m_targetQueue);
            ThenCallbackBase::disconnect();
            m_resolveOrRejectFunction = nullptr;
        }

        void processResult(Result& result) override
        {
            assertIsCurrent(*ThenCallbackBase::m_targetQueue);
            ASSERT(m_resolveOrRejectFunction);
            if constexpr (IsChaining) {
                auto p = m_resolveOrRejectFunction(maybeMove(result));
                std::unique_ptr<typename ReturnPromiseType::Producer> completionProducer;
                {
                    Locker lock { m_lock };
                    completionProducer = std::exchange(m_completionProducer, { });
                }
                if (completionProducer)
                    p->chainTo(WTFMove(*completionProducer), "<chained completion promise>");
            } else
                m_resolveOrRejectFunction(maybeMove(result));

            m_resolveOrRejectFunction = nullptr;
        }

        void setCompletionPromise(std::unique_ptr<typename ReturnPromiseType::Producer>&& completionProducer)
        {
            if constexpr (IsChaining) {
                Locker lock { m_lock };
                m_completionProducer = WTFMove(completionProducer);
            }
        }

#if ASSERT_ENABLED
        RefPtr<NativePromiseBase> completionPromise() override
        {
            if constexpr (IsChaining) {
                Locker lock { m_lock };
                return m_completionProducer ? static_cast<Ref<ReturnPromiseType>>(*m_completionProducer).ptr() : nullptr;
            }
            return nullptr;
        }
#endif

        NO_UNIQUE_ADDRESS std::conditional_t<IsChaining, Lock, std::monostate> m_lock;
        NO_UNIQUE_ADDRESS std::conditional_t<IsChaining, std::unique_ptr<typename ReturnPromiseType::Producer>, std::monostate> m_completionProducer WTF_GUARDED_BY_LOCK(m_lock);
    private:
        CallBackType m_resolveOrRejectFunction WTF_GUARDED_BY_CAPABILITY(*ThenCallbackBase::m_targetQueue);
    };

    void thenInternal(Ref<ThenCallbackBase>&& thenCallback, const char* callSite)
    {
        Locker lock { m_lock };
        ASSERT(!IsExclusive || !m_haveRequest, "Using an exclusive promise in a non-exclusive fashion");
        m_haveRequest = true;
#if LOG_DISABLED && RELEASE_LOG_DISABLED
        UNUSED_PARAM(callSite);
#endif
        PROMISE_LOG("%s invoking then() [this:%p, callback:%p, isNothing:%d]", callSite, this, thenCallback.ptr(), isNothing());
        if (!isNothing())
            thenCallback->dispatch(*this, lock);
        else
            m_thenCallbacks.append(WTFMove(thenCallback));
    }

    template<typename ThenCallbackType>
    class ThenCommand : public ConvertibleToNativePromise {
        // Allow Promise::then() to access the private constructor,
        template<typename, typename, bool>
        friend class NativePromise;

        // used by IsConvertibleToNativePromise to determine how to cast the result.
        using PromiseType = typename ThenCallbackType::ReturnPromiseType;

        ThenCommand(NativePromise& promise, Ref<ThenCallbackType>&& thenCallback, const char* callSite)
            : m_promise(promise)
            , m_thenCallback(WTFMove(thenCallback))
            , m_callSite(callSite)
        {
        }

        ThenCommand(ThenCommand&& other) = default;
        ThenCommand& operator=(ThenCommand&& other) = default;

    public:
        ~ThenCommand()
        {
            // Issue the request now if the return value of then()/whenSettled() is not used.
            if (m_thenCallback)
                m_promise->thenInternal(m_thenCallback.releaseNonNull(), m_callSite);
        }

        // Allow calling ->then()/whenSettled() again for more promise chaining or ->track() to
        // end chaining and track the request for future disconnection.
        // Defined -> operator for consistency in calling pattern.
        ThenCommand* operator->() { return this; }

        // Allow conversion from ThenCommand to Ref<NativePromise> like:
        // Ref<NativePromise> p = somePromise->then(...);
        // p->then(thread1, ...);
        // p->then(thread2, ...);
        operator Ref<PromiseType>()
        {
            static_assert(ThenCallbackType::SupportChaining, "The resolve/reject callback needs to return a Ref<NativePromise> in order to do promise chaining.");
            ASSERT(m_thenCallback, "Conversion can only be done once");
            // We create a completion promise producer which will be resolved or rejected when the ThenCallback will be run
            // with the value returned by the callbacks provided to then().
            auto producer = makeUnique<typename PromiseType::Producer>("<completion promise>");
            auto promise = static_cast<Ref<PromiseType>>(*producer);
            m_thenCallback->setCompletionPromise(WTFMove(producer));
            m_promise->thenInternal(m_thenCallback.releaseNonNull(), m_callSite);
            return promise;
        }

        // Allow calling then() again by converting the ThenCommand to Ref<NativePromise>
        template<typename... Ts>
        auto then(Ts&&... args)
        {
            return static_cast<Ref<PromiseType>>(*this)->then(std::forward<Ts>(args)...);
        }

        template<typename... Ts>
        auto whenSettled(Ts&&... args)
        {
            return static_cast<Ref<PromiseType>>(*this)->whenSettled(std::forward<Ts>(args)...);
        }

        void track(NativePromiseRequest<NativePromise>& requestHolder)
        {
            ASSERT(m_thenCallback, "Can only track a request once");
            requestHolder.track(*m_thenCallback);
            m_promise->thenInternal(m_thenCallback.releaseNonNull(), m_callSite);
        }

    private:
        Ref<NativePromise> m_promise;
        RefPtr<ThenCallbackType> m_thenCallback;
        const char* m_callSite;
    };

    template<typename F>
    struct MethodTraitsHelper : MethodTraitsHelper<decltype(&F::operator())> { };
    template<typename ThisType, typename Ret, typename... ArgTypes>
    struct MethodTraitsHelper<Ret (ThisType::*)(ArgTypes...)> {
        using returnType = Ret;
        static const size_t argSize = sizeof...(ArgTypes);
    };
    template<typename ThisType, typename Ret, typename... ArgTypes>
    struct MethodTraitsHelper<Ret (ThisType::*)(ArgTypes...) const> {
        using returnType = Ret;
        static const size_t argSize = sizeof...(ArgTypes);
    };
    template<typename T>
    struct MethodTrait : MethodTraitsHelper<std::remove_reference_t<T>> { };

    template<typename MethodType>
    using TakesArgument = std::bool_constant<MethodTrait<MethodType>::argSize>;

    struct LambdaReturnTrait {
        template <typename T, typename = std::enable_if_t<IsConvertibleToNativePromise<T>>>
        Ref<typename T::PromiseType> lambda();

        template <typename T, typename = std::enable_if_t<std::is_void_v<T>>>
        void lambda();

        template <typename T, typename = std::enable_if_t<IsConvertibleToNativePromise<T>>>
        typename T::PromiseType type();

        template <typename T, typename = std::enable_if_t<std::is_void_v<T>>>
        void type();
    };

public:
    template<class DispatcherType, typename ResolveRejectFunction>
    auto whenSettled(DispatcherType& targetQueue, const char* callSite, ResolveRejectFunction&& resolveRejectFunction)
    {
        using DispatcherRealType = typename RemoveSmartPointer<DispatcherType>::type;
        static_assert(LooksLikeRCSerialDispatcher<DispatcherRealType>::value, "Must be used with a RefCounted SerialFunctionDispatcher");

        using R1 = typename RemoveSmartPointer<typename MethodTrait<ResolveRejectFunction>::returnType>::type;
        using IsChaining = std::bool_constant<IsConvertibleToNativePromise<R1>>;
        static_assert(IsConvertibleToNativePromise<R1> || std::is_void_v<R1>, "ResolveOrReject method must return a promise or nothing");
        using LambdaReturnType = decltype(std::declval<LambdaReturnTrait>().template lambda<R1>());

        auto lambda = [resolveRejectFunction = WTFMove(resolveRejectFunction)] (ResultParam result) mutable -> LambdaReturnType {
            if constexpr (TakesArgument<ResolveRejectFunction>::value)
                return resolveRejectFunction(maybeMove(result));
            else
                return resolveRejectFunction();
        };

        ManagedSerialFunctionDispatcher dispatcher { &static_cast<DispatcherRealType&>(targetQueue) };
        using ThenCallbackType = ThenCallback<IsChaining::value, decltype(std::declval<LambdaReturnTrait>().template type<R1>())>;
        using ReturnType = ThenCommand<ThenCallbackType>;

        auto thenCallback = adoptRef(*new ThenCallbackType(WTFMove(dispatcher), WTFMove(lambda), callSite));
        return ReturnType(*this, WTFMove(thenCallback), callSite);
    }

    template<class DispatcherType, typename ThisType, typename ResolveOrRejectMethod>
    auto whenSettled(DispatcherType& targetQueue, const char* callSite, ThisType& thisVal, ResolveOrRejectMethod resolveOrRejectMethod)
    {
        static_assert(HasRefCountMethods<ThisType>::value, "ThisType must be refounted object");
        using R1 = typename RemoveSmartPointer<typename MethodTrait<ResolveOrRejectMethod>::returnType>::type;
        static_assert(IsConvertibleToNativePromise<R1> || std::is_void_v<R1>, "ResolveOrReject method must return a promise or nothing");
        using LambdaReturnType = decltype(std::declval<LambdaReturnTrait>().template lambda<R1>());

        return whenSettled(targetQueue, callSite, [thisVal = Ref { thisVal }, resolveOrRejectMethod] (ResultParam result) mutable -> LambdaReturnType {
            if constexpr (TakesArgument<ResolveOrRejectMethod>::value)
                return (thisVal.ptr()->*resolveOrRejectMethod)(maybeMove(result));
            else
                return (thisVal.ptr()->*resolveOrRejectMethod)();
        });
    }

    template<class DispatcherType, typename ResolveFunction, typename RejectFunction>
    auto then(DispatcherType& targetQueue, const char* callSite, ResolveFunction&& resolveFunction, RejectFunction&& rejectFunction)
    {
        using DispatcherRealType = typename RemoveSmartPointer<DispatcherType>::type;
        static_assert(LooksLikeRCSerialDispatcher<DispatcherRealType>::value, "Must be used with a RefCounted SerialFunctionDispatcher");

        using R1 = typename RemoveSmartPointer<typename MethodTrait<ResolveFunction>::returnType>::type;
        using R2 = typename RemoveSmartPointer<typename MethodTrait<RejectFunction>::returnType>::type;
        using IsChaining = std::bool_constant<RelatedNativePromise<R1, R2>>;
        static_assert(IsChaining::value || (std::is_void_v<R1> && std::is_void_v<R2>), "resolve/reject methods must return a promise of the same type or nothing");
        using LambdaReturnType = decltype(std::declval<LambdaReturnTrait>().template lambda<R1>());

        return whenSettled(targetQueue, callSite, [resolveFunction = WTFMove(resolveFunction), rejectFunction = WTFMove(rejectFunction)] (ResultParam result) -> LambdaReturnType {
            if (result) {
                if constexpr (TakesArgument<ResolveFunction>::value)
                    return resolveFunction(maybeMove(result.value()));
                else
                    return resolveFunction();
            } else {
                if constexpr (TakesArgument<RejectFunction>::value)
                    return rejectFunction(maybeMove(result.error()));
                else
                    return rejectFunction();
            }
        });
    }

    template<class DispatcherType, typename ThisType, typename ResolveMethod, typename RejectMethod>
    auto then(DispatcherType& targetQueue, const char* callSite, ThisType& thisVal, ResolveMethod resolveMethod, RejectMethod rejectMethod)
    {
        static_assert(HasRefCountMethods<ThisType>::value, "ThisType must be refounted object");
        using R1 = typename RemoveSmartPointer<typename MethodTrait<ResolveMethod>::returnType>::type;
        using R2 = typename RemoveSmartPointer<typename MethodTrait<RejectMethod>::returnType>::type;
        using IsChaining = std::bool_constant<RelatedNativePromise<R1, R2>>;
        static_assert(IsChaining::value || (std::is_void_v<R1> && std::is_void_v<R2>), "resolve/reject methods must return a promise of the same type or nothing");
        using LambdaReturnType = decltype(std::declval<LambdaReturnTrait>().template lambda<R1>());

        return whenSettled(targetQueue, callSite, [thisVal = Ref { thisVal }, resolveMethod, rejectMethod] (ResultParam result) mutable -> LambdaReturnType {
            if (result.has_value()) {
                if constexpr (TakesArgument<ResolveMethod>::value)
                    return (thisVal.ptr()->*resolveMethod)(maybeMove(result.value()));
                else
                    return (thisVal.ptr()->*resolveMethod)();
            } else {
                if constexpr (TakesArgument<RejectMethod>::value)
                    return (thisVal.ptr()->*rejectMethod)(maybeMove(result.error()));
                else
                    return (thisVal.ptr()->*rejectMethod)();
            }
        });
    }

    void chainTo(NativePromise::Producer&& chainedPromise, const char* callSite)
    {
        Locker lock { m_lock };
        ASSERT(!IsExclusive || !m_haveRequest, "Using an exclusive promise in a non-exclusive fashion");
        m_haveRequest = true;
        PROMISE_LOG("%s invoking chainTo() [this:%p, chainedPromise:%p, isNothing:%d]", callSite, this, static_cast<Ref<NativePromise>>(chainedPromise).ptr(), isNothing());

        if constexpr (IsExclusive)
            chainedPromise.setDispatchMode(m_dispatchMode, callSite);

        if (isNothing())
            m_chainedPromises.append(WTFMove(chainedPromise));
        else
            resolveOrReject(WTFMove(chainedPromise));
    }

    void assertIsDead() final
    {
        Locker lock { m_lock };
        for (auto&& thenCallback : m_thenCallbacks)
            thenCallback->assertIsDead();
        for (auto&& chained : m_chainedPromises)
            chained.assertIsDead();
    }

    bool isResolved() const
    {
        Locker lock { m_lock };
        return m_result && m_result->has_value();
    }

    bool isResolvedOrRejected() const
    {
        Locker lock { m_lock };
        return !isNothing();
    }

private:
    bool isNothing() const
    {
        assertIsHeld(m_lock);
        return !m_result;
    }

    Result& result()
    {
        // Only called by ResolveOrRejectFunction on the target's queue once all operations are complete and settled.
        // So we don't really need to hold the lock to access the value.
        Locker lock { m_lock };
        ASSERT(!isNothing());
        return *m_result;
    }

    void dispatchAll(Locker<Lock>& lock)
    {
        assertIsHeld(m_lock);
        // We move m_thenCallbacks and m_chainedPromises while holding the lock
        // As dispatch() may release the lock when in synchronous run mode.
        auto thenCallbacks = std::exchange(m_thenCallbacks, { });
        auto chainedPromises = std::exchange(m_chainedPromises, { });
        for (auto& thenCallback : thenCallbacks)
            thenCallback->dispatch(*this, lock);

        for (auto&& chainedPromise : chainedPromises)
            resolveOrReject(WTFMove(chainedPromise));
    }

    void resolveOrReject(typename NativePromise::Producer&& other)
    {
        assertIsHeld(m_lock);
        ASSERT(!isNothing());
        if (m_result->has_value()) {
            if constexpr(std::is_void_v<ResolveValueT>)
                other.resolve("<chained promise>");
            else
                other.resolve(maybeMove(m_result->value()), "<chained promise>");
        } else
            other.reject(maybeMove(m_result->error()), "<chained promise>");
        other = { };
    }

    const char* m_creationSite; // For logging
    mutable Lock m_lock;
    std::optional<Result> m_result WTF_GUARDED_BY_LOCK(m_lock); // Set on any threads when the promise is resolved, only read on the promise's target queue.
    // Experiments show that we never have more than 3 elements when IsExclusive is false.
    // So '3' is a good value to avoid heap allocation in most cases.
    Vector<Ref<ThenCallbackBase>, IsExclusive ? 1 : 3> m_thenCallbacks WTF_GUARDED_BY_LOCK(m_lock);
    Vector<typename NativePromise::Producer> m_chainedPromises WTF_GUARDED_BY_LOCK(m_lock);
    bool m_haveRequest WTF_GUARDED_BY_LOCK(m_lock) { false };
    std::atomic<PromiseDispatchMode> m_dispatchMode { PromiseDispatchMode::Default };
};

template<typename ResolveValueT, typename RejectValueT, bool IsExclusive>
class NativePromise<ResolveValueT, RejectValueT, IsExclusive>::Producer final : public ConvertibleToNativePromise {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // used by IsConvertibleToNativePromise to determine how to cast the result.
    using PromiseType = NativePromise<ResolveValueT, RejectValueT, IsExclusive>;

    explicit Producer(const char* creationSite, PromiseDispatchMode dispatchMode = PromiseDispatchMode::Default)
        : m_promise(adoptRef(new PromiseType(creationSite)))
        , m_creationSite(creationSite)
    {
        if constexpr (IsExclusive)
            m_promise->setDispatchMode(dispatchMode, creationSite);
    }

    Producer(Producer&& other) = default;
    Producer& operator=(Producer&& other) = default;

    ~Producer()
    {
        assertIsDead();
    }

    explicit operator bool() const { return m_promise && m_promise->isResolvedOrRejected(); }
    bool isNothing() const
    {
        ASSERT(m_promise, "used after moved");
        return m_promise && !m_promise->isResolvedOrRejected();
    }

    template<typename ResolveValueType_, typename = std::enable_if<!std::is_void_v<ResolveValueT>>>
    void resolve(ResolveValueType_&& resolveValue, const char* resolveSite) const
    {
        ASSERT(isNothing());
        if (!isNothing()) {
            PROMISE_LOG("%s ignored already resolved or rejected NativePromise (%p created at %s)", resolveSite, m_promise.get(), m_creationSite);
            return;
        }
        m_promise->resolve(std::forward<ResolveValueType_>(resolveValue), resolveSite);
    }

    template<typename = std::enable_if<std::is_void_v<ResolveValueT>>>
    void resolve(const char* resolveSite) const
    {
        ASSERT(isNothing());
        if (!isNothing()) {
            PROMISE_LOG("%s ignored already resolved or rejected NativePromise (%p created at %s)", resolveSite,  m_promise.get(), m_creationSite);
            return;
        }
        m_promise->resolve(resolveSite);
    }

    template<typename RejectValueType_>
    void reject(RejectValueType_&& rejectValue, const char* rejectSite) const
    {
        ASSERT(isNothing());
        if (!isNothing()) {
            PROMISE_LOG("%s ignored already resolved or rejected NativePromise (%p created at %s)", rejectSite,  m_promise.get(), m_creationSite);
            return;
        }
        m_promise->reject(std::forward<RejectValueType_>(rejectValue), rejectSite);
    }

    template<typename ResolveOrRejectValue_>
    void resolveOrReject(ResolveOrRejectValue_&& result, const char* site) const
    {
        ASSERT(isNothing());
        if (!isNothing()) {
            PROMISE_LOG("%s ignored already resolved or rejected NativePromise (%p created at %s)", site, this, m_creationSite);
            return;
        }
        m_promise->resolveOrReject(std::forward<ResolveOrRejectValue_>(result), site);
    }

    operator Ref<PromiseType>() const
    {
        ASSERT(m_promise, "used after move");
        return *m_promise;
    }

    // Allow calling ->then()/whenSettled() again for more promise chaining.
    // Defined -> operator for consistency in calling pattern.
    Producer* operator->() { return this; }

    // Allow calling then() again by converting the ThenCommand to Ref<NativePromise>
    template<typename... Ts>
    auto then(Ts&&... args) const
    {
        return static_cast<Ref<PromiseType>>(*this)->then(std::forward<Ts>(args)...);
    }

    template<typename... Ts>
    auto whenSettled(Ts&&... args) const
    {
        return static_cast<Ref<PromiseType>>(*this)->whenSettled(std::forward<Ts>(args)...);
    }

    void chainTo(Producer&& chainedPromise, const char* callSite)
    {
        return static_cast<Ref<PromiseType>>(*this)->chainTo(WTFMove(chainedPromise), callSite);
    }

private:
    void setDispatchMode(PromiseDispatchMode dispatchMode, const char* callSite) const
    {
        ASSERT(m_promise, "used after move");
        m_promise->setDispatchMode(dispatchMode, callSite);
    }

friend PromiseType;
    Producer() = default;

    void assertIsDead() const
    {
        if (m_promise)
            m_promise->assertIsDead();
    }

    RefPtr<PromiseType> m_promise;
    const char* m_creationSite;
};

// A generic promise type that does the trick for simple use cases.
using GenericPromise = NativePromise<void, int, /* IsExclusive = */ true>;

// A generic, non-exclusive promise type that does the trick for simple use cases.
using GenericNonExclusivePromise = NativePromise<void, int, /* IsExclusive = */ false>;

template<typename PromiseType>
class NativePromiseRequest final {
public:
    NativePromiseRequest() = default;
    NativePromiseRequest(NativePromiseRequest&& other) = default;
    NativePromiseRequest& operator=(NativePromiseRequest&& other) = default;
    ~NativePromiseRequest() { ASSERT(!m_request); }

    void track(Ref<typename PromiseType::Request> request)
    {
        ASSERT(!m_request);
        m_request = WTFMove(request);
    }

    void complete()
    {
        ASSERT(m_request);
        m_request = nullptr;
    }

    // Disconnect and forget an outstanding promise. The resolve/reject methods will never be called.
    void disconnect()
    {
        ASSERT(m_request);
        if (!m_request)
            return;
        m_request->disconnect();
        m_request = nullptr;

    }

private:
    RefPtr<typename PromiseType::Request> m_request;
};

// Invoke a function object (e.g., lambda) asynchronously.
// Returns a promise that the function should eventually resolve or reject once the original promise returned by the lambda
// is itself resolved or rejected.
template<typename Function>
static auto invokeAsync(SerialFunctionDispatcher& targetQueue, const char* callerName, Function&& function)
{
    static_assert(!std::is_lvalue_reference_v<Function>, "Function object must not be passed by lvalue-ref (to avoid unplanned copies); WTFMove() the object.");
    static_assert(IsConvertibleToNativePromise<typename RemoveSmartPointer<decltype(function())>::type>, "Function object must return Ref<NativePromise>");
    using ReturnType = typename RemoveSmartPointer<decltype(function())>::type;

    typename ReturnType::PromiseType::Producer proxyPromiseProducer(callerName);
    auto promise = static_cast<Ref<ReturnType>>(proxyPromiseProducer);
    targetQueue.dispatch([producer = WTFMove(proxyPromiseProducer), function = WTFMove(function)] () mutable {
        Ref<typename ReturnType::PromiseType> p = function();
        p->chainTo(WTFMove(producer), "invokeAsync proxy");
    });
    return promise;
}

} // namespace WTF

using WTF::invokeAsync;
using WTF::NativePromise;
