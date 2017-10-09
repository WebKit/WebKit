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

#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebKit {

class AsyncRequest : public RefCounted<AsyncRequest> {
public:
    virtual ~AsyncRequest();

    uint64_t requestID() { return m_requestID; }

    void setAbortHandler(WTF::Function<void ()>&&);
    void requestAborted();
    template<typename... Arguments> void completeRequest(Arguments&&... arguments);

protected:
    explicit AsyncRequest(WTF::Function<void ()>&& abortHandler);

    virtual void clearCompletionHandler() = 0;

    WTF::Function<void ()> m_abortHandler;

private:
    uint64_t m_requestID;
};

template <typename... Arguments>
class AsyncRequestImpl final : public AsyncRequest {
public:
    template<typename T> using ArgumentType = typename std::conditional<std::is_integral<T>::value, T, const T&>::type;

    static Ref<AsyncRequest> create(WTF::Function<void(ArgumentType<Arguments>...)>&& completionHandler)
    {
        return adoptRef(*new AsyncRequestImpl<Arguments...>(WTFMove(completionHandler), nullptr));
    }

    static Ref<AsyncRequest> create(WTF::Function<void(ArgumentType<Arguments>...)>&& completionHandler, WTF::Function<void()>&& abortHandler)
    {
        return adoptRef(*new AsyncRequestImpl<Arguments...>(WTFMove(completionHandler), WTFMove(abortHandler)));
    }

    virtual ~AsyncRequestImpl()
    {
        ASSERT(!m_completionHandler);
    }

    template<typename... RequestArguments>
    void completeRequest(RequestArguments&&... arguments)
    {
        m_completionHandler(std::forward<RequestArguments>(arguments)...);
        m_completionHandler = nullptr;
    }

private:
    AsyncRequestImpl(WTF::Function<void (ArgumentType<Arguments>...)>&& completionHandler, WTF::Function<void ()>&& abortHandler)
        : AsyncRequest(WTFMove(abortHandler))
        , m_completionHandler(WTFMove(completionHandler))
    {
        ASSERT(m_completionHandler);
    }

    void clearCompletionHandler() override
    {
        m_completionHandler = nullptr;
    }

    WTF::Function<void (ArgumentType<Arguments>...)> m_completionHandler;
};

template<typename... Arguments> void AsyncRequest::completeRequest(Arguments&&... arguments)
{
    auto* request = static_cast<AsyncRequestImpl<typename std::decay<Arguments>::type...>*>(this);
    request->completeRequest(std::forward<Arguments>(arguments)...);
    m_abortHandler = nullptr;
}

class AsyncRequestMap {
public:
    using RequestMap = HashMap<uint64_t, RefPtr<AsyncRequest>>;

    AsyncRequestMap()
#ifndef NDEBUG
        : m_lastRequestIDTaken(std::numeric_limits<uint64_t>::max())
#endif
    { }

    Ref<AsyncRequest> take(uint64_t requestID)
    {
#ifndef NDEBUG
        ASSERT_WITH_MESSAGE(requestID != m_lastRequestIDTaken, "Attempt to take the same AsyncRequest twice in a row. A background queue might have dispatched both an error callback and a success callback?");
        m_lastRequestIDTaken = requestID;
#endif

        RefPtr<AsyncRequest> request = m_requestMap.take(requestID);
        RELEASE_ASSERT(request);

        return adoptRef(*request.leakRef());
    }

    void add(uint64_t requestID, RefPtr<AsyncRequest>&& request)
    {
        m_requestMap.add(requestID, WTFMove(request));
    }

    void clear()
    {
        m_requestMap.clear();
    }

    typename RequestMap::ValuesIteratorRange values()
    {
        return m_requestMap.values();
    }

private:
    RequestMap m_requestMap;
#ifndef NDEBUG
    uint64_t m_lastRequestIDTaken;
#endif
};

} // namespace WebKit

#endif // AsyncRequest_h
