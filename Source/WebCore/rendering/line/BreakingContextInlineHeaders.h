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

#ifndef BreakingContextInlineHeaders_h
#define BreakingContextInlineHeaders_h

#include "Hyphenation.h"
#include "LineBreaker.h"
#include "LineInfo.h"
#include "LineWidth.h"
#include "RenderCombineText.h"
#include "RenderCounter.h"
#include "RenderInline.h"
#include "RenderListMarker.h"
#include "RenderRubyRun.h"
#include "RenderSVGInlineText.h"
#include "TrailingObjects.h"
#include "break_lines.h"
#include <wtf/unicode/CharacterNames.h>

#if ENABLE(CSS_SHAPES) && ENABLE(CSS_SHAPE_INSIDE)
#include "ShapeInsideInfo.h"
#endif

namespace WebCore {

// We don't let our line box tree for a single line get any deeper than this.
const unsigned cMaxLineDepth = 200;

struct WordMeasurement {
    WordMeasurement()
        : renderer(0)
        , width(0)
        , startOffset(0)
        , endOffset(0)
    {
    }

    RenderText* renderer;
    float width;
    int startOffset;
    int endOffset;
    HashSet<const SimpleFontData*> fallbackFonts;
};

class BreakingContext {
public:
    BreakingContext(LineBreaker& lineBreaker, InlineBidiResolver& resolver, LineInfo& inLineInfo, LineWidth& lineWidth, RenderTextInfo& inRenderTextInfo, FloatingObject* inLastFloatFromPreviousLine, bool appliedStartWidth, RenderBlockFlow& block)
        : m_lineBreaker(lineBreaker)
        , m_resolver(resolver)
        , m_current(resolver.position())
        , m_lineBreak(resolver.position())
        , m_block(block)
        , m_lastObject(m_current.renderer())
        , m_nextObject(0)
        , m_currentStyle(0)
        , m_blockStyle(block.style())
        , m_lineInfo(inLineInfo)
        , m_renderTextInfo(inRenderTextInfo)
        , m_lastFloatFromPreviousLine(inLastFloatFromPreviousLine)
        , m_width(lineWidth)
        , m_currWS(NORMAL)
        , m_lastWS(NORMAL)
        , m_preservesNewline(false)
        , m_atStart(true)
        , m_ignoringSpaces(false)
        , m_currentCharacterIsSpace(false)
        , m_currentCharacterIsWS(false)
        , m_appliedStartWidth(appliedStartWidth)
        , m_includeEndWidth(true)
        , m_autoWrap(false)
        , m_autoWrapWasEverTrueOnLine(false)
        , m_floatsFitOnLine(true)
        , m_collapseWhiteSpace(false)
        , m_startingNewParagraph(m_lineInfo.previousLineBrokeCleanly())
        , m_allowImagesToBreak(!block.document().inQuirksMode() || !block.isTableCell() || !m_blockStyle.logicalWidth().isIntrinsicOrAuto())
        , m_atEnd(false)
        , m_hadUncommittedWidthBeforeCurrent(false)
        , m_lineMidpointState(resolver.midpointState())
    {
        m_lineInfo.setPreviousLineBrokeCleanly(false);
    }

    RenderObject* currentObject() { return m_current.renderer(); }
    InlineIterator lineBreak() { return m_lineBreak; }
    InlineIterator& lineBreakRef() {return m_lineBreak; }
    LineWidth& lineWidth() { return m_width; }
    bool atEnd() { return m_atEnd; }

    void initializeForCurrentObject();

    void increment();

    void handleBR(EClear&);
    void handleOutOfFlowPositioned(Vector<RenderBox*>& positionedObjects);
    void handleFloat();
    void handleEmptyInline();
    void handleReplaced();
    bool handleText(WordMeasurements&, bool& hyphenated, unsigned& consecutiveHyphenatedLines);
    bool canBreakAtThisPosition();
    void commitAndUpdateLineBreakIfNeeded();
    InlineIterator handleEndOfLine();

    void clearLineBreakIfFitsOnLine(bool ignoringTrailingSpace = false)
    {
        if (m_width.fitsOnLine(ignoringTrailingSpace) || m_lastWS == NOWRAP)
            m_lineBreak.clear();
    }

    void commitLineBreakAtCurrentWidth(RenderObject* object, unsigned offset = 0, int nextBreak = -1)
    {
        m_width.commit();
        m_lineBreak.moveTo(object, offset, nextBreak);
    }

private:
    LineBreaker& m_lineBreaker;
    InlineBidiResolver& m_resolver;

    InlineIterator m_current;
    InlineIterator m_lineBreak;
    InlineIterator m_startOfIgnoredSpaces;

    RenderBlockFlow& m_block;
    RenderObject* m_lastObject;
    RenderObject* m_nextObject;

    RenderStyle* m_currentStyle;

    // Firefox and Opera will allow a table cell to grow to fit an image inside it under
    // very specific circumstances (in order to match common WinIE renderings).
    // Not supporting the quirk has caused us to mis-render some real sites. (See Bugzilla 10517.)
    RenderStyle& m_blockStyle;

    LineInfo& m_lineInfo;

    RenderTextInfo& m_renderTextInfo;

    FloatingObject* m_lastFloatFromPreviousLine;

    LineWidth m_width;

    EWhiteSpace m_currWS;
    EWhiteSpace m_lastWS;

    bool m_preservesNewline;
    bool m_atStart;

    // This variable is used only if whitespace isn't set to PRE, and it tells us whether
    // or not we are currently ignoring whitespace.
    bool m_ignoringSpaces;

    // This variable tracks whether the very last character we saw was a space. We use
    // this to detect when we encounter a second space so we know we have to terminate
    // a run.
    bool m_currentCharacterIsSpace;
    bool m_currentCharacterIsWS;
    bool m_appliedStartWidth;
    bool m_includeEndWidth;
    bool m_autoWrap;
    bool m_autoWrapWasEverTrueOnLine;
    bool m_floatsFitOnLine;
    bool m_collapseWhiteSpace;
    bool m_startingNewParagraph;
    bool m_allowImagesToBreak;
    bool m_atEnd;
    bool m_hadUncommittedWidthBeforeCurrent;

    LineMidpointState& m_lineMidpointState;

    TrailingObjects m_trailingObjects;
};

inline void BreakingContext::initializeForCurrentObject()
{
    m_hadUncommittedWidthBeforeCurrent = !!m_width.uncommittedWidth();

    m_currentStyle = &m_current.renderer()->style();

    ASSERT(m_currentStyle);

    m_nextObject = bidiNextSkippingEmptyInlines(m_block, m_current.renderer());
    if (m_nextObject && m_nextObject->parent() && !m_nextObject->parent()->isDescendantOf(m_current.renderer()->parent()))
        m_includeEndWidth = true;

    m_currWS = m_current.renderer()->isReplaced() ? m_current.renderer()->parent()->style().whiteSpace() : m_currentStyle->whiteSpace();
    m_lastWS = m_lastObject->isReplaced() ? m_lastObject->parent()->style().whiteSpace() : m_lastObject->style().whiteSpace();

    m_autoWrap = RenderStyle::autoWrap(m_currWS);
    m_autoWrapWasEverTrueOnLine = m_autoWrapWasEverTrueOnLine || m_autoWrap;

    m_preservesNewline = m_current.renderer()->isSVGInlineText() ? false : RenderStyle::preserveNewline(m_currWS);

    m_collapseWhiteSpace = RenderStyle::collapseWhiteSpace(m_currWS);
}

inline void BreakingContext::increment()
{
    // Clear out our character space bool, since inline <pre>s don't collapse whitespace
    // with adjacent inline normal/nowrap spans.
    if (!m_collapseWhiteSpace)
        m_currentCharacterIsSpace = false;

    m_current.moveToStartOf(m_nextObject);
    m_atStart = false;
}

inline void BreakingContext::handleBR(EClear& clear)
{
    if (m_width.fitsOnLine()) {
        RenderObject* br = m_current.renderer();
        m_lineBreak.moveToStartOf(br);
        m_lineBreak.increment();

        // A <br> always breaks a line, so don't let the line be collapsed
        // away. Also, the space at the end of a line with a <br> does not
        // get collapsed away. It only does this if the previous line broke
        // cleanly. Otherwise the <br> has no effect on whether the line is
        // empty or not.
        if (m_startingNewParagraph)
            m_lineInfo.setEmpty(false, &m_block, &m_width);
        m_trailingObjects.clear();
        m_lineInfo.setPreviousLineBrokeCleanly(true);

        // A <br> with clearance always needs a linebox in case the lines below it get dirtied later and
        // need to check for floats to clear - so if we're ignoring spaces, stop ignoring them and add a
        // run for this object.
        if (m_ignoringSpaces && m_currentStyle->clear() != CNONE)
            m_lineMidpointState.ensureLineBoxInsideIgnoredSpaces(br);
        // If we were preceded by collapsing space and are in a right-aligned container we need to ensure the space gets
        // collapsed away so that it doesn't push the text out from the container's right-hand edge.
        // FIXME: Do this regardless of the container's alignment - will require rebaselining a lot of test results.
        else if (m_ignoringSpaces && (m_blockStyle.textAlign() == RIGHT || m_blockStyle.textAlign() == WEBKIT_RIGHT))
            m_lineMidpointState.stopIgnoringSpaces(InlineIterator(0, m_current.renderer(), m_current.offset()));

        if (!m_lineInfo.isEmpty())
            clear = m_currentStyle->clear();
    }
    m_atEnd = true;
}

inline LayoutUnit borderPaddingMarginStart(const RenderInline& child)
{
    return child.marginStart() + child.paddingStart() + child.borderStart();
}

inline LayoutUnit borderPaddingMarginEnd(const RenderInline& child)
{
    return child.marginEnd() + child.paddingEnd() + child.borderEnd();
}

inline bool shouldAddBorderPaddingMargin(RenderObject* child)
{
    // When deciding whether we're at the edge of an inline, adjacent collapsed whitespace is the same as no sibling at all.
    return !child || (child->isText() && !toRenderText(child)->textLength());
}

inline RenderObject* previousInFlowSibling(RenderObject* child)
{
    child = child->previousSibling();
    while (child && child->isOutOfFlowPositioned())
        child = child->previousSibling();
    return child;
}

inline LayoutUnit inlineLogicalWidth(RenderObject* child, bool checkStartEdge = true, bool checkEndEdge = true)
{
    unsigned lineDepth = 1;
    LayoutUnit extraWidth = 0;
    RenderElement* parent = child->parent();
    while (parent->isRenderInline() && lineDepth++ < cMaxLineDepth) {
        const RenderInline& parentAsRenderInline = toRenderInline(*parent);
        if (!isEmptyInline(parentAsRenderInline)) {
            checkStartEdge = checkStartEdge && shouldAddBorderPaddingMargin(previousInFlowSibling(child));
            if (checkStartEdge)
                extraWidth += borderPaddingMarginStart(parentAsRenderInline);
            checkEndEdge = checkEndEdge && shouldAddBorderPaddingMargin(child->nextSibling());
            if (checkEndEdge)
                extraWidth += borderPaddingMarginEnd(parentAsRenderInline);
            if (!checkStartEdge && !checkEndEdge)
                return extraWidth;
        }
        child = parent;
        parent = child->parent();
    }
    return extraWidth;
}

inline void BreakingContext::handleOutOfFlowPositioned(Vector<RenderBox*>& positionedObjects)
{
    // If our original display wasn't an inline type, then we can
    // go ahead and determine our static inline position now.
    RenderBox* box = toRenderBox(m_current.renderer());
    bool isInlineType = box->style().isOriginalDisplayInlineType();
    if (!isInlineType)
        m_block.setStaticInlinePositionForChild(*box, m_block.logicalHeight(), m_block.startOffsetForContent(m_block.logicalHeight()));
    else {
        // If our original display was an INLINE type, then we can go ahead
        // and determine our static y position now.
        box->layer()->setStaticBlockPosition(m_block.logicalHeight());
    }

    // If we're ignoring spaces, we have to stop and include this object and
    // then start ignoring spaces again.
    if (isInlineType || box->container()->isRenderInline()) {
        if (m_ignoringSpaces)
            m_lineMidpointState.ensureLineBoxInsideIgnoredSpaces(box);
        m_trailingObjects.appendBoxIfNeeded(box);
    } else
        positionedObjects.append(box);

    m_width.addUncommittedWidth(inlineLogicalWidth(box));
    // Reset prior line break context characters.
    m_renderTextInfo.m_lineBreakIterator.resetPriorContext();
}

inline void BreakingContext::handleFloat()
{
    RenderBox& floatBox = toRenderBox(*m_current.renderer());
    FloatingObject* floatingObject = m_lineBreaker.insertFloatingObject(floatBox);
    // check if it fits in the current line.
    // If it does, position it now, otherwise, position
    // it after moving to next line (in clearFloats() func)
    if (m_floatsFitOnLine && m_width.fitsOnLineExcludingTrailingWhitespace(m_block.logicalWidthForFloat(floatingObject))) {
        m_lineBreaker.positionNewFloatOnLine(floatingObject, m_lastFloatFromPreviousLine, m_lineInfo, m_width);
        if (m_lineBreak.renderer() == m_current.renderer()) {
            ASSERT(!m_lineBreak.offset());
            m_lineBreak.increment();
        }
    } else
        m_floatsFitOnLine = false;
    // Update prior line break context characters, using U+FFFD (OBJECT REPLACEMENT CHARACTER) for floating element.
    m_renderTextInfo.m_lineBreakIterator.updatePriorContext(replacementCharacter);
}

// This is currently just used for list markers and inline flows that have line boxes. Neither should
// have an effect on whitespace at the start of the line.
inline bool shouldSkipWhitespaceAfterStartObject(RenderBlockFlow& block, RenderObject* o, LineMidpointState& lineMidpointState)
{
    RenderObject* next = bidiNextSkippingEmptyInlines(block, o);
    while (next && next->isFloatingOrOutOfFlowPositioned())
        next = bidiNextSkippingEmptyInlines(block, next);

    if (next && next->isText() && toRenderText(next)->textLength() > 0) {
        RenderText* nextText = toRenderText(next);
        UChar nextChar = nextText->characterAt(0);
        if (nextText->style().isCollapsibleWhiteSpace(nextChar)) {
            lineMidpointState.startIgnoringSpaces(InlineIterator(0, o, 0));
            return true;
        }
    }

    return false;
}

inline void BreakingContext::handleEmptyInline()
{
    RenderInline& flowBox = toRenderInline(*m_current.renderer());

    // This should only end up being called on empty inlines
    ASSERT(isEmptyInline(flowBox));

    // Now that some inline flows have line boxes, if we are already ignoring spaces, we need
    // to make sure that we stop to include this object and then start ignoring spaces again.
    // If this object is at the start of the line, we need to behave like list markers and
    // start ignoring spaces.
    bool requiresLineBox = alwaysRequiresLineBox(flowBox);
    if (requiresLineBox || requiresLineBoxForContent(flowBox, m_lineInfo)) {
        // An empty inline that only has line-height, vertical-align or font-metrics will only get a
        // line box to affect the height of the line if the rest of the line is not empty.
        if (requiresLineBox)
            m_lineInfo.setEmpty(false, &m_block, &m_width);
        if (m_ignoringSpaces) {
            m_trailingObjects.clear();
            m_lineMidpointState.ensureLineBoxInsideIgnoredSpaces(m_current.renderer());
        } else if (m_blockStyle.collapseWhiteSpace() && m_resolver.position().renderer() == m_current.renderer()
            && shouldSkipWhitespaceAfterStartObject(m_block, m_current.renderer(), m_lineMidpointState)) {
            // Like with list markers, we start ignoring spaces to make sure that any
            // additional spaces we see will be discarded.
            m_currentCharacterIsSpace = true;
            m_currentCharacterIsWS = true;
            m_ignoringSpaces = true;
        } else
            m_trailingObjects.appendBoxIfNeeded(&flowBox);
    }

    m_width.addUncommittedWidth(inlineLogicalWidth(m_current.renderer()) + borderPaddingMarginStart(flowBox) + borderPaddingMarginEnd(flowBox));
}

inline void BreakingContext::handleReplaced()
{
    RenderBox& replacedBox = toRenderBox(*m_current.renderer());

    if (m_atStart)
        m_width.updateAvailableWidth(replacedBox.logicalHeight());

    // Break on replaced elements if either has normal white-space.
    if ((m_autoWrap || RenderStyle::autoWrap(m_lastWS)) && (!m_current.renderer()->isImage() || m_allowImagesToBreak)) {
        m_width.commit();
        m_lineBreak.moveToStartOf(m_current.renderer());
    }

    if (m_ignoringSpaces)
        m_lineMidpointState.stopIgnoringSpaces(InlineIterator(0, m_current.renderer(), 0));

    m_lineInfo.setEmpty(false, &m_block, &m_width);
    m_ignoringSpaces = false;
    m_currentCharacterIsSpace = false;
    m_currentCharacterIsWS = false;
    m_trailingObjects.clear();

    // Optimize for a common case. If we can't find whitespace after the list
    // item, then this is all moot.
    LayoutUnit replacedLogicalWidth = m_block.logicalWidthForChild(replacedBox) + m_block.marginStartForChild(replacedBox) + m_block.marginEndForChild(replacedBox) + inlineLogicalWidth(m_current.renderer());
    if (m_current.renderer()->isListMarker()) {
        if (m_blockStyle.collapseWhiteSpace() && shouldSkipWhitespaceAfterStartObject(m_block, m_current.renderer(), m_lineMidpointState)) {
            // Like with inline flows, we start ignoring spaces to make sure that any
            // additional spaces we see will be discarded.
            m_currentCharacterIsSpace = true;
            m_currentCharacterIsWS = false;
            m_ignoringSpaces = true;
        }
        if (toRenderListMarker(*m_current.renderer()).isInside())
            m_width.addUncommittedWidth(replacedLogicalWidth);
    } else
        m_width.addUncommittedWidth(replacedLogicalWidth);
    if (m_current.renderer()->isRubyRun())
        m_width.applyOverhang(toRenderRubyRun(m_current.renderer()), m_lastObject, m_nextObject);
    // Update prior line break context characters, using U+FFFD (OBJECT REPLACEMENT CHARACTER) for replaced element.
    m_renderTextInfo.m_lineBreakIterator.updatePriorContext(replacementCharacter);
}

inline float firstPositiveWidth(const WordMeasurements& wordMeasurements)
{
    for (size_t i = 0; i < wordMeasurements.size(); ++i) {
        if (wordMeasurements[i].width > 0)
            return wordMeasurements[i].width;
    }
    return 0;
}

#if ENABLE(CSS_SHAPES) && ENABLE(CSS_SHAPE_INSIDE)
inline void updateSegmentsForShapes(RenderBlockFlow& block, const FloatingObject* lastFloatFromPreviousLine, const WordMeasurements& wordMeasurements, LineWidth& width, bool isFirstLine)
{
    ASSERT(lastFloatFromPreviousLine);

    ShapeInsideInfo* shapeInsideInfo = block.layoutShapeInsideInfo();
    if (!lastFloatFromPreviousLine->isPlaced() || !shapeInsideInfo)
        return;

    bool isHorizontalWritingMode = block.isHorizontalWritingMode();
    LayoutUnit logicalOffsetFromShapeContainer = block.logicalOffsetFromShapeAncestorContainer(&shapeInsideInfo->owner()).height();

    LayoutUnit lineLogicalTop = block.logicalHeight() + logicalOffsetFromShapeContainer;
    LayoutUnit lineLogicalHeight = block.lineHeight(isFirstLine, isHorizontalWritingMode ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
    LayoutUnit lineLogicalBottom = lineLogicalTop + lineLogicalHeight;

    LayoutUnit floatLogicalTop = block.logicalTopForFloat(lastFloatFromPreviousLine);
    LayoutUnit floatLogicalBottom = block.logicalBottomForFloat(lastFloatFromPreviousLine);

    bool lineOverlapsWithFloat = (floatLogicalTop < lineLogicalBottom) && (lineLogicalTop < floatLogicalBottom);
    if (!lineOverlapsWithFloat)
        return;

    float minSegmentWidth = firstPositiveWidth(wordMeasurements);

    LayoutUnit floatLogicalWidth = block.logicalWidthForFloat(lastFloatFromPreviousLine);
    LayoutUnit availableLogicalWidth = block.logicalWidth() - block.logicalRightForFloat(lastFloatFromPreviousLine);
    if (availableLogicalWidth < minSegmentWidth)
        block.setLogicalHeight(floatLogicalBottom);

    if (block.logicalHeight() < floatLogicalTop) {
        shapeInsideInfo->adjustLogicalLineTop(minSegmentWidth + floatLogicalWidth);
        block.setLogicalHeight(shapeInsideInfo->logicalLineTop() - logicalOffsetFromShapeContainer);
    }

    lineLogicalTop = block.logicalHeight() + logicalOffsetFromShapeContainer;

    shapeInsideInfo->updateSegmentsForLine(lineLogicalTop, lineLogicalHeight);
    width.updateCurrentShapeSegment();
    width.updateAvailableWidth();
}
#endif

inline bool iteratorIsBeyondEndOfRenderCombineText(const InlineIterator& iter, RenderCombineText& renderer)
{
    return iter.renderer() == &renderer && iter.offset() >= renderer.textLength();
}

inline void nextCharacter(UChar& currentCharacter, UChar& lastCharacter, UChar& secondToLastCharacter)
{
    secondToLastCharacter = lastCharacter;
    lastCharacter = currentCharacter;
}

// FIXME: Don't let counters mark themselves as needing pref width recalcs during layout
// so we don't need this hack.
inline void updateCounterIfNeeded(RenderText& renderText)
{
    if (!renderText.preferredLogicalWidthsDirty() || !renderText.isCounter())
        return;
    toRenderCounter(renderText).updateCounter();
}

inline float measureHyphenWidth(RenderText* renderer, const Font& font, HashSet<const SimpleFontData*>* fallbackFonts = 0)
{
    const RenderStyle& style = renderer->style();
    return font.width(RenderBlock::constructTextRun(renderer, font, style.hyphenString().string(), style), fallbackFonts);
}

ALWAYS_INLINE float textWidth(RenderText* text, unsigned from, unsigned len, const Font& font, float xPos, bool isFixedPitch, bool collapseWhiteSpace, HashSet<const SimpleFontData*>& fallbackFonts, TextLayout* layout = 0)
{
    const RenderStyle& style = text->style();

    GlyphOverflow glyphOverflow;
    if (isFixedPitch || (!from && len == text->textLength()) || style.hasTextCombine())
        return text->width(from, len, font, xPos, &fallbackFonts, &glyphOverflow);

    if (layout)
        return Font::width(*layout, from, len, &fallbackFonts);

    TextRun run = RenderBlock::constructTextRun(text, font, text, from, len, style);
    run.setCharactersLength(text->textLength() - from);
    ASSERT(run.charactersLength() >= run.length());

    run.setCharacterScanForCodePath(!text->canUseSimpleFontCodePath());
    run.setTabSize(!collapseWhiteSpace, style.tabSize());
    run.setXPos(xPos);
    return font.width(run, &fallbackFonts, &glyphOverflow);
}

// Adding a pair of midpoints before a character will split it out into a new line box.
inline void ensureCharacterGetsLineBox(LineMidpointState& lineMidpointState, InlineIterator& textParagraphSeparator)
{
    InlineIterator midpoint(0, textParagraphSeparator.renderer(), textParagraphSeparator.offset());
    lineMidpointState.startIgnoringSpaces(InlineIterator(0, textParagraphSeparator.renderer(), textParagraphSeparator.offset() - 1));
    lineMidpointState.stopIgnoringSpaces(InlineIterator(0, textParagraphSeparator.renderer(), textParagraphSeparator.offset()));
}

inline void tryHyphenating(RenderText* text, const Font& font, const AtomicString& localeIdentifier, unsigned consecutiveHyphenatedLines, int consecutiveHyphenatedLinesLimit, int minimumPrefixLimit, int minimumSuffixLimit, unsigned lastSpace, unsigned pos, float xPos, int availableWidth, bool isFixedPitch, bool collapseWhiteSpace, int lastSpaceWordSpacing, InlineIterator& lineBreak, int nextBreakable, bool& hyphenated)
{
    // Map 'hyphenate-limit-{before,after}: auto;' to 2.
    unsigned minimumPrefixLength;
    unsigned minimumSuffixLength;

    if (minimumPrefixLimit < 0)
        minimumPrefixLength = 2;
    else
        minimumPrefixLength = static_cast<unsigned>(minimumPrefixLimit);

    if (minimumSuffixLimit < 0)
        minimumSuffixLength = 2;
    else
        minimumSuffixLength = static_cast<unsigned>(minimumSuffixLimit);

    if (pos - lastSpace <= minimumSuffixLength)
        return;

    if (consecutiveHyphenatedLinesLimit >= 0 && consecutiveHyphenatedLines >= static_cast<unsigned>(consecutiveHyphenatedLinesLimit))
        return;

    int hyphenWidth = measureHyphenWidth(text, font);

    float maxPrefixWidth = availableWidth - xPos - hyphenWidth - lastSpaceWordSpacing;
    // If the maximum width available for the prefix before the hyphen is small, then it is very unlikely
    // that an hyphenation opportunity exists, so do not bother to look for it.
    if (maxPrefixWidth <= font.pixelSize() * 5 / 4)
        return;

    const RenderStyle& style = text->style();
    TextRun run = RenderBlock::constructTextRun(text, font, text, lastSpace, pos - lastSpace, style);
    run.setCharactersLength(text->textLength() - lastSpace);
    ASSERT(run.charactersLength() >= run.length());

    run.setTabSize(!collapseWhiteSpace, style.tabSize());
    run.setXPos(xPos + lastSpaceWordSpacing);

    unsigned prefixLength = font.offsetForPosition(run, maxPrefixWidth, false);
    if (prefixLength < minimumPrefixLength)
        return;

    prefixLength = lastHyphenLocation(text->deprecatedCharacters() + lastSpace, pos - lastSpace, std::min(prefixLength, pos - lastSpace - minimumSuffixLength) + 1, localeIdentifier);
    if (!prefixLength || prefixLength < minimumPrefixLength)
        return;

    // When lastSapce is a space, which it always is except sometimes at the beginning of a line or after collapsed
    // space, it should not count towards hyphenate-limit-before.
    if (prefixLength == minimumPrefixLength) {
        UChar characterAtLastSpace = text->characterAt(lastSpace);
        if (characterAtLastSpace == ' ' || characterAtLastSpace == '\n' || characterAtLastSpace == '\t' || characterAtLastSpace == noBreakSpace)
            return;
    }

    ASSERT(pos - lastSpace - prefixLength >= minimumSuffixLength);

#if !ASSERT_DISABLED
    HashSet<const SimpleFontData*> fallbackFonts;
    float prefixWidth = hyphenWidth + textWidth(text, lastSpace, prefixLength, font, xPos, isFixedPitch, collapseWhiteSpace, fallbackFonts) + lastSpaceWordSpacing;
    ASSERT(xPos + prefixWidth <= availableWidth);
#else
    UNUSED_PARAM(isFixedPitch);
#endif

    lineBreak.moveTo(text, lastSpace + prefixLength, nextBreakable);
    hyphenated = true;
}

inline bool BreakingContext::handleText(WordMeasurements& wordMeasurements, bool& hyphenated,  unsigned& consecutiveHyphenatedLines)
{
    if (!m_current.offset())
        m_appliedStartWidth = false;

    RenderText* renderText = toRenderText(m_current.renderer());

    bool isSVGText = renderText->isSVGInlineText();

    // If we have left a no-wrap inline and entered an autowrap inline while ignoring spaces
    // then we need to mark the start of the autowrap inline as a potential linebreak now.
    if (m_autoWrap && !RenderStyle::autoWrap(m_lastWS) && m_ignoringSpaces)
        commitLineBreakAtCurrentWidth(m_current.renderer());

    if (renderText->style().hasTextCombine() && m_current.renderer()->isCombineText() && !toRenderCombineText(*m_current.renderer()).isCombined()) {
        RenderCombineText& combineRenderer = toRenderCombineText(*m_current.renderer());
        combineRenderer.combineText();
        // The length of the renderer's text may have changed. Increment stale iterator positions
        if (iteratorIsBeyondEndOfRenderCombineText(m_lineBreak, combineRenderer)) {
            ASSERT(iteratorIsBeyondEndOfRenderCombineText(m_resolver.position(), combineRenderer));
            m_lineBreak.increment();
            m_resolver.increment();
        }
    }

    const RenderStyle& style = lineStyle(*renderText->parent(), m_lineInfo);
    const Font& font = style.font();
    bool isFixedPitch = font.isFixedPitch();
    bool canHyphenate = style.hyphens() == HyphensAuto && WebCore::canHyphenate(style.locale());

    unsigned lastSpace = m_current.offset();
    float wordSpacing = m_currentStyle->font().wordSpacing();
    float lastSpaceWordSpacing = 0;
    float wordSpacingForWordMeasurement = 0;

    float wrapW = m_width.uncommittedWidth() + inlineLogicalWidth(m_current.renderer(), !m_appliedStartWidth, true);
    float charWidth = 0;
    bool breakNBSP = m_autoWrap && m_currentStyle->nbspMode() == SPACE;
    // Auto-wrapping text should wrap in the middle of a word only if it could not wrap before the word,
    // which is only possible if the word is the first thing on the line, that is, if |w| is zero.
    bool breakWords = m_currentStyle->breakWords() && ((m_autoWrap && !m_width.committedWidth()) || m_currWS == PRE);
    bool midWordBreak = false;
    bool breakAll = m_currentStyle->wordBreak() == BreakAllWordBreak && m_autoWrap;
    float hyphenWidth = 0;

    if (isSVGText) {
        breakWords = false;
        breakAll = false;
    }

    if (m_renderTextInfo.m_text != renderText) {
        updateCounterIfNeeded(*renderText);
        m_renderTextInfo.m_text = renderText;
        m_renderTextInfo.m_font = &font;
        m_renderTextInfo.m_layout = font.createLayout(renderText, m_width.currentWidth(), m_collapseWhiteSpace);
        m_renderTextInfo.m_lineBreakIterator.resetStringAndReleaseIterator(renderText->text(), style.locale());
    } else if (m_renderTextInfo.m_layout && m_renderTextInfo.m_font != &font) {
        m_renderTextInfo.m_font = &font;
        m_renderTextInfo.m_layout = font.createLayout(renderText, m_width.currentWidth(), m_collapseWhiteSpace);
    }

    TextLayout* textLayout = m_renderTextInfo.m_layout.get();

    // Non-zero only when kerning is enabled and TextLayout isn't used, in which case we measure
    // words with their trailing space, then subtract its width.
    HashSet<const SimpleFontData*> fallbackFonts;
    float wordTrailingSpaceWidth = (font.typesettingFeatures() & Kerning) && !textLayout ? font.width(RenderBlock::constructTextRun(renderText, font, &space, 1, style), &fallbackFonts) + wordSpacing : 0;

    UChar lastCharacter = m_renderTextInfo.m_lineBreakIterator.lastCharacter();
    UChar secondToLastCharacter = m_renderTextInfo.m_lineBreakIterator.secondToLastCharacter();
    for (; m_current.offset() < renderText->textLength(); m_current.fastIncrementInTextNode()) {
        bool previousCharacterIsSpace = m_currentCharacterIsSpace;
        bool previousCharacterIsWS = m_currentCharacterIsWS;
        UChar c = m_current.current();
        m_currentCharacterIsSpace = c == ' ' || c == '\t' || (!m_preservesNewline && (c == '\n'));

        if (!m_collapseWhiteSpace || !m_currentCharacterIsSpace)
            m_lineInfo.setEmpty(false, &m_block, &m_width);

        if (c == softHyphen && m_autoWrap && !hyphenWidth && style.hyphens() != HyphensNone) {
            hyphenWidth = measureHyphenWidth(renderText, font, &fallbackFonts);
            m_width.addUncommittedWidth(hyphenWidth);
        }

        bool applyWordSpacing = false;

        m_currentCharacterIsWS = m_currentCharacterIsSpace || (breakNBSP && c == noBreakSpace);

        if ((breakAll || breakWords) && !midWordBreak && (!m_currentCharacterIsSpace || style.whiteSpace() != PRE_WRAP)) {
            wrapW += charWidth;
            bool midWordBreakIsBeforeSurrogatePair = U16_IS_LEAD(c) && m_current.offset() + 1 < renderText->textLength() && U16_IS_TRAIL((*renderText)[m_current.offset() + 1]);
            charWidth = textWidth(renderText, m_current.offset(), midWordBreakIsBeforeSurrogatePair ? 2 : 1, font, m_width.committedWidth() + wrapW, isFixedPitch, m_collapseWhiteSpace, fallbackFonts, textLayout);
            midWordBreak = m_width.committedWidth() + wrapW + charWidth > m_width.availableWidth();
        }

        int nextBreakablePosition = m_current.nextBreakablePosition();
        bool betweenWords = c == '\n' || (m_currWS != PRE && !m_atStart && isBreakable(m_renderTextInfo.m_lineBreakIterator, m_current.offset(), nextBreakablePosition, breakNBSP)
            && (style.hyphens() != HyphensNone || (m_current.previousInSameNode() != softHyphen)));
        m_current.setNextBreakablePosition(nextBreakablePosition);

        if (betweenWords || midWordBreak) {
            bool stoppedIgnoringSpaces = false;
            if (m_ignoringSpaces) {
                lastSpaceWordSpacing = 0;
                if (!m_currentCharacterIsSpace) {
                    // Stop ignoring spaces and begin at this
                    // new point.
                    m_ignoringSpaces = false;
                    wordSpacingForWordMeasurement = 0;
                    lastSpace = m_current.offset(); // e.g., "Foo    goo", don't add in any of the ignored spaces.
                    m_lineMidpointState.stopIgnoringSpaces(InlineIterator(0, m_current.renderer(), m_current.offset()));
                    stoppedIgnoringSpaces = true;
                } else {
                    // Just keep ignoring these spaces.
                    nextCharacter(c, lastCharacter, secondToLastCharacter);
                    continue;
                }
            }

            wordMeasurements.grow(wordMeasurements.size() + 1);
            WordMeasurement& wordMeasurement = wordMeasurements.last();

            wordMeasurement.renderer = renderText;
            wordMeasurement.endOffset = m_current.offset();
            wordMeasurement.startOffset = lastSpace;

            float additionalTempWidth;
            if (wordTrailingSpaceWidth && c == ' ')
                additionalTempWidth = textWidth(renderText, lastSpace, m_current.offset() + 1 - lastSpace, font, m_width.currentWidth(), isFixedPitch, m_collapseWhiteSpace, wordMeasurement.fallbackFonts, textLayout) - wordTrailingSpaceWidth;
            else
                additionalTempWidth = textWidth(renderText, lastSpace, m_current.offset() - lastSpace, font, m_width.currentWidth(), isFixedPitch, m_collapseWhiteSpace, wordMeasurement.fallbackFonts, textLayout);

            if (wordMeasurement.fallbackFonts.isEmpty() && !fallbackFonts.isEmpty())
                wordMeasurement.fallbackFonts.swap(fallbackFonts);
            fallbackFonts.clear();

            wordMeasurement.width = additionalTempWidth + wordSpacingForWordMeasurement;
            additionalTempWidth += lastSpaceWordSpacing;
            m_width.addUncommittedWidth(additionalTempWidth);

            if (m_collapseWhiteSpace && previousCharacterIsSpace && m_currentCharacterIsSpace && additionalTempWidth)
                m_width.setTrailingWhitespaceWidth(additionalTempWidth);

            if (!m_appliedStartWidth) {
                m_width.addUncommittedWidth(inlineLogicalWidth(m_current.renderer(), true, false));
                m_appliedStartWidth = true;
            }

#if ENABLE(CSS_SHAPES) && ENABLE(CSS_SHAPE_INSIDE)
            if (m_lastFloatFromPreviousLine)
                updateSegmentsForShapes(m_block, m_lastFloatFromPreviousLine, wordMeasurements, m_width, m_lineInfo.isFirstLine());
#endif
            applyWordSpacing = wordSpacing && m_currentCharacterIsSpace;

            if (!m_width.committedWidth() && m_autoWrap && !m_width.fitsOnLine())
                m_width.fitBelowFloats(m_lineInfo.isFirstLine());

            if (m_autoWrap || breakWords) {
                // If we break only after white-space, consider the current character
                // as candidate width for this line.
                bool lineWasTooWide = false;
                if (m_width.fitsOnLine() && m_currentCharacterIsWS && m_currentStyle->breakOnlyAfterWhiteSpace() && !midWordBreak) {
                    float charWidth = textWidth(renderText, m_current.offset(), 1, font, m_width.currentWidth(), isFixedPitch, m_collapseWhiteSpace, wordMeasurement.fallbackFonts, textLayout) + (applyWordSpacing ? wordSpacing : 0);
                    // Check if line is too big even without the extra space
                    // at the end of the line. If it is not, do nothing.
                    // If the line needs the extra whitespace to be too long,
                    // then move the line break to the space and skip all
                    // additional whitespace.
                    if (!m_width.fitsOnLineIncludingExtraWidth(charWidth)) {
                        lineWasTooWide = true;
                        m_lineBreak.moveTo(m_current.renderer(), m_current.offset(), m_current.nextBreakablePosition());
                        m_lineBreaker.skipTrailingWhitespace(m_lineBreak, m_lineInfo);
                    }
                }
                if (lineWasTooWide || !m_width.fitsOnLine()) {
                    if (canHyphenate && !m_width.fitsOnLine()) {
                        tryHyphenating(renderText, font, style.locale(), consecutiveHyphenatedLines, m_blockStyle.hyphenationLimitLines(), style.hyphenationLimitBefore(), style.hyphenationLimitAfter(), lastSpace, m_current.offset(), m_width.currentWidth() - additionalTempWidth, m_width.availableWidth(), isFixedPitch, m_collapseWhiteSpace, lastSpaceWordSpacing, m_lineBreak, m_current.nextBreakablePosition(), m_lineBreaker.m_hyphenated);
                        if (m_lineBreaker.m_hyphenated) {
                            m_atEnd = true;
                            return false;
                        }
                    }
                    if (m_lineBreak.atTextParagraphSeparator()) {
                        if (!stoppedIgnoringSpaces && m_current.offset() > 0)
                            ensureCharacterGetsLineBox(m_lineMidpointState, m_current);
                        m_lineBreak.increment();
                        m_lineInfo.setPreviousLineBrokeCleanly(true);
                        wordMeasurement.endOffset = m_lineBreak.offset();
                    }
                    if (m_lineBreak.renderer() && m_lineBreak.offset() && m_lineBreak.renderer()->isText() && toRenderText(m_lineBreak.renderer())->textLength() && toRenderText(m_lineBreak.renderer())->characterAt(m_lineBreak.offset() - 1) == softHyphen && style.hyphens() != HyphensNone)
                        hyphenated = true;
                    if (m_lineBreak.offset() && m_lineBreak.offset() != (unsigned)wordMeasurement.endOffset && !wordMeasurement.width) {
                        if (charWidth) {
                            wordMeasurement.endOffset = m_lineBreak.offset();
                            wordMeasurement.width = charWidth;
                        }
                    }
                    // Didn't fit. Jump to the end unless there's still an opportunity to collapse whitespace.
                    if (m_ignoringSpaces || !m_collapseWhiteSpace || !m_currentCharacterIsSpace || !previousCharacterIsSpace) {
                        m_atEnd = true;
                        return false;
                    }
                } else {
                    if (!betweenWords || (midWordBreak && !m_autoWrap))
                        m_width.addUncommittedWidth(-additionalTempWidth);
                    if (hyphenWidth) {
                        // Subtract the width of the soft hyphen out since we fit on a line.
                        m_width.addUncommittedWidth(-hyphenWidth);
                        hyphenWidth = 0;
                    }
                }
            }

            if (c == '\n' && m_preservesNewline) {
                if (!stoppedIgnoringSpaces && m_current.offset())
                    ensureCharacterGetsLineBox(m_lineMidpointState, m_current);
                m_lineBreak.moveTo(m_current.renderer(), m_current.offset(), m_current.nextBreakablePosition());
                m_lineBreak.increment();
                m_lineInfo.setPreviousLineBrokeCleanly(true);
                return true;
            }

            if (m_autoWrap && betweenWords) {
                m_width.commit();
                wrapW = 0;
                m_lineBreak.moveTo(m_current.renderer(), m_current.offset(), m_current.nextBreakablePosition());
                // Auto-wrapping text should not wrap in the middle of a word once it has had an
                // opportunity to break after a word.
                breakWords = false;
            }

            if (midWordBreak && !U16_IS_TRAIL(c) && !(U_GET_GC_MASK(c) & U_GC_M_MASK)) {
                // Remember this as a breakable position in case
                // adding the end width forces a break.
                m_lineBreak.moveTo(m_current.renderer(), m_current.offset(), m_current.nextBreakablePosition());
                midWordBreak &= (breakWords || breakAll);
            }

            if (betweenWords) {
                lastSpaceWordSpacing = applyWordSpacing ? wordSpacing : 0;
                wordSpacingForWordMeasurement = (applyWordSpacing && wordMeasurement.width) ? wordSpacing : 0;
                lastSpace = m_current.offset();
            }

            if (!m_ignoringSpaces && m_currentStyle->collapseWhiteSpace()) {
                // If we encounter a newline, or if we encounter a
                // second space, we need to go ahead and break up this
                // run and enter a mode where we start collapsing spaces.
                if (m_currentCharacterIsSpace && previousCharacterIsSpace) {
                    m_ignoringSpaces = true;

                    // We just entered a mode where we are ignoring
                    // spaces. Create a midpoint to terminate the run
                    // before the second space.
                    m_lineMidpointState.startIgnoringSpaces(m_startOfIgnoredSpaces);
                    m_trailingObjects.updateMidpointsForTrailingBoxes(m_lineMidpointState, InlineIterator(), TrailingObjects::DoNotCollapseFirstSpace);
                }
            }
        } else if (m_ignoringSpaces) {
            // Stop ignoring spaces and begin at this
            // new point.
            m_ignoringSpaces = false;
            lastSpaceWordSpacing = applyWordSpacing ? wordSpacing : 0;
            wordSpacingForWordMeasurement = (applyWordSpacing && wordMeasurements.last().width) ? wordSpacing : 0;
            lastSpace = m_current.offset(); // e.g., "Foo    goo", don't add in any of the ignored spaces.
            m_lineMidpointState.stopIgnoringSpaces(InlineIterator(0, m_current.renderer(), m_current.offset()));
        }

        if (isSVGText && m_current.offset()) {
            // Force creation of new InlineBoxes for each absolute positioned character (those that start new text chunks).
            if (toRenderSVGInlineText(renderText)->characterStartsNewTextChunk(m_current.offset()))
                ensureCharacterGetsLineBox(m_lineMidpointState, m_current);
        }

        if (m_currentCharacterIsSpace && !previousCharacterIsSpace) {
            m_startOfIgnoredSpaces.setRenderer(m_current.renderer());
            m_startOfIgnoredSpaces.setOffset(m_current.offset());
            // Spaces after right-aligned text and before a line-break get collapsed away completely so that the trailing
            // space doesn't seem to push the text out from the right-hand edge.
            // FIXME: Do this regardless of the container's alignment - will require rebaselining a lot of test results.
            if (m_nextObject && m_nextObject->isBR() && (m_blockStyle.textAlign() == RIGHT || m_blockStyle.textAlign() == WEBKIT_RIGHT)) {
                m_startOfIgnoredSpaces.setOffset(m_startOfIgnoredSpaces.offset() - 1);
                // If there's just a single trailing space start ignoring it now so it collapses away.
                if (m_current.offset() == renderText->textLength() - 1)
                    m_lineMidpointState.startIgnoringSpaces(m_startOfIgnoredSpaces);
            }
        }

        if (!m_currentCharacterIsWS && previousCharacterIsWS) {
            if (m_autoWrap && m_currentStyle->breakOnlyAfterWhiteSpace())
                m_lineBreak.moveTo(m_current.renderer(), m_current.offset(), m_current.nextBreakablePosition());
        }

        if (m_collapseWhiteSpace && m_currentCharacterIsSpace && !m_ignoringSpaces)
            m_trailingObjects.setTrailingWhitespace(toRenderText(m_current.renderer()));
        else if (!m_currentStyle->collapseWhiteSpace() || !m_currentCharacterIsSpace)
            m_trailingObjects.clear();

        m_atStart = false;
        nextCharacter(c, lastCharacter, secondToLastCharacter);
    }

    m_renderTextInfo.m_lineBreakIterator.setPriorContext(lastCharacter, secondToLastCharacter);

    wordMeasurements.grow(wordMeasurements.size() + 1);
    WordMeasurement& wordMeasurement = wordMeasurements.last();
    wordMeasurement.renderer = renderText;

    // IMPORTANT: current.m_pos is > length here!
    float additionalTempWidth = m_ignoringSpaces ? 0 : textWidth(renderText, lastSpace, m_current.offset() - lastSpace, font, m_width.currentWidth(), isFixedPitch, m_collapseWhiteSpace, wordMeasurement.fallbackFonts, textLayout);
    wordMeasurement.startOffset = lastSpace;
    wordMeasurement.endOffset = m_current.offset();
    wordMeasurement.width = m_ignoringSpaces ? 0 : additionalTempWidth + wordSpacingForWordMeasurement;
    additionalTempWidth += lastSpaceWordSpacing;

    float inlineLogicalTempWidth = inlineLogicalWidth(m_current.renderer(), !m_appliedStartWidth, m_includeEndWidth);
    m_width.addUncommittedWidth(additionalTempWidth + inlineLogicalTempWidth);

    if (wordMeasurement.fallbackFonts.isEmpty() && !fallbackFonts.isEmpty())
        wordMeasurement.fallbackFonts.swap(fallbackFonts);
    fallbackFonts.clear();

    if (m_collapseWhiteSpace && m_currentCharacterIsSpace && additionalTempWidth)
        m_width.setTrailingWhitespaceWidth(additionalTempWidth, inlineLogicalTempWidth);

    m_includeEndWidth = false;

    if (!m_width.fitsOnLine()) {
        if (canHyphenate)
            tryHyphenating(renderText, font, style.locale(), consecutiveHyphenatedLines, m_blockStyle.hyphenationLimitLines(), style.hyphenationLimitBefore(), style.hyphenationLimitAfter(), lastSpace, m_current.offset(), m_width.currentWidth() - additionalTempWidth, m_width.availableWidth(), isFixedPitch, m_collapseWhiteSpace, lastSpaceWordSpacing, m_lineBreak, m_current.nextBreakablePosition(), m_lineBreaker.m_hyphenated);

        if (!hyphenated && m_lineBreak.previousInSameNode() == softHyphen && style.hyphens() != HyphensNone) {
            hyphenated = true;
            m_atEnd = true;
        }
    }
    return false;
}

inline bool textBeginsWithBreakablePosition(RenderObject* next)
{
    ASSERT(next->isText());
    RenderText* nextText = toRenderText(next);
    if (!nextText->textLength())
        return false;
    UChar c = nextText->characterAt(0);
    return c == ' ' || c == '\t' || (c == '\n' && !nextText->preservesNewline());
}

inline bool BreakingContext::canBreakAtThisPosition()
{
    // If we are no-wrap and have found a line-breaking opportunity already then we should take it.
    if (m_width.committedWidth() && !m_width.fitsOnLine(m_currentCharacterIsSpace) && m_currWS == NOWRAP)
        return true;

    // Avoid breaking before empty inlines.
    if (m_nextObject && m_nextObject->isRenderInline() && isEmptyInline(toRenderInline(*m_nextObject)))
        return false;

    // Return early if we autowrap and the current character is a space as we will always want to break at such a position.
    if (m_autoWrap && m_currentCharacterIsSpace)
        return true;

    if (m_nextObject && m_nextObject->isLineBreakOpportunity())
        return m_autoWrap;

    bool nextIsAutoWrappingText = (m_nextObject && m_nextObject->isText() && (m_autoWrap || m_nextObject->style().autoWrap()));
    if (!nextIsAutoWrappingText)
        return m_autoWrap;
    bool currentIsTextOrEmptyInline = m_current.renderer()->isText() || (m_current.renderer()->isRenderInline() && isEmptyInline(toRenderInline(*m_current.renderer())));
    if (!currentIsTextOrEmptyInline)
        return m_autoWrap;

    bool canBreakHere = !m_currentCharacterIsSpace && textBeginsWithBreakablePosition(m_nextObject);

    // See if attempting to fit below floats creates more available width on the line.
    if (!m_width.fitsOnLine() && !m_width.committedWidth())
        m_width.fitBelowFloats(m_lineInfo.isFirstLine());

    bool canPlaceOnLine = m_width.fitsOnLine() || !m_autoWrapWasEverTrueOnLine;

    if (canPlaceOnLine && canBreakHere)
        commitLineBreakAtCurrentWidth(m_nextObject);

    return canBreakHere;
}

inline void BreakingContext::commitAndUpdateLineBreakIfNeeded()
{
    bool checkForBreak = canBreakAtThisPosition();

    if (checkForBreak && !m_width.fitsOnLine(m_ignoringSpaces)) {
        // if we have floats, try to get below them.
        if (m_currentCharacterIsSpace && !m_ignoringSpaces && m_currentStyle->collapseWhiteSpace())
            m_trailingObjects.clear();

        if (m_width.committedWidth()) {
            m_atEnd = true;
            return;
        }

        m_width.fitBelowFloats(m_lineInfo.isFirstLine());

        // |width| may have been adjusted because we got shoved down past a float (thus
        // giving us more room), so we need to retest, and only jump to
        // the end label if we still don't fit on the line. -dwh
        if (!m_width.fitsOnLine(m_ignoringSpaces)) {
            m_atEnd = true;
            return;
        }
    } else if (m_blockStyle.autoWrap() && !m_width.fitsOnLine() && !m_width.committedWidth()) {
        // If the container autowraps but the current child does not then we still need to ensure that it
        // wraps and moves below any floats.
        m_width.fitBelowFloats(m_lineInfo.isFirstLine());
    }

    if (!m_current.renderer()->isFloatingOrOutOfFlowPositioned()) {
        m_lastObject = m_current.renderer();
        if (m_lastObject->isReplaced() && m_autoWrap && (!m_lastObject->isImage() || m_allowImagesToBreak) && (!m_lastObject->isListMarker() || toRenderListMarker(*m_lastObject).isInside())) {
            m_width.commit();
            m_lineBreak.moveToStartOf(m_nextObject);
        }
    }
}

inline void checkMidpoints(LineMidpointState& lineMidpointState, InlineIterator& lBreak)
{
    // Check to see if our last midpoint is a start point beyond the line break. If so,
    // shave it off the list, and shave off a trailing space if the previous end point doesn't
    // preserve whitespace.
    if (lBreak.renderer() && lineMidpointState.numMidpoints() && !(lineMidpointState.numMidpoints() % 2)) {
        InlineIterator* midpoints = lineMidpointState.midpoints().data();
        InlineIterator& endpoint = midpoints[lineMidpointState.numMidpoints() - 2];
        const InlineIterator& startpoint = midpoints[lineMidpointState.numMidpoints() - 1];
        InlineIterator currpoint = endpoint;
        while (!currpoint.atEnd() && currpoint != startpoint && currpoint != lBreak)
            currpoint.increment();
        if (currpoint == lBreak) {
            // We hit the line break before the start point. Shave off the start point.
            lineMidpointState.decreaseNumMidpoints();
            if (endpoint.renderer()->style().collapseWhiteSpace() && endpoint.renderer()->isText())
                endpoint.setOffset(endpoint.offset() - 1);
        }
    }
}

inline InlineIterator BreakingContext::handleEndOfLine()
{
#if ENABLE(CSS_SHAPES) && ENABLE(CSS_SHAPE_INSIDE)
    ShapeInsideInfo* shapeInfo = m_block.layoutShapeInsideInfo();
    bool segmentAllowsOverflow = !shapeInfo || !shapeInfo->hasSegments();
#else
    bool segmentAllowsOverflow = true;
#endif
    if (segmentAllowsOverflow) {
        if (m_lineBreak == m_resolver.position()) {
            if (!m_lineBreak.renderer() || !m_lineBreak.renderer()->isBR()) {
                // we just add as much as possible
                if (m_blockStyle.whiteSpace() == PRE && !m_current.offset()) {
                    m_lineBreak.moveTo(m_lastObject, m_lastObject->isText() ? m_lastObject->length() : 0);
                } else if (m_lineBreak.renderer()) {
                    // Don't ever break in the middle of a word if we can help it.
                    // There's no room at all. We just have to be on this line,
                    // even though we'll spill out.
                    m_lineBreak.moveTo(m_current.renderer(), m_current.offset());
                }
            }
            // make sure we consume at least one char/object.
            if (m_lineBreak == m_resolver.position())
                m_lineBreak.increment();
        } else if (!m_current.offset() && !m_width.committedWidth() && m_width.uncommittedWidth() && !m_hadUncommittedWidthBeforeCurrent) {
            // Do not push the current object to the next line, when this line has some content, but it is still considered empty.
            // Empty inline elements like <span></span> can produce such lines and now we just ignore these break opportunities
            // at the start of a line, if no width has been committed yet.
            // Behave as if it was actually empty and consume at least one object.
            m_lineBreak.increment();
        }
    }

    // Sanity check our midpoints.
    checkMidpoints(m_lineMidpointState, m_lineBreak);

    m_trailingObjects.updateMidpointsForTrailingBoxes(m_lineMidpointState, m_lineBreak, TrailingObjects::CollapseFirstSpace);

    // We might have made lineBreak an iterator that points past the end
    // of the object. Do this adjustment to make it point to the start
    // of the next object instead to avoid confusing the rest of the
    // code.
    if (m_lineBreak.offset()) {
        m_lineBreak.setOffset(m_lineBreak.offset() - 1);
        m_lineBreak.increment();
    }

    return m_lineBreak;
}

}

#endif // BreakingContextInlineHeaders_h
