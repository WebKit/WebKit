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

#include "MatchResult.h"
#include "RenderStyle.h"
#include "Timer.h"
#include <wtf/CanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>

namespace WebCore {

namespace Style {

class Resolver;

class MatchedDeclarationsCache : public RefCounted<MatchedDeclarationsCache>, public CanMakeWeakPtr<MatchedDeclarationsCache> {
    WTF_MAKE_TZONE_ALLOCATED(MatchedDeclarationsCache);
public:
    // A "shared" cache is the process-global instance reused across documents/iterations; it uses
    // relaxed (FontDescription-only, for simple selectors) inherited equality so entries can match
    // across documents. A non-shared (per-Resolver) cache uses strict inherited equality, exactly
    // matching ToT — it is the fallback for entries excluded from the shared cache (e.g. viewport-
    // unit styles) and for documents that don't qualify for sharing. (rdar://173598541.)
    static Ref<MatchedDeclarationsCache> create(bool isSharedAcrossDocuments = false) { return adoptRef(*new MatchedDeclarationsCache(isSharedAcrossDocuments)); }
    ~MatchedDeclarationsCache();

    static bool isCacheable(const Element&, const RenderStyle&, const RenderStyle& parentStyle);
    static unsigned computeHash(const MatchResult&, const Style::CustomPropertyData& inheritedCustomProperties);

    struct Entry {
        RefPtr<const MatchResult> matchResult;
        std::unique_ptr<const RenderStyle> renderStyle;
        std::unique_ptr<const RenderStyle> parentRenderStyle;

        bool isUsableAfterHighPriorityProperties(const RenderStyle&) const;
    };

    struct Result {
        const Entry& entry;
        bool inheritedEqual;
    };

    std::optional<Result> find(unsigned hash, const MatchResult&, const Style::CustomPropertyData& inheritedCustomProperties, const RenderStyle&);
    void add(const RenderStyle&, const RenderStyle& parentStyle,  unsigned hash, const MatchResult&);
    void remove(unsigned hash);

    // Every N additions to the matched declaration cache trigger a sweep where entries holding
    // the last reference to a style declaration are garbage collected.
    void invalidate();
    void clearEntriesAffectedByViewportUnits();

    // Flush only if the documentElement's relative-unit basis (font + line-height, what rem/rcap/
    // rch/rex/ric/rlh resolve against) differs from what the cached entries were built under. Lets
    // the shared cache survive identical reloads (cross-document reuse) while still invalidating on
    // a real :root font change. (rdar://173598541.)
    void invalidateIfDocumentElementRelativeUnitsChanged(const RenderStyle& documentElementStyle);

private:
    explicit MatchedDeclarationsCache(bool isSharedAcrossDocuments);

    template<typename Callback>
    void removeAllMatching(const Callback& matches);

    void sweep();

    const bool m_isSharedAcrossDocuments;
    std::unique_ptr<const RenderStyle> m_documentElementStyleForRelativeUnits; // rem/rcap/… basis the entries were built under (rdar://173598541)
    HashMap<unsigned, Vector<Entry>, AlreadyHashed> m_entries;
    Timer m_sweepTimer;
    unsigned m_additionsSinceLastSweep { 0 };
};

}
}
