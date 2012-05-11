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

#include "config.h"
#include "Intent.h"

#if ENABLE(WEB_INTENTS)

#include "ExceptionCode.h"
#include "MessagePort.h"
#include "SerializedScriptValue.h"

namespace WebCore {

PassRefPtr<Intent> Intent::create(const String& action, const String& type, PassRefPtr<SerializedScriptValue> data, const MessagePortArray& ports, ExceptionCode& ec)
{
    if (action.isEmpty()) {
        ec = SYNTAX_ERR;
        return 0;
    }
    if (type.isEmpty()) {
        ec = SYNTAX_ERR;
        return 0;
    }

    OwnPtr<MessagePortChannelArray> channels = MessagePort::disentanglePorts(&ports, ec);

    WTF::HashMap<String, String> extras;
    KURL serviceUrl;

    return adoptRef(new Intent(action, type, data, channels.release(), extras, serviceUrl));
}

PassRefPtr<Intent> Intent::create(ScriptState* scriptState, const Dictionary& options, ExceptionCode& ec)
{
    String action;
    if (!options.get("action", action) || action.isEmpty()) {
        ec = SYNTAX_ERR;
        return 0;
    }

    String type;
    if (!options.get("type", type) || type.isEmpty()) {
        ec = SYNTAX_ERR;
        return 0;
    }

    String service;
    KURL serviceUrl;
    if (options.get("service", service) && !service.isEmpty()) {
      serviceUrl = KURL(KURL(), service);
      if (!serviceUrl.isValid()) {
          ec = SYNTAX_ERR;
          return 0;
      }
    }

    MessagePortArray ports;
    OwnPtr<MessagePortChannelArray> channels;
    if (options.get("transfer", ports))
        channels = MessagePort::disentanglePorts(&ports, ec);

    ScriptValue dataValue;
    RefPtr<SerializedScriptValue> serializedData;
    if (options.get("data", dataValue)) {
        bool didThrow = false;
        serializedData = dataValue.serialize(scriptState, &ports, 0, didThrow);
        if (didThrow) {
            ec = DATA_CLONE_ERR;
            return 0;
        }
    }

    WTF::HashMap<String, String> extras;
    Dictionary extrasDictionary;
    if (options.get("extras", extrasDictionary))
        extrasDictionary.getOwnPropertiesAsStringHashMap(extras);

    return adoptRef(new Intent(action, type, serializedData.release(), channels.release(), extras, serviceUrl));
}

Intent::Intent(const String& action, const String& type,
               PassRefPtr<SerializedScriptValue> data, PassOwnPtr<MessagePortChannelArray> ports,
               const WTF::HashMap<String, String>& extras, const KURL& service)
    : m_action(action)
    , m_type(type)
    , m_ports(ports)
    , m_service(service)
    , m_extras(extras)
{
    if (data)
        m_data = data;
    else
        m_data = SerializedScriptValue::nullValue();
}

} // namespace WebCore

#endif // ENABLE(WEB_INTENTS)
