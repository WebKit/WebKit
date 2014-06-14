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

protected:
    explicit CallbackBase()
        : m_callbackID(generateCallbackID())
    {
    }

private:
    static uint64_t generateCallbackID()
    {
        ASSERT(RunLoop::isMain());
        static uint64_t uniqueCallbackID = 1;
        return uniqueCallbackID++;
    }

    uint64_t m_callbackID;
};

class VoidCallback : public CallbackBase {
public:
    typedef std::function<void (bool)> CallbackFunction;

    static PassRefPtr<VoidCallback> create(CallbackFunction callback)
    {
        return adoptRef(new VoidCallback(callback));
    }

    virtual ~VoidCallback()
    {
        ASSERT(!m_callback);
    }

    void performCallback()
    {
        if (!m_callback)
            return;

        m_callback(false);

        m_callback = nullptr;
    }
    
    void invalidate()
    {
        if (!m_callback)
            return;

        m_callback(true);

        m_callback = nullptr;
    }

private:
    VoidCallback(CallbackFunction callback)
        : m_callback(callback)
    {
    }

    CallbackFunction m_callback;
};

class VoidAPICallback : public CallbackBase {
public:
    typedef void (*CallbackFunction)(WKErrorRef, void*);

    static PassRefPtr<VoidCallback> create(void* context, CallbackFunction callback)
    {
        return VoidCallback::create([context, callback](bool error) {
            callback(error ? toAPI(API::Error::create().get()) : 0, context);
        });
    }
};

template<typename... T>
class GenericCallback : public CallbackBase {
public:
    typedef std::function<void (T..., Error)> CallbackFunction;

    static PassRefPtr<GenericCallback> create(CallbackFunction callback)
    {
        return adoptRef(new GenericCallback(callback));
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
    
    void invalidate(Error error = Error::Unknown)
    {
        if (!m_callback)
            return;

        m_callback(typename std::remove_reference<T>::type()..., error);

        m_callback = nullptr;
    }

private:
    GenericCallback(CallbackFunction callback)
        : m_callback(callback)
    {
    }

    CallbackFunction m_callback;
};

template<typename APIReturnValueType, typename InternalReturnValueType = typename APITypeInfo<APIReturnValueType>::ImplType>
class GenericAPICallback : public CallbackBase {
public:
    typedef void (*CallbackFunction)(APIReturnValueType, WKErrorRef, void*);

    static PassRefPtr<GenericCallback<InternalReturnValueType>> create(void* context, CallbackFunction callback)
    {
        return GenericCallback<InternalReturnValueType>::create([context, callback](InternalReturnValueType returnValue, CallbackBase::Error error) {
            callback(toAPI(returnValue), error != CallbackBase::Error::None ? toAPI(API::Error::create().get()) : 0, context);
        });
    }
};

typedef GenericCallback<const Vector<WebCore::IntRect>&, double> ComputedPagesCallback;
typedef GenericCallback<const ShareableBitmap::Handle&> ImageCallback;

template<typename T>
void invalidateCallbackMap(HashMap<uint64_t, T>& map, CallbackBase::Error error = CallbackBase::Error::Unknown)
{
    Vector<T> callbacksVector;
    copyValuesToVector(map, callbacksVector);
    for (size_t i = 0, size = callbacksVector.size(); i < size; ++i)
        callbacksVector[i]->invalidate();
    map.clear();
}

} // namespace WebKit

#endif // GenericCallback_h
