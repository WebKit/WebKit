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

#include "config.h"
#include "TextManipulationController.h"

#include "Editing.h"
#include "ScriptDisallowedScope.h"
#include "TextIterator.h"
#include "VisibleUnits.h"

namespace WebCore {

TextManipulationController::TextManipulationController(Document& document)
    : m_document(makeWeakPtr(document))
{
}

void TextManipulationController::startObservingParagraphs(ManipulationItemCallback&& callback)
{
    auto document = makeRefPtr(m_document.get());
    if (!document)
        return;

    m_callback = WTFMove(callback);

    VisiblePosition start = firstPositionInNode(m_document.get());
    VisiblePosition end = lastPositionInNode(m_document.get());
    TextIterator iterator { start.deepEquivalent(), end.deepEquivalent() };
    if (!document)
        return; // VisiblePosition or TextIterator's constructor may have updated the layout and executed arbitrary scripts.

    Vector<ManipulationToken> tokensInCurrentParagraph;
    Position startOfCurrentParagraph = start.deepEquivalent();
    while (!iterator.atEnd()) {
        StringView currentText = iterator.text();

        if (startOfCurrentParagraph.isNull())
            startOfCurrentParagraph = iterator.range()->startPosition();

        size_t endOfLastNewLine = 0;
        size_t offsetOfNextNewLine = 0;
        while ((offsetOfNextNewLine = currentText.find('\n', endOfLastNewLine)) != notFound) {
            if (endOfLastNewLine < offsetOfNextNewLine) {
                tokensInCurrentParagraph.append(ManipulationToken { m_tokenIdentifier.generate(),
                    currentText.substring(endOfLastNewLine, offsetOfNextNewLine - endOfLastNewLine).toString() });
            }

            auto lastRange = iterator.range();
            if (offsetOfNextNewLine < currentText.length()) {
                lastRange->setStart(firstPositionInOrBeforeNode(iterator.node())); // Move the start to the beginning of the current node.
                TextIterator::subrange(lastRange, 0, offsetOfNextNewLine);
            }
            Position endOfCurrentParagraph = lastRange->endPosition();

            if (!tokensInCurrentParagraph.isEmpty())
                addItem(startOfCurrentParagraph, endOfCurrentParagraph, WTFMove(tokensInCurrentParagraph));
            startOfCurrentParagraph.clear();
            endOfLastNewLine = offsetOfNextNewLine + 1;
        }

        auto remainingText = currentText.substring(endOfLastNewLine);
        if (remainingText.length())
            tokensInCurrentParagraph.append(ManipulationToken { m_tokenIdentifier.generate(), remainingText.toString() });

        iterator.advance();
    }

    if (!tokensInCurrentParagraph.isEmpty())
        addItem(startOfCurrentParagraph, end.deepEquivalent(), WTFMove(tokensInCurrentParagraph));
}

void TextManipulationController::addItem(const Position& startOfParagraph, const Position& endOfParagraph, Vector<ManipulationToken>&& tokens)
{
    ASSERT(m_document);
    auto result = m_items.add(m_itemIdentifier.generate(), ManipulationItem { startOfParagraph, endOfParagraph, WTFMove(tokens) });
    m_callback(*m_document, result.iterator->key, result.iterator->value.tokens);
}

bool TextManipulationController::completeManipulation(ItemIdentifier itemIdentifier, const Vector<ManipulationToken>& replacementTokens)
{
    if (!itemIdentifier)
        return false;

    auto itemIterator = m_items.find(itemIdentifier);
    if (itemIterator == m_items.end())
        return false;

    auto didReplace = replace(itemIterator->value, replacementTokens);

    m_items.remove(itemIterator);

    return didReplace;
}

bool TextManipulationController::replace(const ManipulationItem& item, const Vector<ManipulationToken>& replacementTokens)
{
    TextIterator iterator { item.start, item.end };
    size_t currentTokenIndex = 0;
    HashMap<TokenIdentifier, Ref<Node>> tokenToNode;
    while (!iterator.atEnd()) {
        auto string = iterator.text().toString();
        if (currentTokenIndex >= item.tokens.size())
            return false;
        auto& currentToken = item.tokens[currentTokenIndex];
        if (iterator.text() != currentToken.content)
            return false;
        tokenToNode.add(currentToken.identifier, *iterator.node());
        iterator.advance();
        ++currentTokenIndex;
    }

    // FIXME: This doesn't preseve the order of the replacement at all.
    for (auto& token : replacementTokens) {
        auto* node = tokenToNode.get(token.identifier);
        if (!node)
            return false;
        if (!is<CharacterData>(node))
            continue;
        // FIXME: It's not safe to update DOM while iterating over the tokens.
        downcast<CharacterData>(node)->setData(token.content);
    }

    return true;
}

} // namespace WebCore
