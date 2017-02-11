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

#include "Blob.h"
#include "DOMFormData.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Event.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "InspectorInstrumentation.h"
#include "JSBlob.h"
#include "JSDOMConvert.h"
#include "JSDOMFormData.h"
#include "JSDOMWindowCustom.h"
#include "JSDocument.h"
#include "JSEvent.h"
#include "JSEventListener.h"
#include "XMLHttpRequest.h"
#include <runtime/ArrayBuffer.h>
#include <runtime/Error.h>
#include <runtime/JSArrayBuffer.h>
#include <runtime/JSArrayBufferView.h>
#include <runtime/JSONObject.h>

using namespace JSC;

namespace WebCore {

void JSXMLHttpRequest::visitAdditionalChildren(SlotVisitor& visitor)
{
    if (XMLHttpRequestUpload* upload = wrapped().optionalUpload())
        visitor.addOpaqueRoot(upload);

    if (Document* responseDocument = wrapped().optionalResponseXML())
        visitor.addOpaqueRoot(responseDocument);
}

JSValue JSXMLHttpRequest::responseText(ExecState& state) const
{
    auto result = wrapped().responseText();

    if (UNLIKELY(result.hasException())) {
        auto& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        propagateException(state, scope, result.releaseException());
        return { };
    }

    auto resultValue = result.releaseReturnValue();
    if (resultValue.isNull())
        return jsNull();

    // See JavaScriptCore for explanation: Should be used for any string that is already owned by another
    // object, to let the engine know that collecting the JSString wrapper is unlikely to save memory.
    return jsOwnedString(&state, resultValue);
}

JSValue JSXMLHttpRequest::retrieveResponse(ExecState& state)
{
    auto type = wrapped().responseType();

    switch (type) {
    case XMLHttpRequest::ResponseType::EmptyString:
    case XMLHttpRequest::ResponseType::Text:
        return responseText(state);
    default:
        break;
    }

    if (!wrapped().doneWithoutErrors())
        return jsNull();

    JSValue value;
    switch (type) {
    case XMLHttpRequest::ResponseType::EmptyString:
    case XMLHttpRequest::ResponseType::Text:
        ASSERT_NOT_REACHED();
        return jsUndefined();

    case XMLHttpRequest::ResponseType::Json:
        value = JSONParse(&state, wrapped().responseTextIgnoringResponseType());
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
    return value;
}

} // namespace WebCore
