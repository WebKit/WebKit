/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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
 */

#pragma once

#include "InternalReadableStream.h"
#include <JavaScriptCore/Strong.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class InternalReadableStream;
class JSDOMGlobalObject;
class ReadableStreamSource;

class ReadableStream : public RefCounted<ReadableStream> {
public:
    static ExceptionOr<Ref<ReadableStream>> create(JSC::JSGlobalObject&, std::optional<JSC::Strong<JSC::JSObject>>&&, std::optional<JSC::Strong<JSC::JSObject>>&&);
    static ExceptionOr<Ref<ReadableStream>> create(JSDOMGlobalObject&, Ref<ReadableStreamSource>&&);
    static Ref<ReadableStream> create(Ref<InternalReadableStream>&&);

    ~ReadableStream() = default;

    void lock() { m_internalReadableStream->lock(); }
    bool isLocked() const { return m_internalReadableStream->isLocked(); }
    bool isDisturbed() const { return m_internalReadableStream->isDisturbed(); }
    void cancel(Exception&& exception) { m_internalReadableStream->cancel(WTFMove(exception)); }
    void pipeTo(ReadableStreamSink& sink) { m_internalReadableStream->pipeTo(sink); }
    ExceptionOr<Vector<Ref<ReadableStream>>> tee(bool shouldClone = false);

    InternalReadableStream& internalReadableStream() { return m_internalReadableStream.get(); }

protected:
    static ExceptionOr<Ref<ReadableStream>> createFromJSValues(JSC::JSGlobalObject&, JSC::JSValue, JSC::JSValue);
    static ExceptionOr<Ref<InternalReadableStream>> createInternalReadableStream(JSDOMGlobalObject&, Ref<ReadableStreamSource>&&);
    explicit ReadableStream(Ref<InternalReadableStream>&&);

private:
    Ref<InternalReadableStream> m_internalReadableStream;
};

} // namespace WebCore
