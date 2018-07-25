/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSXMLHttpRequest.h"

#include "JSBlob.h"
#include "JSDOMConvertBufferSource.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertJSON.h"
#include "JSDOMConvertNullable.h"
#include "JSDOMConvertStrings.h"
#include "JSDocument.h"


namespace WebCore {
using namespace JSC;

void JSXMLHttpRequest::visitAdditionalChildren(SlotVisitor& visitor)
{
    if (auto* upload = wrapped().optionalUpload())
        visitor.addOpaqueRoot(upload);

    if (auto* responseDocument = wrapped().optionalResponseXML())
        visitor.addOpaqueRoot(responseDocument);
}

JSValue JSXMLHttpRequest::response(ExecState& state) const
{
    auto cacheResult = [&] (JSValue value) -> JSValue {
        m_response.set(state.vm(), this, value);
        return value;
    };


    if (wrapped().responseCacheIsValid())
        return m_response.get();

    auto type = wrapped().responseType();

    switch (type) {
    case XMLHttpRequest::ResponseType::EmptyString:
    case XMLHttpRequest::ResponseType::Text: {
        auto scope = DECLARE_THROW_SCOPE(state.vm());
        return cacheResult(toJS<IDLNullable<IDLUSVString>>(state, scope, wrapped().responseText()));
    }
    default:
        break;
    }

    if (!wrapped().doneWithoutErrors())
        return cacheResult(jsNull());

    JSValue value;
    switch (type) {
    case XMLHttpRequest::ResponseType::EmptyString:
    case XMLHttpRequest::ResponseType::Text:
        ASSERT_NOT_REACHED();
        return jsUndefined();

    case XMLHttpRequest::ResponseType::Json:
        value = toJS<IDLJSON>(*globalObject()->globalExec(), wrapped().responseTextIgnoringResponseType());
        if (!value)
            value = jsNull();
        break;

    case XMLHttpRequest::ResponseType::Document: {
        auto document = wrapped().responseXML();
        ASSERT(!document.hasException());
        value = toJS<IDLInterface<Document>>(state, *globalObject(), document.releaseReturnValue());
        break;
    }

    case XMLHttpRequest::ResponseType::Blob:
        value = toJSNewlyCreated<IDLInterface<Blob>>(state, *globalObject(), wrapped().createResponseBlob());
        break;

    case XMLHttpRequest::ResponseType::Arraybuffer:
        value = toJS<IDLInterface<ArrayBuffer>>(state, *globalObject(), wrapped().createResponseArrayBuffer());
        break;
    }

    wrapped().didCacheResponse();
    return cacheResult(value);
}

} // namespace WebCore
