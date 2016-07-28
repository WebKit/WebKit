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

#include "JSDOMPromise.h"
#include "ReadableStreamDefaultController.h"
#include <wtf/Optional.h>

namespace WebCore {

typedef int ExceptionCode;

class ReadableStreamSource : public RefCounted<ReadableStreamSource> {
public:
    virtual ~ReadableStreamSource() { }

    typedef DOMPromise<std::nullptr_t> Promise;

    virtual void firstReadCallback() { }
    void start(ReadableStreamDefaultController&&, Promise&&);
    void cancel(JSC::JSValue);

    bool isStarting() const { return !!m_startPromise; }

protected:
    ReadableStreamDefaultController& controller() { return m_controller.value(); }
    const ReadableStreamDefaultController& controller() const { return m_controller.value(); }

    void startFinished();
    void cancelFinished();
    void clean();

    virtual void setActive() = 0;
    virtual void setInactive() = 0;

    virtual void doStart() { startFinished(); }
    virtual void doCancel() { }

private:
    Optional<Promise> m_startPromise;
    Optional<ReadableStreamDefaultController> m_controller;
};

inline void ReadableStreamSource::start(ReadableStreamDefaultController&& controller, Promise&& promise)
{
    m_startPromise = WTFMove(promise);
    m_controller = WTFMove(controller);

    setActive();
    doStart();
}

inline void ReadableStreamSource::startFinished()
{
    ASSERT(m_startPromise);
    std::exchange(m_startPromise, Nullopt).value().resolve(nullptr);
    setInactive();
}

inline void ReadableStreamSource::cancel(JSC::JSValue)
{
    clean();
    doCancel();
}

inline void ReadableStreamSource::clean()
{
    if (m_startPromise) {
        m_startPromise = Nullopt;
        setInactive();
    }
}

} // namespace WebCore

#endif // ENABLE(STREAMS_API)
