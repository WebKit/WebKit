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
#include "ReadableStreamReader.h"

#if ENABLE(STREAMS_API)

#include "NotImplemented.h"
#include <wtf/RefCountedLeakCounter.h>

namespace WebCore {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, readableStreamReaderCounter, ("ReadableStreamReader"));

ReadableStreamReader::ReadableStreamReader(ReadableStream& stream)
    : ActiveDOMObject(stream.scriptExecutionContext())
{
#ifndef NDEBUG
    readableStreamReaderCounter.increment();
#endif
    suspendIfNeeded();
    ASSERT_WITH_MESSAGE(!stream.reader(), "A ReadableStream cannot be locked by two readers at the same time.");
    m_stream = &stream;
    stream.lock(*this);
}

ReadableStreamReader::~ReadableStreamReader()
{
#ifndef NDEBUG
    readableStreamReaderCounter.decrement();
#endif
    if (m_stream) {
        m_stream->releaseButKeepLocked();
        m_stream = nullptr;
    }
}

void ReadableStreamReader::closed(ClosedSuccessCallback, ClosedErrorCallback)
{
    notImplemented();    
}

const char* ReadableStreamReader::activeDOMObjectName() const
{
    return "ReadableStreamReader";
}

bool ReadableStreamReader::canSuspend() const
{
    // FIXME: We should try and do better here.
    return false;
}

}

#endif
