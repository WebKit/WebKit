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
#include "WKNumber.h"

#include "APINumber.h"
#include "WKAPICast.h"

WKTypeID WKBooleanGetTypeID()
{
    return WebKit::toAPI(API::Boolean::APIType);
}

WKBooleanRef WKBooleanCreate(bool value)
{
    auto booleanObject = API::Boolean::create(value);
    return WebKit::toAPI(&booleanObject.leakRef());
}

bool WKBooleanGetValue(WKBooleanRef booleanRef)
{
    return WebKit::toImpl(booleanRef)->value();
}

WKTypeID WKDoubleGetTypeID()
{
    return WebKit::toAPI(API::Double::APIType);
}

WKDoubleRef WKDoubleCreate(double value)
{
    auto doubleObject = API::Double::create(value);
    return WebKit::toAPI(&doubleObject.leakRef());
}

double WKDoubleGetValue(WKDoubleRef doubleRef)
{
    return WebKit::toImpl(doubleRef)->value();
}

WKTypeID WKUInt64GetTypeID()
{
    return WebKit::toAPI(API::UInt64::APIType);
}

WKUInt64Ref WKUInt64Create(uint64_t value)
{
    auto uint64Object = API::UInt64::create(value);
    return WebKit::toAPI(&uint64Object.leakRef());
}

uint64_t WKUInt64GetValue(WKUInt64Ref uint64Ref)
{
    return WebKit::toImpl(uint64Ref)->value();
}
