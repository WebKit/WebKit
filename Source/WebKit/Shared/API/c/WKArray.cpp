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
#include "WKArray.h"

#include "APIArray.h"
#include "WKAPICast.h"
#include <wtf/StdLibExtras.h>

WKTypeID WKArrayGetTypeID()
{
    return WebKit::toAPI(API::Array::APIType);
}

WKArrayRef WKArrayCreate(WKTypeRef* rawValues, size_t numberOfValues)
{
    auto values = unsafeMakeSpan(rawValues, numberOfValues);
    Vector<RefPtr<API::Object>> elements(numberOfValues, [values](size_t i) -> RefPtr<API::Object> {
        return WebKit::toImpl(values[i]);
    });
    return WebKit::toAPI(&API::Array::create(WTFMove(elements)).leakRef());
}

WKArrayRef WKArrayCreateAdoptingValues(WKTypeRef* rawValues, size_t numberOfValues)
{
    auto values = unsafeMakeSpan(rawValues, numberOfValues);
    Vector<RefPtr<API::Object>> elements(numberOfValues, [values](size_t i) {
        return adoptRef(WebKit::toImpl(values[i]));
    });
    return WebKit::toAPI(&API::Array::create(WTFMove(elements)).leakRef());
}

WKTypeRef WKArrayGetItemAtIndex(WKArrayRef arrayRef, size_t index)
{
    return WebKit::toAPI(WebKit::toImpl(arrayRef)->at(index));
}

size_t WKArrayGetSize(WKArrayRef arrayRef)
{
    return WebKit::toImpl(arrayRef)->size();
}
