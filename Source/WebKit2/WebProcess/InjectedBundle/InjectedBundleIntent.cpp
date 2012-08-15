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
#include "InjectedBundleIntent.h"

#if ENABLE(WEB_INTENTS)

#include "ImmutableArray.h"
#include "ImmutableDictionary.h"
#include "WebSerializedScriptValue.h"
#include "WebString.h"
#include "WebURL.h"

using namespace WebCore;

namespace WebKit {

PassRefPtr<InjectedBundleIntent> InjectedBundleIntent::create(WebCore::Intent* intent)
{
    return adoptRef(new InjectedBundleIntent(intent));
}

InjectedBundleIntent::InjectedBundleIntent(WebCore::Intent* intent)
    : m_intent(intent)
{
}

String InjectedBundleIntent::action() const
{
    return m_intent->action();
}

String InjectedBundleIntent::payloadType() const
{
    return m_intent->type();
}

WebCore::KURL InjectedBundleIntent::service() const
{
    return m_intent->service();
}

PassRefPtr<WebSerializedScriptValue> InjectedBundleIntent::data() const
{
    return WebSerializedScriptValue::create(m_intent->data());
}

String InjectedBundleIntent::extra(const String& key) const
{
    return m_intent->extras().get(key);
}

PassRefPtr<ImmutableDictionary> InjectedBundleIntent::extras() const
{
    const HashMap<String, String>& extras = m_intent->extras();
    ImmutableDictionary::MapType wkExtras;
    HashMap<String, String>::const_iterator end = extras.end();
    for (HashMap<String, String>::const_iterator it = extras.begin(); it != end; ++it)
        wkExtras.set(it->first, WebString::create(it->second));

    return ImmutableDictionary::adopt(wkExtras);
}

PassRefPtr<ImmutableArray> InjectedBundleIntent::suggestions() const
{
    const Vector<KURL>& suggestions = m_intent->suggestions();
    const size_t numSuggestions = suggestions.size();
    Vector<RefPtr<APIObject> > wkSuggestions(numSuggestions);
    for (unsigned i = 0; i < numSuggestions; ++i)
        wkSuggestions[i] = WebURL::create(suggestions[i]);

    return ImmutableArray::adopt(wkSuggestions);
}

} // namespace WebKit

#endif // ENABLE(WEB_INTENTS)
