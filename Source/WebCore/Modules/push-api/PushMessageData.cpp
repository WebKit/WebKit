/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(PushMessageData);

ExceptionOr<RefPtr<JSC::ArrayBuffer>> PushMessageData::arrayBuffer()
{
    auto buffer = ArrayBuffer::tryCreate(m_data.data(), m_data.size());
    if (!buffer)
        return Exception { ExceptionCode::OutOfMemoryError };
    return buffer;
}

RefPtr<Blob> PushMessageData::blob(ScriptExecutionContext& context)
{
    return Blob::create(&context, Vector<uint8_t> { m_data }, { });
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
    return TextResourceDecoder::textFromUTF8(m_data.data(), m_data.size());
}

} // namespace WebCore
