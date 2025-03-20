/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "JavaScriptEvaluationResult.h"

#include "APIArray.h"
#include "APIDictionary.h"
#include "APISerializedScriptValue.h"
#include <WebCore/ExceptionDetails.h>
#include <WebCore/SerializedScriptValue.h>

#if PLATFORM(COCOA)
#include "CoreIPCNumber.h"
#endif

namespace WebKit {

#if !PLATFORM(COCOA)
Ref<API::SerializedScriptValue> JavaScriptEvaluationResult::legacySerializedScriptValue() const
{
    return API::SerializedScriptValue::createFromWireBytes(Vector(wireBytes()));
}

WKRetainPtr<WKTypeRef> JavaScriptEvaluationResult::toWK()
{
    return API::SerializedScriptValue::deserializeWK(legacySerializedScriptValue()->internalRepresentation());
}

JSValueRef JavaScriptEvaluationResult::toJS(JSGlobalContextRef context)
{
    Ref serializedScriptValue = API::SerializedScriptValue::createFromWireBytes(wireBytes());
    return serializedScriptValue->internalRepresentation().deserialize(context, nullptr);
}

JavaScriptEvaluationResult::JavaScriptEvaluationResult(JSGlobalContextRef context, JSValueRef value)
    : m_valueFromJS(WebCore::SerializedScriptValue::create(context, value, nullptr))
    , m_wireBytes(m_valueFromJS ? m_valueFromJS->wireBytes().span() : std::span<const uint8_t>())
{
}
#endif

JavaScriptEvaluationResult::JavaScriptEvaluationResult(JavaScriptEvaluationResult&&) = default;

JavaScriptEvaluationResult& JavaScriptEvaluationResult::operator=(JavaScriptEvaluationResult&&) = default;

JavaScriptEvaluationResult::~JavaScriptEvaluationResult() = default;

} // namespace WebKit
