/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "InspectorCookieStoreHelpers.h"

#include <WebCore/Cookie.h>
#include <wtf/JSONValues.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

CookieFilter CookieFilter::fromProtocol(const RefPtr<JSON::Object>& filter)
{
    CookieFilter parsed;
    if (!filter)
        return parsed;

    parsed.name = filter->getString("name"_s);
    parsed.value = filter->getString("value"_s);
    parsed.domain = filter->getString("domain"_s);
    parsed.path = filter->getString("path"_s);
    parsed.httpOnly = filter->getBoolean("httpOnly"_s);
    parsed.secure = filter->getBoolean("secure"_s);
    return parsed;
}

bool CookieFilter::matches(const WebCore::Cookie& cookie) const
{
    if (!name.isEmpty() && cookie.name != name)
        return false;

    if (!value.isEmpty() && cookie.value != value)
        return false;

    if (!domain.isEmpty() && cookie.domain != domain)
        return false;

    if (!path.isEmpty() && cookie.path != path)
        return false;

    if (httpOnly && cookie.httpOnly != *httpOnly)
        return false;

    if (secure && cookie.secure != *secure)
        return false;

    return true;
}

} // namespace WebKit
