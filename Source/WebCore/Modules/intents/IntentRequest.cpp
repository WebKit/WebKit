/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Google, Inc. ("Google") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
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
#include "IntentRequest.h"

#if ENABLE(WEB_INTENTS)

#include "ScriptExecutionContext.h"
#include "SerializedScriptValue.h"

namespace WebCore {

PassRefPtr<IntentRequest> IntentRequest::create(ScriptExecutionContext* context,
                                                PassRefPtr<Intent> intent,
                                                PassRefPtr<IntentResultCallback> successCallback,
                                                PassRefPtr<IntentResultCallback> errorCallback)
{
    return adoptRef(new IntentRequest(context, intent, successCallback, errorCallback));
}

IntentRequest::IntentRequest(ScriptExecutionContext* context,
                             PassRefPtr<Intent> intent,
                             PassRefPtr<IntentResultCallback> successCallback,
                             PassRefPtr<IntentResultCallback> errorCallback)
    : ActiveDOMObject(context, this)
    , m_intent(intent)
    , m_successCallback(successCallback)
    , m_errorCallback(errorCallback)
{
    setPendingActivity(this);
}

void IntentRequest::contextDestroyed()
{
    ContextDestructionObserver::contextDestroyed();
    m_successCallback.clear();
    m_errorCallback.clear();
}

void IntentRequest::stop()
{
    m_successCallback.clear();
    m_errorCallback.clear();
}

void IntentRequest::postResult(SerializedScriptValue* data)
{
    // Callback could lead to deletion of this.
    RefPtr<IntentRequest> protector(this);

    if (!m_successCallback)
        return;

    m_successCallback->handleEvent(data);

    m_successCallback.clear();
    m_errorCallback.clear();
    unsetPendingActivity(this);
}

void IntentRequest::postFailure(SerializedScriptValue* data)
{
    // Callback could lead to deletion of this.
    RefPtr<IntentRequest> protector(this);

    if (!m_errorCallback)
        return;

    m_errorCallback->handleEvent(data);

    m_successCallback.clear();
    m_errorCallback.clear();
    unsetPendingActivity(this);
}

} // namespace WebCore

#endif // ENABLE(WEB_INTENTS)
