/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "FragmentDirectiveGenerator.h"

#include "Document.h"
#include "FragmentDirectiveParser.h"
#include "FragmentDirectiveRangeFinder.h"
#include "FragmentDirectiveUtilities.h"
#include "HTMLParserIdioms.h"
#include "Logging.h"
#include "Range.h"
#include "SimpleRange.h"
#include "VisibleUnits.h"
#include <wtf/Deque.h>
#include <wtf/URL.h>
#include <wtf/URLParser.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
using namespace FragmentDirectiveUtilities;

constexpr int maximumInlineStringLength = 300;
constexpr int minimumContextlessStringLength = 20;
constexpr int defaultWordsOfContext = 3;
constexpr int maximumExtraWordsOfContext = 4;

FragmentDirectiveGenerator::FragmentDirectiveGenerator(const SimpleRange& textFragmentRange)
{
    generateFragmentDirective(textFragmentRange);
}

static bool positionsHaveSameBlockAncestor(const VisiblePosition& a, const VisiblePosition& b)
{
    RefPtr aNode = a.deepEquivalent().containerNode();
    RefPtr bNode = b.deepEquivalent().containerNode();
    return aNode && bNode && &nearestBlockAncestor(*aNode) == &nearestBlockAncestor(*bNode);
}

static String previousWordsFromPositionInSameBlock(unsigned numberOfWords, VisiblePosition& startPosition)
{
    auto previousPosition = startPosition;
    while (numberOfWords--) {
        auto potentialPreviousPosition = previousWordPosition(previousPosition);
        if (!positionsHaveSameBlockAncestor(potentialPreviousPosition, startPosition))
            break;
        previousPosition = potentialPreviousPosition;
    }

    auto document = startPosition.deepEquivalent().document();
    if (!document)
        return { };

    auto range = Range::create(*document);
    RefPtr startNode = previousPosition.deepEquivalent().containerNode();
    range->setStart(startNode.releaseNonNull(), previousPosition.deepEquivalent().computeOffsetInContainerNode());
    RefPtr endNode = startPosition.deepEquivalent().containerNode();
    range->setEnd(endNode.releaseNonNull(), startPosition.deepEquivalent().computeOffsetInContainerNode());

    return range->toString().trim(isHTMLSpaceButNotLineBreak);
}

static String nextWordsFromPositionInSameBlock(unsigned numberOfWords, VisiblePosition& startPosition)
{
    auto nextPosition = startPosition;
    while (numberOfWords--) {
        auto potentialNextPosition = nextWordPosition(nextPosition);
        if (!positionsHaveSameBlockAncestor(potentialNextPosition, startPosition))
            break;
        nextPosition = potentialNextPosition;
    }

    auto document = nextPosition.deepEquivalent().document();
    if (!document)
        return { };

    auto range = Range::create(*document);
    RefPtr startNode = startPosition.deepEquivalent().containerNode();
    range->setStart(startNode.releaseNonNull(), startPosition.deepEquivalent().computeOffsetInContainerNode());
    RefPtr endNode = nextPosition.deepEquivalent().containerNode();
    range->setEnd(endNode.releaseNonNull(), nextPosition.deepEquivalent().computeOffsetInContainerNode());

    return range->toString().trim(isHTMLSpaceButNotLineBreak);
}

// https://wicg.github.io/scroll-to-text-fragment/#generating-text-fragment-directives
void FragmentDirectiveGenerator::generateFragmentDirective(const SimpleRange& textFragmentRange)
{
    LOG_WITH_STREAM(TextFragment, stream << " generateFragmentDirective: ");

    Ref document = textFragmentRange.startContainer().document();
    document->updateLayoutIgnorePendingStylesheets();

    auto url = document->url();
    auto textFromRange = createLiveRange(textFragmentRange)->toString();

    VisiblePosition visibleStartPosition = VisiblePosition(Position(textFragmentRange.protectedStartContainer(), textFragmentRange.startOffset(), Position::PositionIsOffsetInAnchor));
    VisiblePosition visibleEndPosition = VisiblePosition(Position(textFragmentRange.protectedEndContainer(), textFragmentRange.endOffset(), Position::PositionIsOffsetInAnchor));

    auto generateDirective = [&] (unsigned wordsOfContext, unsigned wordsOfStartAndEndText) {
        ParsedTextDirective directive;

        if (textFromRange.length() >= maximumInlineStringLength) {
            directive.startText = nextWordsFromPositionInSameBlock(wordsOfStartAndEndText, visibleStartPosition);
            directive.endText = previousWordsFromPositionInSameBlock(wordsOfStartAndEndText, visibleEndPosition);
        } else
            directive.startText = textFromRange;

        if (wordsOfContext) {
            directive.prefix = previousWordsFromPositionInSameBlock(wordsOfContext, visibleStartPosition);
            directive.suffix = nextWordsFromPositionInSameBlock(wordsOfContext, visibleEndPosition);
        }

        return directive;
    };

    auto testDirective = [&] (ParsedTextDirective directive) {
        auto foundRange = FragmentDirectiveRangeFinder::findRangeFromTextDirective(directive, document.get());
        if (!foundRange)
            return false;

        return VisiblePosition(makeContainerOffsetPosition(foundRange->start)) == VisiblePosition(makeContainerOffsetPosition(textFragmentRange.start)) && VisiblePosition(makeContainerOffsetPosition(foundRange->end)) == VisiblePosition(makeContainerOffsetPosition(textFragmentRange.end));
    };

    auto wordsOfContext = textFromRange.length() < minimumContextlessStringLength ? defaultWordsOfContext : 0;
    auto wordsOfStartAndEndText = defaultWordsOfContext;

    auto directive = [&] -> std::optional<ParsedTextDirective> {
        for (unsigned extraWordsOfContext = 0; extraWordsOfContext <= maximumExtraWordsOfContext; extraWordsOfContext++) {
            auto directive = generateDirective(wordsOfContext + extraWordsOfContext, wordsOfStartAndEndText + extraWordsOfContext);
            if (testDirective(directive))
                return directive;
        }

        return std::nullopt;
    }();

    m_urlWithFragment = url;

    if (directive) {
        Vector<String> components;
        if (!directive->prefix.isEmpty())
            components.append(makeString(percentEncodeFragmentDirectiveSpecialCharacters(directive->prefix), '-'));
        if (!directive->startText.isEmpty())
            components.append(percentEncodeFragmentDirectiveSpecialCharacters(directive->startText));
        if (!directive->endText.isEmpty())
            components.append(percentEncodeFragmentDirectiveSpecialCharacters(directive->endText));
        if (!directive->suffix.isEmpty())
            components.append(makeString('-', percentEncodeFragmentDirectiveSpecialCharacters(directive->suffix)));

        static constexpr auto textDirectivePrefix = ":~:text="_s;
        m_urlWithFragment.setFragmentIdentifier(makeString(textDirectivePrefix, makeStringByJoining(components, ","_s)));
        LOG_WITH_STREAM(TextFragment, stream << "    Successfully generated fragment directive: " << m_urlWithFragment);
    } else
        LOG_WITH_STREAM(TextFragment, stream << "    Failed to generate fragment directive");
}

} // namespace WebCore
