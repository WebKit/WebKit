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

#include "config.h"
#include "FetchBody.h"

#if ENABLE(FETCH_API)

#include "Dictionary.h"
#include "ExceptionCode.h"
#include "JSBlob.h"
#include "JSDOMFormData.h"
#include <runtime/JSONObject.h>

namespace WebCore {

FetchBody::FetchBody(Ref<Blob>&& blob)
    : m_type(Type::Blob)
    , m_mimeType(blob->type())
    , m_blob(WTFMove(blob))
{
}

FetchBody::FetchBody(Ref<DOMFormData>&& formData)
    : m_type(Type::FormData)
    , m_mimeType(ASCIILiteral("multipart/form-data"))
    , m_formData(WTFMove(formData))
{
    // FIXME: Handle the boundary parameter of multipart/form-data MIME type.
}

FetchBody::FetchBody(String&& text)
    : m_type(Type::Text)
    , m_mimeType(ASCIILiteral("text/plain;charset=UTF-8"))
    , m_text(WTFMove(text))
{
}

FetchBody FetchBody::extract(JSC::ExecState& state, JSC::JSValue value)
{
    if (value.inherits(JSBlob::info()))
        return FetchBody(*JSBlob::toWrapped(value));
    if (value.inherits(JSDOMFormData::info()))
        return FetchBody(*JSDOMFormData::toWrapped(value));
    if (value.isString())
        return FetchBody(value.toWTFString(&state));
    return { };
}

FetchBody FetchBody::extractFromBody(FetchBody* body)
{
    if (!body)
        return { };

    body->m_isDisturbed = true;
    return FetchBody(WTFMove(*body));
}

template<typename T> inline bool FetchBody::processIfEmptyOrDisturbed(DOMPromise<T, ExceptionCode>& promise)
{
    if (m_type == Type::None) {
        promise.resolve(T());
        return true;
    }

    if (m_isDisturbed) {
        promise.reject(TypeError);
        return true;
    }
    m_isDisturbed = true;
    return false;
}

void FetchBody::arrayBuffer(ArrayBufferPromise&& promise)
{
    if (processIfEmptyOrDisturbed(promise))
        return;

    if (m_type == Type::Text) {
        // FIXME: promise expects a Vector<unsigned char> that will be converted to an ArrayBuffer.
        // We should try to create a Vector<unsigned char> or an ArrayBuffer directly from m_text.
        CString data = m_text.utf8();
        Vector<unsigned char> value(data.length());
        memcpy(value.data(), data.data(), data.length());
        promise.resolve(WTFMove(value));
        return;
    }
    // FIXME: Support other types.
    promise.reject(0);
}

void FetchBody::formData(FormDataPromise&& promise)
{
    if (m_type == Type::None || m_isDisturbed) {
        promise.reject(TypeError);
        return;
    }
    m_isDisturbed = true;

    // FIXME: Support other types.
    promise.reject(0);
}

void FetchBody::blob(BlobPromise&& promise)
{
    if (processIfEmptyOrDisturbed(promise))
        return;

    // FIXME: Support other types.
    promise.reject(0);
}

void FetchBody::text(TextPromise&& promise)
{
    if (processIfEmptyOrDisturbed(promise))
        return;

    if (m_type == Type::Text) {
        promise.resolve(m_text);
        return;
    }
    // FIXME: Support other types.
    promise.reject(0);
}

void FetchBody::json(JSC::ExecState& state, JSONPromise&& promise)
{
    if (processIfEmptyOrDisturbed(promise))
        return;

    if (m_type == Type::Text) {
        JSC::JSValue value = JSC::JSONParse(&state, m_text);
        if (!value)
            promise.reject(SYNTAX_ERR);
        else
            promise.resolve(value);
        return;
    }
    // FIXME: Support other types.
    promise.reject(0);
}

}

#endif // ENABLE(FETCH_API)
