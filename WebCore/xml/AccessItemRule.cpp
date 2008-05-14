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
#include "AccessItemRule.h"

#include "PlatformString.h"
#include "ParserUtilities.h"
#include <stdio.h>

namespace WebCore {

AccessItemRule::AccessItemRule(const String& rule)
{
    parseAccessItemRule(rule);
}

static inline bool skipLWS(const UChar*& ptr, const UChar* end)
{
    // LWS as defined by RFC 2616:
    // LWS = [CRLF] 1*( SP | HT )

    if (ptr + 1 < end && *ptr == '\r' && *(ptr + 1) == '\n')
        ptr += 2;

    const UChar* start = ptr;
    while (ptr < end && (*ptr == ' ' || *ptr == '\t'))
        ptr++;
    return ptr != start;
}

void AccessItemRule::parseAccessItemRule(const String& rule)
{
    // Parse the rule according to Section 4.2 (Access-Control HTTP Response Header) of the 
    // Access Control for Cross-site Requests spec.
    // W3C Working Draft 14 February 2008

    //   Access-Control = "Access-Control" ":" 1#rule
    //   rule           = "allow" 1*(LWS pattern) [LWS "exclude" 1*(LWS pattern)]
    //   pattern        = "<" access item ">"

    if (rule.isEmpty())
        return;

    const UChar* ptr = rule.characters();
    const UChar* end = ptr + rule.length();

    // Skip leading whitespace
    skipLWS(ptr, end);
    if (ptr == end)
        return;

    if (!skipString(ptr, end, "allow"))
        return;

    parsePatternList(ptr, end, m_allowList);
    if (m_allowList.isEmpty())
        return;

    if (!skipString(ptr, end, "exclude")) {
        if (ptr != end)
            invalidate();
        return;
    }

    parsePatternList(ptr, end, m_excludeList);
    if (m_excludeList.isEmpty()) {
        invalidate();
        return;
    }

    if (ptr != end)
        invalidate();
}

void AccessItemRule::parsePatternList(const UChar*& ptr, const UChar* end, Vector<AccessItem>& list)
{
    while (ptr < end) {
        if (!skipLWS(ptr, end) || ptr == end) {
            invalidate();
            return;
        }

        if (*ptr != '<')
            return;

        ptr++;

        bool sawEndTag = false;
        const UChar* start = ptr;
        while (ptr < end) {
            if (*ptr == '>') {
                sawEndTag = true;
                break;
            }
            ptr++;
        }
        if (!sawEndTag) {
            invalidate();
            return;
        }
        
        AccessItem accessItem(String(start, ptr - start));
        if (!accessItem.isValid()) {
            invalidate();
            return;
        }

        list.append(accessItem);
        ptr++;
    }
}

void AccessItemRule::invalidate()
{
    m_allowList.clear();
    m_excludeList.clear();
}

#ifndef NDEBUG
void AccessItemRule::show()
{
    printf("  AccessItemRule::show\n");

    printf("  AllowList count: %d\n", static_cast<int>(m_allowList.size()));
    for (size_t i = 0; i < m_allowList.size(); ++i)
        m_allowList[i].show();

    printf("  ExludeList count: %d\n", static_cast<int>(m_excludeList.size()));
    for (size_t i = 0; i < m_excludeList.size(); ++i)
        m_excludeList[i].show();
}
#endif

} // namespace WebCore
