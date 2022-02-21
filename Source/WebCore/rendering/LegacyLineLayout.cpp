/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003-2019 Apple Inc. All right reserved.
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
#include "HTMLParserIdioms.h"
#include "InlineIteratorBox.h"
#include "InlineIteratorTextBox.h"
#include "InlineTextBoxStyle.h"
#include "InlineWalker.h"
#include "LegacyInlineElementBox.h"
#include "LegacyInlineIterator.h"
#include "LegacyInlineTextBox.h"
#include "LineLayoutState.h"
#include "Logging.h"
#include "RenderBlockFlow.h"
#include "RenderFragmentContainer.h"
#include "RenderFragmentedFlow.h"
#include "RenderLayoutState.h"
#include "RenderLineBreak.h"
#include "RenderRubyBase.h"
#include "RenderRubyText.h"
#include "RenderSVGText.h"
#include "RenderView.h"
#include "SVGElementTypeHelpers.h"
#include "SVGRootInlineBox.h"
#include "Settings.h"
#include "VerticalPositionCache.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

LegacyLineLayout::LegacyLineLayout(RenderBlockFlow& flow)
    : m_flow(flow)
{
}

LegacyLineLayout::~LegacyLineLayout()
{
    if (m_flow.containsFloats())
        m_flow.floatingObjects()->clearLineBoxTreePointers();

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
    if (is<RenderSVGText>(m_flow)) {
        auto box = makeUnique<SVGRootInlineBox>(downcast<RenderSVGText>(m_flow));
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

LegacyInlineBox* LegacyLineLayout::createInlineBoxForRenderer(RenderObject* renderer, bool isOnlyRun)
{
    if (renderer == &m_flow)
        return createAndAppendRootInlineBox();

    if (is<RenderText>(*renderer))
        return downcast<RenderText>(*renderer).createInlineTextBox();

    if (is<RenderBox>(*renderer)) {
        // FIXME: This is terrible. This branch returns an *owned* pointer!
        return downcast<RenderBox>(*renderer).createInlineBox().release();
    }

    if (is<RenderLineBreak>(*renderer)) {
        // FIXME: This is terrible. This branch returns an *owned* pointer!
        auto inlineBox = downcast<RenderLineBreak>(*renderer).createInlineBox().release();
        // We only treat a box as text for a <br> if we are on a line by ourself or in strict mode
        // (Note the use of strict mode. In "almost strict" mode, we don't treat the box for <br> as text.)
        inlineBox->setBehavesLikeText(isOnlyRun || renderer->document().inNoQuirksMode() || renderer->isLineBreakOpportunity());
        return inlineBox;
    }

    return downcast<RenderInline>(*renderer).createAndAppendInlineFlowBox();
}

static inline void dirtyLineBoxesForRenderer(RenderObject& renderer, bool fullLayout)
{
    if (is<RenderText>(renderer)) {
        RenderText& renderText = downcast<RenderText>(renderer);
        updateCounterIfNeeded(renderText);
        renderText.dirtyLineBoxes(fullLayout);
    } else if (is<RenderLineBreak>(renderer))
        downcast<RenderLineBreak>(renderer).dirtyLineBoxes(fullLayout);
    else
        downcast<RenderInline>(renderer).dirtyLineBoxes(fullLayout);
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
    bool hasDefaultLineBoxContain = style().lineBoxContain() == RenderStyle::initialLineBoxContain();
    do {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(is<RenderInline>(*obj) || obj == &m_flow);

        RenderInline* inlineFlow = obj != &m_flow ? downcast<RenderInline>(obj) : nullptr;

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

template<typename CharacterType> static inline bool endsWithHTMLSpaces(const CharacterType* characters, unsigned position, unsigned end)
{
    for (unsigned i = position; i < end; ++i) {
        if (!isHTMLSpace(characters[i]))
            return false;
    }
    return true;
}

static bool reachedEndOfTextRenderer(const BidiRunList<BidiRun>& bidiRuns)
{
    BidiRun* run = bidiRuns.logicallyLastRun();
    if (!run)
        return true;
    if (!is<RenderText>(run->renderer()))
        return false;
    auto& text = downcast<RenderText>(run->renderer()).text();
    unsigned position = run->stop();
    unsigned length = text.length();
    if (text.is8Bit())
        return endsWithHTMLSpaces(text.characters8(), position, length);
    return endsWithHTMLSpaces(text.characters16(), position, length);
}

LegacyRootInlineBox* LegacyLineLayout::constructLine(BidiRunList<BidiRun>& bidiRuns, const LineInfo& lineInfo)
{
    ASSERT(bidiRuns.firstRun());

    LegacyInlineFlowBox* parentBox = 0;
    int runCount = bidiRuns.runCount() - lineInfo.runsFromLeadingWhitespace();
    
    for (BidiRun* r = bidiRuns.firstRun(); r; r = r->next()) {
        // Create a box for our object.
        bool isOnlyRun = (runCount == 1);
        if (runCount == 2 && !r->renderer().isListMarker())
            isOnlyRun = (!style().isLeftToRightDirection() ? bidiRuns.lastRun() : bidiRuns.firstRun())->renderer().isListMarker();

        if (lineInfo.isEmpty())
            continue;

        LegacyInlineBox* box = createInlineBoxForRenderer(&r->renderer(), isOnlyRun);
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

        if (is<LegacyInlineTextBox>(*box)) {
            auto& textBox = downcast<LegacyInlineTextBox>(*box);
            textBox.setStart(r->m_start);
            textBox.setLen(r->m_stop - r->m_start);
            if (r->m_hasHyphen)
                textBox.setHasHyphen(true);
        }
    }

    // We should have a root inline box. It should be unconstructed and
    // be the last continuation of our line list.
    ASSERT(lastRootBox() && !lastRootBox()->isConstructed());

    // Set bits on our inline flow boxes that indicate which sides should
    // paint borders/margins/padding. This knowledge will ultimately be used when
    // we determine the horizontal positions and widths of all the inline boxes on
    // the line.
    bool isLogicallyLastRunWrapped = bidiRuns.logicallyLastRun()->renderer().isText() ? !reachedEndOfTextRenderer(bidiRuns) : !is<RenderInline>(bidiRuns.logicallyLastRun()->renderer());
    lastRootBox()->determineSpacingForFlowBoxes(lineInfo.isLastLine(), isLogicallyLastRunWrapped, &bidiRuns.logicallyLastRun()->renderer());

    // Now mark the line boxes as being constructed.
    lastRootBox()->setConstructed();

    // Return the last line.
    return lastRootBox();
}

TextAlignMode LegacyLineLayout::textAlignmentForLine(bool endsWithSoftBreak) const
{
    if (auto overrideAlignment = m_flow.overrideTextAlignmentForLine(endsWithSoftBreak))
        return *overrideAlignment;

    TextAlignMode alignment = style().textAlign();
#if ENABLE(CSS3_TEXT)
    TextJustify textJustify = style().textJustify();
    if (alignment == TextAlignMode::Justify && textJustify == TextJustify::None)
        return style().direction() == TextDirection::LTR ? TextAlignMode::Left : TextAlignMode::Right;
#endif

    if (endsWithSoftBreak)
        return alignment;

#if !ENABLE(CSS3_TEXT)
    return (alignment == TextAlignMode::Justify) ? TextAlignMode::Start : alignment;
#else
    if (alignment != TextAlignMode::Justify)
        return alignment;

    TextAlignLast alignmentLast = style().textAlignLast();
    switch (alignmentLast) {
    case TextAlignLast::Start:
        return TextAlignMode::Start;
    case TextAlignLast::End:
        return TextAlignMode::End;
    case TextAlignLast::Left:
        return TextAlignMode::Left;
    case TextAlignLast::Right:
        return TextAlignMode::Right;
    case TextAlignLast::Center:
        return TextAlignMode::Center;
    case TextAlignLast::Justify:
        return TextAlignMode::Justify;
    case TextAlignLast::Auto:
        if (textJustify == TextJustify::Distribute)
            return TextAlignMode::Justify;
        return TextAlignMode::Start;
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

void LegacyLineLayout::setMarginsForRubyRun(BidiRun* run, RenderRubyRun& renderer, RenderObject* previousObject, const LineInfo& lineInfo)
{
    float startOverhang;
    float endOverhang;
    RenderObject* nextObject = 0;
    for (BidiRun* runWithNextObject = run->next(); runWithNextObject; runWithNextObject = runWithNextObject->next()) {
        if (!runWithNextObject->renderer().isOutOfFlowPositioned() && !runWithNextObject->box()->isLineBreak()) {
            nextObject = &runWithNextObject->renderer();
            break;
        }
    }
    renderer.getOverhang(lineInfo.isFirstLine(), renderer.style().isLeftToRightDirection() ? previousObject : nextObject, renderer.style().isLeftToRightDirection() ? nextObject : previousObject, startOverhang, endOverhang);
    m_flow.setMarginStartForChild(renderer, LayoutUnit(-startOverhang));
    m_flow.setMarginEndForChild(renderer, LayoutUnit(-endOverhang));
}

static inline void setLogicalWidthForTextRun(LegacyRootInlineBox* lineBox, BidiRun* run, RenderText& renderer, float xPos, const LineInfo& lineInfo,
    GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache& verticalPositionCache, WordMeasurements& wordMeasurements)
{
    HashSet<const Font*> fallbackFonts;
    GlyphOverflow glyphOverflow;

    const FontCascade& font = lineStyle(*renderer.parent(), lineInfo).fontCascade();
    // Always compute glyph overflow if the block's line-box-contain value is "glyphs".
    if (lineBox->fitsToGlyphs()) {
        // If we don't stick out of the root line's font box, then don't bother computing our glyph overflow. This optimization
        // will keep us from computing glyph bounds in nearly all cases.
        bool includeRootLine = lineBox->includesRootLineBoxFontOrLeading();
        int baselineShift = lineBox->verticalPositionForBox(run->box(), verticalPositionCache);
        int rootDescent = includeRootLine ? font.metricsOfPrimaryFont().descent() : 0;
        int rootAscent = includeRootLine ? font.metricsOfPrimaryFont().ascent() : 0;
        int boxAscent = font.metricsOfPrimaryFont().ascent() - baselineShift;
        int boxDescent = font.metricsOfPrimaryFont().descent() + baselineShift;
        if (boxAscent > rootDescent ||  boxDescent > rootAscent)
            glyphOverflow.computeBounds = true; 
    }
    
    LayoutUnit hyphenWidth;
    if (downcast<LegacyInlineTextBox>(*run->box()).hasHyphen())
        hyphenWidth = measureHyphenWidth(renderer, font, &fallbackFonts);

    float measuredWidth = 0;

    bool kerningIsEnabled = font.enableKerning();
    bool canUseSimpleFontCodePath = renderer.canUseSimpleFontCodePath();
    
    // Since we don't cache glyph overflows, we need to re-measure the run if
    // the style is linebox-contain: glyph.
    if (!lineBox->fitsToGlyphs() && canUseSimpleFontCodePath) {
        unsigned lastEndOffset = run->m_start;
        bool atFirstWordMeasurement = true;
        for (size_t i = 0, size = wordMeasurements.size(); i < size && lastEndOffset < run->m_stop; ++i) {
            WordMeasurement& wordMeasurement = wordMeasurements[i];
            if (wordMeasurement.width <= 0 || wordMeasurement.startOffset == wordMeasurement.endOffset)
                continue;
            if (wordMeasurement.renderer != &renderer || wordMeasurement.startOffset != lastEndOffset || wordMeasurement.endOffset > run->m_stop)
                continue;

            lastEndOffset = wordMeasurement.endOffset;
            if (kerningIsEnabled && lastEndOffset == run->m_stop) {
                int wordLength = lastEndOffset - wordMeasurement.startOffset;
                GlyphOverflow overflow;
                measuredWidth += renderer.width(wordMeasurement.startOffset, wordLength, xPos + measuredWidth, lineInfo.isFirstLine(),
                    &wordMeasurement.fallbackFonts, &overflow);
                UChar c = renderer.characterAt(wordMeasurement.startOffset);
                // renderer.width() omits word-spacing value for leading whitespace, so let's just add it back here.
                if (!atFirstWordMeasurement && FontCascade::treatAsSpace(c))
                    measuredWidth += renderer.style().fontCascade().wordSpacing();
            } else
                measuredWidth += wordMeasurement.width;
            atFirstWordMeasurement = false;

            for (auto& font : wordMeasurement.fallbackFonts)
                fallbackFonts.add(font);
        }
        if (measuredWidth && lastEndOffset != run->m_stop) {
            // If we don't have enough cached data, we'll measure the run again.
            measuredWidth = 0;
            fallbackFonts.clear();
        }
    }

    if (!measuredWidth)
        measuredWidth = renderer.width(run->m_start, run->m_stop - run->m_start, xPos, lineInfo.isFirstLine(), &fallbackFonts, &glyphOverflow);

    ASSERT(measuredWidth >= 0);
    ASSERT(hyphenWidth >= 0);

    run->box()->setLogicalWidth(measuredWidth + hyphenWidth);
    if (!fallbackFonts.isEmpty()) {
        ASSERT(run->box()->behavesLikeText());
        GlyphOverflowAndFallbackFontsMap::iterator it = textBoxDataMap.add(downcast<LegacyInlineTextBox>(run->box()), std::make_pair(Vector<const Font*>(), GlyphOverflow())).iterator;
        ASSERT(it->value.first.isEmpty());
        it->value.first = copyToVector(fallbackFonts);
        run->box()->parent()->clearDescendantsHaveSameLineHeightAndBaseline();
    }

    // Include text decoration visual overflow as part of the glyph overflow.
    if (!renderer.style().textDecorationsInEffect().isEmpty())
        glyphOverflow.extendTo(visualOverflowForDecorations(run->box()->lineStyle(), InlineIterator::textBoxFor(downcast<LegacyInlineTextBox>(run->box()))));

    if (!glyphOverflow.isEmpty()) {
        ASSERT(run->box()->behavesLikeText());
        GlyphOverflowAndFallbackFontsMap::iterator it = textBoxDataMap.add(downcast<LegacyInlineTextBox>(run->box()), std::make_pair(Vector<const Font*>(), GlyphOverflow())).iterator;
        it->value.second = glyphOverflow;
        run->box()->clearKnownToHaveNoOverflow();
    }
}

void LegacyLineLayout::updateRubyForJustifiedText(RenderRubyRun& rubyRun, BidiRun& r, const Vector<unsigned, 16>& expansionOpportunities, unsigned& expansionOpportunityCount, float& totalLogicalWidth, float availableLogicalWidth, size_t& i)
{
    if (!rubyRun.rubyBase() || !rubyRun.rubyBase()->firstRootBox() || rubyRun.rubyBase()->firstRootBox()->nextRootBox() || !r.renderer().style().collapseWhiteSpace())
        return;

    auto& rubyBase = *rubyRun.rubyBase();
    auto& rootBox = *rubyBase.firstRootBox();

    float totalExpansion = 0;
    unsigned totalOpportunitiesInRun = 0;
    for (auto* leafChild = rootBox.firstLeafDescendant(); leafChild; leafChild = leafChild->nextLeafOnLine()) {
        if (!leafChild->isInlineTextBox())
            continue;

        unsigned opportunitiesInRun = expansionOpportunities[i++];
        ASSERT(opportunitiesInRun <= expansionOpportunityCount);
        auto expansion = (availableLogicalWidth - totalLogicalWidth) * opportunitiesInRun / expansionOpportunityCount;
        totalExpansion += expansion;
        totalOpportunitiesInRun += opportunitiesInRun;
    }

    ASSERT(!rubyRun.hasOverridingLogicalWidth());
    float newBaseWidth = rubyRun.logicalWidth() + totalExpansion + m_flow.marginStartForChild(rubyRun) + m_flow.marginEndForChild(rubyRun);
    float newRubyRunWidth = rubyRun.logicalWidth() + totalExpansion;
    rubyBase.setInitialOffset((newRubyRunWidth - newBaseWidth) / 2);
    rubyRun.setOverridingLogicalWidth(LayoutUnit(newRubyRunWidth));
    rubyRun.setNeedsLayout(MarkOnlyThis);
    rootBox.markDirty();
    if (RenderRubyText* rubyText = rubyRun.rubyText()) {
        if (LegacyRootInlineBox* textRootBox = rubyText->firstRootBox())
            textRootBox->markDirty();
    }
    rubyRun.layoutBlock(true);
    rubyRun.clearOverridingLogicalWidth();
    r.box()->setExpansion(newRubyRunWidth - r.box()->logicalWidth());

    totalLogicalWidth += totalExpansion;
    expansionOpportunityCount -= totalOpportunitiesInRun;
}

void LegacyLineLayout::computeExpansionForJustifiedText(BidiRun* firstRun, BidiRun* trailingSpaceRun, const Vector<unsigned, 16>& expansionOpportunities, unsigned expansionOpportunityCount, float totalLogicalWidth, float availableLogicalWidth)
{
    if (!expansionOpportunityCount || availableLogicalWidth <= totalLogicalWidth)
        return;

    size_t i = 0;
    for (BidiRun* run = firstRun; run; run = run->next()) {
        if (!run->box() || run == trailingSpaceRun)
            continue;

        // Positioned objects are only participating to figure out their correct static x position.
        // They have no affect on the width. Similarly, line break boxes have no affect on the width.
        if (run->renderer().isOutOfFlowPositioned() || run->box()->isLineBreak())
            continue;

        if (is<RenderText>(run->renderer())) {
            unsigned opportunitiesInRun = expansionOpportunities[i++];
            
            ASSERT(opportunitiesInRun <= expansionOpportunityCount);
            
            // Only justify text if whitespace is collapsed.
            if (run->renderer().style().collapseWhiteSpace()) {
                LegacyInlineTextBox& textBox = downcast<LegacyInlineTextBox>(*run->box());
                float expansion = (availableLogicalWidth - totalLogicalWidth) * opportunitiesInRun / expansionOpportunityCount;
                textBox.setExpansion(expansion);
                totalLogicalWidth += expansion;
            }
            expansionOpportunityCount -= opportunitiesInRun;
        } else if (is<RenderRubyRun>(run->renderer()))
            updateRubyForJustifiedText(downcast<RenderRubyRun>(run->renderer()), *run, expansionOpportunities, expansionOpportunityCount, totalLogicalWidth, availableLogicalWidth, i);

        if (!expansionOpportunityCount)
            break;
    }
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
        flow.adjustInlineDirectionLineBounds(expansionOpportunityCount, logicalLeft, availableLogicalWidth);
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

static void updateLogicalInlinePositions(RenderBlockFlow& block, float& lineLogicalLeft, float& lineLogicalRight, float& availableLogicalWidth, bool firstLine,
    IndentTextOrNot shouldIndentText, LayoutUnit boxLogicalHeight)
{
    LayoutUnit lineLogicalHeight = block.minLineHeightForReplacedRenderer(firstLine, boxLogicalHeight);
    lineLogicalLeft = block.logicalLeftOffsetForLine(block.logicalHeight(), shouldIndentText, lineLogicalHeight);
    lineLogicalRight = block.logicalRightOffsetForLine(block.logicalHeight(), shouldIndentText, lineLogicalHeight);
    availableLogicalWidth = lineLogicalRight - lineLogicalLeft;
}

void LegacyLineLayout::computeInlineDirectionPositionsForLine(LegacyRootInlineBox* lineBox, const LineInfo& lineInfo, BidiRun* firstRun, BidiRun* trailingSpaceRun, bool reachedEnd, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache& verticalPositionCache, WordMeasurements& wordMeasurements)
{
    TextAlignMode textAlign = textAlignmentForLine(!reachedEnd && !lineBox->endsWithBreak());
    
    // CSS 2.1: "'Text-indent' only affects a line if it is the first formatted line of an element. For example, the first line of an anonymous block 
    // box is only affected if it is the first child of its parent element."
    // CSS3 "text-indent", "-webkit-each-line" affects the first line of the block container as well as each line after a forced line break,
    // but does not affect lines after a soft wrap break.
    bool isFirstLine = lineInfo.isFirstLine() && !(m_flow.isAnonymousBlock() && m_flow.parent()->firstChild() != &m_flow);
    bool isAfterHardLineBreak = lineBox->prevRootBox() && lineBox->prevRootBox()->endsWithBreak();
    IndentTextOrNot shouldIndentText = requiresIndent(isFirstLine, isAfterHardLineBreak, style());
    float lineLogicalLeft;
    float lineLogicalRight;
    float availableLogicalWidth;
    updateLogicalInlinePositions(m_flow, lineLogicalLeft, lineLogicalRight, availableLogicalWidth, isFirstLine, shouldIndentText, 0);
    bool needsWordSpacing;

    if (firstRun && firstRun->renderer().isReplacedOrInlineBlock()) {
        RenderBox& renderBox = downcast<RenderBox>(firstRun->renderer());
        updateLogicalInlinePositions(m_flow, lineLogicalLeft, lineLogicalRight, availableLogicalWidth, isFirstLine, shouldIndentText, renderBox.logicalHeight());
    }

    computeInlineDirectionPositionsForSegment(lineBox, lineInfo, textAlign, lineLogicalLeft, availableLogicalWidth, firstRun, trailingSpaceRun, textBoxDataMap, verticalPositionCache, wordMeasurements);
    // The widths of all runs are now known. We can now place every inline box (and
    // compute accurate widths for the inline flow boxes).
    needsWordSpacing = false;
    lineBox->placeBoxesInInlineDirection(lineLogicalLeft, needsWordSpacing);
}

static inline ExpansionBehavior expansionBehaviorForInlineTextBox(RenderBlockFlow& block, LegacyInlineTextBox& textBox, BidiRun* previousRun, BidiRun* nextRun, TextAlignMode textAlign, bool isAfterExpansion)
{
    // Tatechuyoko is modeled as the Object Replacement Character (U+FFFC), which can never have expansion opportunities inside nor intrinsically adjacent to it.
    if (textBox.renderer().style().textCombine() == TextCombine::All)
        return ForbidLeftExpansion | ForbidRightExpansion;

    ExpansionBehavior result = 0;
    bool setLeftExpansion = false;
    bool setRightExpansion = false;
    if (textAlign == TextAlignMode::Justify) {
        // If the next box is ruby, and we're justifying, and the first box in the ruby base has a leading expansion, and we are a text box, then force a trailing expansion.
        if (nextRun && is<RenderRubyRun>(nextRun->renderer()) && downcast<RenderRubyRun>(nextRun->renderer()).rubyBase() && nextRun->renderer().style().collapseWhiteSpace()) {
            auto& rubyBase = *downcast<RenderRubyRun>(nextRun->renderer()).rubyBase();
            if (rubyBase.firstRootBox() && !rubyBase.firstRootBox()->nextRootBox()) {
                if (auto* leafChild = rubyBase.firstRootBox()->firstLeafDescendant()) {
                    if (is<LegacyInlineTextBox>(*leafChild)) {
                        // FIXME: This leftExpansionOpportunity doesn't actually work because it doesn't perform the UBA
                        if (FontCascade::leftExpansionOpportunity(downcast<RenderText>(leafChild->renderer()).stringView(), leafChild->direction())) {
                            setRightExpansion = true;
                            result |= ForceRightExpansion;
                        }
                    }
                }
            }
        }
        // Same thing, except if we're following a ruby
        if (previousRun && is<RenderRubyRun>(previousRun->renderer()) && downcast<RenderRubyRun>(previousRun->renderer()).rubyBase() && previousRun->renderer().style().collapseWhiteSpace()) {
            auto& rubyBase = *downcast<RenderRubyRun>(previousRun->renderer()).rubyBase();
            if (rubyBase.firstRootBox() && !rubyBase.firstRootBox()->nextRootBox()) {
                if (auto* leafChild = rubyBase.firstRootBox()->lastLeafDescendant()) {
                    if (is<LegacyInlineTextBox>(*leafChild)) {
                        // FIXME: This leftExpansionOpportunity doesn't actually work because it doesn't perform the UBA
                        if (FontCascade::rightExpansionOpportunity(downcast<RenderText>(leafChild->renderer()).stringView(), leafChild->direction())) {
                            setLeftExpansion = true;
                            result |= ForceLeftExpansion;
                        }
                    }
                }
            }
        }
        // If we're the first box inside a ruby base, forbid a leading expansion, and vice-versa
        if (is<RenderRubyBase>(block)) {
            RenderRubyBase& rubyBase = downcast<RenderRubyBase>(block);
            if (&textBox == rubyBase.firstRootBox()->firstLeafDescendant()) {
                setLeftExpansion = true;
                result |= ForbidLeftExpansion;
            } if (&textBox == rubyBase.firstRootBox()->lastLeafDescendant()) {
                setRightExpansion = true;
                result |= ForbidRightExpansion;
            }
        }
    }
    if (!setLeftExpansion)
        result |= isAfterExpansion ? ForbidLeftExpansion : AllowLeftExpansion;
    if (!setRightExpansion)
        result |= AllowRightExpansion;
    return result;
}

static inline void applyExpansionBehavior(LegacyInlineTextBox& textBox, ExpansionBehavior expansionBehavior)
{
    switch (expansionBehavior & LeftExpansionMask) {
    case ForceLeftExpansion:
        textBox.setForceLeftExpansion();
        break;
    case ForbidLeftExpansion:
        textBox.setCanHaveLeftExpansion(false);
        break;
    case AllowLeftExpansion:
        textBox.setCanHaveLeftExpansion(true);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    switch (expansionBehavior & RightExpansionMask) {
    case ForceRightExpansion:
        textBox.setForceRightExpansion();
        break;
    case ForbidRightExpansion:
        textBox.setCanHaveRightExpansion(false);
        break;
    case AllowRightExpansion:
        textBox.setCanHaveRightExpansion(true);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

static bool inlineAncestorHasStartBorderPaddingOrMargin(const RenderBlockFlow& block, const LegacyInlineBox& box)
{
    bool isLTR = block.style().isLeftToRightDirection();
    for (auto* currentBox = box.parent(); currentBox; currentBox = currentBox->parent()) {
        if ((isLTR && currentBox->marginBorderPaddingLogicalLeft() > 0)
            || (!isLTR && currentBox->marginBorderPaddingLogicalRight() > 0))
            return true;
    }
    return false;
}

static bool inlineAncestorHasEndBorderPaddingOrMargin(const RenderBlockFlow& block, const LegacyInlineBox& box)
{
    bool isLTR = block.style().isLeftToRightDirection();
    for (auto* currentBox = box.parent(); currentBox; currentBox = currentBox->parent()) {
        if ((isLTR && currentBox->marginBorderPaddingLogicalRight() > 0)
            || (!isLTR && currentBox->marginBorderPaddingLogicalLeft() > 0))
            return true;
    }
    return false;
}
    
static bool isLastInFlowRun(BidiRun& runToCheck)
{
    for (auto* run = runToCheck.next(); run; run = run->next()) {
        if (!run->box() || run->renderer().isOutOfFlowPositioned() || run->box()->isLineBreak())
            continue;
        return false;
    }
    return true;
}

BidiRun* LegacyLineLayout::computeInlineDirectionPositionsForSegment(LegacyRootInlineBox* lineBox, const LineInfo& lineInfo, TextAlignMode textAlign, float& lineLogicalLeft,
    float& availableLogicalWidth, BidiRun* firstRun, BidiRun* trailingSpaceRun, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache& verticalPositionCache,
    WordMeasurements& wordMeasurements)
{
    bool needsWordSpacing = false;
    bool canHangPunctuationAtStart = style().hangingPunctuation().contains(HangingPunctuation::First);
    bool canHangPunctuationAtEnd = style().hangingPunctuation().contains(HangingPunctuation::Last);
    bool isLTR = style().isLeftToRightDirection();
    float contentWidth = 0;
    unsigned expansionOpportunityCount = 0;
    bool isAfterExpansion = is<RenderRubyBase>(m_flow) ? downcast<RenderRubyBase>(m_flow).isAfterExpansion() : true;
    Vector<unsigned, 16> expansionOpportunities;

    HashMap<LegacyInlineTextBox*, LayoutUnit> logicalSpacingForInlineTextBoxes;
    auto collectSpacingLogicalWidths = [&] () {
        auto totalSpacingWidth = LayoutUnit { };
        // Collect the spacing positions (margin, border padding) for the textboxes by traversing the inline tree of the current line.
        Vector<LegacyInlineBox*> queue;
        queue.append(lineBox);
        // 1. Visit each inline box in a preorder fashion
        // 2. Accumulate the spacing when we find an LegacyInlineFlowBox (inline container e.g. span)
        // 3. Add the LegacyInlineTextBoxes to the hashmap
        while (!queue.isEmpty()) {
            while (true) {
                auto* inlineBox = queue.last();
                if (is<LegacyInlineFlowBox>(inlineBox)) {
                    auto& inlineFlowBox = downcast<LegacyInlineFlowBox>(*inlineBox);
                    totalSpacingWidth += inlineFlowBox.marginBorderPaddingLogicalLeft();
                    if (auto* child = inlineFlowBox.firstChild()) {
                        queue.append(child);
                        continue;
                    }
                    break;
                }
                if (is<LegacyInlineTextBox>(inlineBox))
                    logicalSpacingForInlineTextBoxes.add(downcast<LegacyInlineTextBox>(inlineBox), totalSpacingWidth);
                break;
            }
            while (!queue.isEmpty()) {
                auto& inlineBox = *queue.takeLast();
                if (is<LegacyInlineFlowBox>(inlineBox))
                    totalSpacingWidth += downcast<LegacyInlineFlowBox>(inlineBox).marginBorderPaddingLogicalRight();
                if (auto* nextSibling = inlineBox.nextOnLine()) {
                    queue.append(nextSibling);
                    break;
                }
            }
        }
    };
    collectSpacingLogicalWidths();

    BidiRun* run = firstRun;
    BidiRun* previousRun = nullptr;
    for (; run; run = run->next()) {
        auto computeExpansionOpportunities = [&expansionOpportunities, &expansionOpportunityCount, textAlign, &isAfterExpansion] (RenderBlockFlow& block,
            LegacyInlineTextBox& textBox, BidiRun* previousRun, BidiRun* nextRun, StringView stringView, TextDirection direction)
        {
            if (stringView.isEmpty()) {
                // Empty runs should still produce an entry in expansionOpportunities list so that the number of items matches the number of runs.
                expansionOpportunities.append(0);
                return;
            }
            ExpansionBehavior expansionBehavior = expansionBehaviorForInlineTextBox(block, textBox, previousRun, nextRun, textAlign, isAfterExpansion);
            applyExpansionBehavior(textBox, expansionBehavior);
            unsigned opportunitiesInRun;
            std::tie(opportunitiesInRun, isAfterExpansion) = FontCascade::expansionOpportunityCount(stringView, direction, expansionBehavior);
            expansionOpportunities.append(opportunitiesInRun);
            expansionOpportunityCount += opportunitiesInRun;
        };
        if (!run->box() || run->renderer().isOutOfFlowPositioned() || run->box()->isLineBreak()) {
            // Positioned objects are only participating to figure out their correct static x position.
            // They have no effect on the width. Similarly, line break boxes have no effect on the width.
            continue;
        }
        if (is<RenderText>(run->renderer())) {
            auto& renderText = downcast<RenderText>(run->renderer());
            auto& textBox = downcast<LegacyInlineTextBox>(*run->box());
            if (canHangPunctuationAtStart && lineInfo.isFirstLine() && (isLTR || isLastInFlowRun(*run))
                && !inlineAncestorHasStartBorderPaddingOrMargin(m_flow, *run->box())) {
                float hangStartWidth = renderText.hangablePunctuationStartWidth(run->m_start);
                availableLogicalWidth += hangStartWidth;
                if (style().isLeftToRightDirection())
                    lineLogicalLeft -= hangStartWidth;
                canHangPunctuationAtStart = false;
            }
            
            if (canHangPunctuationAtEnd && lineInfo.isLastLine() && run->m_stop > 0 && (!isLTR || isLastInFlowRun(*run))
                && !inlineAncestorHasEndBorderPaddingOrMargin(m_flow, *run->box())) {
                float hangEndWidth = renderText.hangablePunctuationEndWidth(run->m_stop - 1);
                availableLogicalWidth += hangEndWidth;
                if (!style().isLeftToRightDirection())
                    lineLogicalLeft -= hangEndWidth;
                canHangPunctuationAtEnd = false;
            }
            
            if (textAlign == TextAlignMode::Justify && run != trailingSpaceRun)
                computeExpansionOpportunities(m_flow, textBox, previousRun, run->next(), renderText.stringView(run->m_start, run->m_stop), run->box()->direction());

            if (unsigned length = renderText.text().length()) {
                if (!run->m_start && needsWordSpacing && isSpaceOrNewline(renderText.characterAt(run->m_start)))
                    contentWidth += lineStyle(*renderText.parent(), lineInfo).fontCascade().wordSpacing();
                // run->m_start == run->m_stop should only be true iff the run is a replaced run for bidi: isolate.
                ASSERT(run->m_stop > 0 || run->m_start == run->m_stop);
                needsWordSpacing = run->m_stop == length && !isSpaceOrNewline(renderText.characterAt(run->m_stop - 1));
            }
            auto currentLogicalLeftPosition = logicalSpacingForInlineTextBoxes.get(&textBox) + contentWidth;
            setLogicalWidthForTextRun(lineBox, run, renderText, currentLogicalLeftPosition, lineInfo, textBoxDataMap, verticalPositionCache, wordMeasurements);
        } else {
            canHangPunctuationAtStart = false;
            bool encounteredJustifiedRuby = false;
            if (is<RenderRubyRun>(run->renderer()) && textAlign == TextAlignMode::Justify && run != trailingSpaceRun && downcast<RenderRubyRun>(run->renderer()).rubyBase()) {
                auto* rubyBase = downcast<RenderRubyRun>(run->renderer()).rubyBase();
                if (rubyBase->firstRootBox() && !rubyBase->firstRootBox()->nextRootBox() && run->renderer().style().collapseWhiteSpace()) {
                    rubyBase->setIsAfterExpansion(isAfterExpansion);
                    for (auto* leafChild = rubyBase->firstRootBox()->firstLeafDescendant(); leafChild; leafChild = leafChild->nextLeafOnLine()) {
                        if (!is<LegacyInlineTextBox>(*leafChild))
                            continue;
                        encounteredJustifiedRuby = true;
                        computeExpansionOpportunities(*rubyBase, downcast<LegacyInlineTextBox>(*leafChild), nullptr, nullptr,
                            downcast<RenderText>(leafChild->renderer()).stringView(), leafChild->direction());
                    }
                }
            }

            if (!encounteredJustifiedRuby)
                isAfterExpansion = false;

            if (!is<RenderInline>(run->renderer())) {
                auto& renderBox = downcast<RenderBox>(run->renderer());
                if (is<RenderRubyRun>(renderBox))
                    setMarginsForRubyRun(run, downcast<RenderRubyRun>(renderBox), previousRun ? &previousRun->renderer() : nullptr, lineInfo);
                run->box()->setLogicalWidth(m_flow.logicalWidthForChild(renderBox));
                contentWidth += m_flow.marginStartForChild(renderBox) + m_flow.marginEndForChild(renderBox);
            }
        }

        contentWidth += run->box()->logicalWidth();
        previousRun = run;
    }

    if (isAfterExpansion && !expansionOpportunities.isEmpty()) {
        // FIXME: see <webkit.org/b/139393#c11>
        int lastValidExpansionOpportunitiesIndex = expansionOpportunities.size() - 1;
        while (lastValidExpansionOpportunitiesIndex >= 0 && !expansionOpportunities.at(lastValidExpansionOpportunitiesIndex))
            --lastValidExpansionOpportunitiesIndex;
        if (lastValidExpansionOpportunitiesIndex >= 0) {
            ASSERT(expansionOpportunities.at(lastValidExpansionOpportunitiesIndex));
            expansionOpportunities.at(lastValidExpansionOpportunitiesIndex)--;
            expansionOpportunityCount--;
        }
    }

    if (is<RenderRubyBase>(m_flow) && !expansionOpportunityCount)
        textAlign = TextAlignMode::Center;

    auto totalLogicalWidth = contentWidth + lineBox->getFlowSpacingLogicalWidth();
    updateLogicalWidthForAlignment(m_flow, textAlign, lineBox, trailingSpaceRun, lineLogicalLeft, totalLogicalWidth, availableLogicalWidth, expansionOpportunityCount);
    computeExpansionForJustifiedText(firstRun, trailingSpaceRun, expansionOpportunities, expansionOpportunityCount, totalLogicalWidth, availableLogicalWidth);
    return run;
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
    if (is<RenderText>(renderer))
        downcast<RenderText>(renderer).removeTextBox(downcast<LegacyInlineTextBox>(*inlineBox));
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

void LegacyLineLayout::computeBlockDirectionPositionsForLine(LegacyRootInlineBox* lineBox, BidiRun* firstRun, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache& verticalPositionCache)
{
    m_flow.setLogicalHeight(lineBox->alignBoxesInBlockDirection(m_flow.logicalHeight(), textBoxDataMap, verticalPositionCache));

    // Now make sure we place replaced render objects correctly.
    for (auto* run = firstRun; run; run = run->next()) {
        ASSERT(run->box());
        if (!run->box())
            continue; // Skip runs with no line boxes.

        // Align positioned boxes with the top of the line box. This is
        // a reasonable approximation of an appropriate y position.
        auto& renderer = run->renderer();
        if (renderer.isOutOfFlowPositioned())
            run->box()->setLogicalTop(m_flow.logicalHeight());

        // Position is used to properly position both replaced elements and
        // to update the static normal flow x/y of positioned elements.
        bool inlineBoxIsRedundant = false;
        if (is<RenderText>(renderer)) {
            auto& inlineTextBox = downcast<LegacyInlineTextBox>(*run->box());
            downcast<RenderText>(renderer).positionLineBox(inlineTextBox);
            inlineBoxIsRedundant = !inlineTextBox.hasTextContent();
        } else if (is<RenderBox>(renderer)) {
            downcast<RenderBox>(renderer).positionLineBox(downcast<LegacyInlineElementBox>(*run->box()));
            inlineBoxIsRedundant = renderer.isOutOfFlowPositioned();
        } else if (is<RenderLineBreak>(renderer))
            downcast<RenderLineBreak>(renderer).replaceInlineBoxWrapper(downcast<LegacyInlineElementBox>(*run->box()));
        // Check if we need to keep this box on the line at all.
        if (inlineBoxIsRedundant)
            removeInlineBox(*run, *lineBox);
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
    const RenderObject& lastObject = trailingSpaceRun->renderer();
    if (!is<RenderText>(lastObject))
        return nullptr;

    const RenderText& lastText = downcast<RenderText>(lastObject);
    unsigned firstSpace;
    if (lastText.text().is8Bit())
        firstSpace = findFirstTrailingSpace(lastText, lastText.text().characters8(), trailingSpaceRun->start(), trailingSpaceRun->stop());
    else
        firstSpace = findFirstTrailingSpace(lastText, lastText.text().characters16(), trailingSpaceRun->start(), trailingSpaceRun->stop());

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

void LegacyLineLayout::appendFloatingObjectToLastLine(FloatingObject& floatingObject)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!floatingObject.originatingLine());
    ASSERT(lastRootBox());
    floatingObject.setOriginatingLine(*lastRootBox());
    lastRootBox()->appendFloat(floatingObject.renderer());
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
    RenderObject* currentRoot = topResolver.position().root();
    topResolver.createBidiRunsForLine(endOfRuns, override, previousLineBrokeCleanly);

    while (!topResolver.isolatedRuns().isEmpty()) {
        // It does not matter which order we resolve the runs as long as we resolve them all.
        auto isolatedRun = WTFMove(topResolver.isolatedRuns().last());
        topResolver.isolatedRuns().removeLast();
        currentRoot = &isolatedRun.root;

        RenderObject& startObject = isolatedRun.object;

        // Only inlines make sense with unicode-bidi: isolate (blocks are already isolated).
        // FIXME: Because enterIsolate is not passed a RenderObject, we have to crawl up the
        // tree to see which parent inline is the isolate. We could change enterIsolate
        // to take a RenderObject and do this logic there, but that would be a layering
        // violation for BidiResolver (which knows nothing about RenderObject).
        RenderInline* isolatedInline = downcast<RenderInline>(highestContainingIsolateWithinRoot(startObject, currentRoot));
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
LegacyRootInlineBox* LegacyLineLayout::createLineBoxesFromBidiRuns(unsigned bidiLevel, BidiRunList<BidiRun>& bidiRuns, const LegacyInlineIterator& end, LineInfo& lineInfo, VerticalPositionCache& verticalPositionCache, BidiRun* trailingSpaceRun, WordMeasurements& wordMeasurements)
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
    
    GlyphOverflowAndFallbackFontsMap textBoxDataMap;
    
    // Now we position all of our text runs horizontally.
    if (!isSVGRootInlineBox)
        computeInlineDirectionPositionsForLine(lineBox, lineInfo, bidiRuns.firstRun(), trailingSpaceRun, end.atEnd(), textBoxDataMap, verticalPositionCache, wordMeasurements);
    
    // Now position our text runs vertically.
    computeBlockDirectionPositionsForLine(lineBox, bidiRuns.firstRun(), textBoxDataMap, verticalPositionCache);
    
    // SVG text layout code computes vertical & horizontal positions on its own.
    // Note that we still need to execute computeVerticalPositionsForLine() as
    // it calls LegacyInlineTextBox::positionLineBox(), which tracks whether the box
    // contains reversed text or not. If we wouldn't do that editing and thus
    // text selection in RTL boxes would not work as expected.
    if (isSVGRootInlineBox) {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(m_flow.isSVGText());
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

static void repaintDirtyFloats(LineLayoutState::FloatList& floats)
{
    // Floats that did not have layout did not repaint when we laid them out. They would have
    // painted by now if they had moved, but if they stayed at (0, 0), they still need to be
    // painted.
    for (auto& floatBox : floats) {
        if (floatBox->everHadLayout())
            continue;
        auto& box = floatBox->renderer();
        if (!box.x() && !box.y() && box.checkForRepaintDuringLayout())
            box.repaint();
    }
}

void LegacyLineLayout::layoutRunsAndFloats(LineLayoutState& layoutState, bool hasInlineChild)
{
    // We want to skip ahead to the first dirty line
    InlineBidiResolver resolver;
    LegacyRootInlineBox* startLine = determineStartPosition(layoutState, resolver);
    
    unsigned consecutiveHyphenatedLines = 0;
    if (startLine) {
        for (auto* line = startLine->prevRootBox(); line && line->isHyphenated(); line = line->prevRootBox())
            consecutiveHyphenatedLines++;
    }

    // FIXME: This would make more sense outside of this function, but since
    // determineStartPosition can change the fullLayout flag we have to do this here. Failure to call
    // determineStartPosition first will break fast/repaint/line-flow-with-floats-9.html.
    if (layoutState.isFullLayout() && hasInlineChild && !m_flow.selfNeedsLayout()) {
        m_flow.setNeedsLayout(MarkOnlyThis); // Mark as needing a full layout to force us to repaint.
        if (!layoutContext().needsFullRepaint() && m_flow.layerRepaintRects()) {
            // Because we waited until we were already inside layout to discover
            // that the block really needed a full layout, we missed our chance to repaint the layer
            // before layout started. Luckily the layer has cached the repaint rect for its original
            // position and size, and so we can use that to make a repaint happen now.
            m_flow.repaintUsingContainer(m_flow.containerForRepaint(), m_flow.layerRepaintRects()->clippedOverflowRect);
        }
    }

    if (m_flow.containsFloats())
        layoutState.floatList().setLastFloat(m_flow.floatingObjects()->set().last().get());

    // We also find the first clean line and extract these lines. We will add them back
    // if we determine that we're able to synchronize after handling all our dirty lines.
    LegacyInlineIterator cleanLineStart;
    BidiStatus cleanLineBidiStatus;
    if (!layoutState.isFullLayout() && startLine)
        determineEndPosition(layoutState, startLine, cleanLineStart, cleanLineBidiStatus);

    if (startLine) {
        if (!layoutState.usesRepaintBounds())
            layoutState.setRepaintRange(m_flow.logicalHeight());
        deleteLineRange(layoutState, startLine);
    }

    if (!layoutState.isFullLayout() && lastRootBox() && lastRootBox()->endsWithBreak()) {
        // If the last line before the start line ends with a line break that clear floats,
        // adjust the height accordingly.
        // A line break can be either the first or the last object on a line, depending on its direction.
        if (LegacyInlineBox* lastLeafDescendant = lastRootBox()->lastLeafDescendant()) {
            RenderObject* lastObject = &lastLeafDescendant->renderer();
            if (!lastObject->isBR())
                lastObject = &lastRootBox()->firstLeafDescendant()->renderer();
            if (lastObject->isBR()) {
                auto clear = RenderStyle::usedClear(*lastObject);
                if (clear != UsedClear::None)
                    m_flow.clearFloats(clear);
            }
        }
    }

    layoutRunsAndFloatsInRange(layoutState, resolver, cleanLineStart, cleanLineBidiStatus, consecutiveHyphenatedLines);
    linkToEndLineIfNeeded(layoutState);
    repaintDirtyFloats(layoutState.floatList());
}

// Before restarting the layout loop with a new logicalHeight, remove all floats that were added and reset the resolver.
inline const LegacyInlineIterator& LegacyLineLayout::restartLayoutRunsAndFloatsInRange(LayoutUnit oldLogicalHeight, LayoutUnit newLogicalHeight,  FloatingObject* lastFloatFromPreviousLine, InlineBidiResolver& resolver,  const LegacyInlineIterator& oldEnd)
{
    m_flow.removeFloatingObjectsBelow(lastFloatFromPreviousLine, oldLogicalHeight);
    m_flow.setLogicalHeight(newLogicalHeight);
    resolver.setPositionIgnoringNestedIsolates(oldEnd);
    return oldEnd;
}

void LegacyLineLayout::layoutRunsAndFloatsInRange(LineLayoutState& layoutState, InlineBidiResolver& resolver, const LegacyInlineIterator& cleanLineStart, const BidiStatus& cleanLineBidiStatus, unsigned consecutiveHyphenatedLines)
{
    const RenderStyle& styleToUse = style();
    bool paginated = layoutContext().layoutState() && layoutContext().layoutState()->isPaginated();
    LineWhitespaceCollapsingState& lineWhitespaceCollapsingState = resolver.whitespaceCollapsingState();
    LegacyInlineIterator end = resolver.position();
    bool checkForEndLineMatch = layoutState.endLine();
    RenderTextInfo renderTextInfo;
    VerticalPositionCache verticalPositionCache;

    LineBreaker lineBreaker(m_flow);

    while (!end.atEnd()) {
        // FIXME: Is this check necessary before the first iteration or can it be moved to the end?
        if (checkForEndLineMatch) {
            layoutState.setEndLineMatched(matchedEndLine(layoutState, resolver, cleanLineStart, cleanLineBidiStatus));
            if (layoutState.endLineMatched()) {
                resolver.setPosition(LegacyInlineIterator(resolver.position().root(), 0, 0), 0);
                layoutState.marginInfo().clearMargin();
                break;
            }
        }

        lineWhitespaceCollapsingState.reset();

        layoutState.lineInfo().setEmpty(true);
        layoutState.lineInfo().resetRunsFromLeadingWhitespace();

        const LegacyInlineIterator oldEnd = end;
        bool isNewUBAParagraph = layoutState.lineInfo().previousLineBrokeCleanly();
        FloatingObject* lastFloatFromPreviousLine = (m_flow.containsFloats()) ? m_flow.floatingObjects()->set().last().get() : nullptr;

        WordMeasurements wordMeasurements;
        end = lineBreaker.nextLineBreak(resolver, layoutState.lineInfo(), renderTextInfo, lastFloatFromPreviousLine, consecutiveHyphenatedLines, wordMeasurements);
        m_flow.cachePriorCharactersIfNeeded(renderTextInfo.lineBreakIterator);
        renderTextInfo.lineBreakIterator.resetPriorContext();
        if (resolver.position().atEnd()) {
            // FIXME: We shouldn't be creating any runs in nextLineBreak to begin with!
            // Once BidiRunList is separated from BidiResolver this will not be needed.
            resolver.runs().clear();
            resolver.markCurrentRunEmpty(); // FIXME: This can probably be replaced by an ASSERT (or just removed).
            layoutState.setCheckForFloatsFromLastLine(true);
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

            BidiRun* trailingSpaceRun = !layoutState.lineInfo().previousLineBrokeCleanly() ? handleTrailingSpaces(bidiRuns, resolver.context()) : nullptr;

            if (bidiRuns.runCount() && lineBreaker.lineWasHyphenated()) {
                bidiRuns.logicallyLastRun()->m_hasHyphen = true;
                consecutiveHyphenatedLines++;
            } else
                consecutiveHyphenatedLines = 0;

            // Now that the runs have been ordered, we create the line boxes.
            // At the same time we figure out where border/padding/margin should be applied for
            // inline flow boxes.

            LayoutUnit oldLogicalHeight = m_flow.logicalHeight();
            LegacyRootInlineBox* lineBox = createLineBoxesFromBidiRuns(resolver.status().context->level(), bidiRuns, end, layoutState.lineInfo(), verticalPositionCache, trailingSpaceRun, wordMeasurements);

            bidiRuns.clear();
            resolver.markCurrentRunEmpty(); // FIXME: This can probably be replaced by an ASSERT (or just removed).

            if (lineBox) {
                lineBox->setLineBreakInfo(end.renderer(), end.offset(), resolver.status());
                if (layoutState.usesRepaintBounds())
                    layoutState.updateRepaintRangeFromBox(lineBox);
                
                LayoutUnit adjustment;
                bool overflowsFragment = false;
                
                layoutState.marginInfo().setAtBeforeSideOfBlock(false);

                if (paginated)
                    m_flow.adjustLinePositionForPagination(lineBox, adjustment, overflowsFragment, layoutState.fragmentedFlow());
                if (adjustment) {
                    IndentTextOrNot shouldIndentText = layoutState.lineInfo().isFirstLine() ? IndentText : DoNotIndentText;
                    LayoutUnit oldLineWidth = m_flow.availableLogicalWidthForLine(oldLogicalHeight, shouldIndentText);
                    lineBox->adjustBlockDirectionPosition(adjustment);
                    if (layoutState.usesRepaintBounds())
                        layoutState.updateRepaintRangeFromBox(lineBox);

                    if (m_flow.availableLogicalWidthForLine(oldLogicalHeight + adjustment, shouldIndentText) != oldLineWidth) {
                        // We have to delete this line, remove all floats that got added, and let line layout re-run.
                        lineBox->deleteLine();
                        end = restartLayoutRunsAndFloatsInRange(oldLogicalHeight, oldLogicalHeight + adjustment, lastFloatFromPreviousLine, resolver, oldEnd);
                        continue;
                    }

                    m_flow.setLogicalHeight(lineBox->lineBoxBottom());
                }
                    
                if (paginated) {
                    if (layoutState.fragmentedFlow())
                        updateFragmentForLine(lineBox);
                }
            }
        }

        for (size_t i = 0; i < lineBreaker.positionedObjects().size(); ++i)
            setStaticPositions(m_flow, *lineBreaker.positionedObjects()[i], DoNotIndentText);

        if (!layoutState.lineInfo().isEmpty()) {
            layoutState.lineInfo().setFirstLine(false);
            m_flow.clearFloats(lineBreaker.usedClear());
        }

        if (m_flow.floatingObjects() && lastRootBox()) {
            const FloatingObjectSet& floatingObjectSet = m_flow.floatingObjects()->set();
            auto it = floatingObjectSet.begin();
            auto end = floatingObjectSet.end();
            if (auto* lastFloat = layoutState.floatList().lastFloat()) {
                auto lastFloatIterator = floatingObjectSet.find(lastFloat);
                ASSERT(lastFloatIterator != end);
                ++lastFloatIterator;
                it = lastFloatIterator;
            }
            for (; it != end; ++it) {
                auto& floatingObject = *it;
                appendFloatingObjectToLastLine(*floatingObject);
                // If a float's geometry has changed, give up on syncing with clean lines.
                auto* floatWithRect = layoutState.floatList().floatWithRect(floatingObject->renderer());
                if (!floatWithRect || floatWithRect->rect() != floatingObject->frameRect())
                    checkForEndLineMatch = false;
            }
            layoutState.floatList().setLastFloat(!floatingObjectSet.isEmpty() ? floatingObjectSet.last().get() : nullptr);
        }

        lineWhitespaceCollapsingState.reset();
        resolver.setPosition(end, numberOfIsolateAncestors(end));
    }

    // In case we already adjusted the line positions during this layout to avoid widows
    // then we need to ignore the possibility of having a new widows situation.
    // Otherwise, we risk leaving empty containers which is against the block fragmentation principles.
    if (paginated && !style().hasAutoWidows() && !m_flow.didBreakAtLineToAvoidWidow()) {
        // Check the line boxes to make sure we didn't create unacceptable widows.
        // However, we'll prioritize orphans - so nothing we do here should create
        // a new orphan.

        LegacyRootInlineBox* lineBox = lastRootBox();

        // Count from the end of the block backwards, to see how many hanging
        // lines we have.
        LegacyRootInlineBox* firstLineInBlock = firstRootBox();
        int numLinesHanging = 1;
        while (lineBox && lineBox != firstLineInBlock && !lineBox->isFirstAfterPageBreak()) {
            ++numLinesHanging;
            lineBox = lineBox->prevRootBox();
        }

        // If there were no breaks in the block, we didn't create any widows.
        if (!lineBox || !lineBox->isFirstAfterPageBreak() || lineBox == firstLineInBlock) {
            if (m_flow.shouldBreakAtLineToAvoidWidow()) {
                // This is the case when the previous line layout marks a line to break at to avoid widows
                // but the current layout does not produce that line. It happens when layout constraints unexpectedly
                // change in between layouts (note that these paginated line layouts run within the same layout frame
                // as opposed to two subsequent full layouts).
                ASSERT_NOT_REACHED();
                m_flow.clearShouldBreakAtLineToAvoidWidow();
            }
            return;
        }

        if (numLinesHanging < style().widows()) {
            // We have detected a widow. Now we need to work out how many
            // lines there are on the previous page, and how many we need
            // to steal.
            int numLinesNeeded = style().widows() - numLinesHanging;
            LegacyRootInlineBox* currentFirstLineOfNewPage = lineBox;

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
            m_flow.setBreakAtLineToAvoidWidow(lineCountUntil(lineBox));
            m_flow.markLinesDirtyInBlockRange(lastRootBox()->lineBoxBottom(), lineBox->lineBoxBottom(), lineBox);
        }
    }
    m_flow.clearDidBreakAtLineToAvoidWidow();
}

void LegacyLineLayout::reattachCleanLineFloats(LegacyRootInlineBox& cleanLine, LayoutUnit delta, bool isFirstCleanLine)
{
    auto* cleanLineFloats = cleanLine.floatsPtr();
    if (!cleanLineFloats)
        return;

    for (auto& floatingBox : *cleanLineFloats) {
        if (!floatingBox)
            continue;
        auto* floatingObject = m_flow.insertFloatingObject(*floatingBox);
        if (isFirstCleanLine && floatingObject->originatingLine()) {
            // Float box does not belong to this line anymore.
            ASSERT_WITH_SECURITY_IMPLICATION(cleanLine.prevRootBox() == floatingObject->originatingLine());
            cleanLine.removeFloat(*floatingBox);
            continue;
        }
        ASSERT_WITH_SECURITY_IMPLICATION(!floatingObject->originatingLine());
        floatingObject->setOriginatingLine(cleanLine);
        m_flow.setLogicalHeight(m_flow.logicalTopForChild(*floatingBox) - m_flow.marginBeforeForChild(*floatingBox) + delta);
        m_flow.positionNewFloats();
    }
}

void LegacyLineLayout::linkToEndLineIfNeeded(LineLayoutState& layoutState)
{
    auto* firstCleanLine = layoutState.endLine();
    if (firstCleanLine) {
        if (layoutState.endLineMatched()) {
            bool paginated = layoutContext().layoutState() && layoutContext().layoutState()->isPaginated();
            // Attach all the remaining lines, and then adjust their y-positions as needed.
            LayoutUnit delta = m_flow.logicalHeight() - layoutState.endLineLogicalTop();
            for (auto* line = firstCleanLine; line; line = line->nextRootBox()) {
                line->attachLine();
                if (paginated) {
                    delta -= line->paginationStrut();
                    bool overflowsFragment;
                    m_flow.adjustLinePositionForPagination(line, delta, overflowsFragment, layoutState.fragmentedFlow());
                }
                if (delta) {
                    layoutState.updateRepaintRangeFromBox(line, delta);
                    line->adjustBlockDirectionPosition(delta);
                }
                if (layoutState.fragmentedFlow())
                    updateFragmentForLine(line);
                reattachCleanLineFloats(*line, delta, line == firstCleanLine);
            }
            m_flow.setLogicalHeight(lastRootBox()->lineBoxBottom());
        } else {
            // Delete all the remaining lines.
            deleteLineRange(layoutState, layoutState.endLine());
        }
    }
    
    if (m_flow.floatingObjects() && (layoutState.checkForFloatsFromLastLine() || m_flow.positionNewFloats()) && lastRootBox()) {
        // In case we have a float on the last line, it might not be positioned up to now.
        // This has to be done before adding in the bottom border/padding, or the float will
        // include the padding incorrectly. -dwh
        if (layoutState.checkForFloatsFromLastLine()) {
            LayoutUnit bottomVisualOverflow = lastRootBox()->logicalBottomVisualOverflow();
            LayoutUnit bottomLayoutOverflow = lastRootBox()->logicalBottomLayoutOverflow();
            auto newLineBox = makeUnique<LegacyRootInlineBox>(m_flow);
            newLineBox->setIsForTrailingFloats();
            auto trailingFloatsLineBox = newLineBox.get();
            m_lineBoxes.appendLineBox(WTFMove(newLineBox));
            trailingFloatsLineBox->setConstructed();
            GlyphOverflowAndFallbackFontsMap textBoxDataMap;
            VerticalPositionCache verticalPositionCache;
            LayoutUnit blockLogicalHeight = m_flow.logicalHeight();
            trailingFloatsLineBox->alignBoxesInBlockDirection(blockLogicalHeight, textBoxDataMap, verticalPositionCache);
            trailingFloatsLineBox->setLineTopBottomPositions(blockLogicalHeight, blockLogicalHeight, blockLogicalHeight, blockLogicalHeight);
            trailingFloatsLineBox->setPaginatedLineWidth(m_flow.availableLogicalWidthForContent(blockLogicalHeight));
            LayoutRect logicalLayoutOverflow(0_lu, blockLogicalHeight, 1_lu, bottomLayoutOverflow - blockLogicalHeight);
            LayoutRect logicalVisualOverflow(0_lu, blockLogicalHeight, 1_lu, bottomVisualOverflow - blockLogicalHeight);
            trailingFloatsLineBox->setOverflowFromLogicalRects(logicalLayoutOverflow, logicalVisualOverflow, trailingFloatsLineBox->lineTop(), trailingFloatsLineBox->lineBottom());
            if (layoutState.fragmentedFlow())
                updateFragmentForLine(trailingFloatsLineBox);
        }

        const FloatingObjectSet& floatingObjectSet = m_flow.floatingObjects()->set();
        auto it = floatingObjectSet.begin();
        auto end = floatingObjectSet.end();
        if (auto* lastFloat = layoutState.floatList().lastFloat()) {
            auto lastFloatIterator = floatingObjectSet.find(lastFloat);
            ASSERT(lastFloatIterator != end);
            ++lastFloatIterator;
            it = lastFloatIterator;
        }
        for (; it != end; ++it)
            appendFloatingObjectToLastLine(**it);
        layoutState.floatList().setLastFloat(!floatingObjectSet.isEmpty() ? floatingObjectSet.last().get() : nullptr);
    }
}

void LegacyLineLayout::layoutLineBoxes(bool relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom)
{
    m_flow.setLogicalHeight(m_flow.borderAndPaddingBefore());
    
    // Lay out our hypothetical grid line as though it occurs at the top of the block.
    if (layoutContext().layoutState() && layoutContext().layoutState()->lineGrid() == &m_flow)
        m_flow.layoutLineGridBox();

    RenderFragmentedFlow* fragmentedFlow = m_flow.enclosingFragmentedFlow();
    bool clearLinesForPagination = firstRootBox() && fragmentedFlow && !fragmentedFlow->hasFragments();

    // Figure out if we should clear out our line boxes.
    // FIXME: Handle resize eventually!
    bool isFullLayout = !firstRootBox() || m_flow.selfNeedsLayout() || relayoutChildren || clearLinesForPagination;
    LineLayoutState layoutState(m_flow, isFullLayout, repaintLogicalTop, repaintLogicalBottom, fragmentedFlow);

    if (isFullLayout)
        lineBoxes().deleteLineBoxes();

    // Text truncation kicks in in two cases:
    //     1) If your overflow isn't visible and your text-overflow-mode isn't clip.
    //     2) If you're an anonymous block with a block parent that satisfies #1.
    // FIXME: CSS3 says that descendants that are clipped must also know how to truncate. This is insanely
    // difficult to figure out in general (especially in the middle of doing layout), so we only handle the
    // simple case of an anonymous block truncating when it's parent is clipped.
    auto* parent = m_flow.parent();
    bool hasTextOverflow = (style().textOverflow() == TextOverflow::Ellipsis && m_flow.hasNonVisibleOverflow())
        || (m_flow.isAnonymousBlock() && parent && parent->isRenderBlock() && parent->style().textOverflow() == TextOverflow::Ellipsis && parent->hasNonVisibleOverflow());

    // Walk all the lines and delete our ellipsis line boxes if they exist.
    if (hasTextOverflow)
        deleteEllipsisLineBoxes();

    if (m_flow.firstChild()) {
        // In full layout mode, clear the line boxes of children upfront. Otherwise,
        // siblings can run into stale root lineboxes during layout. Then layout
        // the replaced elements later. In partial layout mode, line boxes are not
        // deleted and only dirtied. In that case, we can layout the replaced
        // elements at the same time.
        bool hasInlineChild = false;
        Vector<RenderBox*> replacedChildren;
        for (InlineWalker walker(m_flow); !walker.atEnd(); walker.advance()) {
            RenderObject& o = *walker.current();

            if (!hasInlineChild && o.isInline())
                hasInlineChild = true;

            if (o.isReplacedOrInlineBlock() || o.isFloating() || o.isOutOfFlowPositioned()) {
                RenderBox& box = downcast<RenderBox>(o);

                if (relayoutChildren || box.hasRelativeDimensions())
                    box.setChildNeedsLayout(MarkOnlyThis);

                // If relayoutChildren is set and the child has percentage padding or an embedded content box, we also need to invalidate the childs pref widths.
                if (relayoutChildren && box.needsPreferredWidthsRecalculation())
                    box.setPreferredLogicalWidthsDirty(true, MarkOnlyThis);

                if (box.isOutOfFlowPositioned())
                    box.containingBlock()->insertPositionedObject(box);
                else if (box.isFloating())
                    layoutState.floatList().append(FloatWithRect::create(box));
                else if (isFullLayout || box.needsLayout()) {
                    // Replaced element.
                    if (isFullLayout && is<RenderRubyRun>(box)) {
                        // FIXME: This resets the overhanging margins that we set during line layout (see computeInlineDirectionPositionsForSegment)
                        // Find a more suitable place for this.
                        m_flow.setMarginStartForChild(box, 0);
                        m_flow.setMarginEndForChild(box, 0);
                    }
                    box.dirtyLineBoxes(isFullLayout);
                    if (isFullLayout)
                        replacedChildren.append(&box);
                    else
                        box.layoutIfNeeded();
                }
            } else if (o.isTextOrLineBreak() || is<RenderInline>(o)) {
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
        LayoutUnit lowestAllowedPosition = std::max(lastRootBox()->lineBottom(), m_flow.logicalHeight() + m_flow.paddingAfter());
        if (!style().isFlippedLinesWritingMode())
            lastLineAnnotationsAdjustment = lastRootBox()->computeUnderAnnotationAdjustment(lowestAllowedPosition);
        else
            lastLineAnnotationsAdjustment = lastRootBox()->computeOverAnnotationAdjustment(lowestAllowedPosition);
    }
    
    // Now do the handling of the bottom of the block, adding in our bottom border/padding and
    // determining the correct collapsed bottom margin information. This collapse is only necessary
    // if our last child was an anonymous inline block that might need to propagate margin information out to
    // us.
    LayoutUnit afterEdge = m_flow.borderAndPaddingAfter() + m_flow.scrollbarLogicalHeight() + lastLineAnnotationsAdjustment;
    m_flow.setLogicalHeight(m_flow.logicalHeight() + afterEdge);

    if (!firstRootBox() && m_flow.hasLineIfEmpty())
        m_flow.setLogicalHeight(m_flow.logicalHeight() + m_flow.lineHeight(true, m_flow.isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes));

    // See if we have any lines that spill out of our block. If we do, then we will possibly need to
    // truncate text.
    if (hasTextOverflow)
        checkLinesForTextOverflow();
}

void LegacyLineLayout::checkFloatInCleanLine(LegacyRootInlineBox& cleanLine, RenderBox& floatBoxOnCleanLine, FloatWithRect& matchingFloatWithRect,
    bool& encounteredNewFloat, bool& dirtiedByFloat)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!floatBoxOnCleanLine.style().deletionHasBegun());
    if (&matchingFloatWithRect.renderer() != &floatBoxOnCleanLine) {
        encounteredNewFloat = true;
        return;
    }
    floatBoxOnCleanLine.layoutIfNeeded();
    LayoutRect originalFloatRect = matchingFloatWithRect.rect();
    LayoutSize newSize(
        floatBoxOnCleanLine.width() + floatBoxOnCleanLine.horizontalMarginExtent(),
        floatBoxOnCleanLine.height() + floatBoxOnCleanLine.verticalMarginExtent());
    
    // We have to reset the cap-height alignment done by the first-letter floats when initial-letter is set, so just always treat first-letter floats as dirty.
    if (originalFloatRect.size() == newSize && (floatBoxOnCleanLine.style().styleType() != PseudoId::FirstLetter || !floatBoxOnCleanLine.style().initialLetterDrop()))
        return;

    LayoutUnit floatTop = m_flow.isHorizontalWritingMode() ? originalFloatRect.y() : originalFloatRect.x();
    LayoutUnit floatHeight = m_flow.isHorizontalWritingMode() ? std::max(originalFloatRect.height(), newSize.height())
        : std::max(originalFloatRect.width(), newSize.width());
    floatHeight = std::min(floatHeight, LayoutUnit::max() - floatTop);
    cleanLine.markDirty();
    m_flow.markLinesDirtyInBlockRange(cleanLine.lineBoxBottom(), floatTop + floatHeight, &cleanLine);
    LayoutRect newFloatRect = originalFloatRect;
    newFloatRect.setSize(newSize);
    matchingFloatWithRect.adjustRect(newFloatRect);
    dirtiedByFloat = true;
}

LegacyRootInlineBox* LegacyLineLayout::determineStartPosition(LineLayoutState& layoutState, InlineBidiResolver& resolver)
{
    LegacyRootInlineBox* currentLine = nullptr;
    LegacyRootInlineBox* lastLine = nullptr;

    // FIXME: This entire float-checking block needs to be broken into a new function.
    auto& floats = layoutState.floatList();
    bool dirtiedByFloat = false;
    if (!layoutState.isFullLayout()) {
        // Paginate all of the clean lines.
        bool paginated = layoutContext().layoutState() && layoutContext().layoutState()->isPaginated();
        LayoutUnit paginationDelta;
        auto floatsIterator = floats.begin();
        auto end = floats.end();
        for (currentLine = firstRootBox(); currentLine && !currentLine->isDirty(); currentLine = currentLine->nextRootBox()) {
            if (paginated) {
                if (lineWidthForPaginatedLineChanged(currentLine, 0, layoutState.fragmentedFlow())) {
                    currentLine->markDirty();
                    break;
                }
                paginationDelta -= currentLine->paginationStrut();
                bool overflowsFragment;
                m_flow.adjustLinePositionForPagination(currentLine, paginationDelta, overflowsFragment, layoutState.fragmentedFlow());
                if (paginationDelta) {
                    if (m_flow.containsFloats() || !floats.isEmpty()) {
                        // FIXME: Do better eventually. For now if we ever shift because of pagination and floats are present just go to a full layout.
                        layoutState.markForFullLayout();
                        break;
                    }

                    layoutState.updateRepaintRangeFromBox(currentLine, paginationDelta);
                    currentLine->adjustBlockDirectionPosition(paginationDelta);
                }
                if (layoutState.fragmentedFlow())
                    updateFragmentForLine(currentLine);
            }

            if (auto* cleanLineFloats = currentLine->floatsPtr()) {
                // If a new float has been inserted before this line or before its last known float, just do a full layout.
                bool encounteredNewFloat = false;
                for (auto& floatBoxOnCleanLine : *cleanLineFloats) {
                    ASSERT(floatsIterator != end);
                    if (!floatBoxOnCleanLine)
                        continue;
                    checkFloatInCleanLine(*currentLine, *floatBoxOnCleanLine, *floatsIterator, encounteredNewFloat, dirtiedByFloat);
                    ++floatsIterator;
                    if (floatsIterator == end || encounteredNewFloat) {
                        layoutState.markForFullLayout();
                        break;
                    }
                }
                if (dirtiedByFloat || encounteredNewFloat)
                    break;
            }
        }
        // Check if a new float has been inserted after the last known float.
        if (floatsIterator != end) {
            if (!currentLine)
                layoutState.markForFullLayout();
            else {
                for (; floatsIterator != end; ++floatsIterator) {
                    auto& floatWithRect = *floatsIterator;
                    if (!floatWithRect->renderer().needsLayout())
                        continue;
                    layoutState.markForFullLayout();
                    break;
                }
            }
        }
    }

    if (layoutState.isFullLayout()) {
        m_lineBoxes.deleteLineBoxTree();
        currentLine = nullptr;
        ASSERT(!firstRootBox() && !lastRootBox());
    } else {
        if (currentLine) {
            // We have a dirty line.
            if (LegacyRootInlineBox* prevRootBox = currentLine->prevRootBox()) {
                // We have a previous line.
                if (!dirtiedByFloat && (!prevRootBox->endsWithBreak()
                    || !prevRootBox->lineBreakObj()
                    || (is<RenderText>(*prevRootBox->lineBreakObj())
                    && prevRootBox->lineBreakPos() >= downcast<RenderText>(*prevRootBox->lineBreakObj()).text().length()))) {
                    // The previous line didn't break cleanly or broke at a newline
                    // that has been deleted, so treat it as dirty too.
                    currentLine = prevRootBox;
                }
            }
        }
        // If we have no dirty lines, then last is just the last root box.
        lastLine = currentLine ? currentLine->prevRootBox() : lastRootBox();
    }

    if (!floats.isEmpty()) {
        LayoutUnit savedLogicalHeight = m_flow.logicalHeight();
        // Restore floats from clean lines.
        LegacyRootInlineBox* line = firstRootBox();
        while (line != currentLine) {
            if (auto* cleanLineFloats = line->floatsPtr()) {
                for (auto& floatingBox : *cleanLineFloats) {
                    if (!floatingBox)
                        continue;
                    auto* floatingObject = m_flow.insertFloatingObject(*floatingBox);
                    ASSERT_WITH_SECURITY_IMPLICATION(!floatingObject->originatingLine());
                    floatingObject->setOriginatingLine(*line);
                    m_flow.setLogicalHeight(m_flow.logicalTopForChild(*floatingBox) - m_flow.marginBeforeForChild(*floatingBox));
                    m_flow.positionNewFloats();
                    floats.setLastCleanFloat(*floatingBox);
                }
            }
            line = line->nextRootBox();
        }
        m_flow.setLogicalHeight(savedLogicalHeight);
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

void LegacyLineLayout::determineEndPosition(LineLayoutState& layoutState, LegacyRootInlineBox* startLine, LegacyInlineIterator& cleanLineStart, BidiStatus& cleanLineBidiStatus)
{
    auto iteratorForFirstDirtyFloat = [](LineLayoutState::FloatList& floats) {
        auto lastCleanFloat = floats.lastCleanFloat();
        if (!lastCleanFloat)
            return floats.begin();
        auto* lastCleanFloatWithRect = floats.floatWithRect(*lastCleanFloat);
        ASSERT(lastCleanFloatWithRect);
        return ++floats.find(*lastCleanFloatWithRect);
    };

    ASSERT(!layoutState.endLine());
    auto floatsIterator = iteratorForFirstDirtyFloat(layoutState.floatList());
    auto end = layoutState.floatList().end();
    LegacyRootInlineBox* lastLine = nullptr;
    for (auto* currentLine = startLine->nextRootBox(); currentLine; currentLine = currentLine->nextRootBox()) {
        if (!currentLine->isDirty()) {
            if (auto* cleanLineFloats = currentLine->floatsPtr()) {
                bool encounteredNewFloat = false;
                bool dirtiedByFloat = false;
                for (auto& floatBoxOnCleanLine : *cleanLineFloats) {
                    if (!floatBoxOnCleanLine)
                        continue;
                    ASSERT(floatsIterator != end);
                    checkFloatInCleanLine(*currentLine, *floatBoxOnCleanLine, *floatsIterator, encounteredNewFloat, dirtiedByFloat);
                    ++floatsIterator;
                    if (floatsIterator == end || encounteredNewFloat)
                        return;
                }
            }
        }
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
    cleanLineBidiStatus = previousLine->lineBreakBidiStatus();
    layoutState.setEndLineLogicalTop(previousLine->lineBoxBottom());

    for (auto* line = lastLine; line; line = line->nextRootBox()) {
        // Disconnect all line boxes from their render objects while preserving their connections to one another.
        line->extractLine();
    }
    layoutState.setEndLine(lastLine);
}

bool LegacyLineLayout::checkPaginationAndFloatsAtEndLine(LineLayoutState& layoutState)
{
    LayoutUnit lineDelta = m_flow.logicalHeight() - layoutState.endLineLogicalTop();

    bool paginated = layoutContext().layoutState() && layoutContext().layoutState()->isPaginated();
    if (paginated && layoutState.fragmentedFlow()) {
        // Check all lines from here to the end, and see if the hypothetical new position for the lines will result
        // in a different available line width.
        for (auto* lineBox = layoutState.endLine(); lineBox; lineBox = lineBox->nextRootBox()) {
            if (paginated) {
                // This isn't the real move we're going to do, so don't update the line box's pagination
                // strut yet.
                LayoutUnit oldPaginationStrut = lineBox->paginationStrut();
                bool overflowsFragment;
                lineDelta -= oldPaginationStrut;
                m_flow.adjustLinePositionForPagination(lineBox, lineDelta, overflowsFragment, layoutState.fragmentedFlow());
                lineBox->setPaginationStrut(oldPaginationStrut);
            }
            if (lineWidthForPaginatedLineChanged(lineBox, lineDelta, layoutState.fragmentedFlow()))
                return false;
        }
    }
    
    if (!lineDelta || !m_flow.floatingObjects())
        return true;
    
    // See if any floats end in the range along which we want to shift the lines vertically.
    LayoutUnit logicalTop = std::min(m_flow.logicalHeight(), layoutState.endLineLogicalTop());

    LegacyRootInlineBox* lastLine = layoutState.endLine();
    while (LegacyRootInlineBox* nextLine = lastLine->nextRootBox())
        lastLine = nextLine;

    LayoutUnit logicalBottom = lastLine->lineBoxBottom() + absoluteValue(lineDelta);

    const FloatingObjectSet& floatingObjectSet = m_flow.floatingObjects()->set();
    auto end = floatingObjectSet.end();
    for (auto it = floatingObjectSet.begin(); it != end; ++it) {
        const auto& floatingObject = *it->get();
        if (m_flow.logicalBottomForFloat(floatingObject) >= logicalTop && m_flow.logicalBottomForFloat(floatingObject) < logicalBottom)
            return false;
    }

    return true;
}

bool LegacyLineLayout::lineWidthForPaginatedLineChanged(LegacyRootInlineBox* rootBox, LayoutUnit lineDelta, RenderFragmentedFlow* fragmentedFlow) const
{
    if (!fragmentedFlow)
        return false;

    RenderFragmentContainer* currentFragment = m_flow.fragmentAtBlockOffset(rootBox->lineBoxTop() + lineDelta);
    // Just bail if the fragment didn't change.
    if (rootBox->containingFragment() == currentFragment)
        return false;
    return rootBox->paginatedLineWidth() != m_flow.availableLogicalWidthForContent(currentFragment);
}

bool LegacyLineLayout::matchedEndLine(LineLayoutState& layoutState, const InlineBidiResolver& resolver, const LegacyInlineIterator& endLineStart, const BidiStatus& endLineStatus)
{
    if (resolver.position() == endLineStart) {
        if (resolver.status() != endLineStatus)
            return false;
        return checkPaginationAndFloatsAtEndLine(layoutState);
    }

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
                matched = checkPaginationAndFloatsAtEndLine(layoutState);
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
        RenderFragmentContainer* fragment = m_flow.enclosingFragmentedFlow() ? curr->containingFragment() : nullptr;
        if (fragment)
            fragment->addLayoutOverflowForBox(&m_flow, curr->paddedLayoutOverflowRect(endPadding));
        if (!m_flow.hasNonVisibleOverflow()) {
            LayoutRect childVisualOverflowRect = curr->visualOverflowRect(curr->lineTop(), curr->lineBottom());
            m_flow.addVisualOverflow(childVisualOverflowRect);
            if (fragment)
                fragment->addVisualOverflowForBox(&m_flow, childVisualOverflowRect);
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

void LegacyLineLayout::deleteEllipsisLineBoxes()
{
    TextAlignMode textAlign = style().textAlign();
    bool ltr = style().isLeftToRightDirection();
    IndentTextOrNot shouldIndentText = IndentText;
    for (auto* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
        if (curr->hasEllipsisBox()) {
            curr->clearTruncation();

            // Shift the line back where it belongs if we cannot accomodate an ellipsis.
            float logicalLeft = m_flow.logicalLeftOffsetForLine(curr->lineTop(), shouldIndentText);
            float availableLogicalWidth = m_flow.logicalRightOffsetForLine(curr->lineTop(), DoNotIndentText) - logicalLeft;
            float totalLogicalWidth = curr->logicalWidth();
            updateLogicalWidthForAlignment(m_flow, textAlign, curr, 0, logicalLeft, totalLogicalWidth, availableLogicalWidth, 0);

            if (ltr)
                curr->adjustLogicalPosition((logicalLeft - curr->logicalLeft()), 0);
            else
                curr->adjustLogicalPosition(-(curr->logicalLeft() - logicalLeft), 0);
        }
        shouldIndentText = DoNotIndentText;
    }
}

void LegacyLineLayout::checkLinesForTextOverflow()
{
    // Determine the width of the ellipsis using the current font.
    // FIXME: CSS3 says this is configurable, also need to use 0x002E (FULL STOP) if horizontal ellipsis is "not renderable"
    const FontCascade& font = style().fontCascade();
    static MainThreadNeverDestroyed<const AtomString> ellipsisStr(&horizontalEllipsis, 1);
    const FontCascade& firstLineFont = m_flow.firstLineStyle().fontCascade();
    float firstLineEllipsisWidth = firstLineFont.width(m_flow.constructTextRun(&horizontalEllipsis, 1, m_flow.firstLineStyle()));
    float ellipsisWidth = (font == firstLineFont) ? firstLineEllipsisWidth : font.width(m_flow.constructTextRun(&horizontalEllipsis, 1, style()));

    // For LTR text truncation, we want to get the right edge of our padding box, and then we want to see
    // if the right edge of a line box exceeds that. For RTL, we use the left edge of the padding box and
    // check the left edge of the line box to see if it is less
    // Include the scrollbar for overflow blocks, which means we want to use "contentWidth()"
    bool ltr = style().isLeftToRightDirection();
    TextAlignMode textAlign = style().textAlign();
    bool firstLine = true;
    for (auto* curr = firstRootBox(); curr; curr = curr->nextRootBox()) {
        IndentTextOrNot shouldIndentText = firstLine ? IndentText : DoNotIndentText;
        LayoutUnit blockRightEdge = m_flow.logicalRightOffsetForLine(curr->lineTop(), shouldIndentText);
        LayoutUnit blockLeftEdge = m_flow.logicalLeftOffsetForLine(curr->lineTop(), shouldIndentText);
        LayoutUnit lineBoxEdge { ltr ? curr->x() + curr->logicalWidth() : curr->x() };
        if ((ltr && lineBoxEdge > blockRightEdge) || (!ltr && lineBoxEdge < blockLeftEdge)) {
            // This line spills out of our box in the appropriate direction. Now we need to see if the line
            // can be truncated. In order for truncation to be possible, the line must have sufficient space to
            // accommodate our truncation string, and no replaced elements (images, tables) can overlap the ellipsis
            // space.
            LayoutUnit width { firstLine ? firstLineEllipsisWidth : ellipsisWidth };
            LayoutUnit blockEdge { ltr ? blockRightEdge : blockLeftEdge };
            if (curr->lineCanAccommodateEllipsis(ltr, blockEdge, lineBoxEdge, width)) {
                float totalLogicalWidth = curr->placeEllipsis(ellipsisStr, ltr, blockLeftEdge, blockRightEdge, width);

                float logicalLeft = 0; // We are only interested in the delta from the base position.
                float truncatedWidth = m_flow.availableLogicalWidthForLine(curr->lineTop(), shouldIndentText);
                updateLogicalWidthForAlignment(m_flow, textAlign, curr, nullptr, logicalLeft, totalLogicalWidth, truncatedWidth, 0);
                if (ltr)
                    curr->adjustLogicalPosition(logicalLeft, 0);
                else
                    curr->adjustLogicalPosition(-(truncatedWidth - (logicalLeft + totalLogicalWidth)), 0);
            }
        }
        firstLine = false;
    }
}

bool LegacyLineLayout::positionNewFloatOnLine(const FloatingObject& newFloat, FloatingObject* lastFloatFromPreviousLine, LineInfo& lineInfo, LineWidth& width)
{
    if (!m_flow.positionNewFloats())
        return false;

    width.shrinkAvailableWidthForNewFloatIfNeeded(newFloat);

    // We only connect floats to lines for pagination purposes if the floats occur at the start of
    // the line and the previous line had a hard break (so this line is either the first in the block
    // or follows a <br>).
    if (!newFloat.paginationStrut() || !lineInfo.previousLineBrokeCleanly() || !lineInfo.isEmpty())
        return true;

    const FloatingObjectSet& floatingObjectSet = m_flow.floatingObjects()->set();
    ASSERT(floatingObjectSet.last().get() == &newFloat);

    LayoutUnit floatLogicalTop = m_flow.logicalTopForFloat(newFloat);
    LayoutUnit paginationStrut = newFloat.paginationStrut();

    if (floatLogicalTop - paginationStrut != m_flow.logicalHeight() + lineInfo.floatPaginationStrut())
        return true;

    auto it = floatingObjectSet.end();
    --it; // Last float is newFloat, skip that one.
    auto begin = floatingObjectSet.begin();
    while (it != begin) {
        --it;
        auto& floatingObject = *it->get();
        if (&floatingObject == lastFloatFromPreviousLine)
            break;
        if (m_flow.logicalTopForFloat(floatingObject) == m_flow.logicalHeight() + lineInfo.floatPaginationStrut()) {
            floatingObject.setPaginationStrut(paginationStrut + floatingObject.paginationStrut());
            RenderBox& floatBox = floatingObject.renderer();
            m_flow.setLogicalTopForChild(floatBox, m_flow.logicalTopForChild(floatBox) + m_flow.marginBeforeForChild(floatBox) + paginationStrut);

            if (m_flow.updateFragmentRangeForBoxChild(floatBox))
                floatBox.setNeedsLayout(MarkOnlyThis);
            else if (is<RenderBlock>(floatBox))
                downcast<RenderBlock>(floatBox).setChildNeedsLayout(MarkOnlyThis);
            floatBox.layoutIfNeeded();

            // Save the old logical top before calling removePlacedObject which will set
            // isPlaced to false. Otherwise it will trigger an assert in logicalTopForFloat.
            LayoutUnit oldLogicalTop = m_flow.logicalTopForFloat(floatingObject);
            m_flow.floatingObjects()->removePlacedObject(&floatingObject);
            m_flow.setLogicalTopForFloat(floatingObject, oldLogicalTop + paginationStrut);
            m_flow.floatingObjects()->addPlacedObject(&floatingObject);
        }
    }

    // Just update the line info's pagination strut without altering our logical height yet. If the line ends up containing
    // no content, then we don't want to improperly grow the height of the block.
    lineInfo.setFloatPaginationStrut(lineInfo.floatPaginationStrut() + paginationStrut);
    return true;
}

void LegacyLineLayout::updateFragmentForLine(LegacyRootInlineBox* lineBox) const
{
    ASSERT(lineBox);

    if (!m_flow.hasFragmentRangeInFragmentedFlow())
        lineBox->clearContainingFragment();
    else {
        if (auto containingFragment = m_flow.fragmentAtBlockOffset(lineBox->lineBoxTop()))
            lineBox->setContainingFragment(*containingFragment);
        else
            lineBox->clearContainingFragment();
    }

    LegacyRootInlineBox* prevLineBox = lineBox->prevRootBox();
    if (!prevLineBox)
        return;

    // This check is more accurate than the one in |adjustLinePositionForPagination| because it takes into
    // account just the container changes between lines. The before mentioned function doesn't set the flag
    // correctly if the line is positioned at the top of the last fragment container.
    if (lineBox->containingFragment() != prevLineBox->containingFragment())
        lineBox->setIsFirstAfterPageBreak(true);
}

const RenderStyle& LegacyLineLayout::style() const
{
    return m_flow.style();
}

const FrameViewLayoutContext& LegacyLineLayout::layoutContext() const
{
    return m_flow.view().frameView().layoutContext();
}


}
