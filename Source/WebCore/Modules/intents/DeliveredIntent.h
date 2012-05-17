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

#ifndef DeliveredIntent_h
#define DeliveredIntent_h

#if ENABLE(WEB_INTENTS)

#include "FrameDestructionObserver.h"
#include "Intent.h"
#include "MessagePort.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Frame;
class SerializedScriptValue;

// JS calls to postResult/postFailure on DeliveredIntent will be forwarded 
// to this client interface. The object is owned by the DeliveredIntent to
// which it is attached, and will be deleted when the delivered intent will issue
// no further calls to it. Before that point, the destroy() method is invoked.
class DeliveredIntentClient {
public:
    virtual ~DeliveredIntentClient() { }

    virtual void postResult(PassRefPtr<SerializedScriptValue> data) = 0;
    virtual void postFailure(PassRefPtr<SerializedScriptValue> data) = 0;
};

class DeliveredIntent : public Intent, public FrameDestructionObserver {
public:
    static PassRefPtr<DeliveredIntent> create(Frame*, PassOwnPtr<DeliveredIntentClient>, const String& action, const String& type,
                                              PassRefPtr<SerializedScriptValue>, PassOwnPtr<MessagePortArray>,
                                              const HashMap<String, String>&);

    virtual ~DeliveredIntent() { }

    MessagePortArray* ports() const;
    String getExtra(const String& key);
    void postResult(PassRefPtr<SerializedScriptValue> data);
    void postFailure(PassRefPtr<SerializedScriptValue> data);

    void setClient(PassRefPtr<DeliveredIntentClient>);

    virtual void frameDestroyed() OVERRIDE;

private:
    DeliveredIntent(Frame*, PassOwnPtr<DeliveredIntentClient>, const String& action, const String& type,
                    PassRefPtr<SerializedScriptValue>, PassOwnPtr<MessagePortArray>,
                    const HashMap<String, String>&);

    OwnPtr<DeliveredIntentClient> m_client;
    OwnPtr<MessagePortArray> m_ports;
};

} // namespace WebCore

#endif

#endif // Intent_h
