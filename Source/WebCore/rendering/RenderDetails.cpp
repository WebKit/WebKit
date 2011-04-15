/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderDetails.h"

#include "CSSStyleSelector.h"
#include "HTMLDetailsElement.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

RenderDetails::RenderDetails(Node* node)
    : RenderBlock(node)
    , m_summaryBlock(0)
    , m_contentBlock(0)
    , m_mainSummary(0)
{
}

RenderBlock* RenderDetails::summaryBlock()
{
    if (!m_summaryBlock) {
        m_summaryBlock = createAnonymousBlock();
        RenderBlock::addChild(m_summaryBlock, m_contentBlock);
    }
    return m_summaryBlock;
}

RenderBlock* RenderDetails::contentBlock()
{
    if (!m_contentBlock) {
        m_contentBlock = createAnonymousBlock();
        RenderBlock::addChild(m_contentBlock);
    }
    return m_contentBlock;
}

void RenderDetails::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    if (beforeChild && beforeChild == m_mainSummary)
        beforeChild = getRenderPosition(m_mainSummary);
    contentBlock()->addChild(newChild, beforeChild);
}

void RenderDetails::removeChild(RenderObject* oldChild)
{
    if (oldChild == m_summaryBlock) {
        RenderBlock::removeChild(oldChild);
        m_summaryBlock = 0;
        return;
    }

    if (oldChild == m_contentBlock) {
        RenderBlock::removeChild(oldChild);
        m_contentBlock = 0;
        return;
    }

    if (oldChild == m_mainSummary && m_summaryBlock) {
        m_summaryBlock->removeChild(m_mainSummary);
        return;
    }

    if (m_contentBlock) {
        m_contentBlock->removeChild(oldChild);
        return;
    }

    ASSERT_NOT_REACHED();
}

void RenderDetails::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    // Ensure that if we ended up being inline that we set our replaced flag
    // so that we're treated like an inline-block.
    setReplaced(isInline());
}

RenderObject* RenderDetails::getRenderPosition(RenderObject* object)
{
    if (!object || !object->node())
        return 0;

    Node* element = object->node()->nextSibling();

    while (element && !element->renderer())
        element = element->nextSibling();

    return element ? element->renderer() : 0;
}

void RenderDetails::summaryDestroyed(RenderObject* summary)
{
    if (summary == m_mainSummary)
        m_mainSummary = 0;
}

void RenderDetails::moveSummaryToContents()
{
    if (!m_mainSummary)
        return;

    m_mainSummary->remove();
    contentBlock()->addChild(m_mainSummary, getRenderPosition(m_mainSummary));
    m_mainSummary = 0;
}

void RenderDetails::replaceMainSummary(RenderObject* newSummary)
{
    ASSERT(newSummary);
    if (m_mainSummary == newSummary)
        return;

    moveSummaryToContents();
    newSummary->remove();
    summaryBlock()->addChild(newSummary);
    m_mainSummary = newSummary;
}

void RenderDetails::checkMainSummary()
{
    if (!node() || !node()->hasTagName(detailsTag))
        return;

    Node* mainSummaryNode = static_cast<HTMLDetailsElement*>(node())->mainSummary();
    if (mainSummaryNode && mainSummaryNode->renderer())
        replaceMainSummary(mainSummaryNode->renderer());
}

void RenderDetails::layout()
{
    checkMainSummary();
    RenderBlock::layout();
}

bool RenderDetails::isOpen() const
{
    return node() && node()->isElementNode() ? !static_cast<Element*>(node())->getAttribute(openAttr).isNull() : false;
}

} // namespace WebCore
