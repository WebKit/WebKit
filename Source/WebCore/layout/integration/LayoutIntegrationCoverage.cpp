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

#include "HTMLTextFormControlElement.h"
#include "InlineWalker.h"
#include "Logging.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderCounter.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderLineBreak.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderMultiColumnFlow.h"
#include "RenderTable.h"
#include "RenderTextControl.h"
#include "RenderView.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include <pal/Logging.h>
#include <wtf/OptionSet.h>

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#define ALLOW_BIDI_CONTENT 1
#define ALLOW_BIDI_CONTENT_WITH_INLINE_BOX 1

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
    case AvoidanceReason::FlowHasLineBoxContainProperty:
        stream << "line-box-contain value indicates variable line height";
        break;
    case AvoidanceReason::FlowHasUnsupportedWritingMode:
        stream << "unsupported writing mode (vertical-rl/horizontal-bt";
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
    case AvoidanceReason::FlowHasPseudoFirstLetter:
        stream << "first-letter";
        break;
    case AvoidanceReason::FlowHasTextCombine:
        stream << "text combine";
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
    case AvoidanceReason::FlowTextIsCombineText:
        stream << "text is combine";
        break;
    case AvoidanceReason::FlowTextIsRenderCounter:
        stream << "RenderCounter";
        break;
    case AvoidanceReason::ContentIsRenderQuote:
        stream << "RenderQuote";
        break;
    case AvoidanceReason::FlowTextIsTextFragment:
        stream << "TextFragment";
        break;
    case AvoidanceReason::FlowTextIsSVGInlineText:
        stream << "SVGInlineText";
        break;
    case AvoidanceReason::FlowHasLineBoxContainGlyphs:
        stream << "-webkit-line-box-contain: glyphs";
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
    case AvoidanceReason::ChildBoxIsNotInlineBlock:
        stream << "child box has unsupported display type";
        break;
    case AvoidanceReason::UnsupportedImageMap:
        stream << "image map";
        break;
    case AvoidanceReason::InlineBoxNeedsLayer:
        stream << "inline box needs layer";
        break;
    case AvoidanceReason::BoxDecorationBreakClone:
        stream << "webkit-box-decoration-break: clone";
        break;
    case AvoidanceReason::FlowIsUnsupportedListItem:
        stream << "list item with text-indent";
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

static void printTextForSubtree(const RenderElement& renderer, unsigned& charactersLeft, TextStream& stream)
{
    for (auto& child : childrenOfType<RenderObject>(downcast<RenderElement>(renderer))) {
        if (is<RenderText>(child)) {
            String text = downcast<RenderText>(child).text();
            auto textView = StringView(text).stripWhiteSpace();
            auto length = std::min(charactersLeft, textView.length());
            stream << textView.left(length);
            charactersLeft -= length;
            continue;
        }
        printTextForSubtree(downcast<RenderElement>(child), charactersLeft, stream);
    }
}

static unsigned contentLengthForSubtreeStayWithinBlockFlow(const RenderElement& renderer)
{
    unsigned textLength = 0;
    for (auto& child : childrenOfType<RenderObject>(renderer)) {
        if (is<RenderBlockFlow>(child)) {
            // Do not descend into nested RenderBlockFlow.
            continue;
        }
        if (is<RenderText>(child)) {
            textLength += downcast<RenderText>(child).text().length();
            continue;
        }
        textLength += contentLengthForSubtreeStayWithinBlockFlow(downcast<RenderElement>(child));
    }
    return textLength;
}

static unsigned contentLengthForBlockFlow(const RenderBlockFlow& blockFlow)
{
    return contentLengthForSubtreeStayWithinBlockFlow(blockFlow);
}

static Vector<const RenderBlockFlow*> collectRenderBlockFlowsForCurrentPage()
{
    Vector<const RenderBlockFlow*> renderFlows;
    for (const auto* document : Document::allDocuments()) {
        if (!document->renderView() || document->backForwardCacheState() != Document::NotInBackForwardCache)
            continue;
        if (!document->isHTMLDocument() && !document->isXHTMLDocument())
            continue;
        for (auto& descendant : descendantsOfType<RenderBlockFlow>(*document->renderView())) {
            if (descendant.childrenInline())
                renderFlows.append(&descendant);
        }
    }
    return renderFlows;
}

static void printModernLineLayoutBlockList(void)
{
    auto renderBlockFlows = collectRenderBlockFlowsForCurrentPage();
    if (!renderBlockFlows.size()) {
        WTFLogAlways("No text found in this document\n");
        return;
    }
    TextStream stream;
    stream << "---------------------------------------------------\n";
    for (auto* flow : renderBlockFlows) {
        auto reasons = canUseForLineLayoutWithReason(*flow, IncludeReasons::All);
        if (reasons.isEmpty())
            continue;
        unsigned printedLength = 30;
        stream << "\"";
        printTextForSubtree(*flow, printedLength, stream);
        for (;printedLength > 0; --printedLength)
            stream << " ";
        stream << "\"(" << contentLengthForBlockFlow(*flow) << "):";
        printReasons(reasons, stream);
        stream << "\n";
    }
    stream << "---------------------------------------------------\n";
    WTFLogAlways("%s", stream.release().utf8().data());
}

static void printModernLineLayoutCoverage(void)
{
    auto renderBlockFlows = collectRenderBlockFlowsForCurrentPage();
    if (!renderBlockFlows.size()) {
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
    for (auto* flow : renderBlockFlows) {
        auto flowLength = contentLengthForBlockFlow(*flow);
        textLength += flowLength;
        auto reasons = canUseForLineLayoutWithReason(*flow, IncludeReasons::All);
        if (reasons.isEmpty()) {
            if (flow->lineLayoutPath() == RenderBlockFlow::ForcedLegacyPath) {
                supportedButForcedToLineLayoutTextLength += flowLength;
                ++numberOfSupportedButForcedToLineLayoutLeafBlocks;
            }
            continue;
        }
        if (reasons.contains(AvoidanceReason::FlowDoesNotEstablishInlineFormattingContext))
            continue;
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
    stream << "Number of blocks: total(" <<  renderBlockFlows.size() << ") legacy(" << numberOfUnsupportedLeafBlocks << ")\nContent length: total(" <<
        textLength << ") legacy(" << unsupportedTextLength << ")\n";
    for (const auto& reasonEntry : flowStatistics) {
        printReason(static_cast<AvoidanceReason>(reasonEntry.key), stream);
        stream << ": " << (float)reasonEntry.value / (float)textLength * 100 << "%\n";
    }
    stream << "---------------------------------------------------\n";
    WTFLogAlways("%s", stream.release().utf8().data());
}
#endif

static OptionSet<AvoidanceReason> canUseForText(StringView text, IncludeReasons includeReasons)
{
    if (text.is8Bit())
        return { };

    OptionSet<AvoidanceReason> reasons;
    auto length = text.length();
    size_t position = 0;
    while (position < length) {
        UChar32 character;
        U16_NEXT(text.characters16(), position, length, character);

        auto isRTLDirectional = [&](auto character) {
            auto direction = u_charDirection(character);
            return direction == U_RIGHT_TO_LEFT || direction == U_RIGHT_TO_LEFT_ARABIC
                || direction == U_RIGHT_TO_LEFT_EMBEDDING || direction == U_RIGHT_TO_LEFT_OVERRIDE
                || direction == U_LEFT_TO_RIGHT_EMBEDDING || direction == U_LEFT_TO_RIGHT_OVERRIDE
                || direction == U_POP_DIRECTIONAL_FORMAT;
        };
        if (isRTLDirectional(character))
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextHasDirectionCharacter, reasons, includeReasons);
    }
    return { };
}

enum class CheckForBidiCharacters { Yes, No };
static OptionSet<AvoidanceReason> canUseForFontAndText(const RenderBoxModelObject& container, CheckForBidiCharacters checkForBidiCharacters, IncludeReasons includeReasons)
{
    OptionSet<AvoidanceReason> reasons;
    // We assume that all lines have metrics based purely on the primary font.
    const auto& style = container.style();
    if (style.lineBoxContain().contains(LineBoxContain::Glyphs))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineBoxContainGlyphs, reasons, includeReasons);
    for (const auto& textRenderer : childrenOfType<RenderText>(container)) {
        // FIXME: Do not return until after checking all children.
        if (textRenderer.isCombineText())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextIsCombineText, reasons, includeReasons);
        if (textRenderer.isCounter())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextIsRenderCounter, reasons, includeReasons);
        if (textRenderer.isTextFragment())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextIsTextFragment, reasons, includeReasons);
        if (textRenderer.isSVGInlineText())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextIsSVGInlineText, reasons, includeReasons);

        if (checkForBidiCharacters == CheckForBidiCharacters::Yes) {
            if (auto textReasons = canUseForText(textRenderer.stringView(), includeReasons))
                ADD_REASONS_AND_RETURN_IF_NEEDED(textReasons, reasons, includeReasons);
        }
    }
    return reasons;
}

static OptionSet<AvoidanceReason> canUseForStyle(const RenderElement& renderer, IncludeReasons includeReasons)
{
    auto& style = renderer.style();
    OptionSet<AvoidanceReason> reasons;
    if ((style.overflowX() != Overflow::Visible && style.overflowX() != Overflow::Hidden)
        || (style.overflowY() != Overflow::Visible && style.overflowY() != Overflow::Hidden))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasOverflowNotVisible, reasons, includeReasons);
    if (style.textOverflow() == TextOverflow::Ellipsis)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextOverflow, reasons, includeReasons);
    if (style.writingMode() == WritingMode::BottomToTop)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasUnsupportedWritingMode, reasons, includeReasons);
    if (style.textEmphasisFill() != TextEmphasisFill::Filled || style.textEmphasisMark() != TextEmphasisMark::None)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextEmphasisFillOrMark, reasons, includeReasons);
    if (style.hasPseudoStyle(PseudoId::FirstLetter))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasPseudoFirstLetter, reasons, includeReasons);
    if (style.hasTextCombine())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextCombine, reasons, includeReasons);

    // These are non-standard properties.
    if (style.lineBreak() == LineBreak::AfterWhiteSpace)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasAfterWhiteSpaceLineBreak, reasons, includeReasons);
    if (!(style.lineBoxContain().contains(LineBoxContain::Block)))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineBoxContainProperty, reasons, includeReasons);
    if (style.lineAlign() != LineAlign::None)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineAlignEdges, reasons, includeReasons);
    if (style.lineSnap() != LineSnap::None)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineSnap, reasons, includeReasons);
    return reasons;
}

static OptionSet<AvoidanceReason> canUseForRenderInlineChild(const RenderInline& renderInline, IncludeReasons includeReasons)
{
    OptionSet<AvoidanceReason> reasons;

    if (renderInline.isSVGInline())
        SET_REASON_AND_RETURN_IF_NEEDED(ContentIsSVG, reasons, includeReasons);
    if (renderInline.isRubyInline())
        SET_REASON_AND_RETURN_IF_NEEDED(ContentIsRuby, reasons, includeReasons);
    if (renderInline.isQuote())
        SET_REASON_AND_RETURN_IF_NEEDED(ContentIsRenderQuote, reasons, includeReasons);
    if (renderInline.requiresLayer())
        SET_REASON_AND_RETURN_IF_NEEDED(InlineBoxNeedsLayer, reasons, includeReasons)

    auto& style = renderInline.style();
    if (!style.hangingPunctuation().isEmpty())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasHangingPunctuation, reasons, includeReasons)
#if ENABLE(CSS_BOX_DECORATION_BREAK)
    if (style.boxDecorationBreak() == BoxDecorationBreak::Clone)
        SET_REASON_AND_RETURN_IF_NEEDED(BoxDecorationBreakClone, reasons, includeReasons);
#endif
    if (renderInline.isInFlowPositioned())
        SET_REASON_AND_RETURN_IF_NEEDED(ChildBoxIsFloatingOrPositioned, reasons, includeReasons);
    if (renderInline.containingBlock()->style().lineBoxContain() != RenderStyle::initialLineBoxContain())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineBoxContainProperty, reasons, includeReasons);
    auto checkForBidiCharacters = CheckForBidiCharacters::Yes;
#if ALLOW_BIDI_CONTENT_WITH_INLINE_BOX
    checkForBidiCharacters = CheckForBidiCharacters::No;
#endif
    auto fontAndTextReasons = canUseForFontAndText(renderInline, checkForBidiCharacters, includeReasons);
    if (fontAndTextReasons)
        ADD_REASONS_AND_RETURN_IF_NEEDED(fontAndTextReasons, reasons, includeReasons);

    auto styleReasons = canUseForStyle(renderInline, includeReasons);
    if (styleReasons)
        ADD_REASONS_AND_RETURN_IF_NEEDED(styleReasons, reasons, includeReasons);

    return reasons;
}

static OptionSet<AvoidanceReason> canUseForChild(const RenderObject& child, IncludeReasons includeReasons)
{
    OptionSet<AvoidanceReason> reasons;

    if (is<RenderText>(child)) {
        if (is<RenderCounter>(child))
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextIsRenderCounter, reasons, includeReasons);

        return reasons;
    }

    if (is<RenderLineBreak>(child))
        return reasons;

    if (child.isFieldset()) {
        // Fieldsets don't follow the standard CSS box model. They require special handling.
        SET_REASON_AND_RETURN_IF_NEEDED(UnsupportedFieldset, reasons, includeReasons)
    }

    if (is<RenderReplaced>(child)) {
        auto& replaced = downcast<RenderReplaced>(child);
        if (replaced.isFloating() || replaced.isPositioned())
            SET_REASON_AND_RETURN_IF_NEEDED(ChildBoxIsFloatingOrPositioned, reasons, includeReasons)

        if (replaced.isSVGRootOrLegacySVGRoot())
            SET_REASON_AND_RETURN_IF_NEEDED(ContentIsSVG, reasons, includeReasons);

        if (is<RenderImage>(replaced)) {
            auto& image = downcast<RenderImage>(replaced);
            if (image.imageMap())
                SET_REASON_AND_RETURN_IF_NEEDED(UnsupportedImageMap, reasons, includeReasons);
            return reasons;
        }
        return reasons;
    }

    if (is<RenderListItem>(child)) {
        if (child.style().textIndent().value() || !child.style().isHorizontalWritingMode() || !child.style().isLeftToRightDirection() || child.isPositioned() || child.isFloating())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowIsUnsupportedListItem, reasons, includeReasons);
        return reasons;
    }

    if (is<RenderListMarker>(child) && is<RenderListItem>(child.parent()) && !is<RenderListMarker>(child.nextSibling()))
        return reasons;

    if (is<RenderTable>(child)) {
        auto& table = downcast<RenderTable>(child);
        if (!table.isInline() || table.isPositioned() || !table.style().isHorizontalWritingMode())
            SET_REASON_AND_RETURN_IF_NEEDED(ChildBoxIsFloatingOrPositioned, reasons, includeReasons)
        return reasons;
    }

    if (is<RenderBlockFlow>(child)) {
        auto& block = downcast<RenderBlockFlow>(child);
        if (!block.isReplacedOrInlineBlock() || !block.isInline())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowHasNonSupportedChild, reasons, includeReasons)
        if (block.isFloating() || block.isPositioned())
            SET_REASON_AND_RETURN_IF_NEEDED(ChildBoxIsFloatingOrPositioned, reasons, includeReasons)
        if (block.isRubyRun())
            SET_REASON_AND_RETURN_IF_NEEDED(ContentIsRuby, reasons, includeReasons);

        auto& style = block.style();
        if (!style.hangingPunctuation().isEmpty())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowHasHangingPunctuation, reasons, includeReasons)
        if (style.display() != DisplayType::InlineBlock)
            SET_REASON_AND_RETURN_IF_NEEDED(ChildBoxIsNotInlineBlock, reasons, includeReasons)

        return reasons;
    }

    if (is<RenderInline>(child)) {
        auto renderInlineReasons = canUseForRenderInlineChild(downcast<RenderInline>(child), includeReasons);
        if (renderInlineReasons)
            ADD_REASONS_AND_RETURN_IF_NEEDED(renderInlineReasons, reasons, includeReasons);
        return reasons;
    }

    SET_REASON_AND_RETURN_IF_NEEDED(FlowHasNonSupportedChild, reasons, includeReasons);
    return reasons;
}

OptionSet<AvoidanceReason> canUseForLineLayoutWithReason(const RenderBlockFlow& flow, IncludeReasons includeReasons)
{
#ifndef NDEBUG
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PAL::registerNotifyCallback("com.apple.WebKit.showModernLineLayoutCoverage"_s, Function<void()> { printModernLineLayoutCoverage });
        PAL::registerNotifyCallback("com.apple.WebKit.showModernLineLayoutReasons"_s, Function<void()> { printModernLineLayoutBlockList });
    });
#endif
    OptionSet<AvoidanceReason> reasons;
    // FIXME: For tests that disable SLL and expect to get CLL.
    if (!RuntimeEnabledFeatures::sharedFeatures().inlineFormattingContextIntegrationEnabled())
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
        if (columnThread.parent() != &flow.view() || !flow.style().isHorizontalWritingMode())
            SET_REASON_AND_RETURN_IF_NEEDED(MultiColumnFlowIsNotTopLevel, reasons, includeReasons);
        if (columnThread.hasColumnSpanner())
            SET_REASON_AND_RETURN_IF_NEEDED(MultiColumnFlowHasColumnSpanner, reasons, includeReasons);
        auto& style = flow.style();
        if (style.verticalAlign() != VerticalAlign::Baseline)
            SET_REASON_AND_RETURN_IF_NEEDED(MultiColumnFlowVerticalAlign, reasons, includeReasons);
        if (style.isFloating())
            SET_REASON_AND_RETURN_IF_NEEDED(MultiColumnFlowIsFloating, reasons, includeReasons);
    }
    if (flow.isRubyText() || flow.isRubyBase())
        SET_REASON_AND_RETURN_IF_NEEDED(ContentIsRuby, reasons, includeReasons);
    if (is<RenderListItem>(flow) && (flow.style().textIndent().value() || !flow.style().isHorizontalWritingMode() || !flow.style().isLeftToRightDirection() || flow.isPositioned() || flow.isFloating()))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsUnsupportedListItem, reasons, includeReasons);
    if (!flow.style().hangingPunctuation().isEmpty())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasHangingPunctuation, reasons, includeReasons);

    // Printing does pagination without a flow thread.
    if (flow.document().paginated())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsPaginated, reasons, includeReasons);
    if (flow.isAnonymousBlock() && flow.parent()->style().textOverflow() == TextOverflow::Ellipsis)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextOverflow, reasons, includeReasons);
    if (!flow.parent()->style().lineClamp().isNone())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineClamp, reasons, includeReasons);
    // This currently covers <blockflow>#text</blockflow>, <blockflow>#text<br></blockflow> and mutiple (sibling) RenderText cases.
    // The <blockflow><inline>#text</inline></blockflow> case is also popular and should be relatively easy to cover.
    auto hasSeenInlineBox = false;
    for (auto walker = InlineWalker(flow); !walker.atEnd(); walker.advance()) {
        if (auto childReasons = canUseForChild(*walker.current(), includeReasons))
            ADD_REASONS_AND_RETURN_IF_NEEDED(childReasons, reasons, includeReasons);
        hasSeenInlineBox = hasSeenInlineBox || is<RenderInline>(*walker.current());
    }
    auto styleReasons = canUseForStyle(flow, includeReasons);
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
    auto checkForBidiCharacters = CheckForBidiCharacters::Yes;
#if ALLOW_BIDI_CONTENT
    if (!hasSeenInlineBox)
        checkForBidiCharacters = CheckForBidiCharacters::No;
#endif
#if ALLOW_BIDI_CONTENT_WITH_INLINE_BOX
    checkForBidiCharacters = CheckForBidiCharacters::No;
#endif
    auto fontAndTextReasons = canUseForFontAndText(flow, checkForBidiCharacters, includeReasons);
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
    case StyleDifference::RepaintIfText:
    case StyleDifference::RepaintLayer:
        // FIXME: We could do a more focused style check by matching RendererStyle::changeRequiresRepaint&co.
        return canUseForStyle(blockContainer, IncludeReasons::First).isEmpty();
    case StyleDifference::LayoutPositionedMovementOnly:
        return true;
    case StyleDifference::SimplifiedLayout:
    case StyleDifference::SimplifiedLayoutAndPositionedMovement:
        return canUseForStyle(blockContainer, IncludeReasons::First).isEmpty();
    case StyleDifference::Layout:
    case StyleDifference::NewStyle:
        return canUseForLineLayout(blockContainer);
    }
    ASSERT_NOT_REACHED();
    return canUseForLineLayout(blockContainer);
}

bool canUseForLineLayoutAfterInlineBoxStyleChange(const RenderInline& renderer, StyleDifference)
{
    return canUseForRenderInlineChild(renderer, IncludeReasons::First).isEmpty();
}

bool canUseForFlexLayout(const RenderFlexibleBox& flexBox)
{
    if (!flexBox.document().settings().flexFormattingContextIntegrationEnabled())
        return false;

    auto& flexBoxStyle = flexBox.style();
    if (!flexBoxStyle.isHorizontalWritingMode() || !flexBoxStyle.isLeftToRightDirection())
        return false;
    if (flexBoxStyle.flexWrap() == FlexWrap::Reverse)
        return false;
    if (flexBoxStyle.alignItems().position() == ItemPosition::Baseline)
        return false;
    if (flexBoxStyle.alignContent().position() != ContentPosition::Normal || flexBoxStyle.alignContent().distribution() != ContentDistribution::Default || flexBoxStyle.alignContent().overflow() != OverflowAlignment::Default)
        return false;
    if (!flexBoxStyle.rowGap().isNormal() || !flexBoxStyle.columnGap().isNormal())
        return false;

    for (auto& flexItem : childrenOfType<RenderElement>(flexBox)) {
        if (!is<RenderBox>(flexItem))
            return false;
        if (flexItem.isFloating() || flexItem.isOutOfFlowPositioned())
            return false;
        if (flexItem.isSVGRootOrLegacySVGRoot())
            return false;
        auto& flexItemStyle = flexItem.style();
        if (!flexItemStyle.maxWidth().isUndefined() || !flexItemStyle.maxHeight().isUndefined())
            return false;
        if (flexItem.hasIntrinsicAspectRatio() || flexItemStyle.hasAspectRatio())
            return false;
    }
    return true;
}

}
}

#endif
