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

#pragma once

#include "BreakLines.h"
#include "Hyphenation.h"
#include "LineBreaker.h"
#include "LineInfo.h"
#include "LineLayoutState.h"
#include "LineWidth.h"
#include "RenderCombineText.h"
#include "RenderCounter.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLineBreak.h"
#include "RenderListMarker.h"
#include "RenderRubyRun.h"
#include "RenderSVGInlineText.h"
#include "TrailingObjects.h"
#include <wtf/Function.h>
#include <wtf/Optional.h>
#include <wtf/text/StringView.h>
#include <wtf/unicode/CharacterNames.h>

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
    unsigned startOffset;
    unsigned endOffset;
    HashSet<const Font*> fallbackFonts;
};

struct WordTrailingSpace {
    WordTrailingSpace(const RenderStyle& style, bool measuringWithTrailingWhitespaceEnabled = true)
        : m_style(style)
    {
        if (!measuringWithTrailingWhitespaceEnabled || !m_style.fontCascade().enableKerning())
            m_state = WordTrailingSpaceState::Initialized;
    }

    std::optional<float> width(HashSet<const Font*>& fallbackFonts)
    {
        if (m_state == WordTrailingSpaceState::Initialized)
            return m_width;

        auto& font = m_style.fontCascade();
        m_width = font.width(RenderBlock::constructTextRun(&space, 1, m_style), &fallbackFonts) + font.wordSpacing();
        m_state = WordTrailingSpaceState::Initialized;
        return m_width;
    }

private:
    enum class WordTrailingSpaceState { Uninitialized, Initialized };
    const RenderStyle& m_style;
    WordTrailingSpaceState m_state { WordTrailingSpaceState::Uninitialized };
    std::optional<float> m_width;
};

class BreakingContext {
public:
    BreakingContext(LineBreaker& lineBreaker, InlineBidiResolver& resolver, LineInfo& inLineInfo, LineWidth& lineWidth, RenderTextInfo& inRenderTextInfo, FloatingObject* inLastFloatFromPreviousLine, bool appliedStartWidth, RenderBlockFlow& block)
        : m_lineBreaker(lineBreaker)
        , m_resolver(resolver)
        , m_current(resolver.position())
#if ENABLE(CSS_TRAILING_WORD)
        , m_lineBreakHistory(InlineIterator(resolver.position()), block.style().trailingWord() == TrailingWord::PartiallyBalanced ? 5 : 1)
#else
        , m_lineBreakHistory(InlineIterator(resolver.position()), 1)
#endif
        , m_block(block)
        , m_lastObject(m_current.renderer())
        , m_nextObject(nullptr)
        , m_currentStyle(nullptr)
        , m_blockStyle(block.style())
        , m_lineInfo(inLineInfo)
        , m_renderTextInfo(inRenderTextInfo)
        , m_lastFloatFromPreviousLine(inLastFloatFromPreviousLine)
        , m_width(lineWidth)
        , m_currWS(WhiteSpace::Normal)
        , m_lastWS(WhiteSpace::Normal)
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
        , m_lineWhitespaceCollapsingState(resolver.whitespaceCollapsingState())
    {
        m_lineInfo.setPreviousLineBrokeCleanly(false);
    }

    RenderObject* currentObject() { return m_current.renderer(); }
    InlineIterator lineBreak() { return m_lineBreakHistory.current(); }
    LineWidth& lineWidth() { return m_width; }
    bool atEnd() { return m_atEnd; }
    
    bool fitsOnLineOrHangsAtEnd() const { return m_width.fitsOnLine() || m_hangsAtEnd; }

    void initializeForCurrentObject();

    void increment();

    void handleBR(Clear&);
    void handleOutOfFlowPositioned(Vector<RenderBox*>& positionedObjects);
    void handleFloat();
    void handleEmptyInline();
    void handleReplaced();
    bool handleText(WordMeasurements&, bool& hyphenated, unsigned& consecutiveHyphenatedLines);
    bool canBreakAtThisPosition();
    void commitAndUpdateLineBreakIfNeeded();
    InlineIterator handleEndOfLine();
#if ENABLE(CSS_TRAILING_WORD)
    InlineIterator optimalLineBreakLocationForTrailingWord();
#endif
    
    float computeAdditionalBetweenWordsWidth(RenderText&, TextLayout*, UChar, WordTrailingSpace&, HashSet<const Font*>& fallbackFonts, WordMeasurements&, const FontCascade&, bool isFixedPitch, unsigned lastSpace, float lastSpaceWordSpacing, float wordSpacingForWordMeasurement, unsigned offset);

    void clearLineBreakIfFitsOnLine(bool ignoringTrailingSpace = false)
    {
        if (m_width.fitsOnLine(ignoringTrailingSpace) || m_lastWS == WhiteSpace::NoWrap || m_hangsAtEnd)
            m_lineBreakHistory.clear();
        m_hangsAtEnd = false;
    }

    void commitLineBreakClear()
    {
        m_width.commit();
        m_lineBreakHistory.clear();
        m_hangsAtEnd = false;
    }

    void commitLineBreakAtCurrentWidth(RenderObject& object, unsigned offset = 0, std::optional<unsigned> nextBreak = std::optional<unsigned>())
    {
        m_width.commit();
        m_lineBreakHistory.moveTo(object, offset, nextBreak);
        m_hangsAtEnd = false;
    }

private:
    // This class keeps a sliding window of the past n locations for an InlineIterator.
    class InlineIteratorHistory : private Vector<InlineIterator, 1> {
    public:
        InlineIteratorHistory() = delete;
        InlineIteratorHistory(const InlineIterator& initial, size_t capacity)
            : m_capacity(capacity)
        {
            ASSERT(capacity > 0);
            this->append(initial);
        }

        void push(const WTF::Function<void(InlineIterator& modifyMe)>& updater)
        {
            ASSERT(!this->isEmpty());
            if (m_capacity != 1)
                this->insert(0, InlineIterator(this->at(0)));
            updater(this->at(0));
            if (m_capacity != 1)
                this->resize(m_capacity);
        }

        void update(const WTF::Function<void(InlineIterator& modifyMe)>& updater)
        {
            ASSERT(!this->isEmpty());
            updater(this->at(0));
        }

        RenderObject* renderer() const { return this->at(0).renderer(); }
        unsigned offset() const { return this->at(0).offset(); }
        std::optional<unsigned> nextBreakablePosition() const { return this->at(0).nextBreakablePosition(); }
        bool atTextParagraphSeparator() const { return this->at(0).atTextParagraphSeparator(); }
        UChar previousInSameNode() const { return this->at(0).previousInSameNode(); }
        const InlineIterator& get(size_t i) const { return this->at(i); };
        const InlineIterator& current() const { return get(0); }
        size_t historyLength() const { return this->size(); }

        void moveTo(RenderObject& object, unsigned offset, std::optional<unsigned> nextBreak = std::nullopt)
        {
            push([&](InlineIterator& modifyMe) {
                modifyMe.moveTo(object, offset, nextBreak);
            });
        }

        void increment()
        {
            update([](InlineIterator& modifyMe) {
                modifyMe.increment();
            });
        }

        void clear()
        {
            push([](InlineIterator& modifyMe) {
                modifyMe.clear();
            });
        }

    private:
        const size_t m_capacity;
    };

    LineBreaker& m_lineBreaker;
    InlineBidiResolver& m_resolver;

    InlineIterator m_current;
    InlineIteratorHistory m_lineBreakHistory;
    InlineIterator m_startOfIgnoredSpaces;

    RenderBlockFlow& m_block;
    RenderObject* m_lastObject;
    RenderObject* m_nextObject;

    const RenderStyle* m_currentStyle;

    // Firefox and Opera will allow a table cell to grow to fit an image inside it under
    // very specific circumstances (in order to match common WinIE renderings).
    // Not supporting the quirk has caused us to mis-render some real sites. (See Bugzilla 10517.)
    const RenderStyle& m_blockStyle;

    LineInfo& m_lineInfo;

    RenderTextInfo& m_renderTextInfo;

    FloatingObject* m_lastFloatFromPreviousLine;

    LineWidth m_width;

    WhiteSpace m_currWS;
    WhiteSpace m_lastWS;

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
    
    bool m_hangsAtEnd { false };

    LineWhitespaceCollapsingState& m_lineWhitespaceCollapsingState;

    TrailingObjects m_trailingObjects;
};

inline void BreakingContext::initializeForCurrentObject()
{
    m_hadUncommittedWidthBeforeCurrent = !!m_width.uncommittedWidth();

    m_currentStyle = &m_current.renderer()->style(); // FIXME: Should this be &lineStyle(*m_current.renderer(), m_lineInfo); ?

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

    if (m_nextObject)
        m_current.moveToStartOf(*m_nextObject);
    else
        m_current.clear();
    m_atStart = false;
}

inline void BreakingContext::handleBR(Clear& clear)
{
    if (fitsOnLineOrHangsAtEnd()) {
        RenderObject& br = *m_current.renderer();
        m_lineBreakHistory.push([&](InlineIterator& modifyMe) {
            modifyMe.moveToStartOf(br);
            modifyMe.increment();
        });

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
        if (m_ignoringSpaces && m_currentStyle->clear() != Clear::None)
            m_lineWhitespaceCollapsingState.ensureLineBoxInsideIgnoredSpaces(br);
        // If we were preceded by collapsing space and are in a right-aligned container we need to ensure the space gets
        // collapsed away so that it doesn't push the text out from the container's right-hand edge.
        // FIXME: Do this regardless of the container's alignment - will require rebaselining a lot of test results.
        else if (m_ignoringSpaces && (m_blockStyle.textAlign() == TextAlignMode::Right || m_blockStyle.textAlign() == TextAlignMode::WebKitRight))
            m_lineWhitespaceCollapsingState.stopIgnoringSpaces(InlineIterator(0, m_current.renderer(), m_current.offset()));

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
    if (!child)
        return true;
    // When deciding whether we're at the edge of an inline, adjacent collapsed whitespace is the same as no sibling at all.
    if (is<RenderText>(*child) && !downcast<RenderText>(*child).text().length())
        return true;
#if ENABLE(CSS_BOX_DECORATION_BREAK)
    if (is<RenderLineBreak>(*child) && child->parent()->style().boxDecorationBreak() == BoxDecorationBreak::Clone)
        return true;
#endif
    return false;
}

inline RenderObject* previousInFlowSibling(RenderObject* child)
{
    do {
        child = child->previousSibling();
    } while (child && child->isOutOfFlowPositioned());
    return child;
}

inline LayoutUnit inlineLogicalWidth(RenderObject* child, bool checkStartEdge = true, bool checkEndEdge = true)
{
    unsigned lineDepth = 1;
    LayoutUnit extraWidth;
    RenderElement* parent = child->parent();
    while (is<RenderInline>(*parent) && lineDepth++ < cMaxLineDepth) {
        const auto& parentAsRenderInline = downcast<RenderInline>(*parent);
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
    // If our original display wasn't an inline type, then we can determine our static inline position now.
    auto& box = downcast<RenderBox>(*m_current.renderer());
    bool isInlineType = box.style().isOriginalDisplayInlineType();
    if (!isInlineType)
        m_block.setStaticInlinePositionForChild(box, m_block.logicalHeight(), m_block.startOffsetForContent(m_block.logicalHeight()));
    else {
        // If our original display was an DisplayType::Inline type, then we can determine our static y position now.
        box.layer()->setStaticBlockPosition(m_block.logicalHeight());
    }

    // If we're ignoring spaces, we have to stop and include this object and
    // then start ignoring spaces again.
    if (isInlineType || box.container()->isRenderInline()) {
        if (m_ignoringSpaces)
            m_lineWhitespaceCollapsingState.ensureLineBoxInsideIgnoredSpaces(box);
        m_trailingObjects.appendBoxIfNeeded(box);
    } else
        positionedObjects.append(&box);

    m_width.addUncommittedWidth(inlineLogicalWidth(&box));
    // Reset prior line break context characters.
    m_renderTextInfo.lineBreakIterator.resetPriorContext();
}

inline void BreakingContext::handleFloat()
{
    auto& floatBox = downcast<RenderBox>(*m_current.renderer());
    const auto& floatingObject = *m_lineBreaker.insertFloatingObject(floatBox);
    // check if it fits in the current line.
    // If it does, position it now, otherwise, position
    // it after moving to next line (in clearFloats() func)
    if (m_floatsFitOnLine && m_width.fitsOnLineExcludingTrailingWhitespace(m_block.logicalWidthForFloat(floatingObject))) {
        m_lineBreaker.positionNewFloatOnLine(floatingObject, m_lastFloatFromPreviousLine, m_lineInfo, m_width);
        if (m_lineBreakHistory.renderer() == m_current.renderer()) {
            ASSERT(!m_lineBreakHistory.offset());
            m_lineBreakHistory.increment();
        }
    } else
        m_floatsFitOnLine = false;
    // Update prior line break context characters, using U+FFFD (OBJECT REPLACEMENT CHARACTER) for floating element.
    m_renderTextInfo.lineBreakIterator.updatePriorContext(replacementCharacter);
}

// This is currently just used for list markers and inline flows that have line boxes. Neither should
// have an effect on whitespace at the start of the line.
inline bool shouldSkipWhitespaceAfterStartObject(RenderBlockFlow& block, RenderObject* o, LineWhitespaceCollapsingState& lineWhitespaceCollapsingState)
{
    RenderObject* next = bidiNextSkippingEmptyInlines(block, o);
    while (next && next->isFloatingOrOutOfFlowPositioned())
        next = bidiNextSkippingEmptyInlines(block, next);

    if (is<RenderText>(next) && downcast<RenderText>(*next).text().length() > 0) {
        RenderText& nextText = downcast<RenderText>(*next);
        UChar nextChar = nextText.characterAt(0);
        if (nextText.style().isCollapsibleWhiteSpace(nextChar)) {
            lineWhitespaceCollapsingState.startIgnoringSpaces(InlineIterator(nullptr, o, 0));
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
    bool requiresLineBox = alwaysRequiresLineBox(flowBox);
    if (requiresLineBox || requiresLineBoxForContent(flowBox, m_lineInfo)) {
        // An empty inline that only has line-height, vertical-align or font-metrics will only get a
        // line box to affect the height of the line if the rest of the line is not empty.
        if (requiresLineBox)
            m_lineInfo.setEmpty(false, &m_block, &m_width);
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
    
    float inlineWidth = inlineLogicalWidth(m_current.renderer()) + borderPaddingMarginStart(flowBox) + borderPaddingMarginEnd(flowBox);
    m_width.addUncommittedWidth(inlineWidth);
    if (m_hangsAtEnd && inlineWidth)
        m_hangsAtEnd = false;
}

inline void BreakingContext::handleReplaced()
{
    auto& replacedBox = downcast<RenderBox>(*m_current.renderer());

    if (m_atStart)
        m_width.updateAvailableWidth(replacedBox.logicalHeight());

    // Break on replaced elements if either has normal white-space.
    if ((m_autoWrap || RenderStyle::autoWrap(m_lastWS)) && (!replacedBox.isImage() || m_allowImagesToBreak)
        && (!is<RenderRubyRun>(replacedBox) || downcast<RenderRubyRun>(replacedBox).canBreakBefore(m_renderTextInfo.lineBreakIterator))) {
        if (auto* renderer = m_current.renderer())
            commitLineBreakAtCurrentWidth(*renderer);
        else
            commitLineBreakClear();
    } else
        m_hangsAtEnd = false;

    if (m_ignoringSpaces)
        m_lineWhitespaceCollapsingState.stopIgnoringSpaces(InlineIterator(0, &replacedBox, 0));

    m_lineInfo.setEmpty(false, &m_block, &m_width);
    m_ignoringSpaces = false;
    m_currentCharacterIsSpace = false;
    m_currentCharacterIsWS = false;
    m_trailingObjects.clear();

    // Optimize for a common case. If we can't find whitespace after the list
    // item, then this is all moot.
    LayoutUnit replacedLogicalWidth = m_block.logicalWidthForChild(replacedBox) + m_block.marginStartForChild(replacedBox) + m_block.marginEndForChild(replacedBox) + inlineLogicalWidth(&replacedBox);
    if (is<RenderListMarker>(replacedBox)) {
        if (m_blockStyle.collapseWhiteSpace() && shouldSkipWhitespaceAfterStartObject(m_block, &replacedBox, m_lineWhitespaceCollapsingState)) {
            // Like with inline flows, we start ignoring spaces to make sure that any
            // additional spaces we see will be discarded.
            m_currentCharacterIsSpace = true;
            m_currentCharacterIsWS = false;
            m_ignoringSpaces = true;
        }
        if (downcast<RenderListMarker>(replacedBox).isInside())
            m_width.addUncommittedReplacedWidth(replacedLogicalWidth);
    } else
        m_width.addUncommittedReplacedWidth(replacedLogicalWidth);
    if (is<RenderRubyRun>(replacedBox)) {
        m_width.applyOverhang(downcast<RenderRubyRun>(replacedBox), m_lastObject, m_nextObject);
        downcast<RenderRubyRun>(replacedBox).updatePriorContextFromCachedBreakIterator(m_renderTextInfo.lineBreakIterator);
    } else {
        // Update prior line break context characters, using U+FFFD (OBJECT REPLACEMENT CHARACTER) for replaced element.
        m_renderTextInfo.lineBreakIterator.updatePriorContext(replacementCharacter);
    }    
}

inline float firstPositiveWidth(const WordMeasurements& wordMeasurements)
{
    for (size_t i = 0; i < wordMeasurements.size(); ++i) {
        if (wordMeasurements[i].width > 0)
            return wordMeasurements[i].width;
    }
    return 0;
}

inline bool iteratorIsBeyondEndOfRenderCombineText(const InlineIterator& iter, RenderCombineText& renderer)
{
    return iter.renderer() == &renderer && iter.offset() >= renderer.text().length();
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
    if (!renderText.preferredLogicalWidthsDirty() || !is<RenderCounter>(renderText))
        return;
    downcast<RenderCounter>(renderText).updateCounter();
}

inline float measureHyphenWidth(RenderText& renderer, const FontCascade& font, HashSet<const Font*>* fallbackFonts = 0)
{
    const RenderStyle& style = renderer.style();
    return font.width(RenderBlock::constructTextRun(style.hyphenString().string(), style), fallbackFonts);
}

ALWAYS_INLINE float textWidth(RenderText& text, unsigned from, unsigned len, const FontCascade& font, float xPos, bool isFixedPitch, bool collapseWhiteSpace, HashSet<const Font*>& fallbackFonts, TextLayout* layout = nullptr)
{
    const RenderStyle& style = text.style();

    GlyphOverflow glyphOverflow;
    // FIXME: This is not the right level of abstraction for isFixedPitch. Font fallback may make it such that the fixed pitch font is never actually used!
    if (isFixedPitch || (!from && len == text.text().length()) || style.hasTextCombine())
        return text.width(from, len, font, xPos, &fallbackFonts, &glyphOverflow);

    if (layout)
        return FontCascade::width(*layout, from, len, &fallbackFonts);

    TextRun run = RenderBlock::constructTextRun(text, from, len, style);
    run.setCharacterScanForCodePath(!text.canUseSimpleFontCodePath());
    run.setTabSize(!collapseWhiteSpace, style.tabSize());
    run.setXPos(xPos);
    return font.width(run, &fallbackFonts, &glyphOverflow);
}

// Adding a pair of whitespace collapsing transitions before a character will split it out into a new line box.
inline void ensureCharacterGetsLineBox(LineWhitespaceCollapsingState& lineWhitespaceCollapsingState, InlineIterator& textParagraphSeparator)
{
    InlineIterator transition(0, textParagraphSeparator.renderer(), textParagraphSeparator.offset());
    lineWhitespaceCollapsingState.startIgnoringSpaces(InlineIterator(0, textParagraphSeparator.renderer(), textParagraphSeparator.offset() - 1));
    lineWhitespaceCollapsingState.stopIgnoringSpaces(InlineIterator(0, textParagraphSeparator.renderer(), textParagraphSeparator.offset()));
}

inline void tryHyphenating(RenderText& text, const FontCascade& font, const AtomicString& localeIdentifier, unsigned consecutiveHyphenatedLines, int consecutiveHyphenatedLinesLimit, int minimumPrefixLimit, int minimumSuffixLimit, unsigned lastSpace, unsigned pos, float xPos, float availableWidth, bool isFixedPitch, bool collapseWhiteSpace, int lastSpaceWordSpacing, InlineIterator& lineBreak, std::optional<unsigned> nextBreakable, bool& hyphenated)
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

    float hyphenWidth = measureHyphenWidth(text, font);

    float maxPrefixWidth = availableWidth - xPos - hyphenWidth - lastSpaceWordSpacing;
    if (!enoughWidthForHyphenation(maxPrefixWidth, font.pixelSize()))
        return;

    const RenderStyle& style = text.style();
    TextRun run = RenderBlock::constructTextRun(text, lastSpace, pos - lastSpace, style);
    run.setTabSize(!collapseWhiteSpace, style.tabSize());
    run.setXPos(xPos + lastSpaceWordSpacing);

    unsigned prefixLength = font.offsetForPosition(run, maxPrefixWidth, false);
    if (prefixLength < minimumPrefixLength)
        return;

    prefixLength = lastHyphenLocation(StringView(text.text()).substring(lastSpace, pos - lastSpace), std::min(prefixLength, pos - lastSpace - minimumSuffixLength) + 1, localeIdentifier);
    if (!prefixLength || prefixLength < minimumPrefixLength)
        return;

    // When lastSpace is a space, which it always is except sometimes at the beginning of a line or after collapsed
    // space, it should not count towards hyphenate-limit-before.
    if (prefixLength == minimumPrefixLength) {
        UChar characterAtLastSpace = text.characterAt(lastSpace);
        if (characterAtLastSpace == ' ' || characterAtLastSpace == '\n' || characterAtLastSpace == '\t' || characterAtLastSpace == noBreakSpace)
            return;
    }

    ASSERT(pos - lastSpace - prefixLength >= minimumSuffixLength);

#if !ASSERT_DISABLED
    HashSet<const Font*> fallbackFonts;
    float prefixWidth = hyphenWidth + textWidth(text, lastSpace, prefixLength, font, xPos, isFixedPitch, collapseWhiteSpace, fallbackFonts) + lastSpaceWordSpacing;
    ASSERT(xPos + prefixWidth <= availableWidth);
#else
    UNUSED_PARAM(isFixedPitch);
#endif

    lineBreak.moveTo(text, lastSpace + prefixLength, nextBreakable);
    hyphenated = true;
}

inline float BreakingContext::computeAdditionalBetweenWordsWidth(RenderText& renderText, TextLayout* textLayout, UChar currentCharacter, WordTrailingSpace& wordTrailingSpace, HashSet<const Font*>& fallbackFonts, WordMeasurements& wordMeasurements, const FontCascade& font, bool isFixedPitch, unsigned lastSpace, float lastSpaceWordSpacing, float wordSpacingForWordMeasurement, unsigned offset)
{
    wordMeasurements.grow(wordMeasurements.size() + 1);
    WordMeasurement& wordMeasurement = wordMeasurements.last();
    
    wordMeasurement.renderer = &renderText;
    wordMeasurement.endOffset = offset;
    wordMeasurement.startOffset = lastSpace;
    
    float additionalTempWidth = 0;
    std::optional<float> wordTrailingSpaceWidth;
    if (currentCharacter == ' ')
        wordTrailingSpaceWidth = wordTrailingSpace.width(fallbackFonts);
    if (wordTrailingSpaceWidth)
        additionalTempWidth = textWidth(renderText, lastSpace, offset + 1 - lastSpace, font, m_width.currentWidth(), isFixedPitch, m_collapseWhiteSpace, wordMeasurement.fallbackFonts, textLayout) - wordTrailingSpaceWidth.value();
    else
        additionalTempWidth = textWidth(renderText, lastSpace, offset - lastSpace, font, m_width.currentWidth(), isFixedPitch, m_collapseWhiteSpace, wordMeasurement.fallbackFonts, textLayout);
    
    if (wordMeasurement.fallbackFonts.isEmpty() && !fallbackFonts.isEmpty())
        wordMeasurement.fallbackFonts.swap(fallbackFonts);
    fallbackFonts.clear();
    
    wordMeasurement.width = additionalTempWidth + wordSpacingForWordMeasurement;
    additionalTempWidth += lastSpaceWordSpacing;
    return additionalTempWidth;
}

inline bool BreakingContext::handleText(WordMeasurements& wordMeasurements, bool& hyphenated,  unsigned& consecutiveHyphenatedLines)
{
    if (!m_current.offset())
        m_appliedStartWidth = false;

    RenderObject& renderObject = *m_current.renderer();
    RenderText& renderText = downcast<RenderText>(renderObject);

    bool isSVGText = renderText.isSVGInlineText();

    // If we have left a no-wrap inline and entered an autowrap inline while ignoring spaces
    // then we need to mark the start of the autowrap inline as a potential linebreak now.
    if (m_autoWrap && !RenderStyle::autoWrap(m_lastWS) && m_ignoringSpaces)
        commitLineBreakAtCurrentWidth(renderText);

    if (renderText.style().hasTextCombine() && is<RenderCombineText>(*m_current.renderer())) {
        auto& combineRenderer = downcast<RenderCombineText>(*m_current.renderer());
        combineRenderer.combineTextIfNeeded();
        // The length of the renderer's text may have changed. Increment stale iterator positions
        if (iteratorIsBeyondEndOfRenderCombineText(m_lineBreakHistory.current(), combineRenderer)) {
            ASSERT(iteratorIsBeyondEndOfRenderCombineText(m_resolver.position(), combineRenderer));
            m_lineBreakHistory.increment();
            m_resolver.increment();
        }
    }

    const RenderStyle& style = lineStyle(renderText, m_lineInfo);
    const FontCascade& font = style.fontCascade();
    bool isFixedPitch = font.isFixedPitch();
    bool canHyphenate = style.hyphens() == Hyphens::Auto && WebCore::canHyphenate(style.locale());
    bool canHangPunctuationAtStart = style.hangingPunctuation().contains(HangingPunctuation::First);
    bool canHangPunctuationAtEnd = style.hangingPunctuation().contains(HangingPunctuation::Last);
    bool canHangStopOrCommaAtLineEnd = style.hangingPunctuation().contains(HangingPunctuation::AllowEnd);
    int endPunctuationIndex = canHangPunctuationAtEnd && m_collapseWhiteSpace ? renderText.lastCharacterIndexStrippingSpaces() : renderText.text().length() - 1;
    unsigned lastSpace = m_current.offset();
    float wordSpacing = m_currentStyle->fontCascade().wordSpacing();
    float lastSpaceWordSpacing = 0;
    float wordSpacingForWordMeasurement = 0;

    float wrapWidthOffset = m_width.uncommittedWidth() + inlineLogicalWidth(m_current.renderer(), !m_appliedStartWidth, true);
    float wrapW = wrapWidthOffset;
    float charWidth = 0;
    bool breakNBSP = m_autoWrap && m_currentStyle->nbspMode() == NBSPMode::Space;
    // Auto-wrapping text should wrap in the middle of a word only if it could not wrap before the word,
    // which is only possible if the word is the first thing on the line.
    bool breakWords = m_currentStyle->breakWords() && ((m_autoWrap && (!m_width.committedWidth() && !m_width.hasCommittedReplaced())) || m_currWS == WhiteSpace::Pre);
    bool midWordBreak = false;
    bool breakAll = m_currentStyle->wordBreak() == WordBreak::BreakAll && m_autoWrap;
    bool keepAllWords = m_currentStyle->wordBreak() == WordBreak::KeepAll;
    float hyphenWidth = 0;
    auto iteratorMode = mapLineBreakToIteratorMode(m_blockStyle.lineBreak());
    bool canUseLineBreakShortcut = iteratorMode == LineBreakIteratorMode::Default;
    bool isLineEmpty = m_lineInfo.isEmpty();

    if (isSVGText) {
        breakWords = false;
        breakAll = false;
    }

    if (m_renderTextInfo.text != &renderText) {
        updateCounterIfNeeded(renderText);
        m_renderTextInfo.text = &renderText;
        m_renderTextInfo.font = &font;
        m_renderTextInfo.layout = font.createLayout(renderText, m_width.currentWidth(), m_collapseWhiteSpace);
        m_renderTextInfo.lineBreakIterator.resetStringAndReleaseIterator(renderText.text(), style.locale(), iteratorMode);
    } else if (m_renderTextInfo.layout && m_renderTextInfo.font != &font) {
        m_renderTextInfo.font = &font;
        m_renderTextInfo.layout = font.createLayout(renderText, m_width.currentWidth(), m_collapseWhiteSpace);
    }

    HashSet<const Font*> fallbackFonts;
    UChar lastCharacterFromPreviousRenderText = m_renderTextInfo.lineBreakIterator.lastCharacter();
    UChar lastCharacter = m_renderTextInfo.lineBreakIterator.lastCharacter();
    UChar secondToLastCharacter = m_renderTextInfo.lineBreakIterator.secondToLastCharacter();
    // Non-zero only when kerning is enabled and TextLayout isn't used, in which case we measure
    // words with their trailing space, then subtract its width.
    TextLayout* textLayout = m_renderTextInfo.layout.get();
    WordTrailingSpace wordTrailingSpace(style, !textLayout);
    for (; m_current.offset() < renderText.text().length(); m_current.fastIncrementInTextNode()) {
        bool previousCharacterIsSpace = m_currentCharacterIsSpace;
        bool previousCharacterIsWS = m_currentCharacterIsWS;
        UChar c = m_current.current();
        m_currentCharacterIsSpace = c == ' ' || c == '\t' || (!m_preservesNewline && (c == '\n'));

        if (canHangPunctuationAtStart && m_width.isFirstLine() && !m_width.committedWidth() && !wrapW && !inlineLogicalWidth(m_current.renderer(), true, false)) {
            m_width.addUncommittedWidth(-renderText.hangablePunctuationStartWidth(m_current.offset()));
            canHangPunctuationAtStart = false;
        }
        
        if (canHangPunctuationAtEnd && !m_nextObject && (int)m_current.offset() == endPunctuationIndex && !inlineLogicalWidth(m_current.renderer(), false, true)) {
            m_width.addUncommittedWidth(-renderText.hangablePunctuationEndWidth(endPunctuationIndex));
            canHangPunctuationAtEnd = false;
        }

        if (!m_collapseWhiteSpace || !m_currentCharacterIsSpace)
            m_lineInfo.setEmpty(false, &m_block, &m_width);

        if (c == softHyphen && m_autoWrap && !hyphenWidth && style.hyphens() != Hyphens::None) {
            hyphenWidth = measureHyphenWidth(renderText, font, &fallbackFonts);
            m_width.addUncommittedWidth(hyphenWidth);
        }

        bool applyWordSpacing = false;

        m_currentCharacterIsWS = m_currentCharacterIsSpace || (breakNBSP && c == noBreakSpace);

        if ((breakAll || breakWords) && !midWordBreak && (!m_currentCharacterIsSpace || style.whiteSpace() != WhiteSpace::PreWrap)) {
            wrapW += charWidth;
            bool midWordBreakIsBeforeSurrogatePair = U16_IS_LEAD(c) && U16_IS_TRAIL(renderText.characterAt(m_current.offset() + 1));
            charWidth = textWidth(renderText, m_current.offset(), midWordBreakIsBeforeSurrogatePair ? 2 : 1, font, m_width.committedWidth() + wrapW, isFixedPitch, m_collapseWhiteSpace, fallbackFonts, textLayout);
            midWordBreak = m_width.committedWidth() + wrapW + charWidth > m_width.availableWidth();
        }

        std::optional<unsigned> nextBreakablePosition = m_current.nextBreakablePosition();
        bool betweenWords = c == '\n' || (m_currWS != WhiteSpace::Pre && !m_atStart && isBreakable(m_renderTextInfo.lineBreakIterator, m_current.offset(), nextBreakablePosition, breakNBSP, canUseLineBreakShortcut, keepAllWords)
            && (style.hyphens() != Hyphens::None || (m_current.previousInSameNode() != softHyphen)));
        m_current.setNextBreakablePosition(nextBreakablePosition);
        
        if (canHangStopOrCommaAtLineEnd && renderText.isHangableStopOrComma(c) && m_width.fitsOnLine()) {
            // We need to see if a measurement that excludes the stop would fit. If so, then we should hang
            // the stop/comma at the end. First measure including the comma.
            m_hangsAtEnd = false;
            float inlineStartWidth = !m_appliedStartWidth ? inlineLogicalWidth(m_current.renderer(), true, false) : LayoutUnit();
            float widthIncludingComma = computeAdditionalBetweenWordsWidth(renderText, textLayout, c, wordTrailingSpace, fallbackFonts, wordMeasurements, font, isFixedPitch, lastSpace, lastSpaceWordSpacing, wordSpacingForWordMeasurement, m_current.offset() + 1) + inlineStartWidth;
            m_width.addUncommittedWidth(widthIncludingComma);
            if (!m_width.fitsOnLine()) {
                // See if we fit without the comma involved. If we do, then this is a potential hang point.
                float widthWithoutStopOrComma = computeAdditionalBetweenWordsWidth(renderText, textLayout, lastCharacter, wordTrailingSpace, fallbackFonts, wordMeasurements, font, isFixedPitch, lastSpace, lastSpaceWordSpacing, wordSpacingForWordMeasurement, m_current.offset()) + inlineStartWidth;
                m_width.addUncommittedWidth(widthWithoutStopOrComma - widthIncludingComma);
                if (m_width.fitsOnLine())
                    m_hangsAtEnd = true;
            } else
                m_width.addUncommittedWidth(-widthIncludingComma);
        }
        
        if (betweenWords || midWordBreak) {
            bool stoppedIgnoringSpaces = false;
            if (m_ignoringSpaces) {
                lastSpaceWordSpacing = 0;
                if (!m_currentCharacterIsSpace) {
                    // Stop ignoring spaces and begin at this new point.
                    m_ignoringSpaces = false;
                    wordSpacingForWordMeasurement = 0;
                    lastSpace = m_current.offset(); // e.g., "Foo    goo", don't add in any of the ignored spaces.
                    m_lineWhitespaceCollapsingState.stopIgnoringSpaces(InlineIterator(0, m_current.renderer(), m_current.offset()));
                    stoppedIgnoringSpaces = true;
                } else {
                    // Just keep ignoring these spaces.
                    nextCharacter(c, lastCharacter, secondToLastCharacter);
                    continue;
                }
            }
            
            float additionalTempWidth = computeAdditionalBetweenWordsWidth(renderText, textLayout, c, wordTrailingSpace, fallbackFonts, wordMeasurements, font, isFixedPitch, lastSpace, lastSpaceWordSpacing, wordSpacingForWordMeasurement, m_current.offset());
            m_width.addUncommittedWidth(additionalTempWidth);
            
            WordMeasurement& wordMeasurement = wordMeasurements.last();

            if (m_collapseWhiteSpace && previousCharacterIsSpace && m_currentCharacterIsSpace && additionalTempWidth)
                m_width.setTrailingWhitespaceWidth(additionalTempWidth);

            if (!m_appliedStartWidth) {
                float inlineStartWidth = inlineLogicalWidth(m_current.renderer(), true, false);
                m_width.addUncommittedWidth(inlineStartWidth);
                m_appliedStartWidth = true;
                if (m_hangsAtEnd && inlineStartWidth)
                    m_hangsAtEnd = false;
            }

            applyWordSpacing = wordSpacing && m_currentCharacterIsSpace;

            if (!m_width.hasCommitted() && m_autoWrap && !fitsOnLineOrHangsAtEnd())
                m_width.fitBelowFloats(m_lineInfo.isFirstLine());

            if (m_autoWrap || breakWords) {
                // If we break only after white-space, consider the current character
                // as candidate width for this line.
                bool lineWasTooWide = false;
                if (fitsOnLineOrHangsAtEnd() && m_currentCharacterIsWS && m_currentStyle->breakOnlyAfterWhiteSpace() && !midWordBreak) {
                    float charWidth = textWidth(renderText, m_current.offset(), 1, font, m_width.currentWidth(), isFixedPitch, m_collapseWhiteSpace, wordMeasurement.fallbackFonts, textLayout) + (applyWordSpacing ? wordSpacing : 0);
                    // Check if line is too big even without the extra space
                    // at the end of the line. If it is not, do nothing.
                    // If the line needs the extra whitespace to be too long,
                    // then move the line break to the space and skip all
                    // additional whitespace.
                    if (!m_width.fitsOnLineIncludingExtraWidth(charWidth)) {
                        lineWasTooWide = true;
                        m_lineBreakHistory.push([&](InlineIterator& modifyMe) {
                            modifyMe.moveTo(renderObject, m_current.offset(), m_current.nextBreakablePosition());
                            m_lineBreaker.skipTrailingWhitespace(modifyMe, m_lineInfo);
                        });
                    }
                }
                if ((lineWasTooWide || !m_width.fitsOnLine()) && !m_hangsAtEnd) {
                    // Don't try to hyphenate at the final break of a block, since this means there is
                    // no more content, and a hyphenated single word would end up on a line by itself. This looks
                    // bad so just don't allow it.
                    if (canHyphenate && !m_width.fitsOnLine() && (m_nextObject || !renderText.containsOnlyHTMLWhitespace(m_current.offset(), renderText.text().length() - m_current.offset()) || isLineEmpty)) {
                        m_lineBreakHistory.push([&](InlineIterator& modifyMe) {
                            tryHyphenating(renderText, font, style.locale(), consecutiveHyphenatedLines, m_blockStyle.hyphenationLimitLines(), style.hyphenationLimitBefore(), style.hyphenationLimitAfter(), lastSpace, m_current.offset(), m_width.currentWidth() - additionalTempWidth, m_width.availableWidth(), isFixedPitch, m_collapseWhiteSpace, lastSpaceWordSpacing, modifyMe, m_current.nextBreakablePosition(), m_lineBreaker.m_hyphenated);
                        });
                        if (m_lineBreaker.m_hyphenated) {
                            m_atEnd = true;
                            return false;
                        }
                    }
                    if (m_lineBreakHistory.atTextParagraphSeparator()) {
                        if (!stoppedIgnoringSpaces && m_current.offset() > 0)
                            ensureCharacterGetsLineBox(m_lineWhitespaceCollapsingState, m_current);
                        m_lineBreakHistory.increment();
                        m_lineInfo.setPreviousLineBrokeCleanly(true);
                        wordMeasurement.endOffset = m_lineBreakHistory.offset();
                    }
                    // Check if the last breaking position is a soft-hyphen.
                    if (!hyphenated && style.hyphens() != Hyphens::None) {
                        std::optional<unsigned> lastBreakingPositon;
                        const RenderObject* rendererAtBreakingPosition = nullptr;
                        if (m_lineBreakHistory.offset() || m_lineBreakHistory.nextBreakablePosition()) {
                            lastBreakingPositon = m_lineBreakHistory.offset();
                            rendererAtBreakingPosition = m_lineBreakHistory.renderer();
                        } else if (m_current.nextBreakablePosition() && m_current.nextBreakablePosition().value() <= m_current.offset()) {
                            // We might just be right after the soft-hyphen
                            lastBreakingPositon = m_current.nextBreakablePosition().value();
                            rendererAtBreakingPosition = m_current.renderer();
                        }
                        if (lastBreakingPositon) {
                            std::optional<UChar> characterBeforeBreakingPosition;
                            // When last breaking position points to the start of the current context, we need to look at the last character from
                            // the previous non-empty text renderer.
                            if (!lastBreakingPositon.value())
                                characterBeforeBreakingPosition = lastCharacterFromPreviousRenderText;
                            else if (is<RenderText>(rendererAtBreakingPosition)) {
                                const auto& textRenderer = downcast<RenderText>(*rendererAtBreakingPosition);
                                ASSERT(lastBreakingPositon.value() >= 1 && textRenderer.text().length() > (lastBreakingPositon.value() - 1));
                                characterBeforeBreakingPosition = textRenderer.characterAt(lastBreakingPositon.value() - 1);
                            }
                            if (characterBeforeBreakingPosition)
                                hyphenated = characterBeforeBreakingPosition.value() == softHyphen;
                        }
                    }
                    if (m_lineBreakHistory.offset() && m_lineBreakHistory.offset() != (unsigned)wordMeasurement.endOffset && !wordMeasurement.width) {
                        if (charWidth) {
                            wordMeasurement.endOffset = m_lineBreakHistory.offset();
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
                    ensureCharacterGetsLineBox(m_lineWhitespaceCollapsingState, m_current);
                commitLineBreakAtCurrentWidth(renderObject, m_current.offset(), m_current.nextBreakablePosition());
                m_lineBreakHistory.increment();
                m_lineInfo.setPreviousLineBrokeCleanly(true);
                return true;
            }

            if (m_autoWrap && betweenWords) {
                commitLineBreakAtCurrentWidth(renderObject, m_current.offset(), m_current.nextBreakablePosition());
                wrapWidthOffset = 0;
                wrapW = wrapWidthOffset;
                // Auto-wrapping text should not wrap in the middle of a word once it has had an
                // opportunity to break after a word.
                breakWords = false;
            }

            if (midWordBreak && !U16_IS_TRAIL(c) && !(U_GET_GC_MASK(c) & U_GC_M_MASK)) {
                // Remember this as a breakable position in case
                // adding the end width forces a break.
                m_lineBreakHistory.moveTo(renderObject, m_current.offset(), m_current.nextBreakablePosition());
                midWordBreak &= (breakWords || breakAll);
            }

            if (betweenWords) {
                lastSpaceWordSpacing = applyWordSpacing ? wordSpacing : 0;
                wordSpacingForWordMeasurement = (applyWordSpacing && wordMeasurement.width) ? wordSpacing : 0;
                lastSpace = m_current.offset();
            }

            if (!m_ignoringSpaces && m_currentStyle->collapseWhiteSpace()) {
                // If we encounter a newline, or if we encounter a second space,
                // we need to break up this run and enter a mode where we start collapsing spaces.
                if (m_currentCharacterIsSpace && previousCharacterIsSpace) {
                    m_ignoringSpaces = true;

                    // We just entered a mode where we are ignoring
                    // spaces. Create a transition to terminate the run
                    // before the second space.
                    m_lineWhitespaceCollapsingState.startIgnoringSpaces(m_startOfIgnoredSpaces);
                    m_trailingObjects.updateWhitespaceCollapsingTransitionsForTrailingBoxes(m_lineWhitespaceCollapsingState, InlineIterator(), TrailingObjects::DoNotCollapseFirstSpace);
                }
            }
            // Measuring the width of complex text character-by-character, rather than measuring it all together,
            // could produce considerably different width values.
            if (!renderText.canUseSimpleFontCodePath() && midWordBreak && m_width.fitsOnLine()) {
                midWordBreak = false;
                wrapW = wrapWidthOffset + additionalTempWidth;
            }
            isLineEmpty = m_lineInfo.isEmpty();
        } else {
            if (m_ignoringSpaces) {
                // Stop ignoring spaces and begin at this new point.
                m_ignoringSpaces = false;
                lastSpaceWordSpacing = applyWordSpacing ? wordSpacing : 0;
                wordSpacingForWordMeasurement = (applyWordSpacing && wordMeasurements.last().width) ? wordSpacing : 0;
                lastSpace = m_current.offset(); // e.g., "Foo    goo", don't add in any of the ignored spaces.
                m_lineWhitespaceCollapsingState.stopIgnoringSpaces(InlineIterator(nullptr, m_current.renderer(), m_current.offset()));
            }
            if (m_hangsAtEnd && !renderText.isHangableStopOrComma(c))
                m_hangsAtEnd = false;
        }

        if (isSVGText && m_current.offset()) {
            // Force creation of new InlineBoxes for each absolute positioned character (those that start new text chunks).
            if (downcast<RenderSVGInlineText>(renderText).characterStartsNewTextChunk(m_current.offset()))
                ensureCharacterGetsLineBox(m_lineWhitespaceCollapsingState, m_current);
        }

        if (m_currentCharacterIsSpace && !previousCharacterIsSpace) {
            m_startOfIgnoredSpaces.setRenderer(m_current.renderer());
            m_startOfIgnoredSpaces.setOffset(m_current.offset());
            // Spaces after right-aligned text and before a line-break get collapsed away completely so that the trailing
            // space doesn't seem to push the text out from the right-hand edge.
            // FIXME: Do this regardless of the container's alignment - will require rebaselining a lot of test results.
            if (m_nextObject && m_startOfIgnoredSpaces.offset() && m_nextObject->isBR() && (m_blockStyle.textAlign() == TextAlignMode::Right || m_blockStyle.textAlign() == TextAlignMode::WebKitRight)) {
                m_startOfIgnoredSpaces.setOffset(m_startOfIgnoredSpaces.offset() - 1);
                // If there's just a single trailing space start ignoring it now so it collapses away.
                if (m_current.offset() == renderText.text().length() - 1)
                    m_lineWhitespaceCollapsingState.startIgnoringSpaces(m_startOfIgnoredSpaces);
            }
        }

        if (!m_currentCharacterIsWS && previousCharacterIsWS) {
            if (m_autoWrap && m_currentStyle->breakOnlyAfterWhiteSpace())
                m_lineBreakHistory.moveTo(renderObject, m_current.offset(), m_current.nextBreakablePosition());
        }

        if (m_collapseWhiteSpace && m_currentCharacterIsSpace && !m_ignoringSpaces)
            m_trailingObjects.setTrailingWhitespace(downcast<RenderText>(m_current.renderer()));
        else if (!m_currentStyle->collapseWhiteSpace() || !m_currentCharacterIsSpace)
            m_trailingObjects.clear();

        m_atStart = false;
        nextCharacter(c, lastCharacter, secondToLastCharacter);
    }

    m_renderTextInfo.lineBreakIterator.setPriorContext(lastCharacter, secondToLastCharacter);

    wordMeasurements.grow(wordMeasurements.size() + 1);
    WordMeasurement& wordMeasurement = wordMeasurements.last();
    wordMeasurement.renderer = &renderText;

    // IMPORTANT: current.m_pos is > length here!
    float additionalTempWidth = m_ignoringSpaces ? 0 : textWidth(renderText, lastSpace, m_current.offset() - lastSpace, font, m_width.currentWidth(), isFixedPitch, m_collapseWhiteSpace, wordMeasurement.fallbackFonts, textLayout);
    wordMeasurement.startOffset = lastSpace;
    wordMeasurement.endOffset = m_current.offset();
    wordMeasurement.width = m_ignoringSpaces ? 0 : additionalTempWidth + wordSpacingForWordMeasurement;
    additionalTempWidth += lastSpaceWordSpacing;

    float inlineLogicalTempWidth = inlineLogicalWidth(m_current.renderer(), !m_appliedStartWidth, m_includeEndWidth);
    m_width.addUncommittedWidth(additionalTempWidth + inlineLogicalTempWidth);
    if (m_hangsAtEnd && inlineLogicalTempWidth)
        m_hangsAtEnd = false;

    if (wordMeasurement.fallbackFonts.isEmpty() && !fallbackFonts.isEmpty())
        wordMeasurement.fallbackFonts.swap(fallbackFonts);
    fallbackFonts.clear();

    if (m_collapseWhiteSpace && m_currentCharacterIsSpace && additionalTempWidth)
        m_width.setTrailingWhitespaceWidth(additionalTempWidth, inlineLogicalTempWidth);

    m_includeEndWidth = false;

    if (!fitsOnLineOrHangsAtEnd()) {
        // Don't try to hyphenate at the final break of a block, since this means there is
        // no more content, and a hyphenated single word would end up on a line by itself. This looks
        // bad so just don't allow it.
        if (canHyphenate && (m_nextObject || isLineEmpty)) {
            m_lineBreakHistory.push([&](InlineIterator& modifyMe) {
                tryHyphenating(renderText, font, style.locale(), consecutiveHyphenatedLines, m_blockStyle.hyphenationLimitLines(), style.hyphenationLimitBefore(), style.hyphenationLimitAfter(), lastSpace, m_current.offset(), m_width.currentWidth() - additionalTempWidth, m_width.availableWidth(), isFixedPitch, m_collapseWhiteSpace, lastSpaceWordSpacing, modifyMe, m_current.nextBreakablePosition(), m_lineBreaker.m_hyphenated);
            });
        }

        if (!hyphenated && m_lineBreakHistory.previousInSameNode() == softHyphen && style.hyphens() != Hyphens::None) {
            hyphenated = true;
            m_atEnd = true;
        }
    }
    return false;
}

inline bool textBeginsWithBreakablePosition(RenderText& nextText)
{
    UChar c = nextText.characterAt(0);
    return c == ' ' || c == '\t' || (c == '\n' && !nextText.preservesNewline());
}

inline bool BreakingContext::canBreakAtThisPosition()
{
    // If we are no-wrap and have found a line-breaking opportunity already then we should take it.
    if (m_width.committedWidth() && !m_width.fitsOnLine(m_currentCharacterIsSpace) && m_currWS == WhiteSpace::NoWrap)
        return true;

    // Avoid breaking on empty inlines.
    if (is<RenderInline>(*m_current.renderer()) && isEmptyInline(downcast<RenderInline>(*m_current.renderer())))
        return false;

    // Avoid breaking before empty inlines (as long as the current object isn't replaced).
    if (!m_current.renderer()->isReplaced() && is<RenderInline>(m_nextObject) && isEmptyInline(downcast<RenderInline>(*m_nextObject)))
        return false;

    // Return early if we autowrap and the current character is a space as we will always want to break at such a position.
    if (m_autoWrap && m_currentCharacterIsSpace)
        return true;

    if (m_nextObject && m_nextObject->isLineBreakOpportunity())
        return m_autoWrap;

    bool nextIsAutoWrappingText = is<RenderText>(m_nextObject) && (m_autoWrap || m_nextObject->style().autoWrap());
    if (!nextIsAutoWrappingText)
        return m_autoWrap;
    RenderText& nextRenderText = downcast<RenderText>(*m_nextObject);
    bool currentIsTextOrEmptyInline = is<RenderText>(*m_current.renderer()) || (is<RenderInline>(*m_current.renderer()) && isEmptyInline(downcast<RenderInline>(*m_current.renderer())));
    if (!currentIsTextOrEmptyInline)
        return m_autoWrap && !m_current.renderer()->isRubyRun();

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
        if (m_currentCharacterIsSpace && !m_ignoringSpaces && m_currentStyle->collapseWhiteSpace())
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
        if (m_lastObject->isReplaced() && m_autoWrap && !m_lastObject->isRubyRun() && (!m_lastObject->isImage() || m_allowImagesToBreak) && (!is<RenderListMarker>(*m_lastObject) || downcast<RenderListMarker>(*m_lastObject).isInside())) {
            if (m_nextObject)
                commitLineBreakAtCurrentWidth(*m_nextObject);
            else
                commitLineBreakClear();
        }
    }
}

inline TrailingObjects::CollapseFirstSpaceOrNot checkWhitespaceCollapsingTransitions(LineWhitespaceCollapsingState& lineWhitespaceCollapsingState, const InlineIterator& lBreak)
{
    // Check to see if our last transition is a start point beyond the line break. If so,
    // shave it off the list, and shave off a trailing space if the previous end point doesn't
    // preserve whitespace.
    if (lBreak.renderer() && lineWhitespaceCollapsingState.numTransitions() && !(lineWhitespaceCollapsingState.numTransitions() % 2)) {
        const InlineIterator* transitions = lineWhitespaceCollapsingState.transitions().data();
        const InlineIterator& endpoint = transitions[lineWhitespaceCollapsingState.numTransitions() - 2];
        const InlineIterator& startpoint = transitions[lineWhitespaceCollapsingState.numTransitions() - 1];
        InlineIterator currpoint = endpoint;
        while (!currpoint.atEnd() && currpoint != startpoint && currpoint != lBreak)
            currpoint.increment();
        if (currpoint == lBreak) {
            // We hit the line break before the start point. Shave off the start point.
            lineWhitespaceCollapsingState.decrementNumTransitions();
            if (endpoint.renderer()->style().collapseWhiteSpace() && endpoint.renderer()->isText()) {
                lineWhitespaceCollapsingState.decrementTransitionAt(lineWhitespaceCollapsingState.numTransitions() - 1);
                return TrailingObjects::DoNotCollapseFirstSpace;
            }
        }
    }
    return TrailingObjects::CollapseFirstSpace;
}

inline InlineIterator BreakingContext::handleEndOfLine()
{
    if (m_lineBreakHistory.current() == m_resolver.position()) {
        if (!m_lineBreakHistory.renderer() || !m_lineBreakHistory.renderer()->isBR()) {
            // we just add as much as possible
            if (m_blockStyle.whiteSpace() == WhiteSpace::Pre && !m_current.offset()) {
                if (m_lastObject)
                    commitLineBreakAtCurrentWidth(*m_lastObject, m_lastObject->isText() ? m_lastObject->length() : 0);
                else
                    commitLineBreakClear();
            }
            else if (m_lineBreakHistory.renderer()) {
                // Don't ever break in the middle of a word if we can help it.
                // There's no room at all. We just have to be on this line,
                // even though we'll spill out.
                commitLineBreakAtCurrentWidth(*m_current.renderer(), m_current.offset());
            }
        }
        // make sure we consume at least one char/object.
        if (m_lineBreakHistory.current() == m_resolver.position())
            m_lineBreakHistory.increment();
    } else if (!m_current.offset() && !m_width.committedWidth() && m_width.uncommittedWidth() && !m_hadUncommittedWidthBeforeCurrent) {
        // Do not push the current object to the next line, when this line has some content, but it is still considered empty.
        // Empty inline elements like <span></span> can produce such lines and now we just ignore these break opportunities
        // at the start of a line, if no width has been committed yet.
        // Behave as if it was actually empty and consume at least one object.
        m_lineBreakHistory.increment();
    }

    // Sanity check our whitespace collapsing transitions.
    TrailingObjects::CollapseFirstSpaceOrNot collapsed = checkWhitespaceCollapsingTransitions(m_lineWhitespaceCollapsingState, m_lineBreakHistory.current());

    m_trailingObjects.updateWhitespaceCollapsingTransitionsForTrailingBoxes(m_lineWhitespaceCollapsingState, m_lineBreakHistory.current(), collapsed);

    // We might have made lineBreak an iterator that points past the end
    // of the object. Do this adjustment to make it point to the start
    // of the next object instead to avoid confusing the rest of the
    // code.
    if (m_lineBreakHistory.offset()) {
        m_lineBreakHistory.update([](InlineIterator& modifyMe) {
            modifyMe.setOffset(modifyMe.offset() - 1);
            modifyMe.increment();
        });
    }

#if ENABLE(CSS_TRAILING_WORD)
    if (m_blockStyle.trailingWord() == TrailingWord::PartiallyBalanced)
        return optimalLineBreakLocationForTrailingWord();
#endif
    return m_lineBreakHistory.current();
}

#if ENABLE(CSS_TRAILING_WORD)
inline InlineIterator BreakingContext::optimalLineBreakLocationForTrailingWord()
{
    const unsigned longTrailingWordLength = 20;
    const float optimalTrailingLineRatio = 0.1;
    InlineIterator lineBreak = m_lineBreakHistory.current();
    if (!lineBreak.renderer() || !m_lineInfo.isFirstLine() || bidiNextSkippingEmptyInlines(*lineBreak.root(), lineBreak.renderer()) || !is<RenderText>(lineBreak.renderer()))
        return lineBreak;
    RenderText& renderText = downcast<RenderText>(*lineBreak.renderer());
    // Don't even bother measuring if our remaining line has many characters
    if (renderText.text().length() == lineBreak.offset() || renderText.text().length() - lineBreak.offset() > longTrailingWordLength)
        return lineBreak;
    bool canUseLineBreakShortcut = m_renderTextInfo.lineBreakIterator.mode() == LineBreakIteratorMode::Default;
    bool breakNBSP = m_autoWrap && m_currentStyle->nbspMode() == NBSPMode::Space;
    std::optional<unsigned> nextBreakablePosition = lineBreak.nextBreakablePosition();
    isBreakable(m_renderTextInfo.lineBreakIterator, lineBreak.offset() + 1, nextBreakablePosition, breakNBSP, canUseLineBreakShortcut, m_currentStyle->wordBreak() == WordBreak::KeepAll);
    if (!nextBreakablePosition || nextBreakablePosition.value() != renderText.text().length())
        return lineBreak;
    const RenderStyle& style = lineStyle(renderText, m_lineInfo);
    const FontCascade& font = style.fontCascade();
    HashSet<const Font*> dummyFonts;
    InlineIterator best = lineBreak;
    for (size_t i = 1; i < m_lineBreakHistory.historyLength(); ++i) {
        const InlineIterator& candidate = m_lineBreakHistory.get(i);
        if (candidate.renderer() != lineBreak.renderer())
            return best;
        float width = textWidth(renderText, candidate.offset(), renderText.text().length() - candidate.offset(), font, 0, font.isFixedPitch(), m_collapseWhiteSpace, dummyFonts);
        if (width > m_width.availableWidth())
            return best;
        if (width / m_width.availableWidth() > optimalTrailingLineRatio) // Subsequent line is long enough
            return candidate;
        best = candidate;
    }
    return best;
}
#endif

}
