/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "ElementRuleCollector.h"
#include "RenderStyle.h"
#include "Timer.h"

namespace WebCore {

namespace Style {

class MatchedDeclarationsCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    MatchedDeclarationsCache();
    ~MatchedDeclarationsCache();

    static bool isCacheable(const Element&, const RenderStyle&, const RenderStyle& parentStyle);
    static unsigned computeHash(const MatchResult&);

    struct Entry {
        MatchResult matchResult;
        std::unique_ptr<const RenderStyle> renderStyle;
        std::unique_ptr<const RenderStyle> parentRenderStyle;

        bool isUsableAfterHighPriorityProperties(const RenderStyle&) const;
    };

    const Entry* find(unsigned hash, const MatchResult&);
    void add(const RenderStyle&, const RenderStyle& parentStyle, unsigned hash, const MatchResult&);

    // Every N additions to the matched declaration cache trigger a sweep where entries holding
    // the last reference to a style declaration are garbage collected.
    void invalidate();
    void clearEntriesAffectedByViewportUnits();

private:
    void sweep();

    HashMap<unsigned, Entry> m_entries;
    Timer m_sweepTimer;
    unsigned m_additionsSinceLastSweep { 0 };
};

}
}
