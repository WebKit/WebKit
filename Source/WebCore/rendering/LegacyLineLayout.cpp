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
#include "LegacyInlineIterator.h"
#include "LegacyInlineTextBox.h"
#include "LineInfo.h"
#include "LineInlineHeaders.h"
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

    if (UNLIKELY(AXObjectCache::accessibilityEnabled()) && legacyRootBox() == rootBox) {
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

static inline void dirtyLineBoxesForRenderer(RenderObject& renderer)
{
    if (CheckedPtr renderText = dynamicDowncast<RenderText>(renderer))
        renderText->dirtyLineBoxes(true);
    else if (CheckedPtr renderInline = dynamicDowncast<RenderInline>(renderer))
        renderInline->dirtyLineBoxes(true);
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
        RenderInline* inlineFlow = obj != &m_flow ? &downcast<RenderInline>(*obj) : nullptr;

        // Get the last box we made for this render object.
        parentBox = inlineFlow ? inlineFlow->lastLegacyInlineBox() : downcast<RenderBlockFlow>(*obj).legacyRootBox();

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
    ASSERT(legacyRootBox() && !legacyRootBox()->isConstructed());

    // Now mark the line boxes as being constructed.
    legacyRootBox()->setConstructed();
    return legacyRootBox();
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
static inline void constructBidiRunsForSegment(InlineBidiResolver& topResolver, BidiRunList<BidiRun>& bidiRuns, const LegacyInlineIterator& endOfRuns, VisualDirectionOverride override)
{
    // FIXME: We should pass a BidiRunList into createBidiRunsForLine instead
    // of the resolver owning the runs.
    ASSERT(&topResolver.runs() == &bidiRuns);
    ASSERT(topResolver.position() != endOfRuns);
    topResolver.createBidiRunsForLine(endOfRuns, override);

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
        isolatedResolver.createBidiRunsForLine(endOfRuns, NoVisualOverride);
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

    bool isSVGRootInlineBox = is<SVGRootInlineBox>(*lineBox);
    ASSERT(isSVGRootInlineBox);

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

    GlyphOverflowAndFallbackFontsMap textBoxDataMap;
    lineBox->computeOverflow(lineBox->lineTop(), lineBox->lineBottom(), textBoxDataMap);

    return lineBox;
}

static void repaintSelfPaintInlineBoxes(const LegacyRootInlineBox& rootInlineBox)
{
    if (rootInlineBox.hasSelfPaintInlineBox()) {
        for (auto* inlineBox = rootInlineBox.firstChild(); inlineBox; inlineBox = inlineBox->nextOnLine()) {
            if (auto* renderer = dynamicDowncast<RenderLayerModelObject>(inlineBox->renderer()); renderer && renderer->hasSelfPaintingLayer())
                renderer->repaint();
        }
    }
}

void LegacyLineLayout::layoutRunsAndFloats(bool hasInlineChild)
{
    m_lineBoxes.deleteLineBoxTree();

    TextDirection direction = style().direction();
    if (style().unicodeBidi() == UnicodeBidi::Plaintext)
        determineDirectionality(direction, LegacyInlineIterator(&m_flow, firstInlineRendererSkippingEmpty(m_flow), 0));

    InlineBidiResolver resolver;
    resolver.setStatus(BidiStatus(direction, isOverride(style().unicodeBidi())));
    LegacyInlineIterator iter = LegacyInlineIterator(&m_flow, firstInlineRendererSkippingEmpty(m_flow, &resolver), 0);
    resolver.setPosition(iter, numberOfIsolateAncestors(iter));

    if (hasInlineChild && !m_flow.selfNeedsLayout()) {
        m_flow.setNeedsLayout(MarkOnlyThis); // Mark as needing a full layout to force us to repaint.
        if (!layoutContext().needsFullRepaint() && m_flow.cachedLayerClippedOverflowRect()) {
            // Because we waited until we were already inside layout to discover
            // that the block really needed a full layout, we missed our chance to repaint the layer
            // before layout started. Luckily the layer has cached the repaint rect for its original
            // position and size, and so we can use that to make a repaint happen now.
            m_flow.repaintUsingContainer(m_flow.containerForRepaint().renderer.get(), *m_flow.cachedLayerClippedOverflowRect());
        }
    }

    layoutRunsAndFloatsInRange(resolver);
    if (legacyRootBox())
        repaintSelfPaintInlineBoxes(*legacyRootBox());
}

void LegacyLineLayout::layoutRunsAndFloatsInRange(InlineBidiResolver& resolver)
{
    const RenderStyle& styleToUse = style();
    LineWhitespaceCollapsingState& lineWhitespaceCollapsingState = resolver.whitespaceCollapsingState();
    LegacyInlineIterator end = resolver.position();
    RenderTextInfo renderTextInfo;

    LineInfo lineInfo { };
    LineBreaker lineBreaker(m_flow);

    while (!end.atEnd()) {
        lineWhitespaceCollapsingState.reset();

        lineInfo.setEmpty(true);
        lineInfo.resetRunsFromLeadingWhitespace();

        end = lineBreaker.nextLineBreak(resolver, lineInfo, renderTextInfo);
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

        if (!lineInfo.isEmpty()) {
            VisualDirectionOverride override = (styleToUse.rtlOrdering() == Order::Visual ? (styleToUse.direction() == TextDirection::LTR ? VisualLeftToRightOverride : VisualRightToLeftOverride) : NoVisualOverride);

            if (styleToUse.unicodeBidi() == UnicodeBidi::Plaintext && !resolver.context()->parent()) {
                TextDirection direction = styleToUse.direction();
                determineDirectionality(direction, resolver.position());
                resolver.setStatus(BidiStatus(direction, isOverride(styleToUse.unicodeBidi())));
            }
            // FIXME: This ownership is reversed. We should own the BidiRunList and pass it to createBidiRunsForLine.
            BidiRunList<BidiRun>& bidiRuns = resolver.runs();
            constructBidiRunsForSegment(resolver, bidiRuns, end, override);
            ASSERT(resolver.position() == end);

            // Now that the runs have been ordered, we create the line boxes.

            createLineBoxesFromBidiRuns(resolver.status().context->level(), bidiRuns, end, lineInfo);

            bidiRuns.clear();
            resolver.markCurrentRunEmpty(); // FIXME: This can probably be replaced by an ASSERT (or just removed).
        }

        if (!lineInfo.isEmpty())
            lineInfo.setFirstLine(false);

        lineWhitespaceCollapsingState.reset();
        resolver.setPosition(end, numberOfIsolateAncestors(end));
    }
}

void LegacyLineLayout::layoutLineBoxes()
{
    m_flow.setLogicalHeight(0_lu);

    lineBoxes().deleteLineBoxes();

    if (m_flow.firstChild()) {
        // In full layout mode, clear the line boxes of children upfront. Otherwise,
        // siblings can run into stale root lineboxes during layout. Then layout
        // the replaced elements later. In partial layout mode, line boxes are not
        // deleted and only dirtied. In that case, we can layout the replaced
        // elements at the same time.
        bool hasInlineChild = false;
        for (InlineWalker walker(m_flow); !walker.atEnd(); walker.advance()) {
            RenderObject& o = *walker.current();

            if (!hasInlineChild && o.isInline())
                hasInlineChild = true;

            dirtyLineBoxesForRenderer(o);
            o.clearNeedsLayout();
        }

        layoutRunsAndFloats(hasInlineChild);
    }

    if (!legacyRootBox() && m_flow.hasLineIfEmpty())
        m_flow.setLogicalHeight(m_flow.logicalHeight() + m_flow.lineHeight(true, m_flow.isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes));
}

void LegacyLineLayout::addOverflowFromInlineChildren()
{
    if (auto* rootBox = legacyRootBox()) {
        LayoutRect childVisualOverflowRect = rootBox->visualOverflowRect(rootBox->lineTop(), rootBox->lineBottom());
        m_flow.addVisualOverflow(childVisualOverflowRect);
    }
}

size_t LegacyLineLayout::lineCount() const
{
    return legacyRootBox() ? 1 : 0;
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
