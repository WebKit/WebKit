/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PageGroup.h"

#include "ChromeClient.h"
#include "Document.h"
#include "Page.h"

namespace WebCore {

// --------

static bool shouldTrackVisitedLinks;

PageGroup::PageGroup(Page* page)
    : m_visitedLinksPopulated(false)
{
    ASSERT(page);
    m_pages.add(page);
}

void PageGroup::addPage(Page* page)
{
    ASSERT(page);
    ASSERT(!m_pages.contains(page));
    m_pages.add(page);
}

void PageGroup::removePage(Page* page)
{
    ASSERT(page);
    ASSERT(m_pages.contains(page));
    m_pages.remove(page);
}

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

bool PageGroup::isLinkVisited(Document* document, const AtomicString& attributeURL)
{
    if (!m_visitedLinksPopulated) {
        m_visitedLinksPopulated = true;
        ASSERT(!m_pages.isEmpty());
        (*m_pages.begin())->chrome()->client()->populateVisitedLinks();
    }

    const UChar* characters = attributeURL.characters();
    unsigned length = attributeURL.length();
    if (!length)
        return false;

    // FIXME: It is strange that we do not do further processing on strings that have "://" in them.
    // That's clearly incorrect for at least these reasons:
    //    1) The "://" could be in the query or anchor.
    //    2) The URL's path could have a "/./" or a "/../" or a "//" sequence in it.

    // FIXME: needsTrailingSlash does not properly return true for a URL that has no path, but does
    // have a query or anchor.

    bool hasColonSlashSlash = containsColonSlashSlash(characters, length);

    if (hasColonSlashSlash && !needsTrailingSlash(characters, length))
        return m_visitedLinkHashes.contains(StringImpl::computeHash(characters, length));

    Vector<UChar, 512> buffer;

    // This is a poor man's completeURL. Faster with less memory allocation.
    // FIXME: It's missing a lot of what completeURL does and what KURL does.
    // FIXME: Move this into KURL? Or Document? Even the fast version should be in the right place,
    // rather than here.

    if (hasColonSlashSlash) {
        // FIXME: This is incorrect for URLs that have a query or anchor; the "/" needs to go at the
        // end of the path, *before* the query or anchor.
        buffer.append(characters, length);
        buffer.append('/');
        return m_visitedLinkHashes.contains(StringImpl::computeHash(buffer.data(), buffer.size()));
    }

    const KURL& baseURL = document->baseURL();
    switch (characters[0]) {
        case '/':
            buffer.append(baseURL.string().characters(), baseURL.pathStart());
            break;
        case '#':
            buffer.append(baseURL.string().characters(), baseURL.pathEnd());
            break;
        default:
            buffer.append(baseURL.string().characters(), baseURL.pathAfterLastSlash());
            break;
    }
    buffer.append(characters, length);
    cleanPath(buffer);
    if (needsTrailingSlash(buffer.data(), buffer.size())) {
        // FIXME: This is incorrect for URLs that have a query or anchor; the "/" needs to go at the
        // end of the path, *before* the query or anchor.
        buffer.append('/');
    }

    return m_visitedLinkHashes.contains(StringImpl::computeHash(buffer.data(), buffer.size()));
}

void PageGroup::addVisitedLink(const KURL& url)
{
    if (!shouldTrackVisitedLinks)
        return;
    ASSERT(!url.isEmpty());
    m_visitedLinkHashes.add(url.string().impl()->hash());
}

void PageGroup::addVisitedLink(const UChar* characters, size_t length)
{
    if (!shouldTrackVisitedLinks)
        return;
    m_visitedLinkHashes.add(StringImpl::computeHash(characters, length));
}

void PageGroup::removeVisitedLinks()
{
    m_visitedLinkHashes.clear();
    m_visitedLinksPopulated = false;
}

void PageGroup::removeAllVisitedLinks()
{
    Page::removeAllVisitedLinks();
}

void PageGroup::setShouldTrackVisitedLinks(bool shouldTrack)
{
    if (shouldTrackVisitedLinks == shouldTrack)
        return;
    shouldTrackVisitedLinks = shouldTrack;
    if (!shouldTrackVisitedLinks)
        removeAllVisitedLinks();
}

} // namespace WebCore
