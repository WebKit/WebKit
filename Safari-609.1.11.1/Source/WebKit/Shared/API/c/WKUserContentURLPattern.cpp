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
#include "WKUserContentURLPattern.h"

#include "APIUserContentURLPattern.h"
#include "WKAPICast.h"
#include "WKString.h"

WKTypeID WKUserContentURLPatternGetTypeID()
{
    return WebKit::toAPI(API::UserContentURLPattern::APIType);
}

WKUserContentURLPatternRef WKUserContentURLPatternCreate(WKStringRef patternRef)
{
    return WebKit::toAPI(&API::UserContentURLPattern::create(WebKit::toImpl(patternRef)->string()).leakRef());
}

WKStringRef WKUserContentURLPatternCopyHost(WKUserContentURLPatternRef urlPatternRef)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(urlPatternRef)->host());
}

WKStringRef WKUserContentURLPatternCopyScheme(WKUserContentURLPatternRef urlPatternRef)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(urlPatternRef)->scheme());
}

bool WKUserContentURLPatternIsValid(WKUserContentURLPatternRef urlPatternRef)
{
    return WebKit::toImpl(urlPatternRef)->isValid();
}

bool WKUserContentURLPatternMatchesURL(WKUserContentURLPatternRef urlPatternRef, WKURLRef urlRef)
{
    return WebKit::toImpl(urlPatternRef)->matchesURL(WebKit::toWTFString(urlRef));
}

bool WKUserContentURLPatternMatchesSubdomains(WKUserContentURLPatternRef urlPatternRef)
{
    return WebKit::toImpl(urlPatternRef)->matchesSubdomains();
}
