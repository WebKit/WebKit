#include "CompressionDictionaryMatcher.h"

#include "ResourceRequest.h"
#include "URLPattern.h"
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/Optional.h>

namespace WebCore {

// Dummy context for non-JS pattern usage
static ScriptExecutionContext* dummyContext()
{
    static NeverDestroyed<ScriptExecutionContext*> context { nullptr };
    return context.get();
}

// Validate that pattern is syntactically valid, same-origin, no regex groups
bool isValidDictionaryPattern(const String& match, const URL& dictionaryURL)
{
    auto patternOrException = URLPattern::create(*dummyContext(), match, dictionaryURL.string(), URLPatternOptions { });
    if (patternOrException.hasException())
        return false;

    auto& pattern = patternOrException.returnValue();

    // IETF compliance: ensure pattern's resolved URL is same-origin
    URL resolvedURL(match, &dictionaryURL);
    if (!resolvedURL.protocolHostAndPortAreEqual(dictionaryURL))
        return false;

    return !pattern->hasRegExpGroups();
}


// Check if request matches dictionary rules (origin, match-dest, match pattern)
bool doesRequestMatchDictionary(const ResourceRequest& request, const CompressionDictionary& dict)
{
    const URL& requestURL = request.url();
    const URL& dictionaryURL = dict.dictionaryURL;

    // 1. Origin match
    if (!dictionaryURL.protocolHostAndPortAreEqual(requestURL))
        return false;

    // 2. match-dest (if applicable)
    if (!dict.matchDest.isEmpty()) {
        auto requestDest = request.destination();
        if (!dict.matchDest.contains(requestDest))
            return false;
    }

    // 3. URL pattern match
    auto patternOrException = URLPattern::create(*dummyContext(), dict.match, dictionaryURL.string(), URLPatternOptions { });
    if (patternOrException.hasException())
        return false;

    auto& pattern = patternOrException.returnValue();
    auto matchResult = pattern->test(*dummyContext(), requestURL.string(), { });
    return matchResult.hasException() ? false : matchResult.returnValue();
}

// Apply prioritization:
// 1. match-dest > no match-dest
// 2. longest match string
// 3. most recent fetch time
std::optional<CompressionDictionary> selectBestMatchingDictionary(const ResourceRequest& request, const Vector<CompressionDictionary>& candidates)
{
    CompressionDictionary const* best = nullptr;

    for (const auto& dict : candidates) {
        if (!doesRequestMatchDictionary(request, dict))
            continue;

        if (!best) {
            best = &dict;
            continue;
        }

        // Rule 1: Prefer match-dest
        bool bestHasDest = !best->matchDest.isEmpty();
        bool currentHasDest = !dict.matchDest.isEmpty();
        if (bestHasDest != currentHasDest) {
            best = currentHasDest ? &dict : best;
            continue;
        }

        // Rule 2: Prefer longer match
        if (dict.match.length() > best->match.length()) {
            best = &dict;
            continue;
        }

        // Rule 3: Prefer more recently fetched
        if (dict.match.length() == best->match.length() && dict.fetchTime > best->fetchTime) {
            best = &dict;
        }
    }

    if (!best)
        return std::nullopt;

    return *best;
}

}
// Copyright © 2025  All rights reserved.

