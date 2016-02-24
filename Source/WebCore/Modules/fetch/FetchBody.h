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
#include "JSDOMPromise.h"

namespace JSC {
class ExecState;
class JSValue;
};

namespace WebCore {

class Dictionary;
typedef int ExceptionCode;

class FetchBody {
public:
    typedef DOMPromise<Vector<unsigned char>, ExceptionCode> ArrayBufferPromise;
    void arrayBuffer(ArrayBufferPromise&&);

    typedef DOMPromise<RefPtr<DOMFormData>, ExceptionCode> FormDataPromise;
    void formData(FormDataPromise&&);

    typedef DOMPromise<RefPtr<Blob>, ExceptionCode> BlobPromise;
    void blob(BlobPromise&&);

    typedef DOMPromise<JSC::JSValue, ExceptionCode> JSONPromise;
    void json(JSC::ExecState&, JSONPromise&&);

    typedef DOMPromise<String, ExceptionCode> TextPromise;
    void text(TextPromise&&);

    bool isDisturbed() const { return m_isDisturbed; }
    bool isEmpty() const { return m_type == Type::None; }

    void setMimeType(const String& mimeType) { m_mimeType = mimeType; }
    String mimeType() const { return m_mimeType; }

    static FetchBody fromJSValue(JSC::ExecState&, JSC::JSValue);
    static FetchBody fromRequestBody(FetchBody*);
    static FetchBody empty() { return FetchBody(); }

private:
    template<typename T> bool processIfEmptyOrDisturbed(DOMPromise<T, ExceptionCode>&);

    enum class Type { None, Text, Blob, FormData };

    FetchBody(Ref<Blob>&&);
    FetchBody(Ref<DOMFormData>&&);
    FetchBody(String&&);
    FetchBody() { }

    Type m_type = Type::None;
    String m_mimeType;
    bool m_isDisturbed = false;

    // FIXME: Add support for BufferSource and URLSearchParams.
    RefPtr<Blob> m_blob;
    RefPtr<DOMFormData> m_formData;
    String m_text;
};

} // namespace WebCore

#endif // ENABLE(FETCH_API)

#endif // FetchBody_h
