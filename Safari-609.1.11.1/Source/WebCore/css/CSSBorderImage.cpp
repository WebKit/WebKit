/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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
 */

#include "config.h"
#include "CSSBorderImage.h"

#include "CSSValueList.h"

namespace WebCore {

Ref<CSSValueList> createBorderImageValue(RefPtr<CSSValue>&& image, RefPtr<CSSValue>&& imageSlice, RefPtr<CSSValue>&& borderSlice, RefPtr<CSSValue>&& outset, RefPtr<CSSValue>&& repeat)
{
    auto list = CSSValueList::createSpaceSeparated();
    if (image)
        list.get().append(*image);

    if (borderSlice || outset) {
        auto listSlash = CSSValueList::createSlashSeparated();
        if (imageSlice)
            listSlash.get().append(imageSlice.releaseNonNull());

        if (borderSlice)
            listSlash.get().append(borderSlice.releaseNonNull());

        if (outset)
            listSlash.get().append(outset.releaseNonNull());

        list.get().append(WTFMove(listSlash));
    } else if (imageSlice)
        list.get().append(imageSlice.releaseNonNull());
    if (repeat)
        list.get().append(repeat.releaseNonNull());
    return list;
}

} // namespace WebCore
