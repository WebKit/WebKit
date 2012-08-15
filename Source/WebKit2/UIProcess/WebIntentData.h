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

#ifndef WebIntentData_h
#define WebIntentData_h

#if ENABLE(WEB_INTENTS)

#include "APIObject.h"
#include "IntentData.h"
#include <WebCore/KURL.h>
#include <wtf/PassRefPtr.h>

namespace WebKit {

class ImmutableArray;
class ImmutableDictionary;
class WebProcessProxy;
class WebSerializedScriptValue;

class WebIntentData : public APIObject {
public:
    static const Type APIType = TypeIntentData;

    static PassRefPtr<WebIntentData> create(const IntentData& store, WebProcessProxy* process)
    {
        return adoptRef(new WebIntentData(store, process));
    }

    virtual ~WebIntentData();

    const String& action() const { return m_store.action; }
    const String& payloadType() const { return m_store.type; }
    const WebCore::KURL& service() const { return m_store.service; }
    PassRefPtr<WebSerializedScriptValue> data() const;
    String extra(const String& key) const;
    PassRefPtr<ImmutableDictionary> extras() const;
    PassRefPtr<ImmutableArray> suggestions() const;

    const IntentData& store() const { return m_store; }

private:
    WebIntentData(const IntentData&, WebProcessProxy*);

    virtual Type type() const { return APIType; }

    IntentData m_store;
    WebProcessProxy* m_process;
};

} // namespace WebKit

#endif // ENABLE(WEB_INTENTS)

#endif // WebIntentData_h
