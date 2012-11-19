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

#ifndef ewk_intent_private_h
#define ewk_intent_private_h

#if ENABLE(WEB_INTENTS)

#include "WKEinaSharedString.h"
#include "WKIntentData.h"
#include "WKRetainPtr.h"
#include "ewk_object_private.h"
#include <WebKit2/WKBase.h>
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
class WebIntentData;
}

/**
 * \struct  EwkIntent
 * @brief   Contains the intent data.
 */
class EwkIntent : public EwkObject {
public:
    EWK_OBJECT_DECLARE(EwkIntent)

    static PassRefPtr<EwkIntent> create(WKIntentDataRef intentRef)
    {
        return adoptRef(new EwkIntent(intentRef));
    }

    WebKit::WebIntentData* webIntentData() const;
    const char* action() const;
    const char* type() const;
    const char* service() const;
    WKRetainPtr<WKArrayRef> suggestions() const;
    String extra(const char* key) const;
    WKRetainPtr<WKArrayRef> extraKeys() const;

private:
    explicit EwkIntent(WKIntentDataRef intentRef);

    WKRetainPtr<WKIntentDataRef> m_wkIntent;
    WKEinaSharedString m_action;
    WKEinaSharedString m_type;
    WKEinaSharedString m_service;
};

#endif // ENABLE(WEB_INTENTS)

#endif // ewk_intent_private_h
