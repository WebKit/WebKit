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
#include "WKBundleIntent.h"

#if ENABLE(WEB_INTENTS)
#include "ImmutableArray.h"
#include "ImmutableDictionary.h"
#include "InjectedBundleIntent.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WKDictionary.h"
#include "WKString.h"
#include "WebSerializedScriptValue.h"

using namespace WebCore;
using namespace WebKit;
#endif

WKTypeID WKBundleIntentGetTypeID()
{
#if ENABLE(WEB_INTENTS)
    return toAPI(InjectedBundleIntent::APIType);
#else
    return 0;
#endif
}

WKBundleIntentRef WKBundleIntentCreate(WKDictionaryRef dictionaryRef)
{
#if ENABLE(WEB_INTENTS)
    WKStringRef action = static_cast<WKStringRef>(WKDictionaryGetItemForKey(dictionaryRef, WKStringCreateWithUTF8CString("action")));
    ASSERT(action);
    WKStringRef type = static_cast<WKStringRef>(WKDictionaryGetItemForKey(dictionaryRef, WKStringCreateWithUTF8CString("type")));
    ASSERT(type);
    WKSerializedScriptValueRef data = static_cast<WKSerializedScriptValueRef>(WKDictionaryGetItemForKey(dictionaryRef, WKStringCreateWithUTF8CString("data")));
    MessagePortArray dummyPorts;
    ExceptionCode ec;

    RefPtr<Intent> coreIntent = Intent::create(toImpl(action)->string(), toImpl(type)->string(), data ? static_cast<SerializedScriptValue*>(toImpl(data)->internalRepresentation()) : 0, dummyPorts, ec);

    return toAPI(InjectedBundleIntent::create(coreIntent.get()).leakRef());
#else
    return 0;
#endif
}

WKStringRef WKBundleIntentCopyAction(WKBundleIntentRef intentRef)
{
#if ENABLE(WEB_INTENTS)
    return toCopiedAPI(toImpl(intentRef)->action());
#else
    return 0;
#endif
}

WKStringRef WKBundleIntentCopyType(WKBundleIntentRef intentRef)
{
#if ENABLE(WEB_INTENTS)
    return toCopiedAPI(toImpl(intentRef)->payloadType());
#else
    return 0;
#endif
}

WKURLRef WKBundleIntentCopyService(WKBundleIntentRef intentRef)
{
#if ENABLE(WEB_INTENTS)
    return toCopiedURLAPI(toImpl(intentRef)->service());
#else
    return 0;
#endif
}

WKArrayRef WKBundleIntentCopySuggestions(WKBundleIntentRef intentRef)
{
#if ENABLE(WEB_INTENTS)
    return toAPI(toImpl(intentRef)->suggestions().leakRef());
#else
    return 0;
#endif
}

WKStringRef WKBundleIntentCopyExtraValue(WKBundleIntentRef intentRef, WKStringRef key)
{
#if ENABLE(WEB_INTENTS)
    return toCopiedAPI(toImpl(intentRef)->extra(toWTFString(key)));
#else
    return 0;
#endif
}

WKDictionaryRef WKBundleIntentCopyExtras(WKBundleIntentRef intentRef)
{
#if ENABLE(WEB_INTENTS)
    return toAPI(toImpl(intentRef)->extras().leakRef());
#else
    return 0;
#endif
}

size_t WKBundleIntentMessagePortCount(WKBundleIntentRef intentRef)
{
#if ENABLE(WEB_INTENTS)
    MessagePortChannelArray* messagePorts = toImpl(intentRef)->coreIntent()->messagePorts();

    return messagePorts ? messagePorts->size() : 0;
#else
    return 0;
#endif
}
