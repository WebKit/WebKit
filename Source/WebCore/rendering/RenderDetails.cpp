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
#include "LocalizedStrings.h"
#include "RenderDetailsMarker.h"
#include "RenderTextFragment.h"
#include "RenderView.h"

namespace WebCore {

using namespace HTMLNames;

RenderDetails::RenderDetails(Node* node)
    : RenderBlock(node)
    , m_summaryBlock(0)
    , m_contentBlock(0)
    , m_defaultSummaryBlock(0)
    , m_defaultSummaryText(0)
    , m_marker(0)
    , m_mainSummary(0)
{
}

void RenderDetails::destroy()
{
    if (m_marker)
        m_marker->destroy();

    RenderBlock::destroy();
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
    if (oldChild == m_summaryBlock || oldChild == m_contentBlock) {
        RenderBlock::removeChild(oldChild);
        m_summaryBlock = 0;
        return;
    }

    if (oldChild == m_mainSummary) {
        m_summaryBlock->removeChild(m_mainSummary);
        return;
    }

    if (m_contentBlock) {
        m_contentBlock->removeChild(oldChild);
        return;
    }

    ASSERT_NOT_REACHED();
}

void RenderDetails::setMarkerStyle()
{
    if (m_marker) {
        RefPtr<RenderStyle> markerStyle = RenderStyle::create();
        markerStyle->inheritFrom(style());
        m_marker->setStyle(markerStyle.release());
    }
}

void RenderDetails::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);

    if (m_defaultSummaryBlock) {
        m_defaultSummaryBlock->setStyle(createSummaryStyle());
        m_defaultSummaryText->setStyle(m_defaultSummaryBlock->style());
    }

    setMarkerStyle();

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

void RenderDetails::markerDestroyed()
{
    m_marker = 0;
}

void RenderDetails::summaryDestroyed(RenderObject* summary)
{
    if (summary == m_mainSummary)
        m_mainSummary = 0;
}

void RenderDetails::moveSummaryToContents()
{
    if (m_defaultSummaryBlock) {
        ASSERT(!m_mainSummary);
        m_defaultSummaryBlock->destroy();
        m_defaultSummaryBlock = 0;
        m_defaultSummaryText = 0;
        return;
    }

    if (!m_mainSummary)
        return;

    m_mainSummary->remove();
    contentBlock()->addChild(m_mainSummary, getRenderPosition(m_mainSummary));
    m_mainSummary = 0;
}

PassRefPtr<RenderStyle> RenderDetails::createSummaryStyle()
{
    RefPtr<HTMLElement> summary(HTMLElement::create(summaryTag, document()));
    return document()->styleSelector()->styleForElement(summary.get(), style(), true);
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

void RenderDetails::createDefaultSummary()
{
    if (m_defaultSummaryBlock)
        return;

    moveSummaryToContents();

    m_defaultSummaryBlock = summaryBlock()->createAnonymousBlock();
    m_defaultSummaryBlock->setStyle(createSummaryStyle());

    m_defaultSummaryText = new (renderArena()) RenderTextFragment(document(), defaultDetailsSummaryText().impl());
    m_defaultSummaryText->setStyle(m_defaultSummaryBlock->style());
    m_defaultSummaryBlock->addChild(m_defaultSummaryText);

    summaryBlock()->addChild(m_defaultSummaryBlock);
}

void RenderDetails::checkMainSummary()
{
    if (!node() || !node()->hasTagName(detailsTag))
        return;

    Node* mainSummaryNode = static_cast<HTMLDetailsElement*>(node())->mainSummary();

    if (!mainSummaryNode || !mainSummaryNode->renderer() || mainSummaryNode->renderer()->isFloatingOrPositioned())
        createDefaultSummary();
    else
        replaceMainSummary(mainSummaryNode->renderer());

}

void RenderDetails::layout()
{
    ASSERT(needsLayout());

    checkMainSummary();
    ASSERT(m_summaryBlock);

    if (!m_marker) {
        m_marker = new (renderArena()) RenderDetailsMarker(this);
        setMarkerStyle();
    }
    updateMarkerLocation();

    RenderBlock::layout();

    m_interactiveArea = m_summaryBlock->frameRect();

    // FIXME: the following code will not be needed once absoluteToLocal get patched to handle flipped blocks writing modes.
    switch (style()->writingMode()) {
    case TopToBottomWritingMode:
    case LeftToRightWritingMode:
        break;
    case RightToLeftWritingMode: {
        m_interactiveArea.setX(width() - m_interactiveArea.x() - m_interactiveArea.width());
        break;
    }
    case BottomToTopWritingMode: {
        m_interactiveArea.setY(height() - m_interactiveArea.y() - m_interactiveArea.height());
        break;
    }
    }
}

bool RenderDetails::isOpen() const
{
    return node() && node()->isElementNode() ? !static_cast<Element*>(node())->getAttribute(openAttr).isNull() : false;
}

RenderObject* RenderDetails::getParentOfFirstLineBox(RenderBlock* curr)
{
    RenderObject* firstChild = curr->firstChild();
    if (!firstChild)
        return 0;

    for (RenderObject* currChild = firstChild; currChild; currChild = currChild->nextSibling()) {
        if (currChild == m_marker)
            continue;

        if (currChild->isInline() && (!currChild->isRenderInline() || curr->generatesLineBoxesForInlineChild(currChild)))
            return curr;

        if (currChild->isFloating() || currChild->isPositioned())
            continue;

        if (currChild->isTable() || !currChild->isRenderBlock() || (currChild->isBox() && toRenderBox(currChild)->isWritingModeRoot()))
            break;

        if (currChild->isDetails())
            break;

        RenderObject* lineBox = getParentOfFirstLineBox(toRenderBlock(currChild));
        if (lineBox)
            return lineBox;
    }

    return 0;
}

RenderObject* RenderDetails::firstNonMarkerChild(RenderObject* parent)
{
    RenderObject* result = parent->firstChild();
    while (result && result->isDetailsMarker())
        result = result->nextSibling();
    return result;
}

void RenderDetails::updateMarkerLocation()
{
    // Sanity check the location of our marker.
    if (m_marker) {
        RenderObject* markerPar = m_marker->parent();
        RenderObject* lineBoxParent = getParentOfFirstLineBox(m_summaryBlock);
        if (!lineBoxParent) {
            // If the marker is currently contained inside an anonymous box,
            // then we are the only item in that anonymous box (since no line box
            // parent was found). It's ok to just leave the marker where it is
            // in this case.
            if (markerPar && markerPar->isAnonymousBlock())
                lineBoxParent = markerPar;
            else
                lineBoxParent = m_summaryBlock;
        }

        if (markerPar != lineBoxParent || m_marker->preferredLogicalWidthsDirty()) {
            // Removing and adding the marker can trigger repainting in
            // containers other than ourselves, so we need to disable LayoutState.
            view()->disableLayoutState();
            m_marker->remove();
            if (!lineBoxParent)
                lineBoxParent = m_summaryBlock;
            lineBoxParent->addChild(m_marker, firstNonMarkerChild(lineBoxParent));

            if (m_marker->preferredLogicalWidthsDirty())
                m_marker->computePreferredLogicalWidths();

            view()->enableLayoutState();
        }
    }
}

} // namespace WebCore

