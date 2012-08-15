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

#ifndef InjectedBundleIntent_h
#define InjectedBundleIntent_h

#if ENABLE(WEB_INTENTS)

#include "APIObject.h"
#include <WebCore/Intent.h>
#include <WebCore/KURL.h>
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebKit {

class ImmutableArray;
class ImmutableDictionary;
class WebSerializedScriptValue;

class InjectedBundleIntent : public APIObject {
public:
    static const Type APIType = TypeBundleIntent;

    static PassRefPtr<InjectedBundleIntent> create(WebCore::Intent*);

    String action() const;
    String payloadType() const;
    WebCore::KURL service() const;
    PassRefPtr<WebSerializedScriptValue> data() const;
    String extra(const String& key) const;
    PassRefPtr<ImmutableDictionary> extras() const;
    PassRefPtr<ImmutableArray> suggestions() const;

    WebCore::Intent* coreIntent() const { return m_intent.get(); }

private:
    explicit InjectedBundleIntent(WebCore::Intent*);

    virtual Type type() const { return APIType; }

    RefPtr<WebCore::Intent> m_intent;
};

} // namespace WebKit

#endif // ENABLE(WEB_INTENTS)

#endif // InjectedBundleIntent_h
