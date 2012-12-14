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

#include <algorithm>

namespace WebCore {

struct TextAutosizingWindowInfo {
    IntSize windowSize;
    IntSize minLayoutSize;
};

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

    TextAutosizingWindowInfo windowInfo;

    // Window area, in logical (density-independent) pixels.
    windowInfo.windowSize = m_document->settings()->textAutosizingWindowSizeOverride();
    if (windowInfo.windowSize.isEmpty()) {
        bool includeScrollbars = !InspectorInstrumentation::shouldApplyScreenWidthOverride(mainFrame);
        windowInfo.windowSize = mainFrame->view()->visibleContentRect(includeScrollbars).size();
        if (!m_document->settings()->applyPageScaleFactorInCompositor())
            windowInfo.windowSize.scale(1 / m_document->page()->deviceScaleFactor());
    }

    // Largest area of block that can be visible at once (assuming the main
    // frame doesn't get scaled to less than overview scale), in CSS pixels.
    windowInfo.minLayoutSize = mainFrame->view()->layoutSize();
    for (Frame* frame = m_document->frame(); frame; frame = frame->tree()->parent()) {
        if (!frame->view()->isInChildFrameWithFrameFlattening())
            windowInfo.minLayoutSize = windowInfo.minLayoutSize.shrunkTo(frame->view()->layoutSize());
    }

    // The layoutRoot could be neither a container nor a cluster, so walk up the tree till we find each of these.
    RenderBlock* container = layoutRoot->isRenderBlock() ? toRenderBlock(layoutRoot) : layoutRoot->containingBlock();
    while (container && !isAutosizingContainer(container))
        container = container->containingBlock();

    RenderBlock* cluster = container;
    while (cluster && (!isAutosizingContainer(cluster) || !isAutosizingCluster(cluster)))
        cluster = cluster->containingBlock();

    processCluster(cluster, container, layoutRoot, windowInfo);
    return true;
}

void TextAutosizer::processCluster(RenderBlock* cluster, RenderBlock* container, RenderObject* subtreeRoot, const TextAutosizingWindowInfo& windowInfo)
{
    ASSERT(isAutosizingCluster(cluster));

    // Many pages set a max-width on their content. So especially for the
    // RenderView, instead of just taking the width of |cluster| we find
    // the lowest common ancestor of the first and last descendant text node of
    // the cluster (i.e. the deepest wrapper block that contains all the text),
    // and use its width instead.
    const RenderBlock* lowestCommonAncestor = findDeepestBlockContainingAllText(cluster);
    float commonAncestorWidth = lowestCommonAncestor->contentLogicalWidth();

    float multiplier = 1;
    if (clusterShouldBeAutosized(lowestCommonAncestor, commonAncestorWidth)) {
        int logicalWindowWidth = cluster->isHorizontalWritingMode() ? windowInfo.windowSize.width() : windowInfo.windowSize.height();
        int logicalLayoutWidth = cluster->isHorizontalWritingMode() ? windowInfo.minLayoutSize.width() : windowInfo.minLayoutSize.height();
        // Ignore box width in excess of the layout width, to avoid extreme multipliers.
        float logicalClusterWidth = std::min<float>(commonAncestorWidth, logicalLayoutWidth);

        multiplier = logicalClusterWidth / logicalWindowWidth;
        multiplier *= m_document->settings()->textAutosizingFontScaleFactor();
        multiplier = std::max(1.0f, multiplier);
    }

    processContainer(multiplier, container, subtreeRoot, windowInfo);
}

void TextAutosizer::processContainer(float multiplier, RenderBlock* container, RenderObject* subtreeRoot, const TextAutosizingWindowInfo& windowInfo)
{
    ASSERT(isAutosizingContainer(container));

    float localMultiplier = containerShouldbeAutosized(container) ? multiplier: 1;

    RenderObject* descendant = nextInPreOrderSkippingDescendantsOfContainers(subtreeRoot, subtreeRoot);
    while (descendant) {
        if (descendant->isText()) {
            if (localMultiplier != descendant->style()->textAutosizingMultiplier()) {
                setMultiplier(descendant, localMultiplier);
                setMultiplier(descendant->parent(), localMultiplier); // Parent does line spacing.
            }
            // FIXME: Increase list marker size proportionately.
        } else if (isAutosizingContainer(descendant)) {
            RenderBlock* descendantBlock = toRenderBlock(descendant);
            if (isAutosizingCluster(descendantBlock))
                processCluster(descendantBlock, descendantBlock, descendantBlock, windowInfo);
            else
                processContainer(multiplier, descendantBlock, descendantBlock, windowInfo);
        }
        descendant = nextInPreOrderSkippingDescendantsOfContainers(descendant, subtreeRoot);
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

bool TextAutosizer::isAutosizingContainer(const RenderObject* renderer)
{
    // "Autosizing containers" are the smallest unit for which we can
    // enable/disable Text Autosizing.
    // - Must not be inline, as different multipliers on one line looks terrible.
    // - Must not be list items, as items in the same list should look consistent (*).
    // - Must not be normal list items, as items in the same list should look
    //   consistent, unless they are floating or position:absolute/fixed.
    if (!renderer->isRenderBlock() || renderer->isInline())
        return false;
    if (renderer->isListItem())
        return renderer->isFloating() || renderer->isOutOfFlowPositioned();
    return true;
}

bool TextAutosizer::isAutosizingCluster(const RenderBlock* renderer)
{
    // "Autosizing clusters" are special autosizing containers within which we
    // want to enforce a uniform text size multiplier, in the hopes of making
    // the major sections of the page look internally consistent.
    // All their descendents (including other autosizing containers) must share
    // the same multiplier, except for subtrees which are themselves clusters,
    // and some of their descendent containers might not be autosized at all
    // (for example if their height is constrained).
    // Additionally, clusterShouldBeAutosized requires each cluster to contain a
    // minimum amount of text, without which it won't be autosized.
    //
    // Clusters are chosen using very similar criteria to CSS flow roots, aka
    // block formatting contexts (http://w3.org/TR/css3-box/#flow-root), since
    // flow roots correspond to box containers that behave somewhat
    // independently from their parent (for example they don't overlap floats).
    // The definition of a flow flow root also conveniently includes most of the
    // ways that a box and its children can have significantly different width
    // from the box's parent (we want to avoid having significantly different
    // width blocks within a cluster, since the narrower blocks would end up
    // larger than would otherwise be necessary).
    ASSERT(isAutosizingContainer(renderer));

    return renderer->isRenderView()
        || renderer->isFloating()
        || renderer->isOutOfFlowPositioned()
        || renderer->isTableCell()
        || renderer->isTableCaption()
        || renderer->isFlexibleBoxIncludingDeprecated()
        || renderer->hasColumns()
        || renderer->containingBlock()->isHorizontalWritingMode() != renderer->isHorizontalWritingMode();
    // FIXME: Tables need special handling to multiply all their columns by
    // the same amount even if they're different widths; so do hasColumns()
    // renderers, and probably flexboxes...
}

static bool contentHeightIsConstrained(const RenderBlock* container)
{
    // FIXME: Propagate constrainedness down the tree, to avoid inefficiently walking back up from each box.
    // FIXME: This code needs to take into account vertical writing modes.
    // FIXME: Consider additional heuristics, such as ignoring fixed heights if the content is already overflowing before autosizing kicks in.
    for (; container; container = container->containingBlock()) {
        RenderStyle* style = container->style();
        if (style->overflowY() >= OSCROLL)
            return false;
        if (style->height().isSpecified() || style->maxHeight().isSpecified()) {
            // Some sites (e.g. wikipedia) set their html and/or body elements to height:100%,
            // without intending to constrain the height of the content within them.
            return !container->isRoot() && !container->isBody();
        }
        if (container->isFloatingOrOutOfFlowPositioned())
            return false;
    }
    return false;
}

bool TextAutosizer::containerShouldbeAutosized(const RenderBlock* container)
{
    // Don't autosize block-level text that can't wrap (as it's likely to
    // expand sideways and break the page's layout).
    if (!container->style()->autoWrap())
        return false;

    return !contentHeightIsConstrained(container);
}

bool TextAutosizer::clusterShouldBeAutosized(const RenderBlock* lowestCommonAncestor, float commonAncestorWidth)
{
    // Don't autosize clusters that contain less than 4 lines of text (in
    // practice less lines are required, since measureDescendantTextWidth
    // assumes that characters are 1em wide, but most characters are narrower
    // than that, so we're overestimating their contribution to the linecount).
    //
    // This is to reduce the likelihood of autosizing things like headers and
    // footers, which can be quite visually distracting. The rationale is that
    // if a cluster contains very few lines of text then it's ok to have to zoom
    // in and pan from side to side to read each line, since if there are very
    // few lines of text you'll only need to pan across once or twice.
    const float minLinesOfText = 4;
    float minTextWidth = commonAncestorWidth * minLinesOfText;
    float textWidth = 0;
    measureDescendantTextWidth(lowestCommonAncestor, minTextWidth, textWidth);
    if (textWidth >= minTextWidth)
        return true;
    return false;
}

void TextAutosizer::measureDescendantTextWidth(const RenderBlock* container, float minTextWidth, float& textWidth)
{
    bool skipLocalText = !containerShouldbeAutosized(container);

    RenderObject* descendant = nextInPreOrderSkippingDescendantsOfContainers(container, container);
    while (descendant) {
        if (!skipLocalText && descendant->isText()) {
            textWidth += toRenderText(descendant)->renderedTextLength() * descendant->style()->specifiedFontSize();
        } else if (isAutosizingContainer(descendant)) {
            RenderBlock* descendantBlock = toRenderBlock(descendant);
            if (!isAutosizingCluster(descendantBlock))
                measureDescendantTextWidth(descendantBlock, minTextWidth, textWidth);
        }
        if (textWidth >= minTextWidth)
            return;
        descendant = nextInPreOrderSkippingDescendantsOfContainers(descendant, container);
    }
}

RenderObject* TextAutosizer::nextInPreOrderSkippingDescendantsOfContainers(const RenderObject* current, const RenderObject* stayWithin)
{
    if (current == stayWithin || !isAutosizingContainer(current))
        for (RenderObject* child = current->firstChild(); child; child = child->nextSibling())
            return child;

    for (const RenderObject* ancestor = current; ancestor; ancestor = ancestor->parent()) {
        if (ancestor == stayWithin)
            return 0;
        for (RenderObject* sibling = ancestor->nextSibling(); sibling; sibling = sibling->nextSibling())
            return sibling;
    }

    return 0;
}

const RenderBlock* TextAutosizer::findDeepestBlockContainingAllText(const RenderBlock* cluster)
{
    ASSERT(isAutosizingCluster(cluster));

    size_t firstDepth = 0;
    const RenderObject* firstTextLeaf = findFirstTextLeafNotInCluster(cluster, firstDepth, FirstToLast);
    if (!firstTextLeaf)
        return cluster;

    size_t lastDepth = 0;
    const RenderObject* lastTextLeaf = findFirstTextLeafNotInCluster(cluster, lastDepth, LastToFirst);
    ASSERT(lastTextLeaf);

    // Equalize the depths if necessary. Only one of the while loops below will get executed.
    const RenderObject* firstNode = firstTextLeaf;
    const RenderObject* lastNode = lastTextLeaf;
    while (firstDepth > lastDepth) {
        firstNode = firstNode->parent();
        --firstDepth;
    }
    while (lastDepth > firstDepth) {
        lastNode = lastNode->parent();
        --lastDepth;
    }

    // Go up from both nodes until the parent is the same. Both pointers will point to the LCA then.
    while (firstNode != lastNode) {
        firstNode = firstNode->parent();
        lastNode = lastNode->parent();
    }

    if (firstNode->isRenderBlock())
        return toRenderBlock(firstNode);

    // containingBlock() should never leave the cluster, since it only skips ancestors when finding the
    // container of position:absolute/fixed blocks, and those cannot exist between a cluster and its text
    // nodes lowest common ancestor as isAutosizingCluster would have made them into their own independent
    // cluster.
    RenderBlock* containingBlock = firstNode->containingBlock();
    ASSERT(containingBlock->isDescendantOf(cluster));

    return containingBlock;
}

const RenderObject* TextAutosizer::findFirstTextLeafNotInCluster(const RenderObject* parent, size_t& depth, TraversalDirection direction)
{
    if (parent->isEmpty())
        return parent->isText() ? parent : 0;

    ++depth;
    const RenderObject* child = (direction == FirstToLast) ? parent->firstChild() : parent->lastChild();
    while (child) {
        if (!isAutosizingContainer(child) || !isAutosizingCluster(toRenderBlock(child))) {
            const RenderObject* leaf = findFirstTextLeafNotInCluster(child, depth, direction);
            if (leaf)
                return leaf;
        }
        child = (direction == FirstToLast) ? child->nextSibling() : child->previousSibling();
    }
    --depth;

    return 0;
}

} // namespace WebCore

#endif // ENABLE(TEXT_AUTOSIZING)
