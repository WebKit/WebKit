/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "UserMessageHandler.h"

#if ENABLE(USER_MESSAGE_HANDLERS)

#include "Document.h"
#include "JSDOMConvertAny.h"
#include "JSDOMPromiseDeferred.h"
#include "LocalFrameInlines.h"
#include "SerializedScriptValue.h"
#include <JavaScriptCore/HeapCellInlines.h>
#include <JavaScriptCore/JSCJSValue.h>

namespace WebCore {

UserMessageHandler::UserMessageHandler(LocalFrame& frame, const UserMessageHandlerDescriptor& descriptor)
    : FrameDestructionObserver(&frame)
    , m_descriptor(descriptor)
{
}

UserMessageHandler::~UserMessageHandler() = default;

static bool passesSameOriginCheck(JSC::JSGlobalObject& globalObject, RefPtr<LocalFrame> frame)
{
    if (!frame)
        return false;
    RefPtr document = frame->document();
    if (!document)
        return false;
    Ref frameSecurityOrigin = document->securityOrigin();
    if (!globalObject.inherits<JSDOMGlobalObject>())
        return false;
    RefPtr scriptExecutionContext = uncheckedDowncast<JSDOMGlobalObject>(globalObject).scriptExecutionContext();
    if (!scriptExecutionContext)
        return false;
    RefPtr securityOrigin = scriptExecutionContext->securityOrigin();
    if (!securityOrigin)
        return false;
    return securityOrigin->isSameOriginAs(frameSecurityOrigin);
}

void UserMessageHandler::postMessage(JSC::JSGlobalObject& globalObject, JSC::JSValue value, Ref<DeferredPromise>&& promise)
{
    // Check to see if the descriptor has been removed. This can happen if the host application has
    // removed the named message handler at the WebKit2 API level.
    RefPtr descriptor = m_descriptor;
    if (!descriptor)
        return promise->reject(Exception { ExceptionCode::InvalidAccessError });

    if (!passesSameOriginCheck(globalObject, m_frame.get()))
        return promise->reject(Exception { ExceptionCode::InvalidAccessError, "Failed same-origin check."_s });

    descriptor->didPostMessage(*this, globalObject, value, [promise = WTF::move(promise)](JSC::JSValue result, const String& errorMessage) {
        if (errorMessage.isNull())
            return promise->resolveWithJSValue(result);

        auto* globalObject = promise->globalObject();
        if (!globalObject)
            return;
        JSC::JSLockHolder lock(globalObject);
        promise->reject<IDLAny>(JSC::createError(globalObject, errorMessage));
    });
}

ExceptionOr<JSC::JSValue> UserMessageHandler::postLegacySynchronousMessage(JSC::JSGlobalObject& globalObject, JSC::JSValue value)
{
    RefPtr descriptor = m_descriptor;
    if (!descriptor)
        return Exception { ExceptionCode::InvalidAccessError };

    if (!passesSameOriginCheck(globalObject, m_frame.get()))
        return Exception { ExceptionCode::InvalidAccessError, "Failed same-origin check."_s };

    return descriptor->didPostLegacySynchronousMessage(*this, globalObject, value);
}

} // namespace WebCore

#endif // ENABLE(USER_MESSAGE_HANDLERS)
