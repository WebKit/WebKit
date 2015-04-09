/*
 * Copyright (C) 2O15 Canon Inc. 2015
 * Copyright (C) 2015 Igalia S.L. 2015
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

#ifndef ReadableStreamJSSource_h
#define ReadableStreamJSSource_h

#if ENABLE(STREAMS_API)

#include "ReadableStream.h"
#include "ReadableStreamReader.h"
#include "ReadableStreamSource.h"
#include <heap/Strong.h>
#include <heap/StrongInlines.h>
#include <runtime/JSCJSValue.h>
#include <runtime/PrivateName.h>
#include <wtf/Ref.h>

namespace WebCore {

class JSReadableStream;

class ReadableStreamJSSource: public ReadableStreamSource {
public:
    static Ref<ReadableStreamJSSource> create(JSC::ExecState*);
    ~ReadableStreamJSSource() { }

    void start(JSC::ExecState*, JSReadableStream*);

private:
    ReadableStreamJSSource(JSC::ExecState*);

    // Object passed to constructor.
    JSC::Strong<JSC::JSObject> m_source;

    JSC::Strong<JSC::JSObject> m_controller;
};

class ReadableJSStream: public ReadableStream {
public:
    static Ref<ReadableJSStream> create(ScriptExecutionContext&, Ref<ReadableStreamJSSource>&&);
    virtual Ref<ReadableStreamReader> createReader() override;
private:
    ReadableJSStream(ScriptExecutionContext&, Ref<ReadableStreamJSSource>&&);
};

class ReadableJSStreamReader: public ReadableStreamReader {
public:
    static Ref<ReadableJSStreamReader> create(ReadableJSStream&);
private:
    ReadableJSStreamReader(ReadableJSStream&);
};

void setInternalSlotToObject(JSC::ExecState*, JSC::JSValue, JSC::PrivateName&, JSC::JSValue);
JSC::JSValue getInternalSlotFromObject(JSC::ExecState*, JSC::JSValue, JSC::PrivateName&);

} // namespace WebCore

#endif // ENABLE(STREAMS_API)

#endif // ReadableStreamJSSource_h
