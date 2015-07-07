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

#include "ExceptionCode.h"
#include "ReadableJSStream.h"
#include "ReadableStreamReader.h"
#include "ScriptExecutionContext.h"
#include <runtime/JSCJSValueInlines.h>
#include <wtf/RefCountedLeakCounter.h>

namespace WebCore {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, readableStreamCounter, ("ReadableStream"));

RefPtr<ReadableStream> ReadableStream::create(JSC::ExecState& state, JSC::JSValue value, const Dictionary& strategy)
{
    return RefPtr<ReadableStream>(ReadableJSStream::create(state, value, strategy));
}

ReadableStream::ReadableStream(ScriptExecutionContext& scriptExecutionContext)
    : ActiveDOMObject(&scriptExecutionContext)
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
    m_closedPromise = Nullopt;
    m_readRequests.clear();
}

void ReadableStream::changeStateToClosed()
{
    ASSERT(!m_closeRequested);
    ASSERT(m_state != State::Errored);

    m_closeRequested = true;

    if (m_state != State::Readable || hasValue())
        return;
    close();
}

void ReadableStream::close()
{
    m_state = State::Closed;
    releaseReader();
}

void ReadableStream::releaseReader()
{
    if (m_closedPromise)
        m_closedPromise.value().resolve(nullptr);

    for (auto& request : m_readRequests)
        request.resolveEnd();

    clearCallbacks();
    if (m_reader)
        m_releasedReaders.append(WTF::move(m_reader));
}

void ReadableStream::changeStateToErrored()
{
    if (m_state != State::Readable)
        return;
    m_state = State::Errored;

    JSC::JSValue error = this->error();
    if (m_closedPromise)
        m_closedPromise.value().reject(error);

    for (auto& request : m_readRequests)
        request.reject(error);

    clearCallbacks();
    if (m_reader)
        releaseReader();
}

void ReadableStream::start()
{
    m_isStarted = true;
    pull();
}

void ReadableStream::pull()
{
    if (!m_isStarted || m_state == State::Closed || m_state == State::Errored || m_closeRequested)
        return;

    if (m_readRequests.isEmpty() && hasEnoughValues())
        return;

    if (m_isPulling) {
        m_shouldPullAgain = true;
        return;
    }

    m_isPulling = true;
    if (doPull()) {
        RefPtr<ReadableStream> protectedStream(this);
        scriptExecutionContext()->postTask([protectedStream](ScriptExecutionContext&) {
            protectedStream->finishPulling();
        });
    }
}

void ReadableStream::finishPulling()
{
    m_isPulling = false;
    if (m_shouldPullAgain) {
        m_shouldPullAgain = false;
        pull();
    }
}

ReadableStreamReader* ReadableStream::getReader(ExceptionCode& ec)
{
    if (locked()) {
        ec = TypeError;
        return nullptr;
    }
    ASSERT(!m_reader);

    std::unique_ptr<ReadableStreamReader> newReader = std::make_unique<ReadableStreamReader>(*this);
    ReadableStreamReader& reader = *newReader.get();

    if (m_state == State::Readable) {
        m_reader = WTF::move(newReader);
        return &reader;
    }

    m_releasedReaders.append(WTF::move(newReader));
    return &reader;
}

void ReadableStream::cancel(JSC::JSValue reason, CancelPromise&& promise, ExceptionCode& ec)
{
    if (locked()) {
        ec = TypeError;
        return;
    }
    cancelNoCheck(reason, WTF::move(promise));
}

void ReadableStream::cancelNoCheck(JSC::JSValue reason, CancelPromise&& promise)
{
    if (m_state == State::Closed) {
        promise.resolve(nullptr);
        return;
    }
    if (m_state == State::Errored) {
        promise.reject(error());
        return;
    }
    ASSERT(m_state == State::Readable);

    m_cancelPromise = WTF::move(promise);

    close();

    if (doCancel(reason))
        error() ? notifyCancelFailed() : notifyCancelSucceeded();
}

void ReadableStream::notifyCancelSucceeded()
{
    ASSERT(m_state == State::Closed);
    ASSERT(m_cancelPromise);

    m_cancelPromise.value().resolve(nullptr);
    m_cancelPromise = Nullopt;
}

void ReadableStream::notifyCancelFailed()
{
    ASSERT(m_state == State::Closed);
    ASSERT(m_cancelPromise);

    m_cancelPromise.value().reject(error());
    m_cancelPromise = Nullopt;
}

void ReadableStream::closed(ClosedPromise&& promise)
{
    if (m_state == State::Closed) {
        promise.resolve(nullptr);
        return;
    }
    if (m_state == State::Errored) {
        promise.reject(error());
        return;
    }
    m_closedPromise = WTF::move(promise);
}

void ReadableStream::read(ReadPromise&& readPromise)
{
    if (m_state == State::Closed) {
        readPromise.resolveEnd();
        return;
    }
    if (m_state == State::Errored) {
        readPromise.reject(error());
        return;
    }
    if (hasValue()) {
        readPromise.resolve(read());
        if (!m_closeRequested)
            pull();
        else if (!hasValue())
            close();
        return;
    }
    m_readRequests.append(WTF::move(readPromise));
    pull();
}

bool ReadableStream::resolveReadCallback(JSC::JSValue value)
{
    if (m_readRequests.isEmpty())
        return false;

    m_readRequests.takeFirst().resolve(value);
    return true;
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
