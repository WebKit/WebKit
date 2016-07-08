/*
 * Copyright (C) 2015 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSMediaSession.h"

#if ENABLE(MEDIA_SESSION)

#include "ExceptionCode.h"
#include "JSDOMBinding.h"
#include "MediaSession.h"
#include <runtime/Error.h>
#include <runtime/JSString.h>
#include <wtf/GetPtr.h>

using namespace JSC;

namespace WebCore {

EncodedJSValue JSC_HOST_CALL constructJSMediaSession(ExecState& exec)
{
    auto* castedThis = jsCast<DOMConstructorObject*>(exec.callee());

    auto* context = castedThis->scriptExecutionContext();
    if (!context)
        return throwConstructorDocumentUnavailableError(exec, "MediaSession");

    String kind;
    if (exec.argumentCount() > 0) {
        JSString* kindString = exec.uncheckedArgument(0).toString(&exec);
        if (UNLIKELY(exec.hadException()))
            return JSValue::encode(jsUndefined());
        kind = kindString->value(&exec);
        if (kind != "content" && kind != "transient" && kind != "transient-solo" && kind != "ambient")
            return throwArgumentMustBeEnumError(exec, 0, "kind", "MediaSession", nullptr, "\"content\", \"transient\", \"transient-solo\", \"ambient\"");
    } else
        kind = "content";

    return JSValue::encode(toJSNewlyCreated(&exec, castedThis->globalObject(), MediaSession::create(*context, kind)));
}

} // namespace WebCore

#endif
