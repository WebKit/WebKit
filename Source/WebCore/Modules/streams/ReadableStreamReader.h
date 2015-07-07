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

#ifndef ReadableStreamReader_h
#define ReadableStreamReader_h

#if ENABLE(STREAMS_API)

#include "ActiveDOMObject.h"
#include "ReadableStream.h"
#include "ScriptWrappable.h"
#include <functional>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

typedef int ExceptionCode;

// ReadableStreamReader implements access to ReadableStream from JavaScript.
// It basically allows access to the ReadableStream iff the ReadableStreamReader instance is the active reader
// of the ReadableStream.
// Most of this handling is happening in the custom JS binding of ReadableStreamReader.
// See https://streams.spec.whatwg.org/#reader-class for more information.
class ReadableStreamReader {
public:
    ReadableStreamReader(ReadableStream& stream)
        : m_stream(stream) { }

    void cancel(JSC::JSValue, ReadableStream::CancelPromise&&);
    void closed(ReadableStream::ClosedPromise&&);
    void read(ReadableStream::ReadPromise&&);
    void releaseLock(ExceptionCode&);

    void ref() { m_stream.ref(); }
    void deref() { m_stream.deref(); }

private:
    ReadableStream& m_stream;
};

}

#endif

#endif // ReadableStream_h
