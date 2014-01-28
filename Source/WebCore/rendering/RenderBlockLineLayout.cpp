/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All right reserved.
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

#include "AXObjectCache.h"
#include "BidiResolver.h"
#include "BreakingContextInlineHeaders.h"
#include "FloatingObjects.h"
#include "InlineElementBox.h"
#include "InlineIterator.h"
#include "InlineTextBox.h"
#include "LineLayoutState.h"
#include "Logging.h"
#include "RenderBlockFlow.h"
#include "RenderFlowThread.h"
#include "RenderLineBreak.h"
#include "RenderRegion.h"
#include "RenderView.h"
#include "Settings.h"
#include "SimpleLineLayoutFunctions.h"
#include "TrailingFloatsRootInlineBox.h"
#include "VerticalPositionCache.h"
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(SVG)
#include "SVGRootInlineBox.h"
#endif

namespace WebCore {

static void determineDirectionality(TextDirection& dir, InlineIterator iter)
{
    while (!iter.atEnd()) {
        if (iter.atParagraphSeparator())
            return;
        if (UChar current = iter.current()) {
            UCharDirection charDirection = u_charDirection(current);
            if (charDirection == U_LEFT_TO_RIGHT) {
                dir = LTR;
                return;
            }
            if (charDirection == U_RIGHT_TO_LEFT || charDirection == U_RIGHT_TO_LEFT_ARABIC) {
                dir = RTL;
                return;
            }
        }
        iter.increment();
    }
}

inline BidiRun* createRun(int start, int end, RenderObject* obj, InlineBidiResolver& resolver)
{
    ASSERT(obj);
    return new BidiRun(start, end, *obj, resolver.context(), resolver.dir());
}

void RenderBlockFlow::appendRunsForObject(BidiRunList<BidiRun>& runs, int start, int end, RenderObject* obj, InlineBidiResolver& resolver)
{
    if (start > end || shouldSkipCreatingRunsForObject(obj))
        return;

    LineMidpointState& lineMidpointState = resolver.midpointState();
    bool haveNextMidpoint = (lineMidpointState.currentMidpoint() < lineMidpointState.numMidpoints());
    InlineIterator nextMidpoint;
    if (haveNextMidpoint)
        nextMidpoint = lineMidpointState.midpoints()[lineMidpointState.currentMidpoint()];
    if (lineMidpointState.betweenMidpoints()) {
        if (!(haveNextMidpoint && nextMidpoint.renderer() == obj))
            return;
        // This is a new start point. Stop ignoring objects and
        // adjust our start.
        lineMidpointState.setBetweenMidpoints(false);
        start = nextMidpoint.offset();
        lineMidpointState.incrementCurrentMidpoint();
        if (start < end)
            return appendRunsForObject(runs, start, end, obj, resolver);
    } else {
        if (!haveNextMidpoint || (obj != nextMidpoint.renderer())) {
            runs.addRun(createRun(start, end, obj, resolver));
            return;
        }

        // An end midpoint has been encountered within our object.  We
        // need to go ahead and append a run with our endpoint.
        if (static_cast<int>(nextMidpoint.offset() + 1) <= end) {
            lineMidpointState.setBetweenMidpoints(true);
            lineMidpointState.incrementCurrentMidpoint();
            if (nextMidpoint.offset() != UINT_MAX) { // UINT_MAX means stop at the object and don't include any of it.
                if (static_cast<int>(nextMidpoint.offset() + 1) > start)
                    runs.addRun(createRun(start, nextMidpoint.offset() + 1, obj, resolver));
                return appendRunsForObject(runs, nextMidpoint.offset() + 1, end, obj, resolver);
            }
        } else
           runs.addRun(createRun(start, end, obj, resolver));
    }
}

std::unique_ptr<RootInlineBox> RenderBlockFlow::createRootInlineBox()
{
    return std::make_unique<RootInlineBox>(*this);
}

RootInlineBox* RenderBlockFlow::createAndAppendRootInlineBox()
{
    auto newRootBox = createRootInlineBox();
    RootInlineBox* rootBox = newRootBox.get();
    m_lineBoxes.appendLineBox(std::move(newRootBox));

    if (UNLIKELY(AXObjectCache::accessibilityEnabled()) && firstRootBox() == rootBox) {
        if (AXObjectCache* cache = document().existingAXObjectCache())
            cache->recomputeIsIgnored(this);
    }

    return rootBox;
}

static inline InlineBox* createInlineBoxForRenderer(RenderObject* obj, bool isRootLineBox, bool isOnlyRun = false)
{
    if (isRootLineBox)
        return toRenderBlockFlow(obj)->createAndAppendRootInlineBox();

    if (obj->isText())
        return toRenderText(obj)->createInlineTextBox();

    if (obj->isBox()) {
        // FIXME: This is terrible. This branch returns an *owned* pointer!
        return toRenderBox(obj)->createInlineBox().release();
    }

    if (obj->isLineBreak()) {
        // FIXME: This is terrible. This branch returns an *owned* pointer!
        auto inlineBox = toRenderLineBreak(obj)->createInlineBox().release();
        // We only treat a box as text for a <br> if we are on a line by ourself or in strict mode
        // (Note the use of strict mode. In "almost strict" mode, we don't treat the box for <br> as text.)
        inlineBox->setBehavesLikeText(isOnlyRun || obj->document().inNoQuirksMode() || obj->isLineBreakOpportunity());
        return inlineBox;
    }

    return toRenderInline(obj)->createAndAppendInlineFlowBox();
}

static inline void dirtyLineBoxesForRenderer(RenderObject& renderer, bool fullLayout)
{
    if (renderer.isText()) {
        RenderText& renderText = toRenderText(renderer);
        updateCounterIfNeeded(renderText);
        renderText.dirtyLineBoxes(fullLayout);
    } else if (renderer.isLineBreak())
        toRenderLineBreak(renderer).dirtyLineBoxes(fullLayout);
    else
        toRenderInline(renderer).dirtyLineBoxes(fullLayout);
}

static bool parentIsConstructedOrHaveNext(InlineFlowBox* parentBox)
{
    do {
        if (parentBox->isConstructed() || parentBox->nextOnLine())
            return true;
        parentBox = parentBox->parent();
    } while (parentBox);
    return false;
}

InlineFlowBox* RenderBlockFlow::createLineBoxes(RenderObject* obj, const LineInfo& lineInfo, InlineBox* childBox, bool startNewSegment)
{
    // See if we have an unconstructed line box for this object that is also
    // the last item on the line.
    unsigned lineDepth = 1;
    InlineFlowBox* parentBox = 0;
    InlineFlowBox* result = 0;
    bool hasDefaultLineBoxContain = style().lineBoxContain() == RenderStyle::initialLineBoxContain();
    do {
        ASSERT_WITH_SECURITY_IMPLICATION(obj->isRenderInline() || obj == this);

        RenderInline* inlineFlow = (obj != this) ? toRenderInline(obj) : 0;

        // Get the last box we made for this render object.
        parentBox = inlineFlow ? inlineFlow->lastLineBox() : toRenderBlockFlow(obj)->lastRootBox();

        // If this box or its ancestor is constructed then it is from a previous line, and we need
        // to make a new box for our line.  If this box or its ancestor is unconstructed but it has
        // something following it on the line, then we know we have to make a new box
        // as well.  In this situation our inline has actually been split in two on
        // the same line (this can happen with very fancy language mixtures).
        bool constructedNewBox = false;
        bool allowedToConstructNewBox = !hasDefaultLineBoxContain || !inlineFlow || inlineFlow->alwaysCreateLineBoxes();
        bool mustCreateBoxesToRoot = startNewSegment && !(parentBox && parentBox->isRootInlineBox());
        bool canUseExistingParentBox = parentBox && !parentIsConstructedOrHaveNext(parentBox) && !mustCreateBoxesToRoot;
        if (allowedToConstructNewBox && !canUseExistingParentBox) {
            // We need to make a new box for this render object.  Once
            // made, we need to place it at the end of the current line.
            InlineBox* newBox = createInlineBoxForRenderer(obj, obj == this);
            ASSERT_WITH_SECURITY_IMPLICATION(newBox->isInlineFlowBox());
            parentBox = toInlineFlowBox(newBox);
            parentBox->setIsFirstLine(lineInfo.isFirstLine());
            parentBox->setIsHorizontal(isHorizontalWritingMode());
            if (!hasDefaultLineBoxContain)
                parentBox->clearDescendantsHaveSameLineHeightAndBaseline();
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

            if (!constructedNewBox || obj == this)
                break;

            childBox = parentBox;
        }

        // If we've exceeded our line depth, then jump straight to the root and skip all the remaining
        // intermediate inline flows.
        obj = (++lineDepth >= cMaxLineDepth) ? this : obj->parent();

    } while (true);

    return result;
}

template <typename CharacterType>
static inline bool endsWithASCIISpaces(const CharacterType* characters, unsigned pos, unsigned end)
{
    while (isASCIISpace(characters[pos])) {
        pos++;
        if (pos >= end)
            return true;
    }
    return false;
}

static bool reachedEndOfTextRenderer(const BidiRunList<BidiRun>& bidiRuns)
{
    BidiRun* run = bidiRuns.logicallyLastRun();
    if (!run)
        return true;
    unsigned pos = run->stop();
    const RenderObject& r = run->renderer();
    if (!r.isText())
        return false;
    const RenderText& renderText = toRenderText(r);
    unsigned length = renderText.textLength();
    if (pos >= length)
        return true;

    if (renderText.is8Bit())
        return endsWithASCIISpaces(renderText.characters8(), pos, length);
    return endsWithASCIISpaces(renderText.characters16(), pos, length);
}

RootInlineBox* RenderBlockFlow::constructLine(BidiRunList<BidiRun>& bidiRuns, const LineInfo& lineInfo)
{
    ASSERT(bidiRuns.firstRun());

    bool rootHasSelectedChildren = false;
    InlineFlowBox* parentBox = 0;
    int runCount = bidiRuns.runCount() - lineInfo.runsFromLeadingWhitespace();
    for (BidiRun* r = bidiRuns.firstRun(); r; r = r->next()) {
        // Create a box for our object.
        bool isOnlyRun = (runCount == 1);
        if (runCount == 2 && !r->renderer().isListMarker())
            isOnlyRun = (!style().isLeftToRightDirection() ? bidiRuns.lastRun() : bidiRuns.firstRun())->renderer().isListMarker();

        if (lineInfo.isEmpty())
            continue;

        InlineBox* box = createInlineBoxForRenderer(&r->renderer(), false, isOnlyRun);
        r->setBox(*box);

        if (!rootHasSelectedChildren && box->renderer().selectionState() != RenderObject::SelectionNone)
            rootHasSelectedChildren = true;

        // If we have no parent box yet, or if the run is not simply a sibling,
        // then we need to construct inline boxes as necessary to properly enclose the
        // run's inline box. Segments can only be siblings at the root level, as
        // they are positioned separately.
#if ENABLE(CSS_SHAPES)
        bool runStartsSegment = r->m_startsSegment;
#else
        bool runStartsSegment = false;
#endif
        if (!parentBox || &parentBox->renderer() != r->renderer().parent() || runStartsSegment)
            // Create new inline boxes all the way back to the appropriate insertion point.
            parentBox = createLineBoxes(r->renderer().parent(), lineInfo, box, runStartsSegment);
        else {
            // Append the inline box to this line.
            parentBox->addToLine(box);
        }

        bool visuallyOrdered = r->renderer().style().rtlOrdering() == VisualOrder;
        box->setBidiLevel(r->level());

        if (box->isInlineTextBox()) {
            InlineTextBox* text = toInlineTextBox(box);
            text->setStart(r->m_start);
            text->setLen(r->m_stop - r->m_start);
            text->setDirOverride(r->dirOverride(visuallyOrdered));
            if (r->m_hasHyphen)
                text->setHasHyphen(true);
        }
    }

    // We should have a root inline box.  It should be unconstructed and
    // be the last continuation of our line list.
    ASSERT(lastRootBox() && !lastRootBox()->isConstructed());

    // Set the m_selectedChildren flag on the root inline box if one of the leaf inline box
    // from the bidi runs walk above has a selection state.
    if (rootHasSelectedChildren)
        lastRootBox()->root().setHasSelectedChildren(true);

    // Set bits on our inline flow boxes that indicate which sides should
    // paint borders/margins/padding.  This knowledge will ultimately be used when
    // we determine the horizontal positions and widths of all the inline boxes on
    // the line.
    bool isLogicallyLastRunWrapped = bidiRuns.logicallyLastRun()->renderer().isText() ? !reachedEndOfTextRenderer(bidiRuns) : true;
    lastRootBox()->determineSpacingForFlowBoxes(lineInfo.isLastLine(), isLogicallyLastRunWrapped, &bidiRuns.logicallyLastRun()->renderer());

    // Now mark the line boxes as being constructed.
    lastRootBox()->setConstructed();

    // Return the last line.
    return lastRootBox();
}

ETextAlign RenderBlockFlow::textAlignmentForLine(bool endsWithSoftBreak) const
{
    ETextAlign alignment = style().textAlign();
    if (endsWithSoftBreak)
        return alignment;

#if !ENABLE(CSS3_TEXT)
    return (alignment == JUSTIFY) ? TASTART : alignment;
#else
    if (alignment != JUSTIFY)
        return alignment;

    TextAlignLast alignmentLast = style().textAlignLast();
    switch (alignmentLast) {
    case TextAlignLastStart:
        return TASTART;
    case TextAlignLastEnd:
        return TAEND;
    case TextAlignLastLeft:
        return LEFT;
    case TextAlignLastRight:
        return RIGHT;
    case TextAlignLastCenter:
        return CENTER;
    case TextAlignLastJustify:
        return JUSTIFY;
    case TextAlignLastAuto:
        if (style().textJustify() == TextJustifyDistribute)
            return JUSTIFY;
        return TASTART;
    }
    return alignment;
#endif
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
        if (totalLogicalWidth < availableLogicalWidth)
            logicalLeft += availableLogicalWidth - totalLogicalWidth;
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

void RenderBlockFlow::setMarginsForRubyRun(BidiRun* run, RenderRubyRun& renderer, RenderObject* previousObject, const LineInfo& lineInfo)
{
    int startOverhang;
    int endOverhang;
    RenderObject* nextObject = 0;
    for (BidiRun* runWithNextObject = run->next(); runWithNextObject; runWithNextObject = runWithNextObject->next()) {
        if (!runWithNextObject->renderer().isOutOfFlowPositioned() && !runWithNextObject->box()->isLineBreak()) {
            nextObject = &runWithNextObject->renderer();
            break;
        }
    }
    renderer.getOverhang(lineInfo.isFirstLine(), renderer.style().isLeftToRightDirection() ? previousObject : nextObject, renderer.style().isLeftToRightDirection() ? nextObject : previousObject, startOverhang, endOverhang);
    setMarginStartForChild(renderer, -startOverhang);
    setMarginEndForChild(renderer, -endOverhang);
}

static inline void setLogicalWidthForTextRun(RootInlineBox* lineBox, BidiRun* run, RenderText* renderer, float xPos, const LineInfo& lineInfo,
    GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache& verticalPositionCache, WordMeasurements& wordMeasurements)
{
    HashSet<const SimpleFontData*> fallbackFonts;
    GlyphOverflow glyphOverflow;

    const Font& font = lineStyle(*renderer->parent(), lineInfo).font();
    // Always compute glyph overflow if the block's line-box-contain value is "glyphs".
    if (lineBox->fitsToGlyphs()) {
        // If we don't stick out of the root line's font box, then don't bother computing our glyph overflow. This optimization
        // will keep us from computing glyph bounds in nearly all cases.
        bool includeRootLine = lineBox->includesRootLineBoxFontOrLeading();
        int baselineShift = lineBox->verticalPositionForBox(run->box(), verticalPositionCache);
        int rootDescent = includeRootLine ? font.fontMetrics().descent() : 0;
        int rootAscent = includeRootLine ? font.fontMetrics().ascent() : 0;
        int boxAscent = font.fontMetrics().ascent() - baselineShift;
        int boxDescent = font.fontMetrics().descent() + baselineShift;
        if (boxAscent > rootDescent ||  boxDescent > rootAscent)
            glyphOverflow.computeBounds = true; 
    }
    
    LayoutUnit hyphenWidth = 0;
    if (toInlineTextBox(run->box())->hasHyphen())
        hyphenWidth = measureHyphenWidth(renderer, font, &fallbackFonts);

    float measuredWidth = 0;

    bool kerningIsEnabled = font.typesettingFeatures() & Kerning;
    bool canUseSimpleFontCodePath = renderer->canUseSimpleFontCodePath();
    
    // Since we don't cache glyph overflows, we need to re-measure the run if
    // the style is linebox-contain: glyph.
    
    if (!lineBox->fitsToGlyphs() && canUseSimpleFontCodePath) {
        int lastEndOffset = run->m_start;
        for (size_t i = 0, size = wordMeasurements.size(); i < size && lastEndOffset < run->m_stop; ++i) {
            WordMeasurement& wordMeasurement = wordMeasurements[i];
            if (wordMeasurement.width <= 0 || wordMeasurement.startOffset == wordMeasurement.endOffset)
                continue;
            if (wordMeasurement.renderer != renderer || wordMeasurement.startOffset != lastEndOffset || wordMeasurement.endOffset > run->m_stop)
                continue;

            lastEndOffset = wordMeasurement.endOffset;
            if (kerningIsEnabled && lastEndOffset == run->m_stop) {
                int wordLength = lastEndOffset - wordMeasurement.startOffset;
                GlyphOverflow overflow;
                measuredWidth += renderer->width(wordMeasurement.startOffset, wordLength, xPos + measuredWidth, lineInfo.isFirstLine(),
                    &wordMeasurement.fallbackFonts, &overflow);
                UChar c = renderer->characterAt(wordMeasurement.startOffset);
                if (i > 0 && wordLength == 1 && (c == ' ' || c == '\t'))
                    measuredWidth += renderer->style().font().wordSpacing();
            } else
                measuredWidth += wordMeasurement.width;
            if (!wordMeasurement.fallbackFonts.isEmpty()) {
                HashSet<const SimpleFontData*>::const_iterator end = wordMeasurement.fallbackFonts.end();
                for (HashSet<const SimpleFontData*>::const_iterator it = wordMeasurement.fallbackFonts.begin(); it != end; ++it)
                    fallbackFonts.add(*it);
            }
        }
        if (measuredWidth && lastEndOffset != run->m_stop) {
            // If we don't have enough cached data, we'll measure the run again.
            measuredWidth = 0;
            fallbackFonts.clear();
        }
    }

    if (!measuredWidth)
        measuredWidth = renderer->width(run->m_start, run->m_stop - run->m_start, xPos, lineInfo.isFirstLine(), &fallbackFonts, &glyphOverflow);

    run->box()->setLogicalWidth(measuredWidth + hyphenWidth);
    if (!fallbackFonts.isEmpty()) {
        ASSERT(run->box()->behavesLikeText());
        GlyphOverflowAndFallbackFontsMap::iterator it = textBoxDataMap.add(toInlineTextBox(run->box()), std::make_pair(Vector<const SimpleFontData*>(), GlyphOverflow())).iterator;
        ASSERT(it->value.first.isEmpty());
        copyToVector(fallbackFonts, it->value.first);
        run->box()->parent()->clearDescendantsHaveSameLineHeightAndBaseline();
    }
    if ((glyphOverflow.top || glyphOverflow.bottom || glyphOverflow.left || glyphOverflow.right)) {
        ASSERT(run->box()->behavesLikeText());
        GlyphOverflowAndFallbackFontsMap::iterator it = textBoxDataMap.add(toInlineTextBox(run->box()), std::make_pair(Vector<const SimpleFontData*>(), GlyphOverflow())).iterator;
        it->value.second = glyphOverflow;
        run->box()->clearKnownToHaveNoOverflow();
    }
}

static inline void computeExpansionForJustifiedText(BidiRun* firstRun, BidiRun* trailingSpaceRun, Vector<unsigned, 16>& expansionOpportunities, unsigned expansionOpportunityCount, float& totalLogicalWidth, float availableLogicalWidth)
{
    if (!expansionOpportunityCount || availableLogicalWidth <= totalLogicalWidth)
        return;

    size_t i = 0;
    for (BidiRun* r = firstRun; r; r = r->next()) {
#if ENABLE(CSS_SHAPES)
        // This method is called once per segment, do not move past the current segment.
        if (r->m_startsSegment)
            break;
#endif
        if (!r->box() || r == trailingSpaceRun)
            continue;
        
        if (r->renderer().isText()) {
            unsigned opportunitiesInRun = expansionOpportunities[i++];
            
            ASSERT(opportunitiesInRun <= expansionOpportunityCount);
            
            // Only justify text if whitespace is collapsed.
            if (r->renderer().style().collapseWhiteSpace()) {
                InlineTextBox* textBox = toInlineTextBox(r->box());
                int expansion = (availableLogicalWidth - totalLogicalWidth) * opportunitiesInRun / expansionOpportunityCount;
                textBox->setExpansion(expansion);
                totalLogicalWidth += expansion;
            }
            expansionOpportunityCount -= opportunitiesInRun;
            if (!expansionOpportunityCount)
                break;
        }
    }
}

void RenderBlockFlow::updateLogicalWidthForAlignment(const ETextAlign& textAlign, BidiRun* trailingSpaceRun, float& logicalLeft, float& totalLogicalWidth, float& availableLogicalWidth, int expansionOpportunityCount)
{
    // Armed with the total width of the line (without justification),
    // we now examine our text-align property in order to determine where to position the
    // objects horizontally. The total width of the line can be increased if we end up
    // justifying text.
    switch (textAlign) {
    case LEFT:
    case WEBKIT_LEFT:
        updateLogicalWidthForLeftAlignedBlock(style().isLeftToRightDirection(), trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        break;
    case RIGHT:
    case WEBKIT_RIGHT:
        updateLogicalWidthForRightAlignedBlock(style().isLeftToRightDirection(), trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        break;
    case CENTER:
    case WEBKIT_CENTER:
        updateLogicalWidthForCenterAlignedBlock(style().isLeftToRightDirection(), trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        break;
    case JUSTIFY:
        adjustInlineDirectionLineBounds(expansionOpportunityCount, logicalLeft, availableLogicalWidth);
        if (expansionOpportunityCount) {
            if (trailingSpaceRun) {
                totalLogicalWidth -= trailingSpaceRun->box()->logicalWidth();
                trailingSpaceRun->box()->setLogicalWidth(0);
            }
            break;
        }
        FALLTHROUGH;
    case TASTART:
        if (style().isLeftToRightDirection())
            updateLogicalWidthForLeftAlignedBlock(style().isLeftToRightDirection(), trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        else
            updateLogicalWidthForRightAlignedBlock(style().isLeftToRightDirection(), trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        break;
    case TAEND:
        if (style().isLeftToRightDirection())
            updateLogicalWidthForRightAlignedBlock(style().isLeftToRightDirection(), trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        else
            updateLogicalWidthForLeftAlignedBlock(style().isLeftToRightDirection(), trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth);
        break;
    }
}

static void updateLogicalInlinePositions(RenderBlockFlow& block, float& lineLogicalLeft, float& lineLogicalRight, float& availableLogicalWidth, bool firstLine, IndentTextOrNot shouldIndentText, LayoutUnit boxLogicalHeight)
{
    LayoutUnit lineLogicalHeight = block.minLineHeightForReplacedRenderer(firstLine, boxLogicalHeight);
    lineLogicalLeft = block.pixelSnappedLogicalLeftOffsetForLine(block.logicalHeight(), shouldIndentText == IndentText, lineLogicalHeight);
    lineLogicalRight = block.pixelSnappedLogicalRightOffsetForLine(block.logicalHeight(), shouldIndentText == IndentText, lineLogicalHeight);
    availableLogicalWidth = lineLogicalRight - lineLogicalLeft;
}

void RenderBlockFlow::computeInlineDirectionPositionsForLine(RootInlineBox* lineBox, const LineInfo& lineInfo, BidiRun* firstRun, BidiRun* trailingSpaceRun, bool reachedEnd, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache& verticalPositionCache, WordMeasurements& wordMeasurements)
{
    ETextAlign textAlign = textAlignmentForLine(!reachedEnd && !lineBox->endsWithBreak());
    
    // CSS 2.1: "'Text-indent' only affects a line if it is the first formatted line of an element. For example, the first line of an anonymous block 
    // box is only affected if it is the first child of its parent element."
    // CSS3 "text-indent", "-webkit-each-line" affects the first line of the block container as well as each line after a forced line break,
    // but does not affect lines after a soft wrap break.
    bool isFirstLine = lineInfo.isFirstLine() && !(isAnonymousBlock() && parent()->firstChild() != this);
    bool isAfterHardLineBreak = lineBox->prevRootBox() && lineBox->prevRootBox()->endsWithBreak();
    IndentTextOrNot shouldIndentText = requiresIndent(isFirstLine, isAfterHardLineBreak, style());
    float lineLogicalLeft;
    float lineLogicalRight;
    float availableLogicalWidth;
    updateLogicalInlinePositions(*this, lineLogicalLeft, lineLogicalRight, availableLogicalWidth, isFirstLine, shouldIndentText, 0);
    bool needsWordSpacing;
#if ENABLE(CSS_SHAPES)
    ShapeInsideInfo* shapeInsideInfo = layoutShapeInsideInfo();
    if (shapeInsideInfo && shapeInsideInfo->hasSegments()) {
        BidiRun* segmentStart = firstRun;
        const SegmentList& segments = shapeInsideInfo->segments();
        float logicalLeft = std::max<float>(roundToInt(segments[0].logicalLeft), lineLogicalLeft);
        float logicalRight = std::min<float>(floorToInt(segments[0].logicalRight), lineLogicalRight);
        float startLogicalLeft = logicalLeft;
        float endLogicalRight = logicalLeft;
        float minLogicalLeft = logicalLeft;
        float maxLogicalRight = logicalLeft;
        lineBox->beginPlacingBoxRangesInInlineDirection(logicalLeft);
        for (size_t i = 0; i < segments.size(); i++) {
            if (i) {
                logicalLeft = std::max<float>(roundToInt(segments[i].logicalLeft), lineLogicalLeft);
                logicalRight = std::min<float>(floorToInt(segments[i].logicalRight), lineLogicalRight);
            }
            availableLogicalWidth = logicalRight - logicalLeft;
            BidiRun* newSegmentStart = computeInlineDirectionPositionsForSegment(lineBox, lineInfo, textAlign, logicalLeft, availableLogicalWidth, segmentStart, trailingSpaceRun, textBoxDataMap, verticalPositionCache, wordMeasurements);
            needsWordSpacing = false;
            endLogicalRight = lineBox->placeBoxRangeInInlineDirection(segmentStart->box(), newSegmentStart ? newSegmentStart->box() : 0, logicalLeft, minLogicalLeft, maxLogicalRight, needsWordSpacing, textBoxDataMap);
            if (!newSegmentStart || !newSegmentStart->next())
                break;
            ASSERT(newSegmentStart->m_startsSegment);
            // Discard the empty segment start marker bidi runs
            segmentStart = newSegmentStart->next();
        }
        lineBox->endPlacingBoxRangesInInlineDirection(startLogicalLeft, endLogicalRight, minLogicalLeft, maxLogicalRight);
        return;
    }
#endif

    if (firstRun && firstRun->renderer().isReplaced()) {
        RenderBox& renderBox = toRenderBox(firstRun->renderer());
        updateLogicalInlinePositions(*this, lineLogicalLeft, lineLogicalRight, availableLogicalWidth, isFirstLine, shouldIndentText, renderBox.logicalHeight());
    }

    computeInlineDirectionPositionsForSegment(lineBox, lineInfo, textAlign, lineLogicalLeft, availableLogicalWidth, firstRun, trailingSpaceRun, textBoxDataMap, verticalPositionCache, wordMeasurements);
    // The widths of all runs are now known. We can now place every inline box (and
    // compute accurate widths for the inline flow boxes).
    needsWordSpacing = false;
    lineBox->placeBoxesInInlineDirection(lineLogicalLeft, needsWordSpacing, textBoxDataMap);
}

BidiRun* RenderBlockFlow::computeInlineDirectionPositionsForSegment(RootInlineBox* lineBox, const LineInfo& lineInfo, ETextAlign textAlign, float& logicalLeft, 
    float& availableLogicalWidth, BidiRun* firstRun, BidiRun* trailingSpaceRun, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache& verticalPositionCache,
    WordMeasurements& wordMeasurements)
{
    bool needsWordSpacing = false;
    float totalLogicalWidth = lineBox->getFlowSpacingLogicalWidth();
    unsigned expansionOpportunityCount = 0;
    bool isAfterExpansion = true;
    Vector<unsigned, 16> expansionOpportunities;
    RenderObject* previousObject = 0;

    BidiRun* r = firstRun;
    for (; r; r = r->next()) {
#if ENABLE(CSS_SHAPES)
        // Once we have reached the start of the next segment, we have finished
        // computing the positions for this segment's contents.
        if (r->m_startsSegment)
            break;
#endif
        if (!r->box() || r->renderer().isOutOfFlowPositioned() || r->box()->isLineBreak())
            continue; // Positioned objects are only participating to figure out their
                      // correct static x position.  They have no effect on the width.
                      // Similarly, line break boxes have no effect on the width.
        if (r->renderer().isText()) {
            RenderText& rt = toRenderText(r->renderer());
            if (textAlign == JUSTIFY && r != trailingSpaceRun) {
                if (!isAfterExpansion)
                    toInlineTextBox(r->box())->setCanHaveLeadingExpansion(true);
                unsigned opportunitiesInRun;
                if (rt.is8Bit())
                    opportunitiesInRun = Font::expansionOpportunityCount(rt.characters8() + r->m_start, r->m_stop - r->m_start, r->box()->direction(), isAfterExpansion);
                else
                    opportunitiesInRun = Font::expansionOpportunityCount(rt.characters16() + r->m_start, r->m_stop - r->m_start, r->box()->direction(), isAfterExpansion);
                expansionOpportunities.append(opportunitiesInRun);
                expansionOpportunityCount += opportunitiesInRun;
            }

            if (int length = rt.textLength()) {
                if (!r->m_start && needsWordSpacing && isSpaceOrNewline(rt.characterAt(r->m_start)))
                    totalLogicalWidth += lineStyle(*rt.parent(), lineInfo).font().wordSpacing();
                needsWordSpacing = !isSpaceOrNewline(rt.characterAt(r->m_stop - 1)) && r->m_stop == length;
            }

            setLogicalWidthForTextRun(lineBox, r, &rt, totalLogicalWidth, lineInfo, textBoxDataMap, verticalPositionCache, wordMeasurements);
        } else {
            isAfterExpansion = false;
            if (!r->renderer().isRenderInline()) {
                RenderBox& renderBox = toRenderBox(r->renderer());
                if (renderBox.isRubyRun())
                    setMarginsForRubyRun(r, toRenderRubyRun(renderBox), previousObject, lineInfo);
                r->box()->setLogicalWidth(logicalWidthForChild(renderBox));
                totalLogicalWidth += marginStartForChild(renderBox) + marginEndForChild(renderBox);
            }
        }

        totalLogicalWidth += r->box()->logicalWidth();
        previousObject = &r->renderer();
    }

    if (isAfterExpansion && !expansionOpportunities.isEmpty()) {
        expansionOpportunities.last()--;
        expansionOpportunityCount--;
    }

    updateLogicalWidthForAlignment(textAlign, trailingSpaceRun, logicalLeft, totalLogicalWidth, availableLogicalWidth, expansionOpportunityCount);

    computeExpansionForJustifiedText(firstRun, trailingSpaceRun, expansionOpportunities, expansionOpportunityCount, totalLogicalWidth, availableLogicalWidth);

    return r;
}

void RenderBlockFlow::computeBlockDirectionPositionsForLine(RootInlineBox* lineBox, BidiRun* firstRun, GlyphOverflowAndFallbackFontsMap& textBoxDataMap,
                                                        VerticalPositionCache& verticalPositionCache)
{
    setLogicalHeight(lineBox->alignBoxesInBlockDirection(logicalHeight(), textBoxDataMap, verticalPositionCache));

    // Now make sure we place replaced render objects correctly.
    for (BidiRun* r = firstRun; r; r = r->next()) {
        ASSERT(r->box());
        if (!r->box())
            continue; // Skip runs with no line boxes.

        InlineBox& box = *r->box();

        // Align positioned boxes with the top of the line box.  This is
        // a reasonable approximation of an appropriate y position.
        if (r->renderer().isOutOfFlowPositioned())
            box.setLogicalTop(logicalHeight());

        // Position is used to properly position both replaced elements and
        // to update the static normal flow x/y of positioned elements.
        if (r->renderer().isText())
            toRenderText(r->renderer()).positionLineBox(toInlineTextBox(box));
        else if (r->renderer().isBox())
            toRenderBox(r->renderer()).positionLineBox(toInlineElementBox(box));
        else if (r->renderer().isLineBreak())
            toRenderLineBreak(r->renderer()).replaceInlineBoxWrapper(toInlineElementBox(box));
    }
    // Positioned objects and zero-length text nodes destroy their boxes in
    // position(), which unnecessarily dirties the line.
    lineBox->markDirty(false);
}

static inline bool isCollapsibleSpace(UChar character, const RenderText& renderer)
{
    if (character == ' ' || character == '\t' || character == softHyphen)
        return true;
    if (character == '\n')
        return !renderer.style().preserveNewline();
    if (character == noBreakSpace)
        return renderer.style().nbspMode() == SPACE;
    return false;
}

template <typename CharacterType>
static inline int findFirstTrailingSpace(const RenderText& lastText, const CharacterType* characters, int start, int stop)
{
    int firstSpace = stop;
    while (firstSpace > start) {
        UChar current = characters[firstSpace - 1];
        if (!isCollapsibleSpace(current, lastText))
            break;
        firstSpace--;
    }

    return firstSpace;
}

inline BidiRun* RenderBlockFlow::handleTrailingSpaces(BidiRunList<BidiRun>& bidiRuns, BidiContext* currentContext)
{
    if (!bidiRuns.runCount()
        || !bidiRuns.logicallyLastRun()->renderer().style().breakOnlyAfterWhiteSpace()
        || !bidiRuns.logicallyLastRun()->renderer().style().autoWrap())
        return 0;

    BidiRun* trailingSpaceRun = bidiRuns.logicallyLastRun();
    const RenderObject& lastObject = trailingSpaceRun->renderer();
    if (!lastObject.isText())
        return 0;

    const RenderText& lastText = toRenderText(lastObject);
    int firstSpace;
    if (lastText.is8Bit())
        firstSpace = findFirstTrailingSpace(lastText, lastText.characters8(), trailingSpaceRun->start(), trailingSpaceRun->stop());
    else
        firstSpace = findFirstTrailingSpace(lastText, lastText.characters16(), trailingSpaceRun->start(), trailingSpaceRun->stop());

    if (firstSpace == trailingSpaceRun->stop())
        return 0;

    TextDirection direction = style().direction();
    bool shouldReorder = trailingSpaceRun != (direction == LTR ? bidiRuns.lastRun() : bidiRuns.firstRun());
    if (firstSpace != trailingSpaceRun->start()) {
        BidiContext* baseContext = currentContext;
        while (BidiContext* parent = baseContext->parent())
            baseContext = parent;

        BidiRun* newTrailingRun = new BidiRun(firstSpace, trailingSpaceRun->m_stop, trailingSpaceRun->renderer(), baseContext, U_OTHER_NEUTRAL);
        trailingSpaceRun->m_stop = firstSpace;
        if (direction == LTR)
            bidiRuns.addRun(newTrailingRun);
        else
            bidiRuns.prependRun(newTrailingRun);
        trailingSpaceRun = newTrailingRun;
        return trailingSpaceRun;
    }
    if (!shouldReorder)
        return trailingSpaceRun;

    if (direction == LTR) {
        bidiRuns.moveRunToEnd(trailingSpaceRun);
        trailingSpaceRun->m_level = 0;
    } else {
        bidiRuns.moveRunToBeginning(trailingSpaceRun);
        trailingSpaceRun->m_level = 1;
    }
    return trailingSpaceRun;
}

void RenderBlockFlow::appendFloatingObjectToLastLine(FloatingObject* floatingObject)
{
    ASSERT(!floatingObject->originatingLine());
    floatingObject->setOriginatingLine(lastRootBox());
    lastRootBox()->appendFloat(floatingObject->renderer());
}

static inline void setUpResolverToResumeInIsolate(InlineBidiResolver& resolver, RenderObject* root, RenderObject* startObject)
{
    if (root != startObject) {
        RenderObject* parent = startObject->parent();
        setUpResolverToResumeInIsolate(resolver, root, parent);
        notifyObserverEnteredObject(&resolver, startObject);
    }
}

// FIXME: BidiResolver should have this logic.
static inline void constructBidiRunsForSegment(InlineBidiResolver& topResolver, BidiRunList<BidiRun>& bidiRuns, const InlineIterator& endOfRuns, VisualDirectionOverride override, bool previousLineBrokeCleanly)
{
    // FIXME: We should pass a BidiRunList into createBidiRunsForLine instead
    // of the resolver owning the runs.
    ASSERT(&topResolver.runs() == &bidiRuns);
    ASSERT(topResolver.position() != endOfRuns);
    RenderObject* currentRoot = topResolver.position().root();
    topResolver.createBidiRunsForLine(endOfRuns, override, previousLineBrokeCleanly);

    while (!topResolver.isolatedRuns().isEmpty()) {
        // It does not matter which order we resolve the runs as long as we resolve them all.
        BidiRun* isolatedRun = topResolver.isolatedRuns().last();
        topResolver.isolatedRuns().removeLast();

        RenderObject& startObj = isolatedRun->renderer();

        // Only inlines make sense with unicode-bidi: isolate (blocks are already isolated).
        // FIXME: Because enterIsolate is not passed a RenderObject, we have to crawl up the
        // tree to see which parent inline is the isolate. We could change enterIsolate
        // to take a RenderObject and do this logic there, but that would be a layering
        // violation for BidiResolver (which knows nothing about RenderObject).
        RenderInline* isolatedInline = toRenderInline(containingIsolate(&startObj, currentRoot));
        InlineBidiResolver isolatedResolver;
        EUnicodeBidi unicodeBidi = isolatedInline->style().unicodeBidi();
        TextDirection direction;
        if (unicodeBidi == Plaintext)
            determineDirectionality(direction, InlineIterator(isolatedInline, &isolatedRun->renderer(), 0));
        else {
            ASSERT(unicodeBidi == Isolate || unicodeBidi == IsolateOverride);
            direction = isolatedInline->style().direction();
        }
        isolatedResolver.setStatus(BidiStatus(direction, isOverride(unicodeBidi)));

        setUpResolverToResumeInIsolate(isolatedResolver, isolatedInline, &startObj);

        // The starting position is the beginning of the first run within the isolate that was identified
        // during the earlier call to createBidiRunsForLine. This can be but is not necessarily the
        // first run within the isolate.
        InlineIterator iter = InlineIterator(isolatedInline, &startObj, isolatedRun->m_start);
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
            bidiRuns.replaceRunWithRuns(isolatedRun, isolatedResolver.runs());

        // If we encountered any nested isolate runs, just move them
        // to the top resolver's list for later processing.
        if (!isolatedResolver.isolatedRuns().isEmpty()) {
            topResolver.isolatedRuns().appendVector(isolatedResolver.isolatedRuns());
            isolatedResolver.isolatedRuns().clear();
            currentRoot = isolatedInline;
        }
    }
}

static inline void constructBidiRunsForLine(const RenderBlockFlow* block, InlineBidiResolver& topResolver, BidiRunList<BidiRun>& bidiRuns, const InlineIterator& endOfLine, VisualDirectionOverride override, bool previousLineBrokeCleanly)
{
#if !ENABLE(CSS_SHAPES)
    UNUSED_PARAM(block);
    constructBidiRunsForSegment(topResolver, bidiRuns, endOfLine, override, previousLineBrokeCleanly);
#else
    ShapeInsideInfo* shapeInsideInfo = block->layoutShapeInsideInfo();
    if (!shapeInsideInfo || !shapeInsideInfo->hasSegments()) {
        constructBidiRunsForSegment(topResolver, bidiRuns, endOfLine, override, previousLineBrokeCleanly);
        return;
    }

    const SegmentRangeList& segmentRanges = shapeInsideInfo->segmentRanges();
    ASSERT(segmentRanges.size());

    for (size_t i = 0; i < segmentRanges.size(); i++) {
        LineSegmentIterator iterator = segmentRanges[i].start;
        InlineIterator segmentStart(iterator.root, iterator.object, iterator.offset);
        iterator = segmentRanges[i].end;
        InlineIterator segmentEnd(iterator.root, iterator.object, iterator.offset);
        if (i) {
            ASSERT(segmentStart.renderer());
            BidiRun* segmentMarker = createRun(segmentStart.offset(), segmentStart.offset(), segmentStart.renderer(), topResolver);
            segmentMarker->m_startsSegment = true;
            bidiRuns.addRun(segmentMarker);
            // Do not collapse midpoints between segments
            topResolver.midpointState().setBetweenMidpoints(false);
        }
        if (segmentStart == segmentEnd)
            continue;
        topResolver.setPosition(segmentStart, numberOfIsolateAncestors(segmentStart));
        constructBidiRunsForSegment(topResolver, bidiRuns, segmentEnd, override, previousLineBrokeCleanly);
    }
#endif
}

// This function constructs line boxes for all of the text runs in the resolver and computes their position.
RootInlineBox* RenderBlockFlow::createLineBoxesFromBidiRuns(BidiRunList<BidiRun>& bidiRuns, const InlineIterator& end, LineInfo& lineInfo, VerticalPositionCache& verticalPositionCache, BidiRun* trailingSpaceRun, WordMeasurements& wordMeasurements)
{
    if (!bidiRuns.runCount())
        return 0;

    // FIXME: Why is this only done when we had runs?
    lineInfo.setLastLine(!end.renderer());

    RootInlineBox* lineBox = constructLine(bidiRuns, lineInfo);
    if (!lineBox)
        return 0;

    lineBox->setEndsWithBreak(lineInfo.previousLineBrokeCleanly());
    
#if ENABLE(SVG)
    bool isSVGRootInlineBox = lineBox->isSVGRootInlineBox();
#else
    bool isSVGRootInlineBox = false;
#endif
    
    GlyphOverflowAndFallbackFontsMap textBoxDataMap;
    
    // Now we position all of our text runs horizontally.
    if (!isSVGRootInlineBox)
        computeInlineDirectionPositionsForLine(lineBox, lineInfo, bidiRuns.firstRun(), trailingSpaceRun, end.atEnd(), textBoxDataMap, verticalPositionCache, wordMeasurements);
    
    // Now position our text runs vertically.
    computeBlockDirectionPositionsForLine(lineBox, bidiRuns.firstRun(), textBoxDataMap, verticalPositionCache);
    
#if ENABLE(SVG)
    // SVG text layout code computes vertical & horizontal positions on its own.
    // Note that we still need to execute computeVerticalPositionsForLine() as
    // it calls InlineTextBox::positionLineBox(), which tracks whether the box
    // contains reversed text or not. If we wouldn't do that editing and thus
    // text selection in RTL boxes would not work as expected.
    if (isSVGRootInlineBox) {
        ASSERT_WITH_SECURITY_IMPLICATION(isSVGText());
        toSVGRootInlineBox(lineBox)->computePerCharacterLayoutInformation();
    }
#endif
    
    // Compute our overflow now.
    lineBox->computeOverflow(lineBox->lineTop(), lineBox->lineBottom(), textBoxDataMap);
    
#if PLATFORM(MAC)
    // Highlight acts as an overflow inflation.
    if (style().highlight() != nullAtom)
        lineBox->addHighlightOverflow();
#endif
    return lineBox;
}

static void deleteLineRange(LineLayoutState& layoutState, RootInlineBox* startLine, RootInlineBox* stopLine = 0)
{
    RootInlineBox* boxToDelete = startLine;
    while (boxToDelete && boxToDelete != stopLine) {
        layoutState.updateRepaintRangeFromBox(boxToDelete);
        // Note: deleteLineRange(firstRootBox()) is not identical to deleteLineBoxTree().
        // deleteLineBoxTree uses nextLineBox() instead of nextRootBox() when traversing.
        RootInlineBox* next = boxToDelete->nextRootBox();
        boxToDelete->deleteLine();
        boxToDelete = next;
    }
}

void RenderBlockFlow::layoutRunsAndFloats(LineLayoutState& layoutState, bool hasInlineChild)
{
    // We want to skip ahead to the first dirty line
    InlineBidiResolver resolver;
    RootInlineBox* startLine = determineStartPosition(layoutState, resolver);

    unsigned consecutiveHyphenatedLines = 0;
    if (startLine) {
        for (RootInlineBox* line = startLine->prevRootBox(); line && line->isHyphenated(); line = line->prevRootBox())
            consecutiveHyphenatedLines++;
    }

    // FIXME: This would make more sense outside of this function, but since
    // determineStartPosition can change the fullLayout flag we have to do this here. Failure to call
    // determineStartPosition first will break fast/repaint/line-flow-with-floats-9.html.
    if (layoutState.isFullLayout() && hasInlineChild && !selfNeedsLayout()) {
        setNeedsLayout(MarkOnlyThis); // Mark as needing a full layout to force us to repaint.
        if (!view().doingFullRepaint() && hasLayer()) {
            // Because we waited until we were already inside layout to discover
            // that the block really needed a full layout, we missed our chance to repaint the layer
            // before layout started.  Luckily the layer has cached the repaint rect for its original
            // position and size, and so we can use that to make a repaint happen now.
            repaintUsingContainer(containerForRepaint(), pixelSnappedIntRect(layer()->repaintRect()));
        }
    }

    if (containsFloats())
        layoutState.setLastFloat(m_floatingObjects->set().last().get());

    // We also find the first clean line and extract these lines.  We will add them back
    // if we determine that we're able to synchronize after handling all our dirty lines.
    InlineIterator cleanLineStart;
    BidiStatus cleanLineBidiStatus;
    if (!layoutState.isFullLayout() && startLine)
        determineEndPosition(layoutState, startLine, cleanLineStart, cleanLineBidiStatus);

    if (startLine) {
        if (!layoutState.usesRepaintBounds())
            layoutState.setRepaintRange(logicalHeight());
        deleteLineRange(layoutState, startLine);
    }

    if (!layoutState.isFullLayout() && lastRootBox() && lastRootBox()->endsWithBreak()) {
        // If the last line before the start line ends with a line break that clear floats,
        // adjust the height accordingly.
        // A line break can be either the first or the last object on a line, depending on its direction.
        if (InlineBox* lastLeafChild = lastRootBox()->lastLeafChild()) {
            RenderObject* lastObject = &lastLeafChild->renderer();
            if (!lastObject->isBR())
                lastObject = &lastRootBox()->firstLeafChild()->renderer();
            if (lastObject->isBR()) {
                EClear clear = lastObject->style().clear();
                if (clear != CNONE)
                    newLine(clear);
            }
        }
    }

    layoutRunsAndFloatsInRange(layoutState, resolver, cleanLineStart, cleanLineBidiStatus, consecutiveHyphenatedLines);
    linkToEndLineIfNeeded(layoutState);
    repaintDirtyFloats(layoutState.floats());
}

RenderTextInfo::RenderTextInfo()
    : m_text(0)
    , m_font(0)
{
}

RenderTextInfo::~RenderTextInfo()
{
}

// Before restarting the layout loop with a new logicalHeight, remove all floats that were added and reset the resolver.
inline const InlineIterator& RenderBlockFlow::restartLayoutRunsAndFloatsInRange(LayoutUnit oldLogicalHeight, LayoutUnit newLogicalHeight,  FloatingObject* lastFloatFromPreviousLine, InlineBidiResolver& resolver,  const InlineIterator& oldEnd)
{
    removeFloatingObjectsBelow(lastFloatFromPreviousLine, oldLogicalHeight);
    setLogicalHeight(newLogicalHeight);
    resolver.setPositionIgnoringNestedIsolates(oldEnd);
    return oldEnd;
}

#if ENABLE(CSS_SHAPES)
static inline void pushShapeContentOverflowBelowTheContentBox(RenderBlockFlow* block, ShapeInsideInfo* shapeInsideInfo, LayoutUnit lineTop, LayoutUnit lineHeight)
{
    ASSERT(shapeInsideInfo);

    LayoutUnit logicalLineBottom = lineTop + lineHeight;
    LayoutUnit shapeLogicalBottom = shapeInsideInfo->shapeLogicalBottom();
    LayoutUnit shapeContainingBlockLogicalHeight = shapeInsideInfo->shapeContainingBlockLogicalHeight();

    bool isOverflowPositionedAlready = (shapeContainingBlockLogicalHeight - shapeInsideInfo->owner().borderAndPaddingAfter() + lineHeight) <= lineTop;

    // If the last line overlaps with the shape, we don't need the segments anymore
    if (lineTop < shapeLogicalBottom && shapeLogicalBottom < logicalLineBottom)
        shapeInsideInfo->clearSegments();

    if (logicalLineBottom <= shapeLogicalBottom || !shapeContainingBlockLogicalHeight || isOverflowPositionedAlready)
        return;

    LayoutUnit newLogicalHeight = block->logicalHeight() + (shapeContainingBlockLogicalHeight - (lineTop + shapeInsideInfo->owner().borderAndPaddingAfter()));
    block->setLogicalHeight(newLogicalHeight);
}

void RenderBlockFlow::updateShapeAndSegmentsForCurrentLine(ShapeInsideInfo*& shapeInsideInfo, const LayoutSize& logicalOffsetFromShapeContainer, LineLayoutState& layoutState)
{
    if (layoutState.flowThread())
        return updateShapeAndSegmentsForCurrentLineInFlowThread(shapeInsideInfo, layoutState);

    if (!shapeInsideInfo)
        return;

    LayoutUnit lineTop = logicalHeight() + logicalOffsetFromShapeContainer.height();
    LayoutUnit lineLeft = logicalOffsetFromShapeContainer.width();
    LayoutUnit lineHeight = this->lineHeight(layoutState.lineInfo().isFirstLine(), isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);

    // FIXME: Bug 95361: It is possible for a line to grow beyond lineHeight, in which case these segments may be incorrect.
    shapeInsideInfo->updateSegmentsForLine(LayoutSize(lineLeft, lineTop), lineHeight);

    pushShapeContentOverflowBelowTheContentBox(this, shapeInsideInfo, lineTop, lineHeight);
}

void RenderBlockFlow::updateShapeAndSegmentsForCurrentLineInFlowThread(ShapeInsideInfo*& shapeInsideInfo, LineLayoutState& layoutState)
{
    ASSERT(layoutState.flowThread());

    RenderRegion* currentRegion = regionAtBlockOffset(logicalHeight());
    if (!currentRegion || !currentRegion->logicalHeight())
        return;

    shapeInsideInfo = currentRegion->shapeInsideInfo();

    RenderRegion* nextRegion = 0;
    if (!currentRegion->isLastRegion()) {
        RenderRegionList regionList = layoutState.flowThread()->renderRegionList();
        auto it = regionList.find(currentRegion);
        nextRegion = *(++it);
    }

    // We only want to deal regions with shapes, so we check if the next region has a shape
    if (!shapeInsideInfo && nextRegion && !nextRegion->shapeInsideInfo())
        return;

    LayoutUnit lineHeight = this->lineHeight(layoutState.lineInfo().isFirstLine(), isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
    LayoutUnit logicalLineTopInFlowThread = logicalHeight() + offsetFromLogicalTopOfFirstPage();
    LayoutUnit logicalLineBottomInFlowThread = logicalLineTopInFlowThread + lineHeight;
    LayoutUnit logicalRegionTopInFlowThread = currentRegion->logicalTopForFlowThreadContent();
    LayoutUnit logicalRegionBottomInFlowThread = logicalRegionTopInFlowThread + currentRegion->logicalHeight() - currentRegion->borderAndPaddingBefore() - currentRegion->borderAndPaddingAfter();

    LayoutUnit shapeBottomInFlowThread = LayoutUnit::max();
    if (shapeInsideInfo)
        shapeBottomInFlowThread = shapeInsideInfo->shapeLogicalBottom() + currentRegion->logicalTopForFlowThreadContent();

    bool lineOverLapsWithShapeBottom = shapeBottomInFlowThread < logicalLineBottomInFlowThread;
    bool lineTopAdjustedIntoNextRegion = layoutState.adjustedLogicalLineTop() >= currentRegion->logicalHeight();
    bool lineOverLapsWithRegionBottom = logicalLineBottomInFlowThread > logicalRegionBottomInFlowThread || lineTopAdjustedIntoNextRegion;
    bool overFlowsToNextRegion = nextRegion && (lineOverLapsWithShapeBottom || lineOverLapsWithRegionBottom);

    // If the line is between two shapes/regions we position the line to the top of the next shape/region
    if (overFlowsToNextRegion) {
        ASSERT(currentRegion != nextRegion);
        LayoutUnit deltaToNextRegion = logicalRegionBottomInFlowThread - logicalLineTopInFlowThread;
        setLogicalHeight(logicalHeight() + deltaToNextRegion);

        currentRegion = nextRegion;
        shapeInsideInfo = currentRegion->shapeInsideInfo();

        logicalLineTopInFlowThread = logicalHeight() + offsetFromLogicalTopOfFirstPage();
        logicalLineBottomInFlowThread = logicalLineTopInFlowThread + lineHeight;
        logicalRegionTopInFlowThread = currentRegion->logicalTopForFlowThreadContent();
        logicalRegionBottomInFlowThread = logicalRegionTopInFlowThread + currentRegion->logicalHeight() - currentRegion->borderAndPaddingBefore() - currentRegion->borderAndPaddingAfter();

        if (lineTopAdjustedIntoNextRegion)
            layoutState.setAdjustedLogicalLineTop(0);
    }

    if (!shapeInsideInfo)
        return;

    bool isFirstLineInRegion = logicalLineBottomInFlowThread <= (logicalRegionTopInFlowThread + lineHeight);
    bool isFirstLineAdjusted = (logicalLineTopInFlowThread - logicalRegionTopInFlowThread) < (layoutState.adjustedLogicalLineTop() - currentRegion->borderAndPaddingBefore());
    // We position the first line to the top of the shape in the region or to the previously adjusted position in the shape
    if (isFirstLineInRegion || isFirstLineAdjusted) {
        LayoutUnit shapeTopOffset = layoutState.adjustedLogicalLineTop();

        if (!shapeTopOffset && (shapeInsideInfo->shapeLogicalTop() > 0))
            shapeTopOffset = shapeInsideInfo->shapeLogicalTop();

        LayoutUnit shapePositionInFlowThread = currentRegion->logicalTopForFlowThreadContent() + shapeTopOffset;
        LayoutUnit shapeTopLineTopDelta = shapePositionInFlowThread - logicalLineTopInFlowThread - currentRegion->borderAndPaddingBefore();

        setLogicalHeight(logicalHeight() + shapeTopLineTopDelta);
        logicalLineTopInFlowThread += shapeTopLineTopDelta;
        layoutState.setAdjustedLogicalLineTop(0);
    }

    LayoutUnit lineTop = logicalLineTopInFlowThread - currentRegion->logicalTopForFlowThreadContent() + currentRegion->borderAndPaddingBefore();

    // FIXME: 118571 - Shape inside on a region does not yet take into account its padding for nested flow blocks
    shapeInsideInfo->updateSegmentsForLine(LayoutSize(0, lineTop), lineHeight);

    if (currentRegion->isLastRegion())
        pushShapeContentOverflowBelowTheContentBox(this, shapeInsideInfo, lineTop, lineHeight);
}

static inline LayoutUnit adjustLogicalLineTop(ShapeInsideInfo* shapeInsideInfo, InlineIterator start, InlineIterator end, const WordMeasurements& wordMeasurements)
{
    if (!shapeInsideInfo || end != start)
        return 0;

    float minWidth = firstPositiveWidth(wordMeasurements);
    ASSERT(minWidth || wordMeasurements.isEmpty());
    if (minWidth > 0 && shapeInsideInfo->adjustLogicalLineTop(minWidth))
        return shapeInsideInfo->logicalLineTop();

    return shapeInsideInfo->shapeLogicalBottom();
}

bool RenderBlockFlow::adjustLogicalLineTopAndLogicalHeightIfNeeded(ShapeInsideInfo* shapeInsideInfo, LayoutUnit absoluteLogicalTop, LineLayoutState& layoutState, InlineBidiResolver& resolver, FloatingObject* lastFloatFromPreviousLine, InlineIterator& end, WordMeasurements& wordMeasurements)
{
    LayoutUnit adjustedLogicalLineTop = adjustLogicalLineTop(shapeInsideInfo, resolver.position(), end, wordMeasurements);

    if (shapeInsideInfo && containsFloats()) {
        lastFloatFromPreviousLine = m_floatingObjects->set().last().get();
        if (!wordMeasurements.size()) {
            LayoutUnit floatLogicalTopOffset = shapeInsideInfo->computeFirstFitPositionForFloat(logicalSizeForFloat(lastFloatFromPreviousLine));
            if (logicalHeight() < floatLogicalTopOffset)
                adjustedLogicalLineTop = floatLogicalTopOffset;
        }
    }

    if (!adjustedLogicalLineTop)
        return false;

    LayoutUnit newLogicalHeight = adjustedLogicalLineTop - absoluteLogicalTop;

    if (layoutState.flowThread()) {
        layoutState.setAdjustedLogicalLineTop(adjustedLogicalLineTop);
        newLogicalHeight = logicalHeight();
    }

    end = restartLayoutRunsAndFloatsInRange(logicalHeight(), newLogicalHeight, lastFloatFromPreviousLine, resolver, end);
    return true;
}
#endif

void RenderBlockFlow::layoutRunsAndFloatsInRange(LineLayoutState& layoutState, InlineBidiResolver& resolver, const InlineIterator& cleanLineStart, const BidiStatus& cleanLineBidiStatus, unsigned consecutiveHyphenatedLines)
{
    const RenderStyle& styleToUse = style();
    bool paginated = view().layoutState() && view().layoutState()->isPaginated();
    LineMidpointState& lineMidpointState = resolver.midpointState();
    InlineIterator end = resolver.position();
    bool checkForEndLineMatch = layoutState.endLine();
    RenderTextInfo renderTextInfo;
    VerticalPositionCache verticalPositionCache;

    LineBreaker lineBreaker(*this);

#if ENABLE(CSS_SHAPES)
    LayoutSize logicalOffsetFromShapeContainer;
    ShapeInsideInfo* shapeInsideInfo = layoutShapeInsideInfo();
    if (shapeInsideInfo) {
        ASSERT(&shapeInsideInfo->owner() == this || allowsShapeInsideInfoSharing());
        if (shapeInsideInfo != this->shapeInsideInfo()) {
            // FIXME Bug 100284: If subsequent LayoutStates are pushed, we will have to add
            // their offsets from the original shape-inside container.
            logicalOffsetFromShapeContainer = logicalOffsetFromShapeAncestorContainer(&shapeInsideInfo->owner());
        }
        // Begin layout at the logical top of our shape inside.
        if (logicalHeight() + logicalOffsetFromShapeContainer.height() < shapeInsideInfo->shapeLogicalTop()) {
            LayoutUnit logicalHeight = shapeInsideInfo->shapeLogicalTop() - logicalOffsetFromShapeContainer.height();
            if (layoutState.flowThread())
                logicalHeight -= shapeInsideInfo->owner().borderAndPaddingBefore();
            setLogicalHeight(logicalHeight);
        }
    }
#endif

    while (!end.atEnd()) {
        // FIXME: Is this check necessary before the first iteration or can it be moved to the end?
        if (checkForEndLineMatch) {
            layoutState.setEndLineMatched(matchedEndLine(layoutState, resolver, cleanLineStart, cleanLineBidiStatus));
            if (layoutState.endLineMatched()) {
                resolver.setPosition(InlineIterator(resolver.position().root(), 0, 0), 0);
                break;
            }
        }

        lineMidpointState.reset();

        layoutState.lineInfo().setEmpty(true);
        layoutState.lineInfo().resetRunsFromLeadingWhitespace();

        const InlineIterator oldEnd = end;
        bool isNewUBAParagraph = layoutState.lineInfo().previousLineBrokeCleanly();
        FloatingObject* lastFloatFromPreviousLine = (containsFloats()) ? m_floatingObjects->set().last().get() : 0;

#if ENABLE(CSS_SHAPES)
        updateShapeAndSegmentsForCurrentLine(shapeInsideInfo, logicalOffsetFromShapeContainer, layoutState);
#endif
        WordMeasurements wordMeasurements;
        end = lineBreaker.nextLineBreak(resolver, layoutState.lineInfo(), renderTextInfo, lastFloatFromPreviousLine, consecutiveHyphenatedLines, wordMeasurements);
        renderTextInfo.m_lineBreakIterator.resetPriorContext();
        if (resolver.position().atEnd()) {
            // FIXME: We shouldn't be creating any runs in nextLineBreak to begin with!
            // Once BidiRunList is separated from BidiResolver this will not be needed.
            resolver.runs().deleteRuns();
            resolver.markCurrentRunEmpty(); // FIXME: This can probably be replaced by an ASSERT (or just removed).
            layoutState.setCheckForFloatsFromLastLine(true);
            resolver.setPosition(InlineIterator(resolver.position().root(), 0, 0), 0);
            break;
        }

#if ENABLE(CSS_SHAPES)
        if (adjustLogicalLineTopAndLogicalHeightIfNeeded(shapeInsideInfo, logicalOffsetFromShapeContainer.height(), layoutState, resolver, lastFloatFromPreviousLine, end, wordMeasurements))
            continue;
#endif
        ASSERT(end != resolver.position());

        // This is a short-cut for empty lines.
        if (layoutState.lineInfo().isEmpty()) {
            if (lastRootBox())
                lastRootBox()->setLineBreakInfo(end.renderer(), end.offset(), resolver.status());
        } else {
            VisualDirectionOverride override = (styleToUse.rtlOrdering() == VisualOrder ? (styleToUse.direction() == LTR ? VisualLeftToRightOverride : VisualRightToLeftOverride) : NoVisualOverride);

            if (isNewUBAParagraph && styleToUse.unicodeBidi() == Plaintext && !resolver.context()->parent()) {
                TextDirection direction = styleToUse.direction();
                determineDirectionality(direction, resolver.position());
                resolver.setStatus(BidiStatus(direction, isOverride(styleToUse.unicodeBidi())));
            }
            // FIXME: This ownership is reversed. We should own the BidiRunList and pass it to createBidiRunsForLine.
            BidiRunList<BidiRun>& bidiRuns = resolver.runs();
            constructBidiRunsForLine(this, resolver, bidiRuns, end, override, layoutState.lineInfo().previousLineBrokeCleanly());
            ASSERT(resolver.position() == end);

            BidiRun* trailingSpaceRun = !layoutState.lineInfo().previousLineBrokeCleanly() ? handleTrailingSpaces(bidiRuns, resolver.context()) : 0;

            if (bidiRuns.runCount() && lineBreaker.lineWasHyphenated()) {
                bidiRuns.logicallyLastRun()->m_hasHyphen = true;
                consecutiveHyphenatedLines++;
            } else
                consecutiveHyphenatedLines = 0;

            // Now that the runs have been ordered, we create the line boxes.
            // At the same time we figure out where border/padding/margin should be applied for
            // inline flow boxes.

            LayoutUnit oldLogicalHeight = logicalHeight();
            RootInlineBox* lineBox = createLineBoxesFromBidiRuns(bidiRuns, end, layoutState.lineInfo(), verticalPositionCache, trailingSpaceRun, wordMeasurements);

            bidiRuns.deleteRuns();
            resolver.markCurrentRunEmpty(); // FIXME: This can probably be replaced by an ASSERT (or just removed).

            if (lineBox) {
                lineBox->setLineBreakInfo(end.renderer(), end.offset(), resolver.status());
                if (layoutState.usesRepaintBounds())
                    layoutState.updateRepaintRangeFromBox(lineBox);

                if (paginated) {
                    LayoutUnit adjustment = 0;
                    adjustLinePositionForPagination(lineBox, adjustment, layoutState.flowThread());
                    if (adjustment) {
                        LayoutUnit oldLineWidth = availableLogicalWidthForLine(oldLogicalHeight, layoutState.lineInfo().isFirstLine());
                        lineBox->adjustBlockDirectionPosition(adjustment);
                        if (layoutState.usesRepaintBounds())
                            layoutState.updateRepaintRangeFromBox(lineBox);

                        if (availableLogicalWidthForLine(oldLogicalHeight + adjustment, layoutState.lineInfo().isFirstLine()) != oldLineWidth) {
                            // We have to delete this line, remove all floats that got added, and let line layout re-run.
                            lineBox->deleteLine();
                            end = restartLayoutRunsAndFloatsInRange(oldLogicalHeight, oldLogicalHeight + adjustment, lastFloatFromPreviousLine, resolver, oldEnd);
                            continue;
                        }

                        setLogicalHeight(lineBox->lineBottomWithLeading());
                    }

                    if (layoutState.flowThread())
                        updateRegionForLine(lineBox);
                }
            }
        }

        for (size_t i = 0; i < lineBreaker.positionedObjects().size(); ++i)
            setStaticPositions(*this, *lineBreaker.positionedObjects()[i]);

        if (!layoutState.lineInfo().isEmpty()) {
            layoutState.lineInfo().setFirstLine(false);
            newLine(lineBreaker.clear());
        }

        if (m_floatingObjects && lastRootBox()) {
            const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
            auto it = floatingObjectSet.begin();
            auto end = floatingObjectSet.end();
            if (layoutState.lastFloat()) {
                auto lastFloatIterator = floatingObjectSet.find<FloatingObject&, FloatingObjectHashTranslator>(*layoutState.lastFloat());
                ASSERT(lastFloatIterator != end);
                ++lastFloatIterator;
                it = lastFloatIterator;
            }
            for (; it != end; ++it) {
                FloatingObject* f = it->get();
                appendFloatingObjectToLastLine(f);
                ASSERT(&f->renderer() == &layoutState.floats()[layoutState.floatIndex()].object);
                // If a float's geometry has changed, give up on syncing with clean lines.
                if (layoutState.floats()[layoutState.floatIndex()].rect != f->frameRect())
                    checkForEndLineMatch = false;
                layoutState.setFloatIndex(layoutState.floatIndex() + 1);
            }
            layoutState.setLastFloat(!floatingObjectSet.isEmpty() ? floatingObjectSet.last().get() : nullptr);
        }

        lineMidpointState.reset();
        resolver.setPosition(end, numberOfIsolateAncestors(end));
    }

    // In case we already adjusted the line positions during this layout to avoid widows
    // then we need to ignore the possibility of having a new widows situation.
    // Otherwise, we risk leaving empty containers which is against the block fragmentation principles.
    if (paginated && !style().hasAutoWidows() && !didBreakAtLineToAvoidWidow()) {
        // Check the line boxes to make sure we didn't create unacceptable widows.
        // However, we'll prioritize orphans - so nothing we do here should create
        // a new orphan.

        RootInlineBox* lineBox = lastRootBox();

        // Count from the end of the block backwards, to see how many hanging
        // lines we have.
        RootInlineBox* firstLineInBlock = firstRootBox();
        int numLinesHanging = 1;
        while (lineBox && lineBox != firstLineInBlock && !lineBox->isFirstAfterPageBreak()) {
            ++numLinesHanging;
            lineBox = lineBox->prevRootBox();
        }

        // If there were no breaks in the block, we didn't create any widows.
        if (!lineBox || !lineBox->isFirstAfterPageBreak() || lineBox == firstLineInBlock)
            return;

        if (numLinesHanging < style().widows()) {
            // We have detected a widow. Now we need to work out how many
            // lines there are on the previous page, and how many we need
            // to steal.
            int numLinesNeeded = style().widows() - numLinesHanging;
            RootInlineBox* currentFirstLineOfNewPage = lineBox;

            // Count the number of lines in the previous page.
            lineBox = lineBox->prevRootBox();
            int numLinesInPreviousPage = 1;
            while (lineBox && lineBox != firstLineInBlock && !lineBox->isFirstAfterPageBreak()) {
                ++numLinesInPreviousPage;
                lineBox = lineBox->prevRootBox();
            }

            // If there was an explicit value for orphans, respect that. If not, we still
            // shouldn't create a situation where we make an orphan bigger than the initial value.
            // This means that setting widows implies we also care about orphans, but given
            // the specification says the initial orphan value is non-zero, this is ok. The
            // author is always free to set orphans explicitly as well.
            int orphans = style().hasAutoOrphans() ? style().initialOrphans() : style().orphans();
            int numLinesAvailable = numLinesInPreviousPage - orphans;
            if (numLinesAvailable <= 0)
                return;

            int numLinesToTake = std::min(numLinesAvailable, numLinesNeeded);
            // Wind back from our first widowed line.
            lineBox = currentFirstLineOfNewPage;
            for (int i = 0; i < numLinesToTake; ++i)
                lineBox = lineBox->prevRootBox();

            // We now want to break at this line. Remember for next layout and trigger relayout.
            setBreakAtLineToAvoidWidow(lineCount(lineBox));
            markLinesDirtyInBlockRange(lastRootBox()->lineBottomWithLeading(), lineBox->lineBottomWithLeading(), lineBox);
        }
    }

    clearDidBreakAtLineToAvoidWidow();
}

void RenderBlockFlow::linkToEndLineIfNeeded(LineLayoutState& layoutState)
{
    if (layoutState.endLine()) {
        if (layoutState.endLineMatched()) {
            bool paginated = view().layoutState() && view().layoutState()->isPaginated();
            // Attach all the remaining lines, and then adjust their y-positions as needed.
            LayoutUnit delta = logicalHeight() - layoutState.endLineLogicalTop();
            for (RootInlineBox* line = layoutState.endLine(); line; line = line->nextRootBox()) {
                line->attachLine();
                if (paginated) {
                    delta -= line->paginationStrut();
                    adjustLinePositionForPagination(line, delta, layoutState.flowThread());
                }
                if (delta) {
                    layoutState.updateRepaintRangeFromBox(line, delta);
                    line->adjustBlockDirectionPosition(delta);
                }
                if (layoutState.flowThread())
                    updateRegionForLine(line);
                if (Vector<RenderBox*>* cleanLineFloats = line->floatsPtr()) {
                    for (auto it = cleanLineFloats->begin(), end = cleanLineFloats->end(); it != end; ++it) {
                        RenderBox* floatingBox = *it;
                        FloatingObject* floatingObject = insertFloatingObject(*floatingBox);
                        ASSERT(!floatingObject->originatingLine());
                        floatingObject->setOriginatingLine(line);
                        setLogicalHeight(logicalTopForChild(*floatingBox) - marginBeforeForChild(*floatingBox) + delta);
                        positionNewFloats();
                    }
                }
            }
            setLogicalHeight(lastRootBox()->lineBottomWithLeading());
        } else {
            // Delete all the remaining lines.
            deleteLineRange(layoutState, layoutState.endLine());
        }
    }
    
    if (m_floatingObjects && (layoutState.checkForFloatsFromLastLine() || positionNewFloats()) && lastRootBox()) {
        // In case we have a float on the last line, it might not be positioned up to now.
        // This has to be done before adding in the bottom border/padding, or the float will
        // include the padding incorrectly. -dwh
        if (layoutState.checkForFloatsFromLastLine()) {
            LayoutUnit bottomVisualOverflow = lastRootBox()->logicalBottomVisualOverflow();
            LayoutUnit bottomLayoutOverflow = lastRootBox()->logicalBottomLayoutOverflow();
            auto newLineBox = std::make_unique<TrailingFloatsRootInlineBox>(*this);
            auto trailingFloatsLineBox = newLineBox.get();
            m_lineBoxes.appendLineBox(std::move(newLineBox));
            trailingFloatsLineBox->setConstructed();
            GlyphOverflowAndFallbackFontsMap textBoxDataMap;
            VerticalPositionCache verticalPositionCache;
            LayoutUnit blockLogicalHeight = logicalHeight();
            trailingFloatsLineBox->alignBoxesInBlockDirection(blockLogicalHeight, textBoxDataMap, verticalPositionCache);
            trailingFloatsLineBox->setLineTopBottomPositions(blockLogicalHeight, blockLogicalHeight, blockLogicalHeight, blockLogicalHeight);
            trailingFloatsLineBox->setPaginatedLineWidth(availableLogicalWidthForContent(blockLogicalHeight));
            LayoutRect logicalLayoutOverflow(0, blockLogicalHeight, 1, bottomLayoutOverflow - blockLogicalHeight);
            LayoutRect logicalVisualOverflow(0, blockLogicalHeight, 1, bottomVisualOverflow - blockLogicalHeight);
            trailingFloatsLineBox->setOverflowFromLogicalRects(logicalLayoutOverflow, logicalVisualOverflow, trailingFloatsLineBox->lineTop(), trailingFloatsLineBox->lineBottom());
            if (layoutState.flowThread())
                updateRegionForLine(trailingFloatsLineBox);
        }

        const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
        auto it = floatingObjectSet.begin();
        auto end = floatingObjectSet.end();
        if (layoutState.lastFloat()) {
            auto lastFloatIterator = floatingObjectSet.find<FloatingObject&, FloatingObjectHashTranslator>(*layoutState.lastFloat());
            ASSERT(lastFloatIterator != end);
            ++lastFloatIterator;
            it = lastFloatIterator;
        }
        for (; it != end; ++it)
            appendFloatingObjectToLastLine(it->get());
        layoutState.setLastFloat(!floatingObjectSet.isEmpty() ? floatingObjectSet.last().get() : nullptr);
    }
}

void RenderBlockFlow::repaintDirtyFloats(Vector<FloatWithRect>& floats)
{
    size_t floatCount = floats.size();
    // Floats that did not have layout did not repaint when we laid them out. They would have
    // painted by now if they had moved, but if they stayed at (0, 0), they still need to be
    // painted.
    for (size_t i = 0; i < floatCount; ++i) {
        if (!floats[i].everHadLayout) {
            RenderBox& box = floats[i].object;
            if (!box.x() && !box.y() && box.checkForRepaintDuringLayout())
                box.repaint();
        }
    }
}

void RenderBlockFlow::layoutLineBoxes(bool relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom)
{
    ASSERT(!m_simpleLineLayout);

    setLogicalHeight(borderAndPaddingBefore());
    
    // Lay out our hypothetical grid line as though it occurs at the top of the block.
    if (view().layoutState() && view().layoutState()->lineGrid() == this)
        layoutLineGridBox();

    RenderFlowThread* flowThread = flowThreadContainingBlock();
    bool clearLinesForPagination = firstRootBox() && flowThread && !flowThread->hasRegions();

    // Figure out if we should clear out our line boxes.
    // FIXME: Handle resize eventually!
    bool isFullLayout = !firstRootBox() || selfNeedsLayout() || relayoutChildren || clearLinesForPagination;
    LineLayoutState layoutState(isFullLayout, repaintLogicalTop, repaintLogicalBottom, flowThread);

    if (isFullLayout)
        lineBoxes().deleteLineBoxes();

    // Text truncation kicks in in two cases:
    //     1) If your overflow isn't visible and your text-overflow-mode isn't clip.
    //     2) If you're an anonymous block with a block parent that satisfies #1.
    // FIXME: CSS3 says that descendants that are clipped must also know how to truncate.  This is insanely
    // difficult to figure out in general (especially in the middle of doing layout), so we only handle the
    // simple case of an anonymous block truncating when it's parent is clipped.
    bool hasTextOverflow = (style().textOverflow() && hasOverflowClip())
        || (isAnonymousBlock() && parent() && parent()->isRenderBlock() && parent()->style().textOverflow() && parent()->hasOverflowClip());

    // Walk all the lines and delete our ellipsis line boxes if they exist.
    if (hasTextOverflow)
         deleteEllipsisLineBoxes();

    if (firstChild()) {
        // In full layout mode, clear the line boxes of children upfront. Otherwise,
        // siblings can run into stale root lineboxes during layout. Then layout
        // the replaced elements later. In partial layout mode, line boxes are not
        // deleted and only dirtied. In that case, we can layout the replaced
        // elements at the same time.
        bool hasInlineChild = false;
        Vector<RenderBox*> replacedChildren;
        for (InlineWalker walker(*this); !walker.atEnd(); walker.advance()) {
            RenderObject& o = *walker.current();

            if (!hasInlineChild && o.isInline())
                hasInlineChild = true;

            if (o.isReplaced() || o.isFloating() || o.isOutOfFlowPositioned()) {
                RenderBox& box = toRenderBox(o);

                if (relayoutChildren || box.hasRelativeDimensions())
                    box.setChildNeedsLayout(MarkOnlyThis);

                // If relayoutChildren is set and the child has percentage padding or an embedded content box, we also need to invalidate the childs pref widths.
                if (relayoutChildren && box.needsPreferredWidthsRecalculation())
                    box.setPreferredLogicalWidthsDirty(true, MarkOnlyThis);

                if (box.isOutOfFlowPositioned())
                    box.containingBlock()->insertPositionedObject(box);
                else if (box.isFloating())
                    layoutState.floats().append(FloatWithRect(box));
                else if (isFullLayout || box.needsLayout()) {
                    // Replaced element.
                    box.dirtyLineBoxes(isFullLayout);
                    if (isFullLayout)
                        replacedChildren.append(&box);
                    else
                        box.layoutIfNeeded();
                }
            } else if (o.isTextOrLineBreak() || (o.isRenderInline() && !walker.atEndOfInline())) {
                if (o.isRenderInline())
                    toRenderInline(o).updateAlwaysCreateLineBoxes(layoutState.isFullLayout());
                if (layoutState.isFullLayout() || o.selfNeedsLayout())
                    dirtyLineBoxesForRenderer(o, layoutState.isFullLayout());
                o.clearNeedsLayout();
            }
        }

        for (size_t i = 0; i < replacedChildren.size(); i++)
             replacedChildren[i]->layoutIfNeeded();

        layoutRunsAndFloats(layoutState, hasInlineChild);
    }

    // Expand the last line to accommodate Ruby and emphasis marks.
    int lastLineAnnotationsAdjustment = 0;
    if (lastRootBox()) {
        LayoutUnit lowestAllowedPosition = std::max(lastRootBox()->lineBottom(), logicalHeight() + paddingAfter());
        if (!style().isFlippedLinesWritingMode())
            lastLineAnnotationsAdjustment = lastRootBox()->computeUnderAnnotationAdjustment(lowestAllowedPosition);
        else
            lastLineAnnotationsAdjustment = lastRootBox()->computeOverAnnotationAdjustment(lowestAllowedPosition);
    }

    // Now add in the bottom border/padding.
    setLogicalHeight(logicalHeight() + lastLineAnnotationsAdjustment + borderAndPaddingAfter() + scrollbarLogicalHeight());

    if (!firstRootBox() && hasLineIfEmpty())
        setLogicalHeight(logicalHeight() + lineHeight(true, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes));

    // See if we have any lines that spill out of our block.  If we do, then we will possibly need to
    // truncate text.
    if (hasTextOverflow)
        checkLinesForTextOverflow();
}

void RenderBlockFlow::checkFloatsInCleanLine(RootInlineBox* line, Vector<FloatWithRect>& floats, size_t& floatIndex, bool& encounteredNewFloat, bool& dirtiedByFloat)
{
    Vector<RenderBox*>* cleanLineFloats = line->floatsPtr();
    if (!cleanLineFloats)
        return;

    for (auto it = cleanLineFloats->begin(), end = cleanLineFloats->end(); it != end; ++it) {
        RenderBox* floatingBox = *it;
        floatingBox->layoutIfNeeded();
        LayoutSize newSize(floatingBox->width() + floatingBox->marginWidth(), floatingBox->height() + floatingBox->marginHeight());
        ASSERT_WITH_SECURITY_IMPLICATION(floatIndex < floats.size());
        if (&floats[floatIndex].object != floatingBox) {
            encounteredNewFloat = true;
            return;
        }

        if (floats[floatIndex].rect.size() != newSize) {
            LayoutUnit floatTop = isHorizontalWritingMode() ? floats[floatIndex].rect.y() : floats[floatIndex].rect.x();
            LayoutUnit floatHeight = isHorizontalWritingMode() ? std::max(floats[floatIndex].rect.height(), newSize.height()) : std::max(floats[floatIndex].rect.width(), newSize.width());
            floatHeight = std::min(floatHeight, LayoutUnit::max() - floatTop);
            line->markDirty();
            markLinesDirtyInBlockRange(line->lineBottomWithLeading(), floatTop + floatHeight, line);
            floats[floatIndex].rect.setSize(newSize);
            dirtiedByFloat = true;
        }
        floatIndex++;
    }
}

RootInlineBox* RenderBlockFlow::determineStartPosition(LineLayoutState& layoutState, InlineBidiResolver& resolver)
{
    RootInlineBox* curr = 0;
    RootInlineBox* last = 0;

    // FIXME: This entire float-checking block needs to be broken into a new function.
    bool dirtiedByFloat = false;
    if (!layoutState.isFullLayout()) {
        // Paginate all of the clean lines.
        bool paginated = view().layoutState() && view().layoutState()->isPaginated();
        LayoutUnit paginationDelta = 0;
        size_t floatIndex = 0;
        for (curr = firstRootBox(); curr && !curr->isDirty(); curr = curr->nextRootBox()) {
            if (paginated) {
                if (lineWidthForPaginatedLineChanged(curr, 0, layoutState.flowThread())) {
                    curr->markDirty();
                    break;
                }
                paginationDelta -= curr->paginationStrut();
                adjustLinePositionForPagination(curr, paginationDelta, layoutState.flowThread());
                if (paginationDelta) {
                    if (containsFloats() || !layoutState.floats().isEmpty()) {
                        // FIXME: Do better eventually.  For now if we ever shift because of pagination and floats are present just go to a full layout.
                        layoutState.markForFullLayout();
                        break;
                    }

                    layoutState.updateRepaintRangeFromBox(curr, paginationDelta);
                    curr->adjustBlockDirectionPosition(paginationDelta);
                }
                if (layoutState.flowThread())
                    updateRegionForLine(curr);
            }

            // If a new float has been inserted before this line or before its last known float, just do a full layout.
            bool encounteredNewFloat = false;
            checkFloatsInCleanLine(curr, layoutState.floats(), floatIndex, encounteredNewFloat, dirtiedByFloat);
            if (encounteredNewFloat)
                layoutState.markForFullLayout();

            if (dirtiedByFloat || layoutState.isFullLayout())
                break;
        }
        // Check if a new float has been inserted after the last known float.
        if (!curr && floatIndex < layoutState.floats().size())
            layoutState.markForFullLayout();
    }

    if (layoutState.isFullLayout()) {
        m_lineBoxes.deleteLineBoxTree();
        curr = 0;

        ASSERT(!firstRootBox() && !lastRootBox());
    } else {
        if (curr) {
            // We have a dirty line.
            if (RootInlineBox* prevRootBox = curr->prevRootBox()) {
                // We have a previous line.
                if (!dirtiedByFloat && (!prevRootBox->endsWithBreak() || !prevRootBox->lineBreakObj() || (prevRootBox->lineBreakObj()->isText() && prevRootBox->lineBreakPos() >= toRenderText(prevRootBox->lineBreakObj())->textLength())))
                    // The previous line didn't break cleanly or broke at a newline
                    // that has been deleted, so treat it as dirty too.
                    curr = prevRootBox;
            }
        } else {
            // No dirty lines were found.
            // If the last line didn't break cleanly, treat it as dirty.
            if (lastRootBox() && !lastRootBox()->endsWithBreak())
                curr = lastRootBox();
        }

        // If we have no dirty lines, then last is just the last root box.
        last = curr ? curr->prevRootBox() : lastRootBox();
    }

    unsigned numCleanFloats = 0;
    if (!layoutState.floats().isEmpty()) {
        LayoutUnit savedLogicalHeight = logicalHeight();
        // Restore floats from clean lines.
        RootInlineBox* line = firstRootBox();
        while (line != curr) {
            if (Vector<RenderBox*>* cleanLineFloats = line->floatsPtr()) {
                for (auto it = cleanLineFloats->begin(), end = cleanLineFloats->end(); it != end; ++it) {
                    RenderBox* floatingBox = *it;
                    FloatingObject* floatingObject = insertFloatingObject(*floatingBox);
                    ASSERT(!floatingObject->originatingLine());
                    floatingObject->setOriginatingLine(line);
                    setLogicalHeight(logicalTopForChild(*floatingBox) - marginBeforeForChild(*floatingBox));
                    positionNewFloats();
                    ASSERT(&layoutState.floats()[numCleanFloats].object == floatingBox);
                    numCleanFloats++;
                }
            }
            line = line->nextRootBox();
        }
        setLogicalHeight(savedLogicalHeight);
    }
    layoutState.setFloatIndex(numCleanFloats);

    layoutState.lineInfo().setFirstLine(!last);
    layoutState.lineInfo().setPreviousLineBrokeCleanly(!last || last->endsWithBreak());

    if (last) {
        setLogicalHeight(last->lineBottomWithLeading());
        InlineIterator iter = InlineIterator(this, last->lineBreakObj(), last->lineBreakPos());
        resolver.setPosition(iter, numberOfIsolateAncestors(iter));
        resolver.setStatus(last->lineBreakBidiStatus());
    } else {
        TextDirection direction = style().direction();
        if (style().unicodeBidi() == Plaintext)
            determineDirectionality(direction, InlineIterator(this, bidiFirstSkippingEmptyInlines(*this), 0));
        resolver.setStatus(BidiStatus(direction, isOverride(style().unicodeBidi())));
        InlineIterator iter = InlineIterator(this, bidiFirstSkippingEmptyInlines(*this, &resolver), 0);
        resolver.setPosition(iter, numberOfIsolateAncestors(iter));
    }
    return curr;
}

void RenderBlockFlow::determineEndPosition(LineLayoutState& layoutState, RootInlineBox* startLine, InlineIterator& cleanLineStart, BidiStatus& cleanLineBidiStatus)
{
    ASSERT(!layoutState.endLine());
    size_t floatIndex = layoutState.floatIndex();
    RootInlineBox* last = 0;
    for (RootInlineBox* curr = startLine->nextRootBox(); curr; curr = curr->nextRootBox()) {
        if (!curr->isDirty()) {
            bool encounteredNewFloat = false;
            bool dirtiedByFloat = false;
            checkFloatsInCleanLine(curr, layoutState.floats(), floatIndex, encounteredNewFloat, dirtiedByFloat);
            if (encounteredNewFloat)
                return;
        }
        if (curr->isDirty())
            last = 0;
        else if (!last)
            last = curr;
    }

    if (!last)
        return;

    // At this point, |last| is the first line in a run of clean lines that ends with the last line
    // in the block.

    RootInlineBox* prev = last->prevRootBox();
    cleanLineStart = InlineIterator(this, prev->lineBreakObj(), prev->lineBreakPos());
    cleanLineBidiStatus = prev->lineBreakBidiStatus();
    layoutState.setEndLineLogicalTop(prev->lineBottomWithLeading());

    for (RootInlineBox* line = last; line; line = line->nextRootBox())
        line->extractLine(); // Disconnect all line boxes from their render objects while preserving
                             // their connections to one another.

    layoutState.setEndLine(last);
}

bool RenderBlockFlow::checkPaginationAndFloatsAtEndLine(LineLayoutState& layoutState)
{
    LayoutUnit lineDelta = logicalHeight() - layoutState.endLineLogicalTop();

    bool paginated = view().layoutState() && view().layoutState()->isPaginated();
    if (paginated && layoutState.flowThread()) {
        // Check all lines from here to the end, and see if the hypothetical new position for the lines will result
        // in a different available line width.
        for (RootInlineBox* lineBox = layoutState.endLine(); lineBox; lineBox = lineBox->nextRootBox()) {
            if (paginated) {
                // This isn't the real move we're going to do, so don't update the line box's pagination
                // strut yet.
                LayoutUnit oldPaginationStrut = lineBox->paginationStrut();
                lineDelta -= oldPaginationStrut;
                adjustLinePositionForPagination(lineBox, lineDelta, layoutState.flowThread());
                lineBox->setPaginationStrut(oldPaginationStrut);
            }
            if (lineWidthForPaginatedLineChanged(lineBox, lineDelta, layoutState.flowThread()))
                return false;
        }
    }
    
    if (!lineDelta || !m_floatingObjects)
        return true;
    
    // See if any floats end in the range along which we want to shift the lines vertically.
    LayoutUnit logicalTop = std::min(logicalHeight(), layoutState.endLineLogicalTop());

    RootInlineBox* lastLine = layoutState.endLine();
    while (RootInlineBox* nextLine = lastLine->nextRootBox())
        lastLine = nextLine;

    LayoutUnit logicalBottom = lastLine->lineBottomWithLeading() + absoluteValue(lineDelta);

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    auto end = floatingObjectSet.end();
    for (auto it = floatingObjectSet.begin(); it != end; ++it) {
        FloatingObject* floatingObject = it->get();
        if (logicalBottomForFloat(floatingObject) >= logicalTop && logicalBottomForFloat(floatingObject) < logicalBottom)
            return false;
    }

    return true;
}

bool RenderBlockFlow::lineWidthForPaginatedLineChanged(RootInlineBox* rootBox, LayoutUnit lineDelta, RenderFlowThread* flowThread) const
{
    if (!flowThread)
        return false;

    RenderRegion* currentRegion = regionAtBlockOffset(rootBox->lineTopWithLeading() + lineDelta);
    // Just bail if the region didn't change.
    if (rootBox->containingRegion() == currentRegion)
        return false;
    return rootBox->paginatedLineWidth() != availableLogicalWidthForContent(currentRegion);
}

bool RenderBlockFlow::matchedEndLine(LineLayoutState& layoutState, const InlineBidiResolver& resolver, const InlineIterator& endLineStart, const BidiStatus& endLineStatus)
{
    if (resolver.position() == endLineStart) {
        if (resolver.status() != endLineStatus)
            return false;
        return checkPaginationAndFloatsAtEndLine(layoutState);
    }

    // The first clean line doesn't match, but we can check a handful of following lines to try
    // to match back up.
    static int numLines = 8; // The # of lines we're willing to match against.
    RootInlineBox* originalEndLine = layoutState.endLine();
    RootInlineBox* line = originalEndLine;
    for (int i = 0; i < numLines && line; i++, line = line->nextRootBox()) {
        if (line->lineBreakObj() == resolver.position().renderer() && line->lineBreakPos() == resolver.position().offset()) {
            // We have a match.
            if (line->lineBreakBidiStatus() != resolver.status())
                return false; // ...but the bidi state doesn't match.
            
            bool matched = false;
            RootInlineBox* result = line->nextRootBox();
            layoutState.setEndLine(result);
            if (result) {
                layoutState.setEndLineLogicalTop(line->lineBottomWithLeading());
                matched = checkPaginationAndFloatsAtEndLine(layoutState);
            }

            // Now delete the lines that we failed to sync.
            deleteLineRange(layoutState, originalEndLine, result);
            return matched;
        }
    }

    return false;
}

bool RenderBlock::generatesLineBoxesForInlineChild(RenderObject* inlineObj)
{
    ASSERT(inlineObj->parent() == this);

    InlineIterator it(this, inlineObj, 0);
    // FIXME: We should pass correct value for WhitespacePosition.
    while (!it.atEnd() && !requiresLineBox(it))
        it.increment();

    return !it.atEnd();
}

void RenderBlockFlow::addOverflowFromInlineChildren()
{
    if (auto layout = simpleLineLayout()) {
        ASSERT(!hasOverflowClip());
        SimpleLineLayout::collectFlowOverflow(*this, *layout);
        return;
    }
    LayoutUnit endPadding = hasOverflowClip() ? paddingEnd() : LayoutUnit();
    // FIXME: Need to find another way to do this, since scrollbars could show when we don't want them to.
    if (hasOverflowClip() && !endPadding && element() && element()->isRootEditableElement() && style().isLeftToRightDirection())
        endPadding = 1;
    for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
        addLayoutOverflow(curr->paddedLayoutOverflowRect(endPadding));
        RenderRegion* region = curr->containingRegion();
        if (region)
            region->addLayoutOverflowForBox(this, curr->paddedLayoutOverflowRect(endPadding));
        if (!hasOverflowClip()) {
            addVisualOverflow(curr->visualOverflowRect(curr->lineTop(), curr->lineBottom()));
            if (region)
                region->addVisualOverflowForBox(this, curr->visualOverflowRect(curr->lineTop(), curr->lineBottom()));
        }
    }
}

void RenderBlockFlow::deleteEllipsisLineBoxes()
{
    ETextAlign textAlign = style().textAlign();
    bool ltr = style().isLeftToRightDirection();
    bool firstLine = true;
    for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
        if (curr->hasEllipsisBox()) {
            curr->clearTruncation();

            // Shift the line back where it belongs if we cannot accomodate an ellipsis.
            float logicalLeft = pixelSnappedLogicalLeftOffsetForLine(curr->lineTop(), firstLine);
            float availableLogicalWidth = logicalRightOffsetForLine(curr->lineTop(), false) - logicalLeft;
            float totalLogicalWidth = curr->logicalWidth();
            updateLogicalWidthForAlignment(textAlign, 0, logicalLeft, totalLogicalWidth, availableLogicalWidth, 0);

            if (ltr)
                curr->adjustLogicalPosition((logicalLeft - curr->logicalLeft()), 0);
            else
                curr->adjustLogicalPosition(-(curr->logicalLeft() - logicalLeft), 0);
        }
        firstLine = false;
    }
}

void RenderBlockFlow::checkLinesForTextOverflow()
{
    // Determine the width of the ellipsis using the current font.
    // FIXME: CSS3 says this is configurable, also need to use 0x002E (FULL STOP) if horizontal ellipsis is "not renderable"
    const Font& font = style().font();
    DEFINE_STATIC_LOCAL(AtomicString, ellipsisStr, (&horizontalEllipsis, 1));
    const Font& firstLineFont = firstLineStyle().font();
    int firstLineEllipsisWidth = firstLineFont.width(constructTextRun(this, firstLineFont, &horizontalEllipsis, 1, firstLineStyle()));
    int ellipsisWidth = (font == firstLineFont) ? firstLineEllipsisWidth : font.width(constructTextRun(this, font, &horizontalEllipsis, 1, style()));

    // For LTR text truncation, we want to get the right edge of our padding box, and then we want to see
    // if the right edge of a line box exceeds that.  For RTL, we use the left edge of the padding box and
    // check the left edge of the line box to see if it is less
    // Include the scrollbar for overflow blocks, which means we want to use "contentWidth()"
    bool ltr = style().isLeftToRightDirection();
    ETextAlign textAlign = style().textAlign();
    bool firstLine = true;
    for (RootInlineBox* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
        // FIXME: Use pixelSnappedLogicalRightOffsetForLine instead of snapping it ourselves once the column workaround in said method has been fixed.
        // https://bugs.webkit.org/show_bug.cgi?id=105461
        int blockRightEdge = snapSizeToPixel(logicalRightOffsetForLine(curr->lineTop(), firstLine), curr->x());
        int blockLeftEdge = pixelSnappedLogicalLeftOffsetForLine(curr->lineTop(), firstLine);
        int lineBoxEdge = ltr ? snapSizeToPixel(curr->x() + curr->logicalWidth(), curr->x()) : snapSizeToPixel(curr->x(), 0);
        if ((ltr && lineBoxEdge > blockRightEdge) || (!ltr && lineBoxEdge < blockLeftEdge)) {
            // This line spills out of our box in the appropriate direction.  Now we need to see if the line
            // can be truncated.  In order for truncation to be possible, the line must have sufficient space to
            // accommodate our truncation string, and no replaced elements (images, tables) can overlap the ellipsis
            // space.

            LayoutUnit width = firstLine ? firstLineEllipsisWidth : ellipsisWidth;
            LayoutUnit blockEdge = ltr ? blockRightEdge : blockLeftEdge;
            if (curr->lineCanAccommodateEllipsis(ltr, blockEdge, lineBoxEdge, width)) {
                float totalLogicalWidth = curr->placeEllipsis(ellipsisStr, ltr, blockLeftEdge, blockRightEdge, width);

                float logicalLeft = 0; // We are only interested in the delta from the base position.
                float truncatedWidth = pixelSnappedLogicalRightOffsetForLine(curr->lineTop(), firstLine);
                updateLogicalWidthForAlignment(textAlign, 0, logicalLeft, totalLogicalWidth, truncatedWidth, 0);
                if (ltr)
                    curr->adjustLogicalPosition(logicalLeft, 0);
                else
                    curr->adjustLogicalPosition(-(truncatedWidth - (logicalLeft + totalLogicalWidth)), 0);
            }
        }
        firstLine = false;
    }
}

bool RenderBlockFlow::positionNewFloatOnLine(FloatingObject* newFloat, FloatingObject* lastFloatFromPreviousLine, LineInfo& lineInfo, LineWidth& width)
{
    if (!positionNewFloats())
        return false;

    width.shrinkAvailableWidthForNewFloatIfNeeded(newFloat);

    // We only connect floats to lines for pagination purposes if the floats occur at the start of
    // the line and the previous line had a hard break (so this line is either the first in the block
    // or follows a <br>).
    if (!newFloat->paginationStrut() || !lineInfo.previousLineBrokeCleanly() || !lineInfo.isEmpty())
        return true;

    const FloatingObjectSet& floatingObjectSet = m_floatingObjects->set();
    ASSERT(floatingObjectSet.last().get() == newFloat);

    LayoutUnit floatLogicalTop = logicalTopForFloat(newFloat);
    int paginationStrut = newFloat->paginationStrut();

    if (floatLogicalTop - paginationStrut != logicalHeight() + lineInfo.floatPaginationStrut())
        return true;

    auto it = floatingObjectSet.end();
    --it; // Last float is newFloat, skip that one.
    auto begin = floatingObjectSet.begin();
    while (it != begin) {
        --it;
        FloatingObject* floatingObject = it->get();
        if (floatingObject == lastFloatFromPreviousLine)
            break;
        if (logicalTopForFloat(floatingObject) == logicalHeight() + lineInfo.floatPaginationStrut()) {
            floatingObject->setPaginationStrut(paginationStrut + floatingObject->paginationStrut());
            RenderBox& floatBox = floatingObject->renderer();
            setLogicalTopForChild(floatBox, logicalTopForChild(floatBox) + marginBeforeForChild(floatBox) + paginationStrut);

            if (updateRegionRangeForBoxChild(floatingObject->renderer()))
                floatBox.setNeedsLayout(MarkOnlyThis);
            else if (floatBox.isRenderBlock())
                toRenderBlock(floatBox).setChildNeedsLayout(MarkOnlyThis);
            floatBox.layoutIfNeeded();

            // Save the old logical top before calling removePlacedObject which will set
            // isPlaced to false. Otherwise it will trigger an assert in logicalTopForFloat.
            LayoutUnit oldLogicalTop = logicalTopForFloat(floatingObject);
            m_floatingObjects->removePlacedObject(floatingObject);
            setLogicalTopForFloat(floatingObject, oldLogicalTop + paginationStrut);
            m_floatingObjects->addPlacedObject(floatingObject);
        }
    }

    // Just update the line info's pagination strut without altering our logical height yet. If the line ends up containing
    // no content, then we don't want to improperly grow the height of the block.
    lineInfo.setFloatPaginationStrut(lineInfo.floatPaginationStrut() + paginationStrut);
    return true;
}

LayoutUnit RenderBlockFlow::startAlignedOffsetForLine(LayoutUnit position, bool firstLine)
{
    ETextAlign textAlign = style().textAlign();

    // <rdar://problem/15427571>
    // https://bugs.webkit.org/show_bug.cgi?id=124522
    // This quirk is for legacy content that doesn't work properly with the center positioning scheme
    // being honored (e.g., epubs).
    if (textAlign == TASTART || document().settings()->useLegacyTextAlignPositionedElementBehavior()) // FIXME: Handle TAEND here
        return startOffsetForLine(position, firstLine);

    // updateLogicalWidthForAlignment() handles the direction of the block so no need to consider it here
    float totalLogicalWidth = 0;
    float logicalLeft = logicalLeftOffsetForLine(logicalHeight(), false);
    float availableLogicalWidth = logicalRightOffsetForLine(logicalHeight(), false) - logicalLeft;
    updateLogicalWidthForAlignment(textAlign, 0, logicalLeft, totalLogicalWidth, availableLogicalWidth, 0);

    if (!style().isLeftToRightDirection())
        return logicalWidth() - logicalLeft;
    return logicalLeft;
}

void RenderBlockFlow::updateRegionForLine(RootInlineBox* lineBox) const
{
    ASSERT(lineBox);

    if (auto containingRegion = regionAtBlockOffset(lineBox->lineTopWithLeading()))
        lineBox->setContainingRegion(*containingRegion);
    else
        lineBox->clearContainingRegion();

    RootInlineBox* prevLineBox = lineBox->prevRootBox();
    if (!prevLineBox)
        return;

    // This check is more accurate than the one in |adjustLinePositionForPagination| because it takes into
    // account just the container changes between lines. The before mentioned function doesn't set the flag
    // correctly if the line is positioned at the top of the last fragment container.
    if (lineBox->containingRegion() != prevLineBox->containingRegion())
        lineBox->setIsFirstAfterPageBreak(true);
}

}
