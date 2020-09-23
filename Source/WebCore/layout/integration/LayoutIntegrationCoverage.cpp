/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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
#include "LayoutIntegrationCoverage.h"

#include "DocumentMarkerController.h"
#include "HTMLTextFormControlElement.h"
#include "Logging.h"
#include "RenderBlockFlow.h"
#include "RenderMultiColumnFlow.h"
#include "RenderTextControl.h"
#include "RenderView.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#ifndef NDEBUG
#define SET_REASON_AND_RETURN_IF_NEEDED(reason, reasons, includeReasons) { \
        reasons.add(AvoidanceReason::reason); \
        if (includeReasons == IncludeReasons::First) \
            return reasons; \
    }
#else
#define SET_REASON_AND_RETURN_IF_NEEDED(reason, reasons, includeReasons) { \
        ASSERT_UNUSED(includeReasons, includeReasons == IncludeReasons::First); \
        reasons.add(AvoidanceReason::reason); \
        return reasons; \
    }
#endif

#ifndef NDEBUG
#define ADD_REASONS_AND_RETURN_IF_NEEDED(newReasons, reasons, includeReasons) { \
        reasons.add(newReasons); \
        if (includeReasons == IncludeReasons::First) \
            return reasons; \
    }
#else
#define ADD_REASONS_AND_RETURN_IF_NEEDED(newReasons, reasons, includeReasons) { \
        ASSERT_UNUSED(includeReasons, includeReasons == IncludeReasons::First); \
        reasons.add(newReasons); \
        return reasons; \
    }
#endif

namespace WebCore {
namespace LayoutIntegration {

template <typename CharacterType> OptionSet<AvoidanceReason> canUseForCharacter(CharacterType, bool textIsJustified, IncludeReasons);

template<> OptionSet<AvoidanceReason> canUseForCharacter(UChar character, bool textIsJustified, IncludeReasons includeReasons)
{
    OptionSet<AvoidanceReason> reasons;
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

template<> OptionSet<AvoidanceReason> canUseForCharacter(LChar, bool, IncludeReasons)
{
    return { };
}

template <typename CharacterType>
static OptionSet<AvoidanceReason> canUseForText(const CharacterType* text, unsigned length, const FontCascade& fontCascade, Optional<float> lineHeightConstraint,
    bool textIsJustified, IncludeReasons includeReasons)
{
    OptionSet<AvoidanceReason> reasons;
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
        if (characterReasons)
            ADD_REASONS_AND_RETURN_IF_NEEDED(characterReasons, reasons, includeReasons);

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

static OptionSet<AvoidanceReason> canUseForText(StringView text, const FontCascade& fontCascade, Optional<float> lineHeightConstraint, bool textIsJustified, IncludeReasons includeReasons)
{
    if (text.is8Bit())
        return canUseForText(text.characters8(), text.length(), fontCascade, lineHeightConstraint, textIsJustified, includeReasons);
    return canUseForText(text.characters16(), text.length(), fontCascade, lineHeightConstraint, textIsJustified, includeReasons);
}

static OptionSet<AvoidanceReason> canUseForFontAndText(const RenderBlockFlow& flow, IncludeReasons includeReasons)
{
    OptionSet<AvoidanceReason> reasons;
    // We assume that all lines have metrics based purely on the primary font.
    const auto& style = flow.style();
    auto& fontCascade = style.fontCascade();
    if (fontCascade.primaryFont().isInterstitial())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsMissingPrimaryFont, reasons, includeReasons);
    Optional<float> lineHeightConstraint;
    if (style.lineBoxContain().contains(LineBoxContain::Glyphs))
        lineHeightConstraint = flow.lineHeight(false, HorizontalLine, PositionOfInteriorLineBoxes).toFloat();
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
        if (textReasons)
            ADD_REASONS_AND_RETURN_IF_NEEDED(textReasons, reasons, includeReasons);
    }
    return reasons;
}

static OptionSet<AvoidanceReason> canUseForStyle(const RenderStyle& style, IncludeReasons includeReasons)
{
    OptionSet<AvoidanceReason> reasons;
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
    if (style.writingMode() != WritingMode::TopToBottom)
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
        if (textReasons)
            ADD_REASONS_AND_RETURN_IF_NEEDED(textReasons, reasons, includeReasons);
    }
    return reasons;
}

OptionSet<AvoidanceReason> canUseForLineLayoutWithReason(const RenderBlockFlow& flow, IncludeReasons includeReasons)
{
    OptionSet<AvoidanceReason> reasons;
    // FIXME: For tests that disable SLL and expect to get CLL.
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
    if (styleReasons)
        ADD_REASONS_AND_RETURN_IF_NEEDED(styleReasons, reasons, includeReasons);
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
    if (fontAndTextReasons)
        ADD_REASONS_AND_RETURN_IF_NEEDED(fontAndTextReasons, reasons, includeReasons);
    return reasons;
}

bool canUseForLineLayout(const RenderBlockFlow& flow)
{
    return canUseForLineLayoutWithReason(flow, IncludeReasons::First).isEmpty();
}

bool canUseForLineLayoutAfterStyleChange(const RenderBlockFlow& blockContainer, StyleDifference diff)
{
    switch (diff) {
    case StyleDifference::Equal:
    case StyleDifference::RecompositeLayer:
        return true;
    case StyleDifference::Repaint:
    case StyleDifference::RepaintIfTextOrBorderOrOutline:
    case StyleDifference::RepaintLayer:
        // FIXME: We could do a more focused style check by matching RendererStyle::changeRequiresRepaint&co.
        return canUseForStyle(blockContainer.style(), IncludeReasons::First).isEmpty();
    case StyleDifference::LayoutPositionedMovementOnly:
        return true;
    case StyleDifference::SimplifiedLayout:
    case StyleDifference::SimplifiedLayoutAndPositionedMovement:
        return canUseForStyle(blockContainer.style(), IncludeReasons::First).isEmpty();
    case StyleDifference::Layout:
    case StyleDifference::NewStyle:
        return canUseForLineLayout(blockContainer);
    }
    ASSERT_NOT_REACHED();
    return canUseForLineLayout(blockContainer);
}

}
}

#endif
