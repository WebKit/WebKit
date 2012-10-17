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
#include <WebKit2/WKBase.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

typedef struct _Ewk_Intent Ewk_Intent;

/**
 * \struct  _Ewk_Intent
 * @brief   Contains the intent data.
 */
class _Ewk_Intent : public RefCounted<_Ewk_Intent> {
public:
    WKRetainPtr<WKIntentDataRef> wkIntent;
    WKEinaSharedString action;
    WKEinaSharedString type;
    WKEinaSharedString service;

    static PassRefPtr<_Ewk_Intent> create(WKIntentDataRef intentRef)
    {
        return adoptRef(new _Ewk_Intent(intentRef));
    }

private:
    explicit _Ewk_Intent(WKIntentDataRef intentRef)
        : wkIntent(intentRef)
        , action(AdoptWK, WKIntentDataCopyAction(intentRef))
        , type(AdoptWK, WKIntentDataCopyType(intentRef))
        , service(AdoptWK, WKIntentDataCopyService(intentRef))
    { }
};

#endif // ENABLE(WEB_INTENTS)

#endif // ewk_intent_private_h
