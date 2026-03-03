/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Cookie.h"

#include <wtf/WallTime.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {
    
#if !PLATFORM(COCOA)
bool Cookie::operator==(const Cookie& other) const
{
    return name == other.name
        && domain == other.domain
        && path == other.path
        && secure == other.secure;
}
    
unsigned Cookie::hash() const
{
    return StringHash::hash(name) + StringHash::hash(domain) + StringHash::hash(path) + secure;
}
#endif

namespace CookieUtil {

String buildSetCookieStringWithoutDomain(const Cookie& cookie)
{
    StringBuilder builder;
    builder.append(cookie.name, '=', cookie.value);

    if (!cookie.path.isEmpty())
        builder.append("; Path="_s, cookie.path);

    if (cookie.expires) {
        auto now = WallTime::now().secondsSinceEpoch().milliseconds();
        auto maxAgeSeconds = static_cast<int64_t>((*cookie.expires - now) / 1000);
        if (maxAgeSeconds > 0)
            builder.append("; Max-Age="_s, maxAgeSeconds);
        else
            builder.append("; Max-Age=0"_s);
    }

    if (cookie.httpOnly)
        builder.append("; HttpOnly"_s);

    if (cookie.secure)
        builder.append("; Secure"_s);

    switch (cookie.sameSite) {
    case Cookie::SameSitePolicy::Default:
        break;
    case Cookie::SameSitePolicy::None:
        builder.append("; SameSite=None"_s);
        break;
    case Cookie::SameSitePolicy::Lax:
        builder.append("; SameSite=Lax"_s);
        break;
    case Cookie::SameSitePolicy::Strict:
        builder.append("; SameSite=Strict"_s);
        break;
    }

    return builder.toString();
}

String defaultPathForURL(const URL& url)
{
    // Algorithm to generate the default path is outlined in https://tools.ietf.org/html/rfc6265#section-5.1.4

    String path = url.path().toString();
    if (path.isEmpty() || !path.startsWith('/'))
        return "/"_s;

    auto lastSlashPosition = path.reverseFind('/');
    if (!lastSlashPosition)
        return "/"_s;

    return path.left(lastSlashPosition);
}

} // namespace CookieUtil

} // namespace WebCore

