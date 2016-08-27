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

#include "AudioBuffer.h"
#include "Document.h"
#include "JSAudioBuffer.h"
#include "JSAudioContext.h"
#include "JSDOMBinding.h"
#include "JSDOMPromise.h"
#include "JSOfflineAudioContext.h"
#include "OfflineAudioContext.h"
#include <runtime/ArrayBuffer.h>
#include <runtime/Error.h>
#include <runtime/JSArrayBuffer.h>

using namespace JSC;

namespace WebCore {

EncodedJSValue JSC_HOST_CALL constructJSAudioContext(ExecState& exec)
{
    DOMConstructorObject* jsConstructor = jsCast<DOMConstructorObject*>(exec.callee());
    ASSERT(jsConstructor);

    ScriptExecutionContext* scriptExecutionContext = jsConstructor->scriptExecutionContext();
    if (!scriptExecutionContext)
        return throwConstructorScriptExecutionContextUnavailableError(exec, "AudioContext");
    ASSERT(scriptExecutionContext->isDocument());

    Document& document = downcast<Document>(*scriptExecutionContext);

    RefPtr<AudioContext> audioContext;

    if (!exec.argumentCount()) {
        // Constructor for default AudioContext which talks to audio hardware.
        ExceptionCode ec = 0;
        audioContext = AudioContext::create(document, ec);
        if (ec) {
            setDOMException(&exec, ec);
            return JSValue::encode(JSValue());
        }
        if (!audioContext)
            return throwVMError(&exec, createSyntaxError(&exec, "audio resources unavailable for AudioContext construction"));
    } else
        return throwVMError(&exec, createSyntaxError(&exec, "Illegal AudioContext constructor"));

    return JSValue::encode(CREATE_DOM_WRAPPER(jsConstructor->globalObject(), AudioContext, audioContext.releaseNonNull()));
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
