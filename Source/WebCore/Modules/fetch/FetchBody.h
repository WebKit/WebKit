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
#include "DOMFormData.h"
#include "FetchLoader.h"
#include "JSDOMPromise.h"

namespace JSC {
class ExecState;
class JSValue;
};

namespace WebCore {

class FetchBodyOwner;
class FormData;

class FetchBody {
public:
    void arrayBuffer(FetchBodyOwner&, DeferredWrapper&&);
    void blob(FetchBodyOwner&, DeferredWrapper&&);
    void json(FetchBodyOwner&, DeferredWrapper&&);
    void text(FetchBodyOwner&, DeferredWrapper&&);
    void formData(FetchBodyOwner&, DeferredWrapper&& promise) { promise.reject<ExceptionCode>(0); }

    bool isDisturbed() const { return m_isDisturbed; }
    bool isEmpty() const { return m_type == Type::None; }

    void setMimeType(const String& mimeType) { m_mimeType = mimeType; }
    String mimeType() const { return m_mimeType; }

    static FetchBody extract(JSC::ExecState&, JSC::JSValue);
    static FetchBody extractFromBody(FetchBody*);
    static FetchBody loadingBody() { return { Type::Loading }; }
    FetchBody() = default;

    void loadingFailed();
    void loadedAsArrayBuffer(RefPtr<ArrayBuffer>&&);
    void loadedAsText(String&&);

    RefPtr<FormData> bodyForInternalRequest() const;

private:
    enum class Type { None, ArrayBuffer, Loading, Text, Blob, FormData };

    FetchBody(Ref<Blob>&&);
    FetchBody(Ref<DOMFormData>&&);
    FetchBody(String&&);
    FetchBody(Type type) : m_type(type) { }

    struct Consumer {
        enum class Type { Text, Blob, JSON, ArrayBuffer };

        Type type;
        DeferredWrapper promise;
    };
    void consume(FetchBodyOwner&, Consumer::Type, DeferredWrapper&&);

    Vector<uint8_t> extractFromText() const;
    bool processIfEmptyOrDisturbed(Consumer::Type, DeferredWrapper&);
    void consumeArrayBuffer(Consumer::Type, DeferredWrapper&);
    void consumeText(Consumer::Type, DeferredWrapper&);
    void consumeBlob(FetchBodyOwner&, Consumer::Type, DeferredWrapper&&);
    static FetchLoader::Type loadingType(Consumer::Type);
    static void fulfillTextPromise(FetchBody::Consumer::Type, const String&, DeferredWrapper&);
    static void fulfillArrayBufferPromise(FetchBody::Consumer::Type, const String&, DeferredWrapper&);

    Type m_type { Type::None };
    String m_mimeType;
    bool m_isDisturbed { false };

    // FIXME: Add support for BufferSource and URLSearchParams.
    RefPtr<Blob> m_blob;
    RefPtr<DOMFormData> m_formData;
    RefPtr<ArrayBuffer> m_data;
    String m_text;

    Optional<Consumer> m_consumer;
};

} // namespace WebCore

#endif // ENABLE(FETCH_API)

#endif // FetchBody_h
