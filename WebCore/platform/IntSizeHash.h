/*
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
#ifndef INTSIZEHASH_H_
#define INTSIZEHASH_H_

#include "IntSize.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WTF {

    template<> struct IntHash<WebCore::IntSize> {
        static unsigned hash(const WebCore::IntSize& key) { return intHash((static_cast<uint64_t>(key.width()) << 32 | key.height())); }
        static bool equal(const WebCore::IntSize& a, const WebCore::IntSize& b) { return a == b; }
    };
    template<> struct DefaultHash<WebCore::IntSize> { typedef IntHash<WebCore::IntSize> Hash; };

} // namespace WTF

#endif
