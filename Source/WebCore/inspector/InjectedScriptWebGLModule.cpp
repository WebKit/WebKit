/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(INSPECTOR) && ENABLE(WEBGL)

#include "InjectedScriptWebGLModule.h"

#include "InjectedScript.h"
#include "InjectedScriptManager.h"
#include "InjectedScriptWebGLModuleSource.h"
#include "ScriptFunctionCall.h"
#include "ScriptObject.h"

namespace WebCore {

InjectedScriptWebGLModule::InjectedScriptWebGLModule()
    : InjectedScriptModule("InjectedScriptWebGLModule")
{
}

InjectedScriptWebGLModule InjectedScriptWebGLModule::moduleForState(InjectedScriptManager* injectedScriptManager, ScriptState* scriptState)
{
    InjectedScriptWebGLModule result;
    result.ensureInjected(injectedScriptManager, scriptState);
    return result;
}

String InjectedScriptWebGLModule::source() const
{
    return String(reinterpret_cast<const char*>(InjectedScriptWebGLModuleSource_js), sizeof(InjectedScriptWebGLModuleSource_js));
}

ScriptObject InjectedScriptWebGLModule::wrapWebGLContext(const ScriptObject& glContext)
{
    ScriptFunctionCall function(injectedScriptObject(), "wrapWebGLContext");
    function.appendArgument(glContext);
    bool hadException = false;
    ScriptValue resultValue = callFunctionWithEvalEnabled(function, hadException);
    if (hadException || resultValue.hasNoValue() || !resultValue.isObject()) {
        ASSERT_NOT_REACHED();
        return ScriptObject();
    }
    return ScriptObject(glContext.scriptState(), resultValue);
}

void InjectedScriptWebGLModule::captureFrame(ErrorString* errorString, const String& contextId)
{
    ScriptFunctionCall function(injectedScriptObject(), "captureFrame");
    function.appendArgument(contextId);
    bool hadException = false;
    callFunctionWithEvalEnabled(function, hadException);
    ASSERT(!hadException);
    if (hadException)
        *errorString = "Internal error";
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(WEBGL)
