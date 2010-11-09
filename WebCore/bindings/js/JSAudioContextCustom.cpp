/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioContext.h"

#include "JSAudioContext.h"
#include <runtime/Error.h>

namespace WebCore {

JSC::EncodedJSValue JSC_HOST_CALL JSAudioContextConstructor::constructJSAudioContext(JSC::ExecState* exec)
{
    JSAudioContextConstructor* jsConstructor = static_cast<JSAudioContextConstructor*>(exec->callee());
    if (!jsConstructor)
      return throwError(exec, createReferenceError(exec, "AudioContext constructor callee is unavailable"));

    ScriptExecutionContext* scriptExecutionContext = jsConstructor->scriptExecutionContext();
    if (!scriptExecutionContext)
      return throwError(exec, createReferenceError(exec, "AudioContext constructor script execution context is unavailable"));
        
    if (!scriptExecutionContext->isDocument())
      return throwError(exec, createReferenceError(exec, "AudioContext constructor called in a script execution context which is not a document"));

    Document* document = static_cast<Document*>(scriptExecutionContext);

    RefPtr<AudioContext> context = AudioContext::create(document);
    return JSC::JSValue::encode(asObject(toJS(exec, jsConstructor->globalObject(), context.get())));
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
