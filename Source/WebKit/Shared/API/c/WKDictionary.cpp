/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "WKDictionary.h"

#include "APIArray.h"
#include "APIDictionary.h"
#include "WKAPICast.h"

WKTypeID WKDictionaryGetTypeID()
{
    return WebKit::toAPI(API::Dictionary::APIType);
}

WK_EXPORT WKDictionaryRef WKDictionaryCreate(const WKStringRef* keys, const WKTypeRef* values, size_t numberOfValues)
{
    API::Dictionary::MapType map;
    map.reserveInitialCapacity(numberOfValues);
    for (size_t i = 0; i < numberOfValues; ++i)
        map.add(WebKit::toImpl(keys[i])->string(), WebKit::toImpl(values[i]));

    return WebKit::toAPI(&API::Dictionary::create(WTFMove(map)).leakRef());
}

WKTypeRef WKDictionaryGetItemForKey(WKDictionaryRef dictionaryRef, WKStringRef key)
{
    return WebKit::toAPI(WebKit::toImpl(dictionaryRef)->get(WebKit::toImpl(key)->string()));
}

size_t WKDictionaryGetSize(WKDictionaryRef dictionaryRef)
{
    return WebKit::toImpl(dictionaryRef)->size();
}

WKArrayRef WKDictionaryCopyKeys(WKDictionaryRef dictionaryRef)
{
    return WebKit::toAPI(&WebKit::toImpl(dictionaryRef)->keys().leakRef());
}
