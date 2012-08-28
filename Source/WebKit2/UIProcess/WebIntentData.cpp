/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebIntentData.h"

#if ENABLE(WEB_INTENTS)

#include "ImmutableArray.h"
#include "ImmutableDictionary.h"
#include "WebProcessProxy.h"
#include "WebSerializedScriptValue.h"
#include "WebString.h"
#include "WebURL.h"

namespace WebKit {

WebIntentData::WebIntentData(const IntentData& store, WebProcessProxy* process)
    : m_store(store)
    , m_process(process)
{
}

WebIntentData::~WebIntentData()
{
    // Remove MessagePortChannels from WebProcess.
    if (m_process) {
        size_t numMessagePorts = m_store.messagePorts.size();
        for (size_t i = 0; i < numMessagePorts; ++i)
            m_process->removeMessagePortChannel(m_store.messagePorts[i]);
    }
}

PassRefPtr<WebSerializedScriptValue> WebIntentData::data() const
{
    Vector<uint8_t> dataCopy = m_store.data;
    return WebSerializedScriptValue::adopt(dataCopy);
}

PassRefPtr<ImmutableArray> WebIntentData::suggestions() const
{
    const size_t numSuggestions = m_store.suggestions.size();
    Vector<RefPtr<APIObject> > wkSuggestions(numSuggestions);
    for (unsigned i = 0; i < numSuggestions; ++i)
        wkSuggestions[i] = WebURL::create(m_store.suggestions[i]);
    return ImmutableArray::adopt(wkSuggestions);
}

String WebIntentData::extra(const String& key) const
{
    return m_store.extras.get(key);
}

PassRefPtr<ImmutableDictionary> WebIntentData::extras() const
{
    ImmutableDictionary::MapType wkExtras;
    HashMap<String, String>::const_iterator end = m_store.extras.end();
    for (HashMap<String, String>::const_iterator it = m_store.extras.begin(); it != end; ++it)
        wkExtras.set(it->first, WebString::create(it->second));

    return ImmutableDictionary::adopt(wkExtras);
}

} // namespace WebKit

#endif // ENABLE(WEB_INTENTS)

