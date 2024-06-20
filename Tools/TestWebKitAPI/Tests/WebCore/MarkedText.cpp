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
#include <WebCore/MarkedText.h>
#include <WebCore/RenderedDocumentMarker.h>

using namespace WebCore;

namespace WebCore {

std::ostream& operator<<(std::ostream& os, MarkedText::Type type)
{
    switch (type) {
    case MarkedText::Type::Correction:
        return os << "Correction";
    case MarkedText::Type::DictationAlternatives:
        return os << "DictationAlternatives";
#if PLATFORM(IOS_FAMILY)
    // FIXME: See <rdar://problem/8933352>. Also, remove the PLATFORM(IOS_FAMILY)-guard.
    case MarkedText::Type::DictationPhraseWithAlternatives:
        return os << "DictationPhraseWithAlternatives";
#endif
    case MarkedText::Type::DraggedContent:
        return os << "DraggedContent";
    case MarkedText::Type::GrammarError:
        return os << "GrammarError";
    case MarkedText::Type::Selection:
        return os << "Selection";
    case MarkedText::Type::SpellingError:
        return os << "SpellingError";
    case MarkedText::Type::TextMatch:
        return os << "TextMatch";
    case MarkedText::Type::Highlight:
        return os << "Highlight";
    case MarkedText::Type::FragmentHighlight:
        return os << "FragmentHighlight";
#if ENABLE(APP_HIGHLIGHTS)
    case MarkedText::Type::AppHighlight:
        return os << "AppHighlight";
#endif
#if ENABLE(WRITING_TOOLS)
    case MarkedText::Type::WritingToolsTextSuggestion:
        return os << "WritingToolsTextSuggestion";
#endif
    case MarkedText::Type::TransparentContent:
        return os << "TransparentContent";
    case MarkedText::Type::Unmarked:
        return os << "Unmarked";
    }
}

std::ostream& operator<<(std::ostream& os, const MarkedText& markedText)
{
    os << "(" << markedText.startOffset << ", " << markedText.endOffset << ", " << markedText.type;
    if (markedText.marker)
        os << static_cast<const void*>(markedText.marker);
    return os << ")";
}

}

namespace TestWebKitAPI {

TEST(MarkedText, SubdivideEmpty)
{
    EXPECT_EQ(0U, MarkedText::subdivide({ }).size());
    EXPECT_EQ(0U, MarkedText::subdivide({ }, MarkedText::OverlapStrategy::Frontmost).size());
}

TEST(MarkedText, SubdivideSimple)
{
    MarkedText markedText { 0, 9, MarkedText::Type::SpellingError };
    auto results = MarkedText::subdivide({ markedText });
    ASSERT_EQ(1U, results.size());
    EXPECT_EQ(markedText, results[0]);
}

TEST(MarkedText, SubdivideSpellingAndGrammarSimple)
{
    RenderedDocumentMarker grammarErrorMarker { DocumentMarker { DocumentMarker::Type::Grammar, { 7, 8 } } };
    Vector<MarkedText> expectedMarkedTexts {
        MarkedText { grammarErrorMarker.startOffset(), grammarErrorMarker.endOffset(), MarkedText::Type::GrammarError, &grammarErrorMarker },
        MarkedText { 22, 32, MarkedText::Type::SpellingError },
    };
    auto results = MarkedText::subdivide(expectedMarkedTexts);
    ASSERT_EQ(expectedMarkedTexts.size(), results.size());
    for (size_t i = 0; i < expectedMarkedTexts.size(); ++i)
        EXPECT_EQ(expectedMarkedTexts[i], results[i]);
}

TEST(MarkedText, SubdivideSpellingAndGrammarOverlap)
{
    Vector<MarkedText> markedTexts {
        MarkedText { 0, 40, MarkedText::Type::GrammarError },
        MarkedText { 2, 17, MarkedText::Type::SpellingError },
        MarkedText { 20, 40, MarkedText::Type::SpellingError },
        MarkedText { 41, 45, MarkedText::Type::SpellingError },
    };
    Vector<MarkedText> expectedMarkedTexts {
        MarkedText { 0, 2, MarkedText::Type::GrammarError },
        MarkedText { 2, 17, MarkedText::Type::GrammarError },
        MarkedText { 2, 17, MarkedText::Type::SpellingError },
        MarkedText { 17, 20, MarkedText::Type::GrammarError },
        MarkedText { 20, 40, MarkedText::Type::GrammarError },
        MarkedText { 20, 40, MarkedText::Type::SpellingError },
        MarkedText { 41, 45, MarkedText::Type::SpellingError },
    };
    auto results = MarkedText::subdivide(markedTexts);
    ASSERT_EQ(expectedMarkedTexts.size(), results.size());
    for (size_t i = 0; i < expectedMarkedTexts.size(); ++i)
        EXPECT_EQ(expectedMarkedTexts[i], results[i]);
}

TEST(MarkedText, SubdivideSpellingAndGrammarOverlapFrontmost)
{
    Vector<MarkedText> markedTexts {
        MarkedText { 0, 40, MarkedText::Type::GrammarError },
        MarkedText { 2, 17, MarkedText::Type::SpellingError },
        MarkedText { 20, 40, MarkedText::Type::SpellingError },
        MarkedText { 41, 45, MarkedText::Type::SpellingError },
    };
    Vector<MarkedText> expectedMarkedTexts {
        MarkedText { 0, 2, MarkedText::Type::GrammarError },
        MarkedText { 2, 17, MarkedText::Type::SpellingError },
        MarkedText { 17, 20, MarkedText::Type::GrammarError },
        MarkedText { 20, 40, MarkedText::Type::SpellingError },
        MarkedText { 41, 45, MarkedText::Type::SpellingError },
    };
    auto results = MarkedText::subdivide(markedTexts, MarkedText::OverlapStrategy::Frontmost);
    ASSERT_EQ(expectedMarkedTexts.size(), results.size());
    for (size_t i = 0; i < expectedMarkedTexts.size(); ++i)
        EXPECT_EQ(expectedMarkedTexts[i], results[i]);
}

TEST(MarkedText, SubdivideSpellingAndGrammarComplicatedFrontmost)
{
    Vector<MarkedText> markedTexts {
        MarkedText { 0, 6, MarkedText::Type::SpellingError },
        MarkedText { 0, 46, MarkedText::Type::GrammarError },
        MarkedText { 7, 16, MarkedText::Type::SpellingError },
        MarkedText { 22, 27, MarkedText::Type::SpellingError },
        MarkedText { 34, 44, MarkedText::Type::SpellingError },
        MarkedText { 46, 50, MarkedText::Type::SpellingError },
        MarkedText { 51, 58, MarkedText::Type::SpellingError },
        MarkedText { 59, 63, MarkedText::Type::GrammarError },
    };
    Vector<MarkedText> expectedMarkedTexts {
        MarkedText { 0, 6, MarkedText::Type::SpellingError },
        MarkedText { 6, 7, MarkedText::Type::GrammarError },
        MarkedText { 7, 16, MarkedText::Type::SpellingError },
        MarkedText { 16, 22, MarkedText::Type::GrammarError },
        MarkedText { 22, 27, MarkedText::Type::SpellingError },
        MarkedText { 27, 34, MarkedText::Type::GrammarError },
        MarkedText { 34, 44, MarkedText::Type::SpellingError },
        MarkedText { 44, 46, MarkedText::Type::GrammarError },
        MarkedText { 46, 50, MarkedText::Type::SpellingError },
        MarkedText { 51, 58, MarkedText::Type::SpellingError },
        MarkedText { 59, 63, MarkedText::Type::GrammarError },
    };
    auto results = MarkedText::subdivide(markedTexts, MarkedText::OverlapStrategy::Frontmost);
    ASSERT_EQ(expectedMarkedTexts.size(), results.size());
    for (size_t i = 0; i < expectedMarkedTexts.size(); ++i)
        EXPECT_EQ(expectedMarkedTexts[i], results[i]);
}

TEST(MarkedText, SubdivideGrammarAndSelectionOverlap)
{
    Vector<MarkedText> markedTexts {
        MarkedText { 0, 40, MarkedText::Type::GrammarError },
        MarkedText { 2, 60, MarkedText::Type::Selection },
        MarkedText { 50, 60, MarkedText::Type::GrammarError },
    };
    Vector<MarkedText> expectedMarkedTexts {
        MarkedText { 0, 2, MarkedText::Type::GrammarError },
        MarkedText { 2, 40, MarkedText::Type::GrammarError },
        MarkedText { 2, 40, MarkedText::Type::Selection },
        MarkedText { 40, 50, MarkedText::Type::Selection },
        MarkedText { 50, 60, MarkedText::Type::GrammarError },
        MarkedText { 50, 60, MarkedText::Type::Selection },
    };
    auto results = MarkedText::subdivide(markedTexts);
    ASSERT_EQ(expectedMarkedTexts.size(), results.size());
    for (size_t i = 0; i < expectedMarkedTexts.size(); ++i)
        EXPECT_EQ(expectedMarkedTexts[i], results[i]);
}

TEST(MarkedText, SubdivideGrammarAndSelectionOverlapFrontmost)
{
    Vector<MarkedText> markedTexts {
        MarkedText { 0, 40, MarkedText::Type::GrammarError },
        MarkedText { 2, 60, MarkedText::Type::Selection },
        MarkedText { 50, 60, MarkedText::Type::GrammarError },
    };
    Vector<MarkedText> expectedMarkedTexts {
        MarkedText { 0, 2, MarkedText::Type::GrammarError },
        MarkedText { 2, 40, MarkedText::Type::Selection },
        MarkedText { 40, 50, MarkedText::Type::Selection },
        MarkedText { 50, 60, MarkedText::Type::Selection },
    };
    auto results = MarkedText::subdivide(markedTexts, MarkedText::OverlapStrategy::Frontmost);
    ASSERT_EQ(expectedMarkedTexts.size(), results.size());
    for (size_t i = 0; i < expectedMarkedTexts.size(); ++i)
        EXPECT_EQ(expectedMarkedTexts[i], results[i]);
}

}
