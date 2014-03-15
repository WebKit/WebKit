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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
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
 *
 */

#ifndef AsyncRequest_h
#define AsyncRequest_h

#include <functional>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebKit {

class AsyncRequest : public RefCounted<AsyncRequest> {
public:
    virtual ~AsyncRequest();

    uint64_t requestID() { return m_requestID; }

    void setAbortHandler(std::function<void ()>);
    void requestAborted();
    template<typename... Arguments> void completeRequest(Arguments&&... arguments);

protected:
    explicit AsyncRequest(std::function<void ()> abortHandler);

    virtual void clearCompletionHandler() = 0;

    std::function<void ()> m_abortHandler;

private:
    uint64_t m_requestID;
};

template <typename... Arguments>
class AsyncRequestImpl final : public AsyncRequest {
public:
    static PassRefPtr<AsyncRequest> create(std::function<void (Arguments...)> completionHandler)
    {
        return adoptRef(new AsyncRequestImpl<Arguments...>(std::move(completionHandler), nullptr));
    }

    static PassRefPtr<AsyncRequest> create(std::function<void (Arguments...)> completionHandler, std::function<void ()> abortHandler)
    {
        return adoptRef(new AsyncRequestImpl<Arguments...>(std::move(completionHandler), std::move(abortHandler)));
    }

    virtual ~AsyncRequestImpl()
    {
        ASSERT(!m_completionHandler);
    }

    void completeRequest(Arguments&&... arguments)
    {
        m_completionHandler(std::forward<Arguments>(arguments)...);
        m_completionHandler = nullptr;
    }

private:
    AsyncRequestImpl(std::function<void (Arguments...)> completionHandler, std::function<void ()> abortHandler)
        : AsyncRequest(std::move(abortHandler))
        , m_completionHandler(std::move(completionHandler))
    {
        ASSERT(m_completionHandler);
    }

    virtual void clearCompletionHandler() override
    {
        m_completionHandler = nullptr;
    }

    std::function<void (Arguments...)> m_completionHandler;
};

template<typename... Arguments> void AsyncRequest::completeRequest(Arguments&&... arguments)
{
    AsyncRequestImpl<Arguments...>* request = static_cast<AsyncRequestImpl<Arguments...>*>(this);
    request->completeRequest(std::forward<Arguments>(arguments)...);
    m_abortHandler = nullptr;
}

} // namespace WebKit

#endif // AsyncRequest_h
