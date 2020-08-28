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

#include "DocumentMarkerController.h"
#include "FontCache.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HTMLTextFormControlElement.h"
#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "Hyphenation.h"
#include "InlineTextBox.h"
#include "LineWidth.h"
#include "Logging.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderFragmentedFlow.h"
#include "RenderLineBreak.h"
#include "RenderMultiColumnFlow.h"
#include "RenderStyle.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderView.h"
#include "Settings.h"
#include "SimpleLineLayoutFlowContents.h"
#include "SimpleLineLayoutFunctions.h"
#include "SimpleLineLayoutResolver.h"
#include "SimpleLineLayoutTextFragmentIterator.h"
#include "Text.h"
#include "TextPaintStyle.h"
#include <pal/Logging.h>

namespace WebCore {
namespace SimpleLineLayout {

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


template <typename CharacterType> AvoidanceReasonFlags canUseForCharacter(CharacterType, bool textIsJustified, IncludeReasons);

template<> AvoidanceReasonFlags canUseForCharacter(UChar character, bool textIsJustified, IncludeReasons includeReasons)
{
    AvoidanceReasonFlags reasons = { };
    if (textIsJustified) {
        // Include characters up to Latin Extended-B and some punctuation range when text is justified.
        bool isLatinIncludingExtendedB = character <= 0x01FF;
        bool isPunctuationRange = character >= 0x2010 && character <= 0x2027;
        if (!(isLatinIncludingExtendedB || isPunctuationRange))
            SET_REASON_AND_RETURN_IF_NEEDED(FlowHasJustifiedNonLatinText, reasons, includeReasons);
    }

    if (U16_IS_SURROGATE(character))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowTextHasSurrogatePair, reasons, includeReasons);
    
    UCharDirection direction = u_charDirection(character);
    if (direction == U_RIGHT_TO_LEFT || direction == U_RIGHT_TO_LEFT_ARABIC
        || direction == U_RIGHT_TO_LEFT_EMBEDDING || direction == U_RIGHT_TO_LEFT_OVERRIDE
        || direction == U_LEFT_TO_RIGHT_EMBEDDING || direction == U_LEFT_TO_RIGHT_OVERRIDE
        || direction == U_POP_DIRECTIONAL_FORMAT || direction == U_BOUNDARY_NEUTRAL)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowTextHasDirectionCharacter, reasons, includeReasons);

    return reasons;
}

template<> AvoidanceReasonFlags canUseForCharacter(LChar, bool, IncludeReasons)
{
    return { };
}

template <typename CharacterType>
static AvoidanceReasonFlags canUseForText(const CharacterType* text, unsigned length, const FontCascade& fontCascade, Optional<float> lineHeightConstraint,
    bool textIsJustified, IncludeReasons includeReasons)
{
    AvoidanceReasonFlags reasons = { };
    auto& primaryFont = fontCascade.primaryFont();
    auto& fontMetrics = primaryFont.fontMetrics();
    auto availableSpaceForGlyphAscent = fontMetrics.ascent();
    auto availableSpaceForGlyphDescent = fontMetrics.descent();
    if (lineHeightConstraint) {
        auto lineHeightPadding = *lineHeightConstraint - fontMetrics.height();
        availableSpaceForGlyphAscent += lineHeightPadding / 2;
        availableSpaceForGlyphDescent += lineHeightPadding / 2;
    }

    for (unsigned i = 0; i < length; ++i) {
        auto character = text[i];
        if (FontCascade::treatAsSpace(character))
            continue;

        if (character == softHyphen)
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextHasSoftHyphen, reasons, includeReasons);

        auto characterReasons = canUseForCharacter(character, textIsJustified, includeReasons);
        if (characterReasons != NoReason)
            SET_REASON_AND_RETURN_IF_NEEDED(characterReasons, reasons, includeReasons);

        auto glyphData = fontCascade.glyphDataForCharacter(character, false);
        if (!glyphData.isValid() || glyphData.font != &primaryFont)
            SET_REASON_AND_RETURN_IF_NEEDED(FlowPrimaryFontIsInsufficient, reasons, includeReasons);

        if (lineHeightConstraint) {
            auto bounds = primaryFont.boundsForGlyph(glyphData.glyph);
            if (ceilf(-bounds.y()) > availableSpaceForGlyphAscent || ceilf(bounds.maxY()) > availableSpaceForGlyphDescent)
                SET_REASON_AND_RETURN_IF_NEEDED(FlowFontHasOverflowGlyph, reasons, includeReasons);
        }
    }
    return reasons;
}

static AvoidanceReasonFlags canUseForText(StringView text, const FontCascade& fontCascade, Optional<float> lineHeightConstraint, bool textIsJustified, IncludeReasons includeReasons)
{
    if (text.is8Bit())
        return canUseForText(text.characters8(), text.length(), fontCascade, lineHeightConstraint, textIsJustified, includeReasons);
    return canUseForText(text.characters16(), text.length(), fontCascade, lineHeightConstraint, textIsJustified, includeReasons);
}

static AvoidanceReasonFlags canUseForFontAndText(const RenderBlockFlow& flow, IncludeReasons includeReasons)
{
    AvoidanceReasonFlags reasons = { };
    // We assume that all lines have metrics based purely on the primary font.
    const auto& style = flow.style();
    auto& fontCascade = style.fontCascade();
    if (fontCascade.primaryFont().isInterstitial())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsMissingPrimaryFont, reasons, includeReasons);
    Optional<float> lineHeightConstraint;
    if (style.lineBoxContain().contains(LineBoxContain::Glyphs))
        lineHeightConstraint = lineHeightFromFlow(flow).toFloat();
    bool flowIsJustified = style.textAlign() == TextAlignMode::Justify;
    for (const auto& textRenderer : childrenOfType<RenderText>(flow)) {
        // FIXME: Do not return until after checking all children.
        if (textRenderer.text().isEmpty())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextIsEmpty, reasons, includeReasons);
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
        if (!textRenderer.canUseSimpleFontCodePath()) {
            // No need to check the code path at this point. We already know it can't be simple.
            SET_REASON_AND_RETURN_IF_NEEDED(FlowHasComplexFontCodePath, reasons, includeReasons);
        } else {
            TextRun run(String(textRenderer.text()));
            run.setCharacterScanForCodePath(false);
            if (style.fontCascade().codePath(run) != FontCascade::Simple)
                SET_REASON_AND_RETURN_IF_NEEDED(FlowHasComplexFontCodePath, reasons, includeReasons);
        }

        auto textReasons = canUseForText(textRenderer.stringView(), fontCascade, lineHeightConstraint, flowIsJustified, includeReasons);
        if (textReasons != NoReason)
            SET_REASON_AND_RETURN_IF_NEEDED(textReasons, reasons, includeReasons);
    }
    return reasons;
}

static AvoidanceReasonFlags canUseForStyle(const RenderStyle& style, IncludeReasons includeReasons)
{
    AvoidanceReasonFlags reasons = { };
    if (style.textOverflow() == TextOverflow::Ellipsis)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextOverflow, reasons, includeReasons);
    if (style.textUnderlinePosition() != TextUnderlinePosition::Auto || !style.textUnderlineOffset().isAuto() || !style.textDecorationThickness().isAuto())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasUnsupportedUnderlineDecoration, reasons, includeReasons);
    // Non-visible overflow should be pretty easy to support.
    if (style.overflowX() != Overflow::Visible || style.overflowY() != Overflow::Visible)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasOverflowNotVisible, reasons, includeReasons);
    if (!style.isLeftToRightDirection())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsNotLTR, reasons, includeReasons);
    if (!(style.lineBoxContain().contains(LineBoxContain::Block)))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineBoxContainProperty, reasons, includeReasons);
    if (style.writingMode() != TopToBottomWritingMode)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsNotTopToBottom, reasons, includeReasons);
    if (style.lineBreak() != LineBreak::Auto)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineBreak, reasons, includeReasons);
    if (style.unicodeBidi() != UBNormal)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasNonNormalUnicodeBiDi, reasons, includeReasons);
    if (style.rtlOrdering() != Order::Logical)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasRTLOrdering, reasons, includeReasons);
    if (style.lineAlign() != LineAlign::None)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineAlignEdges, reasons, includeReasons);
    if (style.lineSnap() != LineSnap::None)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineSnap, reasons, includeReasons);
    if (style.textEmphasisFill() != TextEmphasisFill::Filled || style.textEmphasisMark() != TextEmphasisMark::None)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextEmphasisFillOrMark, reasons, includeReasons);
    if (style.textShadow())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextShadow, reasons, includeReasons);
    if (style.hasPseudoStyle(PseudoId::FirstLine))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasPseudoFirstLine, reasons, includeReasons);
    if (style.hasPseudoStyle(PseudoId::FirstLetter))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasPseudoFirstLetter, reasons, includeReasons);
    if (style.hasTextCombine())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextCombine, reasons, includeReasons);
    if (style.backgroundClip() == FillBox::Text)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextFillBox, reasons, includeReasons);
    if (style.borderFit() == BorderFit::Lines)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasBorderFitLines, reasons, includeReasons);
    if (style.lineBreak() != LineBreak::Auto)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasNonAutoLineBreak, reasons, includeReasons);
    if (style.nbspMode() != NBSPMode::Normal)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasWebKitNBSPMode, reasons, includeReasons);
    // Special handling of text-security:disc is not yet implemented in the simple line layout code path.
    // See RenderBlock::updateSecurityDiscCharacters.
    if (style.textSecurity() != TextSecurity::None)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextSecurity, reasons, includeReasons);
    if (style.hyphens() == Hyphens::Auto) {
        auto textReasons = canUseForText(style.hyphenString(), style.fontCascade(), WTF::nullopt, false, includeReasons);
        if (textReasons != NoReason)
            SET_REASON_AND_RETURN_IF_NEEDED(textReasons, reasons, includeReasons);
    }
    return reasons;
}

AvoidanceReasonFlags canUseForWithReason(const RenderBlockFlow& flow, IncludeReasons includeReasons)
{
#ifndef NDEBUG
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PAL::registerNotifyCallback("com.apple.WebKit.showSimpleLineLayoutCoverage", WTF::Function<void()> { printSimpleLineLayoutCoverage });
        PAL::registerNotifyCallback("com.apple.WebKit.showSimpleLineLayoutReasons", WTF::Function<void()> { printSimpleLineLayoutBlockList });
        PAL::registerNotifyCallback("com.apple.WebKit.toggleSimpleLineLayout", WTF::Function<void()> { toggleSimpleLineLayout });
    });
#endif
    AvoidanceReasonFlags reasons = { };
    if (!flow.settings().simpleLineLayoutEnabled())
        SET_REASON_AND_RETURN_IF_NEEDED(FeatureIsDisabled, reasons, includeReasons);
    if (!flow.parent())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasNoParent, reasons, includeReasons);
    if (!flow.firstChild())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasNoChild, reasons, includeReasons);
    if (flow.fragmentedFlowState() != RenderObject::NotInsideFragmentedFlow) {
        auto* fragmentedFlow = flow.enclosingFragmentedFlow();
        if (!is<RenderMultiColumnFlow>(fragmentedFlow))
            SET_REASON_AND_RETURN_IF_NEEDED(FlowIsInsideANonMultiColumnThread, reasons, includeReasons);
        auto& columnThread = downcast<RenderMultiColumnFlow>(*fragmentedFlow);
        if (columnThread.parent() != &flow.view())
            SET_REASON_AND_RETURN_IF_NEEDED(MultiColumnFlowIsNotTopLevel, reasons, includeReasons);
        if (columnThread.hasColumnSpanner())
            SET_REASON_AND_RETURN_IF_NEEDED(MultiColumnFlowHasColumnSpanner, reasons, includeReasons);
        auto& style = flow.style();
        if (style.verticalAlign() != VerticalAlign::Baseline)
            SET_REASON_AND_RETURN_IF_NEEDED(MultiColumnFlowVerticalAlign, reasons, includeReasons);
        if (style.isFloating())
            SET_REASON_AND_RETURN_IF_NEEDED(MultiColumnFlowIsFloating, reasons, includeReasons);
    }
    if (!flow.isHorizontalWritingMode())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasHorizonalWritingMode, reasons, includeReasons);
    if (flow.hasOutline())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasOutline, reasons, includeReasons);
    if (flow.isRubyText() || flow.isRubyBase())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsRuby, reasons, includeReasons);
    if (!flow.style().hangingPunctuation().isEmpty())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasHangingPunctuation, reasons, includeReasons);
    
    // Printing does pagination without a flow thread.
    if (flow.document().paginated())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsPaginated, reasons, includeReasons);
    if (flow.firstLineBlock())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasPseudoFirstLine, reasons, includeReasons);
    if (flow.isAnonymousBlock() && flow.parent()->style().textOverflow() == TextOverflow::Ellipsis)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextOverflow, reasons, includeReasons);
    if (flow.parent()->isDeprecatedFlexibleBox())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsDepricatedFlexBox, reasons, includeReasons);
    // FIXME: Placeholders do something strange.
    if (is<RenderTextControl>(*flow.parent()) && downcast<RenderTextControl>(*flow.parent()).textFormControlElement().placeholderElement())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowParentIsPlaceholderElement, reasons, includeReasons);
    // FIXME: Implementation of wrap=hard looks into lineboxes.
    if (flow.parent()->isTextArea() && flow.parent()->element()->hasAttributeWithoutSynchronization(HTMLNames::wrapAttr))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowParentIsTextAreaWithWrapping, reasons, includeReasons);
    // This currently covers <blockflow>#text</blockflow>, <blockflow>#text<br></blockflow> and mutiple (sibling) RenderText cases.
    // The <blockflow><inline>#text</inline></blockflow> case is also popular and should be relatively easy to cover.
    for (const auto* child = flow.firstChild(); child;) {
        if (child->selectionState() != RenderObject::HighlightState::None)
            SET_REASON_AND_RETURN_IF_NEEDED(FlowChildIsSelected, reasons, includeReasons);
        if (is<RenderText>(*child)) {
            const auto& renderText = downcast<RenderText>(*child);
            if (renderText.textNode() && !renderText.document().markers().markersFor(*renderText.textNode()).isEmpty())
                SET_REASON_AND_RETURN_IF_NEEDED(FlowIncludesDocumentMarkers, reasons, includeReasons);
            child = child->nextSibling();
            continue;
        }
        if (is<RenderLineBreak>(child) && !downcast<RenderLineBreak>(*child).isWBR() && child->style().clear() == Clear::None) {
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
                // if a float has a shape, we cannot tell if content will need to be shifted until after we lay it out,
                // since the amount of space is not uniform for the height of the float.
                if (floatingObject->renderer().shapeOutsideInfo())
                    SET_REASON_AND_RETURN_IF_NEEDED(FlowHasUnsupportedFloat, reasons, includeReasons);
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

bool canUseForAfterStyleChange(const RenderBlockFlow& blockContainer, StyleDifference diff)
{
    switch (diff) {
    case StyleDifference::Equal:
    case StyleDifference::RecompositeLayer:
        return true;
    case StyleDifference::Repaint:
    case StyleDifference::RepaintIfTextOrBorderOrOutline:
    case StyleDifference::RepaintLayer:
        // FIXME: We could do a more focused style check by matching RendererStyle::changeRequiresRepaint&co.
        return canUseForStyle(blockContainer.style(), IncludeReasons::First) == NoReason;
    case StyleDifference::LayoutPositionedMovementOnly:
        return true;
    case StyleDifference::SimplifiedLayout:
    case StyleDifference::SimplifiedLayoutAndPositionedMovement:
        return canUseForStyle(blockContainer.style(), IncludeReasons::First) == NoReason;
    case StyleDifference::Layout:
    case StyleDifference::NewStyle:
        return canUseFor(blockContainer);
    }
    ASSERT_NOT_REACHED();
    return canUseFor(blockContainer);
}

static void revertAllRunsOnCurrentLine(Layout::RunVector& runs)
{
    while (!runs.isEmpty() && !runs.last().isEndOfLine)
        runs.removeLast();
}

static void revertRuns(Layout::RunVector& runs, unsigned positionToRevertTo, float width)
{
    while (runs.size()) {
        auto& lastRun = runs.last();
        if (lastRun.end <= positionToRevertTo)
            break;
        if (lastRun.start >= positionToRevertTo) {
            // Revert this run completely.
            width -= (lastRun.logicalRight - lastRun.logicalLeft);
            runs.removeLast();
        } else {
            lastRun.logicalRight -= width;
            width = 0;
            lastRun.end = positionToRevertTo;
            // Partial removal.
            break;
        }
    }
}

class LineState {
public:
    void setAvailableWidth(float width) { m_availableWidth = width; }
    void setCollapedWhitespaceWidth(float width) { m_collapsedWhitespaceWidth = width; }
    void setLogicalLeftOffset(float offset) { m_logicalLeftOffset = offset; }
    void setOverflowedFragment(const TextFragmentIterator::TextFragment& fragment) { m_overflowedFragment = fragment; }
    void setNeedsAllFragments()
    {
        ASSERT(!m_fragments);
        m_fragments.emplace();
    }
    void setHyphenationDisabled() { m_hyphenationDisabled = true; }
    bool isHyphenationDisabled() const { return m_hyphenationDisabled; }

    float availableWidth() const { return m_availableWidth; }
    float logicalLeftOffset() const { return m_logicalLeftOffset; }
    const TextFragmentIterator::TextFragment& overflowedFragment() const { return m_overflowedFragment; }
    bool hasTrailingWhitespace() const { return m_lastFragment.type() == TextFragmentIterator::TextFragment::Whitespace && m_lastFragment.length() > 0; }
    bool hasWhitespaceFragments() const { return m_lastWhitespaceFragment != WTF::nullopt; }
    TextFragmentIterator::TextFragment lastFragment() const { return m_lastFragment; }
    bool isWhitespaceOnly() const { return m_trailingWhitespaceWidth && m_runsWidth == m_trailingWhitespaceWidth; }
    bool fits(float extra) const { return m_availableWidth >= m_runsWidth + extra; }
    bool firstCharacterFits() const { return m_firstCharacterFits; }
    float width() const { return m_runsWidth; }
    std::pair<unsigned, bool> expansionOpportunityCount(unsigned from, unsigned to) const
    {
        ASSERT(m_fragments);
        // linebreak runs are special.
        if (from == to)
            return std::make_pair(0, false);
        unsigned expansionOpportunityCount = 0;
        auto previousFragmentType = TextFragmentIterator::TextFragment::ContentEnd;
        for (const auto& fragment : *m_fragments) {
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
        if (!m_lastFragment.isValid())
            return true;
        if (!m_lastCompleteFragment.isEmpty())
            return false;
        return m_lastFragment.overlapsToNextRenderer();
    }

    static inline unsigned endPositionForCollapsedFragment(const TextFragmentIterator::TextFragment& fragment)
    {
        return fragment.isCollapsed() ? fragment.start() + 1 : fragment.end();
    }

    void appendFragmentAndCreateRunIfNeeded(const TextFragmentIterator::TextFragment& fragment, Layout::RunVector& runs)
    {
        // Adjust end position while collapsing.
        unsigned endPosition = endPositionForCollapsedFragment(fragment);
        // New line needs new run.
        if (!m_runsWidth) {
            ASSERT(!m_uncompletedWidth);
            runs.append(Run(fragment.start(), endPosition, m_runsWidth, m_runsWidth + fragment.width(), false, fragment.hasHyphen(), fragment.isLineBreak()));
        } else {
            // Advance last completed fragment when the previous fragment is all set (including multiple parts across renderers)
            if ((m_lastFragment.type() != fragment.type()) || !m_lastFragment.overlapsToNextRenderer()) {
                m_lastCompleteFragment = m_lastFragment;
                m_uncompletedWidth = fragment.width();
            } else
                m_uncompletedWidth += fragment.width();
            // Collapse neighbouring whitespace, if they are across multiple renderers and are not collapsed yet.
            if (m_lastFragment.isCollapsible() && fragment.isCollapsible()) {
                ASSERT(m_lastFragment.isLastInRenderer());
                if (!m_lastFragment.isCollapsed()) {
                    // Line width needs to be adjusted so that now it takes collapsing into consideration.
                    m_runsWidth -= (m_lastFragment.width() - m_collapsedWhitespaceWidth);
                }
                // This fragment is collapsed completely. No run is needed.
                return;
            }
            Run& lastRun = runs.last();
            if (m_lastFragment.isLastInRenderer() || m_lastFragment.isCollapsed() || fragment.isLineBreak() || lastRun.isLineBreak)
                runs.append(Run(fragment.start(), endPosition, m_runsWidth, m_runsWidth + fragment.width(), false, fragment.hasHyphen(), fragment.isLineBreak()));
            else {
                lastRun.end = endPosition;
                lastRun.logicalRight += fragment.width();
                ASSERT(!lastRun.hasHyphen);
                lastRun.hasHyphen = fragment.hasHyphen();
            }
        }
        m_runsWidth += fragment.width();
        m_lastFragment = fragment;
        if (m_fragments)
            (*m_fragments).append(fragment);

        if (fragment.type() == TextFragmentIterator::TextFragment::Whitespace) {
            m_trailingWhitespaceWidth += fragment.width();
            m_lastWhitespaceFragment = fragment;
        } else {
            m_trailingWhitespaceWidth = 0;
            m_lastNonWhitespaceFragment = fragment;
        }

        if (!m_firstCharacterFits)
            m_firstCharacterFits = fragment.start() + 1 > endPosition || m_runsWidth <= m_availableWidth;
    }

    TextFragmentIterator::TextFragment revertToLastCompleteFragment(Layout::RunVector& runs)
    {
        if (!m_uncompletedWidth) {
            ASSERT(m_lastFragment == m_lastCompleteFragment);
            return m_lastFragment;
        }
        ASSERT(m_lastFragment.isValid());
        m_runsWidth -= m_uncompletedWidth;
        revertRuns(runs, endPositionForCollapsedFragment(m_lastCompleteFragment), m_uncompletedWidth);
        m_uncompletedWidth = 0;
        ASSERT(m_lastCompleteFragment.isValid());
        return m_lastCompleteFragment;
    }

    void removeTrailingWhitespace(Layout::RunVector& runs)
    {
        if (!hasTrailingWhitespace())
            return;
        if (m_lastNonWhitespaceFragment) {
            auto needsReverting = m_lastNonWhitespaceFragment->end() != m_lastFragment.end();
            // Trailing whitespace fragment might actually have zero length.
            ASSERT(needsReverting || !m_trailingWhitespaceWidth);
            if (needsReverting) {
                revertRuns(runs, m_lastNonWhitespaceFragment->end(), m_trailingWhitespaceWidth);
                m_runsWidth -= m_trailingWhitespaceWidth;
            }
            m_trailingWhitespaceWidth = 0;
            m_lastFragment = *m_lastNonWhitespaceFragment;
            return;
        }
        // This line is all whitespace.
        revertAllRunsOnCurrentLine(runs);
        m_runsWidth = 0;
        m_trailingWhitespaceWidth = 0;
        // FIXME: Make m_lastFragment optional.
        m_lastFragment = TextFragmentIterator::TextFragment();
    }

    float trailingWhitespaceWidth() const { return m_trailingWhitespaceWidth; }

private:
    bool expansionOpportunity(TextFragmentIterator::TextFragment::Type currentFragmentType, TextFragmentIterator::TextFragment::Type previousFragmentType) const
    {
        return (currentFragmentType == TextFragmentIterator::TextFragment::Whitespace
            || (currentFragmentType == TextFragmentIterator::TextFragment::NonWhitespace && previousFragmentType == TextFragmentIterator::TextFragment::NonWhitespace));
    }

    float m_availableWidth { 0 };
    float m_logicalLeftOffset { 0 };
    float m_runsWidth { 0 };
    TextFragmentIterator::TextFragment m_overflowedFragment;
    TextFragmentIterator::TextFragment m_lastFragment;
    Optional<TextFragmentIterator::TextFragment> m_lastNonWhitespaceFragment;
    Optional<TextFragmentIterator::TextFragment> m_lastWhitespaceFragment;
    TextFragmentIterator::TextFragment m_lastCompleteFragment;
    float m_uncompletedWidth { 0 };
    float m_trailingWhitespaceWidth { 0 }; // Use this to remove trailing whitespace without re-mesuring the text.
    float m_collapsedWhitespaceWidth { 0 };
    // Having one character on the line does not necessarily mean it actually fits.
    // First character of the first fragment might be forced on to the current line even if it does not fit.
    bool m_firstCharacterFits { false };
    bool m_hyphenationDisabled { false };
    Optional<Vector<TextFragmentIterator::TextFragment, 30>> m_fragments;
};

static float computeLineLeft(const LineState& line, TextAlignMode textAlign, float& hangingWhitespaceWidth)
{
    float totalWidth = line.width() - hangingWhitespaceWidth;
    float remainingWidth = line.availableWidth() - totalWidth;
    float left = line.logicalLeftOffset();
    switch (textAlign) {
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
    case TextAlignMode::Start:
        hangingWhitespaceWidth = std::max(0.f, std::min(hangingWhitespaceWidth, remainingWidth));
        return left;
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
    case TextAlignMode::End:
        hangingWhitespaceWidth = 0;
        return left + std::max<float>(remainingWidth, 0);
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        hangingWhitespaceWidth = std::max(0.f, std::min(hangingWhitespaceWidth, (remainingWidth + 1) / 2));
        return left + std::max<float>(remainingWidth / 2, 0);
    case TextAlignMode::Justify:
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static bool preWrap(const TextFragmentIterator::Style& style)
{
    return style.wrapLines && !style.collapseWhitespace && !style.breakSpaces;
}

static void updateLineConstrains(const RenderBlockFlow& flow, LineState& line, const LineState& previousLine, unsigned& numberOfPrecedingLinesWithHyphen, const TextFragmentIterator::Style& style, bool isFirstLine)
{
    bool shouldApplyTextIndent = !flow.isAnonymous() || flow.parent()->firstChild() == &flow;
    LayoutUnit height = flow.logicalHeight();
    LayoutUnit logicalHeight = flow.minLineHeightForReplacedRenderer(false, 0);
    line.setLogicalLeftOffset(flow.logicalLeftOffsetForLine(height, DoNotIndentText, logicalHeight) + (shouldApplyTextIndent && isFirstLine ? flow.textIndentOffset() : 0_lu));
    float logicalRightOffset = flow.logicalRightOffsetForLine(height, DoNotIndentText, logicalHeight);
    line.setAvailableWidth(std::max<float>(0, logicalRightOffset - line.logicalLeftOffset()));
    if (style.textAlign == TextAlignMode::Justify)
        line.setNeedsAllFragments();
    numberOfPrecedingLinesWithHyphen = (previousLine.isEmpty() || !previousLine.lastFragment().hasHyphen()) ? 0 : numberOfPrecedingLinesWithHyphen + 1;
    if (style.hyphenLimitLines && numberOfPrecedingLinesWithHyphen >= *style.hyphenLimitLines)
        line.setHyphenationDisabled();
    line.setCollapedWhitespaceWidth(style.font.spaceWidth() + style.wordSpacing);
}

struct SplitFragmentData {
    unsigned position;
    float width;
};
static Optional<unsigned> hyphenPositionForFragment(SplitFragmentData splitData, const TextFragmentIterator::TextFragment& fragmentToSplit,
    const LineState& line, const TextFragmentIterator& textFragmentIterator, float availableWidth)
{
    auto& style = textFragmentIterator.style();
    if (!style.shouldHyphenate || line.isHyphenationDisabled())
        return WTF::nullopt;

    // FIXME: This is a workaround for webkit.org/b/169613. See maxPrefixWidth computation in tryHyphenating().
    // It does not work properly with non-collapsed leading tabs when font is enlarged.
    auto adjustedAvailableWidth = availableWidth - style.hyphenStringWidth;
    if (!line.isEmpty())
        adjustedAvailableWidth += style.font.spaceWidth();
    if (!enoughWidthForHyphenation(adjustedAvailableWidth, style.font.pixelSize()))
        return WTF::nullopt;

    // We might be able to fit the hyphen at the split position.
    auto splitPositionWithHyphen = splitData.position;
    // Find a splitting position where hyphen surely fits.
    unsigned start = fragmentToSplit.start();
    auto leftSideWidth = splitData.width;
    while (leftSideWidth + style.hyphenStringWidth > availableWidth) {
        if (--splitPositionWithHyphen <= start)
            return WTF::nullopt; // No space for hyphen.
        leftSideWidth -= textFragmentIterator.textWidth(splitPositionWithHyphen, splitPositionWithHyphen + 1, 0);
    }
    ASSERT(splitPositionWithHyphen > start);
    return textFragmentIterator.lastHyphenPosition(fragmentToSplit, splitPositionWithHyphen + 1);
}

static SplitFragmentData split(const TextFragmentIterator::TextFragment& fragment, float availableWidth,
    const TextFragmentIterator& textFragmentIterator)
{
    ASSERT(availableWidth >= 0);
    auto left = fragment.start();
    // Pathological case of (extremely)long string and narrow lines.
    // Adjust the range so that we can pick a reasonable midpoint.
    auto averageCharacterWidth = fragment.width() / fragment.length();
    auto right = std::min<unsigned>(left + (2 * availableWidth / averageCharacterWidth), fragment.end() - 1);
    // Preserve the left width for the final split position so that we don't need to remeasure the left side again.
    float leftSideWidth = 0;
    while (left < right) {
        auto middle = (left + right) / 2;
        auto width = textFragmentIterator.textWidth(fragment.start(), middle + 1, 0);
        if (width < availableWidth) {
            left = middle + 1;
            leftSideWidth = width;
        } else if (width > availableWidth)
            right = middle;
        else {
            right = middle + 1;
            leftSideWidth = width;
            break;
        }
    }
    return { right, leftSideWidth };
}

static TextFragmentIterator::TextFragment splitFragmentToFitLine(TextFragmentIterator::TextFragment& fragmentToSplit,
    const LineState& line, const TextFragmentIterator& textFragmentIterator)
{
    auto availableWidth = line.availableWidth() - line.width();
    auto splitFragmentData = split(fragmentToSplit, availableWidth, textFragmentIterator);
    Optional<unsigned> hyphenPosition = WTF::nullopt;
    // Does first character fit this line?
    if (splitFragmentData.position == fragmentToSplit.start()) {
        // Keep at least one character on empty lines.
        if (line.isEmpty())
            splitFragmentData.width = textFragmentIterator.textWidth(fragmentToSplit.start(), ++splitFragmentData.position, 0);
    } else {
        hyphenPosition = hyphenPositionForFragment(splitFragmentData, fragmentToSplit, line, textFragmentIterator, availableWidth);
        if (hyphenPosition) {
            splitFragmentData.position = *hyphenPosition;
            splitFragmentData.width = textFragmentIterator.textWidth(fragmentToSplit.start(), splitFragmentData.position, 0);
        }
    }
    // If the right side surely does not fit the (next)line, we don't need the width to be kerning/ligature adjusted.
    // Part of it gets re-measured as the left side during next split.
    // This saves measuring long chunk of text repeatedly (see pathological case at ::split).
    auto rightSideWidth = fragmentToSplit.width() - splitFragmentData.width;
    if (rightSideWidth < 2 * availableWidth)
        rightSideWidth = textFragmentIterator.textWidth(splitFragmentData.position, fragmentToSplit.end(), 0);
    return hyphenPosition ? fragmentToSplit.splitWithHyphen(splitFragmentData.position, textFragmentIterator.style().hyphenStringWidth,
        splitFragmentData.width, rightSideWidth) : fragmentToSplit.split(splitFragmentData.position, splitFragmentData.width, rightSideWidth);
}

enum PreWrapLineBreakRule { Preserve, Ignore };

static TextFragmentIterator::TextFragment consumeLineBreakIfNeeded(const TextFragmentIterator::TextFragment& fragment, TextFragmentIterator& textFragmentIterator, LineState& line, Layout::RunVector& runs,
    PreWrapLineBreakRule preWrapLineBreakRule = PreWrapLineBreakRule::Preserve)
{
    if (!fragment.isLineBreak())
        return fragment;

    bool isHardLinebreak = fragment.type() == TextFragmentIterator::TextFragment::HardLineBreak;
    // <br> always produces a run. (required by testing output)
    if (isHardLinebreak)
        line.appendFragmentAndCreateRunIfNeeded(fragment, runs);

    auto& style = textFragmentIterator.style();
    if (style.preserveNewline && preWrapLineBreakRule == PreWrapLineBreakRule::Preserve) {
        if (!isHardLinebreak)
            return fragment;
    }
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
    // Handle overflow fragment from previous line.
    auto overflowedFragment = previousLine.overflowedFragment();
    if (overflowedFragment.isEmpty())
        return skipWhitespaceIfNeeded(textFragmentIterator.nextTextFragment(), textFragmentIterator);

    if (overflowedFragment.type() != TextFragmentIterator::TextFragment::Whitespace)
        return overflowedFragment;

    // Leading whitespace handling.
    auto& style = textFragmentIterator.style();
    if (style.breakSpaces) {
        // Leading whitespace created after breaking the previous line.
        // Breaking before the first space after a word is only allowed in combination with break-all or break-word.
        if (style.breakFirstWordOnOverflow || previousLine.hasTrailingWhitespace())
            return overflowedFragment;
    }
    // Special overflow pre-wrap whitespace handling: skip the overflowed whitespace (even when style says not-collapsible)
    // if we manage to fit at least one character on the previous line.
    if ((style.collapseWhitespace || style.wrapLines) && previousLine.firstCharacterFits()) {
        // If skipping the whitespace puts us on a newline, skip the newline too as we already wrapped the line.
        auto firstFragmentCandidate = consumeLineBreakIfNeeded(textFragmentIterator.nextTextFragment(), textFragmentIterator, currentLine, runs,
            preWrap(style) ? PreWrapLineBreakRule::Ignore : PreWrapLineBreakRule::Preserve);
        return skipWhitespaceIfNeeded(firstFragmentCandidate, textFragmentIterator);
    }
    return skipWhitespaceIfNeeded(overflowedFragment, textFragmentIterator);
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
    bool lineCanBeWrapped = style.wrapLines || style.breakFirstWordOnOverflow || style.breakAnyWordOnOverflow;
    auto fragment = firstFragment(textFragmentIterator, line, previousLine, runs);
    while (fragment.type() != TextFragmentIterator::TextFragment::ContentEnd) {
        // Hard and soft linebreaks.
        if (fragment.isLineBreak()) {
            // Add the new line fragment only if there's nothing on the line. (otherwise the extra new line character would show up at the end of the content.)
            if (line.isEmpty() || fragment.type() == TextFragmentIterator::TextFragment::HardLineBreak || preWrap(style) || style.preserveNewline) {
                if (style.textAlign == TextAlignMode::Right || style.textAlign == TextAlignMode::WebKitRight)
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
                if (style.collapseWhitespace) {
                    // Push collapased whitespace to the next line.
                    line.setOverflowedFragment(fragment);
                    break;
                }
                if (style.breakSpaces && line.hasWhitespaceFragments() && fragment.length() == 1) {
                    // Breaking before the first space after a word is not allowed if there are previous breaking opportunities in the line.
                    textFragmentIterator.revertToEndOfFragment(line.revertToLastCompleteFragment(runs));
                    break;
                }
                if (preWrap(style)) {
                    line.appendFragmentAndCreateRunIfNeeded(fragment, runs);
                    fragment = textFragmentIterator.nextTextFragment(line.width());
                    if (fragment.isLineBreak())
                        continue;
                    line.setOverflowedFragment(fragment);
                    break;
                }
                // Split the whitespace; left part stays on this line, right is pushed to next line.
                line.setOverflowedFragment(splitFragmentToFitLine(fragment, line, textFragmentIterator));
                line.appendFragmentAndCreateRunIfNeeded(fragment, runs);
                break;
            }
            // Non-whitespace fragment. (!style.wrapLines: bug138102(preserve existing behavior)
            if (((emptyLine && style.breakFirstWordOnOverflow) || style.breakAnyWordOnOverflow) || !style.wrapLines) {
                // Split the fragment; (modified)fragment stays on this line, overflowedFragment is pushed to next line.
                line.setOverflowedFragment(splitFragmentToFitLine(fragment, line, textFragmentIterator));
                line.appendFragmentAndCreateRunIfNeeded(fragment, runs);
                break;
            }
            ASSERT(fragment.type() == TextFragmentIterator::TextFragment::NonWhitespace);
            // Find out if this non-whitespace fragment has a hyphen where we can break.
            if (style.shouldHyphenate) {
                auto fragmentToSplit = fragment;
                // Split and check if we actually ended up with a hyphen.
                auto overflowFragment = splitFragmentToFitLine(fragmentToSplit, line, textFragmentIterator);
                if (fragmentToSplit.hasHyphen()) {
                    line.setOverflowedFragment(overflowFragment);
                    line.appendFragmentAndCreateRunIfNeeded(fragmentToSplit, runs);
                    break;
                }
                // No hyphen, no split.
            }
            // Non-breakable non-whitespace first fragment. Add it to the current line. -it overflows though.
            if (emptyLine) {
                forceFragmentToLine(line, textFragmentIterator, runs, fragment);
                break;
            }
            // Non-breakable non-whitespace fragment when there's already content on the line. Push it to the next line.
            ASSERT(line.lastFragment().isValid());
            if (line.lastFragment().overlapsToNextRenderer()) {
                // Check if this fragment is a continuation of a previous segment. In such cases, we need to remove them all.
                textFragmentIterator.revertToEndOfFragment(line.revertToLastCompleteFragment(runs));
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
    expansionBehavior = isAfterExpansion ? ForbidLeftExpansion : AllowLeftExpansion;
    expansionBehavior |= lastRunOnLine ? ForbidRightExpansion : AllowRightExpansion;
    return expansionBehavior;
}

static void justifyRuns(const LineState& line, Layout::RunVector& runs, unsigned firstRunIndex)
{
    ASSERT(runs.size());
    auto widthToDistribute = line.availableWidth() - line.width();
    if (widthToDistribute <= 0)
        return;

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

static TextAlignMode textAlignForLine(const TextFragmentIterator::Style& style, bool lastLine)
{
    // Fallback to TextAlignMode::Left (START) alignment for non-collapsable content and for the last line before a forced break or the end of the block.
    auto textAlign = style.textAlign;
    if (textAlign == TextAlignMode::Justify && (!style.collapseWhitespace || lastLine))
        textAlign = TextAlignMode::Left;
    return textAlign;
}

static void closeLineEndingAndAdjustRuns(LineState& line, Layout::RunVector& runs, Optional<unsigned> lastRunIndexOfPreviousLine, unsigned& lineCount,
    const TextFragmentIterator& textFragmentIterator, bool lastLineInFlow)
{
    if (!runs.size() || (lastRunIndexOfPreviousLine && runs.size() - 1 == lastRunIndexOfPreviousLine.value()))
        return;

    const auto& style = textFragmentIterator.style();

    if (style.collapseWhitespace)
        line.removeTrailingWhitespace(runs);

    if (!runs.size())
        return;

    // Adjust runs' position by taking line's alignment into account.
    auto firstRunIndex = lastRunIndexOfPreviousLine ? lastRunIndexOfPreviousLine.value() + 1 : 0;
    auto lineLogicalLeft = line.logicalLeftOffset();
    auto textAlign = textAlignForLine(style, lastLineInFlow || (line.lastFragment().isValid() && line.lastFragment().type() == TextFragmentIterator::TextFragment::HardLineBreak));

    // https://www.w3.org/TR/css-text-3/#white-space-phase-2
    bool shouldHangTrailingWhitespace = style.wrapLines && line.trailingWhitespaceWidth();
    auto hangingWhitespaceWidth = shouldHangTrailingWhitespace ? line.trailingWhitespaceWidth() : 0;

    if (textAlign == TextAlignMode::Justify) {
        justifyRuns(line, runs, firstRunIndex);
        hangingWhitespaceWidth = 0;
    } else
        lineLogicalLeft = computeLineLeft(line, textAlign, hangingWhitespaceWidth);

    for (auto i = firstRunIndex; i < runs.size(); ++i) {
        runs[i].logicalLeft += lineLogicalLeft;
        runs[i].logicalRight += lineLogicalLeft;
    }

    if (shouldHangTrailingWhitespace && hangingWhitespaceWidth < line.trailingWhitespaceWidth())
        runs.last().logicalRight = runs.last().logicalRight - (line.trailingWhitespaceWidth() - hangingWhitespaceWidth);

    runs.last().isEndOfLine = true;
    ++lineCount;
}

static void createTextRuns(Layout::RunVector& runs, RenderBlockFlow& flow, unsigned& lineCount)
{
    LayoutUnit borderAndPaddingBefore = flow.borderAndPaddingBefore();
    LayoutUnit lineHeight = lineHeightFromFlow(flow);
    LineState line;
    unsigned numberOfPrecedingLinesWithHyphen = 0;
    bool isEndOfContent = false;
    TextFragmentIterator textFragmentIterator = TextFragmentIterator(flow);
    Optional<unsigned> lastRunIndexOfPreviousLine;
    do {
        flow.setLogicalHeight(lineHeight * lineCount + borderAndPaddingBefore);
        LineState previousLine = line;
        line = LineState();
        updateLineConstrains(flow, line, previousLine, numberOfPrecedingLinesWithHyphen, textFragmentIterator.style(), !lineCount);
        isEndOfContent = createLineRuns(line, previousLine, runs, textFragmentIterator);
        closeLineEndingAndAdjustRuns(line, runs, lastRunIndexOfPreviousLine, lineCount, textFragmentIterator, isEndOfContent);
        if (runs.size())
            lastRunIndexOfPreviousLine = runs.size() - 1;
    } while (!isEndOfContent);
}

Ref<Layout> create(RenderBlockFlow& flow)
{
    unsigned lineCount = 0;
    Layout::RunVector runs;
    createTextRuns(runs, flow, lineCount);
    return Layout::create(runs, lineCount, flow);
}

Ref<Layout> Layout::create(const RunVector& runVector, unsigned lineCount, const RenderBlockFlow& blockFlow)
{
    void* slot = WTF::fastMalloc(sizeof(Layout) + sizeof(Run) * runVector.size());
    return adoptRef(*new (NotNull, slot) Layout(runVector, lineCount, blockFlow));
}

Layout::Layout(const RunVector& runVector, unsigned lineCount, const RenderBlockFlow& blockFlow)
    : m_lineCount(lineCount)
    , m_runCount(runVector.size())
    , m_blockFlowRenderer(blockFlow)
{
    memcpy(m_runs, runVector.data(), m_runCount * sizeof(Run));
}

const RunResolver& Layout::runResolver() const
{
    if (!m_runResolver)
        m_runResolver = makeUnique<RunResolver>(m_blockFlowRenderer, *this);
    return *m_runResolver;
}

Layout::~Layout()
{
    simpleLineLayoutWillBeDeleted(*this);
}

}
}
