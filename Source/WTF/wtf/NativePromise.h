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
#include <functional>
#include <type_traits>
#include <utility>
#include <wtf/Assertions.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/Expected.h>
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/FunctionDispatcher.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/Logger.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TypeTraits.h>
#include <wtf/Unexpected.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WTF {

/*
 * A promise manages an asynchronous request that may or may not be able to be fulfilled immediately.
 * When an API returns a promise, the consumer may attach callbacks to be invoked (asynchronously, on a specified thread)
 * when the request is either completed (resolved) or cannot be completed (rejected).
 *
 * A NativePromise object is thread safe, and may be ->then()/whenSettle()ed on any threads.
 * The then() call accepts either a resolve and reject callback, while whenSettled() accepts a resolveOrReject one.
 *
 * NativePromise::then() and NativePromise::whenSettled() returns a NativePromise::ThenCommand object. This object can be either:
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
 * By default, a NativePromise will use crossThreadCopy() on the resolved or rejected object if it contains a type with the `isolatedCopy()` method
 * or an AtomString (be it directly, or in a composited object (e.g. Vector<AtomString>).
 * This behaviour can be overridden with either PromiseOption::WithCrossThreadCopy or PromiseOption::WithoutCrossThreadCopy
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
 *  - Allocate a NativePromise::Producer and return it to the consumer has a Ref<NativePromise>
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
 * The object given to resolve, reject or settle must have a CrossThreadCopier specialisation as needed.
 * The type of this object may not be identical to ResolveValueType or RejectValueType as the methods allow for implicit conversion.
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
 *            return GenericPromise::createAndResolve();
 *        else
 *            return GenericPromise::createAndReject(-1);
 *    }
 *
 *    static void methodA()
 *    {
 *        assertIsMainThread();
 *        auto arg = std::make_unique<int>(20);
 *        // invokeAsync returns a promise of same type as what the function returns,
 *        // and it will be resolved or rejected when the original promise is settled.
 *        invokeAsync(workQueue, [arg = WTFMove(arg)] () mutable { return methodB(WTFMove(arg); })
 *        ->then(RunLoop::main(),
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
 *    p->then(RunLoop::main(), [] (NativePromise::Result&& result) {
 *        assertIsMainThread();
 *        if (result) {
 *            auto resolveValue = WTFMove(result.value());
 *        } else {
 *            auto rejectValue = WTFMove(result.error());
 *        }
 *    }
 *
 * 3. Using a NativePromiseRequest
 *    NativePromiseRequest request;
 *
 *    GenericPromise::Producer p;
 *    // Note that if you're not interested in the result you can provide a Function<void()>
 *    p->then(RunLoop::main(),
 *            [] { CRASH("resolve callback won't be run"); },
 *            [] { CRASH("reject callback won't be run"); })
 *      ->track(request);
 *
 *    // We resolve the promise.
 *    p.resolve();
 *
 *    // We are no longer interested by the result of the promise. We disconnect the request holder.
 *    request.disconnect();
 *
 * 4. Chaining promises of different types
 *    auto p = MyAsyncMethod(); // MyAsyncMethod returns a Ref<MyNativePromise>, and perform some work on some thread.
 *    auto p2 = p->then(RunLoop::main(), [] (MyNativePromise::ResolveValueType val) {
 *            assertIsMainThread();
 *            if (val)
 *                return MyOtherPromise::createAndResolve(val);
 *            return MyOtherPromise::createAndReject(val);
 *        }, [] (MyOtherPromise::RejectValueType val) {
 *            return MyOtherPromise::createAndReject(val);
 *        }) // The type returned by then() is of the last PromiseType returned in the chain.
 *        ->whenSettled(RunLoop::main(), [] (const MyOtherPromise::Result&) -> void {
 *            // do something else
 *        });
 *
 * Another Example:
 * Consider a PhotoProducer class that can take a photo and returns an image and its mimetype.
 * The PhotoProducer uses some system framework that takes a completion handler which will receive the photo once taken.
 * The PhotoProducer uses its own WorkQueue to perform the work so that it won't block the thread it's called on.
 * We want the PhotoProducer to be able to be called on any threads.
 *
 * // This would be the system framework.
 * struct AVCaptureMethod {
 *    // Note that we must use Function as std::function requires the lambda to be copyable.
 *    static void captureImage(Function<void(std::vector<uint8_t>&&, std::string&&)>&& handler)
 *    {
 *        handler({ 1, 2, 3, 4, 5 }, "image/jpeg");
 *    }
 * };
 *
 * struct PhotoSettings { };
 *
 * class PhotoProducer : public ThreadSafeRefCounted<PhotoProducer> {
 * public:
 *    using PhotoPromise = NativePromise<std::pair<Vector<uint8_t>, String>, int>;
 *    static Ref<PhotoProducer> create(const PhotoSettings& settings) { return adoptRef(*new PhotoProducer(settings)); }
 *
 *    Ref<PhotoPromise> takePhoto() const
 *    {
 *        // This can be called on any threads.
 *        // It uses invokeAsync which returns a NativePromise that will be settled when the promise returned by the method will itself be settled.
 *        // (the invokeAsync promise is "chained" to the promise returned by `takePhotoImpl()`)
 *        return invokeAsync(m_generatePhotoQueue, [protectedThis = Ref { *this }] {
 *            assertIsCurrent(protectedThis->m_generatePhotoQueue);
 *            return protectedThis->takePhotoImpl();
 *        });
 *    }
 * private:
 *    explicit PhotoProducer(const PhotoSettings& settings)
 *        : m_generatePhotoQueue(WorkQueue::create("takePhoto queue"))
 *    {
 *    }
 *
 *    Ref<PhotoPromise> takePhotoImpl() const
 *    {
 *        PhotoPromise::Producer producer;
 *        Ref<PhotoPromise> promise = producer;
 *
 *        AVCaptureMethod::captureImage([producer = WTFMove(producer)] (std::vector<uint8_t>&& image, std::string&& mimeType) {
 *            // Note that you can resolve a NativePromise on any threads. Unlike with a CompletionHandler it is not the responsibility of the producer
 *            // to resolve the promise on a particular thread.
 *            // The consumer specifies the thread on which it wants to be called back.
 *            producer.resolve(std::make_pair<Vector<uint8_t>, String>({ image.data(), image.size() }, { mimeType.data(), static_cast<unsigned>(mimeType.size()) }));
 *        });
 *
 *        // Return the promise which the producer will resolve at a later stage.
 *        return promise;
 *    }
 *    Ref<WorkQueue> m_generatePhotoQueue;
 * };
 *
 * And usage would be:
 *  auto photoProducer = PhotoProducer::create(PhotoSettings { });
 *  photoProducer->takePhoto()->whenSettled(RunLoop::main(), [] (PhotoProducer::PhotoPromise::Result&& result) mutable {
 *      static_assert(std::is_same_v<decltype(result.value()), std::pair<Vector<uint8_t>, String>&>);
 *      if (result)
 *          EXPECT_EQ(result.value().second, "image/jpeg"_s);
 *      else
 *          EXPECT_TRUE(false); // Got an unexpected error.
 *  });
 *
 * For additional examples on how to use NativePromise, refer to NativePromise.cpp API tests.
 */

class NativePromiseBase : public ThreadSafeRefCounted<NativePromiseBase>  {
public:
    virtual void assertIsDead() = 0;
    virtual ~NativePromiseBase() = default;
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    WTF_EXPORT_PRIVATE static WTFLogChannel& logChannel();
#endif
    template<typename... Args>
    static inline void log(UNUSED_VARIADIC_PARAMS const Args&... arguments)
    {
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
        auto& channel = logChannel();
        if (channel.state == WTFLogChannelState::Off || WTFLogLevel::Debug > channel.level)
            return;

        Logger::log(channel, WTFLogLevel::Debug,  arguments...);
#endif
    }
};

#define PROMISE_LOG(...) NativePromiseBase::log(__VA_ARGS__)

// Ideally we would use C++20 source_location, but it's currently broken in XCode see rdar://116228776
#define DEFAULT_LOGSITEIDENTIFIER Logger::LogSiteIdentifier(__builtin_FUNCTION(), nullptr)

class ConvertibleToNativePromise { };

class NativePromiseRequest :  public CanMakeWeakPtr<NativePromiseRequest> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    NativePromiseRequest() = default;
    NativePromiseRequest(NativePromiseRequest&& other) = default;
    NativePromiseRequest& operator=(NativePromiseRequest&& other) = default;
    ~NativePromiseRequest()
    {
        ASSERT(!m_callback, "complete() or disconnect() wasn't called");
    }

    class Callback : public ThreadSafeRefCounted<Callback> {
    public:
        virtual ~Callback() = default;
        virtual void disconnect() = 0;
    };


    void track(Ref<Callback> callback)
    {
        ASSERT(!m_callback);
        m_callback = WTFMove(callback);
    }

    explicit operator bool() const { return !!m_callback; }

    void complete()
    {
        ASSERT(m_callback);
        m_callback = nullptr;
    }

    // Disconnect and forget an outstanding promise. The resolve/reject methods will never be called.
    void disconnect()
    {
        ASSERT(m_callback);
        if (!m_callback)
            return;
        std::exchange(m_callback, nullptr)->disconnect();
    }

private:
    RefPtr<Callback> m_callback;
};

template<typename ResolveValueT, typename RejectValueT, unsigned options = 0>
class NativePromiseProducer;

enum class PromiseDispatchMode : uint8_t {
    Default, // ResolveRejectCallbacks will be dispatched on the target thread.
    RunSynchronouslyOnTarget, // ResolveRejectCallbacks will be run synchronously if target thread is current.
};

enum class PromiseOption : uint8_t {
    Default = 0, // Exclusive | WithAutomaticCrossThreadCopy
    NonExclusive = (1 << 0),
    WithCrossThreadCopy = (1 << 2),
    WithoutCrossThreadCopy = (1 << 3),
};
constexpr unsigned operator|(PromiseOption a, PromiseOption b)
{
    return static_cast<unsigned>(a) | static_cast<unsigned>(b);
}
constexpr unsigned operator|(unsigned a, PromiseOption b)
{
    return a | static_cast<unsigned>(b);
}
constexpr unsigned operator&(PromiseOption a, PromiseOption b)
{
    return static_cast<unsigned>(a) & static_cast<unsigned>(b);
}
constexpr unsigned operator&(unsigned a, PromiseOption b)
{
    return a & static_cast<unsigned>(b);
}

namespace detail {
struct VoidPlaceholder {
};
} // namespace detail

template<typename ResolveValueT, typename RejectValueT, unsigned options>
class NativePromise final : public NativePromiseBase, public ConvertibleToNativePromise {
public:

    static constexpr bool IsExclusive = !(options & PromiseOption::NonExclusive);
    static constexpr bool WithCrossThreadCopy = !!(options & PromiseOption::WithCrossThreadCopy);
    static constexpr bool WithAutomaticCrossThreadCopy = !(options & (PromiseOption::WithCrossThreadCopy | PromiseOption::WithoutCrossThreadCopy)) && (CrossThreadCopier<ResolveValueT>::IsNeeded || CrossThreadCopier<RejectValueT>::IsNeeded);
    static_assert(!WithAutomaticCrossThreadCopy || IsExclusive, "Using Non-Exclusive NativePromise with a ResolveValueT or RejectValueT requiring a call to isolatedCopy() must be explicitly set with WithCrossThreadCopy or WithoutCrossThreadCopy option");
    using ResolveValueType = std::conditional_t<WithAutomaticCrossThreadCopy || WithCrossThreadCopy, typename CrossThreadCopier<ResolveValueT>::Type, ResolveValueT>;
    using RejectValueType = std::conditional_t<std::is_void_v<RejectValueT>, detail::VoidPlaceholder, std::conditional_t<WithAutomaticCrossThreadCopy || WithCrossThreadCopy, typename CrossThreadCopier<RejectValueT>::Type, RejectValueT>>;
    using Result = Expected<ResolveValueType, RejectValueType>;
    using Error = Unexpected<RejectValueType>;

    // used by IsConvertibleToNativePromise to determine how to cast the result.
    using PromiseType = NativePromise;

    // We split the functionalities from a "Producer" that can create and resolve/reject a promise and a "Consumer"
    // that will then()/whenSettled() on such promise.
    using Producer = NativePromiseProducer<ResolveValueT, RejectValueT, options>;

    virtual ~NativePromise()
    {
        PROMISE_LOG("destroying ", *this);
        assertIsDead();
#if ASSERT_ENABLED
        Locker lock { m_lock };
        ASSERT(!isNothing());
        ASSERT(m_thenCallbacks.isEmpty());
        ASSERT(m_chainedPromises.isEmpty());
#endif
    }

    const Logger::LogSiteIdentifier& logSiteIdentifier() const { return m_logSiteIdentifier; }

private:
    // Return a |T&&| to enable move when IsExclusive is true or a |const T&| to enforce copy otherwise.
    template<typename T, typename R = std::conditional_t<IsExclusive, T&&, const T&>>
    static R maybeMove(T& aX)
    {
        return static_cast<R>(aX);
    }

public:
    template<typename ResolveValueType_, typename = std::enable_if<!std::is_void_v<ResolveValueT>>>
    static Ref<NativePromise> createAndResolve(ResolveValueType_&& resolveValue, const Logger::LogSiteIdentifier& resolveSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        auto p = adoptRef(*new NativePromise(resolveSite));
        p->resolve(std::forward<ResolveValueType_>(resolveValue), resolveSite);
        return p;
    }

    template<typename = std::enable_if<std::is_void_v<ResolveValueT>>>
    static Ref<NativePromise> createAndResolve(const Logger::LogSiteIdentifier& resolveSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        auto p = adoptRef(*new NativePromise(resolveSite));
        p->resolve(resolveSite);
        return p;
    }

    template<typename RejectValueType_, typename = std::enable_if<!std::is_void_v<RejectValueT>>>
    static Ref<NativePromise> createAndReject(RejectValueType_&& rejectValue, const Logger::LogSiteIdentifier& rejectSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        auto p = adoptRef(*new NativePromise(rejectSite));
        p->reject(std::forward<RejectValueType_>(rejectValue), rejectSite);
        return p;
    }

    template<typename = std::enable_if<std::is_void_v<RejectValueT>>>
    static Ref<NativePromise> createAndReject(const Logger::LogSiteIdentifier& rejectSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        auto p = adoptRef(*new NativePromise(rejectSite));
        p->reject(rejectSite);
        return p;
    }

    template<typename SettleValueType>
    static Ref<NativePromise> createAndSettle(SettleValueType&& result, const Logger::LogSiteIdentifier& site = DEFAULT_LOGSITEIDENTIFIER)
    {
        auto p = adoptRef(*new NativePromise(site));
        p->settle(std::forward<SettleValueType>(result), site);
        return p;
    }

    using AllPromiseType = NativePromise<std::conditional_t<std::is_void_v<ResolveValueType>, void, Vector<ResolveValueType>>, RejectValueType, options>;
    using AllSettledPromiseType = NativePromise<Vector<Result>, bool, options>;

private:
    template<typename ResolveValueT2, typename RejectValueT2, unsigned options2>
    friend class NativePromiseProducer;

    template<typename SettleValueType>
    inline void settleImpl(SettleValueType&& result, Locker<Lock>& lock)
    {
        assertIsHeld(m_lock);
        ASSERT(isNothing());
        m_result.emplace(std::forward<SettleValueType>(result));
        dispatchAll(lock);
    }

    template<typename ResolveValueType_, typename = std::enable_if<!std::is_void_v<ResolveValueT>>>
    void resolve(ResolveValueType_&& resolveValue, const Logger::LogSiteIdentifier& resolveSite)
    {
        static_assert(std::is_convertible_v<ResolveValueType_, ResolveValueT>, "resolve() argument must be implicitly convertible to NativePromise's ResolveValueT");
        Locker lock { m_lock };
        PROMISE_LOG(resolveSite, " resolving ", *this);
        if constexpr (WithCrossThreadCopy || WithAutomaticCrossThreadCopy)
            settleImpl(crossThreadCopy(std::forward<ResolveValueType_>(resolveValue)), lock);
        else
            settleImpl(std::forward<ResolveValueType_>(resolveValue), lock);
    }

    template<typename = std::enable_if<std::is_void_v<ResolveValueT>>>
    void resolve(const Logger::LogSiteIdentifier& resolveSite)
    {
        Locker lock { m_lock };
        PROMISE_LOG(resolveSite, " resolving ", *this);
        settleImpl(Result { }, lock);
    }

    template<typename RejectValueType_, typename = std::enable_if<!std::is_void_v<RejectValueT>>>
    void reject(RejectValueType_&& rejectValue, const Logger::LogSiteIdentifier& rejectSite)
    {
        static_assert(std::is_convertible_v<RejectValueType_, RejectValueT>, "reject() argument must be implicitly convertible to NativePromise's RejectValueT");
        Locker lock { m_lock };
        PROMISE_LOG(rejectSite, " rejecting ", *this);
        if constexpr (WithCrossThreadCopy || WithAutomaticCrossThreadCopy)
            settleImpl(Unexpected<RejectValueT>(crossThreadCopy(std::forward<RejectValueType_>(rejectValue))), lock);
        else
            settleImpl(Unexpected<RejectValueT>(std::forward<RejectValueType_>(rejectValue)), lock);
    }

    template<typename = std::enable_if<std::is_void_v<RejectValueT>>>
    void reject(const Logger::LogSiteIdentifier& rejectSite)
    {
        Locker lock { m_lock };
        PROMISE_LOG(rejectSite, " rejecting ", *this);
        settleImpl(makeUnexpected(detail::VoidPlaceholder()), lock);
    }

    template<typename SettleValueType>
    void settle(SettleValueType&& result, const Logger::LogSiteIdentifier& site)
    {
        static_assert(std::is_convertible_v<SettleValueType, Result>, "settle() argument must be implicitly convertible to NativePromise's Result");
        Locker lock { m_lock };
        PROMISE_LOG(site, " settling ", *this);
        if constexpr (WithCrossThreadCopy || WithAutomaticCrossThreadCopy)
            settleImpl(crossThreadCopy(std::forward<SettleValueType>(result)), lock);
        else
            settleImpl(std::forward<SettleValueType>(result), lock);
    }

    template<typename StorageType>
    void settleAsChainedPromise(StorageType&& storage, const Logger::LogSiteIdentifier& site)
    {
        Locker lock { m_lock };
        ASSERT(isNothing());
        PROMISE_LOG(site, " settling chained promise ", *this);
        m_result = std::forward<StorageType>(storage);
        dispatchAll(lock);
    }

    void setDispatchMode(PromiseDispatchMode dispatchMode, const Logger::LogSiteIdentifier& site)
    {
        static_assert(IsExclusive, "setDispatchMode can only be used with exclusive promises");
        Locker lock { m_lock };
        PROMISE_LOG(site, " runSynchronouslyOnTarget ", *this);
        ASSERT(isNothing(), "A Promise must not have been already resolved or rejected to set dispatch state");
        m_dispatchMode = dispatchMode;
    }

    // We can't move the Result object with non-exclusive promise.
    using ResultParam = std::conditional_t<IsExclusive, Result&&, const Result&>;

    class AllPromiseProducer : public ThreadSafeRefCounted<AllPromiseProducer> {
    public:
        explicit AllPromiseProducer(size_t dependentPromisesCount)
            : m_producer(makeUnique<typename AllPromiseType::Producer>())
            , m_outstandingPromises(dependentPromisesCount)
        {
            ASSERT(dependentPromisesCount);
            if constexpr (!std::is_void_v<ResolveValueT>)
                m_resolveValues.grow(dependentPromisesCount);
        }

        template<typename ResolveValueType_>
        void resolve(size_t index, ResolveValueType_&& resolveValue)
        {
            Locker lock { m_lock };
            if (!m_producer) {
                // Already resolved or rejected.
                return;
            }

            if constexpr (!std::is_void_v<ResolveValueT>)
                m_resolveValues[index] = std::forward<ResolveValueType_>(resolveValue);
            if (!--m_outstandingPromises) {
                if constexpr (std::is_void_v<ResolveValueT>)
                    m_producer->resolve();
                else {
                    m_producer->resolve(WTF::map(std::exchange(m_resolveValues, { }), [](auto&& resolveValue) {
                        return WTFMove(*resolveValue);
                    }));
                }
                m_producer = nullptr;
            }
        }

        template<typename RejectValueType_>
        void reject(RejectValueType_&& rejectValue)
        {
            Locker lock { m_lock };
            if (!m_producer) {
                // Already resolved or rejected.
                return;
            }
            if constexpr (std::is_void_v<RejectValueT>)
                m_producer->reject();
            else
                m_producer->reject(std::forward<RejectValueType_>(rejectValue));
            m_producer = nullptr;
            if constexpr (!std::is_void_v<ResolveValueT>)
                m_resolveValues.clear();
        }

        Ref<AllPromiseType> promise()
        {
            Locker lock { m_lock };
            return m_producer->promise();
        }

    private:
        Lock m_lock;
        NO_UNIQUE_ADDRESS std::conditional_t<!std::is_void_v<ResolveValueT>, Vector<std::optional<ResolveValueType>>, detail::VoidPlaceholder> m_resolveValues WTF_GUARDED_BY_LOCK(m_lock);
        std::unique_ptr<typename AllPromiseType::Producer> m_producer WTF_GUARDED_BY_LOCK(m_lock);
        size_t m_outstandingPromises WTF_GUARDED_BY_LOCK(m_lock);
    };

    class AllSettledPromiseProducer : public ThreadSafeRefCounted<AllSettledPromiseProducer> {
    public:
        explicit AllSettledPromiseProducer(size_t dependentPromisesCount)
            : m_producer(makeUnique<typename AllSettledPromiseType::Producer>())
            , m_outstandingPromises(dependentPromisesCount)
        {
            ASSERT(dependentPromisesCount);
            m_results.grow(dependentPromisesCount);
        }

        void settle(size_t index, ResultParam result)
        {
            Locker lock { m_lock };
            if (!m_producer) {
                // Already settled.
                return;
            }

            m_results[index].emplace(maybeMove(result));
            if (!--m_outstandingPromises) {
                m_producer->resolve(WTF::map(std::exchange(m_results, { }), [](auto&& result) {
                    return WTFMove(*result);
                }));
                m_producer = nullptr;
            }
        }

        Ref<AllSettledPromiseType> promise()
        {
            Locker lock { m_lock };
            return m_producer->promise();
        }

    private:
        Lock m_lock;
        Vector<std::optional<Result>> m_results WTF_GUARDED_BY_LOCK(m_lock);
        std::unique_ptr<typename AllSettledPromiseType::Producer> m_producer WTF_GUARDED_BY_LOCK(m_lock);
        size_t m_outstandingPromises WTF_GUARDED_BY_LOCK(m_lock);
    };

public:
    static Ref<AllPromiseType> all(const Vector<Ref<NativePromise>>& promises)
    {
        if (promises.isEmpty()) {
            if constexpr (std::is_void_v<ResolveValueT>)
                return AllPromiseType::createAndResolve();
            else
                return AllPromiseType::createAndResolve(typename AllPromiseType::ResolveValueType());
        }
        auto producer = adoptRef(new AllPromiseProducer(promises.size()));
        auto promise = producer->promise();
        for (size_t i = 0; i < promises.size(); ++i) {
            promises[i]->whenSettled([producer, i] (ResultParam result) {
                if (result) {
                    if constexpr (std::is_void_v<ResolveValueT>)
                        producer->resolve(i, detail::VoidPlaceholder());
                    else
                        producer->resolve(i, maybeMove(result.value()));
                    return;
                }
                if constexpr (std::is_void_v<RejectValueT>)
                    producer->reject(detail::VoidPlaceholder());
                else
                    producer->reject(maybeMove(result.error()));
            });
        }
        return promise;
    }

    static Ref<AllSettledPromiseType> allSettled(const Vector<Ref<NativePromise>>& promises)
    {
        if (promises.isEmpty())
            return AllSettledPromiseType::createAndResolve(Vector<Result>());

        auto producer = adoptRef(new AllSettledPromiseProducer(promises.size()));
        auto promise = producer->promise();
        for (size_t i = 0; i < promises.size(); ++i) {
            promises[i]->whenSettled([producer, i] (ResultParam result) {
                producer->settle(i, maybeMove(result));
            });
        }
        return promise;
    }

private:
    explicit NativePromise(const Logger::LogSiteIdentifier& creationSite)
        : m_logSiteIdentifier(creationSite)
    {
        PROMISE_LOG("creating ", *this);
    }

    class ThenCallbackBase : public NativePromiseRequest::Callback {

    public:
        ThenCallbackBase(RefPtr<RefCountedSerialFunctionDispatcher>&& targetQueue, const Logger::LogSiteIdentifier& callSite)
            : m_targetQueue(WTFMove(targetQueue))
            , m_logSiteIdentifier(callSite)
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

            if (UNLIKELY(!m_targetQueue || (promise.m_dispatchMode == PromiseDispatchMode::RunSynchronouslyOnTarget && m_targetQueue->isCurrent()))) {
                PROMISE_LOG(*promise.m_result ? "Resolving" : "Rejecting", " synchronous then() call made from ", m_logSiteIdentifier, "[", promise, " callback:", (const void*)this, "]");
                if (m_disconnected) {
                    PROMISE_LOG("ThenCallback disconnected aborting [callback:", (const void*)this, " callSite:", m_logSiteIdentifier, "]");
                    return;
                }
                {
                    // Holding the lock is unnecessary while running the resolve/reject callback and we don't want to hold the lock for too long.
                    DropLockForScope unlocker(lock);
                    processResult(promise.result());
                }
                return;
            }
            m_targetQueue->dispatch([this, protectedThis = Ref { *this }, promise = Ref { promise }, operation = *promise.m_result ? "Resolving" : "Rejecting"] () mutable {
                PROMISE_LOG(operation, " then() call made from ", m_logSiteIdentifier, "[", promise.get(), " callback:", (const void*)this, "]");
                if (m_disconnected) {
                    PROMISE_LOG("ThenCallback disconnected aborting [callback:", (const void*)this, " callSite:", m_logSiteIdentifier, "]");
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
        const RefPtr<RefCountedSerialFunctionDispatcher> m_targetQueue;
        const Logger::LogSiteIdentifier m_logSiteIdentifier;

#if ASSERT_ENABLED
        virtual RefPtr<NativePromiseBase> completionPromise() = 0;
        // In a debug build, m_disconnected is checked in the destructor
        // Otherwise it is only ever modified and read on the target queue.
        std::atomic<bool> m_disconnected { false };
#else
        bool m_disconnected { false };
#endif
    };
    friend LogArgument<NativePromise::ThenCallbackBase>;

    template<bool IsChaining, typename ReturnPromiseType_>
    class ThenCallback : public ThenCallbackBase {
    public:
        using ReturnPromiseType = std::conditional_t<IsChaining, ReturnPromiseType_, GenericPromise>;
        using CallBackType = std::conditional_t<IsChaining, Function<Ref<ReturnPromiseType_>(ResultParam)>, Function<void(ResultParam)>>;

        ThenCallback(RefPtr<RefCountedSerialFunctionDispatcher>&& targetQueue, CallBackType&& function, const Logger::LogSiteIdentifier& callSite)
            : ThenCallbackBase(WTFMove(targetQueue), callSite)
            , m_settleFunction(WTFMove(function))
        {
        }

        void disconnect() override
        {
            assertIsCurrent(*ThenCallbackBase::m_targetQueue);
            ThenCallbackBase::disconnect();
            m_settleFunction = nullptr;
        }

        void processResult(Result& result) override
        {
            if (ThenCallbackBase::m_targetQueue)
                assertIsCurrent(*ThenCallbackBase::m_targetQueue);
            ASSERT(m_settleFunction);
            auto completionProducer = [this] {
                Locker lock { m_lock };
                return std::exchange(m_completionProducer, { });
            }();
            if constexpr (IsChaining) {
                auto p = m_settleFunction(maybeMove(result));
                if (completionProducer)
                    p->chainTo(WTFMove(*completionProducer), { "<chained completion promise>", nullptr });
            } else {
                m_settleFunction(maybeMove(result));
                if (completionProducer)
                    completionProducer->resolve({ "<chained completion promise>", nullptr });
            }

            m_settleFunction = nullptr;
        }

        void setCompletionPromise(std::unique_ptr<typename ReturnPromiseType::Producer>&& completionProducer)
        {
            Locker lock { m_lock };
            m_completionProducer = WTFMove(completionProducer);
        }

#if ASSERT_ENABLED
        RefPtr<NativePromiseBase> completionPromise() override
        {
            Locker lock { m_lock };
            return m_completionProducer ? m_completionProducer->promise().ptr() : nullptr;
        }
#endif

        Lock m_lock;
        std::unique_ptr<typename ReturnPromiseType::Producer> m_completionProducer WTF_GUARDED_BY_LOCK(m_lock);
    private:
        CallBackType m_settleFunction;
    };

    void maybeSettle(Ref<ThenCallbackBase>&& thenCallback, const Logger::LogSiteIdentifier& callSite)
    {
        Locker lock { m_lock };
        ASSERT(!IsExclusive || !m_haveRequest, "Using an exclusive promise in a non-exclusive fashion");
        m_haveRequest = true;
        PROMISE_LOG(callSite, " invoking maybeSettle() [", *this, " callback:", (const void*)thenCallback.ptr(), " isNothing:", isNothing(), "]");
        if (!isNothing())
            thenCallback->dispatch(*this, lock);
        else
            m_thenCallbacks.append(WTFMove(thenCallback));
    }

    template<typename ThenCallbackType>
    class ThenCommand : public ConvertibleToNativePromise {
        // Allow Promise::then() to access the private constructor,
        template<typename, typename, unsigned>
        friend class NativePromise;

        // used by IsConvertibleToNativePromise to determine how to cast the result.
        using PromiseType = typename ThenCallbackType::ReturnPromiseType;

        ThenCommand(NativePromise& promise, Ref<ThenCallbackType>&& thenCallback, const Logger::LogSiteIdentifier& callSite)
            : m_promise(promise)
            , m_thenCallback(WTFMove(thenCallback))
            , m_logSiteIdentifier(callSite)
        {
        }

        ThenCommand(ThenCommand&& other) = default;
        ThenCommand& operator=(ThenCommand&& other) = default;

    public:
        ~ThenCommand()
        {
            // Issue the request now if the return value of then()/whenSettled() is not used.
            if (m_thenCallback)
                m_promise->maybeSettle(m_thenCallback.releaseNonNull(), m_logSiteIdentifier);
        }

        // Allow calling ->then()/whenSettled() again for more promise chaining or ->track() to
        // end chaining and track the request for future disconnection.
        // Defined -> operator for consistency in calling pattern.
        ThenCommand* operator->() { return this; }

        operator RefPtr<PromiseType>()
        {
            return Ref<PromiseType>(*this);
        }

        // Allow conversion from ThenCommand to Ref<NativePromise> like:
        // Ref<NativePromise> p = somePromise->then(...);
        // p->then(thread1, ...);
        // p->then(thread2, ...);
        operator Ref<PromiseType>()
        {
            return completionPromise();
        }

        // Allow calling then() again by converting the ThenCommand to Ref<NativePromise>
        template<typename ResolveFunction, typename RejectFunction>
        auto then(RefCountedSerialFunctionDispatcher& targetQueue, ResolveFunction&& resolveFunction, RejectFunction&& rejectFunction, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
        {
            return completionPromise()->then(targetQueue, std::forward<ResolveFunction>(resolveFunction), std::forward<RejectFunction>(rejectFunction), callSite);
        }

        template<typename ThisType, typename ResolveMethod, typename RejectMethod>
        auto then(RefCountedSerialFunctionDispatcher& targetQueue, ThisType& thisVal, ResolveMethod resolveMethod, RejectMethod rejectMethod, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
        {
            return completionPromise()->then(targetQueue, thisVal, std::forward<ResolveMethod>(resolveMethod), std::forward<RejectMethod>(rejectMethod), callSite);
        }

        // Allow calling whenSettled() again by converting the ThenCommand to Ref<NativePromise>
        template<typename SettleFunction>
        auto whenSettled(RefCountedSerialFunctionDispatcher& targetQueue, SettleFunction&& settleFunction, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
        {
            return completionPromise()->whenSettled(targetQueue, std::forward<SettleFunction>(settleFunction), callSite);
        }

        template<typename ThisType, typename SettleMethod>
        auto whenSettled(RefCountedSerialFunctionDispatcher& targetQueue, ThisType& thisVal, SettleMethod settleMethod, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
        {
            return completionPromise()->whenSettled(targetQueue, thisVal, std::forward<SettleMethod>(settleMethod), callSite);
        }

        void track(NativePromiseRequest& requestHolder)
        {
            ASSERT(m_thenCallback, "Can only track a request once");
            requestHolder.track(*m_thenCallback);
            m_promise->maybeSettle(m_thenCallback.releaseNonNull(), m_logSiteIdentifier);
        }

        void chainTo(Producer&& chainedPromise, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
        {
            completionPromise()->chainTo(WTFMove(chainedPromise), callSite);
        }

        template<typename ResolveValueT2, typename RejectValueT2, unsigned options2 = 0>
        void chainTo(NativePromiseProducer<ResolveValueT2, RejectValueT2, options2>&& chainedPromise, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
        {
            completionPromise()->template chainTo<ResolveValueT2, RejectValueT2, options2>(WTFMove(chainedPromise), callSite);
        }

    private:
        Ref<PromiseType> completionPromise()
        {
            ASSERT(m_thenCallback, "Conversion can only be done once");
            // We create a completion promise producer which will be resolved or rejected when the ThenCallback will be run
            // with the value returned by the callbacks provided to then().
            auto producer = makeUnique<typename PromiseType::Producer>(PromiseDispatchMode::Default, Logger::LogSiteIdentifier { "<completion promise>", nullptr });
            auto promise = producer->promise();
            m_thenCallback->setCompletionPromise(WTFMove(producer));
            m_promise->maybeSettle(m_thenCallback.releaseNonNull(), m_logSiteIdentifier);
            return promise;
        }
        Ref<NativePromise> m_promise;
        RefPtr<ThenCallbackType> m_thenCallback;
        const Logger::LogSiteIdentifier m_logSiteIdentifier;
    };

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

    template <typename F, typename Arg>
    static auto invokeWithVoidOrWithArg(F&& f, Arg&& arg)
    {
        if constexpr (std::is_invocable_v<F>)
            return std::invoke(std::forward<F>(f));
        else
            return std::invoke(std::forward<F>(f), std::forward<Arg>(arg));
    }

    template <typename ThisType, typename M, typename Arg>
    static auto invokeWithVoidOrWithArg(ThisType& thisVal, M m, Arg&& arg)
    {
        if constexpr (std::is_invocable_v<M, ThisType>)
            return std::invoke(m, thisVal);
        else
            return std::invoke(m, thisVal, std::forward<Arg>(arg));
    }

    template<typename SettleFunction>
    auto whenSettled(SettleFunction&& settleFunction, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        using ThenCallbackType = ThenCallback<false, void>;
        using ReturnType = ThenCommand<ThenCallbackType>;

        auto thenCallback = adoptRef(*new ThenCallbackType(RefPtr<RefCountedSerialFunctionDispatcher> { }, WTFMove(settleFunction), callSite));
        return ReturnType(*this, WTFMove(thenCallback), callSite);
    }

public:
    template<typename SettleFunction>
    auto whenSettled(RefCountedSerialFunctionDispatcher& targetQueue, SettleFunction&& settleFunction, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        using R1 = typename RemoveSmartPointer<decltype(invokeWithVoidOrWithArg(std::forward<SettleFunction>(settleFunction), std::declval<Result>()))>::type;
        using IsChaining = std::bool_constant<IsConvertibleToNativePromise<R1>>;
        static_assert(IsConvertibleToNativePromise<R1> || std::is_void_v<R1>, "Settle method must return a promise or nothing");
        using LambdaReturnType = decltype(std::declval<LambdaReturnTrait>().template lambda<R1>());

        auto lambda = [settleFunction = std::forward<SettleFunction>(settleFunction)] (ResultParam result) mutable -> LambdaReturnType {
            return invokeWithVoidOrWithArg(WTFMove(settleFunction), maybeMove(result));
        };

        using ThenCallbackType = ThenCallback<IsChaining::value, decltype(std::declval<LambdaReturnTrait>().template type<R1>())>;
        using ReturnType = ThenCommand<ThenCallbackType>;

        auto thenCallback = adoptRef(*new ThenCallbackType(RefPtr { &targetQueue }, WTFMove(lambda), callSite));
        return ReturnType(*this, WTFMove(thenCallback), callSite);
    }

    template<typename ThisType, typename SettleMethod>
    auto whenSettled(RefCountedSerialFunctionDispatcher& targetQueue, ThisType& thisVal, SettleMethod settleMethod, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        using R1 = typename RemoveSmartPointer<decltype(invokeWithVoidOrWithArg(thisVal, settleMethod, std::declval<Result>()))>::type;
        static_assert(IsConvertibleToNativePromise<R1> || std::is_void_v<R1>, "Settle method must return a promise or nothing");
        using LambdaReturnType = decltype(std::declval<LambdaReturnTrait>().template lambda<R1>());

        return whenSettled(targetQueue, [thisVal = Ref { thisVal }, settleMethod] (ResultParam result) mutable -> LambdaReturnType {
            return invokeWithVoidOrWithArg(thisVal.get(), settleMethod, maybeMove(result));
        }, callSite);
    }

    template<typename ResolveFunction, typename RejectFunction>
    auto then(RefCountedSerialFunctionDispatcher& targetQueue, ResolveFunction&& resolveFunction, RejectFunction&& rejectFunction, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        using R1 = typename RemoveSmartPointer<decltype(invokeWithVoidOrWithArg(std::forward<ResolveFunction>(resolveFunction), std::declval<std::conditional_t<std::is_void_v<ResolveValueT>, detail::VoidPlaceholder, ResolveValueType>>()))>::type;
        using R2 = typename RemoveSmartPointer<decltype(invokeWithVoidOrWithArg(std::forward<RejectFunction>(rejectFunction), std::declval<RejectValueType>()))>::type;
        using IsChaining = std::bool_constant<RelatedNativePromise<R1, R2>>;
        static_assert(IsChaining::value || (std::is_void_v<R1> && std::is_void_v<R2>), "resolve/reject methods must return a promise of the same type or nothing");
        using LambdaReturnType = decltype(std::declval<LambdaReturnTrait>().template lambda<R1>());

        return whenSettled(targetQueue, [resolveFunction = std::forward<ResolveFunction>(resolveFunction), rejectFunction = std::forward<RejectFunction>(rejectFunction)] (ResultParam result) mutable -> LambdaReturnType {
            if (result) {
                if constexpr (std::is_void_v<ResolveValueT>)
                    return invokeWithVoidOrWithArg(WTFMove(resolveFunction), detail::VoidPlaceholder());
                else
                    return invokeWithVoidOrWithArg(WTFMove(resolveFunction), maybeMove(result.value()));
            }
            return invokeWithVoidOrWithArg(WTFMove(rejectFunction), maybeMove(result.error()));
        }, callSite);
    }

    template<typename ThisType, typename ResolveMethod, typename RejectMethod>
    auto then(RefCountedSerialFunctionDispatcher& targetQueue, ThisType& thisVal, ResolveMethod resolveMethod, RejectMethod rejectMethod, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        using R1 = typename RemoveSmartPointer<decltype(invokeWithVoidOrWithArg(thisVal, resolveMethod, std::declval<std::conditional_t<std::is_void_v<ResolveValueT>, detail::VoidPlaceholder, ResolveValueType>>()))>::type;
        using R2 = typename RemoveSmartPointer<decltype(invokeWithVoidOrWithArg(thisVal, rejectMethod, std::declval<RejectValueType>()))>::type;
        using IsChaining = std::bool_constant<RelatedNativePromise<R1, R2>>;
        static_assert(IsChaining::value || (std::is_void_v<R1> && std::is_void_v<R2>), "resolve/reject methods must return a promise of the same type or nothing");
        using LambdaReturnType = decltype(std::declval<LambdaReturnTrait>().template lambda<R1>());

        return whenSettled(targetQueue, [thisVal = Ref { thisVal }, resolveMethod, rejectMethod] (ResultParam result) -> LambdaReturnType {
            if (result) {
                if constexpr (std::is_void_v<ResolveValueT>)
                    return invokeWithVoidOrWithArg(thisVal.get(), resolveMethod, detail::VoidPlaceholder());
                else
                    return invokeWithVoidOrWithArg(thisVal.get(), resolveMethod, maybeMove(result.value()));
            }
            return invokeWithVoidOrWithArg(thisVal.get(), rejectMethod, maybeMove(result.error()));
        }, callSite);
    }

    void chainTo(Producer&& chainedPromise, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        Locker lock { m_lock };
        ASSERT(!IsExclusive || !m_haveRequest, "Using an exclusive promise in a non-exclusive fashion");
        m_haveRequest = true;
        PROMISE_LOG(callSite, " invoking chainTo() [", *this, " chainedPromise:", chainedPromise.promise().get(), " isNothing:", isNothing(), "]");

        if constexpr (IsExclusive)
            chainedPromise.setDispatchMode(m_dispatchMode, callSite);

        if (isNothing())
            m_chainedPromises.append(WTFMove(chainedPromise));
        else
            settleChainedPromise(WTFMove(chainedPromise));
    }

    template<typename ResolveValueT2, typename RejectValueT2, unsigned options2 = 0>
    void chainTo(NativePromiseProducer<ResolveValueT2, RejectValueT2, options2>&& chainedPromise, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        static_assert(std::is_convertible_v<ResolveValueT, ResolveValueT2> || std::is_void_v<ResolveValueT2>, "resolve type must be compatible");
        static_assert(std::is_convertible_v<RejectValueT, RejectValueT2> || std::is_void_v<RejectValueT2>, "reject type must be compatible");
        PROMISE_LOG(callSite, " invoking chainTo() [", *this, " chainedPromise:", chainedPromise.promise().get(), " isSettled:", isSettled(), "]");
        if constexpr (NativePromiseProducer<ResolveValueT2, RejectValueT2, options2>::PromiseType::IsExclusive && IsExclusive)
            chainedPromise.setDispatchMode(m_dispatchMode, callSite);

        whenSettled([producer = WTFMove(chainedPromise)](auto&& result) {
            if (!result) {
                if constexpr (std::is_void_v<RejectValueT2>)
                    producer.reject();
                else
                    producer.reject(maybeMove(result.error()));
                return;
            }
            if constexpr (std::is_void_v<ResolveValueT2>)
                producer.resolve();
            else
                producer.resolve(maybeMove(*result));
        });
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

    bool isSettled() const
    {
        Locker lock { m_lock };
        return !isNothing();
    }

    template<typename T>
    Ref<T> convert(const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        static_assert(IsNativePromise<T>, "convert expects another promise type");
        typename T::Producer producer { PromiseDispatchMode::Default, callSite };
        Ref promise = producer.promise();
        chainTo(WTFMove(producer));
        return promise;
    }

    template<typename newResolveValueT, typename newRejectValueT, unsigned newOptions = 0>
    operator Ref<NativePromise<newResolveValueT, newRejectValueT, newOptions>>()
    {
        return convert<NativePromise<newResolveValueT, newRejectValueT, newOptions>>();
    }

private:
    bool isNothing() const
    {
        assertIsHeld(m_lock);
        return !m_result;
    }

    Result& result()
    {
        // Only called by SettleFunction on the target's queue once all operations are complete and settled.
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
            settleChainedPromise(WTFMove(chainedPromise));
    }

    void settleChainedPromise(Producer&& other)
    {
        assertIsHeld(m_lock);
        ASSERT(!isNothing());
        auto producer = WTFMove(other);
        producer.promise()->settleAsChainedPromise(maybeMove(m_result), { "<chained promise>", nullptr });
    }

    // Replicate either std::optional<Result> if Exclusive or Ref<std::optional<Result>> otherwise.
    class Storage {
        struct RefCountedResult : ThreadSafeRefCounted<RefCountedResult> {
            std::optional<Result> result;
        };
        using ResultType = std::conditional_t<IsExclusive, std::optional<Result>, Ref<RefCountedResult>>;
        ResultType m_result;
        std::optional<Result>& optionalResult()
        {
            if constexpr (IsExclusive)
                return m_result;
            else
                return m_result->result;
        }
        const std::optional<Result>& optionalResult() const
        {
            if constexpr (IsExclusive)
                return m_result;
            else
                return m_result->result;
        }
    public:
        Storage()
            : m_result([] {
                if constexpr(IsExclusive)
                    return std::nullopt;
                else
                    return adoptRef(*new RefCountedResult);
            }())
        {
        }
        bool has_value() const
        {
            if constexpr (IsExclusive)
                return m_result.has_value();
            else
                return m_result->result.has_value();
        }
        explicit operator bool() const { return has_value(); }
        Storage& operator=(Storage&&) = default;
        Storage& operator=(const Storage&) = default;
        const Result& operator*() const
        {
            ASSERT(has_value());
            return *optionalResult();
        }
        Result& operator*()
        {
            ASSERT(has_value());
            return *optionalResult();
        }
        const Result* operator->() const
        {
            if (!has_value())
                return nullptr;
            return &(this->operator*());
        }
        template <typename... Args>
        void emplace(Args&&... args)
        {
            optionalResult().emplace(std::forward<Args>(args)...);
        }
    };
    const Logger::LogSiteIdentifier m_logSiteIdentifier; // For logging
    mutable Lock m_lock;
    Storage m_result WTF_GUARDED_BY_LOCK(m_lock); // Set on any threads when the promise is resolved, only read on the promise's target queue.
    // Experiments show that we never have more than 3 elements when IsExclusive is false.
    // So '3' is a good value to avoid heap allocation in most cases.
    Vector<Ref<ThenCallbackBase>, IsExclusive ? 1 : 3> m_thenCallbacks WTF_GUARDED_BY_LOCK(m_lock);
    Vector<Producer> m_chainedPromises WTF_GUARDED_BY_LOCK(m_lock);
    bool m_haveRequest WTF_GUARDED_BY_LOCK(m_lock) { false };
    std::atomic<PromiseDispatchMode> m_dispatchMode { PromiseDispatchMode::Default };
};

template<typename ResolveValueT, typename RejectValueT, unsigned options>
class NativePromiseProducer final : public ConvertibleToNativePromise {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // used by IsConvertibleToNativePromise to determine how to cast the result.
    using PromiseType = NativePromise<ResolveValueT, RejectValueT, options>;

    explicit NativePromiseProducer(PromiseDispatchMode dispatchMode = PromiseDispatchMode::Default, const Logger::LogSiteIdentifier& creationSite = DEFAULT_LOGSITEIDENTIFIER)
        : m_promise(adoptRef(new PromiseType(creationSite)))
        , m_creationSite(creationSite)
    {
        if constexpr (PromiseType::IsExclusive)
            m_promise->setDispatchMode(dispatchMode, creationSite);
    }

    NativePromiseProducer(NativePromiseProducer&& other) = default;
    NativePromiseProducer& operator=(NativePromiseProducer&& other) = default;

    ~NativePromiseProducer()
    {
        assertIsDead();
    }

    explicit operator bool() const { return m_promise && m_promise->isSettled(); }
    bool isNothing() const
    {
        ASSERT(m_promise, "used after moved");
        return m_promise && !m_promise->isSettled();
    }

    template<typename ResolveValueType_, typename = std::enable_if<!std::is_void_v<ResolveValueT>>>
    void resolve(ResolveValueType_&& resolveValue, const Logger::LogSiteIdentifier& resolveSite = DEFAULT_LOGSITEIDENTIFIER) const
    {
        ASSERT(isNothing());
        if (!isNothing()) {
            PROMISE_LOG(resolveSite, " ignored already resolved or rejected ", *m_promise);
            return;
        }
        m_promise->resolve(std::forward<ResolveValueType_>(resolveValue), resolveSite);
    }

    template<typename = std::enable_if<std::is_void_v<ResolveValueT>>>
    void resolve(const Logger::LogSiteIdentifier& resolveSite = DEFAULT_LOGSITEIDENTIFIER) const
    {
        ASSERT(isNothing());
        if (!isNothing()) {
            PROMISE_LOG(resolveSite, " ignored already resolved or rejected ", *m_promise);
            return;
        }
        m_promise->resolve(resolveSite);
    }

    template<typename RejectValueType_, typename = std::enable_if<!std::is_void_v<RejectValueT>>>
    void reject(RejectValueType_&& rejectValue, const Logger::LogSiteIdentifier& rejectSite = DEFAULT_LOGSITEIDENTIFIER) const
    {
        ASSERT(isNothing());
        if (!isNothing()) {
            PROMISE_LOG(rejectSite, " ignored already resolved or rejected ", *m_promise);
            return;
        }
        m_promise->reject(std::forward<RejectValueType_>(rejectValue), rejectSite);
    }

    template<typename = std::enable_if<std::is_void_v<RejectValueT>>>
    void reject(const Logger::LogSiteIdentifier& rejectSite = DEFAULT_LOGSITEIDENTIFIER) const
    {
        ASSERT(isNothing());
        if (!isNothing()) {
            PROMISE_LOG(rejectSite, " ignored already resolved or rejected ", *m_promise);
            return;
        }
        m_promise->reject(rejectSite);
    }

    template<typename SettleValue>
    void settle(SettleValue&& result, const Logger::LogSiteIdentifier& site = DEFAULT_LOGSITEIDENTIFIER) const
    {
        ASSERT(isNothing());
        if (!isNothing()) {
            PROMISE_LOG(site, " ignored already resolved or rejected ", *m_promise);
            return;
        }
        m_promise->settle(std::forward<SettleValue>(result), site);
    }

    operator Ref<PromiseType>() const
    {
        ASSERT(m_promise, "used after move");
        RefPtr promise = m_promise;
        return promise.releaseNonNull();
    }

    Ref<PromiseType> promise() const
    {
        ASSERT(m_promise, "used after move");
        RefPtr promise = m_promise;
        return promise.releaseNonNull();
    }

    // Allow calling ->then()/whenSettled() again for more promise chaining.
    // Defined -> operator for consistency in calling pattern.
    NativePromiseProducer* operator->() { return this; }

    // Allow calling then() again by converting the ThenCommand to Ref<NativePromise>
    template<typename ResolveFunction, typename RejectFunction>
    auto then(RefCountedSerialFunctionDispatcher& targetQueue, ResolveFunction&& resolveFunction, RejectFunction&& rejectFunction, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        ASSERT(m_promise, "used after move");
        return m_promise->then(targetQueue, std::forward<ResolveFunction>(resolveFunction), std::forward<RejectFunction>(rejectFunction), callSite);
    }

    template<typename ThisType, typename ResolveMethod, typename RejectMethod>
    auto then(RefCountedSerialFunctionDispatcher& targetQueue, ThisType& thisVal, ResolveMethod resolveMethod, RejectMethod rejectMethod, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        ASSERT(m_promise, "used after move");
        return m_promise->then(targetQueue, thisVal, std::forward<ResolveMethod>(resolveMethod), std::forward<RejectMethod>(rejectMethod), callSite);
    }

    // Allow calling whenSettled() again by converting the ThenCommand to Ref<NativePromise>
    template<class DispatcherType, typename SettleFunction>
    auto whenSettled(DispatcherType& targetQueue, SettleFunction&& settleFunction, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        ASSERT(m_promise, "used after move");
        return m_promise->whenSettled(targetQueue, std::forward<SettleFunction>(settleFunction), callSite);
    }

    template<typename ThisType, typename SettleMethod>
    auto whenSettled(RefCountedSerialFunctionDispatcher& targetQueue, ThisType& thisVal, SettleMethod settleMethod, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        ASSERT(m_promise, "used after move");
        return m_promise->whenSettled(targetQueue, thisVal, std::forward<SettleMethod>(settleMethod), callSite);
    }

    void chainTo(NativePromiseProducer&& chainedPromise, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        ASSERT(m_promise, "used after move");
        m_promise->chainTo(WTFMove(chainedPromise), callSite);
    }

    template<typename ResolveValueT2, typename RejectValueT2, unsigned options2 = 0>
    void chainTo(NativePromiseProducer<ResolveValueT2, RejectValueT2, options2>&& chainedPromise, const Logger::LogSiteIdentifier& callSite = DEFAULT_LOGSITEIDENTIFIER)
    {
        ASSERT(m_promise, "used after move");
        m_promise->template chainTo<ResolveValueT2, RejectValueT2, options2>(WTFMove(chainedPromise), callSite);
    }

private:
    template<typename ResolveValueT2, typename RejectValueT2, unsigned options2>
    friend class NativePromise;

    void setDispatchMode(PromiseDispatchMode dispatchMode, const Logger::LogSiteIdentifier& callSite) const
    {
        ASSERT(m_promise, "used after move");
        m_promise->setDispatchMode(dispatchMode, callSite);
    }

    friend PromiseType;
    void assertIsDead() const
    {
        if (m_promise)
            m_promise->assertIsDead();
    }

    // The Producer may be moved to resolve/reject the completion promise.
    // While we expect m_promise to never be null, it would cause a null dereference in the destructor if the destructor was called after a move.
    RefPtr<PromiseType> m_promise;
    const Logger::LogSiteIdentifier m_creationSite; // For logging
};

// A generic promise type that does the trick for simple use cases.
using GenericPromise = NativePromise<void, void>;

// A generic, non-exclusive promise type that does the trick for simple use cases.
using GenericNonExclusivePromise = NativePromise<void, void, PromiseOption::Default | PromiseOption::NonExclusive>;

template<typename S, typename E>
Ref<NativePromise<S, E>> createSettledPromise(Expected<S, E>&& result)
{
    return NativePromise<S, E>::createAndSettle(WTFMove(result));
}

// Invoke a function object (e.g., lambda) asynchronously.
// Returns a promise that the function should eventually resolve or reject once the original promise returned by the lambda
// is itself resolved or rejected.
// The lambda can return an Expected<T, U> or void.
template<typename Function>
static auto invokeAsync(SerialFunctionDispatcher& targetQueue, Function&& function, const Logger::LogSiteIdentifier& callerName = DEFAULT_LOGSITEIDENTIFIER)
{
    static_assert(!std::is_lvalue_reference_v<Function>, "Function object must not be passed by lvalue-ref (to avoid unplanned copies); WTFMove() the object.");
    using ReturnType = decltype(function());
    using ReturnTypeNoRef = typename RemoveSmartPointer<ReturnType>::type;
    static_assert((IsSmartRef<ReturnType>::value && IsConvertibleToNativePromise<ReturnTypeNoRef>) || IsExpected<ReturnType>::value || std::is_void_v<ReturnType>, "Function object must return Ref<NativePromise>, Expected<T, F> or void");

    if constexpr (IsConvertibleToNativePromise<ReturnTypeNoRef>) {
        typename ReturnTypeNoRef::PromiseType::Producer proxyPromiseProducer(PromiseDispatchMode::Default, callerName);
        auto promise = proxyPromiseProducer.promise();
        targetQueue.dispatch([producer = WTFMove(proxyPromiseProducer), function = WTFMove(function)] () mutable {
            static_cast<Ref<typename ReturnTypeNoRef::PromiseType>>(function())->chainTo(WTFMove(producer), { "invokeAsync proxy", nullptr });
        });
        return promise;
    } else if constexpr (std::is_void_v<ReturnType>) {
        GenericPromise::Producer proxyPromiseProducer(PromiseDispatchMode::Default, callerName);
        auto promise = proxyPromiseProducer.promise();
        targetQueue.dispatch([producer = WTFMove(proxyPromiseProducer), function = WTFMove(function)] () mutable {
            function();
            producer.resolve({ "invokeAsync proxy", nullptr });
        });
        return promise;
    } else {
        NativePromiseProducer<typename ReturnType::value_type, typename ReturnType::error_type> proxyPromiseProducer(PromiseDispatchMode::Default, callerName);
        auto promise = proxyPromiseProducer.promise();
        targetQueue.dispatch([producer = WTFMove(proxyPromiseProducer), function = WTFMove(function)] () mutable {
            createSettledPromise(function())->chainTo(WTFMove(producer), { "invokeAsync proxy", nullptr });
        });
        return promise;
    }
}

template<typename ResolveValueT, typename RejectValueT, unsigned options>
struct LogArgument<NativePromise<ResolveValueT, RejectValueT, options>> {
    static String toString(const NativePromise<ResolveValueT, RejectValueT, options>& p)
    {
        return makeString("NativePromise", LogArgument<const void*>::toString(&p), '<', LogArgument<Logger::LogSiteIdentifier>::toString(p.logSiteIdentifier()), '>');
    }
};

template<>
struct LogArgument<GenericPromise> {
    static String toString(const GenericPromise& p)
    {
        return makeString("GenericPromise", LogArgument<const void*>::toString(&p), '<', LogArgument<Logger::LogSiteIdentifier>::toString(p.logSiteIdentifier()), '>');
    }
};

} // namespace WTF

#undef PROMISE_LOG
#undef HAS_SOURCE_LOCATION
#undef DEFAULT_LOGSITEIDENTIFIER

using WTF::invokeAsync;
using WTF::GenericPromise;
using WTF::GenericNonExclusivePromise;
using WTF::NativePromise;
using WTF::NativePromiseRequest;
using WTF::PromiseDispatchMode;
using WTF::PromiseOption;
