/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "Test.h"
#include <WebCore/MarkerSubrange.h>
#include <WebCore/RenderedDocumentMarker.h>

using namespace WebCore;

namespace WebCore {

std::ostream& operator<<(std::ostream& os, MarkerSubrange::Type type)
{
    switch (type) {
    case MarkerSubrange::Correction:
        return os << "Correction";
    case MarkerSubrange::DictationAlternatives:
        return os << "DictationAlternatives";
#if PLATFORM(IOS)
    // FIXME: See <rdar://problem/8933352>. Also, remove the PLATFORM(IOS)-guard.
    case MarkerSubrange::DictationPhraseWithAlternatives:
        return os << "DictationPhraseWithAlternatives";
#endif
    case MarkerSubrange::GrammarError:
        return os << "GrammarError";
    case MarkerSubrange::Selection:
        return os << "Selection";
    case MarkerSubrange::SpellingError:
        return os << "SpellingError";
    case MarkerSubrange::TextMatch:
        return os << "TextMatch";
    case MarkerSubrange::Unmarked:
        return os << "Unmarked";
    }
}

std::ostream& operator<<(std::ostream& os, const MarkerSubrange& subrange)
{
    os << "(" << subrange.startOffset << ", " << subrange.endOffset << ", " << subrange.type;
    if (subrange.marker)
        os << static_cast<const void*>(subrange.marker);
    return os << ")";
}

bool operator==(const MarkerSubrange& a, const MarkerSubrange& b)
{
    return a.startOffset == b.startOffset && a.endOffset == b.endOffset && a.type == b.type && a.marker == b.marker;
}

}

namespace TestWebKitAPI {

TEST(MarkerSubrange, SubdivideEmpty)
{
    EXPECT_EQ(0U, subdivide({ }).size());
    EXPECT_EQ(0U, subdivide({ }, OverlapStrategy::Frontmost).size());
}

TEST(MarkerSubrange, SubdivideSimple)
{
    MarkerSubrange subrange { 0, 9, MarkerSubrange::SpellingError };
    auto results = subdivide({ subrange });
    ASSERT_EQ(1U, results.size());
    EXPECT_EQ(subrange, results[0]);
}

TEST(MarkerSubrange, SubdivideSpellingAndGrammarSimple)
{
    RenderedDocumentMarker grammarErrorMarker { DocumentMarker { DocumentMarker::Grammar, 7, 8 } };
    Vector<MarkerSubrange> expectedSubranges {
        MarkerSubrange { grammarErrorMarker.startOffset(), grammarErrorMarker.endOffset(), MarkerSubrange::GrammarError, &grammarErrorMarker },
        MarkerSubrange { 22, 32, MarkerSubrange::SpellingError },
    };
    auto results = subdivide(expectedSubranges);
    ASSERT_EQ(expectedSubranges.size(), results.size());
    for (size_t i = 0; i < expectedSubranges.size(); ++i)
        EXPECT_EQ(expectedSubranges[i], results[i]);
}

TEST(MarkerSubrange, SubdivideSpellingAndGrammarOverlap)
{
    Vector<MarkerSubrange> subranges {
        MarkerSubrange { 0, 40, MarkerSubrange::GrammarError },
        MarkerSubrange { 2, 17, MarkerSubrange::SpellingError },
        MarkerSubrange { 20, 40, MarkerSubrange::SpellingError },
        MarkerSubrange { 41, 45, MarkerSubrange::SpellingError },
    };
    Vector<MarkerSubrange> expectedSubranges {
        MarkerSubrange { 0, 2, MarkerSubrange::GrammarError },
        MarkerSubrange { 2, 17, MarkerSubrange::GrammarError },
        MarkerSubrange { 2, 17, MarkerSubrange::SpellingError },
        MarkerSubrange { 17, 20, MarkerSubrange::GrammarError },
        MarkerSubrange { 20, 40, MarkerSubrange::GrammarError },
        MarkerSubrange { 20, 40, MarkerSubrange::SpellingError },
        MarkerSubrange { 41, 45, MarkerSubrange::SpellingError },
    };
    auto results = subdivide(subranges);
    ASSERT_EQ(expectedSubranges.size(), results.size());
    for (size_t i = 0; i < expectedSubranges.size(); ++i)
        EXPECT_EQ(expectedSubranges[i], results[i]);
}

TEST(MarkerSubrange, SubdivideSpellingAndGrammarOverlapFrontmost)
{
    Vector<MarkerSubrange> subranges {
        MarkerSubrange { 0, 40, MarkerSubrange::GrammarError },
        MarkerSubrange { 2, 17, MarkerSubrange::SpellingError },
        MarkerSubrange { 20, 40, MarkerSubrange::SpellingError },
        MarkerSubrange { 41, 45, MarkerSubrange::SpellingError },
    };
    Vector<MarkerSubrange> expectedSubranges {
        MarkerSubrange { 0, 2, MarkerSubrange::GrammarError },
        MarkerSubrange { 2, 17, MarkerSubrange::SpellingError },
        MarkerSubrange { 17, 20, MarkerSubrange::GrammarError },
        MarkerSubrange { 20, 40, MarkerSubrange::SpellingError },
        MarkerSubrange { 41, 45, MarkerSubrange::SpellingError },
    };
    auto results = subdivide(subranges, OverlapStrategy::Frontmost);
    ASSERT_EQ(expectedSubranges.size(), results.size());
    for (size_t i = 0; i < expectedSubranges.size(); ++i)
        EXPECT_EQ(expectedSubranges[i], results[i]);
}

TEST(MarkerSubrange, SubdivideSpellingAndGrammarComplicatedFrontmost)
{
    Vector<MarkerSubrange> subranges {
        MarkerSubrange { 0, 6, MarkerSubrange::SpellingError },
        MarkerSubrange { 0, 46, MarkerSubrange::GrammarError },
        MarkerSubrange { 7, 16, MarkerSubrange::SpellingError },
        MarkerSubrange { 22, 27, MarkerSubrange::SpellingError },
        MarkerSubrange { 34, 44, MarkerSubrange::SpellingError },
        MarkerSubrange { 46, 50, MarkerSubrange::SpellingError },
        MarkerSubrange { 51, 58, MarkerSubrange::SpellingError },
        MarkerSubrange { 59, 63, MarkerSubrange::GrammarError },
    };
    Vector<MarkerSubrange> expectedSubranges {
        MarkerSubrange { 0, 6, MarkerSubrange::SpellingError },
        MarkerSubrange { 6, 7, MarkerSubrange::GrammarError },
        MarkerSubrange { 7, 16, MarkerSubrange::SpellingError },
        MarkerSubrange { 16, 22, MarkerSubrange::GrammarError },
        MarkerSubrange { 22, 27, MarkerSubrange::SpellingError },
        MarkerSubrange { 27, 34, MarkerSubrange::GrammarError },
        MarkerSubrange { 34, 44, MarkerSubrange::SpellingError },
        MarkerSubrange { 44, 46, MarkerSubrange::GrammarError },
        MarkerSubrange { 46, 50, MarkerSubrange::SpellingError },
        MarkerSubrange { 51, 58, MarkerSubrange::SpellingError },
        MarkerSubrange { 59, 63, MarkerSubrange::GrammarError },
    };
    auto results = subdivide(subranges, OverlapStrategy::Frontmost);
    ASSERT_EQ(expectedSubranges.size(), results.size());
    for (size_t i = 0; i < expectedSubranges.size(); ++i)
        EXPECT_EQ(expectedSubranges[i], results[i]);
}

TEST(MarkerSubrange, SubdivideGrammarAndSelectionOverlapFrontmost)
{
    Vector<MarkerSubrange> subranges {
        MarkerSubrange { 0, 40, MarkerSubrange::GrammarError },
        MarkerSubrange { 2, 60, MarkerSubrange::Selection },
        MarkerSubrange { 50, 60, MarkerSubrange::GrammarError },
    };
    Vector<MarkerSubrange> expectedSubranges {
        MarkerSubrange { 0, 2, MarkerSubrange::GrammarError },
        MarkerSubrange { 2, 40, MarkerSubrange::Selection },
        MarkerSubrange { 40, 50, MarkerSubrange::Selection },
        MarkerSubrange { 50, 60, MarkerSubrange::Selection },
    };
    auto results = subdivide(subranges, OverlapStrategy::Frontmost);
    ASSERT_EQ(expectedSubranges.size(), results.size());
    for (size_t i = 0; i < expectedSubranges.size(); ++i)
        EXPECT_EQ(expectedSubranges[i], results[i]);
}

TEST(MarkerSubrange, SubdivideGrammarAndSelectionOverlapFrontmostWithLongestEffectiveRange)
{
    Vector<MarkerSubrange> subranges {
        MarkerSubrange { 0, 40, MarkerSubrange::GrammarError },
        MarkerSubrange { 2, 60, MarkerSubrange::Selection },
        MarkerSubrange { 50, 60, MarkerSubrange::GrammarError },
    };
    Vector<MarkerSubrange> expectedSubranges {
        MarkerSubrange { 0, 2, MarkerSubrange::GrammarError },
        MarkerSubrange { 2, 60, MarkerSubrange::Selection },
    };
    auto results = subdivide(subranges, OverlapStrategy::FrontmostWithLongestEffectiveRange);
    ASSERT_EQ(expectedSubranges.size(), results.size());
    for (size_t i = 0; i < expectedSubranges.size(); ++i)
        EXPECT_EQ(expectedSubranges[i], results[i]);
}

}
