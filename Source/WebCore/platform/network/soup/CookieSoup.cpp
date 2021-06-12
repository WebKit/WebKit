/*
 * Copyright (C) 2016 Igalia S.L.
 * Copyright (C) 2017 Endless Mobile, Inc.
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

#include <libsoup/soup.h>
#include <wtf/DateMath.h>

namespace WebCore {

#if SOUP_CHECK_VERSION(2, 69, 90)
static Cookie::SameSitePolicy coreSameSitePolicy(SoupSameSitePolicy policy)
{
    switch (policy) {
    case SOUP_SAME_SITE_POLICY_NONE:
        return Cookie::SameSitePolicy::None;
    case SOUP_SAME_SITE_POLICY_LAX:
        return Cookie::SameSitePolicy::Lax;
    case SOUP_SAME_SITE_POLICY_STRICT:
        return Cookie::SameSitePolicy::Strict;
    }

    ASSERT_NOT_REACHED();
    return Cookie::SameSitePolicy::None;
}

static SoupSameSitePolicy soupSameSitePolicy(Cookie::SameSitePolicy policy)
{
    switch (policy) {
    case Cookie::SameSitePolicy::None:
        return SOUP_SAME_SITE_POLICY_NONE;
    case Cookie::SameSitePolicy::Lax:
        return SOUP_SAME_SITE_POLICY_LAX;
    case Cookie::SameSitePolicy::Strict:
        return SOUP_SAME_SITE_POLICY_STRICT;
    }

    ASSERT_NOT_REACHED();
    return SOUP_SAME_SITE_POLICY_NONE;
}
#endif

Cookie::Cookie(SoupCookie* cookie)
    : name(String::fromUTF8(soup_cookie_get_name(cookie)))
    , value(String::fromUTF8(soup_cookie_get_value(cookie)))
    , domain(String::fromUTF8(soup_cookie_get_domain(cookie)))
    , path(String::fromUTF8(soup_cookie_get_path(cookie)))
#if USE(SOUP2)
    , expires(soup_cookie_get_expires(cookie) ? std::make_optional(static_cast<double>(soup_date_to_time_t(soup_cookie_get_expires(cookie))) * 1000) : std::nullopt)
#else
    , expires(soup_cookie_get_expires(cookie) ? std::make_optional(static_cast<double>(g_date_time_to_unix(soup_cookie_get_expires(cookie))) * 1000) : std::nullopt)
#endif
    , httpOnly(soup_cookie_get_http_only(cookie))
    , secure(soup_cookie_get_secure(cookie))
    , session(!soup_cookie_get_expires(cookie))

{
#if SOUP_CHECK_VERSION(2, 69, 90)
    sameSite = coreSameSitePolicy(soup_cookie_get_same_site_policy(cookie));
#endif
}

#if USE(SOUP2)
static SoupDate* msToSoupDate(double ms)
{
    int year = msToYear(ms);
    int dayOfYear = dayInYear(ms, year);
    bool leapYear = isLeapYear(year);

    // monthFromDayInYear() returns a value in the [0,11] range, while soup_date_new() expects
    // a value in the [1,12] range, meaning we have to manually adjust the month value.
    return soup_date_new(year, monthFromDayInYear(dayOfYear, leapYear) + 1, dayInMonthFromDayInYear(dayOfYear, leapYear), msToHours(ms), msToMinutes(ms), static_cast<int64_t>(ms / 1000) % 60);
}
#endif

SoupCookie* Cookie::toSoupCookie() const
{
    if (name.isNull() || value.isNull() || domain.isNull() || path.isNull())
        return nullptr;

    SoupCookie* soupCookie = soup_cookie_new(name.utf8().data(), value.utf8().data(),
        domain.utf8().data(), path.utf8().data(), -1);

    soup_cookie_set_http_only(soupCookie, httpOnly);
    soup_cookie_set_secure(soupCookie, secure);
#if SOUP_CHECK_VERSION(2, 69, 90)
    soup_cookie_set_same_site_policy(soupCookie, soupSameSitePolicy(sameSite));
#endif

    if (!session && expires) {
#if USE(SOUP2)
        SoupDate* date = msToSoupDate(*expires);
        soup_cookie_set_expires(soupCookie, date);
        soup_date_free(date);
#else
        GRefPtr<GDateTime> date = adoptGRef(g_date_time_new_from_unix_utc(*expires / 1000.));
        soup_cookie_set_expires(soupCookie, date.get());
#endif
    }

    return soupCookie;
}

} // namespace WebCore
