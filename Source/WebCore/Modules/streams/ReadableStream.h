/*
 * Copyright (C) 2015 Canon Inc.
 * Copyright (C) 2015 Igalia S.L.
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

#ifndef ReadableStream_h
#define ReadableStream_h

#if ENABLE(STREAMS_API)

#include "ActiveDOMObject.h"
#include "JSDOMPromise.h"
#include "ScriptWrappable.h"
#include <functional>
#include <wtf/Deque.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace JSC {
class JSValue;
}

namespace WebCore {

class Dictionary;
class ReadableStreamReader;
class ScriptExecutionContext;

typedef int ExceptionCode;

// ReadableStream implements the core of the streams API ReadableStream functionality.
// It handles in particular the backpressure according the queue size.
// ReadableStream is using a ReadableStreamSource to get data in its queue.
// See https://streams.spec.whatwg.org/#rs
class ReadableStream : public ActiveDOMObject, public ScriptWrappable, public RefCounted<ReadableStream> {
public:
    enum class State {
        Readable,
        Closed,
        Errored
    };

    static RefPtr<ReadableStream> create(JSC::ExecState&, JSC::JSValue, const Dictionary&);
    virtual ~ReadableStream();

    ReadableStreamReader* getReader(ExceptionCode&);
    const ReadableStreamReader* reader() const { return m_reader.get(); }

    bool locked() const { return !!m_reader; }

    void releaseReader();
    bool hasReadPendingRequests() { return !m_readRequests.isEmpty(); }

    bool isErrored() const { return m_state == State::Errored; }
    bool isReadable() const { return m_state == State::Readable; }
    bool isCloseRequested() const { return m_closeRequested; }

    virtual JSC::JSValue error() = 0;

    void start();
    void changeStateToClosed();
    void changeStateToErrored();
    void finishPulling();
    void notifyCancelSucceeded();
    void notifyCancelFailed();

    typedef DOMPromise<std::nullptr_t, JSC::JSValue> CancelPromise;
    void cancel(JSC::JSValue, CancelPromise&&, ExceptionCode&);
    void cancelNoCheck(JSC::JSValue, CancelPromise&&);

    typedef DOMPromise<std::nullptr_t, JSC::JSValue> ClosedPromise;
    void closed(ClosedPromise&&);

    typedef DOMPromiseIteratorWithCallback<JSC::JSValue, JSC::JSValue> ReadPromise;
    void read(ReadPromise&&);

protected:
    explicit ReadableStream(ScriptExecutionContext&);

    bool resolveReadCallback(JSC::JSValue);
    void pull();

private:
    // ActiveDOMObject API.
    const char* activeDOMObjectName() const override;
    bool canSuspendForPageCache() const override;

    void clearCallbacks();
    void close();

    virtual bool hasEnoughValues() const = 0;
    virtual bool hasValue() const = 0;
    virtual JSC::JSValue read() = 0;
    virtual bool doPull() = 0;
    virtual bool doCancel(JSC::JSValue) = 0;

    std::unique_ptr<ReadableStreamReader> m_reader;
    Vector<std::unique_ptr<ReadableStreamReader>> m_releasedReaders;

    Optional<CancelPromise> m_cancelPromise;
    Optional<ClosedPromise> m_closedPromise;

    Deque<ReadPromise> m_readRequests;

    bool m_isStarted { false };
    bool m_isPulling { false };
    bool m_shouldPullAgain { false };
    bool m_closeRequested { false };
    State m_state { State::Readable };
};

}

#endif

#endif // ReadableStream_h
