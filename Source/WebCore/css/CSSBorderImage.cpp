/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "CSSPropertyParserConsumer+Background.h"
#include "CSSValueList.h"

namespace WebCore {

Ref<CSSValueList> createBorderImageValue(CSS::BorderImageComponents&& components)
{
    CSSValueListBuilder list;
    if (components.source)
        list.append(components.source.releaseNonNull());
    if (components.width || components.outset) {
        CSSValueListBuilder listSlash;
        if (components.slice)
            listSlash.append(components.slice.releaseNonNull());
        if (components.width)
            listSlash.append(components.width.releaseNonNull());
        if (components.outset)
            listSlash.append(components.outset.releaseNonNull());
        list.append(CSSValueList::createSlashSeparated(WTF::move(listSlash)));
    } else if (components.slice)
        list.append(components.slice.releaseNonNull());
    if (components.repeat)
        list.append(components.repeat.releaseNonNull());
    return CSSValueList::createSpaceSeparated(WTF::move(list));
}

} // namespace WebCore
