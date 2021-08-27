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
#include "HighlightRegister.h"
#include "InlineIterator.h"
#include "Logging.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderCounter.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderLineBreak.h"
#include "RenderMultiColumnFlow.h"
#include "RenderTextControl.h"
#include "RenderView.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include <pal/Logging.h>
#include <wtf/OptionSet.h>

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#define ALLOW_IMAGES 1
#define ALLOW_ALL_REPLACED 1
#define ALLOW_INLINE_BLOCK 1
#define ALLOW_INLINES 1

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

#ifndef NDEBUG
static void printReason(AvoidanceReason reason, TextStream& stream)
{
    switch (reason) {
    case AvoidanceReason::FlowIsInsideANonMultiColumnThread:
        stream << "flow is inside a non-multicolumn container";
        break;
    case AvoidanceReason::FlowHasHorizonalWritingMode:
        stream << "horizontal writing mode";
        break;
    case AvoidanceReason::ContentHasOutline:
        stream << "outline";
        break;
    case AvoidanceReason::ContentIsRuby:
        stream << "ruby";
        break;
    case AvoidanceReason::FlowHasHangingPunctuation:
        stream << "hanging punctuation";
        break;
    case AvoidanceReason::FlowIsPaginated:
        stream << "paginated";
        break;
    case AvoidanceReason::FlowHasTextOverflow:
        stream << "text-overflow";
        break;
    case AvoidanceReason::FlowHasLineClamp:
        stream << "-webkit-line-clamp";
        break;
    case AvoidanceReason::FlowHasNonSupportedChild:
        stream << "unsupported child renderer";
        break;
    case AvoidanceReason::FlowHasUnsupportedFloat:
        stream << "complicated float";
        break;
    case AvoidanceReason::FlowHasOverflowNotVisible:
        stream << "overflow: hidden | scroll | auto";
        break;
    case AvoidanceReason::FlowIsNotLTR:
        stream << "dir is not LTR";
        break;
    case AvoidanceReason::FlowHasLineBoxContainProperty:
        stream << "line-box-contain value indicates variable line height";
        break;
    case AvoidanceReason::FlowIsNotTopToBottom:
        stream << "non top-to-bottom flow";
        break;
    case AvoidanceReason::FlowHasNonNormalUnicodeBiDi:
        stream << "non-normal Unicode bidi";
        break;
    case AvoidanceReason::FlowHasRTLOrdering:
        stream << "-webkit-rtl-ordering";
        break;
    case AvoidanceReason::FlowHasLineAlignEdges:
        stream << "-webkit-line-align edges";
        break;
    case AvoidanceReason::FlowHasLineSnap:
        stream << "-webkit-line-snap property";
        break;
    case AvoidanceReason::FlowHasTextEmphasisFillOrMark:
        stream << "text-emphasis (fill/mark)";
        break;
    case AvoidanceReason::FlowHasPseudoFirstLine:
        stream << "first-line";
        break;
    case AvoidanceReason::FlowHasPseudoFirstLetter:
        stream << "first-letter";
        break;
    case AvoidanceReason::FlowHasTextCombine:
        stream << "text combine";
        break;
    case AvoidanceReason::FlowHasTextFillBox:
        stream << "background-color (text-fill)";
        break;
    case AvoidanceReason::FlowHasBorderFitLines:
        stream << "-webkit-border-fit";
        break;
    case AvoidanceReason::FlowHasAfterWhiteSpaceLineBreak:
        stream << "line-break is after-white-space";
        break;
    case AvoidanceReason::FlowHasSVGFont:
        stream << "SVG font";
        break;
    case AvoidanceReason::FlowTextHasDirectionCharacter:
        stream << "direction character";
        break;
    case AvoidanceReason::FlowIsMissingPrimaryFont:
        stream << "missing primary font";
        break;
    case AvoidanceReason::FlowPrimaryFontIsInsufficient:
        stream << "missing glyph or glyph needs another font";
        break;
    case AvoidanceReason::FlowTextIsCombineText:
        stream << "text is combine";
        break;
    case AvoidanceReason::FlowTextIsRenderCounter:
        stream << "RenderCounter";
        break;
    case AvoidanceReason::FlowTextIsRenderQuote:
        stream << "RenderQuote";
        break;
    case AvoidanceReason::FlowTextIsTextFragment:
        stream << "TextFragment";
        break;
    case AvoidanceReason::FlowTextIsSVGInlineText:
        stream << "SVGInlineText";
        break;
    case AvoidanceReason::FlowHasComplexFontCodePath:
        stream << "text with complex font codepath";
        break;
    case AvoidanceReason::FlowHasTextShadow:
        stream << "text-shadow";
        break;
    case AvoidanceReason::FlowChildIsSelected:
        stream << "selected content";
        break;
    case AvoidanceReason::FlowHasLineBoxContainGlyphs:
        stream << "-webkit-line-box-contain: glyphs";
        break;
    case AvoidanceReason::FlowTextHasSurrogatePair:
        stream << "surrogate pair";
        break;
    case AvoidanceReason::MultiColumnFlowIsNotTopLevel:
        stream << "non top level column";
        break;
    case AvoidanceReason::MultiColumnFlowHasColumnSpanner:
        stream << "column has spanner";
        break;
    case AvoidanceReason::MultiColumnFlowVerticalAlign:
        stream << "column with vertical-align != baseline";
        break;
    case AvoidanceReason::MultiColumnFlowIsFloating:
        stream << "column with floating objects";
        break;
    case AvoidanceReason::FlowIncludesDocumentMarkers:
        stream << "text includes document markers";
        break;
    case AvoidanceReason::FlowIncludesHighlights:
        stream << "text includes highlights";
        break;
    case AvoidanceReason::FlowHasJustifiedNonBreakingSpace:
        stream << "justified text has non-breaking-space character";
        break;
    case AvoidanceReason::FlowDoesNotEstablishInlineFormattingContext:
        stream << "flow does not establishes inline formatting context";
        break;
    case AvoidanceReason::UnsupportedFieldset:
        stream << "fieldset box";
        break;
    case AvoidanceReason::ChildBoxIsFloatingOrPositioned:
        stream << "child box is floating or positioned";
        break;
    case AvoidanceReason::ContentIsSVG:
        stream << "SVG content";
        break;
    case AvoidanceReason::ChildBoxHasUnsupportedStyle:
        stream << "child box has unsupported style";
        break;
    case AvoidanceReason::UnsupportedImageMap:
        stream << "image map";
        break;
    case AvoidanceReason::InlineBoxNeedsLayer:
        stream << "inline box needs layer";
        break;
    case AvoidanceReason::InlineBoxHasBorderOrBorderImage:
        stream << "inline box has border or border image";
        break;
    case AvoidanceReason::InlineBoxHasBackground:
        stream << "inline box has background";
        break;
    default:
        break;
    }
}

static void printReasons(OptionSet<AvoidanceReason> reasons, TextStream& stream)
{
    stream << " ";
    for (auto reason : reasons) {
        printReason(reason, stream);
        stream << ", ";
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
        return downcast<RenderText>(renderer).text().length();
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
        if (!downcast<RenderText>(renderer).text().length())
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
        if (!document->renderView() || document->backForwardCacheState() != Document::NotInBackForwardCache)
            continue;
        if (!document->isHTMLDocument() && !document->isXHTMLDocument())
            continue;
        collectNonEmptyLeafRenderBlockFlows(*document->renderView(), leafRenderers);
    }
}

static void printModernLineLayoutBlockList(void)
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
        auto reasons = canUseForLineLayoutWithReason(*flow, IncludeReasons::All);
        if (reasons.isEmpty())
            continue;
        unsigned printedLength = 30;
        stream << "\"";
        printTextForSubtree(*flow, printedLength, stream);
        for (;printedLength > 0; --printedLength)
            stream << " ";
        stream << "\"(" << textLengthForSubtree(*flow) << "):";
        printReasons(reasons, stream);
        stream << "\n";
    }
    stream << "---------------------------------------------------\n";
    WTFLogAlways("%s", stream.release().utf8().data());
}

static void printModernLineLayoutCoverage(void)
{
    HashSet<const RenderBlockFlow*> leafRenderers;
    collectNonEmptyLeafRenderBlockFlowsForCurrentPage(leafRenderers);
    if (!leafRenderers.size()) {
        WTFLogAlways("No text found in this document\n");
        return;
    }
    TextStream stream;
    HashMap<AvoidanceReason, unsigned, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> flowStatistics;
    unsigned textLength = 0;
    unsigned unsupportedTextLength = 0;
    unsigned numberOfUnsupportedLeafBlocks = 0;
    unsigned supportedButForcedToLineLayoutTextLength = 0;
    unsigned numberOfSupportedButForcedToLineLayoutLeafBlocks = 0;
    for (const auto* flow : leafRenderers) {
        auto flowLength = textLengthForSubtree(*flow);
        textLength += flowLength;
        auto reasons = canUseForLineLayoutWithReason(*flow, IncludeReasons::All);
        if (reasons.isEmpty()) {
            if (flow->lineLayoutPath() == RenderBlockFlow::ForceLineBoxesPath) {
                supportedButForcedToLineLayoutTextLength += flowLength;
                ++numberOfSupportedButForcedToLineLayoutLeafBlocks;
            }
            continue;
        }
        ++numberOfUnsupportedLeafBlocks;
        unsupportedTextLength += flowLength;
        for (auto reason : reasons) {
            auto result = flowStatistics.add(static_cast<uint64_t>(reason), flowLength);
            if (!result.isNewEntry)
                result.iterator->value += flowLength;
        }
    }
    stream << "---------------------------------------------------\n";
    if (supportedButForcedToLineLayoutTextLength) {
        stream << "Modern line layout potential coverage: " << (float)(textLength - unsupportedTextLength) / (float)textLength * 100 << "%\n\n";
        stream << "Modern line layout actual coverage: " << (float)(textLength - unsupportedTextLength - supportedButForcedToLineLayoutTextLength) / (float)textLength * 100 << "%\nForced line layout blocks: " << numberOfSupportedButForcedToLineLayoutLeafBlocks << " content length: " << supportedButForcedToLineLayoutTextLength << "(" << (float)supportedButForcedToLineLayoutTextLength / (float)textLength * 100 << "%)";
    } else
        stream << "Modern line layout coverage: " << (float)(textLength - unsupportedTextLength) / (float)textLength * 100 << "%";
    stream << "\n\n";
    stream << "Number of blocks: total(" <<  leafRenderers.size() << ") legacy(" << numberOfUnsupportedLeafBlocks << ")\nContent length: total(" <<
        textLength << ") legacy(" << unsupportedTextLength << ")\n";
    for (const auto& reasonEntry : flowStatistics) {
        printReason(static_cast<AvoidanceReason>(reasonEntry.key), stream);
        stream << ": " << (float)reasonEntry.value / (float)textLength * 100 << "%\n";
    }
    stream << "---------------------------------------------------\n";
    WTFLogAlways("%s", stream.release().utf8().data());
}
#endif

template <typename CharacterType> OptionSet<AvoidanceReason> canUseForCharacter(CharacterType, IncludeReasons);

template<> OptionSet<AvoidanceReason> canUseForCharacter(UChar character, IncludeReasons includeReasons)
{
    OptionSet<AvoidanceReason> reasons;
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

template<> OptionSet<AvoidanceReason> canUseForCharacter(LChar, IncludeReasons)
{
    return { };
}

template <typename CharacterType>
static OptionSet<AvoidanceReason> canUseForText(const CharacterType* text, unsigned length, const FontCascade& fontCascade, IncludeReasons includeReasons)
{
    OptionSet<AvoidanceReason> reasons;
    auto& primaryFont = fontCascade.primaryFont();

    // FIXME: Teach FontCascade::widthForSimpleText() how to deal with synthetic bold, so we can use LFC.
    if (primaryFont.syntheticBoldOffset())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowPrimaryFontIsInsufficient, reasons, includeReasons);

    for (unsigned i = 0; i < length; ++i) {
        auto character = text[i];
        auto characterReasons = canUseForCharacter(character, includeReasons);
        if (characterReasons)
            ADD_REASONS_AND_RETURN_IF_NEEDED(characterReasons, reasons, includeReasons);

        auto glyphData = fontCascade.glyphDataForCharacter(character, false);
        if (!glyphData.isValid() || glyphData.font != &primaryFont)
            SET_REASON_AND_RETURN_IF_NEEDED(FlowPrimaryFontIsInsufficient, reasons, includeReasons);
    }
    return reasons;
}

static OptionSet<AvoidanceReason> canUseForText(StringView text, const FontCascade& fontCascade, IncludeReasons includeReasons)
{
    if (text.is8Bit())
        return canUseForText(text.characters8(), text.length(), fontCascade, includeReasons);
    return canUseForText(text.characters16(), text.length(), fontCascade, includeReasons);
}

static OptionSet<AvoidanceReason> canUseForFontAndText(const RenderBoxModelObject& container, IncludeReasons includeReasons)
{
    OptionSet<AvoidanceReason> reasons;
    // We assume that all lines have metrics based purely on the primary font.
    const auto& style = container.style();
    auto& fontCascade = style.fontCascade();
    if (fontCascade.primaryFont().isInterstitial())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsMissingPrimaryFont, reasons, includeReasons);
    if (style.lineBoxContain().contains(LineBoxContain::Glyphs))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineBoxContainGlyphs, reasons, includeReasons);
    for (const auto& textRenderer : childrenOfType<RenderText>(container)) {
        // FIXME: Do not return until after checking all children.
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
            WebCore::TextRun run(String(textRenderer.text()));
            run.setCharacterScanForCodePath(false);
            if (style.fontCascade().codePath(run) != FontCascade::CodePath::Simple)
                SET_REASON_AND_RETURN_IF_NEEDED(FlowHasComplexFontCodePath, reasons, includeReasons);
        }

        auto textReasons = canUseForText(textRenderer.stringView(), fontCascade, includeReasons);
        if (textReasons)
            ADD_REASONS_AND_RETURN_IF_NEEDED(textReasons, reasons, includeReasons);
    }
    return reasons;
}

static OptionSet<AvoidanceReason> canUseForStyle(const RenderStyle& style, IncludeReasons includeReasons)
{
    OptionSet<AvoidanceReason> reasons;
    if ((style.overflowX() != Overflow::Visible && style.overflowX() != Overflow::Hidden)
        || (style.overflowY() != Overflow::Visible && style.overflowY() != Overflow::Hidden))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasOverflowNotVisible, reasons, includeReasons);
    if (style.textOverflow() == TextOverflow::Ellipsis)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextOverflow, reasons, includeReasons);
    if (!style.isLeftToRightDirection())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsNotLTR, reasons, includeReasons);
    if (style.writingMode() != WritingMode::TopToBottom)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsNotTopToBottom, reasons, includeReasons);
    if (style.unicodeBidi() != UBNormal)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasNonNormalUnicodeBiDi, reasons, includeReasons);
    if (style.rtlOrdering() != Order::Logical)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasRTLOrdering, reasons, includeReasons);
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

    // These are non-standard properties.
    if (style.lineBreak() == LineBreak::AfterWhiteSpace)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasAfterWhiteSpaceLineBreak, reasons, includeReasons);
    if (!(style.lineBoxContain().contains(LineBoxContain::Block)))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineBoxContainProperty, reasons, includeReasons);
    if (style.lineAlign() != LineAlign::None)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineAlignEdges, reasons, includeReasons);
    if (style.lineSnap() != LineSnap::None)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineSnap, reasons, includeReasons);
    if (style.borderFit() == BorderFit::Lines)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasBorderFitLines, reasons, includeReasons);    
    return reasons;
}

static OptionSet<AvoidanceReason> canUseForChild(const RenderBlockFlow& flow, const RenderObject& child, IncludeReasons includeReasons)
{
    OptionSet<AvoidanceReason> reasons;
    if (child.selectionState() != RenderObject::HighlightState::None)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowChildIsSelected, reasons, includeReasons);
    if (is<RenderCounter>(child))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowTextIsRenderCounter, reasons, includeReasons);
    if (is<RenderText>(child)) {
        const auto& renderText = downcast<RenderText>(child);
        if (renderText.textNode() && !renderText.document().markers().markersFor(*renderText.textNode()).isEmpty())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowIncludesDocumentMarkers, reasons, includeReasons);
        return reasons;
    }
    
    if (flow.containsFloats()) {
        // Non-text content may stretch the line and we don't yet have support for dynamic float avoiding (as the line grows).
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasUnsupportedFloat, reasons, includeReasons);
    }

    if (is<RenderLineBreak>(child))
        return reasons;

    if (child.isFieldset()) {
        // Fieldsets don't follow the standard CSS box model. They require special handling.
        SET_REASON_AND_RETURN_IF_NEEDED(UnsupportedFieldset, reasons, includeReasons)
    }

#if ALLOW_IMAGES || ALLOW_ALL_REPLACED || ALLOW_INLINE_BLOCK
    auto isSupportedStyle = [] (const auto& style) {
        if (style.boxShadow())
            return false;
        if (!style.hangingPunctuation().isEmpty())
            return false;
        return true;
    };
#endif
#if ALLOW_IMAGES || ALLOW_ALL_REPLACED
    if (is<RenderReplaced>(child)) {
        auto& replaced = downcast<RenderReplaced>(child);
        if (replaced.isFloating() || replaced.isPositioned())
            SET_REASON_AND_RETURN_IF_NEEDED(ChildBoxIsFloatingOrPositioned, reasons, includeReasons)

        if (replaced.isSVGRoot())
            SET_REASON_AND_RETURN_IF_NEEDED(ContentIsSVG, reasons, includeReasons);

        if (!isSupportedStyle(replaced.style()))
            SET_REASON_AND_RETURN_IF_NEEDED(ChildBoxHasUnsupportedStyle, reasons, includeReasons);

        if (is<RenderImage>(replaced)) {
            auto& image = downcast<RenderImage>(replaced);
            if (image.imageMap())
                SET_REASON_AND_RETURN_IF_NEEDED(UnsupportedImageMap, reasons, includeReasons);
            return reasons;
        }
#if !ALLOW_ALL_REPLACED
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasNonSupportedChild, reasons, includeReasons);
#endif
        return reasons;
    }
#endif

#if ALLOW_INLINE_BLOCK
    if (is<RenderBlockFlow>(child)) {
        auto& block = downcast<RenderBlockFlow>(child);
        if (!block.isReplaced() || !block.isInline())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowHasNonSupportedChild, reasons, includeReasons)
        if (block.isFloating() || block.isPositioned())
            SET_REASON_AND_RETURN_IF_NEEDED(ChildBoxIsFloatingOrPositioned, reasons, includeReasons)
        if (block.isRubyRun())
            SET_REASON_AND_RETURN_IF_NEEDED(ContentIsRuby, reasons, includeReasons);

        auto& style = block.style();
        if (!isSupportedStyle(style))
            SET_REASON_AND_RETURN_IF_NEEDED(ChildBoxHasUnsupportedStyle, reasons, includeReasons)
        if (style.display() != DisplayType::InlineBlock)
            SET_REASON_AND_RETURN_IF_NEEDED(ChildBoxHasUnsupportedStyle, reasons, includeReasons)

        return reasons;
    }
#endif

#if ALLOW_INLINES
    if (is<RenderInline>(child)) {
        auto& renderInline = downcast<RenderInline>(child);
        if (renderInline.isSVGInline())
            SET_REASON_AND_RETURN_IF_NEEDED(ContentIsSVG, reasons, includeReasons);
        if (renderInline.isRubyInline() || renderInline.isQuote())
            SET_REASON_AND_RETURN_IF_NEEDED(ContentIsRuby, reasons, includeReasons);
        if (renderInline.requiresLayer())
            SET_REASON_AND_RETURN_IF_NEEDED(InlineBoxNeedsLayer, reasons, includeReasons)
        if (flow.fragmentedFlowState() != RenderObject::NotInsideFragmentedFlow)
            SET_REASON_AND_RETURN_IF_NEEDED(FlowHasNonSupportedChild, reasons, includeReasons);

        auto& style = renderInline.style();
        if (!isSupportedStyle(style))
            SET_REASON_AND_RETURN_IF_NEEDED(ChildBoxHasUnsupportedStyle, reasons, includeReasons)
        if (style.hasBorder() || style.borderImage().hasImage())
            SET_REASON_AND_RETURN_IF_NEEDED(InlineBoxHasBorderOrBorderImage, reasons, includeReasons);
        if (style.hasBackground())
            SET_REASON_AND_RETURN_IF_NEEDED(InlineBoxHasBackground, reasons, includeReasons);
        if (style.hasOutline())
            SET_REASON_AND_RETURN_IF_NEEDED(ContentHasOutline, reasons, includeReasons);
        if (renderInline.isInFlowPositioned())
            SET_REASON_AND_RETURN_IF_NEEDED(ChildBoxIsFloatingOrPositioned, reasons, includeReasons);
        if (renderInline.containingBlock()->style().lineBoxContain() != RenderStyle::initialLineBoxContain())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineBoxContainProperty, reasons, includeReasons);
        auto fontAndTextReasons = canUseForFontAndText(downcast<RenderInline>(child), includeReasons);
        if (fontAndTextReasons)
            ADD_REASONS_AND_RETURN_IF_NEEDED(fontAndTextReasons, reasons, includeReasons);
        auto styleReasons = canUseForStyle(style, includeReasons);
        if (styleReasons)
            ADD_REASONS_AND_RETURN_IF_NEEDED(styleReasons, reasons, includeReasons);

        return reasons;
    }
#endif

    SET_REASON_AND_RETURN_IF_NEEDED(FlowHasNonSupportedChild, reasons, includeReasons);
    return reasons;
}

OptionSet<AvoidanceReason> canUseForLineLayoutWithReason(const RenderBlockFlow& flow, IncludeReasons includeReasons)
{
#ifndef NDEBUG
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PAL::registerNotifyCallback("com.apple.WebKit.showModernLineLayoutCoverage", WTF::Function<void()> { printModernLineLayoutCoverage });
        PAL::registerNotifyCallback("com.apple.WebKit.showModernLineLayoutReasons", WTF::Function<void()> { printModernLineLayoutBlockList });
    });
#endif
    OptionSet<AvoidanceReason> reasons;
    // FIXME: For tests that disable SLL and expect to get CLL.
    if (!flow.settings().simpleLineLayoutEnabled())
        SET_REASON_AND_RETURN_IF_NEEDED(FeatureIsDisabled, reasons, includeReasons);
    auto establishesInlineFormattingContext = [&] {
        if (flow.isRenderView()) {
            // RenderView initiates a block formatting context.
            return false;
        }
        ASSERT(flow.parent());
        auto* firstInFlowChild = flow.firstInFlowChild();
        if (!firstInFlowChild) {
            // Empty block containers do not initiate inline formatting context.
            return false;
        }
        return firstInFlowChild->isInline() || firstInFlowChild->isInlineBlockOrInlineTable();
    };
    if (!establishesInlineFormattingContext())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowDoesNotEstablishInlineFormattingContext, reasons, includeReasons);
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
        SET_REASON_AND_RETURN_IF_NEEDED(ContentHasOutline, reasons, includeReasons);
    if (flow.isRubyText() || flow.isRubyBase())
        SET_REASON_AND_RETURN_IF_NEEDED(ContentIsRuby, reasons, includeReasons);
    if (!flow.style().hangingPunctuation().isEmpty())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasHangingPunctuation, reasons, includeReasons);

    // Printing does pagination without a flow thread.
    if (flow.document().paginated())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsPaginated, reasons, includeReasons);
    if (flow.document().highlightRegisterIfExists() && !flow.document().highlightRegisterIfExists()->map().isEmpty())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIncludesHighlights, reasons, includeReasons);
#if ENABLE(APP_HIGHLIGHTS)
    if (flow.document().appHighlightRegisterIfExists() && !flow.document().appHighlightRegisterIfExists()->map().isEmpty())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIncludesHighlights, reasons, includeReasons);
#endif
    if (flow.firstLineBlock())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasPseudoFirstLine, reasons, includeReasons);
    if (flow.isAnonymousBlock() && flow.parent()->style().textOverflow() == TextOverflow::Ellipsis)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextOverflow, reasons, includeReasons);
    if (!flow.parent()->style().lineClamp().isNone())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineClamp, reasons, includeReasons);
    // This currently covers <blockflow>#text</blockflow>, <blockflow>#text<br></blockflow> and mutiple (sibling) RenderText cases.
    // The <blockflow><inline>#text</inline></blockflow> case is also popular and should be relatively easy to cover.
    for (auto walker = InlineWalker(const_cast<RenderBlockFlow&>(flow)); !walker.atEnd(); walker.advance()) {
        if (auto childReasons = canUseForChild(flow, *walker.current(), includeReasons))
            ADD_REASONS_AND_RETURN_IF_NEEDED(childReasons, reasons, includeReasons);
    }
    auto styleReasons = canUseForStyle(flow.style(), includeReasons);
    if (styleReasons)
        ADD_REASONS_AND_RETURN_IF_NEEDED(styleReasons, reasons, includeReasons);
    // We can't use the code path if any lines would need to be shifted below floats. This is because we don't keep per-line y coordinates.
    if (flow.containsFloats()) {
        for (auto& floatingObject : *flow.floatingObjectSet()) {
            ASSERT(floatingObject);
            // if a float has a shape, we cannot tell if content will need to be shifted until after we lay it out,
            // since the amount of space is not uniform for the height of the float.
            if (floatingObject->renderer().shapeOutsideInfo())
                SET_REASON_AND_RETURN_IF_NEEDED(FlowHasUnsupportedFloat, reasons, includeReasons);
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
