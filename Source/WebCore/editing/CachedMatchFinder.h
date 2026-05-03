/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "FindOptions.h"
#include "SimpleRange.h"
#include <wtf/Function.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/text/StringView.h>

namespace WebCore {

class ContainerNode;
class Document;
class ShadowRoot;

class CachedMatchFinder {
    WTF_MAKE_TZONE_ALLOCATED(CachedMatchFinder);
public:
    explicit CachedMatchFinder(Document&);

    std::optional<SimpleRange> findMatchFrom(const std::optional<SimpleRange>&, const String& target, FindOptions);
    Vector<SimpleRange> findMatches(const std::optional<SimpleRange>&, const String& target, FindOptions, std::optional<unsigned> limit = std::nullopt);
    unsigned countMatches(const std::optional<SimpleRange>&, const String& target, FindOptions, std::optional<unsigned> limit = std::nullopt);

private:
    struct TextRun {
        unsigned offset;
        SimpleRange range;
    };

    struct TextRunCache {
        String text;
        Vector<TextRun> runs;
        bool dirty { true };
    };

    bool isTextBufferCacheValid() const;
    bool clearTextBufferCache();
    TextRunCache& bufferForOptions(FindOptions);
    bool isSearchResultCacheValid(const String&, FindOptions, std::optional<unsigned> limit) const;
    static std::pair<String, Vector<TextRun>> textForScope(ContainerNode&, FindOptions);
    enum class SearchShouldContinue : bool { No, Yes };
    std::optional<SimpleRange> findNextMatchInShadowIncludingAncestorTree(ShadowRoot&, const SimpleRange&, const String& target, FindOptions);
    static void performSearch(StringView, unsigned startOffset, const String& target, FindOptions, NOESCAPE const Function<SearchShouldContinue(size_t, size_t)>&);
    static std::optional<SimpleRange> findNextMatch(StringView, const Vector<TextRun>&, unsigned startOffset, const String& target, FindOptions, const std::optional<SimpleRange>& excludeRange = std::nullopt);
    static unsigned bufferOffsetForBoundaryPoint(StringView, const Vector<TextRun>&, const BoundaryPoint&, FindOptions);
    static unsigned startingOffsetForSelection(StringView, const Vector<TextRun>&, const SimpleRange&, FindOptions);
    static SimpleRange bufferRangeToSimpleRange(const Vector<TextRun>&, size_t start, size_t end);
    enum class BoundaryEdge : bool { Start, End };
    static BoundaryPoint boundaryForOffset(const Vector<TextRun>&, unsigned position, BoundaryEdge);

    WeakRef<Document, WeakPtrImplWithEventTargetData> m_document;
    TextRunCache m_flatTreeBuffer;
    TextRunCache m_docBuffer;
    std::optional<Vector<SimpleRange>> m_matchCache;
    std::optional<size_t> m_countCache;

    struct TextBufferCacheKeys {
        uint64_t domTreeVersion { 0 };
        unsigned styleRecalcCount { 0 };
    };

    struct SearchResultCacheKeys {
        std::optional<String> targetString;
        std::optional<unsigned> limit;
        std::optional<FindOptions> options;
    };

    TextBufferCacheKeys m_textBufferCacheKeys;
    SearchResultCacheKeys m_searchResultCacheKeys;
};

} // namespace WebCore
