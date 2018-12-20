/*
 * Copyright (C) 2016 Canon Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Canon Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(STREAMS_API)

#include "JSDOMPromiseDeferred.h"
#include "ReadableStreamDefaultController.h"
#include <wtf/Optional.h>

namespace WebCore {

class ReadableStreamSource : public RefCounted<ReadableStreamSource> {
public:
    virtual ~ReadableStreamSource() = default;

    void start(ReadableStreamDefaultController&&, DOMPromiseDeferred<void>&&);
    void pull(DOMPromiseDeferred<void>&&);
    void cancel(JSC::JSValue);

    bool isPulling() const { return !!m_promise; }

protected:
    ReadableStreamDefaultController& controller() { return m_controller.value(); }
    const ReadableStreamDefaultController& controller() const { return m_controller.value(); }

    void startFinished();
    void pullFinished();
    void cancelFinished();
    void clean();

    virtual void setActive() = 0;
    virtual void setInactive() = 0;

    virtual void doStart() = 0;
    virtual void doPull() = 0;
    virtual void doCancel() = 0;

private:
    Optional<DOMPromiseDeferred<void>> m_promise;
    Optional<ReadableStreamDefaultController> m_controller;
};

inline void ReadableStreamSource::start(ReadableStreamDefaultController&& controller, DOMPromiseDeferred<void>&& promise)
{
    ASSERT(!m_promise);
    m_promise = WTFMove(promise);
    m_controller = WTFMove(controller);

    setActive();
    doStart();
}

inline void ReadableStreamSource::pull(DOMPromiseDeferred<void>&& promise)
{
    ASSERT(!m_promise);
    ASSERT(m_controller);

    m_promise = WTFMove(promise);

    setActive();
    doPull();
}

inline void ReadableStreamSource::startFinished()
{
    ASSERT(m_promise);
    std::exchange(m_promise, WTF::nullopt).value().resolve();
    setInactive();
}

inline void ReadableStreamSource::pullFinished()
{
    ASSERT(m_promise);
    std::exchange(m_promise, WTF::nullopt).value().resolve();
    setInactive();
}

inline void ReadableStreamSource::cancel(JSC::JSValue)
{
    clean();
    doCancel();
}

inline void ReadableStreamSource::clean()
{
    if (m_promise) {
        m_promise = WTF::nullopt;
        setInactive();
    }
}

} // namespace WebCore

#endif // ENABLE(STREAMS_API)
