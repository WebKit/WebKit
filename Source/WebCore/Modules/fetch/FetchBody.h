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

#ifndef FetchBody_h
#define FetchBody_h

#if ENABLE(FETCH_API)

#include "Blob.h"
#include "FetchBodyConsumer.h"
#include "FetchLoader.h"
#include "FormData.h"
#include "JSDOMPromise.h"

namespace JSC {
class ExecState;
class JSValue;
};

namespace WebCore {

class DOMFormData;
class FetchBodyOwner;
class FetchHeaders;
class FetchResponseSource;
class ScriptExecutionContext;

class FetchBody {
public:
    void arrayBuffer(FetchBodyOwner&, DeferredWrapper&&);
    void blob(FetchBodyOwner&, DeferredWrapper&&);
    void json(FetchBodyOwner&, DeferredWrapper&&);
    void text(FetchBodyOwner&, DeferredWrapper&&);
    void formData(FetchBodyOwner&, DeferredWrapper&& promise) { promise.reject(0); }

#if ENABLE(STREAMS_API)
    void consumeAsStream(FetchBodyOwner&, FetchResponseSource&);
#endif

    bool isEmpty() const { return m_type == Type::None; }

    void updateContentType(FetchHeaders&);
    void setContentType(const String& contentType) { m_contentType = contentType; }
    String contentType() const { return m_contentType; }

    static FetchBody extract(ScriptExecutionContext&, JSC::ExecState&, JSC::JSValue);
    static FetchBody extractFromBody(FetchBody*);
    static FetchBody loadingBody() { return { Type::Loading }; }
    FetchBody() = default;

    void loadingFailed();
    void loadingSucceeded();

    RefPtr<FormData> bodyForInternalRequest(ScriptExecutionContext&) const;

    enum class Type { None, ArrayBuffer, ArrayBufferView, Blob, FormData, Text, Loading, Loaded, ReadableStream };
    Type type() const { return m_type; }

    FetchBodyConsumer& consumer() { return m_consumer; }

    void cleanConsumePromise() { m_consumePromise = Nullopt; }

private:
    FetchBody(Ref<Blob>&&);
    FetchBody(Ref<ArrayBuffer>&&);
    FetchBody(Ref<ArrayBufferView>&&);
    FetchBody(DOMFormData&, Document&);
    FetchBody(String&&);
    FetchBody(Type type) : m_type(type) { }

    void consume(FetchBodyOwner&, DeferredWrapper&&);

    Vector<uint8_t> extractFromText() const;
    void consumeArrayBuffer(DeferredWrapper&);
    void consumeArrayBufferView(DeferredWrapper&);
    void consumeText(DeferredWrapper&);
    void consumeBlob(FetchBodyOwner&, DeferredWrapper&&);

    Type m_type { Type::None };
    String m_contentType;

    // FIXME: Add support for URLSearchParams.
    RefPtr<Blob> m_blob;
    RefPtr<FormData> m_formData;
    RefPtr<ArrayBuffer> m_data;
    RefPtr<ArrayBufferView> m_dataView;
    String m_text;

    FetchBodyConsumer m_consumer { FetchBodyConsumer::Type::None };
    Optional<DeferredWrapper> m_consumePromise;
};

} // namespace WebCore

#endif // ENABLE(FETCH_API)

#endif // FetchBody_h
