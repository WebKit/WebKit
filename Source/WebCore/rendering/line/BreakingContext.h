/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All right reserved.
 * Copyright (C) 2010-2015 Google Inc. All rights reserved.
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

#pragma once

#include "BreakLines.h"
#include "LegacyInlineIteratorInlines.h"
#include "LineBreaker.h"
#include "LineInfo.h"
#include "LineInlineHeaders.h"
#include "LineWidth.h"
#include "RenderBlockInlines.h"
#include "RenderCounter.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLineBreak.h"
#include "RenderListMarker.h"
#include "RenderObjectInlines.h"
#include "RenderSVGInlineText.h"
#include "RenderTextInlines.h"
#include "TrailingObjects.h"
#include "WordTrailingSpace.h"
#include <wtf/Function.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/StringView.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

// We don't let our line box tree for a single line get any deeper than this.
const unsigned cMaxLineDepth = 200;

class BreakingContext {
public:
    BreakingContext(LineBreaker& lineBreaker, InlineBidiResolver& resolver, LineInfo& inLineInfo, LineWidth& lineWidth, RenderTextInfo& inRenderTextInfo, bool appliedStartWidth, RenderBlockFlow& block)
        : m_lineBreaker(lineBreaker)
        , m_resolver(resolver)
        , m_current(resolver.position())
        , m_lineBreak(resolver.position())
        , m_block(block)
        , m_lastObject(m_current.renderer())
        , m_nextObject(nullptr)
        , m_blockStyle(block.style())
        , m_lineInfo(inLineInfo)
        , m_renderTextInfo(inRenderTextInfo)
        , m_width(lineWidth)
        , m_preservesNewline(false)
        , m_atStart(true)
        , m_ignoringSpaces(false)
        , m_currentCharacterIsSpace(false)
        , m_currentCharacterIsWS(false)
        , m_hasFormerOpportunity(false)
        , m_appliedStartWidth(appliedStartWidth)
        , m_includeEndWidth(true)
        , m_autoWrap(false)
        , m_autoWrapWasEverTrueOnLine(false)
        , m_collapseWhiteSpace(false)
        , m_allowImagesToBreak(!block.document().inQuirksMode() || !block.isRenderTableCell() || !m_blockStyle.logicalWidth().isIntrinsicOrAuto())
        , m_atEnd(false)
        , m_hadUncommittedWidthBeforeCurrent(false)
        , m_lineWhitespaceCollapsingState(resolver.whitespaceCollapsingState())
    {
    }

    RenderObject* currentObject() { return m_current.renderer(); }
    LegacyInlineIterator lineBreak() { return m_lineBreak; }
    LineWidth& lineWidth() { return m_width; }
    bool atEnd() { return m_atEnd; }
    
    bool fitsOnLineOrHangsAtEnd() const { return m_width.fitsOnLine() || m_hangsAtEnd; }

    void initializeForCurrentObject();

    void increment();

    void handleEmptyInline();
    bool handleText();
    void trailingSpacesHang(LegacyInlineIterator&, RenderObject&, bool canBreakMidWord, bool previousCharacterIsSpace);
    bool canBreakAtThisPosition();
    void commitAndUpdateLineBreakIfNeeded();
    LegacyInlineIterator handleEndOfLine();

    void clearLineBreakIfFitsOnLine(bool ignoringTrailingSpace = false)
    {
        if (m_width.fitsOnLine(ignoringTrailingSpace) || (m_lastObjectTextWrap == TextWrapMode::NoWrap && m_lastObjectWhitespaceCollapse == WhiteSpaceCollapse::Collapse) || m_hangsAtEnd)
            m_lineBreak.clear();
        m_hangsAtEnd = false;
    }

    void commitLineBreakClear()
    {
        m_width.commit();
        m_lineBreak.clear();
        m_hangsAtEnd = false;
    }

    void commitLineBreakAtCurrentWidth(RenderObject& object, unsigned offset = 0, std::optional<unsigned> nextBreak = std::optional<unsigned>())
    {
        m_width.commit();
        m_lineBreak.moveTo(object, offset, nextBreak);
        m_hangsAtEnd = false;
    }

private:
    LineBreaker& m_lineBreaker;
    InlineBidiResolver& m_resolver;

    LegacyInlineIterator m_current;
    LegacyInlineIterator m_lineBreak;
    LegacyInlineIterator m_startOfIgnoredSpaces;

    RenderBlockFlow& m_block;
    RenderObject* m_lastObject;
    RenderObject* m_nextObject;

    // Firefox and Opera will allow a table cell to grow to fit an image inside it under
    // very specific circumstances (in order to match common WinIE renderings).
    // Not supporting the quirk has caused us to mis-render some real sites. (See Bugzilla 10517.)
    const RenderStyle& m_blockStyle;

    LineInfo& m_lineInfo;

    RenderTextInfo& m_renderTextInfo;

    LineWidth m_width;

    TextWrapMode m_currentTextWrap { TextWrapMode::Wrap };
    TextWrapMode m_lastObjectTextWrap { TextWrapMode::Wrap };

    WhiteSpaceCollapse m_currentWhitespaceCollapse { WhiteSpaceCollapse::Collapse };
    WhiteSpaceCollapse m_lastObjectWhitespaceCollapse { WhiteSpaceCollapse::Collapse };

    bool m_preservesNewline;
    bool m_atStart;

    // This variable is used only if whitespace isn't set to WhiteSpace::Pre, and it tells us whether
    // or not we are currently ignoring whitespace.
    bool m_ignoringSpaces;

    // This variable tracks whether the very last character we saw was a space. We use
    // this to detect when we encounter a second space so we know we have to terminate
    // a run.
    bool m_currentCharacterIsSpace;
    bool m_currentCharacterIsWS;
    bool m_hasFormerOpportunity;
    bool m_appliedStartWidth;
    bool m_includeEndWidth;
    bool m_autoWrap;
    bool m_autoWrapWasEverTrueOnLine;
    bool m_collapseWhiteSpace;
    bool m_allowImagesToBreak;
    bool m_atEnd;
    bool m_hadUncommittedWidthBeforeCurrent;
    
    bool m_hangsAtEnd { false };

    LineWhitespaceCollapsingState& m_lineWhitespaceCollapsingState;

    TrailingObjects m_trailingObjects;
};

inline void BreakingContext::initializeForCurrentObject()
{
    m_hadUncommittedWidthBeforeCurrent = !!m_width.uncommittedWidth();
    auto& renderer = *m_current.renderer();

    m_nextObject = nextInlineRendererSkippingEmpty(m_block, &renderer);
    if (m_nextObject && m_nextObject->parent() && !m_nextObject->parent()->isDescendantOf(renderer.parent()))
        m_includeEndWidth = true;

    m_currentTextWrap = renderer.isReplacedOrInlineBlock() ? renderer.parent()->style().textWrapMode() : renderer.style().textWrapMode();
    m_currentWhitespaceCollapse = renderer.isReplacedOrInlineBlock() ? renderer.parent()->style().whiteSpaceCollapse() : renderer.style().whiteSpaceCollapse();

    m_lastObjectTextWrap = m_lastObject->isReplacedOrInlineBlock() ? m_lastObject->parent()->style().textWrapMode() : m_lastObject->style().textWrapMode();
    m_lastObjectWhitespaceCollapse = m_lastObject->isReplacedOrInlineBlock() ? m_lastObject->parent()->style().whiteSpaceCollapse() : m_lastObject->style().whiteSpaceCollapse();

    bool isSVGText = renderer.isRenderSVGInlineText();
    m_autoWrap = !isSVGText && m_currentTextWrap != TextWrapMode::NoWrap;
    m_autoWrapWasEverTrueOnLine = m_autoWrapWasEverTrueOnLine || m_autoWrap;

    m_preservesNewline = !isSVGText && m_currentWhitespaceCollapse != WhiteSpaceCollapse::Collapse;

    m_collapseWhiteSpace = m_currentWhitespaceCollapse == WhiteSpaceCollapse::Collapse || m_currentWhitespaceCollapse == WhiteSpaceCollapse::PreserveBreaks;
}

inline void BreakingContext::increment()
{
    // Clear out our character space bool, since inline <pre>s don't collapse whitespace
    // with adjacent inline normal/nowrap spans.
    if (!m_collapseWhiteSpace)
        m_currentCharacterIsSpace = false;

    if (m_nextObject)
        m_current.moveToStartOf(*m_nextObject);
    else
        m_current.clear();
    m_atStart = false;
}

inline LayoutUnit borderPaddingMarginStart(const RenderInline& child)
{
    return child.marginStart() + child.paddingStart() + child.borderStart();
}

inline LayoutUnit borderPaddingMarginEnd(const RenderInline& child)
{
    return child.marginEnd() + child.paddingEnd() + child.borderEnd();
}

inline LayoutUnit inlineLogicalWidth(const RenderObject& renderer, bool checkStartEdge = true, bool checkEndEdge = true)
{
    auto previousInFlowSibling = [] (const auto& renderer) {
        auto* previousSibling = renderer.previousSibling();
        for (; previousSibling && previousSibling->isOutOfFlowPositioned(); previousSibling = previousSibling->previousSibling()) { }
        return previousSibling;
    };

    auto shouldAddBorderPaddingMargin = [] (const auto& renderer) {
        // When deciding whether we're at the edge of an inline, adjacent collapsed whitespace is the same as no sibling at all.
        auto* textRenderer = dynamicDowncast<RenderText>(renderer);
        if (textRenderer && !textRenderer->text().length())
            return true;
        if (is<RenderLineBreak>(renderer) && renderer.parent()->style().boxDecorationBreak() == BoxDecorationBreak::Clone)
            return true;
        return false;
    };

    unsigned lineDepth = 1;
    auto extraWidth = LayoutUnit { };
    auto* parent = dynamicDowncast<RenderInline>(renderer.parent());
    auto* child = &renderer;
    while (parent && lineDepth++ < cMaxLineDepth) {
        if (!isEmptyInline(*parent)) {
            auto* previousSibling = previousInFlowSibling(*child);
            checkStartEdge = checkStartEdge && (!previousSibling || shouldAddBorderPaddingMargin(*previousSibling));
            if (checkStartEdge)
                extraWidth += borderPaddingMarginStart(*parent);
            auto* nextSibling = child->nextSibling();
            checkEndEdge = checkEndEdge && (!nextSibling || shouldAddBorderPaddingMargin(*nextSibling));
            if (checkEndEdge)
                extraWidth += borderPaddingMarginEnd(*parent);
            if (!checkStartEdge && !checkEndEdge)
                return extraWidth;
        }
        child = parent;
        ASSERT(child->parent());
        parent = dynamicDowncast<RenderInline>(child->parent());
    }
    return extraWidth;
}

// This is currently just used for list markers and inline flows that have line boxes. Neither should
// have an effect on whitespace at the start of the line.
inline bool shouldSkipWhitespaceAfterStartObject(RenderBlockFlow& block, RenderObject* o, LineWhitespaceCollapsingState& lineWhitespaceCollapsingState)
{
    RenderObject* next = nextInlineRendererSkippingEmpty(block, o);
    auto* nextText = dynamicDowncast<RenderText>(next);
    if (nextText && nextText->text().length() > 0) {
        UChar nextChar = nextText->characterAt(0);
        if (nextText->style().isCollapsibleWhiteSpace(nextChar)) {
            lineWhitespaceCollapsingState.startIgnoringSpaces(LegacyInlineIterator(nullptr, o, 0));
            return true;
        }
    }

    return false;
}

inline void BreakingContext::handleEmptyInline()
{
    RenderInline& flowBox = downcast<RenderInline>(*m_current.renderer());

    // This should only end up being called on empty inlines
    ASSERT(isEmptyInline(flowBox));

    // Now that some inline flows have line boxes, if we are already ignoring spaces, we need
    // to make sure that we stop to include this object and then start ignoring spaces again.
    // If this object is at the start of the line, we need to behave like list markers and
    // start ignoring spaces.
    if (requiresLineBoxForContent(flowBox, m_lineInfo)) {
        // An empty inline that only has line-height, vertical-align or font-metrics will only get a
        // line box to affect the height of the line if the rest of the line is not empty.
        if (m_ignoringSpaces) {
            m_trailingObjects.clear();
            m_lineWhitespaceCollapsingState.ensureLineBoxInsideIgnoredSpaces(*m_current.renderer());
        } else if (m_blockStyle.collapseWhiteSpace() && m_resolver.position().renderer() == m_current.renderer()
            && shouldSkipWhitespaceAfterStartObject(m_block, m_current.renderer(), m_lineWhitespaceCollapsingState)) {
            // Like with list markers, we start ignoring spaces to make sure that any
            // additional spaces we see will be discarded.
            m_currentCharacterIsSpace = true;
            m_currentCharacterIsWS = true;
            m_ignoringSpaces = true;
        } else
            m_trailingObjects.appendBoxIfNeeded(flowBox);
    }
    
    float inlineWidth = inlineLogicalWidth(*m_current.renderer()) + borderPaddingMarginStart(flowBox) + borderPaddingMarginEnd(flowBox);
    m_width.addUncommittedWidth(inlineWidth);
    if (m_hangsAtEnd && inlineWidth)
        m_hangsAtEnd = false;
}

inline void nextCharacter(UChar& currentCharacter, UChar& lastCharacter, UChar& secondToLastCharacter)
{
    secondToLastCharacter = lastCharacter;
    lastCharacter = currentCharacter;
}

// Adding a pair of whitespace collapsing transitions before a character will split it out into a new line box.
inline void ensureCharacterGetsLineBox(LineWhitespaceCollapsingState& lineWhitespaceCollapsingState, LegacyInlineIterator& textParagraphSeparator)
{
    LegacyInlineIterator transition(0, textParagraphSeparator.renderer(), textParagraphSeparator.offset());
    lineWhitespaceCollapsingState.startIgnoringSpaces(LegacyInlineIterator(0, textParagraphSeparator.renderer(), textParagraphSeparator.offset() - 1));
    lineWhitespaceCollapsingState.stopIgnoringSpaces(LegacyInlineIterator(0, textParagraphSeparator.renderer(), textParagraphSeparator.offset()));
}

inline bool BreakingContext::handleText()
{
    auto& renderer = downcast<RenderText>(*m_current.renderer());
    auto* svgInlineTextRenderer = dynamicDowncast<RenderSVGInlineText>(renderer);

    // If we have left a no-wrap inline and entered an autowrap inline while ignoring spaces
    // then we need to mark the start of the autowrap inline as a potential linebreak now.
    if (m_autoWrap && m_lastObjectTextWrap == TextWrapMode::NoWrap && m_ignoringSpaces)
        commitLineBreakAtCurrentWidth(renderer);

    const RenderStyle& style = lineStyle(renderer, m_lineInfo);
    const FontCascade& font = style.fontCascade();
    bool canHangPunctuationAtStart = style.hangingPunctuation().contains(HangingPunctuation::First);
    bool canHangPunctuationAtEnd = style.hangingPunctuation().contains(HangingPunctuation::Last);
    bool canHangStopOrCommaAtLineEnd = style.hangingPunctuation().contains(HangingPunctuation::AllowEnd);
    int endPunctuationIndex = canHangPunctuationAtEnd && m_collapseWhiteSpace ? renderer.lastCharacterIndexStrippingSpaces() : renderer.text().length() - 1;

    float wrapWidthOffset = m_width.uncommittedWidth() + inlineLogicalWidth(renderer, !m_appliedStartWidth, true);
    float wrapW = wrapWidthOffset;
    float charWidth = 0;
    bool breakNBSP = m_autoWrap && style.nbspMode() == NBSPMode::Space;
    // Auto-wrapping text should wrap in the middle of a word only if it could not wrap before the word,
    // which is only possible if the word is the first thing on the line.
    auto isWrappingAllowed = m_currentTextWrap != TextWrapMode::NoWrap;
    bool breakWords = isWrappingAllowed && style.breakWords() && !m_width.committedWidth() && !m_width.hasCommittedReplaced();
    bool midWordBreak = false;
    bool breakAnywhere = style.lineBreak() == LineBreak::Anywhere && m_autoWrap;
    bool breakAll = (style.wordBreak() == WordBreak::BreakAll || breakAnywhere) && m_autoWrap;
    bool keepAllWords = style.wordBreak() == WordBreak::KeepAll;
    auto iteratorMode = mapLineBreakToIteratorMode(m_blockStyle.lineBreak());
    auto contentAnalysis = mapWordBreakToContentAnalysis(m_blockStyle.wordBreak());
    bool canUseLineBreakShortcut = iteratorMode == TextBreakIterator::LineMode::Behavior::Default
        && contentAnalysis == TextBreakIterator::ContentAnalysis::Mechanical;

    if (svgInlineTextRenderer) {
        breakWords = false;
        breakAll = false;
    }

    if (m_renderTextInfo.text != &renderer) {
        m_renderTextInfo.text = &renderer;
        m_renderTextInfo.font = &font;
        m_renderTextInfo.layout = font.createLayout(renderer, m_width.currentWidth(), m_collapseWhiteSpace);
        m_renderTextInfo.lineBreakIteratorFactory.resetStringAndReleaseIterator(renderer.text(), style.computedLocale(), iteratorMode, contentAnalysis);
    } else if (m_renderTextInfo.layout && m_renderTextInfo.font != &font) {
        m_renderTextInfo.font = &font;
        m_renderTextInfo.layout = font.createLayout(renderer, m_width.currentWidth(), m_collapseWhiteSpace);
    }

    SingleThreadWeakHashSet<const Font> fallbackFonts;
    m_hasFormerOpportunity = false;
    bool canBreakMidWord = breakWords || breakAll;
    UChar lastCharacter = m_renderTextInfo.lineBreakIteratorFactory.priorContext().lastCharacter();
    UChar secondToLastCharacter = m_renderTextInfo.lineBreakIteratorFactory.priorContext().secondToLastCharacter();
    // Non-zero only when kerning is enabled and TextLayout isn't used, in which case we measure
    // words with their trailing space, then subtract its width.
    TextLayout* textLayout = m_renderTextInfo.layout.get();
    WordTrailingSpace wordTrailingSpace(style, !textLayout);
    for (; m_current.offset() < renderer.text().length(); m_current.incrementByCodePointInTextNode()) {
        ASSERT(&renderer == m_current.renderer());
        bool previousCharacterIsSpace = m_currentCharacterIsSpace;
        bool previousCharacterIsWS = m_currentCharacterIsWS;
        UChar c = m_current.current(); // FIXME: It's silly to pull out a single surrogate from the content and attempt to do anything useful with it.
        m_currentCharacterIsSpace = c == ' ' || c == '\t' || (!m_preservesNewline && (c == '\n'));

        // A single preserved leading white-space doesn't fulfill the 'betweenWords' condition, however it's indeed a
        // soft-breaking opportunty so we may want to avoid breaking in the middle of the word.
        if (m_atStart && m_currentCharacterIsSpace && !previousCharacterIsSpace) {
            m_hasFormerOpportunity = !breakAnywhere;
            breakWords = false;
            canBreakMidWord = breakAll;
        }

        if (canHangPunctuationAtStart && m_width.isFirstLine() && !m_width.committedWidth() && !wrapW && !inlineLogicalWidth(renderer, true, false)) {
            m_width.addUncommittedWidth(-renderer.hangablePunctuationStartWidth(m_current.offset()));
            canHangPunctuationAtStart = false;
        }
        
        if (canHangPunctuationAtEnd && !m_nextObject && (int)m_current.offset() == endPunctuationIndex && !inlineLogicalWidth(renderer, false, true)) {
            m_width.addUncommittedWidth(-renderer.hangablePunctuationEndWidth(endPunctuationIndex));
            canHangPunctuationAtEnd = false;
        }

        if (!m_collapseWhiteSpace || !m_currentCharacterIsSpace)
            m_lineInfo.setEmpty(false);

        m_currentCharacterIsWS = m_currentCharacterIsSpace || (breakNBSP && c == noBreakSpace);

        if (canBreakMidWord && !midWordBreak && (!m_currentCharacterIsSpace || m_atStart || !(m_currentWhitespaceCollapse == WhiteSpaceCollapse::Preserve && m_currentTextWrap == TextWrapMode::Wrap))) {
            // FIXME: This code is ultra wrong.
            // The spec says "word-break: break-all: Any typographic letter units are treated as ID(“ideographic characters”) for the purpose of line-breaking."
            // The spec describes how a "typographic letter unit" is a cluster, not a code point: https://drafts.csswg.org/css-text-3/#typographic-character-unit
            wrapW += charWidth;
            charWidth = 0;
            midWordBreak = m_width.committedWidth() + wrapW + charWidth > m_width.availableWidth();
        }

        std::optional<unsigned> nextBreakablePosition = m_current.nextBreakablePosition();
        auto mayBreakHere = !(m_currentWhitespaceCollapse == WhiteSpaceCollapse::Preserve && m_currentTextWrap == TextWrapMode::NoWrap);
        bool betweenWords = c == newlineCharacter || (mayBreakHere && !m_atStart && BreakLines::isBreakable(m_renderTextInfo.lineBreakIteratorFactory, m_current.offset(), nextBreakablePosition, breakNBSP, canUseLineBreakShortcut, keepAllWords, breakAnywhere));
        m_current.setNextBreakablePosition(nextBreakablePosition);
        
        if (canHangStopOrCommaAtLineEnd && renderer.isHangableStopOrComma(c) && m_width.fitsOnLine()) {
            // We need to see if a measurement that excludes the stop would fit. If so, then we should hang
            // the stop/comma at the end. First measure including the comma.
            m_hangsAtEnd = false;
            float widthIncludingComma = 0;
            m_width.addUncommittedWidth(widthIncludingComma);
            if (!m_width.fitsOnLine()) {
                // See if we fit without the comma involved. If we do, then this is a potential hang point.
                float widthWithoutStopOrComma = 0;
                m_width.addUncommittedWidth(widthWithoutStopOrComma - widthIncludingComma);
                if (m_width.fitsOnLine())
                    m_hangsAtEnd = true;
            } else
                m_width.addUncommittedWidth(-widthIncludingComma);
        }
        
        if (betweenWords || midWordBreak) {
            bool stoppedIgnoringSpaces = false;
            if (m_ignoringSpaces) {
                if (!m_currentCharacterIsSpace) {
                    // Stop ignoring spaces and begin at this new point.
                    m_ignoringSpaces = false;
                    m_lineWhitespaceCollapsingState.stopIgnoringSpaces(LegacyInlineIterator(0, &renderer, m_current.offset()));
                    stoppedIgnoringSpaces = true;
                } else {
                    // Just keep ignoring these spaces.
                    nextCharacter(c, lastCharacter, secondToLastCharacter);
                    continue;
                }
            }
            
            float additionalTempWidth = 0;

            if (m_collapseWhiteSpace && previousCharacterIsSpace && m_currentCharacterIsSpace && additionalTempWidth)
                m_width.setTrailingWhitespaceWidth(additionalTempWidth);

            if (!m_appliedStartWidth) {
                float inlineStartWidth = inlineLogicalWidth(renderer, true, false);
                m_width.addUncommittedWidth(inlineStartWidth);
                m_appliedStartWidth = true;
                if (m_hangsAtEnd && inlineStartWidth)
                    m_hangsAtEnd = false;
            }

            if (!m_width.hasCommitted() && m_autoWrap && !fitsOnLineOrHangsAtEnd())
                m_width.fitBelowFloats(m_lineInfo.isFirstLine());

            if (m_autoWrap || breakWords) {
                // If we break only after white-space, consider the current character
                // as candidate width for this line.
                bool lineWasTooWide = false;
                auto mayBreakSpaces = m_currentWhitespaceCollapse == WhiteSpaceCollapse::BreakSpaces && m_currentTextWrap == TextWrapMode::Wrap;
                if (fitsOnLineOrHangsAtEnd() && m_currentCharacterIsWS && style.breakOnlyAfterWhiteSpace() && (!midWordBreak || mayBreakSpaces)) {
                    float charWidth = 0;
                    // Check if line is too big even without the extra space
                    // at the end of the line. If it is not, do nothing.
                    // If the line needs the extra whitespace to be too long,
                    // then move the line break to the space and skip all
                    // additional whitespace.
                    if (!m_width.fitsOnLineIncludingExtraWidth(charWidth)) {
                        lineWasTooWide = true;
                        if (mayBreakSpaces)
                            trailingSpacesHang(m_lineBreak, renderer, canBreakMidWord, previousCharacterIsSpace);
                        else {
                            m_lineBreak.moveTo(renderer, m_current.offset(), m_current.nextBreakablePosition());
                            m_lineBreaker.skipTrailingWhitespace(m_lineBreak, m_lineInfo);
                        }
                    }
                }
                if ((lineWasTooWide || !m_width.fitsOnLine()) && !m_hangsAtEnd) {
                    if (m_lineBreak.atTextParagraphSeparator()) {
                        if (!stoppedIgnoringSpaces && m_current.offset() > 0)
                            ensureCharacterGetsLineBox(m_lineWhitespaceCollapsingState, m_current);
                        m_lineBreak.increment();
                    }
                    // Didn't fit. Jump to the end unless there's still an opportunity to collapse whitespace.
                    if (m_ignoringSpaces || !m_collapseWhiteSpace || !m_currentCharacterIsSpace || !previousCharacterIsSpace) {
                        m_atEnd = true;
                        return false;
                    }
                } else {
                    if (!betweenWords || (midWordBreak && !m_autoWrap))
                        m_width.addUncommittedWidth(-additionalTempWidth);
                }
            }

            if (c == newlineCharacter && m_preservesNewline) {
                if (!stoppedIgnoringSpaces && m_current.offset())
                    ensureCharacterGetsLineBox(m_lineWhitespaceCollapsingState, m_current);
                commitLineBreakAtCurrentWidth(renderer, m_current.offset(), m_current.nextBreakablePosition());
                m_lineBreak.increment();
                return true;
            }

            if (m_autoWrap && betweenWords) {
                commitLineBreakAtCurrentWidth(renderer, m_current.offset(), m_current.nextBreakablePosition());
                wrapWidthOffset = 0;
                wrapW = wrapWidthOffset;
                // Auto-wrapping text should not wrap in the middle of a word once it has had an
                // opportunity to break after a word.
                m_hasFormerOpportunity = !breakAnywhere;
                breakWords = false;
                canBreakMidWord = breakAll;
            }

            if (midWordBreak && !U16_IS_TRAIL(c) && !(U_GET_GC_MASK(c) & U_GC_M_MASK)) {
                // Remember this as a breakable position in case
                // adding the end width forces a break.
                m_lineBreak.moveTo(renderer, m_current.offset(), m_current.nextBreakablePosition());
                midWordBreak &= canBreakMidWord;
            }

            if (!m_ignoringSpaces && style.collapseWhiteSpace()) {
                // If we encounter a newline, or if we encounter a second space,
                // we need to break up this run and enter a mode where we start collapsing spaces.
                if (m_currentCharacterIsSpace && previousCharacterIsSpace) {
                    m_ignoringSpaces = true;

                    // We just entered a mode where we are ignoring
                    // spaces. Create a transition to terminate the run
                    // before the second space.
                    m_lineWhitespaceCollapsingState.startIgnoringSpaces(m_startOfIgnoredSpaces);
                    m_trailingObjects.updateWhitespaceCollapsingTransitionsForTrailingBoxes(m_lineWhitespaceCollapsingState, LegacyInlineIterator(), TrailingObjects::CollapseFirstSpace::No);
                }
            }
            // Measuring the width of complex text character-by-character, rather than measuring it all together,
            // could produce considerably different width values.
            if (!renderer.canUseSimpleFontCodePath() && midWordBreak && m_width.fitsOnLine()) {
                midWordBreak = false;
                wrapW = wrapWidthOffset + additionalTempWidth;
            }
        } else {
            if (m_ignoringSpaces) {
                // Stop ignoring spaces and begin at this new point.
                m_ignoringSpaces = false;
                m_lineWhitespaceCollapsingState.stopIgnoringSpaces(LegacyInlineIterator(nullptr, &renderer, m_current.offset()));
            }
            if (m_hangsAtEnd && !renderer.isHangableStopOrComma(c))
                m_hangsAtEnd = false;
        }

        if (svgInlineTextRenderer && m_current.offset()) {
            // Force creation of new InlineBoxes for each absolute positioned character (those that start new text chunks).
            if (svgInlineTextRenderer->characterStartsNewTextChunk(m_current.offset()))
                ensureCharacterGetsLineBox(m_lineWhitespaceCollapsingState, m_current);
        }

        if (m_currentCharacterIsSpace && !previousCharacterIsSpace) {
            m_startOfIgnoredSpaces.setRenderer(&renderer);
            m_startOfIgnoredSpaces.setOffset(m_current.offset());
            // Spaces after right-aligned text and before a line-break get collapsed away completely so that the trailing
            // space doesn't seem to push the text out from the right-hand edge.
            // FIXME: Do this regardless of the container's alignment - will require rebaselining a lot of test results.
            if (m_nextObject && m_startOfIgnoredSpaces.offset() && m_nextObject->isBR() && (m_blockStyle.textAlign() == TextAlignMode::Right || m_blockStyle.textAlign() == TextAlignMode::WebKitRight)) {
                m_startOfIgnoredSpaces.setOffset(m_startOfIgnoredSpaces.offset() - 1);
                // If there's just a single trailing space start ignoring it now so it collapses away.
                if (m_current.offset() == renderer.text().length() - 1)
                    m_lineWhitespaceCollapsingState.startIgnoringSpaces(m_startOfIgnoredSpaces);
            }
        }

        if (!m_currentCharacterIsWS && previousCharacterIsWS) {
            if (m_autoWrap && style.breakOnlyAfterWhiteSpace())
                m_lineBreak.moveTo(renderer, m_current.offset(), m_current.nextBreakablePosition());
        }

        if (m_collapseWhiteSpace && m_currentCharacterIsSpace && !m_ignoringSpaces)
            m_trailingObjects.setTrailingWhitespace(renderer);
        else if (!style.collapseWhiteSpace() || !m_currentCharacterIsSpace)
            m_trailingObjects.clear();

        m_atStart = false;
        nextCharacter(c, lastCharacter, secondToLastCharacter);
    }

    m_renderTextInfo.lineBreakIteratorFactory.priorContext().set({ secondToLastCharacter, lastCharacter });

    // IMPORTANT: current.m_pos is > length here!
    float additionalTempWidth = 0;

    float inlineLogicalTempWidth = inlineLogicalWidth(renderer, !m_appliedStartWidth, m_includeEndWidth);
    m_width.addUncommittedWidth(additionalTempWidth + inlineLogicalTempWidth);
    if (m_hangsAtEnd && inlineLogicalTempWidth)
        m_hangsAtEnd = false;

    fallbackFonts.clear();

    if (m_collapseWhiteSpace && m_currentCharacterIsSpace && additionalTempWidth)
        m_width.setTrailingWhitespaceWidth(additionalTempWidth, inlineLogicalTempWidth);

    m_includeEndWidth = false;
    m_appliedStartWidth = false;
    return false;
}

inline bool textBeginsWithBreakablePosition(RenderText& nextText)
{
    UChar c = nextText.characterAt(0);
    return c == ' ' || c == '\t' || (c == '\n' && !nextText.preservesNewline());
}

inline void BreakingContext::trailingSpacesHang(LegacyInlineIterator& lineBreak, RenderObject& renderObject, bool canBreakMidWord, bool previousCharacterIsSpace)
{
    ASSERT(m_currentWhitespaceCollapse == WhiteSpaceCollapse::BreakSpaces && m_currentTextWrap == TextWrapMode::Wrap);
    // Avoid breaking before the first white-space after a word if there is a
    // breaking opportunity before.
    if (m_hasFormerOpportunity && !previousCharacterIsSpace)
        return;

    lineBreak.moveTo(renderObject, m_current.offset(), m_current.nextBreakablePosition());

    // Avoid breaking before the first white-space after a word, unless
    // overflow-wrap or word-break allow to.
    if (!previousCharacterIsSpace && !canBreakMidWord)
        lineBreak.increment();
}

inline bool BreakingContext::canBreakAtThisPosition()
{
    // If we are no-wrap and have found a line-breaking opportunity already then we should take it.
    if (m_width.committedWidth() && !m_width.fitsOnLine(m_currentCharacterIsSpace) && m_currentWhitespaceCollapse == WhiteSpaceCollapse::Collapse && m_currentTextWrap == TextWrapMode::NoWrap)
        return true;

    // Avoid breaking on empty inlines.
    auto* renderInline = dynamicDowncast<RenderInline>(*m_current.renderer());
    if (renderInline && isEmptyInline(*renderInline))
        return false;

    // Avoid breaking before empty inlines (as long as the current object isn't replaced).
    if (!m_current.renderer()->isReplacedOrInlineBlock()) {
        auto* renderInline = dynamicDowncast<RenderInline>(m_nextObject);
        if (renderInline && isEmptyInline(*renderInline))
            return false;
    }

    // Return early if we autowrap and the current character is a space as we will always want to break at such a position.
    if (m_autoWrap && m_currentCharacterIsSpace)
        return true;

    if (m_nextObject && m_nextObject->isLineBreakOpportunity())
        return m_autoWrap;

    auto* renderText = dynamicDowncast<RenderText>(m_nextObject);
    bool nextIsAutoWrappingText = renderText && (m_autoWrap || m_nextObject->style().autoWrap());
    if (!nextIsAutoWrappingText)
        return m_autoWrap;
    RenderText& nextRenderText = *renderText;
    bool currentIsTextOrEmptyInline = [this] {
        if (is<RenderText>(*m_current.renderer()))
            return true;
        auto* renderInline = dynamicDowncast<RenderInline>(*m_current.renderer());
        return renderInline && isEmptyInline(*renderInline);
    }();
    if (!currentIsTextOrEmptyInline)
        return m_autoWrap;

    bool canBreakHere = !m_currentCharacterIsSpace && textBeginsWithBreakablePosition(nextRenderText);

    // See if attempting to fit below floats creates more available width on the line.
    if (!m_width.fitsOnLine() && !m_width.hasCommitted())
        m_width.fitBelowFloats(m_lineInfo.isFirstLine());

    bool canPlaceOnLine = m_width.fitsOnLine() || !m_autoWrapWasEverTrueOnLine;

    if (canPlaceOnLine && canBreakHere)
        commitLineBreakAtCurrentWidth(nextRenderText);

    return canBreakHere;
}

inline void BreakingContext::commitAndUpdateLineBreakIfNeeded()
{
    bool checkForBreak = canBreakAtThisPosition();

    if (checkForBreak && !m_width.fitsOnLine(m_ignoringSpaces) && !m_hangsAtEnd) {
        // if we have floats, try to get below them.
        if (m_currentCharacterIsSpace && !m_ignoringSpaces && m_collapseWhiteSpace)
            m_trailingObjects.clear();

        if (m_width.committedWidth()) {
            m_atEnd = true;
            return;
        }

        if (!m_hangsAtEnd)
            m_width.fitBelowFloats(m_lineInfo.isFirstLine());

        // |width| may have been adjusted because we got shoved down past a float (thus
        // giving us more room), so we need to retest, and only jump to
        // the end label if we still don't fit on the line. -dwh
        if (!m_width.fitsOnLine(m_ignoringSpaces)) {
            m_atEnd = true;
            return;
        }
    } else if (m_blockStyle.autoWrap() && !m_width.fitsOnLine() && !m_width.hasCommitted() && !m_hangsAtEnd) {
        // If the container autowraps but the current child does not then we still need to ensure that it
        // wraps and moves below any floats.
        m_width.fitBelowFloats(m_lineInfo.isFirstLine());
    }

    if (!m_current.renderer()->isFloatingOrOutOfFlowPositioned()) {
        m_lastObject = m_current.renderer();
        if (m_lastObject->isReplacedOrInlineBlock() && m_autoWrap && (!m_lastObject->isImage() || m_allowImagesToBreak)) {
            auto* renderListMarker = dynamicDowncast<RenderListMarker>(*m_lastObject);
            if (!renderListMarker || renderListMarker->isInside()) {
                if (m_nextObject)
                    commitLineBreakAtCurrentWidth(*m_nextObject);
                else
                    commitLineBreakClear();
            }
        }
    }
}

inline TrailingObjects::CollapseFirstSpace checkWhitespaceCollapsingTransitions(LineWhitespaceCollapsingState& lineWhitespaceCollapsingState, const LegacyInlineIterator& lBreak)
{
    // Check to see if our last transition is a start point beyond the line break. If so,
    // shave it off the list, and shave off a trailing space if the previous end point doesn't
    // preserve whitespace.
    if (lBreak.renderer() && lineWhitespaceCollapsingState.numTransitions() && !(lineWhitespaceCollapsingState.numTransitions() % 2)) {
        const LegacyInlineIterator* transitions = lineWhitespaceCollapsingState.transitions().data();
        const LegacyInlineIterator& endpoint = transitions[lineWhitespaceCollapsingState.numTransitions() - 2];
        const LegacyInlineIterator& startpoint = transitions[lineWhitespaceCollapsingState.numTransitions() - 1];
        LegacyInlineIterator currpoint = endpoint;
        while (!currpoint.atEnd() && currpoint != startpoint && currpoint != lBreak)
            currpoint.increment();
        if (currpoint == lBreak) {
            // We hit the line break before the start point. Shave off the start point.
            lineWhitespaceCollapsingState.decrementNumTransitions();
            if (endpoint.renderer()->style().collapseWhiteSpace() && endpoint.renderer()->isRenderText()) {
                lineWhitespaceCollapsingState.decrementTransitionAt(lineWhitespaceCollapsingState.numTransitions() - 1);
                return TrailingObjects::CollapseFirstSpace::No;
            }
        }
    }
    return TrailingObjects::CollapseFirstSpace::Yes;
}

inline LegacyInlineIterator BreakingContext::handleEndOfLine()
{
    if (m_lineBreak == m_resolver.position()) {
        if (!m_lineBreak.renderer() || !m_lineBreak.renderer()->isBR()) {
            // we just add as much as possible
            if (m_blockStyle.whiteSpaceCollapse() == WhiteSpaceCollapse::Preserve && m_blockStyle.textWrapMode() == TextWrapMode::NoWrap && !m_current.offset()) {
                if (m_lastObject)
                    commitLineBreakAtCurrentWidth(*m_lastObject, m_lastObject->isRenderText() ? m_lastObject->length() : 0);
                else
                    commitLineBreakClear();
            } else if (m_lineBreak.renderer()) {
                // Don't ever break in the middle of a word if we can help it.
                // There's no room at all. We just have to be on this line,
                // even though we'll spill out.
                if (m_current.renderer())
                    commitLineBreakAtCurrentWidth(*m_current.renderer(), m_current.offset());
                else if (m_lastObject)
                    commitLineBreakAtCurrentWidth(*m_lastObject, m_lastObject->isRenderText() ? m_lastObject->length() : 0);
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
        auto overflowingBoxIsInlineLevelBox = m_lineBreak.renderer() && !m_lineBreak.renderer()->isFloatingOrOutOfFlowPositioned();
        if (overflowingBoxIsInlineLevelBox)
            m_lineBreak.increment();
    }

    // Sanity check our whitespace collapsing transitions.
    auto collapsed = checkWhitespaceCollapsingTransitions(m_lineWhitespaceCollapsingState, m_lineBreak);
    m_trailingObjects.updateWhitespaceCollapsingTransitionsForTrailingBoxes(m_lineWhitespaceCollapsingState, m_lineBreak, collapsed);

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

} // namespace WebCore
