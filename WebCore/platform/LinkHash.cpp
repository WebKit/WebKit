/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "AtomicString.h"
#include "KURL.h"
#include "LinkHash.h"
#include "PlatformString.h"
#include "StringHash.h"
#include "StringImpl.h"

namespace WebCore {

static inline int findSlashDotDotSlash(const UChar* characters, size_t length)
{
    if (length < 4)
        return -1;
    unsigned loopLimit = length - 3;
    for (unsigned i = 0; i < loopLimit; ++i) {
        if (characters[i] == '/' && characters[i + 1] == '.' && characters[i + 2] == '.' && characters[i + 3] == '/')
            return i;
    }
    return -1;
}

static inline int findSlashSlash(const UChar* characters, size_t length, int position)
{
    if (length < 2)
        return -1;
    unsigned loopLimit = length - 1;
    for (unsigned i = position; i < loopLimit; ++i) {
        if (characters[i] == '/' && characters[i + 1] == '/')
            return i;
    }
    return -1;
}

static inline int findSlashDotSlash(const UChar* characters, size_t length)
{
    if (length < 3)
        return -1;
    unsigned loopLimit = length - 2;
    for (unsigned i = 0; i < loopLimit; ++i) {
        if (characters[i] == '/' && characters[i + 1] == '.' && characters[i + 2] == '/')
            return i;
    }
    return -1;
}

static inline bool containsColonSlashSlash(const UChar* characters, unsigned length)
{
    if (length < 3)
        return false;
    unsigned loopLimit = length - 2;
    for (unsigned i = 0; i < loopLimit; ++i) {
        if (characters[i] == ':' && characters[i + 1] == '/' && characters[i + 2] == '/')
            return true;
    }
    return false;
}

static inline void cleanPath(Vector<UChar, 512>& path)
{
    // FIXME: Shold not do this in the query or anchor part.
    int pos;
    while ((pos = findSlashDotDotSlash(path.data(), path.size())) != -1) {
        int prev = reverseFind(path.data(), path.size(), '/', pos - 1);
        // don't remove the host, i.e. http://foo.org/../foo.html
        if (prev < 0 || (prev > 3 && path[prev - 2] == ':' && path[prev - 1] == '/'))
            path.remove(pos, 3);
        else
            path.remove(prev, pos - prev + 3);
    }

    // FIXME: Shold not do this in the query part.
    // Set refPos to -2 to mean "I haven't looked for the anchor yet".
    // We don't want to waste a function call on the search for the the anchor
    // in the vast majority of cases where there is no "//" in the path.
    pos = 0;
    int refPos = -2;
    while ((pos = findSlashSlash(path.data(), path.size(), pos)) != -1) {
        if (refPos == -2)
            refPos = find(path.data(), path.size(), '#');
        if (refPos > 0 && pos >= refPos)
            break;

        if (pos == 0 || path[pos - 1] != ':')
            path.remove(pos);
        else
            pos += 2;
    }

    // FIXME: Shold not do this in the query or anchor part.
    while ((pos = findSlashDotSlash(path.data(), path.size())) != -1)
        path.remove(pos, 2);
}


static inline bool matchLetter(UChar c, UChar lowercaseLetter)
{
    return (c | 0x20) == lowercaseLetter;
}

static inline bool needsTrailingSlash(const UChar* characters, unsigned length)
{
    if (length < 6)
        return false;
    if (!matchLetter(characters[0], 'h')
            || !matchLetter(characters[1], 't')
            || !matchLetter(characters[2], 't')
            || !matchLetter(characters[3], 'p'))
        return false;
    if (!(characters[4] == ':'
            || (matchLetter(characters[4], 's') && characters[5] == ':')))
        return false;

    unsigned pos = characters[4] == ':' ? 5 : 6;

    // Skip initial two slashes if present.
    if (pos + 1 < length && characters[pos] == '/' && characters[pos + 1] == '/')
        pos += 2;

    // Find next slash.
    while (pos < length && characters[pos] != '/')
        ++pos;

    return pos == length;
}

LinkHash visitedLinkHash(const UChar* url, unsigned length)
{
  return AlreadyHashed::avoidDeletedValue(StringImpl::computeHash(url, length));
}

void visitedURL(const KURL& base, const AtomicString& attributeURL, Vector<UChar, 512>& buffer)
{
    const UChar* characters = attributeURL.characters();
    unsigned length = attributeURL.length();
    if (!length)
        return;

    // This is a poor man's completeURL. Faster with less memory allocation.
    // FIXME: It's missing a lot of what completeURL does and a lot of what KURL does.
    // For example, it does not handle international domain names properly.

    // FIXME: It is wrong that we do not do further processing on strings that have "://" in them:
    //    1) The "://" could be in the query or anchor.
    //    2) The URL's path could have a "/./" or a "/../" or a "//" sequence in it.

    // FIXME: needsTrailingSlash does not properly return true for a URL that has no path, but does
    // have a query or anchor.

    bool hasColonSlashSlash = containsColonSlashSlash(characters, length);

    if (hasColonSlashSlash && !needsTrailingSlash(characters, length)) {
        buffer.append(attributeURL.characters(), attributeURL.length());
        return;
    }


    if (hasColonSlashSlash) {
        // FIXME: This is incorrect for URLs that have a query or anchor; the "/" needs to go at the
        // end of the path, *before* the query or anchor.
        buffer.append(characters, length);
        buffer.append('/');
        return;
    }

    switch (characters[0]) {
        case '/':
            buffer.append(base.string().characters(), base.pathStart());
            break;
        case '#':
            buffer.append(base.string().characters(), base.pathEnd());
            break;
        default:
            buffer.append(base.string().characters(), base.pathAfterLastSlash());
            break;
    }
    buffer.append(characters, length);
    cleanPath(buffer);
    if (needsTrailingSlash(buffer.data(), buffer.size())) {
        // FIXME: This is incorrect for URLs that have a query or anchor; the "/" needs to go at the
        // end of the path, *before* the query or anchor.
        buffer.append('/');
    }

    return;
}

LinkHash visitedLinkHash(const KURL& base, const AtomicString& attributeURL)
{
    Vector<UChar, 512> url;
    visitedURL(base, attributeURL, url);
    if (url.isEmpty())
        return 0;

    return visitedLinkHash(url.data(), url.size());
}

}  // namespace WebCore
