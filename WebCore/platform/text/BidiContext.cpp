/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2009, 2010 Apple Inc. All right reserved.
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

#include "config.h"
#include "BidiContext.h"

namespace WebCore {

using namespace WTF::Unicode;

inline PassRefPtr<BidiContext> BidiContext::createUncached(unsigned char level, Direction direction, bool override, BidiContext* parent)
{
    return adoptRef(new BidiContext(level, direction, override, parent));
}

PassRefPtr<BidiContext> BidiContext::create(unsigned char level, Direction direction, bool override, BidiContext* parent)
{
    ASSERT(direction == (level % 2 ? RightToLeft : LeftToRight));

    if (parent)
        return createUncached(level, direction, override, parent);

    ASSERT(level <= 1);
    if (!level) {
        if (!override) {
            static BidiContext* ltrContext = createUncached(0, LeftToRight, false, 0).releaseRef();
            return ltrContext;
        }

        static BidiContext* ltrOverrideContext = createUncached(0, LeftToRight, true, 0).releaseRef();
        return ltrOverrideContext;
    }

    if (!override) {
        static BidiContext* rtlContext = createUncached(1, RightToLeft, false, 0).releaseRef();
        return rtlContext;
    }

    static BidiContext* rtlOverrideContext = createUncached(1, RightToLeft, true, 0).releaseRef();
    return rtlOverrideContext;
}

bool operator==(const BidiContext& c1, const BidiContext& c2)
{
    if (&c1 == &c2)
        return true;
    if (c1.level() != c2.level() || c1.override() != c2.override() || c1.dir() != c2.dir())
        return false;
    if (!c1.parent())
        return !c2.parent();
    return c2.parent() && *c1.parent() == *c2.parent();
}

} // namespace WebCore
