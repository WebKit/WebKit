/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "FragmentDirectiveRangeFinder.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "ComposedTreeIterator.h"
#include "Document.h"
#include "DocumentType.h"
#include "Editor.h"
#include "HTMLAudioElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLImageElement.h"
#include "HTMLMeterElement.h"
#include "HTMLObjectElement.h"
#include "HTMLProgressElement.h"
#include "HTMLScriptElement.h"
#include "HTMLStyleElement.h"
#include "HTMLVideoElement.h"
#include "NodeRenderStyle.h"
#include "NodeTraversal.h"
#include "Position.h"
#include "RenderStyleInlines.h"
#include "SimpleRange.h"
#include "TextBoundaries.h"
#include "TextIterator.h"

namespace WebCore {
namespace FragmentDirectiveRangeFinder {

enum class BoundaryPointIsAtEnd : bool { No, Yes };
enum class WordBounded : bool { No, Yes };

// https://wicg.github.io/scroll-to-text-fragment/#search-invisible
static bool isSearchInvisible(const Node& node)
{
    if (!node.renderStyle() || node.renderStyle()->display() == DisplayType::None)
        return true;
    
    // FIXME: If the node serializes as void.
    
    if (is<HTMLIFrameElement>(node)
        || is<HTMLImageElement>(node)
        || is<HTMLMeterElement>(node)
        || is<HTMLObjectElement>(node)
        || is<HTMLProgressElement>(node)
        || is<HTMLStyleElement>(node)
        || is<HTMLScriptElement>(node)
#if ENABLE(VIDEO)
        || is<HTMLVideoElement>(node)
        || is<HTMLAudioElement>(node)
#endif // ENABLE(VIDEO)
        )
        return true;
    
    // FIXME: Is a select element whose multiple content attribute is absent.
    
    return false;
}

// https://wicg.github.io/scroll-to-text-fragment/#non-searchable-subtree
static bool isNonSearchableSubtree(const Node& node)
{
    const Node* traversingNode = &node;
    while (traversingNode) {
        if (isSearchInvisible(*traversingNode))
            return true;
        traversingNode = traversingNode->parentOrShadowHostNode();
    }
    return false;
}

// https://wicg.github.io/scroll-to-text-fragment/#nearest-block-ancestor
static const Node& nearestBlockAncestor(const Node& node)
{
    const Node* currentNode = &node;
    while (currentNode) {
        if (!currentNode->isTextNode() && currentNode->renderer() && currentNode->renderer()->style().isDisplayBlockLevel())
            return *currentNode;
        currentNode = currentNode->parentNode();
    }
    return node.document();
}

// https://wicg.github.io/scroll-to-text-fragment/#get-boundary-point-at-index
static std::optional<BoundaryPoint> boundaryPointAtIndexInNodes(unsigned index, const Vector<Ref<Text>>& textNodeList, BoundaryPointIsAtEnd isEnd)
{
    unsigned currentLength = 0;
    
    for (auto& currentNode : textNodeList) {
        unsigned nodeEnd = currentLength + currentNode->length();
        if (isEnd == BoundaryPointIsAtEnd::Yes)
            nodeEnd++;
        if (nodeEnd > index)
            return BoundaryPoint(currentNode.get(), index - currentLength);
        currentLength += currentNode->length();
    }
    return std::nullopt;
}

// https://wicg.github.io/scroll-to-text-fragment/#is-at-a-word-boundary
static bool indexIsAtWordBoundary(const String& string, unsigned index)
{
    UBreakIterator* it = wordBreakIterator(string);
    return ubrk_isBoundary(it, index);
}

// https://wicg.github.io/scroll-to-text-fragment/#visible-text-node
static bool isVisibleTextNode(const Node& node)
{
    return node.isTextNode() && node.renderer() && node.renderer()->style().visibility() == Visibility::Visible;
}

static bool isVisibleTextNode(const Text& node)
{
    return node.renderer() && node.renderer()->style().visibility() == Visibility::Visible;
}

// https://wicg.github.io/scroll-to-text-fragment/#find-a-range-from-a-node-list
static std::optional<SimpleRange> findRangeFromNodeList(const String& query, const SimpleRange& searchRange, Vector<Ref<Text>>& nodes, WordBounded wordStartBounded, WordBounded wordEndBounded)
{
    String searchBuffer;
    
    if (nodes.size() < 1)
        return std::nullopt;
    
    StringBuilder searchBufferBuilder;
    for (auto& node : nodes)
        searchBufferBuilder.append(node->data());
    searchBuffer = searchBufferBuilder.toString();
    
    searchBuffer = foldQuoteMarks(searchBuffer);
    auto foldedQuery = foldQuoteMarks(query);
    
    // FIXME: add quote folding to TextIterator instead of leaving it here?
    FindOptions options = { FindOption::CaseInsensitive, FindOption::DoNotRevealSelection };

    if (wordStartBounded == WordBounded::Yes)
        options.add(FindOption::AtWordStarts);
    if (wordEndBounded == WordBounded::Yes)
        options.add(FindOption::AtWordEnds);

    auto foundText = findPlainText(searchRange, foldedQuery, options);

    if (!foundText.collapsed())
        return foundText;

    unsigned searchStart = 0;
    
    if (nodes[0].ptr() == &searchRange.startContainer())
        searchStart = searchRange.startOffset();
    
    std::optional<BoundaryPoint> start;
    std::optional<BoundaryPoint> end;
    
    std::optional<unsigned> matchIndex;
    
    while (!matchIndex) {
        // FIXME: find only using the base characters i.e. also fold accents and others.
        auto potentialIndex = searchBuffer.findIgnoringASCIICase(foldedQuery, searchStart);

        if (potentialIndex == notFound)
            return std::nullopt;
        matchIndex = potentialIndex;
        
        start = boundaryPointAtIndexInNodes(*matchIndex, nodes, BoundaryPointIsAtEnd::No);
        end = boundaryPointAtIndexInNodes(*matchIndex + foldedQuery.length(), nodes, BoundaryPointIsAtEnd::Yes);
        
        if (((wordStartBounded == WordBounded::Yes) && !indexIsAtWordBoundary(searchBuffer, *matchIndex))
            || ((wordEndBounded == WordBounded::Yes) && !indexIsAtWordBoundary(searchBuffer, *matchIndex + foldedQuery.length()))) {
            searchStart = *matchIndex + 1;
            matchIndex = std::nullopt;
        }
    }
        
    unsigned endInset = 0;
    if (nodes.last().ptr() == &searchRange.endContainer())
        endInset = searchRange.endContainer().length() - searchRange.endOffset();
    
    if (*matchIndex + foldedQuery.length() > searchBuffer.length() - endInset)
        return std::nullopt;
    
    ASSERT_WITH_MESSAGE(start, "Scroll To Text Fragment: Start cannot be null");
    ASSERT_WITH_MESSAGE(end, "Scroll To Text Fragment: End cannot be null");
    
    ASSERT_WITH_MESSAGE(contains(searchRange, start.value()), "Scroll To Text Fragment: Start is within SearchRange.");
    ASSERT_WITH_MESSAGE(contains(searchRange, end.value()), "Scroll To Text Fragment: End is within SearchRange.");
    
    if (!start || !end || !contains(searchRange, start.value()) || !contains(searchRange, end.value()))
        return std::nullopt;
    
    return SimpleRange(*start, *end);
    
}
// https://wicg.github.io/scroll-to-text-fragment/#find-a-string-in-range
static std::optional<SimpleRange> rangeOfStringInRange(const String& query, SimpleRange& searchRange, WordBounded wordStartBounded, WordBounded wordEndBounded)
{
    while (!searchRange.collapsed()) {
        RefPtr<Node> currentNode = &searchRange.startContainer();
        
        if (isNonSearchableSubtree(*currentNode)) {
            if (auto newStart = NodeTraversal::nextSkippingChildren(*currentNode))
                searchRange.start = { *newStart, 0 };
            else
                return std::nullopt;
            continue;
        }
        
        if (!isVisibleTextNode(*currentNode)) {
            do {
                if (auto newStart = NodeTraversal::next(*currentNode)) {
                    searchRange.start = { *newStart, 0 };
                    currentNode = newStart;
                } else
                    return std::nullopt;
            } while (is<DocumentType>(currentNode));
            continue;
        }

        Ref blockAncestor = nearestBlockAncestor(*currentNode);
        Vector<Ref<Text>> textNodeList;
        // FIXME: this is O^2 since treeOrder will also do traversal, optimize.
        while (currentNode && currentNode->isDescendantOf(blockAncestor) && is_lteq(treeOrder(BoundaryPoint(*currentNode, 0), searchRange.end))) {
            if (currentNode->renderer() && is<Element>(currentNode) && currentNode->renderer()->style().isDisplayBlockLevel())
                break;

            if (isSearchInvisible(*currentNode)) {
                currentNode = NodeTraversal::nextSkippingChildren(*currentNode);
                continue;
            }
            auto* textNode = dynamicDowncast<Text>(*currentNode);
            if (textNode && isVisibleTextNode(*textNode))
                textNodeList.append(*textNode);
            currentNode = NodeTraversal::next(*currentNode);
        }

        if (auto range = findRangeFromNodeList(query, searchRange, textNodeList, wordStartBounded, wordEndBounded))
            return range;

        if (!currentNode)
            break;
        
        ASSERT_WITH_MESSAGE(is_gt(treeOrder(*currentNode, searchRange.start.container)), "Scroll To Text Fragment: Current node must follow start.");
        
        if (!is_gt(treeOrder(*currentNode, searchRange.start.container)))
            return std::nullopt;
        
        searchRange.start = { *currentNode, 0 };
    }
    return std::nullopt;
}

// https://wicg.github.io/scroll-to-text-fragment/#next-non-whitespace-position
static std::optional<SimpleRange> advanceRangeStartToNextNonWhitespace(SimpleRange& range)
{
    auto newRange = range;
    while (!newRange.collapsed()) {
        Ref node = newRange.startContainer();
        auto offset = newRange.startOffset();
        
        // This check is not in the spec.
        // I believe there is an error in the spec which I have filed an issue for
        // https://github.com/WICG/scroll-to-text-fragment/issues/189
        if (offset == node->length()) {
            if (auto newStart = NodeTraversal::next(node)) {
                newRange.start = { *newStart, 0 };
                continue;
            }
            return newRange;
        }
        
        if (isNonSearchableSubtree(node)) {
            if (auto newStart = NodeTraversal::next(node))
                newRange.start = { *newStart, 0 };
            else
                return newRange;
            continue;
        }

        if (!isVisibleTextNode(node)) {
            if (auto newStart = NodeTraversal::next(node))
                newRange.start = { *newStart, 0 };
            else
                return newRange;
            continue;
        }

        auto string = node->textContent();

        if (string.substringSharingImpl(offset, 6) == "&nbsp;"_s)
            offset += 6;

        if (string.substringSharingImpl(offset, 5) == "&nbsp"_s)
            offset += 5;
        
        if (!isUnicodeWhitespace(string[offset]))
            return newRange;
        offset++;

        if (offset >= node->length()) {
            if (auto newStart = NodeTraversal::next(node))
                newRange.start = { *newStart, 0 };
            else
                return newRange;
        } else
            newRange.start = { node.get(), offset };
    }
    return newRange;
}

// https://wicg.github.io/scroll-to-text-fragment/#find-a-range-from-a-text-directive
std::optional<SimpleRange> findRangeFromTextDirective(const ParsedTextDirective parsedTextDirective, Document& document)
{
    auto searchRange = makeRangeSelectingNodeContents(document);
    std::optional<SimpleRange> matchRange;
    WordBounded mustEndAtWordBoundary = (!parsedTextDirective.textEnd.isNull() || parsedTextDirective.suffix.isNull()) ? WordBounded::Yes : WordBounded::No;
    std::optional<SimpleRange> potentialMatch;
    
    while (!searchRange.collapsed()) {
        if (!parsedTextDirective.prefix.isNull()) {
            auto prefixMatch = rangeOfStringInRange(parsedTextDirective.prefix, searchRange, WordBounded::Yes, WordBounded::No);
            if (!prefixMatch)
                return std::nullopt;
            
            // .next() does a bit  more than just find the next boundary point, so we differ from the spec here slightly.
            auto newStart = makeBoundaryPoint(makeContainerOffsetPosition(prefixMatch->start).next());
            if (!newStart)
                return std::nullopt;
            
            searchRange.start = newStart.value();
            matchRange = makeSimpleRange(prefixMatch->end, searchRange.end);
            ASSERT(matchRange);
            matchRange = advanceRangeStartToNextNonWhitespace(matchRange.value());
            if (!matchRange)
                return std::nullopt;
            
            if (matchRange->collapsed())
                return std::nullopt;
            
            ASSERT(matchRange->start.container->isTextNode(), "MatchRange start is not a Text Node");
            
            potentialMatch = rangeOfStringInRange(parsedTextDirective.textStart, matchRange.value(), WordBounded::No, mustEndAtWordBoundary);

            if (!potentialMatch)
                return std::nullopt;
            if (potentialMatch->start != matchRange->start)
                continue;
        } else {
            potentialMatch = rangeOfStringInRange(parsedTextDirective.textStart, searchRange, WordBounded::Yes, mustEndAtWordBoundary);
            if (!potentialMatch)
                return std::nullopt;
            
            // .next() does a bit  more than just find the next boundary point, so we differ from the spec here slightly.
            auto newStart = makeBoundaryPoint(makeContainerOffsetPosition(potentialMatch->start).next());
            if (!newStart)
                return std::nullopt;
            searchRange.start = newStart.value();
        }
        
        auto rangeEndSearchRange = makeSimpleRange(potentialMatch->end, searchRange.end);
        while (!rangeEndSearchRange.collapsed()) {
            if (!parsedTextDirective.textEnd.isNull()) {
                mustEndAtWordBoundary = !parsedTextDirective.suffix ? WordBounded::Yes : WordBounded::No;
                auto textEndMatch = rangeOfStringInRange(parsedTextDirective.textEnd, rangeEndSearchRange, WordBounded::Yes, mustEndAtWordBoundary);
                if (!textEndMatch)
                    return std::nullopt;
                
                potentialMatch->end = textEndMatch->end;
            }
            // FIXME: Assert: potentialMatch represents a range exactly containing an instance of matching text.
            ASSERT_WITH_MESSAGE(potentialMatch && !potentialMatch->collapsed(), "Scroll To Text Fragment: potentialMatch cannot be null or collapsed");
            if (!potentialMatch || potentialMatch->collapsed())
                return std::nullopt;
            
            if (!parsedTextDirective.suffix)
                return potentialMatch;
            
            std::optional<SimpleRange> suffixRange = makeSimpleRange(potentialMatch->end, searchRange.end);
            suffixRange = advanceRangeStartToNextNonWhitespace(suffixRange.value());
            if (!suffixRange)
                return std::nullopt;
            auto suffixMatch = rangeOfStringInRange(parsedTextDirective.suffix, suffixRange.value(), WordBounded::No, WordBounded::Yes);
            
            if (!suffixMatch)
                return std::nullopt;
            
            if (suffixMatch->start == suffixRange->start)
                return potentialMatch;
            
            if (parsedTextDirective.textEnd.isNull())
                break;
            
            rangeEndSearchRange.start = potentialMatch->end;
        }
        
        if (rangeEndSearchRange.collapsed()) {
            ASSERT(!parsedTextDirective.textEnd.isNull());
            return std::nullopt;
        }
    }
    return std::nullopt;
}

const Vector<SimpleRange> findRangesFromTextDirectives(Vector<ParsedTextDirective>& parsedTextDirectives, Document& document)
{
    Vector<SimpleRange> ranges;
    
    for (auto directive : parsedTextDirectives) {
        auto range = findRangeFromTextDirective(directive, document);
        if (range && !range->collapsed())
            ranges.append(range.value());
    }
    
    return ranges;
}

} // FragmentDirectiveRangeFinder

} // WebCore
