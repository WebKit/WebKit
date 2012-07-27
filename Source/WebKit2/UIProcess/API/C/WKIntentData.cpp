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
#include "WKIntentData.h"

#include "ImmutableArray.h"
#include "ImmutableDictionary.h"
#include "WKAPICast.h"
#include "WKDictionary.h"
#include "WKString.h"

#if ENABLE(WEB_INTENTS)
#include "WebIntentData.h"
#endif

using namespace WebKit;

WKTypeID WKIntentDataGetTypeID()
{
#if ENABLE(WEB_INTENTS)
    return toAPI(WebIntentData::APIType);
#else
    return 0;
#endif
}

WKIntentDataRef WKIntentDataCreate(WKDictionaryRef initDictionaryRef)
{
#if ENABLE(WEB_INTENTS)
    IntentData intentData;
    WKStringRef action = static_cast<WKStringRef>(WKDictionaryGetItemForKey(initDictionaryRef, WKStringCreateWithUTF8CString("action")));
    ASSERT(action);
    intentData.action = toImpl(action)->string();
    WKStringRef type = static_cast<WKStringRef>(WKDictionaryGetItemForKey(initDictionaryRef, WKStringCreateWithUTF8CString("type")));
    ASSERT(type);
    intentData.type = toImpl(type)->string();
    WKSerializedScriptValueRef data = static_cast<WKSerializedScriptValueRef>(WKDictionaryGetItemForKey(initDictionaryRef, WKStringCreateWithUTF8CString("data")));
    if (data)
        intentData.data = toImpl(data)->dataReference().vector();

    RefPtr<WebIntentData> webIntentData = WebIntentData::create(intentData);
    return toAPI(webIntentData.release().leakRef());
#else
    return 0;
#endif
}

WKStringRef WKIntentDataCopyAction(WKIntentDataRef intentRef)
{
#if ENABLE(WEB_INTENTS)
    return toCopiedAPI(toImpl(intentRef)->action());
#else
    return 0;
#endif
}

WKStringRef WKIntentDataCopyType(WKIntentDataRef intentRef)
{
#if ENABLE(WEB_INTENTS)
    return toCopiedAPI(toImpl(intentRef)->payloadType());
#else
    return 0;
#endif
}

WKURLRef WKIntentDataCopyService(WKIntentDataRef intentRef)
{
#if ENABLE(WEB_INTENTS)
    return toCopiedURLAPI(toImpl(intentRef)->service());
#else
    return 0;
#endif
}

WKArrayRef WKIntentDataCopySuggestions(WKIntentDataRef intentRef)
{
#if ENABLE(WEB_INTENTS)
    return toAPI(toImpl(intentRef)->suggestions().leakRef());
#else
    return 0;
#endif
}

WKStringRef WKIntentDataCopyExtra(WKIntentDataRef intentRef, WKStringRef key)
{
#if ENABLE(WEB_INTENTS)
    return toCopiedAPI(toImpl(intentRef)->extra(toWTFString(key)));
#else
    return 0;
#endif
}

WKDictionaryRef WKIntentDataCopyExtras(WKIntentDataRef intentRef)
{
#if ENABLE(WEB_INTENTS)
    return toAPI(toImpl(intentRef)->extras().leakRef());
#else
    return 0;
#endif
}
