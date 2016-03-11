/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SimpleLineLayout.h"

#include "FontCache.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HTMLTextFormControlElement.h"
#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "InlineTextBox.h"
#include "LineWidth.h"
#include "Logging.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderLineBreak.h"
#include "RenderStyle.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderView.h"
#include "Settings.h"
#include "SimpleLineLayoutFlowContents.h"
#include "SimpleLineLayoutFunctions.h"
#include "SimpleLineLayoutTextFragmentIterator.h"
#include "Text.h"
#include "TextPaintStyle.h"
#include "TextStream.h"

namespace WebCore {
namespace SimpleLineLayout {

#ifndef NDEBUG
void printSimpleLineLayoutCoverage();
void printSimpleLineLayoutBlockList();
void toggleSimpleLineLayout();
#endif

enum AvoidanceReason_ : uint64_t {
    FlowIsInsideRegion                    = 1LLU  << 0,
    FlowHasHorizonalWritingMode           = 1LLU  << 1,
    FlowHasOutline                        = 1LLU  << 2,
    FlowIsRuby                            = 1LLU  << 3,
    FlowIsPaginated                       = 1LLU  << 4,
    FlowHasTextOverflow                   = 1LLU  << 5,
    FlowIsDepricatedFlexBox               = 1LLU  << 6,
    FlowParentIsPlaceholderElement        = 1LLU  << 7,
    FlowParentIsTextAreaWithWrapping      = 1LLU  << 8,
    FlowHasNonSupportedChild              = 1LLU  << 9,
    FlowHasUnsupportedFloat               = 1LLU  << 10,
    FlowHasUnsupportedUnderlineDecoration = 1LLU  << 11,
    FlowHasJustifiedNonLatinText          = 1LLU  << 12,
    FlowHasOverflowVisible                = 1LLU  << 13,
    FlowIsNotLTR                          = 1LLU  << 14,
    FlowHasLineBoxContainProperty         = 1LLU  << 15,
    FlowIsNotTopToBottom                  = 1LLU  << 16,
    FlowHasLineBreak                      = 1LLU  << 17,
    FlowHasNonNormalUnicodeBiDi           = 1LLU  << 18,
    FlowHasRTLOrdering                    = 1LLU  << 19,
    FlowHasLineAlignEdges                 = 1LLU  << 20,
    FlowHasLineSnap                       = 1LLU  << 21,
    FlowHasHypensAuto                     = 1LLU  << 22,
    FlowHasTextEmphasisFillOrMark         = 1LLU  << 23,
    FlowHasTextShadow                     = 1LLU  << 24,
    FlowHasPseudoFirstLine                = 1LLU  << 25,
    FlowHasPseudoFirstLetter              = 1LLU  << 26,
    FlowHasTextCombine                    = 1LLU  << 27,
    FlowHasTextFillBox                    = 1LLU  << 28,
    FlowHasBorderFitLines                 = 1LLU  << 29,
    FlowHasNonAutoLineBreak               = 1LLU  << 30,
    FlowHasNonAutoTrailingWord            = 1LLU  << 31,
    FlowHasSVGFont                        = 1LLU  << 32,
    FlowTextIsEmpty                       = 1LLU  << 33,
    FlowTextHasNoBreakSpace               = 1LLU  << 34,
    FlowTextHasSoftHyphen                 = 1LLU  << 35,
    FlowTextHasDirectionCharacter         = 1LLU  << 36,
    FlowIsMissingPrimaryFont              = 1LLU  << 37,
    FlowFontIsMissingGlyph                = 1LLU  << 38,
    FlowTextIsCombineText                 = 1LLU  << 39,
    FlowTextIsRenderCounter               = 1LLU  << 40,
    FlowTextIsRenderQuote                 = 1LLU  << 41,
    FlowTextIsTextFragment                = 1LLU  << 42,
    FlowTextIsSVGInlineText               = 1LLU  << 43,
    FlowFontIsNotSimple                   = 1LLU  << 44,
    FeatureIsDisabled                     = 1LLU  << 45,
    FlowHasNoParent                       = 1LLU  << 46,
    FlowHasNoChild                        = 1LLU  << 47,
    FlowChildIsSelected                   = 1LLU  << 48,
    FlowHasHangingPunctuation             = 1LLU  << 49,
    EndOfReasons                          = 1LLU  << 50
};
const unsigned NoReason = 0;

typedef uint64_t AvoidanceReason;
typedef uint64_t AvoidanceReasonFlags;

enum class IncludeReasons { First , All };

#ifndef NDEBUG
#define SET_REASON_AND_RETURN_IF_NEEDED(reason, reasons, includeReasons) { \
        reasons |= reason; \
        if (includeReasons == IncludeReasons::First) \
            return reasons; \
    }
#else
#define SET_REASON_AND_RETURN_IF_NEEDED(reason, reasons, includeReasons) { \
        ASSERT_UNUSED(includeReasons, includeReasons == IncludeReasons::First); \
        reasons |= reason; \
        return reasons; \
    }
#endif

template <typename CharacterType>
static AvoidanceReasonFlags canUseForText(const CharacterType* text, unsigned length, const Font& font, IncludeReasons includeReasons)
{
    AvoidanceReasonFlags reasons = { };
    // FIXME: <textarea maxlength=0> generates empty text node.
    if (!length)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowTextIsEmpty, reasons, includeReasons);

    for (unsigned i = 0; i < length; ++i) {
        UChar character = text[i];
        if (character == ' ')
            continue;

        // These would be easy to support.
        if (character == noBreakSpace)
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextHasNoBreakSpace, reasons, includeReasons);
        if (character == softHyphen)
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextHasSoftHyphen, reasons, includeReasons);

        UCharDirection direction = u_charDirection(character);
        if (direction == U_RIGHT_TO_LEFT || direction == U_RIGHT_TO_LEFT_ARABIC
            || direction == U_RIGHT_TO_LEFT_EMBEDDING || direction == U_RIGHT_TO_LEFT_OVERRIDE
            || direction == U_LEFT_TO_RIGHT_EMBEDDING || direction == U_LEFT_TO_RIGHT_OVERRIDE
            || direction == U_POP_DIRECTIONAL_FORMAT || direction == U_BOUNDARY_NEUTRAL)
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextHasDirectionCharacter, reasons, includeReasons);

        if (!font.glyphForCharacter(character))
            SET_REASON_AND_RETURN_IF_NEEDED(FlowFontIsMissingGlyph, reasons, includeReasons);
    }
    return reasons;
}

static AvoidanceReasonFlags canUseForText(const RenderText& textRenderer, const Font& font, IncludeReasons includeReasons)
{
    if (textRenderer.is8Bit())
        return canUseForText(textRenderer.characters8(), textRenderer.textLength(), font, includeReasons);
    return canUseForText(textRenderer.characters16(), textRenderer.textLength(), font, includeReasons);
}

static AvoidanceReasonFlags canUseForFontAndText(const RenderBlockFlow& flow, IncludeReasons includeReasons)
{
    AvoidanceReasonFlags reasons = { };
    // We assume that all lines have metrics based purely on the primary font.
    const auto& style = flow.style();
    auto& primaryFont = style.fontCascade().primaryFont();
    if (primaryFont.isLoading())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsMissingPrimaryFont, reasons, includeReasons);
    if (primaryFont.isSVGFont())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasSVGFont, reasons, includeReasons);

    for (const auto& textRenderer : childrenOfType<RenderText>(flow)) {
        if (style.textAlign() == JUSTIFY && !textRenderer.originalText().containsOnlyLatin1())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowHasJustifiedNonLatinText, reasons, includeReasons);
        if (textRenderer.isCombineText())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextIsCombineText, reasons, includeReasons);
        if (textRenderer.isCounter())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextIsRenderCounter, reasons, includeReasons);
        if (textRenderer.isQuote())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextIsRenderQuote, reasons, includeReasons);
        if (textRenderer.isTextFragment())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextIsTextFragment, reasons, includeReasons);
        if (textRenderer.isSVGInlineText())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextIsSVGInlineText, reasons, includeReasons);
        if (style.fontCascade().codePath(TextRun(textRenderer.text())) != FontCascade::Simple)
            SET_REASON_AND_RETURN_IF_NEEDED(FlowFontIsNotSimple, reasons, includeReasons);

        auto textReasons = canUseForText(textRenderer, primaryFont, includeReasons);
        if (textReasons != NoReason)
            SET_REASON_AND_RETURN_IF_NEEDED(textReasons, reasons, includeReasons);
    }
    return reasons;
}

static AvoidanceReasonFlags canUseForStyle(const RenderStyle& style, IncludeReasons includeReasons)
{
    AvoidanceReasonFlags reasons = { };
    if (style.textOverflow())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextOverflow, reasons, includeReasons);
    if ((style.textDecorationsInEffect() & TextDecorationUnderline) && style.textUnderlinePosition() == TextUnderlinePositionUnder)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasUnsupportedUnderlineDecoration, reasons, includeReasons);
    // Non-visible overflow should be pretty easy to support.
    if (style.overflowX() != OVISIBLE || style.overflowY() != OVISIBLE)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasOverflowVisible, reasons, includeReasons);
    if (!style.isLeftToRightDirection())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsNotLTR, reasons, includeReasons);
    if (style.lineBoxContain() != RenderStyle::initialLineBoxContain())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineBoxContainProperty, reasons, includeReasons);
    if (style.writingMode() != TopToBottomWritingMode)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsNotTopToBottom, reasons, includeReasons);
    if (style.lineBreak() != LineBreakAuto)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineBreak, reasons, includeReasons);
    if (style.unicodeBidi() != UBNormal)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasNonNormalUnicodeBiDi, reasons, includeReasons);
    if (style.rtlOrdering() != LogicalOrder)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasRTLOrdering, reasons, includeReasons);
    if (style.lineAlign() != LineAlignNone)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineAlignEdges, reasons, includeReasons);
    if (style.lineSnap() != LineSnapNone)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineSnap, reasons, includeReasons);
    if (style.hyphens() == HyphensAuto)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasHypensAuto, reasons, includeReasons);
    if (style.textEmphasisFill() != TextEmphasisFillFilled || style.textEmphasisMark() != TextEmphasisMarkNone)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextEmphasisFillOrMark, reasons, includeReasons);
    if (style.textShadow())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextShadow, reasons, includeReasons);
    if (style.hasPseudoStyle(FIRST_LINE))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasPseudoFirstLine, reasons, includeReasons);
    if (style.hasPseudoStyle(FIRST_LETTER))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasPseudoFirstLetter, reasons, includeReasons);
    if (style.hasTextCombine())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextCombine, reasons, includeReasons);
    if (style.backgroundClip() == TextFillBox)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextFillBox, reasons, includeReasons);
    if (style.borderFit() == BorderFitLines)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasBorderFitLines, reasons, includeReasons);
    if (style.lineBreak() != LineBreakAuto)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasNonAutoLineBreak, reasons, includeReasons);
#if ENABLE(CSS_TRAILING_WORD)
    if (style.trailingWord() != TrailingWord::Auto)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasNonAutoTrailingWord, reasons, includeReasons);
#endif
    return reasons;
}

static AvoidanceReasonFlags canUseForWithReason(const RenderBlockFlow& flow, IncludeReasons includeReasons)
{
#ifndef NDEBUG
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        registerNotifyCallback("com.apple.WebKit.showSimpleLineLayoutCoverage", printSimpleLineLayoutCoverage);
        registerNotifyCallback("com.apple.WebKit.showSimpleLineLayoutReasons", printSimpleLineLayoutBlockList);
        registerNotifyCallback("com.apple.WebKit.toggleSimpleLineLayout", toggleSimpleLineLayout);
    });
#endif
    AvoidanceReasonFlags reasons = { };
    if (!flow.frame().settings().simpleLineLayoutEnabled())
        SET_REASON_AND_RETURN_IF_NEEDED(FeatureIsDisabled, reasons, includeReasons);
    if (!flow.parent())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasNoParent, reasons, includeReasons);
    if (!flow.firstChild())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasNoChild, reasons, includeReasons);
    if (flow.flowThreadState() != RenderObject::NotInsideFlowThread)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsInsideRegion, reasons, includeReasons);
    if (!flow.isHorizontalWritingMode())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasHorizonalWritingMode, reasons, includeReasons);
    if (flow.hasOutline())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasOutline, reasons, includeReasons);
    if (flow.isRubyText() || flow.isRubyBase())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsRuby, reasons, includeReasons);
    if (flow.style().hangingPunctuation() != NoHangingPunctuation)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasHangingPunctuation, reasons, includeReasons);
    
    // Printing does pagination without a flow thread.
    if (flow.document().paginated())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsPaginated, reasons, includeReasons);
    if (flow.firstLineBlock())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasPseudoFirstLine, reasons, includeReasons);
    if (flow.isAnonymousBlock() && flow.parent()->style().textOverflow())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextOverflow, reasons, includeReasons);
    if (flow.parent()->isDeprecatedFlexibleBox())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsDepricatedFlexBox, reasons, includeReasons);
    // FIXME: Placeholders do something strange.
    if (is<RenderTextControl>(*flow.parent()) && downcast<RenderTextControl>(*flow.parent()).textFormControlElement().placeholderElement())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowParentIsPlaceholderElement, reasons, includeReasons);
    // FIXME: Implementation of wrap=hard looks into lineboxes.
    if (flow.parent()->isTextArea() && flow.parent()->element()->fastHasAttribute(HTMLNames::wrapAttr))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowParentIsTextAreaWithWrapping, reasons, includeReasons);
    // This currently covers <blockflow>#text</blockflow>, <blockflow>#text<br></blockflow> and mutiple (sibling) RenderText cases.
    // The <blockflow><inline>#text</inline></blockflow> case is also popular and should be relatively easy to cover.
    for (const auto* child = flow.firstChild(); child;) {
        if (child->selectionState() != RenderObject::SelectionNone)
            SET_REASON_AND_RETURN_IF_NEEDED(FlowChildIsSelected, reasons, includeReasons);
        if (is<RenderText>(*child)) {
            child = child->nextSibling();
            continue;
        }
        if (is<RenderLineBreak>(child) && !downcast<RenderLineBreak>(*child).isWBR() && child->style().clear() == CNONE) {
            child = child->nextSibling();
            continue;
        }
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasNonSupportedChild, reasons, includeReasons);
        break;
    }
    auto styleReasons = canUseForStyle(flow.style(), includeReasons);
    if (styleReasons != NoReason)
        SET_REASON_AND_RETURN_IF_NEEDED(styleReasons, reasons, includeReasons);
    // We can't use the code path if any lines would need to be shifted below floats. This is because we don't keep per-line y coordinates.
    if (flow.containsFloats()) {
        float minimumWidthNeeded = std::numeric_limits<float>::max();
        for (const auto& textRenderer : childrenOfType<RenderText>(flow)) {
            minimumWidthNeeded = std::min(minimumWidthNeeded, textRenderer.minLogicalWidth());

            for (auto& floatingObject : *flow.floatingObjectSet()) {
                ASSERT(floatingObject);
#if ENABLE(CSS_SHAPES)
                // if a float has a shape, we cannot tell if content will need to be shifted until after we lay it out,
                // since the amount of space is not uniform for the height of the float.
                if (floatingObject->renderer().shapeOutsideInfo())
                    SET_REASON_AND_RETURN_IF_NEEDED(FlowHasUnsupportedFloat, reasons, includeReasons);
#endif
                float availableWidth = flow.availableLogicalWidthForLine(floatingObject->y(), DoNotIndentText);
                if (availableWidth < minimumWidthNeeded)
                    SET_REASON_AND_RETURN_IF_NEEDED(FlowHasUnsupportedFloat, reasons, includeReasons);
            }
        }
    }
    auto fontAndTextReasons = canUseForFontAndText(flow, includeReasons);
    if (fontAndTextReasons != NoReason)
        SET_REASON_AND_RETURN_IF_NEEDED(fontAndTextReasons, reasons, includeReasons);
    return reasons;
}

bool canUseFor(const RenderBlockFlow& flow)
{
    return canUseForWithReason(flow, IncludeReasons::First) == NoReason;
}

static float computeLineLeft(ETextAlign textAlign, float availableWidth, float committedWidth, float logicalLeftOffset)
{
    float remainingWidth = availableWidth - committedWidth;
    float left = logicalLeftOffset;
    switch (textAlign) {
    case LEFT:
    case WEBKIT_LEFT:
    case TASTART:
        return left;
    case RIGHT:
    case WEBKIT_RIGHT:
    case TAEND:
        return left + std::max<float>(remainingWidth, 0);
    case CENTER:
    case WEBKIT_CENTER:
        return left + std::max<float>(remainingWidth / 2, 0);
    case JUSTIFY:
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static void revertRuns(Layout::RunVector& runs, unsigned length, float width)
{
    while (length) {
        ASSERT(runs.size());
        Run& lastRun = runs.last();
        unsigned lastRunLength = lastRun.end - lastRun.start;
        if (lastRunLength > length) {
            lastRun.logicalRight -= width;
            lastRun.end -= length;
            break;
        }
        length -= lastRunLength;
        width -= (lastRun.logicalRight - lastRun.logicalLeft);
        runs.removeLast();
    }
}

class LineState {
public:
    void setAvailableWidth(float width) { m_availableWidth = width; }
    void setCollapedWhitespaceWidth(float width) { m_collapsedWhitespaceWidth = width; }
    void setLogicalLeftOffset(float offset) { m_logicalLeftOffset = offset; }
    void setOverflowedFragment(const TextFragmentIterator::TextFragment& fragment) { m_overflowedFragment = fragment; }

    float availableWidth() const { return m_availableWidth; }
    float logicalLeftOffset() const { return m_logicalLeftOffset; }
    const TextFragmentIterator::TextFragment& overflowedFragment() const { return m_overflowedFragment; }
    bool hasTrailingWhitespace() const { return m_trailingWhitespaceLength; }
    TextFragmentIterator::TextFragment lastFragment() const { return m_fragments.last(); }
    bool isWhitespaceOnly() const { return m_trailingWhitespaceWidth && m_runsWidth == m_trailingWhitespaceWidth; }
    bool fits(float extra) const { return m_availableWidth >= m_runsWidth + extra; }
    bool firstCharacterFits() const { return m_firstCharacterFits; }
    float width() const { return m_runsWidth; }
    std::pair<unsigned, bool> expansionOpportunityCount(unsigned from, unsigned to) const
    {
        // linebreak runs are special.
        if (from == to)
            return std::make_pair(0, false);
        unsigned expansionOpportunityCount = 0;
        auto previousFragmentType = TextFragmentIterator::TextFragment::ContentEnd;
        for (const auto& fragment : m_fragments) {
            if (fragment.end() <= from)
                continue;
            auto currentFragmentType = fragment.type();
            auto expansionOpportunity = this->expansionOpportunity(currentFragmentType, previousFragmentType);
            if (expansionOpportunity)
                ++expansionOpportunityCount;
            previousFragmentType = currentFragmentType;
            if (fragment.end() >= to)
                return std::make_pair(expansionOpportunityCount, expansionOpportunity);
        }
        ASSERT_NOT_REACHED();
        return std::make_pair(expansionOpportunityCount, false);
    }

    bool isEmpty() const
    {
        if (!m_fragments.size())
            return true;
        if (!m_lastCompleteFragment.isEmpty())
            return false;
        return m_fragments.last().overlapsToNextRenderer();
    }

    void appendFragmentAndCreateRunIfNeeded(const TextFragmentIterator::TextFragment& fragment, Layout::RunVector& runs)
    {
        // Adjust end position while collapsing.
        unsigned endPosition = fragment.isCollapsed() ? fragment.start() + 1 : fragment.end();
        // New line needs new run.
        if (!m_runsWidth)
            runs.append(Run(fragment.start(), endPosition, m_runsWidth, m_runsWidth + fragment.width(), false));
        else {
            const auto& lastFragment = m_fragments.last();
            // Advance last completed fragment when the previous fragment is all set (including multiple parts across renderers)
            if ((lastFragment.type() != fragment.type()) || !lastFragment.overlapsToNextRenderer())
                m_lastCompleteFragment = lastFragment;
            // Collapse neighbouring whitespace, if they are across multiple renderers and are not collapsed yet.
            if (lastFragment.isCollapsible() && fragment.isCollapsible()) {
                ASSERT(lastFragment.isLastInRenderer());
                if (!lastFragment.isCollapsed()) {
                    // Line width needs to be reset so that now it takes collapsing into consideration.
                    m_runsWidth -= (lastFragment.width() - m_collapsedWhitespaceWidth);
                }
                // This fragment is collapsed completely. No run is needed.
                return;
            }
            if (lastFragment.isLastInRenderer() || lastFragment.isCollapsed())
                runs.append(Run(fragment.start(), endPosition, m_runsWidth, m_runsWidth + fragment.width(), false));
            else {
                Run& lastRun = runs.last();
                lastRun.end = endPosition;
                lastRun.logicalRight += fragment.width();
            }
        }
        m_fragments.append(fragment);
        m_runsWidth += fragment.width();

        if (fragment.type() == TextFragmentIterator::TextFragment::Whitespace) {
            m_trailingWhitespaceLength += endPosition - fragment.start();
            m_trailingWhitespaceWidth += fragment.width();
        } else {
            m_trailingWhitespaceLength = 0;
            m_trailingWhitespaceWidth = 0;
        }

        if (!m_firstCharacterFits)
            m_firstCharacterFits = fragment.start() + 1 > endPosition || m_runsWidth <= m_availableWidth;
    }

    TextFragmentIterator::TextFragment revertToLastCompleteFragment(Layout::RunVector& runs)
    {
        ASSERT(m_fragments.size());
        unsigned revertLength = 0;
        float revertWidth = 0;
        while (m_fragments.size()) {
            const auto& current = m_fragments.last();
            if (current == m_lastCompleteFragment)
                break;
            revertLength += current.end() - current.start();
            revertWidth += current.width();
            m_fragments.removeLast();
        }
        m_runsWidth -= revertWidth;
        if (revertLength)
            revertRuns(runs, revertLength, revertWidth);
        return m_lastCompleteFragment;
    }

    void removeTrailingWhitespace(Layout::RunVector& runs)
    {
        if (!m_trailingWhitespaceLength)
            return;
        revertRuns(runs, m_trailingWhitespaceLength, m_trailingWhitespaceWidth);
        m_runsWidth -= m_trailingWhitespaceWidth;
        ASSERT(m_fragments.last().type() == TextFragmentIterator::TextFragment::Whitespace);
        while (m_fragments.size()) {
            const auto& current = m_fragments.last();
            if (current.type() != TextFragmentIterator::TextFragment::Whitespace)
                break;
#if !ASSERT_DISABLED
            m_trailingWhitespaceLength -= (current.isCollapsed() ? 1 : current.end() - current.start());
            m_trailingWhitespaceWidth -= current.width();
#endif
            m_fragments.removeLast();
        }
#if !ASSERT_DISABLED
        ASSERT(!m_trailingWhitespaceLength);
        ASSERT(!m_trailingWhitespaceWidth);
#endif
        m_trailingWhitespaceLength = 0;
        m_trailingWhitespaceWidth = 0;
    }

private:
    bool expansionOpportunity(TextFragmentIterator::TextFragment::Type currentFragmentType, TextFragmentIterator::TextFragment::Type previousFragmentType) const
    {
        return (currentFragmentType == TextFragmentIterator::TextFragment::Whitespace
            || (currentFragmentType == TextFragmentIterator::TextFragment::NonWhitespace && previousFragmentType == TextFragmentIterator::TextFragment::NonWhitespace));
    }

    float m_availableWidth { 0 };
    float m_logicalLeftOffset { 0 };
    TextFragmentIterator::TextFragment m_overflowedFragment;
    float m_runsWidth { 0 };
    TextFragmentIterator::TextFragment m_lastCompleteFragment;
    float m_trailingWhitespaceWidth { 0 }; // Use this to remove trailing whitespace without re-mesuring the text.
    unsigned m_trailingWhitespaceLength { 0 };
    float m_collapsedWhitespaceWidth { 0 };
    // Having one character on the line does not necessarily mean it actually fits.
    // First character of the first fragment might be forced on to the current line even if it does not fit.
    bool m_firstCharacterFits { false };
    Vector<TextFragmentIterator::TextFragment> m_fragments;
};

class FragmentForwardIterator : public std::iterator<std::forward_iterator_tag, unsigned> {
public:
    FragmentForwardIterator(unsigned fragmentIndex)
        : m_fragmentIndex(fragmentIndex)
    {
    }

    FragmentForwardIterator& operator++()
    {
        ++m_fragmentIndex;
        return *this;
    }

    bool operator!=(const FragmentForwardIterator& other) const { return m_fragmentIndex != other.m_fragmentIndex; }
    unsigned operator*() const { return m_fragmentIndex; }

private:
    unsigned m_fragmentIndex { 0 };
};

static FragmentForwardIterator begin(const TextFragmentIterator::TextFragment& fragment)  { return FragmentForwardIterator(fragment.start()); }
static FragmentForwardIterator end(const TextFragmentIterator::TextFragment& fragment)  { return FragmentForwardIterator(fragment.end()); }

static bool preWrap(const TextFragmentIterator::Style& style)
{
    return style.wrapLines && !style.collapseWhitespace;
}
    
static void removeTrailingWhitespace(LineState& lineState, Layout::RunVector& runs, const TextFragmentIterator& textFragmentIterator)
{
    if (!lineState.hasTrailingWhitespace())
        return;

    // Remove collapsed whitespace, or non-collapsed pre-wrap whitespace, unless it's the only content on the line -so removing the whitesapce would produce an empty line.
    const auto& style = textFragmentIterator.style();
    bool collapseWhitespace = style.collapseWhitespace | preWrap(style);
    if (!collapseWhitespace)
        return;

    if (preWrap(style) && lineState.isWhitespaceOnly())
        return;

    lineState.removeTrailingWhitespace(runs);
}

static void updateLineConstrains(const RenderBlockFlow& flow, LineState& line, bool isFirstLine)
{
    bool shouldApplyTextIndent = !flow.isAnonymous() || flow.parent()->firstChild() == &flow;
    LayoutUnit height = flow.logicalHeight();
    LayoutUnit logicalHeight = flow.minLineHeightForReplacedRenderer(false, 0);
    float logicalRightOffset = flow.logicalRightOffsetForLine(height, DoNotIndentText, logicalHeight);
    line.setLogicalLeftOffset(flow.logicalLeftOffsetForLine(height, DoNotIndentText, logicalHeight) +
        (shouldApplyTextIndent && isFirstLine ? flow.textIndentOffset() : LayoutUnit(0)));
    line.setAvailableWidth(std::max<float>(0, logicalRightOffset - line.logicalLeftOffset()));
}

static TextFragmentIterator::TextFragment splitFragmentToFitLine(TextFragmentIterator::TextFragment& fragmentToSplit, float availableWidth, bool keepAtLeastOneCharacter, const TextFragmentIterator& textFragmentIterator)
{
    // FIXME: add surrogate pair support.
    unsigned start = fragmentToSplit.start();
    auto it = std::upper_bound(begin(fragmentToSplit), end(fragmentToSplit), availableWidth, [&textFragmentIterator, start](float availableWidth, unsigned index) {
        // FIXME: use the actual left position of the line (instead of 0) to calculated width. It might give false width for tab characters.
        return availableWidth < textFragmentIterator.textWidth(start, index + 1, 0);
    });
    unsigned splitPosition = (*it);
    if (keepAtLeastOneCharacter && splitPosition == fragmentToSplit.start())
        ++splitPosition;
    return fragmentToSplit.split(splitPosition, textFragmentIterator);
}

enum PreWrapLineBreakRule { Preserve, Ignore };

static TextFragmentIterator::TextFragment consumeLineBreakIfNeeded(const TextFragmentIterator::TextFragment& fragment, TextFragmentIterator& textFragmentIterator, LineState& line, Layout::RunVector& runs,
    PreWrapLineBreakRule preWrapLineBreakRule = PreWrapLineBreakRule::Preserve)
{
    if (!fragment.isLineBreak())
        return fragment;

    if (preWrap(textFragmentIterator.style()) && preWrapLineBreakRule != PreWrapLineBreakRule::Ignore)
        return fragment;

    // <br> always produces a run. (required by testing output)
    if (fragment.type() == TextFragmentIterator::TextFragment::HardLineBreak)
        line.appendFragmentAndCreateRunIfNeeded(fragment, runs);
    return textFragmentIterator.nextTextFragment();
}

static TextFragmentIterator::TextFragment skipWhitespaceIfNeeded(const TextFragmentIterator::TextFragment& fragment, TextFragmentIterator& textFragmentIterator)
{
    if (!textFragmentIterator.style().collapseWhitespace)
        return fragment;

    TextFragmentIterator::TextFragment firstNonWhitespaceFragment = fragment;
    while (firstNonWhitespaceFragment.type() == TextFragmentIterator::TextFragment::Whitespace)
        firstNonWhitespaceFragment = textFragmentIterator.nextTextFragment();
    return firstNonWhitespaceFragment;
}

static TextFragmentIterator::TextFragment firstFragment(TextFragmentIterator& textFragmentIterator, LineState& currentLine, const LineState& previousLine, Layout::RunVector& runs)
{
    // Handle overflowed fragment from previous line.
    TextFragmentIterator::TextFragment firstFragment(previousLine.overflowedFragment());

    if (firstFragment.isEmpty())
        firstFragment = textFragmentIterator.nextTextFragment();
    else if (firstFragment.type() == TextFragmentIterator::TextFragment::Whitespace && preWrap(textFragmentIterator.style()) && previousLine.firstCharacterFits()) {
        // Special overflow pre-wrap whitespace handling: skip the overflowed whitespace (even when style says not-collapsible) if we managed to fit at least one character on the previous line.
        firstFragment = textFragmentIterator.nextTextFragment();
        // If skipping the whitespace puts us on a newline, skip the newline too as we already wrapped the line.
        firstFragment = consumeLineBreakIfNeeded(firstFragment, textFragmentIterator, currentLine, runs, PreWrapLineBreakRule::Ignore);
    }
    return skipWhitespaceIfNeeded(firstFragment, textFragmentIterator);
}

static void forceFragmentToLine(LineState& line, TextFragmentIterator& textFragmentIterator, Layout::RunVector& runs, const TextFragmentIterator::TextFragment& fragment)
{
    line.appendFragmentAndCreateRunIfNeeded(fragment, runs);
    // Check if there are more fragments to add to the current line.
    auto nextFragment = textFragmentIterator.nextTextFragment();
    if (fragment.overlapsToNextRenderer()) {
        while (true) {
            if (nextFragment.type() != fragment.type())
                break;
            line.appendFragmentAndCreateRunIfNeeded(nextFragment, runs);
            // Does it overlap to the next segment?
            if (!nextFragment.overlapsToNextRenderer())
                return;
            nextFragment = textFragmentIterator.nextTextFragment();
        }
    }
    // When the forced fragment is followed by either whitespace and/or line break, consume them too, otherwise we end up with an extra whitespace and/or line break.
    nextFragment = skipWhitespaceIfNeeded(nextFragment, textFragmentIterator);
    nextFragment = consumeLineBreakIfNeeded(nextFragment, textFragmentIterator, line, runs);
    line.setOverflowedFragment(nextFragment);
}

static bool createLineRuns(LineState& line, const LineState& previousLine, Layout::RunVector& runs, TextFragmentIterator& textFragmentIterator)
{
    const auto& style = textFragmentIterator.style();
    line.setCollapedWhitespaceWidth(style.spaceWidth + style.wordSpacing);
    bool lineCanBeWrapped = style.wrapLines || style.breakFirstWordOnOverflow || style.breakAnyWordOnOverflow;
    auto fragment = firstFragment(textFragmentIterator, line, previousLine, runs);
    while (fragment.type() != TextFragmentIterator::TextFragment::ContentEnd) {
        // Hard linebreak.
        if (fragment.isLineBreak()) {
            // Add the new line fragment only if there's nothing on the line. (otherwise the extra new line character would show up at the end of the content.)
            if (line.isEmpty() || fragment.type() == TextFragmentIterator::TextFragment::HardLineBreak) {
                if (style.textAlign == RIGHT || style.textAlign == WEBKIT_RIGHT)
                    line.removeTrailingWhitespace(runs);
                line.appendFragmentAndCreateRunIfNeeded(fragment, runs);
            }
            break;
        }
        if (lineCanBeWrapped && !line.fits(fragment.width())) {
            // Overflow wrapping behaviour:
            // 1. Whitesapce collapse on: whitespace is skipped. Jump to next line.
            // 2. Whitespace collapse off: whitespace is wrapped.
            // 3. First, non-whitespace fragment is either wrapped or kept on the line. (depends on overflow-wrap)
            // 5. Non-whitespace fragment when there's already another fragment on the line either gets wrapped (word-break: break-all)
            // or gets pushed to the next line.
            bool emptyLine = line.isEmpty();
            // Whitespace fragment.
            if (fragment.type() == TextFragmentIterator::TextFragment::Whitespace) {
                if (!style.collapseWhitespace) {
                    // Split the fragment; (modified)fragment stays on this line, overflowedFragment is pushed to next line.
                    line.setOverflowedFragment(splitFragmentToFitLine(fragment, line.availableWidth() - line.width(), emptyLine, textFragmentIterator));
                    line.appendFragmentAndCreateRunIfNeeded(fragment, runs);
                }
                // When whitespace collapse is on, whitespace that doesn't fit is simply skipped.
                break;
            }
            // Non-whitespace fragment. (!style.wrapLines: bug138102(preserve existing behavior)
            if (((emptyLine && style.breakFirstWordOnOverflow) || style.breakAnyWordOnOverflow) || !style.wrapLines) {
                // Split the fragment; (modified)fragment stays on this line, overflowedFragment is pushed to next line.
                line.setOverflowedFragment(splitFragmentToFitLine(fragment, line.availableWidth() - line.width(), emptyLine, textFragmentIterator));
                line.appendFragmentAndCreateRunIfNeeded(fragment, runs);
                break;
            }
            // Non-breakable non-whitespace first fragment. Add it to the current line. -it overflows though.
            ASSERT(fragment.type() == TextFragmentIterator::TextFragment::NonWhitespace);
            if (emptyLine) {
                forceFragmentToLine(line, textFragmentIterator, runs, fragment);
                break;
            }
            // Non-breakable non-whitespace fragment when there's already content on the line. Push it to the next line.
            if (line.lastFragment().overlapsToNextRenderer()) {
                // Check if this fragment is a continuation of a previous segment. In such cases, we need to remove them all.
                const auto& lastCompleteFragment = line.revertToLastCompleteFragment(runs);
                textFragmentIterator.revertToEndOfFragment(lastCompleteFragment);
                break;
            }
            line.setOverflowedFragment(fragment);
            break;
        }
        line.appendFragmentAndCreateRunIfNeeded(fragment, runs);
        // Find the next text fragment.
        fragment = textFragmentIterator.nextTextFragment(line.width());
    }
    return (fragment.type() == TextFragmentIterator::TextFragment::ContentEnd && line.overflowedFragment().isEmpty()) || line.overflowedFragment().type() == TextFragmentIterator::TextFragment::ContentEnd;
}

static ExpansionBehavior expansionBehavior(bool isAfterExpansion, bool lastRunOnLine)
{
    ExpansionBehavior expansionBehavior;
    expansionBehavior = isAfterExpansion ? ForbidLeadingExpansion : AllowLeadingExpansion;
    expansionBehavior |= lastRunOnLine ? ForbidTrailingExpansion : AllowTrailingExpansion;
    return expansionBehavior;
}

static void justifyRuns(const LineState& line, Layout::RunVector& runs, Optional<unsigned> lastRunIndexOfPreviousLine)
{
    ASSERT(runs.size());
    auto widthToDistribute = line.availableWidth() - line.width();
    if (widthToDistribute <= 0)
        return;

    auto firstRunIndex = lastRunIndexOfPreviousLine ? lastRunIndexOfPreviousLine.value() + 1 : 0;
    auto lastRunIndex = runs.size() - 1;
    ASSERT(firstRunIndex <= lastRunIndex);
    Vector<std::pair<unsigned, ExpansionBehavior>> expansionOpportunityList;
    unsigned expansionOpportunityCountOnThisLine = 0;
    auto isAfterExpansion = true;
    for (auto i = firstRunIndex; i <= lastRunIndex; ++i) {
        const auto& run = runs.at(i);
        unsigned opportunityCountInRun = 0;
        std::tie(opportunityCountInRun, isAfterExpansion) = line.expansionOpportunityCount(run.start, run.end);
        expansionOpportunityList.append(std::make_pair(opportunityCountInRun, expansionBehavior(isAfterExpansion, i == lastRunIndex)));
        expansionOpportunityCountOnThisLine += opportunityCountInRun;
    }
    if (!expansionOpportunityCountOnThisLine)
        return;

    ASSERT(expansionOpportunityList.size() == lastRunIndex - firstRunIndex + 1);
    auto expansion = widthToDistribute / expansionOpportunityCountOnThisLine;
    float accumulatedExpansion = 0;
    for (auto i = firstRunIndex; i <= lastRunIndex; ++i) {
        auto& run = runs.at(i);
        unsigned opportunityCountInRun;
        std::tie(opportunityCountInRun, run.expansionBehavior) = expansionOpportunityList.at(i - firstRunIndex);
        run.expansion = opportunityCountInRun * expansion;
        run.logicalLeft += accumulatedExpansion;
        run.logicalRight += (accumulatedExpansion + run.expansion);
        accumulatedExpansion += run.expansion;
    }
}

static void closeLineEndingAndAdjustRuns(LineState& line, Layout::RunVector& runs, Optional<unsigned> lastRunIndexOfPreviousLine, unsigned& lineCount,
    const TextFragmentIterator& textFragmentIterator, bool lastLine)
{
    if (!runs.size() || (lastRunIndexOfPreviousLine && runs.size() - 1 == lastRunIndexOfPreviousLine.value()))
        return;
    removeTrailingWhitespace(line, runs, textFragmentIterator);
    if (!runs.size())
        return;
    // Adjust runs' position by taking line's alignment into account.
    const auto& style = textFragmentIterator.style();
    auto textAlign = style.textAlign;
    // Fallback to LEFT alignment both for non-collapsable content and for the last line.
    if (textAlign == JUSTIFY && (!style.collapseWhitespace || lastLine))
        textAlign = LEFT;

    auto lineLogicalLeft = line.logicalLeftOffset();
    if (textAlign == JUSTIFY)
        justifyRuns(line, runs, lastRunIndexOfPreviousLine);
    else
        lineLogicalLeft = computeLineLeft(textAlign, line.availableWidth(), line.width(), line.logicalLeftOffset());
    auto firstRunIndex = lastRunIndexOfPreviousLine ? lastRunIndexOfPreviousLine.value() + 1 : 0;
    for (auto i = firstRunIndex; i < runs.size(); ++i) {
        runs[i].logicalLeft += lineLogicalLeft;
        runs[i].logicalRight += lineLogicalLeft;
    }
    runs.last().isEndOfLine = true;
    ++lineCount;
}

static void createTextRuns(Layout::RunVector& runs, RenderBlockFlow& flow, unsigned& lineCount)
{
    LayoutUnit borderAndPaddingBefore = flow.borderAndPaddingBefore();
    LayoutUnit lineHeight = lineHeightFromFlow(flow);
    LineState line;
    bool isEndOfContent = false;
    TextFragmentIterator textFragmentIterator = TextFragmentIterator(flow);
    Optional<unsigned> lastRunIndexOfPreviousLine;
    do {
        flow.setLogicalHeight(lineHeight * lineCount + borderAndPaddingBefore);
        LineState previousLine = line;
        line = LineState();
        updateLineConstrains(flow, line, !lineCount);
        isEndOfContent = createLineRuns(line, previousLine, runs, textFragmentIterator);
        closeLineEndingAndAdjustRuns(line, runs, lastRunIndexOfPreviousLine, lineCount, textFragmentIterator, isEndOfContent);
        if (runs.size())
            lastRunIndexOfPreviousLine = runs.size() - 1;
    } while (!isEndOfContent);
}

std::unique_ptr<Layout> create(RenderBlockFlow& flow)
{
    unsigned lineCount = 0;
    Layout::RunVector runs;

    createTextRuns(runs, flow, lineCount);
    for (auto& renderer : childrenOfType<RenderObject>(flow)) {
        ASSERT(is<RenderText>(renderer) || is<RenderLineBreak>(renderer));
        renderer.clearNeedsLayout();
    }
    return Layout::create(runs, lineCount);
}

std::unique_ptr<Layout> Layout::create(const RunVector& runVector, unsigned lineCount)
{
    void* slot = WTF::fastMalloc(sizeof(Layout) + sizeof(Run) * runVector.size());
    return std::unique_ptr<Layout>(new (NotNull, slot) Layout(runVector, lineCount));
}

Layout::Layout(const RunVector& runVector, unsigned lineCount)
    : m_lineCount(lineCount)
    , m_runCount(runVector.size())
{
    memcpy(m_runs, runVector.data(), m_runCount * sizeof(Run));
}

#ifndef NDEBUG
static void printReason(AvoidanceReason reason, TextStream& stream)
{
    switch (reason) {
    case FlowIsInsideRegion:
        stream << "flow is inside region";
        break;
    case FlowHasHorizonalWritingMode:
        stream << "horizontal writing mode";
        break;
    case FlowHasOutline:
        stream << "outline";
        break;
    case FlowIsRuby:
        stream << "ruby";
        break;
    case FlowHasHangingPunctuation:
        stream << "hanging punctuation";
        break;
    case FlowIsPaginated:
        stream << "paginated";
        break;
    case FlowHasTextOverflow:
        stream << "text-overflow";
        break;
    case FlowIsDepricatedFlexBox:
        stream << "depricatedFlexBox";
        break;
    case FlowParentIsPlaceholderElement:
        stream << "placeholder element";
        break;
    case FlowParentIsTextAreaWithWrapping:
        stream << "wrapping textarea";
        break;
    case FlowHasNonSupportedChild:
        stream << "nested renderers";
        break;
    case FlowHasUnsupportedFloat:
        stream << "complicated float";
        break;
    case FlowHasUnsupportedUnderlineDecoration:
        stream << "text-underline-position: under";
        break;
    case FlowHasJustifiedNonLatinText:
        stream << "text-align: justify with non-latin text";
        break;
    case FlowHasOverflowVisible:
        stream << "overflow: visible";
        break;
    case FlowIsNotLTR:
        stream << "dir is not LTR";
        break;
    case FlowHasLineBoxContainProperty:
        stream << "line-box-contain property";
        break;
    case FlowIsNotTopToBottom:
        stream << "non top-to-bottom flow";
        break;
    case FlowHasLineBreak:
        stream << "line-break property";
        break;
    case FlowHasNonNormalUnicodeBiDi:
        stream << "non-normal Unicode bidi";
        break;
    case FlowHasRTLOrdering:
        stream << "-webkit-rtl-ordering";
        break;
    case FlowHasLineAlignEdges:
        stream << "-webkit-line-align edges";
        break;
    case FlowHasLineSnap:
        stream << "-webkit-line-snap property";
        break;
    case FlowHasHypensAuto:
        stream << "hyphen: auto";
        break;
    case FlowHasTextEmphasisFillOrMark:
        stream << "text-emphasis (fill/mark)";
        break;
    case FlowHasPseudoFirstLine:
        stream << "first-line";
        break;
    case FlowHasPseudoFirstLetter:
        stream << "first-letter";
        break;
    case FlowHasTextCombine:
        stream << "text combine";
        break;
    case FlowHasTextFillBox:
        stream << "background-color (text-fill)";
        break;
    case FlowHasBorderFitLines:
        stream << "-webkit-border-fit";
        break;
    case FlowHasNonAutoLineBreak:
        stream << "line-break is not auto";
        break;
    case FlowHasNonAutoTrailingWord:
        stream << "-apple-trailing-word is not auto";
        break;
    case FlowHasSVGFont:
        stream << "SVG font";
        break;
    case FlowTextHasNoBreakSpace:
        stream << "No-break-space character";
        break;
    case FlowTextHasSoftHyphen:
        stream << "soft hyphen character";
        break;
    case FlowTextHasDirectionCharacter:
        stream << "direction character";
        break;
    case FlowIsMissingPrimaryFont:
        stream << "missing primary font";
        break;
    case FlowFontIsMissingGlyph:
        stream << "missing glyph";
        break;
    case FlowTextIsCombineText:
        stream << "text is combine";
        break;
    case FlowTextIsRenderCounter:
        stream << "unsupported RenderCounter";
        break;
    case FlowTextIsRenderQuote:
        stream << "unsupported RenderQuote";
        break;
    case FlowTextIsTextFragment:
        stream << "unsupported TextFragment";
        break;
    case FlowTextIsSVGInlineText:
        stream << "unsupported SVGInlineText";
        break;
    case FlowFontIsNotSimple:
        stream << "complext font";
        break;
    case FlowHasTextShadow:
        stream << "text-shadow";
        break;
    case FlowChildIsSelected:
        stream << "selected content";
        break;
    case FlowTextIsEmpty:
    case FlowHasNoChild:
    case FlowHasNoParent:
    case FeatureIsDisabled:
    default:
        break;
    }
}

static void printReasons(AvoidanceReasonFlags reasons, TextStream& stream)
{
    bool first = true;
    for (auto reasonItem = EndOfReasons >> 1; reasonItem != NoReason; reasonItem >>= 1) {
        if (!(reasons & reasonItem))
            continue;
        stream << (first ? " " : ", ");
        first = false;
        printReason(reasonItem, stream);
    }
}

static void printTextForSubtree(const RenderObject& renderer, unsigned& charactersLeft, TextStream& stream)
{
    if (!charactersLeft)
        return;
    if (is<RenderText>(renderer)) {
        String text = downcast<RenderText>(renderer).text();
        text = text.stripWhiteSpace();
        unsigned len = std::min(charactersLeft, text.length());
        stream << text.left(len);
        charactersLeft -= len;
        return;
    }
    if (!is<RenderElement>(renderer))
        return;
    for (const auto* child = downcast<RenderElement>(renderer).firstChild(); child; child = child->nextSibling())
        printTextForSubtree(*child, charactersLeft, stream);
}

static unsigned textLengthForSubtree(const RenderObject& renderer)
{
    if (is<RenderText>(renderer))
        return downcast<RenderText>(renderer).textLength();
    if (!is<RenderElement>(renderer))
        return 0;
    unsigned textLength = 0;
    for (const auto* child = downcast<RenderElement>(renderer).firstChild(); child; child = child->nextSibling())
        textLength += textLengthForSubtree(*child);
    return textLength;
}

static void collectNonEmptyLeafRenderBlockFlows(const RenderObject& renderer, HashSet<const RenderBlockFlow*>& leafRenderers)
{
    if (is<RenderText>(renderer)) {
        if (!downcast<RenderText>(renderer).textLength())
            return;
        // Find RenderBlockFlow ancestor.
        for (const auto* current = renderer.parent(); current; current = current->parent()) {
            if (!is<RenderBlockFlow>(current))
                continue;
            leafRenderers.add(downcast<RenderBlockFlow>(current));
            break;
        }
        return;
    }
    if (!is<RenderElement>(renderer))
        return;
    for (const auto* child = downcast<RenderElement>(renderer).firstChild(); child; child = child->nextSibling())
        collectNonEmptyLeafRenderBlockFlows(*child, leafRenderers);
}

static void collectNonEmptyLeafRenderBlockFlowsForCurrentPage(HashSet<const RenderBlockFlow*>& leafRenderers)
{
    for (const auto* document : Document::allDocuments()) {
        if (!document->renderView() || document->inPageCache())
            continue;
        if (!document->isHTMLDocument() && !document->isXHTMLDocument())
            continue;
        collectNonEmptyLeafRenderBlockFlows(*document->renderView(), leafRenderers);
    }
}

void toggleSimpleLineLayout()
{
    for (const auto* document : Document::allDocuments()) {
        auto* settings = document->settings();
        if (!settings)
            continue;
        settings->setSimpleLineLayoutEnabled(!settings->simpleLineLayoutEnabled());
    }
}

void printSimpleLineLayoutBlockList()
{
    HashSet<const RenderBlockFlow*> leafRenderers;
    collectNonEmptyLeafRenderBlockFlowsForCurrentPage(leafRenderers);
    if (!leafRenderers.size()) {
        WTFLogAlways("No text found in this document\n");
        return;
    }
    TextStream stream;
    stream << "---------------------------------------------------\n";
    for (const auto* flow : leafRenderers) {
        auto reason = canUseForWithReason(*flow, IncludeReasons::All);
        if (reason == NoReason)
            continue;
        unsigned printedLength = 30;
        stream << "\"";
        printTextForSubtree(*flow, printedLength, stream);
        for (;printedLength > 0; --printedLength)
            stream << " ";
        stream << "\"(" << textLengthForSubtree(*flow) << "):";
        printReasons(reason, stream);
        stream << "\n";
    }
    stream << "---------------------------------------------------\n";
    WTFLogAlways("%s", stream.release().utf8().data());
}

void printSimpleLineLayoutCoverage()
{
    HashSet<const RenderBlockFlow*> leafRenderers;
    collectNonEmptyLeafRenderBlockFlowsForCurrentPage(leafRenderers);
    if (!leafRenderers.size()) {
        WTFLogAlways("No text found in this document\n");
        return;
    }
    TextStream stream;
    HashMap<AvoidanceReason, unsigned> flowStatistics;
    unsigned textLength = 0;
    unsigned unsupportedTextLength = 0;
    unsigned numberOfUnsupportedLeafBlocks = 0;
    for (const auto* flow : leafRenderers) {
        auto flowLength = textLengthForSubtree(*flow);
        textLength += flowLength;
        auto reasons = canUseForWithReason(*flow, IncludeReasons::All);
        if (reasons == NoReason)
            continue;
        ++numberOfUnsupportedLeafBlocks;
        unsupportedTextLength += flowLength;
        for (auto reasonItem = EndOfReasons >> 1; reasonItem != NoReason; reasonItem >>= 1) {
            if (!(reasons & reasonItem))
                continue;
            auto result = flowStatistics.add(reasonItem, flowLength);
            if (!result.isNewEntry)
                result.iterator->value += flowLength;
        }
    }
    stream << "---------------------------------------------------\n";
    stream << "Number of text blocks: total(" <<  leafRenderers.size() << ") non-simple(" << numberOfUnsupportedLeafBlocks << ")\nText length: total(" <<
        textLength << ") non-simple(" << unsupportedTextLength << ")\n";
    for (const auto reasonEntry : flowStatistics) {
        printReason(reasonEntry.key, stream);
        stream << ": " << (float)reasonEntry.value / (float)textLength * 100 << "%\n";
    }
    stream << "simple line layout coverage: " << (float)(textLength - unsupportedTextLength) / (float)textLength * 100 << "%\n";
    stream << "---------------------------------------------------\n";
    WTFLogAlways("%s", stream.release().utf8().data());
}
#endif
}
}
