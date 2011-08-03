/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DispatchTask_h
#define DispatchTask_h

#if ENABLE(MEDIA_STREAM)

#include "ScriptExecutionContext.h"
#include <wtf/PassRefPtr.h>

namespace WebCore {

template <typename CallbackClass, typename SimpleDataType>
class SimpleDispatchTask : public ScriptExecutionContext::Task {
public:
    typedef void (CallbackClass::*Callback)(const SimpleDataType&);

    static PassOwnPtr<SimpleDispatchTask> create(PassRefPtr<CallbackClass> object, Callback callback, const SimpleDataType& data)
    {
        return adoptPtr(new SimpleDispatchTask(object, callback, data));
    }

    virtual void performTask(ScriptExecutionContext*)
    {
        (m_object.get()->*m_callback)(m_data);
    }

private:
    SimpleDispatchTask(PassRefPtr<CallbackClass> object, Callback callback, const SimpleDataType& data)
        : m_object(object)
        , m_callback(callback)
        , m_data(data) { }

    RefPtr<CallbackClass> m_object;
    Callback m_callback;
    SimpleDataType m_data;
};

template <typename CallbackClass, typename SimpleDataType, typename RefCountedDataType>
class DispatchTask : public ScriptExecutionContext::Task {
public:
    typedef void (CallbackClass::*Callback)(const SimpleDataType&, PassRefPtr<RefCountedDataType>);

    static PassOwnPtr<DispatchTask> create(PassRefPtr<CallbackClass> object, Callback callback, const SimpleDataType& arg1, PassRefPtr<RefCountedDataType> arg2)
    {
        return adoptPtr(new DispatchTask(object, callback, arg1, arg2));
    }

    virtual void performTask(ScriptExecutionContext*)
    {
        (m_object.get()->*m_callback)(m_arg1, m_arg2);
    }

private:
    DispatchTask(PassRefPtr<CallbackClass> object, Callback callback, const SimpleDataType& arg1, PassRefPtr<RefCountedDataType> arg2)
        : m_object(object)
        , m_callback(callback)
        , m_arg1(arg1)
        , m_arg2(arg2) { }

    RefPtr<CallbackClass> m_object;
    Callback m_callback;
    SimpleDataType m_arg1;
    RefPtr<RefCountedDataType> m_arg2;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // DispatchTask_h

