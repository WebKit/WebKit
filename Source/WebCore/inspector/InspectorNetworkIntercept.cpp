/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "InspectorNetworkIntercept.h"

#include <wtf/TZoneMallocInlines.h>

namespace Inspector {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Inspector::PendingInterceptRequest);
WTF_MAKE_TZONE_ALLOCATED_IMPL(Inspector::PendingInterceptResponse);

bool Intercept::matches(const String& url, NetworkStage networkStage)
{
    if (this->networkStage != networkStage)
        return false;

    if (this->url.isEmpty())
        return true;

    if (m_knownMatchingURLs.contains(url))
        return true;

    if (!m_urlSearcher) {
        auto searchType = isRegex ? ContentSearchUtilities::SearchType::Regex : ContentSearchUtilities::SearchType::ExactString;
        auto searchCaseSensitive = caseSensitive ? ContentSearchUtilities::SearchCaseSensitive::Yes : ContentSearchUtilities::SearchCaseSensitive::No;
        m_urlSearcher = ContentSearchUtilities::createSearcherForString(this->url, searchType, searchCaseSensitive);
    }
    if (!ContentSearchUtilities::searcherMatchesText(*m_urlSearcher, url))
        return false;

    m_knownMatchingURLs.add(url);
    return true;
}

} // namespace Inspector
