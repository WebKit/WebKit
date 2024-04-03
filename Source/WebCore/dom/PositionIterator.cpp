/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PositionIterator.h"

#include "Editing.h"
#include "HTMLBodyElement.h"
#include "HTMLElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLNames.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderGrid.h"
#include "RenderText.h"

namespace WebCore {

using namespace HTMLNames;

PositionIterator::operator Position() const
{
    auto anchorNode = protectedNode();
    if (m_nodeAfterPositionInAnchor) {
        ASSERT(m_nodeAfterPositionInAnchor->parentNode() == anchorNode.get());
        // FIXME: This check is inadaquete because any ancestor could be ignored by editing
        if (positionBeforeOrAfterNodeIsCandidate(*anchorNode))
            return positionBeforeNode(anchorNode.get());
        return positionInParentBeforeNode(m_nodeAfterPositionInAnchor.get());
    }
    if (positionBeforeOrAfterNodeIsCandidate(*anchorNode))
        return atStartOfNode() ? positionBeforeNode(anchorNode.get()) : positionAfterNode(anchorNode.get());
    if (anchorNode->hasChildNodes())
        return lastPositionInOrAfterNode(anchorNode.get());
    return makeDeprecatedLegacyPosition(WTFMove(anchorNode), m_offsetInAnchor);
}

void PositionIterator::increment()
{
    if (!m_anchorNode)
        return;

    if (m_nodeAfterPositionInAnchor) {
        m_anchorNode = m_nodeAfterPositionInAnchor;
        m_nodeAfterPositionInAnchor = m_anchorNode->firstChild();
        m_offsetInAnchor = 0;
        return;
    }

    auto anchorNode = protectedNode();
    if (anchorNode->renderer() && !anchorNode->hasChildNodes() && m_offsetInAnchor < lastOffsetForEditing(*anchorNode))
        m_offsetInAnchor = Position::uncheckedNextOffset(anchorNode.get(), m_offsetInAnchor);
    else {
        m_nodeAfterPositionInAnchor = WTFMove(anchorNode);
        m_anchorNode = m_nodeAfterPositionInAnchor->parentNode();
        m_nodeAfterPositionInAnchor = m_nodeAfterPositionInAnchor->nextSibling();
        m_offsetInAnchor = 0;
    }
}

void PositionIterator::decrement()
{
    if (!m_anchorNode)
        return;

    if (m_nodeAfterPositionInAnchor) {
        m_anchorNode = m_nodeAfterPositionInAnchor->previousSibling();
        if (auto anchorNode = protectedNode()) {
            m_nodeAfterPositionInAnchor = nullptr;
            m_offsetInAnchor = anchorNode->hasChildNodes() ? 0 : lastOffsetForEditing(*anchorNode);
        } else {
            m_nodeAfterPositionInAnchor = m_nodeAfterPositionInAnchor->parentNode();
            m_anchorNode = m_nodeAfterPositionInAnchor->parentNode();
            m_offsetInAnchor = 0;
        }
        return;
    }
    
    if (m_anchorNode->hasChildNodes()) {
        m_anchorNode = m_anchorNode->lastChild();
        m_offsetInAnchor = m_anchorNode->hasChildNodes()? 0: lastOffsetForEditing(*m_anchorNode);
    } else {
        if (m_offsetInAnchor && m_anchorNode->renderer())
            m_offsetInAnchor = Position::uncheckedPreviousOffset(m_anchorNode.get(), m_offsetInAnchor);
        else {
            m_nodeAfterPositionInAnchor = m_anchorNode;
            m_anchorNode = m_anchorNode->parentNode();
        }
    }
}

bool PositionIterator::atStart() const
{
    if (!m_anchorNode)
        return true;
    if (m_anchorNode->parentNode())
        return false;
    return (!m_anchorNode->hasChildNodes() && !m_offsetInAnchor) || (m_nodeAfterPositionInAnchor && !m_nodeAfterPositionInAnchor->previousSibling());
}

bool PositionIterator::atEnd() const
{
    auto anchorNode = protectedNode();
    if (!anchorNode)
        return true;
    if (m_nodeAfterPositionInAnchor)
        return false;
    return !anchorNode->parentNode() && (anchorNode->hasChildNodes() || m_offsetInAnchor >= lastOffsetForEditing(*anchorNode));
}

bool PositionIterator::atStartOfNode() const
{
    if (!m_anchorNode)
        return true;
    if (!m_nodeAfterPositionInAnchor)
        return !m_anchorNode->hasChildNodes() && !m_offsetInAnchor;
    return !m_nodeAfterPositionInAnchor->previousSibling();
}

bool PositionIterator::atEndOfNode() const
{
    auto anchorNode = protectedNode();
    if (!anchorNode)
        return true;
    if (m_nodeAfterPositionInAnchor)
        return false;
    return anchorNode->hasChildNodes() || m_offsetInAnchor >= lastOffsetForEditing(*anchorNode);
}

// This function should be kept in sync with Position::isCandidate().
bool PositionIterator::isCandidate() const
{
    auto anchorNode = protectedNode();
    if (!anchorNode)
        return false;

    RenderObject* renderer = anchorNode->renderer();
    if (!renderer)
        return false;

    if (renderer->style().visibility() != Visibility::Visible)
        return false;

    if (renderer->isBR())
        return Position(*this).isCandidate();

    if (auto* renderText = dynamicDowncast<RenderText>(*renderer))
        return !Position::nodeIsUserSelectNone(anchorNode.get()) && renderText->containsCaretOffset(m_offsetInAnchor);

    if (positionBeforeOrAfterNodeIsCandidate(*anchorNode))
        return (atStartOfNode() || atEndOfNode()) && !Position::nodeIsUserSelectNone(anchorNode->parentNode());

    if (is<HTMLHtmlElement>(*anchorNode))
        return false;

    if (auto* block = dynamicDowncast<RenderBlock>(*renderer)) {
        if (is<RenderBlockFlow>(*block) || is<RenderGrid>(*block) || is<RenderFlexibleBox>(*block)) {
            if (block->logicalHeight() || is<HTMLBodyElement>(*anchorNode) || anchorNode->isRootEditableElement()) {
                if (!Position::hasRenderedNonAnonymousDescendantsWithHeight(*block))
                    return atStartOfNode() && !Position::nodeIsUserSelectNone(anchorNode.get());
                return anchorNode->hasEditableStyle() && !Position::nodeIsUserSelectNone(anchorNode.get()) && Position(*this).atEditingBoundary();
            }
            return false;
        }
    }

    return anchorNode->hasEditableStyle() && !Position::nodeIsUserSelectNone(anchorNode.get()) && Position(*this).atEditingBoundary();
}

} // namespace WebCore
