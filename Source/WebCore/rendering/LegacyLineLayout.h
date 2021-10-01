/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "LegacyInlineFlowBox.h"
#include "LineWidth.h"
#include "RenderLineBoxList.h"
#include "RenderStyleConstants.h"
#include "TrailingObjects.h"

namespace WebCore {

class BidiContext;
class FloatingObject;
class FloatWithRect;
class FrameViewLayoutContext;
class LegacyInlineBox;
class LegacyInlineIterator;
class LineInfo;
class LineLayoutState;
class RenderBlockFlow;
class RenderObject;
class RenderRubyRun;
class LegacyRootInlineBox;
class VerticalPositionCache;
struct BidiStatus;
struct WordMeasurement;

template <class Run> class BidiRunList;
typedef Vector<WordMeasurement, 64> WordMeasurements;

class LegacyLineLayout {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LegacyLineLayout(RenderBlockFlow&);
    ~LegacyLineLayout();

    RenderLineBoxList& lineBoxes() { return m_lineBoxes; }
    const RenderLineBoxList& lineBoxes() const { return m_lineBoxes; }

    LegacyRootInlineBox* firstRootBox() const { return downcast<LegacyRootInlineBox>(m_lineBoxes.firstLineBox()); }
    LegacyRootInlineBox* lastRootBox() const { return downcast<LegacyRootInlineBox>(m_lineBoxes.lastLineBox()); }

    void layoutLineBoxes(bool relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom);

    LegacyRootInlineBox* constructLine(BidiRunList<BidiRun>&, const LineInfo&);
    bool positionNewFloatOnLine(const FloatingObject& newFloat, FloatingObject* lastFloatFromPreviousLine, LineInfo&, LineWidth&);
    void addOverflowFromInlineChildren();

    size_t lineCount() const;
    size_t lineCountUntil(const LegacyRootInlineBox*) const;

    static void appendRunsForObject(BidiRunList<BidiRun>*, int start, int end, RenderObject&, InlineBidiResolver&);
    static void updateLogicalWidthForAlignment(RenderBlockFlow&, const TextAlignMode&, const LegacyRootInlineBox*, BidiRun* trailingSpaceRun, float& logicalLeft, float& totalLogicalWidth, float& availableLogicalWidth, int expansionOpportunityCount);

private:
    std::unique_ptr<LegacyRootInlineBox> createRootInlineBox();
    LegacyRootInlineBox* createAndAppendRootInlineBox();
    LegacyInlineBox* createInlineBoxForRenderer(RenderObject*, bool isOnlyRun = false);
    LegacyInlineFlowBox* createLineBoxes(RenderObject*, const LineInfo&, LegacyInlineBox*);
    TextAlignMode textAlignmentForLine(bool endsWithSoftBreak) const;
    void setMarginsForRubyRun(BidiRun*, RenderRubyRun&, RenderObject* previousObject, const LineInfo&);
    void updateRubyForJustifiedText(RenderRubyRun&, BidiRun&, const Vector<unsigned, 16>& expansionOpportunities, unsigned& expansionOpportunityCount, float& totalLogicalWidth, float availableLogicalWidth, size_t&);
    void computeExpansionForJustifiedText(BidiRun* firstRun, BidiRun* trailingSpaceRun, const Vector<unsigned, 16>& expansionOpportunities, unsigned expansionOpportunityCount, float totalLogicalWidth, float availableLogicalWidth);
    void computeInlineDirectionPositionsForLine(LegacyRootInlineBox*, const LineInfo&, BidiRun* firstRun, BidiRun* trailingSpaceRun, bool reachedEnd, GlyphOverflowAndFallbackFontsMap&, VerticalPositionCache&, WordMeasurements&);
    BidiRun* computeInlineDirectionPositionsForSegment(LegacyRootInlineBox*, const LineInfo&, TextAlignMode, float& logicalLeft, float& availableLogicalWidth, BidiRun* firstRun, BidiRun* trailingSpaceRun, GlyphOverflowAndFallbackFontsMap&, VerticalPositionCache&, WordMeasurements&);
    void removeInlineBox(BidiRun&, const LegacyRootInlineBox&) const;
    void computeBlockDirectionPositionsForLine(LegacyRootInlineBox*, BidiRun* firstRun, GlyphOverflowAndFallbackFontsMap& textBoxDataMap, VerticalPositionCache&);
    inline BidiRun* handleTrailingSpaces(BidiRunList<BidiRun>& bidiRuns, BidiContext* currentContext);
    void appendFloatingObjectToLastLine(FloatingObject&);
    LegacyRootInlineBox* createLineBoxesFromBidiRuns(unsigned bidiLevel, BidiRunList<BidiRun>& bidiRuns, const LegacyInlineIterator& end, LineInfo&, VerticalPositionCache&, BidiRun* trailingSpaceRun, WordMeasurements&);
    void layoutRunsAndFloats(LineLayoutState&, bool hasInlineChild);
    inline const LegacyInlineIterator& restartLayoutRunsAndFloatsInRange(LayoutUnit oldLogicalHeight, LayoutUnit newLogicalHeight, FloatingObject* lastFloatFromPreviousLine, InlineBidiResolver&,  const LegacyInlineIterator& oldEnd);
    void layoutRunsAndFloatsInRange(LineLayoutState&, InlineBidiResolver&, const LegacyInlineIterator& cleanLineStart, const BidiStatus& cleanLineBidiStatus, unsigned consecutiveHyphenatedLines);
    void reattachCleanLineFloats(LegacyRootInlineBox& cleanLine, LayoutUnit delta, bool isFirstCleanLine);
    void linkToEndLineIfNeeded(LineLayoutState&);
    void checkFloatInCleanLine(LegacyRootInlineBox& cleanLine, RenderBox& floatBoxOnCleanLine, FloatWithRect& matchingFloatWithRect, bool& encounteredNewFloat, bool& dirtiedByFloat);
    LegacyRootInlineBox* determineStartPosition(LineLayoutState&, InlineBidiResolver&);
    void determineEndPosition(LineLayoutState&, LegacyRootInlineBox* startLine, LegacyInlineIterator& cleanLineStart, BidiStatus& cleanLineBidiStatus);
    bool checkPaginationAndFloatsAtEndLine(LineLayoutState&);
    bool lineWidthForPaginatedLineChanged(LegacyRootInlineBox* rootBox, LayoutUnit lineDelta, RenderFragmentedFlow*) const;
    bool matchedEndLine(LineLayoutState&, const InlineBidiResolver&, const LegacyInlineIterator& endLineStart, const BidiStatus& endLineStatus);
    void deleteEllipsisLineBoxes();
    void checkLinesForTextOverflow();
    void updateFragmentForLine(LegacyRootInlineBox*) const;

    const RenderStyle& style() const;
    const FrameViewLayoutContext& layoutContext() const;

    RenderBlockFlow& m_flow;
    RenderLineBoxList m_lineBoxes;
};

};
