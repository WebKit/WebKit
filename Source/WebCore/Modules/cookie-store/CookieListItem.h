/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#pragma once

#include "Cookie.h"
#include "CookieSameSite.h"
#include "DOMHighResTimeStamp.h"
#include <optional>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct CookieListItem {
    static CookieSameSite from(Cookie::SameSitePolicy policy)
    {
        switch (policy) {
        case Cookie::SameSitePolicy::Strict:
            return CookieSameSite::Strict;
        case Cookie::SameSitePolicy::Lax:
            return CookieSameSite::Lax;
        case Cookie::SameSitePolicy::None:
            return CookieSameSite::None;
        }

        ASSERT_NOT_REACHED();
        return CookieSameSite::Strict;
    }

    static CookieListItem from(Cookie&& cookie)
    {
        return CookieListItem {
            WTFMove(cookie.name),
            WTFMove(cookie.value),
            WTFMove(cookie.domain),
            WTFMove(cookie.path),
            cookie.expires,
            cookie.secure,
            from(cookie.sameSite)
        };
    }

    String name;
    String value;
    String domain;
    String path;
    std::optional<DOMHighResTimeStamp> expires;
    bool secure { false };
    CookieSameSite sameSite { CookieSameSite::Strict };
};

}
