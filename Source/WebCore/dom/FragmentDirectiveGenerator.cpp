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

constexpr int maximumInlineStringLength = 300;
constexpr int minimumInlineContextlessStringLength = 20;
constexpr int numberOfWordsOfContext = 3;

FragmentDirectiveGenerator::FragmentDirectiveGenerator(const SimpleRange& textFragmentRange)
{
    generateFragmentDirective(textFragmentRange);
}

static String previousWordsFromPosition(unsigned numberOfWords, VisiblePosition& startPosition)
{
    auto previousPosition = startPosition;
    while (numberOfWords--) {
        auto potentialPreviousPosition = previousWordPosition(previousPosition);
        if (potentialPreviousPosition.deepEquivalent().containerNode() != startPosition.deepEquivalent().containerNode())
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

static String nextWordsFromPosition(unsigned numberOfWords, VisiblePosition& startPosition)
{
    auto nextPosition = startPosition;
    while (numberOfWords--) {
        auto potentialNextPosition = nextWordPosition(nextPosition);
        if (potentialNextPosition.deepEquivalent().containerNode() != startPosition.deepEquivalent().containerNode())
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

    String textDirectivePrefix = ":~:text="_s;

    auto url = textFragmentRange.startContainer().document().url();
    auto textFromRange = createLiveRange(textFragmentRange)->toString();

    VisiblePosition visibleEndPosition = VisiblePosition(Position(textFragmentRange.protectedEndContainer(), textFragmentRange.endOffset(), Position::PositionIsOffsetInAnchor));
    VisiblePosition visibleStartPosition = VisiblePosition(Position(textFragmentRange.protectedStartContainer(), textFragmentRange.startOffset(), Position::PositionIsOffsetInAnchor));

    std::optional<String> prefix;
    std::optional<String> startText;
    std::optional<String> endText;
    std::optional<String> suffix;

    auto encodeComponent = [] (const String& component) -> std::optional<String> {
        auto encodedComponent = percentEncodeFragmentDirectiveSpecialCharacters(component);
        if (encodedComponent.isEmpty())
            return std::nullopt;
        return encodedComponent;
    };

    if (textFromRange.length() >= maximumInlineStringLength) {
        startText = encodeComponent(nextWordsFromPosition(numberOfWordsOfContext, visibleStartPosition));
        endText = encodeComponent(previousWordsFromPosition(numberOfWordsOfContext, visibleEndPosition));
    } else if (textFromRange.length() > minimumInlineContextlessStringLength)
        startText = encodeComponent(textFromRange);
    else {
        prefix = encodeComponent(previousWordsFromPosition(numberOfWordsOfContext, visibleStartPosition));
        startText = encodeComponent(textFromRange);
        suffix = encodeComponent(nextWordsFromPosition(numberOfWordsOfContext, visibleEndPosition));
    }

    Vector<String> components;
    if (prefix)
        components.append(makeString(*prefix, '-'));
    if (startText)
        components.append(*startText);
    if (endText)
        components.append(*endText);
    if (suffix)
        components.append(makeString('-', *suffix));

    url.setFragmentIdentifier(makeString(textDirectivePrefix, makeStringByJoining(components, ","_s)));

    m_urlWithFragment = url;

    LOG_WITH_STREAM(TextFragment, stream << m_urlWithFragment);
}

} // namespace WebCore
