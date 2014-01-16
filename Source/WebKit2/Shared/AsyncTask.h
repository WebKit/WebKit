/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef AsyncTask_h
#define AsyncTask_h

#include <WebCore/CrossThreadCopier.h>
#include <functional>

namespace WebKit {

class AsyncTask {
    WTF_MAKE_NONCOPYABLE(AsyncTask);
public:
    virtual ~AsyncTask() { }

    virtual void performTask() = 0;

protected:
    explicit AsyncTask() { }
};

template <typename T, typename... Arguments>
class AsyncTaskImpl final : public AsyncTask {
public:
    AsyncTaskImpl(T* callee, void (T::*method)(Arguments...), Arguments&&... arguments)
    {
        m_taskFunction = [callee, method, arguments...]() {
            (callee->*method)(arguments...);
        };
    }

    virtual ~AsyncTaskImpl()
    {
    }

private:
    virtual void performTask() override
    {
        m_taskFunction();
    }

    std::function<void()> m_taskFunction;
};

template<typename T>
std::unique_ptr<AsyncTask> createAsyncTask(
    T& callee,
    void (T::*method)())
{
    return std::make_unique<AsyncTaskImpl<T>>(&callee, method);
}

template<typename T, typename P1, typename MP1>
std::unique_ptr<AsyncTask> createAsyncTask(
    T& callee,
    void (T::*method)(MP1),
    const P1& parameter1)
{
    return std::make_unique<AsyncTaskImpl<T, MP1>>(
        &callee,
        method,
        WebCore::CrossThreadCopier<P1>::copy(parameter1));
}

template<typename T, typename P1, typename MP1, typename P2, typename MP2>
std::unique_ptr<AsyncTask> createAsyncTask(
    T& callee,
    void (T::*method)(MP1, MP2),
    const P1& parameter1,
    const P2& parameter2)
{
    return std::make_unique<AsyncTaskImpl<T, MP1, MP2>>(
        &callee,
        method,
        WebCore::CrossThreadCopier<P1>::copy(parameter1),
        WebCore::CrossThreadCopier<P2>::copy(parameter2));

}

template<typename T, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3>
std::unique_ptr<AsyncTask> createAsyncTask(
    T& callee,
    void (T::*method)(MP1, MP2, MP3),
    const P1& parameter1,
    const P2& parameter2,
    const P3& parameter3)
{
    return std::make_unique<AsyncTaskImpl<T, MP1, MP2, MP3>>(
        &callee,
        method,
        WebCore::CrossThreadCopier<P1>::copy(parameter1),
        WebCore::CrossThreadCopier<P2>::copy(parameter2),
        WebCore::CrossThreadCopier<P3>::copy(parameter3));
}

} // namespace WebKit

#endif // AsyncTask_h
