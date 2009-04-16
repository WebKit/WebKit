/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "ScriptScope.h"

#include "ScriptState.h"

#include "Document.h"
#include "Frame.h"
#include "V8Binding.h"
#include "V8Proxy.h"

#include <v8.h>

namespace WebCore {

ScriptScope::ScriptScope(ScriptState* scriptState, bool reportExceptions)
    : m_context(V8Proxy::GetContext(scriptState->frame()))
    , m_scope(m_context)
    , m_scriptState(scriptState)
    , m_reportExceptions(reportExceptions)
{
    ASSERT(!m_context.IsEmpty());
}

bool ScriptScope::success()
{
    if (!m_exceptionCatcher.HasCaught())
        return true;

    v8::Local<v8::Message> message = m_exceptionCatcher.Message();
    if (m_reportExceptions)
        m_scriptState->frame()->document()->reportException(toWebCoreString(message->Get()), message->GetLineNumber(), toWebCoreString(message->GetScriptResourceName()));

    m_exceptionCatcher.Reset();
    return false;
}

} // namespace WebCore
