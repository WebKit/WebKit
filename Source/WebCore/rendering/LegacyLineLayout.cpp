/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 ChangSeok Oh <shivamidow@gmail.com>
 * Copyright (C) 2013 Adobe Systems Inc. All right reserved.
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
#include "LegacyLineLayout.h"

#include "AXObjectCache.h"
#include "BidiResolver.h"
#include "BreakingContext.h"
#include "DocumentInlines.h"
#include "FloatingObjects.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorTextBox.h"
#include "InlineTextBoxStyle.h"
#include "InlineWalker.h"
#include "LegacyInlineFlowBoxInlines.h"
#include "LegacyInlineIterator.h"
#include "LegacyInlineTextBox.h"
#include "LineInlineHeaders.h"
#include "LineLayoutState.h"
#include "Logging.h"
#include "RenderBlockFlowInlines.h"
#include "RenderFragmentContainer.h"
#include "RenderFragmentedFlow.h"
#include "RenderLayoutState.h"
#include "RenderLineBreak.h"
#include "RenderSVGText.h"
#include "RenderView.h"
#include "SVGElementTypeHelpers.h"
#include "SVGRootInlineBox.h"
#include "Settings.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

LegacyLineLayout::LegacyLineLayout(RenderBlockFlow& flow)
    : m_flow(flow)
{
}

LegacyLineLayout::~LegacyLineLayout()
{
    lineBoxes().deleteLineBoxTree();
};

static void determineDirectionality(TextDirection& dir, LegacyInlineIterator iter)
{
    while (!iter.atEnd()) {
        if (iter.atParagraphSeparator())
            return;
        if (UChar current = iter.current()) {
            UCharDirection charDirection = u_charDirection(current);
            if (charDirection == U_LEFT_TO_RIGHT) {
                dir = TextDirection::LTR;
                return;
            }
            if (charDirection == U_RIGHT_TO_LEFT || charDirection == U_RIGHT_TO_LEFT_ARABIC) {
                dir = TextDirection::RTL;
                return;
            }
        }
        iter.increment();
    }
}

inline std::unique_ptr<BidiRun> createRun(int start, int end, RenderObject& obj, InlineBidiResolver& resolver)
{
    return makeUnique<BidiRun>(start, end, obj, resolver.context(), resolver.dir());
}

void LegacyLineLayout::appendRunsForObject(BidiRunList<BidiRun>* runs, int start, int end, RenderObject& obj, InlineBidiResolver& resolver)
{
    if (start > end || RenderBlock::shouldSkipCreatingRunsForObject(obj))
        return;

    LineWhitespaceCollapsingState& lineWhitespaceCollapsingState = resolver.whitespaceCollapsingState();
    bool haveNextTransition = (lineWhitespaceCollapsingState.currentTransition() < lineWhitespaceCollapsingState.numTransitions());
    LegacyInlineIterator nextTransition;
    if (haveNextTransition)
        nextTransition = lineWhitespaceCollapsingState.transitions()[lineWhitespaceCollapsingState.currentTransition()];
    if (lineWhitespaceCollapsingState.betweenTransitions()) {
        if (!haveNextTransition || (&obj != nextTransition.renderer()))
            return;
        // This is a new start point. Stop ignoring objects and
        // adjust our start.
        start = nextTransition.offset();
        lineWhitespaceCollapsingState.incrementCurrentTransition();
        if (start < end) {
            appendRunsForObject(runs, start, end, obj, resolver);
            return;
        }
    } else {
        if (!haveNextTransition || (&obj != nextTransition.renderer())) {
            if (runs)
                runs->appendRun(createRun(start, end, obj, resolver));
            return;
        }

        // An end transition has been encountered within our object. We need to append a run with our endpoint.
        if (static_cast<int>(nextTransition.offset() + 1) <= end) {
            lineWhitespaceCollapsingState.incrementCurrentTransition();
            // The end of the line is before the object we're inspecting. Skip everything and return
            if (nextTransition.refersToEndOfPreviousNode())
                return;
            if (static_cast<int>(nextTransition.offset() + 1) > start && runs)
                runs->appendRun(createRun(start, nextTransition.offset() + 1, obj, resolver));
            appendRunsForObject(runs, nextTransition.offset() + 1, end, obj, resolver);
        } else if (runs)
            runs->appendRun(createRun(start, end, obj, resolver));
    }
}

std::unique_ptr<LegacyRootInlineBox> LegacyLineLayout::createRootInlineBox()
{
    if (CheckedPtr svgText = dynamicDowncast<RenderSVGText>(m_flow)) {
        auto box = makeUnique<SVGRootInlineBox>(*svgText);
        box->setHasVirtualLogicalHeight();
        return box;
    }
        
    return makeUnique<LegacyRootInlineBox>(m_flow);
}

LegacyRootInlineBox* LegacyLineLayout::createAndAppendRootInlineBox()
{
    auto newRootBox = createRootInlineBox();
    LegacyRootInlineBox* rootBox = newRootBox.get();
    m_lineBoxes.appendLineBox(WTFMove(newRootBox));

    if (UNLIKELY(AXObjectCache::accessibilityEnabled()) && firstRootBox() == rootBox) {
        if (AXObjectCache* cache = m_flow.document().existingAXObjectCache())
            cache->deferRecomputeIsIgnored(m_flow.element());
    }

    return rootBox;
}

LegacyInlineBox* LegacyLineLayout::createInlineBoxForRenderer(RenderObject* renderer)
{
    if (renderer == &m_flow)
        return createAndAppendRootInlineBox();

    if (CheckedPtr textRenderer = dynamicDowncast<RenderText>(*renderer))
        return textRenderer->createInlineTextBox();

    if (CheckedPtr renderInline = dynamicDowncast<RenderInline>(renderer))
        return renderInline->createAndAppendInlineFlowBox();

    ASSERT_NOT_REACHED();
    return nullptr;
}

static inline void dirtyLineBoxesForRenderer(RenderObject& renderer, bool fullLayout)
{
    if (CheckedPtr renderText = dynamicDowncast<RenderText>(renderer))
        renderText->dirtyLineBoxes(fullLayout);
    else if (CheckedPtr renderInline = dynamicDowncast<RenderInline>(renderer))
        renderInline->dirtyLineBoxes(fullLayout);
}

static bool parentIsConstructedOrHaveNext(LegacyInlineFlowBox* parentBox)
{
    do {
        if (parentBox->isConstructed() || parentBox->nextOnLine())
            return true;
        parentBox = parentBox->parent();
    } while (parentBox);
    return false;
}

LegacyInlineFlowBox* LegacyLineLayout::createLineBoxes(RenderObject* obj, const LineInfo& lineInfo, LegacyInlineBox* childBox)
{
    // See if we have an unconstructed line box for this object that is also
    // the last item on the line.
    unsigned lineDepth = 1;
    LegacyInlineFlowBox* parentBox = nullptr;
    LegacyInlineFlowBox* result = nullptr;
    do {
        RenderInline* inlineFlow = obj != &m_flow ? &checkedDowncast<RenderInline>(*obj) : nullptr;

        // Get the last box we made for this render object.
        parentBox = inlineFlow ? inlineFlow->lastLineBox() : downcast<RenderBlockFlow>(*obj).lastRootBox();

        // If this box or its ancestor is constructed then it is from a previous line, and we need
        // to make a new box for our line. If this box or its ancestor is unconstructed but it has
        // something following it on the line, then we know we have to make a new box
        // as well. In this situation our inline has actually been split in two on
        // the same line (this can happen with very fancy language mixtures).
        bool constructedNewBox = false;
        bool canUseExistingParentBox = parentBox && !parentIsConstructedOrHaveNext(parentBox);
        if (!canUseExistingParentBox) {
            // We need to make a new box for this render object. Once
            // made, we need to place it at the end of the current line.
            LegacyInlineBox* newBox = createInlineBoxForRenderer(obj);
            parentBox = downcast<LegacyInlineFlowBox>(newBox);
            parentBox->setIsFirstLine(lineInfo.isFirstLine());
            parentBox->setIsHorizontal(m_flow.isHorizontalWritingMode());
            constructedNewBox = true;
        }

        if (constructedNewBox || canUseExistingParentBox) {
            if (!result)
                result = parentBox;

            // If we have hit the block itself, then |box| represents the root
            // inline box for the line, and it doesn't have to be appended to any parent
            // inline.
            if (childBox)
                parentBox->addToLine(childBox);

            if (!constructedNewBox || obj == &m_flow)
                break;

            childBox = parentBox;
        }

        // If we've exceeded our line depth, then jump straight to the root and skip all the remaining
        // intermediate inline flows.
        obj = (++lineDepth >= cMaxLineDepth) ? &m_flow : obj->parent();

    } while (true);

    return result;
}

LegacyRootInlineBox* LegacyLineLayout::constructLine(BidiRunList<BidiRun>& bidiRuns, const LineInfo& lineInfo)
{
    ASSERT(bidiRuns.firstRun());

    LegacyInlineFlowBox* parentBox = 0;
    for (BidiRun* r = bidiRuns.firstRun(); r; r = r->next()) {
        if (lineInfo.isEmpty())
            continue;

        LegacyInlineBox* box = createInlineBoxForRenderer(&r->renderer());
        r->setBox(box);

        // If we have no parent box yet, or if the run is not simply a sibling,
        // then we need to construct inline boxes as necessary to properly enclose the
        // run's inline box. Segments can only be siblings at the root level, as
        // they are positioned separately.
        if (!parentBox || &parentBox->renderer() != r->renderer().parent()) {
            // Create new inline boxes all the way back to the appropriate insertion point.
            RenderObject* parentToUse = r->renderer().parent();
            parentBox = createLineBoxes(parentToUse, lineInfo, box);
        } else {
            // Append the inline box to this line.
            parentBox->addToLine(box);
        }

        box->setBidiLevel(r->level());

        if (auto* textBox = dynamicDowncast<LegacyInlineTextBox>(*box)) {
            textBox->setStart(r->m_start);
            textBox->setLen(r->m_stop - r->m_start);
        }
    }

    // We should have a root inline box. It should be unconstructed and
    // be the last continuation of our line list.
    ASSERT(lastRootBox() && !lastRootBox()->isConstructed());

    // Now mark the line boxes as being constructed.
    lastRootBox()->setConstructed();

    // Return the last line.
    return lastRootBox();
}

static void updateLogicalWidthForLeftAlignedBlock(bool isLeftToRightDirection, BidiRun* trailingSpaceRun, float& logicalLeft, float& totalLogicalWidth, float availableLogicalWidth)
{
    // The direction of the block should determine what happens with wide lines.
    // In particular with RTL blocks, wide lines should still spill out to the left.
    if (isLeftToRightDirection) {
        if (totalLogicalWidth > availableLogicalWidth && trailingSpaceRun)
            trailingSpaceRun->box()->setLogicalWidth(std::max<float>(0, trailingSpaceRun->box()->logicalWidth() - totalLogicalWidth + availableLogicalWidth));
        return;
    }

    if (trailingSpaceRun)
        trailingSpaceRun->box()->setLogicalWidth(0);
    else if (totalLogicalWidth > availableLogicalWidth)
        logicalLeft -= (totalLogicalWidth - availableLogicalWidth);
}

static void updateLogicalWidthForRightAlignedBlock(bool isLeftToRightDirection, BidiRun* trailingSpaceRun, float& logicalLeft, float& totalLogicalWidth, float availableLogicalWidth)
{
    // Wide lines spill out of the block based off direction.
    // So even if text-align is right, if direction is LTR, wide lines should overflow out of the right
    // side of the block.
    if (isLeftToRightDirection) {
        if (trailingSpaceRun) {
            totalLogicalWidth -= trailingSpaceRun->box()->logicalWidth();
            trailingSpaceRun->box()->setLogicalWidth(0);
        }
        logicalLeft += std::max(0.f, availableLogicalWidth - totalLogicalWidth);
        return;
    }

    if (totalLogicalWidth > availableLogicalWidth && trailingSpaceRun) {
        trailingSpaceRun->box()->setLogicalWidth(std::max<float>(0, trailingSpaceRun->box()->logicalWidth() - totalLogicalWidth + availableLogicalWidth));
        totalLogicalWidth -= trailingSpaceRun->box()->logicalWidth();
    } else
        logicalLeft += availableLogicalWidth - totalLogicalWidth;
}

static void updateLogicalWidthForCenterAlignedBlock(bool isLeftToRightDirection, BidiRun* trailingSpaceRun, float& logicalLeft, float& totalLogicalWidth, float availableLogicalWidth)
{
    float trailingSpaceWidth = 0;
    if (trailingSpaceRun) {
        totalLogicalWidth -= trailingSpaceRun->box()->logicalWidth();
        trailingSpaceWidth = std::min(trailingSpaceRun->box()->logicalWidth(), (availableLogicalWidth - totalLogicalWidth + 1) / 2);
        trailingSpaceRun->box()->setLogicalWidth(std::max<float>(0, trailingSpaceWidth));
    }
    if (isLeftToRightDirection)
        logicalLeft += std::max<float>((availableLogicalWidth - totalLogicalWidth) / 2, 0);
    else
        logicalLeft += totalLogicalWidth > availableLogicalWidth ? (availableLogicalWidth - totalLogicalWidth) : (availableLogicalWidth - totalLogicalWidth) / 2 - trailingSpaceWidth;
}

void LegacyLineLayout::updateLogicalWidthForAlignment(RenderBlockFlow& flow, const TextAlignMode& textAlign, const LegacyRootInlineBox* rootInlineBox, BidiRun* trailingSpaceRun, float& logicalLeft, float& totalLogicalWidth, float& availableLogicalWidth, int expansionOpportunityCount)
{
    TextDirection direction;
    if (rootInlineBox && flow.style().unicodeBidi() == UnicodeBidi::Plaintext)
        direction = rootInlineBox->direction();
    else
        direction = flow.style().direction();

    bool isLeftToRightDirection = flow.style().isLeftToRightDirection();

    // Armed with the total width of the line (without justification),
    // we now examine our text-align property in order to determine where to position the
    // objects horizontally. The total width of the line can be increased if we end up
    // justifying text.
    switch (textAlign) {
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
        updateLogicalWidthForLeftAlignedBlock(isLeftToRightDirection, trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        break;
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
        updateLogicalWidthForRightAlignedBlock(isLeftToRightDirection, trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        break;
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        updateLogicalWidthForCenterAlignedBlock(isLeftToRightDirection, trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        break;
    case TextAlignMode::Justify:
        if (expansionOpportunityCount) {
            if (trailingSpaceRun) {
                totalLogicalWidth -= trailingSpaceRun->box()->logicalWidth();
                trailingSpaceRun->box()->setLogicalWidth(0);
            }
            break;
        }
        FALLTHROUGH;
    case TextAlignMode::Start:
        if (direction == TextDirection::LTR)
            updateLogicalWidthForLeftAlignedBlock(isLeftToRightDirection, trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        else
            updateLogicalWidthForRightAlignedBlock(isLeftToRightDirection, trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        break;
    case TextAlignMode::End:
        if (direction == TextDirection::LTR)
            updateLogicalWidthForRightAlignedBlock(isLeftToRightDirection, trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        else
            updateLogicalWidthForLeftAlignedBlock(isLeftToRightDirection, trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        break;
    }
}

void LegacyLineLayout::removeInlineBox(BidiRun& run, const LegacyRootInlineBox& rootLineBox) const
{
    auto* inlineBox = run.box();
#if ASSERT_ENABLED
    auto* inlineParent = inlineBox->parent();
    while (inlineParent && inlineParent != &rootLineBox) {
        ASSERT(!inlineParent->isDirty());
        inlineParent = inlineParent->parent();
    }
    ASSERT(!rootLineBox.isDirty());
#endif
    auto* parent = inlineBox->parent();
    inlineBox->removeFromParent();

    auto& renderer = run.renderer();
    if (CheckedPtr textRenderer = dynamicDowncast<RenderText>(renderer))
        textRenderer->removeTextBox(downcast<LegacyInlineTextBox>(*inlineBox));
    delete inlineBox;
    run.setBox(nullptr);
    // removeFromParent() unnecessarily dirties the ancestor subtree.
    auto* ancestor = parent;
    while (ancestor) {
        ancestor->markDirty(false);
        if (ancestor == &rootLineBox)
            break;
        ancestor = ancestor->parent();
    }
}

void LegacyLineLayout::removeEmptyTextBoxesAndUpdateVisualReordering(LegacyRootInlineBox* lineBox, BidiRun* firstRun)
{
    for (auto* run = firstRun; run; run = run->next()) {
        if (auto* inlineTextBox = dynamicDowncast<LegacyInlineTextBox>(*run->box())) {
            auto& renderer = inlineTextBox->renderer();
            if (!inlineTextBox->hasTextContent()) {
                removeInlineBox(*run, *lineBox);
                continue;
            }
            if (!inlineTextBox->isLeftToRightDirection())
                renderer.setNeedsVisualReordering();
        }
    }
}

static inline bool isCollapsibleSpace(UChar character, const RenderText& renderer)
{
    if (character == ' ' || character == '\t' || character == softHyphen)
        return true;
    if (character == '\n')
        return !renderer.style().preserveNewline();
    if (character == noBreakSpace)
        return renderer.style().nbspMode() == NBSPMode::Space;
    return false;
}

template <typename CharacterType>
static inline unsigned findFirstTrailingSpace(const RenderText& lastText, const CharacterType* characters, unsigned start, unsigned stop)
{
    unsigned firstSpace = stop;
    while (firstSpace > start) {
        UChar current = characters[firstSpace - 1];
        if (!isCollapsibleSpace(current, lastText))
            break;
        firstSpace--;
    }

    return firstSpace;
}

inline BidiRun* LegacyLineLayout::handleTrailingSpaces(BidiRunList<BidiRun>& bidiRuns, BidiContext* currentContext)
{
    if (!bidiRuns.runCount()
        || !bidiRuns.logicallyLastRun()->renderer().style().breakOnlyAfterWhiteSpace()
        || !bidiRuns.logicallyLastRun()->renderer().style().autoWrap())
        return nullptr;

    BidiRun* trailingSpaceRun = bidiRuns.logicallyLastRun();
    CheckedPtr lastText = dynamicDowncast<RenderText>(trailingSpaceRun->renderer());
    if (!lastText)
        return nullptr;

    unsigned firstSpace;
    if (lastText->text().is8Bit())
        firstSpace = findFirstTrailingSpace(*lastText, lastText->text().characters8(), trailingSpaceRun->start(), trailingSpaceRun->stop());
    else
        firstSpace = findFirstTrailingSpace(*lastText, lastText->text().characters16(), trailingSpaceRun->start(), trailingSpaceRun->stop());

    if (firstSpace == trailingSpaceRun->stop())
        return nullptr;

    TextDirection direction = style().direction();
    bool shouldReorder = trailingSpaceRun != (direction == TextDirection::LTR ? bidiRuns.lastRun() : bidiRuns.firstRun());
    if (firstSpace != trailingSpaceRun->start()) {
        BidiContext* baseContext = currentContext;
        while (BidiContext* parent = baseContext->parent())
            baseContext = parent;

        std::unique_ptr<BidiRun> newTrailingRun = makeUnique<BidiRun>(firstSpace, trailingSpaceRun->m_stop, trailingSpaceRun->renderer(), baseContext, U_OTHER_NEUTRAL);
        trailingSpaceRun->m_stop = firstSpace;
        trailingSpaceRun = newTrailingRun.get();
        if (direction == TextDirection::LTR)
            bidiRuns.appendRun(WTFMove(newTrailingRun));
        else
            bidiRuns.prependRun(WTFMove(newTrailingRun));
        return trailingSpaceRun;
    }
    if (!shouldReorder)
        return trailingSpaceRun;

    if (direction == TextDirection::LTR) {
        bidiRuns.moveRunToEnd(trailingSpaceRun);
        trailingSpaceRun->m_level = 0;
    } else {
        bidiRuns.moveRunToBeginning(trailingSpaceRun);
        trailingSpaceRun->m_level = 1;
    }
    return trailingSpaceRun;
}

static inline void notifyResolverToResumeInIsolate(InlineBidiResolver& resolver, RenderObject* root, RenderObject* startObject)
{
    if (root != startObject) {
        RenderObject* parent = startObject->parent();
        notifyResolverToResumeInIsolate(resolver, root, parent);
        notifyObserverEnteredObject(&resolver, startObject);
    }
}

static inline void setUpResolverToResumeInIsolate(InlineBidiResolver& resolver, InlineBidiResolver& topResolver, BidiRun& isolatedRun, RenderObject* root, RenderObject* startObject)
{
    // Set up m_whitespaceCollapsingState
    resolver.whitespaceCollapsingState() = topResolver.whitespaceCollapsingState();
    resolver.whitespaceCollapsingState().setCurrentTransition(topResolver.whitespaceCollapsingTransitionForIsolatedRun(isolatedRun));

    // Set up m_nestedIsolateCount
    notifyResolverToResumeInIsolate(resolver, root, startObject);
}

// FIXME: BidiResolver should have this logic.
static inline void constructBidiRunsForSegment(InlineBidiResolver& topResolver, BidiRunList<BidiRun>& bidiRuns, const LegacyInlineIterator& endOfRuns, VisualDirectionOverride override, bool previousLineBrokeCleanly)
{
    // FIXME: We should pass a BidiRunList into createBidiRunsForLine instead
    // of the resolver owning the runs.
    ASSERT(&topResolver.runs() == &bidiRuns);
    ASSERT(topResolver.position() != endOfRuns);
    topResolver.createBidiRunsForLine(endOfRuns, override, previousLineBrokeCleanly);

    while (!topResolver.isolatedRuns().isEmpty()) {
        // It does not matter which order we resolve the runs as long as we resolve them all.
        auto isolatedRun = WTFMove(topResolver.isolatedRuns().last());
        topResolver.isolatedRuns().removeLast();

        RenderObject& startObject = isolatedRun.object;

        // Only inlines make sense with unicode-bidi: isolate (blocks are already isolated).
        // FIXME: Because enterIsolate is not passed a RenderObject, we have to crawl up the
        // tree to see which parent inline is the isolate. We could change enterIsolate
        // to take a RenderObject and do this logic there, but that would be a layering
        // violation for BidiResolver (which knows nothing about RenderObject).
        RenderInline* isolatedInline = downcast<RenderInline>(highestContainingIsolateWithinRoot(startObject, &isolatedRun.root));
        ASSERT(isolatedInline);

        InlineBidiResolver isolatedResolver;
        auto unicodeBidi = isolatedInline->style().unicodeBidi();
        TextDirection direction;
        if (unicodeBidi == UnicodeBidi::Plaintext)
            determineDirectionality(direction, LegacyInlineIterator(isolatedInline, &isolatedRun.object, 0));
        else {
            ASSERT(unicodeBidi == UnicodeBidi::Isolate || unicodeBidi == UnicodeBidi::IsolateOverride);
            direction = isolatedInline->style().direction();
        }
        isolatedResolver.setStatus(BidiStatus(direction, isOverride(unicodeBidi)));

        setUpResolverToResumeInIsolate(isolatedResolver, topResolver, isolatedRun.runToReplace, isolatedInline, &startObject);

        // The starting position is the beginning of the first run within the isolate that was identified
        // during the earlier call to createBidiRunsForLine. This can be but is not necessarily the
        // first run within the isolate.
        LegacyInlineIterator iter = LegacyInlineIterator(isolatedInline, &startObject, isolatedRun.position);
        isolatedResolver.setPositionIgnoringNestedIsolates(iter);

        // We stop at the next end of line; we may re-enter this isolate in the next call to constructBidiRuns().
        // FIXME: What should end and previousLineBrokeCleanly be?
        // rniwa says previousLineBrokeCleanly is just a WinIE hack and could always be false here?
        isolatedResolver.createBidiRunsForLine(endOfRuns, NoVisualOverride, previousLineBrokeCleanly);
        // Note that we do not delete the runs from the resolver.
        // We're not guaranteed to get any BidiRuns in the previous step. If we don't, we allow the placeholder
        // itself to be turned into an InlineBox. We can't remove it here without potentially losing track of
        // the logically last run.
        if (isolatedResolver.runs().runCount())
            bidiRuns.replaceRunWithRuns(&isolatedRun.runToReplace, isolatedResolver.runs());

        // If we encountered any nested isolate runs, just move them
        // to the top resolver's list for later processing.
        while (!isolatedResolver.isolatedRuns().isEmpty()) {
            auto runWithContext = WTFMove(isolatedResolver.isolatedRuns().last());
            isolatedResolver.isolatedRuns().removeLast();
            topResolver.setWhitespaceCollapsingTransitionForIsolatedRun(runWithContext.runToReplace, isolatedResolver.whitespaceCollapsingTransitionForIsolatedRun(runWithContext.runToReplace));
            topResolver.isolatedRuns().append(WTFMove(runWithContext));
        }
    }
}

// This function constructs line boxes for all of the text runs in the resolver and computes their position.
LegacyRootInlineBox* LegacyLineLayout::createLineBoxesFromBidiRuns(unsigned bidiLevel, BidiRunList<BidiRun>& bidiRuns, const LegacyInlineIterator& end, LineInfo& lineInfo)
{
    if (!bidiRuns.runCount())
        return nullptr;

    // FIXME: Why is this only done when we had runs?
    lineInfo.setLastLine(!end.renderer());

    LegacyRootInlineBox* lineBox = constructLine(bidiRuns, lineInfo);
    if (!lineBox)
        return nullptr;

    lineBox->setBidiLevel(bidiLevel);
    lineBox->setEndsWithBreak(lineInfo.previousLineBrokeCleanly());
    
    bool isSVGRootInlineBox = is<SVGRootInlineBox>(*lineBox);
    ASSERT(isSVGRootInlineBox);

    GlyphOverflowAndFallbackFontsMap textBoxDataMap;
    
    // Now we position all of our text runs horizontally.

    removeEmptyTextBoxesAndUpdateVisualReordering(lineBox, bidiRuns.firstRun());

    // SVG text layout code computes vertical & horizontal positions on its own.
    // Note that we still need to execute computeVerticalPositionsForLine() as
    // it calls LegacyInlineTextBox::positionLineBox(), which tracks whether the box
    // contains reversed text or not. If we wouldn't do that editing and thus
    // text selection in RTL boxes would not work as expected.
    if (isSVGRootInlineBox) {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(m_flow.isRenderSVGText());
        downcast<SVGRootInlineBox>(*lineBox).computePerCharacterLayoutInformation();
    }
    
    // Compute our overflow now.
    lineBox->computeOverflow(lineBox->lineTop(), lineBox->lineBottom(), textBoxDataMap);
    
    return lineBox;
}

static void deleteLineRange(LineLayoutState& layoutState, LegacyRootInlineBox* startLine, LegacyRootInlineBox* stopLine = 0)
{
    LegacyRootInlineBox* boxToDelete = startLine;
    while (boxToDelete && boxToDelete != stopLine) {
        layoutState.updateRepaintRangeFromBox(boxToDelete);
        // Note: deleteLineRange(firstRootBox()) is not identical to deleteLineBoxTree().
        // deleteLineBoxTree uses nextLineBox() instead of nextRootBox() when traversing.
        LegacyRootInlineBox* next = boxToDelete->nextRootBox();
        boxToDelete->deleteLine();
        boxToDelete = next;
    }
}

static void repaintSelfPaintInlineBoxes(const LegacyRootInlineBox& firstRootInlineBox, const LegacyRootInlineBox& lastRootInlineBox)
{
    for (auto* rootInlineBox = &firstRootInlineBox; rootInlineBox; rootInlineBox = rootInlineBox->nextRootBox()) {
        if (rootInlineBox->hasSelfPaintInlineBox()) {
            for (auto* inlineBox = rootInlineBox->firstChild(); inlineBox; inlineBox = inlineBox->nextOnLine()) {
                if (auto* renderer = dynamicDowncast<RenderLayerModelObject>(inlineBox->renderer()); renderer && renderer->hasSelfPaintingLayer())
                    renderer->repaint();
            }
        }
        if (rootInlineBox == &lastRootInlineBox)
            break;
    }
}

void LegacyLineLayout::layoutRunsAndFloats(LineLayoutState& layoutState, bool hasInlineChild)
{
    // We want to skip ahead to the first dirty line
    InlineBidiResolver resolver;
    LegacyRootInlineBox* startLine = determineStartPosition(layoutState, resolver);

    // FIXME: This would make more sense outside of this function, but since
    // determineStartPosition can change the fullLayout flag we have to do this here. Failure to call
    // determineStartPosition first will break fast/repaint/line-flow-with-floats-9.html.
    if (layoutState.isFullLayout() && hasInlineChild && !m_flow.selfNeedsLayout()) {
        m_flow.setNeedsLayout(MarkOnlyThis); // Mark as needing a full layout to force us to repaint.
        if (!layoutContext().needsFullRepaint() && m_flow.cachedLayerClippedOverflowRect()) {
            // Because we waited until we were already inside layout to discover
            // that the block really needed a full layout, we missed our chance to repaint the layer
            // before layout started. Luckily the layer has cached the repaint rect for its original
            // position and size, and so we can use that to make a repaint happen now.
            m_flow.repaintUsingContainer(m_flow.containerForRepaint().renderer.get(), *m_flow.cachedLayerClippedOverflowRect());
        }
    }

    // We also find the first clean line and extract these lines. We will add them back
    // if we determine that we're able to synchronize after handling all our dirty lines.
    LegacyInlineIterator cleanLineStart;
    if (!layoutState.isFullLayout() && startLine)
        determineEndPosition(layoutState, startLine, cleanLineStart);

    if (startLine) {
        if (!layoutState.usesRepaintBounds())
            layoutState.setRepaintRange(m_flow.logicalHeight());
        deleteLineRange(layoutState, startLine);
    }

    layoutRunsAndFloatsInRange(layoutState, resolver, cleanLineStart);
    linkToEndLineIfNeeded(layoutState);
    if (firstRootBox())
        repaintSelfPaintInlineBoxes(*firstRootBox(), layoutState.endLine() ? *layoutState.endLine() : *lastRootBox());
}

void LegacyLineLayout::layoutRunsAndFloatsInRange(LineLayoutState& layoutState, InlineBidiResolver& resolver, const LegacyInlineIterator& cleanLineStart)
{
    const RenderStyle& styleToUse = style();
    LineWhitespaceCollapsingState& lineWhitespaceCollapsingState = resolver.whitespaceCollapsingState();
    LegacyInlineIterator end = resolver.position();
    bool checkForEndLineMatch = layoutState.endLine();
    RenderTextInfo renderTextInfo;

    LineBreaker lineBreaker(m_flow);

    while (!end.atEnd()) {
        // FIXME: Is this check necessary before the first iteration or can it be moved to the end?
        if (checkForEndLineMatch) {
            layoutState.setEndLineMatched(matchedEndLine(layoutState, resolver, cleanLineStart));
            if (layoutState.endLineMatched()) {
                resolver.setPosition(LegacyInlineIterator(resolver.position().root(), 0, 0), 0);
                layoutState.marginInfo().clearMargin();
                break;
            }
        }

        lineWhitespaceCollapsingState.reset();

        layoutState.lineInfo().setEmpty(true);
        layoutState.lineInfo().resetRunsFromLeadingWhitespace();

        bool isNewUBAParagraph = layoutState.lineInfo().previousLineBrokeCleanly();

        WordMeasurements wordMeasurements;
        end = lineBreaker.nextLineBreak(resolver, layoutState.lineInfo(), renderTextInfo, wordMeasurements);
        m_flow.cachePriorCharactersIfNeeded(renderTextInfo.lineBreakIteratorFactory);
        renderTextInfo.lineBreakIteratorFactory.priorContext().reset();
        if (resolver.position().atEnd()) {
            // FIXME: We shouldn't be creating any runs in nextLineBreak to begin with!
            // Once BidiRunList is separated from BidiResolver this will not be needed.
            resolver.runs().clear();
            resolver.markCurrentRunEmpty(); // FIXME: This can probably be replaced by an ASSERT (or just removed).
            resolver.setPosition(LegacyInlineIterator(resolver.position().root(), 0, 0), 0);
            break;
        }

        ASSERT(end != resolver.position());

        // This is a short-cut for empty lines.
        if (layoutState.lineInfo().isEmpty()) {
            if (lastRootBox())
                lastRootBox()->setLineBreakInfo(end.renderer(), end.offset(), resolver.status());
        } else {
            VisualDirectionOverride override = (styleToUse.rtlOrdering() == Order::Visual ? (styleToUse.direction() == TextDirection::LTR ? VisualLeftToRightOverride : VisualRightToLeftOverride) : NoVisualOverride);

            if (isNewUBAParagraph && styleToUse.unicodeBidi() == UnicodeBidi::Plaintext && !resolver.context()->parent()) {
                TextDirection direction = styleToUse.direction();
                determineDirectionality(direction, resolver.position());
                resolver.setStatus(BidiStatus(direction, isOverride(styleToUse.unicodeBidi())));
            }
            // FIXME: This ownership is reversed. We should own the BidiRunList and pass it to createBidiRunsForLine.
            BidiRunList<BidiRun>& bidiRuns = resolver.runs();
            constructBidiRunsForSegment(resolver, bidiRuns, end, override, layoutState.lineInfo().previousLineBrokeCleanly());
            ASSERT(resolver.position() == end);

            if (!layoutState.lineInfo().previousLineBrokeCleanly())
                handleTrailingSpaces(bidiRuns, resolver.context());

            // Now that the runs have been ordered, we create the line boxes.
            // At the same time we figure out where border/padding/margin should be applied for
            // inline flow boxes.

            LegacyRootInlineBox* lineBox = createLineBoxesFromBidiRuns(resolver.status().context->level(), bidiRuns, end, layoutState.lineInfo());

            bidiRuns.clear();
            resolver.markCurrentRunEmpty(); // FIXME: This can probably be replaced by an ASSERT (or just removed).

            if (lineBox) {
                lineBox->setLineBreakInfo(end.renderer(), end.offset(), resolver.status());
                if (layoutState.usesRepaintBounds())
                    layoutState.updateRepaintRangeFromBox(lineBox);

                layoutState.marginInfo().setAtBeforeSideOfBlock(false);
            }
        }

        if (!layoutState.lineInfo().isEmpty())
            layoutState.lineInfo().setFirstLine(false);

        lineWhitespaceCollapsingState.reset();
        resolver.setPosition(end, numberOfIsolateAncestors(end));
    }
}

void LegacyLineLayout::linkToEndLineIfNeeded(LineLayoutState& layoutState)
{
    auto* firstCleanLine = layoutState.endLine();
    if (firstCleanLine) {
        if (layoutState.endLineMatched()) {
            // Attach all the remaining lines, and then adjust their y-positions as needed.
            LayoutUnit delta = m_flow.logicalHeight() - layoutState.endLineLogicalTop();
            for (auto* line = firstCleanLine; line; line = line->nextRootBox()) {
                line->attachLine();
                if (delta) {
                    layoutState.updateRepaintRangeFromBox(line, delta);
                    line->adjustBlockDirectionPosition(delta);
                }
            }
            m_flow.setLogicalHeight(lastRootBox()->lineBoxBottom());
        } else {
            // Delete all the remaining lines.
            deleteLineRange(layoutState, layoutState.endLine());
        }
    }
}

void LegacyLineLayout::layoutLineBoxes(bool relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom)
{
    m_flow.setLogicalHeight(m_flow.borderAndPaddingBefore());

    // Figure out if we should clear out our line boxes.
    // FIXME: Handle resize eventually!
    bool isFullLayout = !firstRootBox() || m_flow.selfNeedsLayout() || relayoutChildren;
    LineLayoutState layoutState(m_flow, isFullLayout, repaintLogicalTop, repaintLogicalBottom);

    if (isFullLayout)
        lineBoxes().deleteLineBoxes();

    if (m_flow.firstChild()) {
        // In full layout mode, clear the line boxes of children upfront. Otherwise,
        // siblings can run into stale root lineboxes during layout. Then layout
        // the replaced elements later. In partial layout mode, line boxes are not
        // deleted and only dirtied. In that case, we can layout the replaced
        // elements at the same time.
        bool hasInlineChild = false;
        auto hasDirtyRenderCounterWithInlineBoxParent = false;
        Vector<RenderBox*> replacedChildren;
        for (InlineWalker walker(m_flow); !walker.atEnd(); walker.advance()) {
            RenderObject& o = *walker.current();

            if (!hasInlineChild && o.isInline())
                hasInlineChild = true;

            if (layoutState.isFullLayout() || o.selfNeedsLayout()) {
                dirtyLineBoxesForRenderer(o, layoutState.isFullLayout());
                hasDirtyRenderCounterWithInlineBoxParent = hasDirtyRenderCounterWithInlineBoxParent || (is<RenderCounter>(o) && is<RenderInline>(o.parent()));
            }
            o.clearNeedsLayout();
        }

        for (size_t i = 0; i < replacedChildren.size(); i++)
            replacedChildren[i]->layoutIfNeeded();

        auto clearNeedsLayoutIfNeeded = [&] {
            if (!hasDirtyRenderCounterWithInlineBoxParent)
                return;
            for (InlineWalker walker(m_flow); !walker.atEnd(); walker.advance()) {
                auto& renderer = *walker.current();
                if (is<RenderCounter>(renderer) || is<RenderInline>(renderer))
                    renderer.clearNeedsLayout();
            }
        };
        clearNeedsLayoutIfNeeded();

        layoutRunsAndFloats(layoutState, hasInlineChild);
    }

    // Now do the handling of the bottom of the block, adding in our bottom border/padding and
    // determining the correct collapsed bottom margin information. This collapse is only necessary
    // if our last child was an anonymous inline block that might need to propagate margin information out to
    // us.
    LayoutUnit afterEdge = m_flow.borderAndPaddingAfter() + m_flow.scrollbarLogicalHeight();
    m_flow.setLogicalHeight(m_flow.logicalHeight() + afterEdge);

    if (!firstRootBox() && m_flow.hasLineIfEmpty())
        m_flow.setLogicalHeight(m_flow.logicalHeight() + m_flow.lineHeight(true, m_flow.isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes));
}

LegacyRootInlineBox* LegacyLineLayout::determineStartPosition(LineLayoutState& layoutState, InlineBidiResolver& resolver)
{
    LegacyRootInlineBox* currentLine = nullptr;
    LegacyRootInlineBox* lastLine = nullptr;

    if (layoutState.isFullLayout()) {
        m_lineBoxes.deleteLineBoxTree();
        ASSERT(!firstRootBox() && !lastRootBox());
    } else {
        for (currentLine = firstRootBox(); currentLine && !currentLine->isDirty(); currentLine = currentLine->nextRootBox()) { }
        if (currentLine) {
            // We have a dirty line.
            if (LegacyRootInlineBox* prevRootBox = currentLine->prevRootBox()) {
                // We have a previous line.
                if (!prevRootBox->endsWithBreak()
                    || !prevRootBox->lineBreakObj()
                    || (is<RenderText>(*prevRootBox->lineBreakObj())
                    && prevRootBox->lineBreakPos() >= downcast<RenderText>(*prevRootBox->lineBreakObj()).text().length())) {
                    // The previous line didn't break cleanly or broke at a newline
                    // that has been deleted, so treat it as dirty too.
                    currentLine = prevRootBox;
                }
            }
        }
        // If we have no dirty lines, then last is just the last root box.
        lastLine = currentLine ? currentLine->prevRootBox() : lastRootBox();
    }

    layoutState.lineInfo().setFirstLine(!lastLine);
    layoutState.lineInfo().setPreviousLineBrokeCleanly(!lastLine || lastLine->endsWithBreak());

    if (lastLine) {
        m_flow.setLogicalHeight(lastLine->lineBoxBottom());
        LegacyInlineIterator iter = LegacyInlineIterator(&m_flow, lastLine->lineBreakObj(), lastLine->lineBreakPos());
        resolver.setPosition(iter, numberOfIsolateAncestors(iter));
        resolver.setStatus(lastLine->lineBreakBidiStatus());
    } else {
        TextDirection direction = style().direction();
        if (style().unicodeBidi() == UnicodeBidi::Plaintext)
            determineDirectionality(direction, LegacyInlineIterator(&m_flow, firstInlineRendererSkippingEmpty(m_flow), 0));
        resolver.setStatus(BidiStatus(direction, isOverride(style().unicodeBidi())));
        LegacyInlineIterator iter = LegacyInlineIterator(&m_flow, firstInlineRendererSkippingEmpty(m_flow, &resolver), 0);
        resolver.setPosition(iter, numberOfIsolateAncestors(iter));
    }
    return currentLine;
}

void LegacyLineLayout::determineEndPosition(LineLayoutState& layoutState, LegacyRootInlineBox* startLine, LegacyInlineIterator& cleanLineStart)
{
    ASSERT(!layoutState.endLine());
    LegacyRootInlineBox* lastLine = nullptr;
    for (auto* currentLine = startLine->nextRootBox(); currentLine; currentLine = currentLine->nextRootBox()) {
        if (currentLine->isDirty())
            lastLine = nullptr;
        else if (!lastLine)
            lastLine = currentLine;
    }

    if (!lastLine)
        return;

    // At this point, |last| is the first line in a run of clean lines that ends with the last line
    // in the block.
    LegacyRootInlineBox* previousLine = lastLine->prevRootBox();
    cleanLineStart = LegacyInlineIterator(&m_flow, previousLine->lineBreakObj(), previousLine->lineBreakPos());
    layoutState.setEndLineLogicalTop(previousLine->lineBoxBottom());

    for (auto* line = lastLine; line; line = line->nextRootBox()) {
        // Disconnect all line boxes from their render objects while preserving their connections to one another.
        line->extractLine();
    }
    layoutState.setEndLine(lastLine);
}

bool LegacyLineLayout::matchedEndLine(LineLayoutState& layoutState, const InlineBidiResolver& resolver, const LegacyInlineIterator& endLineStart)
{
    if (resolver.position() == endLineStart)
        return false;

    // The first clean line doesn't match, but we can check a handful of following lines to try
    // to match back up.
    static const int numLines = 8; // The # of lines we're willing to match against.
    LegacyRootInlineBox* originalEndLine = layoutState.endLine();
    LegacyRootInlineBox* line = originalEndLine;
    for (int i = 0; i < numLines && line; i++, line = line->nextRootBox()) {
        if (line->lineBreakObj() == resolver.position().renderer() && line->lineBreakPos() == resolver.position().offset()) {
            // We have a match.
            if (line->lineBreakBidiStatus() != resolver.status())
                return false; // ...but the bidi state doesn't match.
            
            bool matched = false;
            LegacyRootInlineBox* result = line->nextRootBox();
            layoutState.setEndLine(result);
            if (result) {
                layoutState.setEndLineLogicalTop(line->lineBoxBottom());
                matched = true;
            }

            // Now delete the lines that we failed to sync.
            deleteLineRange(layoutState, originalEndLine, result);
            return matched;
        }
    }

    return false;
}

void LegacyLineLayout::addOverflowFromInlineChildren()
{
    LayoutUnit endPadding = m_flow.hasNonVisibleOverflow() ? m_flow.paddingEnd() : 0_lu;
    // FIXME: Need to find another way to do this, since scrollbars could show when we don't want them to.
    if (!endPadding)
        endPadding = m_flow.endPaddingWidthForCaret();
    if (m_flow.hasNonVisibleOverflow() && !endPadding && m_flow.element() && m_flow.element()->isRootEditableElement() && style().isLeftToRightDirection())
        endPadding = 1;
    for (auto* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
        m_flow.addLayoutOverflow(curr->paddedLayoutOverflowRect(endPadding));
        if (!m_flow.hasNonVisibleOverflow()) {
            LayoutRect childVisualOverflowRect = curr->visualOverflowRect(curr->lineTop(), curr->lineBottom());
            m_flow.addVisualOverflow(childVisualOverflowRect);
        }
    }
}

size_t LegacyLineLayout::lineCount() const
{
    size_t count = 0;
    for (auto* box = firstRootBox(); box; box = box->nextRootBox())
        ++count;

    return count;
}

size_t LegacyLineLayout::lineCountUntil(const LegacyRootInlineBox* stopRootInlineBox) const
{
    size_t count = 0;
    for (auto* box = firstRootBox(); box; box = box->nextRootBox()) {
        ++count;
        if (box == stopRootInlineBox)
            break;
    }

    return count;
}

const RenderStyle& LegacyLineLayout::style() const
{
    return m_flow.style();
}

const LocalFrameViewLayoutContext& LegacyLineLayout::layoutContext() const
{
    return m_flow.view().frameView().layoutContext();
}


}
