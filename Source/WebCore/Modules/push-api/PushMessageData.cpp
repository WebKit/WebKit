/*
 * Copyright (C) 2021-2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PushMessageData.h"

#include "Blob.h"
#include "JSDOMGlobalObject.h"
#include "TextResourceDecoder.h"
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSONObject.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(PushMessageData);

ExceptionOr<Ref<JSC::ArrayBuffer>> PushMessageData::arrayBuffer()
{
    RefPtr buffer = ArrayBuffer::tryCreate(m_data.span());
    if (!buffer)
        return Exception { ExceptionCode::OutOfMemoryError };
    return buffer.releaseNonNull();
}

Ref<Blob> PushMessageData::blob(ScriptExecutionContext& context)
{
    return Blob::create(&context, Vector<uint8_t> { m_data }, { });
}

ExceptionOr<Ref<JSC::Uint8Array>> PushMessageData::bytes()
{
    RefPtr view = Uint8Array::tryCreate(m_data.span());
    if (!view)
        return Exception { ExceptionCode::OutOfMemoryError };
    return view.releaseNonNull();
}

ExceptionOr<JSC::JSValue> PushMessageData::json(JSDOMGlobalObject& globalObject)
{
    JSC::JSLockHolder lock(&globalObject);

    auto value = JSC::JSONParse(&globalObject, text());
    if (!value)
        return Exception { ExceptionCode::SyntaxError, "JSON parsing failed"_s };

    return value;
}

String PushMessageData::text()
{
    return TextResourceDecoder::textFromUTF8(m_data.span());
}

} // namespace WebCore
