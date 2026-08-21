/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
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
#include "SharedStringHash.h"

#include <wtf/HashFunctions.h>
#include <wtf/URLParser.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringView.h>

namespace WebCore {

template <typename CharacterType>
static ALWAYS_INLINE SharedStringHash NODELETE computeSharedStringHashInline(std::span<const CharacterType> url)
{
    return AlreadyHashed::avoidDeletedValue(SuperFastHash::computeHash(url));
}

SharedStringHash computeSharedStringHash(const String& url)
{
    if (url.isEmpty() || url.is8Bit())
        return computeSharedStringHashInline(url.span8());
    return computeSharedStringHashInline(url.span16());
}

SharedStringHash computeSharedStringHash(std::span<const char16_t> url)
{
    return computeSharedStringHashInline(url);
}

SharedStringHash computeSharedStringHash(StringView url)
{
    if (url.isEmpty() || url.is8Bit())
        return computeSharedStringHashInline(url.span8());
    return computeSharedStringHashInline(url.span16());
}

SharedStringHash computeVisitedLinkHash(const URL& base, const AtomString& attributeURL)
{
    if (attributeURL.isEmpty())
        return 0;

    // Resolve "attributeURL" against "base" using the real URL canonicalizer and hash the
    // canonical characters directly out of the parser's buffer — no result String is allocated.
    return WTF::URLParser::parseAndConsume(String { attributeURL.string() }, base, nullptr, [](StringView canonicalURL) {
        return computeSharedStringHash(canonicalURL);
    });
}

} // namespace WebCore
