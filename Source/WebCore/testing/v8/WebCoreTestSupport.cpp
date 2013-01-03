/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "WebCoreTestSupport.h"

#include "Document.h"
#include "Frame.h"
#include "InternalSettings.h"
#include "Internals.h"
#include "ScriptExecutionContext.h"
#include "V8Internals.h"

#include <v8.h>

using namespace WebCore;

namespace WebCoreTestSupport {

void injectInternalsObject(v8::Local<v8::Context> context)
{
    v8::Context::Scope contextScope(context);
    v8::HandleScope scope;

    ScriptExecutionContext* scriptContext = getScriptExecutionContext();
    if (scriptContext->isDocument())
        context->Global()->Set(v8::String::New(Internals::internalsId), toV8(Internals::create(static_cast<Document*>(scriptContext))));
}

void resetInternalsObject(v8::Local<v8::Context> context)
{
    // This can happen if JavaScript is disabled in the main frame.
    if (context.IsEmpty())
        return;

    v8::Context::Scope contextScope(context);
    v8::HandleScope scope;

    ScriptExecutionContext* scriptContext = getScriptExecutionContext();
    ASSERT(scriptContext->isDocument());
    Page* page = static_cast<Document*>(scriptContext)->frame()->page();
    Internals::resetToConsistentState(page);
    InternalSettings::from(page)->resetToConsistentState();
}

}
