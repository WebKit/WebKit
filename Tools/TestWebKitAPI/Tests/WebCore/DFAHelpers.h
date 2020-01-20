/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CombinedURLFilters.h>
#include <WebCore/NFA.h>
#include <WebCore/NFAToDFA.h>
#include <WebCore/URLFilterParser.h>

using namespace WebCore;

namespace TestWebKitAPI {

static unsigned countLiveNodes(const ContentExtensions::DFA& dfa)
{
    unsigned counter = 0;
    for (const auto& node : dfa.nodes) {
        if (!node.isKilled())
            ++counter;
    }
    return counter;
}

static Vector<ContentExtensions::NFA> createNFAs(ContentExtensions::CombinedURLFilters& combinedURLFilters)
{
    Vector<ContentExtensions::NFA> nfas;

    combinedURLFilters.processNFAs(std::numeric_limits<size_t>::max(), [&](ContentExtensions::NFA&& nfa) {
        nfas.append(WTFMove(nfa));
    });

    return nfas;
}

static ContentExtensions::DFA buildDFAFromPatterns(Vector<const char*> patterns)
{
    ContentExtensions::CombinedURLFilters combinedURLFilters;
    ContentExtensions::URLFilterParser parser(combinedURLFilters);

    for (const char* pattern : patterns)
        parser.addPattern(pattern, false, 0);
    Vector<ContentExtensions::NFA> nfas = createNFAs(combinedURLFilters);
    return ContentExtensions::NFAToDFA::convert(nfas[0]);
}

} // namespace TestWebKitAPI
