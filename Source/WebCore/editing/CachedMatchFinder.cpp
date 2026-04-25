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

#include "config.h"
#include "CachedMatchFinder.h"

#include "Document.h"
#include "ICUSearcher.h"
#include "NodeInlines.h"
#include "ShadowRoot.h"
#include "SimpleRange.h"
#include "TextIterator.h"
#include "TextIteratorBehavior.h"
#include "dom/BoundaryPoint.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static inline FindOptions matchAffectingOptions(FindOptions options)
{
    static constexpr OptionSet matchAffectingFlags { FindOption::CaseInsensitive, FindOption::AtWordStarts, FindOption::TreatMedialCapitalAsWordStart, FindOption::AtWordEnds, FindOption::DoNotTraverseFlatTree };
    return options & matchAffectingFlags;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(CachedMatchFinder);

CachedMatchFinder::CachedMatchFinder(Document& document)
    : m_document(document)
{
}

void CachedMatchFinder::performSearch(StringView buffer, unsigned startOffset, const String& target, FindOptions options, NOESCAPE const Function<SearchShouldContinue(size_t, size_t)>& callback)
{
    if (buffer.isEmpty() || target.isEmpty())
        return;

    String foldedTarget = foldQuoteMarks(target);
    bool targetRequiresKanaWorkaround = containsKanaLetters(foldedTarget);

    Vector<char16_t> normalizedTarget;
    if (targetRequiresKanaWorkaround) {
        StringView::UpconvertedCharacters upconverted(foldedTarget);
        normalizeCharacters(upconverted, foldedTarget.length(), normalizedTarget);
    }

    StringView::UpconvertedCharacters targetChars(foldedTarget);

    ICUSearcher icuSearcher { foldedTarget, options };
    icuSearcher.setPattern(targetChars.span());
    StringView::UpconvertedCharacters upconverted(buffer);
    auto bufferSpan = upconverted.span();
    icuSearcher.setText(bufferSpan);

    bool backwards = options.contains(FindOption::Backwards);

    Vector<char16_t> scratchBuffer;

    auto isMatch = [&](int matchStart, size_t matchLength) -> bool {
        if (targetRequiresKanaWorkaround && isBadMatch(bufferSpan.subspan(matchStart, matchLength), normalizedTarget.span(), scratchBuffer))
            return false;
        if (options.contains(FindOption::AtWordStarts) && !isWordStartMatch(bufferSpan, matchStart, matchLength, options))
            return false;
        if (options.contains(FindOption::AtWordEnds) && !isWordEndMatch(bufferSpan, matchStart, matchLength, options))
            return false;
        return true;
    };

    if (backwards) {
#if !PLATFORM(PLAYSTATION)
        icuSearcher.setOffset(startOffset);
        while (true) {
            std::optional<size_t> matchStartCandidate = icuSearcher.previous();
            if (!matchStartCandidate)
                break;
            size_t matchLength = static_cast<size_t>(icuSearcher.matchedLength());
            if (!isMatch(*matchStartCandidate, matchLength))
                continue;
            if (callback(*matchStartCandidate, *matchStartCandidate + matchLength) == SearchShouldContinue::No)
                break;
        }
#else
        icuSearcher.setOffset(0);
        Vector<std::pair<size_t, size_t>> matches;
        while (true) {
            std::optional<size_t> matchStartCandidate = icuSearcher.next();
            if (!matchStartCandidate || *matchStartCandidate >= startOffset)
                break;
            size_t matchLength = static_cast<size_t>(icuSearcher.matchedLength());
            if (!isMatch(*matchStartCandidate, matchLength))
                continue;
            matches.append({ *matchStartCandidate, *matchStartCandidate + matchLength });
        }
        for (auto [start, end] : matches | std::views::reverse) {
            if (callback(start, end) == SearchShouldContinue::No)
                break;
        }
#endif
    } else {
        icuSearcher.setOffset(startOffset);
        while (true) {
            std::optional<size_t> matchStartCandidate = icuSearcher.next();
            if (!matchStartCandidate)
                break;
            size_t matchLength = icuSearcher.matchedLength();
            if (!isMatch(*matchStartCandidate, matchLength))
                continue;
            if (callback(*matchStartCandidate, *matchStartCandidate + matchLength) == SearchShouldContinue::No)
                break;
        }
    }
}

static bool matchIsWithinSingleScope(const SimpleRange& range)
{
    return &range.start.container->rootNode() == &range.end.container->rootNode();
}

std::optional<SimpleRange> CachedMatchFinder::findNextMatch(StringView buffer, const Vector<TextRun>& runs, unsigned startOffset, const String& target, FindOptions options, const std::optional<SimpleRange>& excludeRange)
{
    std::optional<SimpleRange> result;
    performSearch(buffer, startOffset, target, options, [&](size_t start, size_t end) {
        auto matchRange = bufferRangeToSimpleRange(runs, start, end);
        if (excludeRange && matchRange == *excludeRange)
            return SearchShouldContinue::Yes;
        if (!matchIsWithinSingleScope(matchRange))
            return SearchShouldContinue::Yes;
        result = matchRange;
        return SearchShouldContinue::No;
    });
    return result;
}

std::optional<SimpleRange> CachedMatchFinder::findNextMatchInShadowIncludingAncestorTree(ShadowRoot& startingShadowRoot, const SimpleRange& selectionRange, const String& target, FindOptions options)
{
    RefPtr shadowRoot = &startingShadowRoot;
    auto [shadowBuffer, shadowRuns] = textForScope(*shadowRoot, options);

    unsigned startOffset = startingOffsetForSelection(shadowBuffer, shadowRuns, selectionRange, options);
    if (auto result = findNextMatch(shadowBuffer, shadowRuns, startOffset, target, options, selectionRange))
        return result;

    while (RefPtr host = shadowRoot->shadowHost()) {
        auto afterHost = options.contains(FindOption::Backwards)
            ? makeBoundaryPointBeforeNode(*host)
            : makeBoundaryPointAfterNode(*host);
        if (!afterHost)
            break;

        RefPtr parentShadow = host->containingShadowRoot();
        if (!parentShadow) {
            auto& cached = bufferForOptions(options);
            unsigned docOffset = bufferOffsetForBoundaryPoint(cached.text, cached.runs, *afterHost, options);
            if (auto result = findNextMatch(cached.text, cached.runs, docOffset, target, options))
                return result;
            if (options.contains(FindOption::WrapAround))
                return findNextMatch(cached.text, cached.runs, 0, target, options);
            return std::nullopt;
        }

        std::tie(shadowBuffer, shadowRuns) = textForScope(*parentShadow, options);
        unsigned parentOffset = bufferOffsetForBoundaryPoint(shadowBuffer, shadowRuns, *afterHost, options);
        if (auto result = findNextMatch(shadowBuffer, shadowRuns, parentOffset, target, options))
            return result;
        shadowRoot = parentShadow;
    }

    return std::nullopt;
}

std::optional<SimpleRange> CachedMatchFinder::findMatchFrom(const std::optional<SimpleRange>& selectionRange, const String& target, FindOptions options)
{
    if (!isTextBufferCacheValid()) {
        if (!clearTextBufferCache())
            return std::nullopt;
    }

    RefPtr shadowRoot = selectionRange ? selectionRange->startContainer().containingShadowRoot() : nullptr;
    if (shadowRoot && options.contains(FindOption::DoNotTraverseFlatTree))
        return findNextMatchInShadowIncludingAncestorTree(*shadowRoot, *selectionRange, target, options);

    auto& cached = bufferForOptions(options);
    unsigned startOffset;
    if (selectionRange)
        startOffset = startingOffsetForSelection(cached.text, cached.runs, *selectionRange, options);
    else
        startOffset = options.contains(FindOption::Backwards) ? cached.text.length() : 0;

    if (auto result = findNextMatch(cached.text, cached.runs, startOffset, target, options, selectionRange))
        return result;

    if (selectionRange && options.contains(FindOption::WrapAround)) {
        unsigned wrapOffset = options.contains(FindOption::Backwards) ? cached.text.length() : 0;
        return findNextMatch(cached.text, cached.runs, wrapOffset, target, options);
    }

    return std::nullopt;
}

Vector<SimpleRange> CachedMatchFinder::findMatches(const std::optional<SimpleRange>& searchRange, const String& target, FindOptions options, std::optional<unsigned> limit)
{
    if (!isTextBufferCacheValid()) {
        if (!clearTextBufferCache())
            return { };
    } else if (isSearchResultCacheValid(target, options, limit) && m_matchCache)
        return *m_matchCache;

    auto& cached = bufferForOptions(options);

    unsigned startOffset = searchRange ? bufferOffsetForBoundaryPoint(cached.text, cached.runs, searchRange->start, options) : 0;
    Vector<SimpleRange> results;
    performSearch(cached.text, startOffset, target, options, [&](size_t start, size_t end) {
        results.append(bufferRangeToSimpleRange(cached.runs, start, end));
        return !limit || results.size() < *limit ? SearchShouldContinue::Yes : SearchShouldContinue::No;
    });

    m_matchCache = results;
    m_countCache = results.size();
    m_searchResultCacheKeys.targetString = target;
    m_searchResultCacheKeys.limit = limit;
    m_searchResultCacheKeys.options = matchAffectingOptions(options);
    return results;
}

unsigned CachedMatchFinder::countMatches(const std::optional<SimpleRange>& searchRange, const String& target, FindOptions options, std::optional<unsigned> limit)
{
    if (!isTextBufferCacheValid()) {
        if (!clearTextBufferCache())
            return 0;
    } else if (isSearchResultCacheValid(target, options, limit) && m_countCache)
        return *m_countCache;

    auto& cached = bufferForOptions(options);

    unsigned count { 0 };
    unsigned startOffset = searchRange ? bufferOffsetForBoundaryPoint(cached.text, cached.runs, searchRange->start, options) : 0;
    performSearch(cached.text, startOffset, target, options, [&](size_t, size_t) {
        ++count;
        return !limit || count < *limit ? SearchShouldContinue::Yes : SearchShouldContinue::No;
    });

    m_countCache = count;
    m_searchResultCacheKeys.targetString = target;
    m_searchResultCacheKeys.limit = limit;
    m_searchResultCacheKeys.options = matchAffectingOptions(options);
    return count;
}

unsigned CachedMatchFinder::bufferOffsetForBoundaryPoint(StringView buffer, const Vector<TextRun>& runs, const BoundaryPoint& point, FindOptions options)
{
    std::optional<unsigned> lastChunkEnd = std::nullopt;
    for (auto [i, run] : indexedRange(runs)) {
        if (&run.range.start.container.get() != &point.container.get())
            continue;
        if (point.offset < run.range.start.offset)
            continue;

        if (point.offset <= run.range.end.offset)
            return run.offset + (point.offset - run.range.start.offset);

        lastChunkEnd = i + 1 < runs.size() ? runs[i + 1].offset : buffer.length();
    }

    if (lastChunkEnd)
        return *lastChunkEnd;

    for (const auto& run : runs) {
        auto order = options.contains(FindOption::DoNotTraverseFlatTree)
            ? treeOrder<ShadowIncludingTree>(run.range.start, point)
            : treeOrder<ComposedTree>(run.range.start, point);
        if (std::is_gteq(order))
            return run.offset;
    }

    return buffer.length();
}

unsigned CachedMatchFinder::startingOffsetForSelection(StringView buffer, const Vector<TextRun>& runs, const SimpleRange& selectionRange, FindOptions options)
{
    bool backwards = options.contains(FindOption::Backwards);
    bool startInSelection = options.contains(FindOption::StartInSelection);

    auto& boundary = startInSelection == backwards ? selectionRange.end : selectionRange.start;
    unsigned offset = bufferOffsetForBoundaryPoint(buffer, runs, boundary, options);
    if (!backwards && !startInSelection && offset > 0)
        --offset;
    return offset;
}

bool CachedMatchFinder::isTextBufferCacheValid() const
{
    RefPtr document = m_document.get();
    if (!document)
        return false;
    return document->domTreeVersion() == m_textBufferCacheKeys.domTreeVersion && document->styleRecalcCount() == m_textBufferCacheKeys.styleRecalcCount;
}

bool CachedMatchFinder::clearTextBufferCache()
{
    RefPtr document = m_document.get();
    if (!document) {
        m_flatTreeBuffer.dirty = true;
        m_docBuffer.dirty = true;
        m_searchResultCacheKeys.targetString = std::nullopt;
        m_searchResultCacheKeys.limit = std::nullopt;
        m_searchResultCacheKeys.options = std::nullopt;
        m_matchCache = std::nullopt;
        m_countCache = std::nullopt;
        return false;
    }

    m_textBufferCacheKeys.domTreeVersion = document->domTreeVersion();
    m_textBufferCacheKeys.styleRecalcCount = document->styleRecalcCount();
    m_searchResultCacheKeys.targetString = std::nullopt;
    m_searchResultCacheKeys.limit = std::nullopt;
    m_searchResultCacheKeys.options = std::nullopt;
    m_flatTreeBuffer.dirty = true;
    m_docBuffer.dirty = true;
    m_matchCache = std::nullopt;
    m_countCache = std::nullopt;
    return true;
}

CachedMatchFinder::TextRunCache& CachedMatchFinder::bufferForOptions(FindOptions options)
{
    auto& cache = options.contains(FindOption::DoNotTraverseFlatTree) ? m_docBuffer : m_flatTreeBuffer;
    if (RefPtr document = m_document.get(); cache.dirty) {
        std::tie(cache.text, cache.runs) = textForScope(*document, options);
        cache.dirty = false;
    }
    return cache;
}

auto CachedMatchFinder::textForScope(ContainerNode& scope, FindOptions options) -> std::pair<String, Vector<TextRun>> {
    protect(scope.document())->updateLayoutIgnorePendingStylesheets({ LayoutOptions::TreatContentVisibilityAutoAsVisible, LayoutOptions::TreatRevealedWhenFoundAsVisible });
    SimpleRange range = makeRangeSelectingNodeContents(scope);
    TextIterator it(range, findIteratorOptions(options));

    StringBuilder builder;
    Vector<TextRun> runs;
    for (; !it.atEnd(); it.advance()) {
        auto textRunRange = it.range();
        runs.append(TextRun { static_cast<unsigned>(builder.length()), textRunRange });
        auto text = it.text();
        if (text.is8Bit()) {
            for (auto character : text.span8())
                builder.append(foldQuoteMarkAndReplaceNoBreakSpace(static_cast<char16_t>(character)));
        } else {
            for (auto character : text.span16())
                builder.append(foldQuoteMarkAndReplaceNoBreakSpace(character));
        }
    }

    return { builder.toString(), runs };
}

BoundaryPoint CachedMatchFinder::boundaryForOffset(const Vector<CachedMatchFinder::TextRun>& runs, unsigned position, BoundaryEdge boundaryEdge)
{
    ASSERT(!runs.isEmpty());

    auto it = std::partition_point(runs.begin(), runs.end(), [position, boundaryEdge](const CachedMatchFinder::TextRun& run) {
        return boundaryEdge == BoundaryEdge::Start ? run.offset <= position : run.offset < position;
    });

    size_t index = it != runs.begin() ? static_cast<size_t>(it - runs.begin()) - 1 : 0;

    auto& run = runs[index];
    RELEASE_ASSERT(run.offset <= position);
    unsigned offsetWithinChunk = static_cast<unsigned>(position - run.offset);
    unsigned domOffset = std::min(run.range.start.offset + offsetWithinChunk, run.range.end.offset);
    return { run.range.start.container.copyRef(), domOffset };
}

SimpleRange CachedMatchFinder::bufferRangeToSimpleRange(const Vector<TextRun>& runs, size_t start, size_t end)
{
    return {
        boundaryForOffset(runs, static_cast<unsigned>(start), BoundaryEdge::Start),
        boundaryForOffset(runs, static_cast<unsigned>(end), BoundaryEdge::End)
    };
}

bool CachedMatchFinder::isSearchResultCacheValid(const String& target, FindOptions options, std::optional<unsigned> limit) const
{
    RefPtr document = m_document.get();
    if (!document)
        return false;
    if (!isTextBufferCacheValid())
        return false;
    return m_searchResultCacheKeys.targetString
        && target == *m_searchResultCacheKeys.targetString
        && m_searchResultCacheKeys.limit == limit
        && m_searchResultCacheKeys.options == matchAffectingOptions(options);
}

} // namespace WebCore
