/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 */

#include "config.h"

#if ENABLE(TEXT_AUTOSIZING)

#include "TextAutosizer.h"

#include "Document.h"
#include "InspectorInstrumentation.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include "RenderText.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleInheritedData.h"

namespace WebCore {

TextAutosizer::TextAutosizer(Document* document)
    : m_document(document)
{
}

TextAutosizer::~TextAutosizer()
{
}

bool TextAutosizer::processSubtree(RenderObject* layoutRoot)
{
    // FIXME: Text Autosizing should only be enabled when m_document->page()->mainFrame()->view()->useFixedLayout()
    // is true, but for now it's useful to ignore this so that it can be tested on desktop.
    if (!m_document->settings() || !m_document->settings()->textAutosizingEnabled() || layoutRoot->view()->printing() || !m_document->page())
        return false;

    Frame* mainFrame = m_document->page()->mainFrame();

    // Window area, in logical (density-independent) pixels.
    IntSize windowSize = m_document->settings()->textAutosizingWindowSizeOverride();
    if (windowSize.isEmpty()) {
        bool includeScrollbars = !InspectorInstrumentation::shouldApplyScreenWidthOverride(mainFrame);
        windowSize = mainFrame->view()->visibleContentRect(includeScrollbars).size(); // FIXME: Check that this is always in logical (density-independent) pixels (see wkbug.com/87440).
    }

    // Largest area of block that can be visible at once (assuming the main
    // frame doesn't get scaled to less than overview scale), in CSS pixels.
    IntSize minLayoutSize = mainFrame->view()->layoutSize();
    for (Frame* frame = m_document->frame(); frame; frame = frame->tree()->parent()) {
        if (!frame->view()->isInChildFrameWithFrameFlattening())
            minLayoutSize = minLayoutSize.shrunkTo(frame->view()->layoutSize());
    }

    for (RenderObject* descendant = layoutRoot->nextInPreOrder(layoutRoot); descendant; descendant = descendant->nextInPreOrder(layoutRoot)) {
        if (isNotAnAutosizingContainer(descendant))
            continue;
        processBox(toRenderBox(descendant), windowSize, minLayoutSize);
    }

    return true;
}

static bool contentHeightIsConstrained(const RenderBox* box)
{
    // FIXME: Propagate constrainedness down the tree, to avoid inefficiently walking back up from each box.
    // FIXME: This code needs to take into account vertical writing modes.
    // FIXME: Consider additional heuristics, such as ignoring fixed heights if the content is already overflowing before autosizing kicks in.
    for (const RenderBox* container = box; container; container = container->containingBlock()) {
        RenderStyle* style = container->style();
        if (style->overflowY() >= OSCROLL)
            return false;
        if (style->height().isSpecified() || style->maxHeight().isSpecified())
            return true;
    }
    return false;
}

void TextAutosizer::processBox(RenderBox* box, const IntSize& windowSize, const IntSize& layoutSize)
{
    if (contentHeightIsConstrained(box))
        return;

    int logicalWindowWidth = box->isHorizontalWritingMode() ? windowSize.width() : windowSize.height();
    int logicalLayoutWidth = box->isHorizontalWritingMode() ? layoutSize.width() : layoutSize.height();
    // Ignore box width in excess of the layout width, to avoid extreme multipliers.
    float logicalBoxWidth = std::min<float>(box->logicalWidth(), logicalLayoutWidth);

    float multiplier = logicalBoxWidth / logicalWindowWidth;
    multiplier *= m_document->settings()->textAutosizingFontScaleFactor();

    if (multiplier < 1)
        return;
    RenderObject* descendant = nextInPreOrderMatchingFilter(box, box, isNotAnAutosizingContainer);
    while (descendant) {
        if (descendant->isText()) {
            setMultiplier(descendant, multiplier);
            setMultiplier(descendant->parent(), multiplier); // Parent does line spacing.
            // FIXME: Increase list marker size proportionately.
        }
        descendant = nextInPreOrderMatchingFilter(descendant, box, isNotAnAutosizingContainer);
    }
}

void TextAutosizer::setMultiplier(RenderObject* renderer, float multiplier)
{
    RefPtr<RenderStyle> newStyle = RenderStyle::clone(renderer->style());
    newStyle->setTextAutosizingMultiplier(multiplier);
    renderer->setStyle(newStyle.release());
}

float TextAutosizer::computeAutosizedFontSize(float specifiedSize, float multiplier)
{
    // Somewhat arbitrary "pleasant" font size.
    const float pleasantSize = 16;

    // Multiply fonts that the page author has specified to be larger than
    // pleasantSize by less and less, until huge fonts are not increased at all.
    // For specifiedSize between 0 and pleasantSize we directly apply the
    // multiplier; hence for specifiedSize == pleasantSize, computedSize will be
    // multiplier * pleasantSize. For greater specifiedSizes we want to
    // gradually fade out the multiplier, so for every 1px increase in
    // specifiedSize beyond pleasantSize we will only increase computedSize
    // by gradientAfterPleasantSize px until we meet the
    // computedSize = specifiedSize line, after which we stay on that line (so
    // then every 1px increase in specifiedSize increases computedSize by 1px).
    const float gradientAfterPleasantSize = 0.5;

    float computedSize;
    if (specifiedSize <= pleasantSize)
        computedSize = multiplier * specifiedSize;
    else {
        computedSize = multiplier * pleasantSize + gradientAfterPleasantSize * (specifiedSize - pleasantSize);
        if (computedSize < specifiedSize)
            computedSize = specifiedSize;
    }
    return computedSize;
}

bool TextAutosizer::isNotAnAutosizingContainer(const RenderObject* renderer)
{
    // "Autosizing containers" are the smallest unit for which we can enable/disable
    // Text Autosizing. A uniform text size multiplier is enforced within them.
    // - Must be RenderBoxes since they need a well-defined logicalWidth().
    // - Must not be inline, as different multipliers on one line looks terrible.
    // - Must not be list items, as items in the same list should look consistent.
    return !renderer->isBox() || renderer->isInline() || renderer->isListItem();
}

RenderObject* TextAutosizer::nextInPreOrderMatchingFilter(RenderObject* current, const RenderObject* stayWithin, RenderObjectFilterFunctor filter)
{
    for (RenderObject* child = current->firstChild(); child; child = child->nextSibling())
        if (filter(child))
            return child;

    for (const RenderObject* ancestor = current; ancestor; ancestor = ancestor->parent()) {
        if (ancestor == stayWithin)
            return 0;
        for (RenderObject* sibling = ancestor->nextSibling(); sibling; sibling = sibling->nextSibling())
            if (filter(sibling))
                return sibling;
    }

    return 0;
}

} // namespace WebCore

#endif // ENABLE(TEXT_AUTOSIZING)
