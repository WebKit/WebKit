/*
 *  Copyright (C) 2010 Igalia S.L
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

#ifndef GOwnPtrSoup_h
#define GOwnPtrSoup_h

// We need to include libsoup headers here because the way that SoupBuffer
// is defined is not compatible with forward declaration.
#include <libsoup/soup.h>
#include <wtf/gobject/GOwnPtr.h>

namespace WTF {

template<> void freeOwnedGPtr<SoupURI>(SoupURI* ptr);
template<> void freeOwnedGPtr<SoupCookie>(SoupCookie* ptr);
template<> void freeOwnedGPtr<SoupMessageHeaders>(SoupMessageHeaders*);
template<> void freeOwnedGPtr<SoupBuffer>(SoupBuffer*);

}

#endif
