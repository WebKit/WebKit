/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "WebIntent.h"

#include "Intent.h"
#include "MessagePort.h"
#include "PlatformMessagePortChannel.h"
#include "SerializedScriptValue.h"
#include <wtf/HashMap.h>

namespace WebKit {

WebIntent::WebIntent(const WebString& action, const WebString& type, const WebString& data)
{
#if ENABLE(WEB_INTENTS)
    WebCore::ExceptionCode ec = 0;
    WebCore::MessagePortArray ports;
    RefPtr<WebCore::Intent> intent = WebCore::Intent::create(action, type, WebCore::SerializedScriptValue::createFromWire(data), ports, ec);
    if (ec)
        return;

    m_private = intent.release();
#endif
}

#if ENABLE(WEB_INTENTS)
WebIntent::WebIntent(const PassRefPtr<WebCore::Intent>& intent)
    : m_private(intent)
{
}
#endif

void WebIntent::reset()
{
#if ENABLE(WEB_INTENTS)
    m_private.reset();
#endif
}

bool WebIntent::isNull() const
{
#if ENABLE(WEB_INTENTS)
    return m_private.isNull();
#else
    return true;
#endif
}

bool WebIntent::equals(const WebIntent& other) const
{
#if ENABLE(WEB_INTENTS)
    return (m_private.get() == other.m_private.get());
#else
    return true;
#endif
}

void WebIntent::assign(const WebIntent& other)
{
#if ENABLE(WEB_INTENTS)
    m_private = other.m_private;
#endif
}

WebString WebIntent::action() const
{
#if ENABLE(WEB_INTENTS)
    return m_private->action();
#else
    return WebString();
#endif
}

WebString WebIntent::type() const
{
#if ENABLE(WEB_INTENTS)
    return m_private->type();
#else
    return WebString();
#endif
}

WebString WebIntent::data() const
{
#if ENABLE(WEB_INTENTS)
    return m_private->data()->toWireString();
#else
    return WebString();
#endif
}

WebURL WebIntent::service() const
{
#if ENABLE(WEB_INTENTS)
    return WebURL(m_private->service());
#else
    return WebURL();
#endif
}

WebMessagePortChannelArray* WebIntent::messagePortChannelsRelease() const
{
    // Note: see PlatformMessagePortChannel::postMessageToRemote.
    WebMessagePortChannelArray* webChannels = 0;
    WebCore::MessagePortChannelArray* messagePorts = m_private->messagePorts();
    if (messagePorts) {
        webChannels = new WebMessagePortChannelArray(messagePorts->size());
        for (size_t i = 0; i < messagePorts->size(); ++i) {
            WebCore::PlatformMessagePortChannel* platformChannel = messagePorts->at(i)->channel();
            (*webChannels)[i] = platformChannel->webChannelRelease();
            (*webChannels)[i]->setClient(0);
        }
    }

    return webChannels;
}

WebIntent::operator WebCore::Intent*() const
{
    return m_private.get();
}

WebVector<WebString> WebIntent::extrasNames() const
{
#if ENABLE(WEB_INTENTS)
    size_t numExtras = m_private->extras().size();
    WebVector<WebString> keyStrings(numExtras);
    WTF::HashMap<String, String>::const_iterator::Keys keyIter = m_private->extras().begin().keys();
    for (size_t i = 0; keyIter != m_private->extras().end().keys(); ++keyIter, ++i)
        keyStrings[i] = *keyIter;
    return keyStrings;
#else
    return WebVector<WebString>();
#endif
}

WebString WebIntent::extrasValue(const WebString& name) const
{
#if ENABLE(WEB_INTENTS)
    WTF::HashMap<String, String>::const_iterator val = m_private->extras().find(name);
    if (val == m_private->extras().end())
        return WebString();
    return val->second;
#else
    return WebString();
#endif
}

} // namespace WebKit
