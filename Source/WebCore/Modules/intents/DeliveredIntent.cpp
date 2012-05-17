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
#include "DeliveredIntent.h"

#if ENABLE(WEB_INTENTS)

#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "SerializedScriptValue.h"

namespace WebCore {

PassRefPtr<DeliveredIntent> DeliveredIntent::create(Frame* frame, PassOwnPtr<DeliveredIntentClient> client, const String& action, const String& type,
                                                    PassRefPtr<SerializedScriptValue> data, PassOwnPtr<MessagePortArray> ports,
                                                    const HashMap<String, String>& extras)
{
    return adoptRef(new DeliveredIntent(frame, client, action, type, data, ports, extras));
}

DeliveredIntent::DeliveredIntent(Frame* frame, PassOwnPtr<DeliveredIntentClient> client, const String& action, const String& type,
                                 PassRefPtr<SerializedScriptValue> data, PassOwnPtr<MessagePortArray> ports,
                                 const HashMap<String, String>& extras)
    : Intent(action, type, data, PassOwnPtr<MessagePortChannelArray>(), extras, KURL())
    , FrameDestructionObserver(frame)
    , m_client(client)
    , m_ports(ports)
{
}

void DeliveredIntent::frameDestroyed()
{
    FrameDestructionObserver::frameDestroyed();
    m_client.clear();
}

MessagePortArray* DeliveredIntent::ports() const
{
    return m_ports.get();
}

String DeliveredIntent::getExtra(const String& key)
{
    return extras().get(key);
}

void DeliveredIntent::postResult(PassRefPtr<SerializedScriptValue> data)
{
    if (!m_client)
        return;

    m_client->postResult(data);
}

void DeliveredIntent::postFailure(PassRefPtr<SerializedScriptValue> data)
{
    if (!m_client)
        return;

    m_client->postFailure(data);
}

} // namespace WebCore

#endif // ENABLE(WEB_INTENTS)
