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

#include <wtf/URL.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringView.h>

namespace WebCore {

template <typename CharacterType>
static inline size_t findSlashDotDotSlash(std::span<const CharacterType> characters, size_t position)
{
    if (characters.size() < 4)
        return notFound;
    size_t loopLimit = characters.size() - 3;
    for (size_t i = position; i < loopLimit; ++i) {
        if (characters[i] == '/' && characters[i + 1] == '.' && characters[i + 2] == '.' && characters[i + 3] == '/')
            return i;
    }
    return notFound;
}

template <typename CharacterType>
static inline size_t findSlashSlash(std::span<const CharacterType> characters, size_t position)
{
    if (characters.size() < 2)
        return notFound;
    size_t loopLimit = characters.size() - 1;
    for (size_t i = position; i < loopLimit; ++i) {
        if (characters[i] == '/' && characters[i + 1] == '/')
            return i;
    }
    return notFound;
}

template <typename CharacterType>
static inline size_t findSlashDotSlash(std::span<const CharacterType> characters, size_t position)
{
    if (characters.size() < 3)
        return notFound;
    size_t loopLimit = characters.size() - 2;
    for (size_t i = position; i < loopLimit; ++i) {
        if (characters[i] == '/' && characters[i + 1] == '.' && characters[i + 2] == '/')
            return i;
    }
    return notFound;
}

template <typename CharacterType>
static inline bool containsColonSlashSlash(std::span<const CharacterType> characters)
{
    if (characters.size() < 3)
        return false;
    unsigned loopLimit = characters.size() - 2;
    for (unsigned i = 0; i < loopLimit; ++i) {
        if (characters[i] == ':' && characters[i + 1] == '/' && characters[i + 2] == '/')
            return true;
    }
    return false;
}

template <typename CharacterType>
static inline void squeezeOutNullCharacters(Vector<CharacterType, 512>& string)
{
    size_t size = string.size();
    size_t i = 0;
    for (i = 0; i < size; ++i) {
        if (!string[i])
            break;
    }
    if (i == size)
        return;
    size_t j = i;
    for (++i; i < size; ++i) {
        if (CharacterType character = string[i])
            string[j++] = character;
    }
    ASSERT(j < size);
    string.shrink(j);
}

template <typename CharacterType>
static void cleanSlashDotDotSlashes(Vector<CharacterType, 512>& path, size_t firstSlash)
{
    size_t slash = firstSlash;
    do {
        size_t previousSlash = slash ? reverseFind(path.span(), '/', slash - 1) : notFound;
        // Don't remove the host, i.e. http://foo.org/../foo.html
        if (previousSlash == notFound || (previousSlash > 3 && path[previousSlash - 2] == ':' && path[previousSlash - 1] == '/')) {
            path[slash] = 0;
            path[slash + 1] = 0;
            path[slash + 2] = 0;
        } else {
            for (size_t i = previousSlash; i < slash + 3; ++i)
                path[i] = 0;
        }
        slash += 3;
    } while ((slash = findSlashDotDotSlash(path.span(), slash)) != notFound);
    squeezeOutNullCharacters(path);
}

template <typename CharacterType>
static void mergeDoubleSlashes(Vector<CharacterType, 512>& path, size_t firstSlash)
{
    size_t refPos = find(path.span(), '#');
    if (!refPos || refPos == notFound)
        refPos = path.size();

    size_t slash = firstSlash;
    while (slash < refPos) {
        if (!slash || path[slash - 1] != ':')
            path[slash++] = 0;
        else
            slash += 2;
        if ((slash = findSlashSlash(path.span(), slash)) == notFound)
            break;
    }
    squeezeOutNullCharacters(path);
}

template <typename CharacterType>
static void cleanSlashDotSlashes(Vector<CharacterType, 512>& path, size_t firstSlash)
{
    size_t slash = firstSlash;
    do {
        path[slash] = 0;
        path[slash + 1] = 0;
        slash += 2;
    } while ((slash = findSlashDotSlash(path.span(), slash)) != notFound);
    squeezeOutNullCharacters(path);
}

template <typename CharacterType>
static inline void cleanPath(Vector<CharacterType, 512>& path)
{
    // FIXME: Should not do this in the query or anchor part of the URL.
    size_t firstSlash = findSlashDotDotSlash(path.span(), 0);
    if (firstSlash != notFound)
        cleanSlashDotDotSlashes(path, firstSlash);

    // FIXME: Should not do this in the query part.
    firstSlash = findSlashSlash(path.span(), 0);
    if (firstSlash != notFound)
        mergeDoubleSlashes(path, firstSlash);

    // FIXME: Should not do this in the query or anchor part.
    firstSlash = findSlashDotSlash(path.span(), 0);
    if (firstSlash != notFound)
        cleanSlashDotSlashes(path, firstSlash);
}

template <typename CharacterType>
static inline bool matchLetter(CharacterType c, char lowercaseLetter)
{
    return (c | 0x20) == lowercaseLetter;
}

template <typename CharacterType>
static inline bool needsTrailingSlash(std::span<const CharacterType> characters)
{
    if (characters.size() < 6)
        return false;
    if (!matchLetter(characters[0], 'h') || !matchLetter(characters[1], 't') || !matchLetter(characters[2], 't') || !matchLetter(characters[3], 'p'))
        return false;
    if (!(characters[4] == ':' || (matchLetter(characters[4], 's') && characters[5] == ':')))
        return false;

    unsigned pos = characters[4] == ':' ? 5 : 6;

    // Skip initial two slashes if present.
    if (pos + 1 < characters.size() && characters[pos] == '/' && characters[pos + 1] == '/')
        pos += 2;

    // Find next slash.
    while (pos < characters.size() && characters[pos] != '/')
        ++pos;

    return pos == characters.size();
}

template <typename CharacterType>
static ALWAYS_INLINE SharedStringHash computeSharedStringHashInline(std::span<const CharacterType> url)
{
    return AlreadyHashed::avoidDeletedValue(SuperFastHash::computeHash(url));
}

SharedStringHash computeSharedStringHash(const String& url)
{
    if (url.isEmpty() || url.is8Bit())
        return computeSharedStringHashInline(url.span8());
    return computeSharedStringHashInline(url.span16());
}

SharedStringHash computeSharedStringHash(std::span<const UChar> url)
{
    return computeSharedStringHashInline(url);
}

template <typename CharacterType>
static ALWAYS_INLINE SharedStringHash computeSharedStringHashInline(const URL& base, std::span<const CharacterType> characters)
{
    if (characters.empty())
        return 0;

    // This is a poor man's completeURL. Faster with less memory allocation.
    // FIXME: It's missing a lot of what completeURL does and a lot of what URL does.
    // For example, it does not handle international domain names properly.

    // FIXME: It is wrong that we do not do further processing on strings that have "://" in them:
    //    1) The "://" could be in the query or anchor.
    //    2) The URL's path could have a "/./" or a "/../" or a "//" sequence in it.

    // FIXME: needsTrailingSlash does not properly return true for a URL that has no path, but does
    // have a query or anchor.

    if (containsColonSlashSlash(characters)) {
        if (!needsTrailingSlash(characters))
            return computeSharedStringHashInline(characters);

        // FIXME: This is incorrect for URLs that have a query or anchor; the "/" needs to go at the
        // end of the path, *before* the query or anchor.
        SuperFastHash hasher;
        hasher.addCharacters(characters);
        hasher.addCharacter('/');
        return AlreadyHashed::avoidDeletedValue(hasher.hash());
    }

    Vector<CharacterType, 512> buffer;
    if (characters.empty())
        append(buffer, base.string());
    else {
        switch (characters[0]) {
        case '/':
            append(buffer, StringView(base.string()).left(base.pathStart()));
            break;
        case '#':
            append(buffer, StringView(base.string()).left(base.pathEnd()));
            break;
        default:
            append(buffer, StringView(base.string()).left(base.pathAfterLastSlash()));
            break;
        }
    }
    buffer.append(characters);
    cleanPath(buffer);
    if (needsTrailingSlash(buffer.span())) {
        // FIXME: This is incorrect for URLs that have a query or anchor; the "/" needs to go at the
        // end of the path, *before* the query or anchor.
        buffer.append('/');
    }

    return computeSharedStringHashInline(buffer.span());
}

SharedStringHash computeVisitedLinkHash(const URL& base, const AtomString& attributeURL)
{
    if (attributeURL.isEmpty())
        return 0;

    if ((base.string().isEmpty() || base.string().is8Bit()) && attributeURL.is8Bit())
        return computeSharedStringHashInline(base, attributeURL.span8());

    auto upconvertedCharacters = StringView(attributeURL.string()).upconvertedCharacters();
    return computeSharedStringHashInline(base, upconvertedCharacters.span());
}

} // namespace WebCore
