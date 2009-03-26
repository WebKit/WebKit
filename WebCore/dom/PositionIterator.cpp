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

#include "Node.h"
#include "RenderBlock.h"
#include "htmlediting.h"

namespace WebCore {

using namespace HTMLNames;

PositionIterator::operator Position() const
{
    if (m_child) {
        ASSERT(m_child->parentNode() == m_parent);
        return positionBeforeNode(m_child);
    }
    if (m_parent->hasChildNodes())
        return lastDeepEditingPositionForNode(m_parent);
    return Position(m_parent, m_offset);
}

void PositionIterator::increment()
{
    if (!m_parent)
        return;

    if (m_child) {
        m_parent = m_child;
        m_child = m_parent->firstChild();
        m_offset = 0;
        return;
    }

    if (!m_parent->hasChildNodes() && m_offset < lastOffsetForEditing(m_parent))
        m_offset = Position::uncheckedNextOffset(m_parent, m_offset);
    else {
        m_child = m_parent;
        m_parent = m_child->parentNode();
        m_child = m_child->nextSibling();
        m_offset = 0;
    }
}

void PositionIterator::decrement()
{
    if (!m_parent)
        return;

    if (m_child) {
        m_parent = m_child->previousSibling();
        if (m_parent) {
            m_child = 0;
            m_offset = m_parent->hasChildNodes() ? 0 : lastOffsetForEditing(m_parent);
        } else {
            m_child = m_child->parentNode();
            m_parent = m_child->parentNode();
            m_offset = 0;
        }
        return;
    }

    if (m_offset) {
        m_offset = Position::uncheckedPreviousOffset(m_parent, m_offset);
    } else {
        if (m_parent->hasChildNodes()) {
            m_parent = m_parent->lastChild();
            if (!m_parent->hasChildNodes())
                m_offset = lastOffsetForEditing(m_parent);
        } else {
            m_child = m_parent;
            m_parent = m_parent->parentNode();
        }
    }
}

bool PositionIterator::atStart() const
{
    if (!m_parent)
        return true;
    if (m_parent->parentNode())
        return false;
    return (!m_parent->hasChildNodes() && !m_offset) || (m_child && !m_child->previousSibling());
}

bool PositionIterator::atEnd() const
{
    if (!m_parent)
        return true;
    if (m_child)
        return false;
    return !m_parent->parentNode() && (m_parent->hasChildNodes() || m_offset >= lastOffsetForEditing(m_parent));
}

bool PositionIterator::atStartOfNode() const
{
    if (!m_parent)
        return true;
    if (!m_child)
        return !m_parent->hasChildNodes() && !m_offset;
    return !m_child->previousSibling();
}

bool PositionIterator::atEndOfNode() const
{
    if (!m_parent)
        return true;
    if (m_child)
        return false;
    return m_parent->hasChildNodes() || m_offset >= lastOffsetForEditing(m_parent);
}

bool PositionIterator::isCandidate() const
{
    if (!m_parent)
        return false;

    RenderObject* renderer = m_parent->renderer();
    if (!renderer)
        return false;
    
    if (renderer->style()->visibility() != VISIBLE)
        return false;

    if (renderer->isBR())
        return !m_offset && !Position::nodeIsUserSelectNone(m_parent->parent());

    if (renderer->isText())
        return Position(*this).inRenderedText() && !Position::nodeIsUserSelectNone(m_parent);

    if (isTableElement(m_parent) || editingIgnoresContent(m_parent))
        return (atStartOfNode() || atEndOfNode()) && !Position::nodeIsUserSelectNone(m_parent->parent());

    if (!m_parent->hasTagName(htmlTag) && renderer->isBlockFlow() && !Position::hasRenderedNonAnonymousDescendantsWithHeight(renderer) &&
       (toRenderBlock(renderer)->height() || m_parent->hasTagName(bodyTag)))
        return atStartOfNode() && !Position::nodeIsUserSelectNone(m_parent);
    
    return false;
}

} // namespace WebCore
