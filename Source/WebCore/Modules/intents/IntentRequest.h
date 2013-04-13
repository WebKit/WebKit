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

#ifndef IntentRequest_h
#define IntentRequest_h

#if ENABLE(WEB_INTENTS)

#include "ActiveDOMObject.h"
#include "Intent.h"
#include "IntentResultCallback.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class ScriptExecutionContext;

class IntentRequest : public RefCounted<IntentRequest>, public ActiveDOMObject {
public:
    static PassRefPtr<IntentRequest> create(ScriptExecutionContext*, PassRefPtr<Intent>, PassRefPtr<IntentResultCallback> successCallback, PassRefPtr<IntentResultCallback> errorCallback);

    Intent* intent() { return m_intent.get(); }

    void postResult(PassRefPtr<SerializedScriptValue>);
    void postFailure(PassRefPtr<SerializedScriptValue>);

    virtual void contextDestroyed() OVERRIDE;

    virtual void stop() OVERRIDE;

private:
    IntentRequest(ScriptExecutionContext*, PassRefPtr<Intent>, PassRefPtr<IntentResultCallback> successCallback, PassRefPtr<IntentResultCallback> errorCallback);

    RefPtr<Intent> m_intent;
    RefPtr<IntentResultCallback> m_successCallback;
    RefPtr<IntentResultCallback> m_errorCallback;
    bool m_stopped;
};

} // namespace WebCore

#endif

#endif // IntentRequest_h
