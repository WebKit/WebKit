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

#include "config.h"
#include "ReadableStream.h"

#if ENABLE(STREAMS_API)

#include "NotImplemented.h"
#include "ReadableStreamReader.h"
#include <runtime/JSCJSValueInlines.h>
#include <wtf/RefCountedLeakCounter.h>

namespace WebCore {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, readableStreamCounter, ("ReadableStream"));

ReadableStream::ReadableStream(ScriptExecutionContext& scriptExecutionContext, Ref<ReadableStreamSource>&& source)
    : ActiveDOMObject(&scriptExecutionContext)
    , m_source(WTF::move(source))
{
#ifndef NDEBUG
    readableStreamCounter.increment();
#endif
    suspendIfNeeded();
}

ReadableStream::~ReadableStream()
{
#ifndef NDEBUG
    readableStreamCounter.decrement();
#endif
}

void ReadableStream::clearCallbacks()
{
    m_closedSuccessCallback = nullptr;
    m_closedFailureCallback = nullptr;

    m_readRequests.clear();
}

void ReadableStream::changeStateToClosed()
{
    if (m_state != State::Readable)
        return;
    m_state = State::Closed;

    if (m_reader)
        m_releasedReaders.append(WTF::move(m_reader));

    if (m_closedSuccessCallback)
        m_closedSuccessCallback();

    for (auto& request : m_readRequests)
        request.endCallback();

    clearCallbacks();
}

void ReadableStream::changeStateToErrored()
{
    if (m_state != State::Readable)
        return;
    m_state = State::Errored;

    if (m_reader)
        m_releasedReaders.append(WTF::move(m_reader));

    JSC::JSValue error = this->error();
    if (m_closedFailureCallback)
        m_closedFailureCallback(error);

    for (auto& request : m_readRequests)
        request.failureCallback(error);

    clearCallbacks();
}

ReadableStreamReader& ReadableStream::getReader()
{
    ASSERT(!m_reader);

    std::unique_ptr<ReadableStreamReader> newReader = std::make_unique<ReadableStreamReader>(*this);
    ReadableStreamReader& reader = *newReader.get();

    if (m_state == State::Readable) {
        m_reader = WTF::move(newReader);
        return reader;
    }

    m_releasedReaders.append(WTF::move(newReader));
    return reader;
}

void ReadableStream::closed(ClosedSuccessCallback&& successCallback, FailureCallback&& failureCallback)
{
    if (m_state == State::Closed) {
        successCallback();
        return;
    }
    if (m_state == State::Errored) {
        failureCallback(error());
        return;
    }
    m_closedSuccessCallback = WTF::move(successCallback);
    m_closedFailureCallback = WTF::move(failureCallback);
}

void ReadableStream::read(ReadSuccessCallback&& successCallback, ReadEndCallback&& endCallback, FailureCallback&& failureCallback)
{
    if (m_state == State::Closed) {
        endCallback();
        return;
    }
    if (m_state == State::Errored) {
        failureCallback(error());
        return;
    }
    if (hasValue()) {
        successCallback(read());
        return;
    }
    m_readRequests.append({ WTF::move(successCallback), WTF::move(endCallback), WTF::move(failureCallback) });
    // FIXME: We should try to pull.
}

void ReadableStream::start()
{
    notImplemented();
}

const char* ReadableStream::activeDOMObjectName() const
{
    return "ReadableStream";
}

bool ReadableStream::canSuspendForPageCache() const
{
    // FIXME: We should try and do better here.
    return false;
}

}

#endif
