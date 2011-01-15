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

#include "WKAPICast.h"

#include "WebError.h"
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebKit {

template<typename APIReturnValueType, typename InternalReturnValueType = typename APITypeInfo<APIReturnValueType>::ImplType>
class GenericCallback : public RefCounted<GenericCallback<APIReturnValueType, InternalReturnValueType> > {
public:
    typedef void (*CallbackFunction)(APIReturnValueType, WKErrorRef, void*);

    static PassRefPtr<GenericCallback> create(void* context, CallbackFunction callback)
    {
        return adoptRef(new GenericCallback(context, callback));
    }

    ~GenericCallback()
    {
        ASSERT(!m_callback);
    }

    void performCallbackWithReturnValue(InternalReturnValueType returnValue)
    {
        ASSERT(m_callback);

        m_callback(toAPI(returnValue), 0, m_context);

        m_callback = 0;
    }
    
    void invalidate()
    {
        ASSERT(m_callback);

        RefPtr<WebError> error = WebError::create();
        m_callback(0, toAPI(error.get()), m_context);
        
        m_callback = 0;
    }

    uint64_t callbackID() const { return m_callbackID; }

private:
    static uint64_t generateCallbackID()
    {
        static uint64_t uniqueCallbackID = 1;
        return uniqueCallbackID++;
    }

    GenericCallback(void* context, CallbackFunction callback)
        : m_context(context)
        , m_callback(callback)
        , m_callbackID(generateCallbackID())
    {
    }

    void* m_context;
    CallbackFunction m_callback;
    uint64_t m_callbackID;
};

template<typename T>
void invalidateCallbackMap(HashMap<uint64_t, T>& map)
{
    Vector<T> callbacksVector;
    copyValuesToVector(map, callbacksVector);
    for (size_t i = 0, size = callbacksVector.size(); i < size; ++i)
        callbacksVector[i]->invalidate();
    map.clear();
}

} // namespace WebKit

#endif // GenericCallback_h
