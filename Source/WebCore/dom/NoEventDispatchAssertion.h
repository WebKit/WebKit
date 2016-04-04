/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef NoEventDispatchAssertion_h
#define NoEventDispatchAssertion_h

#include <wtf/MainThread.h>

namespace WebCore {

class NoEventDispatchAssertion {
public:
    NoEventDispatchAssertion()
    {
#if !ASSERT_DISABLED
        if (!isMainThread())
            return;
        ++s_count;
#endif
    }

    NoEventDispatchAssertion(const NoEventDispatchAssertion&)
        : NoEventDispatchAssertion()
    {
    }

    ~NoEventDispatchAssertion()
    {
#if !ASSERT_DISABLED
        if (!isMainThread())
            return;
        ASSERT(s_count);
        s_count--;
#endif
    }

    static bool isEventDispatchForbidden()
    {
#if ASSERT_DISABLED
        return false;
#else
        return isMainThread() && s_count;
#endif
    }

    static unsigned dropTemporarily()
    {
#if ASSERT_DISABLED
        return 0;
#else
        unsigned count = s_count;
        s_count = 0;
        return count;
#endif
    }

    static void restoreDropped(unsigned count)
    {
#if ASSERT_DISABLED
        UNUSED_PARAM(count);
#else
        ASSERT(!s_count);
        s_count = count;
#endif
    }

#if !ASSERT_DISABLED
private:
    WEBCORE_EXPORT static unsigned s_count;
#endif
};

}

#endif
