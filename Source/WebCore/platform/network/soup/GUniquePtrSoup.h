/*
 *  Copyright (C) 2014 Igalia S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#pragma once

#include <wtf/Platform.h>

#if USE(SOUP)

#include <libsoup/soup.h>
#include <wtf/glib/GUniquePtr.h>

namespace WTF {

WTF_DEFINE_GPTR_DELETER(SoupCookie, soup_cookie_free)
#if SOUP_CHECK_VERSION(2, 67, 1)
WTF_DEFINE_GPTR_DELETER(SoupHSTSPolicy, soup_hsts_policy_free)
#endif
#if USE(SOUP2)
WTF_DEFINE_GPTR_DELETER(SoupURI, soup_uri_free)
#endif
#if SOUP_CHECK_VERSION(2, 99, 3)
WTF_DEFINE_GPTR_DELETER(SoupMessageHeaders, soup_message_headers_unref)
#else
WTF_DEFINE_GPTR_DELETER(SoupMessageHeaders, soup_message_headers_free)
#endif

} // namespace WTF

#endif // USE(SOUP)
