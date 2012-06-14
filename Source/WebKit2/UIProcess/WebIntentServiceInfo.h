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

#ifndef WebIntentServiceInfo_h
#define WebIntentServiceInfo_h

#if ENABLE(WEB_INTENTS_TAG)

#include "APIObject.h"
#include "IntentServiceInfo.h"
#include <wtf/PassRefPtr.h>

namespace WebKit {

class WebIntentServiceInfo : public APIObject {
public:
    static const Type APIType = TypeIntentServiceInfo;

    static PassRefPtr<WebIntentServiceInfo> create(const IntentServiceInfo& store)
    {
        return adoptRef(new WebIntentServiceInfo(store));
    }

    virtual ~WebIntentServiceInfo() { }

    const String& action() const { return m_store.action; }
    const String& payloadType() const { return m_store.type; }
    const WebCore::KURL& href() const { return m_store.href; }
    const String& title() const { return m_store.title; }
    const String& disposition() const { return m_store.disposition; }

private:
    WebIntentServiceInfo(const IntentServiceInfo&);

    virtual Type type() const { return APIType; }

    IntentServiceInfo m_store;
};

} // namespace WebKit

#endif // ENABLE(WEB_INTENTS_TAG)

#endif // WebIntentServiceInfo_h
