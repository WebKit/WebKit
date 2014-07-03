/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef GenericCallback_h
#define GenericCallback_h

#include "APIError.h"
#include "ProcessThrottler.h"
#include "ShareableBitmap.h"
#include "WKAPICast.h"
#include <functional>
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RunLoop.h>

namespace WebKit {

class CallbackBase : public RefCounted<CallbackBase> {
public:
    enum class Error {
        None,
        Unknown,
        ProcessExited,
        OwnerWasInvalidated,
    };

    virtual ~CallbackBase()
    {
    }

    uint64_t callbackID() const { return m_callbackID; }

    template<class T>
    T* as()
    {
        if (T::type() == m_type)
            return static_cast<T*>(this);

        return nullptr;
    }

    virtual void invalidate(Error) = 0;

protected:
    struct TypeTag { };
    typedef const TypeTag* Type;

    explicit CallbackBase(Type type, std::unique_ptr<ProcessThrottler::BackgroundActivityToken> activityToken)
        : m_type(type)
        , m_callbackID(generateCallbackID())
        , m_activityToken(WTF::move(activityToken))
    {
    }

private:
    static uint64_t generateCallbackID()
    {
        ASSERT(RunLoop::isMain());
        static uint64_t uniqueCallbackID = 1;
        return uniqueCallbackID++;
    }

    Type m_type;
    uint64_t m_callbackID;
    std::unique_ptr<ProcessThrottler::BackgroundActivityToken> m_activityToken;
};

template<typename... T>
class GenericCallback : public CallbackBase {
public:
    typedef std::function<void (T..., Error)> CallbackFunction;

    static PassRefPtr<GenericCallback> create(CallbackFunction callback, std::unique_ptr<ProcessThrottler::BackgroundActivityToken> activityToken = nullptr)
    {
        return adoptRef(new GenericCallback(callback, WTF::move(activityToken)));
    }

    virtual ~GenericCallback()
    {
        ASSERT(!m_callback);
    }

    void performCallbackWithReturnValue(T... returnValue)
    {
        if (!m_callback)
            return;

        m_callback(returnValue..., Error::None);

        m_callback = nullptr;
    }

    void performCallback()
    {
        performCallbackWithReturnValue();
    }

    virtual void invalidate(Error error = Error::Unknown) override final
    {
        if (!m_callback)
            return;

        m_callback(typename std::remove_reference<T>::type()..., error);

        m_callback = nullptr;
    }

private:
    GenericCallback(CallbackFunction callback, std::unique_ptr<ProcessThrottler::BackgroundActivityToken> activityToken)
        : CallbackBase(type(), WTF::move(activityToken))
        , m_callback(callback)
    {
    }

    friend class CallbackBase;
    static Type type()
    {
        static TypeTag tag;
        return &tag;
    }

    CallbackFunction m_callback;
};

template<typename APIReturnValueType, typename InternalReturnValueType = typename APITypeInfo<APIReturnValueType>::ImplType>
static typename GenericCallback<InternalReturnValueType>::CallbackFunction toGenericCallbackFunction(void* context, void (*callback)(APIReturnValueType, WKErrorRef, void*))
{
    return [context, callback](InternalReturnValueType returnValue, CallbackBase::Error error) {
        callback(toAPI(returnValue), error != CallbackBase::Error::None ? toAPI(API::Error::create().get()) : 0, context);
    };
}

typedef GenericCallback<> VoidCallback;
typedef GenericCallback<const Vector<WebCore::IntRect>&, double> ComputedPagesCallback;
typedef GenericCallback<const ShareableBitmap::Handle&> ImageCallback;

template<typename T>
void invalidateCallbackMap(HashMap<uint64_t, T>& callbackMap, CallbackBase::Error error)
{
    Vector<T> callbacks;
    copyValuesToVector(callbackMap, callbacks);
    for (auto& callback : callbacks)
        callback->invalidate(error);

    callbackMap.clear();
}

class CallbackMap {
public:
    uint64_t put(PassRefPtr<CallbackBase> callback)
    {
        ASSERT(!m_map.contains(callback->callbackID()));

        uint64_t callbackID = callback->callbackID();
        m_map.set(callbackID, callback);
        return callbackID;
    }

    template<unsigned I, typename T, typename... U>
    struct GenericCallbackType {
        typedef typename GenericCallbackType<I - 1, U..., T>::type type;
    };

    template<typename... U>
    struct GenericCallbackType<1, CallbackBase::Error, U...> {
        typedef GenericCallback<U...> type;
    };

    template<typename... T>
    uint64_t put(std::function<void (T...)> function, std::unique_ptr<ProcessThrottler::BackgroundActivityToken> activityToken)
    {
        auto callback = GenericCallbackType<sizeof...(T), T...>::type::create(WTF::move(function), WTF::move(activityToken));
        return put(callback);
    }

    template<class T>
    RefPtr<T> take(uint64_t callbackID)
    {
        RefPtr<CallbackBase> base = m_map.take(callbackID);
        if (!base)
            return nullptr;

        return adoptRef(base.release().leakRef()->as<T>());
    }

    void invalidate(CallbackBase::Error error)
    {
        invalidateCallbackMap(m_map, error);
    }

private:
    HashMap<uint64_t, RefPtr<CallbackBase>> m_map;
};

} // namespace WebKit

#endif // GenericCallback_h
