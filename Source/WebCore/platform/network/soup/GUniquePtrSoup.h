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

#ifndef GUniquePtrSoup_h
#define GUniquePtrSoup_h

#include <libsoup/soup.h>
#include <wtf/gobject/GUniquePtr.h>

namespace WTF {

WTF_DEFINE_GPTR_DELETER(SoupURI, soup_uri_free)
WTF_DEFINE_GPTR_DELETER(SoupCookie, soup_cookie_free)
WTF_DEFINE_GPTR_DELETER(SoupMessageHeaders, soup_message_headers_free)
WTF_DEFINE_GPTR_DELETER(SoupBuffer, soup_buffer_free)

} // namespace WTF

#endif
