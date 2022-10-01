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

#include "DeprecatedGlobalSettings.h"
#include "HTMLTextFormControlElement.h"
#include "InlineWalker.h"
#include "Logging.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderFlexibleBox.h"
#include "RenderGrid.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderLineBreak.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderMultiColumnFlow.h"
#include "RenderTable.h"
#include "RenderTextControl.h"
#include "RenderView.h"
#include "Settings.h"
#include <pal/Logging.h>
#include <wtf/OptionSet.h>

#define ALLOW_FLOATS 1
#define ALLOW_RTL_FLOATS 1
#define ALLOW_VERTICAL_FLOATS 1

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
    case AvoidanceReason::FlowHasLineClamp:
        stream << "-webkit-line-clamp";
        break;
    case AvoidanceReason::FlowHasNonSupportedChild:
        stream << "unsupported child renderer";
        break;
    case AvoidanceReason::FlowHasUnsupportedFloat:
        stream << "complicated float";
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
    case AvoidanceReason::FlowHasTextCombine:
        stream << "text combine";
        break;
    case AvoidanceReason::FlowHasAfterWhiteSpaceLineBreak:
        stream << "line-break is after-white-space";
        break;
    case AvoidanceReason::FlowTextIsCombineText:
        stream << "text is combine";
        break;
    case AvoidanceReason::FlowTextIsSVGInlineText:
        stream << "SVGInlineText";
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
    case AvoidanceReason::ChildBoxIsFloatingOrPositioned:
        stream << "child box is floating or positioned";
        break;
    case AvoidanceReason::ContentIsSVG:
        stream << "SVG content";
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

static OptionSet<AvoidanceReason> canUseForStyle(const RenderElement& renderer, IncludeReasons includeReasons)
{
    auto& style = renderer.style();
    OptionSet<AvoidanceReason> reasons;
    if (style.writingMode() == WritingMode::BottomToTop)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasUnsupportedWritingMode, reasons, includeReasons);
    if (style.hasTextCombine())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasTextCombine, reasons, includeReasons);
    if (!style.hangingPunctuation().isEmpty())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasHangingPunctuation, reasons, includeReasons)
#if ENABLE(CSS_BOX_DECORATION_BREAK)
    if (style.boxDecorationBreak() == BoxDecorationBreak::Clone)
        SET_REASON_AND_RETURN_IF_NEEDED(BoxDecorationBreakClone, reasons, includeReasons);
#endif
    // These are non-standard properties.
    if (style.lineBreak() == LineBreak::AfterWhiteSpace)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasAfterWhiteSpaceLineBreak, reasons, includeReasons);
    if (style.lineBoxContain() != RenderStyle::initialLineBoxContain())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineBoxContainProperty, reasons, includeReasons);
    if (style.lineAlign() != LineAlign::None)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineAlignEdges, reasons, includeReasons);
    if (style.lineSnap() != LineSnap::None)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineSnap, reasons, includeReasons);
    if (!renderer.parent()->style().lineClamp().isNone()) {
        // Line clamp is on the deprecated flex box and not the root.
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineClamp, reasons, includeReasons);
    }

    return reasons;
}

static OptionSet<AvoidanceReason> canUseForRenderInlineChild(const RenderInline& renderInline, IncludeReasons includeReasons)
{
    OptionSet<AvoidanceReason> reasons;

    if (renderInline.isSVGInline())
        SET_REASON_AND_RETURN_IF_NEEDED(ContentIsSVG, reasons, includeReasons);
    if (renderInline.isRubyInline())
        SET_REASON_AND_RETURN_IF_NEEDED(ContentIsRuby, reasons, includeReasons);

    auto styleReasons = canUseForStyle(renderInline, includeReasons);
    if (styleReasons)
        ADD_REASONS_AND_RETURN_IF_NEEDED(styleReasons, reasons, includeReasons);

    return reasons;
}

static OptionSet<AvoidanceReason> canUseForChild(const RenderObject& child, IncludeReasons includeReasons)
{
    OptionSet<AvoidanceReason> reasons;

    if (is<RenderText>(child)) {
        if (child.isCombineText())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextIsCombineText, reasons, includeReasons);
        if (child.isSVGInlineText())
            SET_REASON_AND_RETURN_IF_NEEDED(FlowTextIsSVGInlineText, reasons, includeReasons);

        return reasons;
    }

    if (is<RenderLineBreak>(child))
        return reasons;

    auto isSupportedFloatingOrPositioned = [&] (auto& renderer) {
        if (renderer.style().styleType() == PseudoId::FirstLetter) {
            // Initial letter implementation uses a specialized float behavior internally.
            auto& style = renderer.style();
            if (renderer.isFloating() && (!style.initialLetter().isEmpty() || style.initialLetterDrop() || style.initialLetterHeight()))
                return false;
        }
#if !ALLOW_FLOATS
        if (renderer.isFloating())
            return false;
#endif
#if !ALLOW_RTL_FLOATS
        if (renderer.isFloating() && !renderer.parent()->style().isLeftToRightDirection())
            return false;
#endif
#if !ALLOW_VERTICAL_FLOATS
        if (renderer.isFloating() && !renderer.parent()->style().isHorizontalWritingMode())
            return false;
#endif
        if (renderer.style().shapeOutside())
            return false;
        if (renderer.isOutOfFlowPositioned()) {
            if (!is<RenderReplaced>(renderer))
                return false;
            if (!renderer.parent()->style().isLeftToRightDirection())
                return false;
            // This simply checks the not-yet supported case when the out-of-flow positioned
            // content has a relatively positioned inline containing block (has to call container to not skip RenderInlines).
            return is<RenderBlockFlow>(child.container());
        }
        return true;
    };

    auto& renderer = downcast<RenderElement>(child);
    if (is<RenderReplaced>(renderer)) {
        if (!isSupportedFloatingOrPositioned(renderer))
            SET_REASON_AND_RETURN_IF_NEEDED(ChildBoxIsFloatingOrPositioned, reasons, includeReasons)

        if (renderer.isSVGRootOrLegacySVGRoot())
            SET_REASON_AND_RETURN_IF_NEEDED(ContentIsSVG, reasons, includeReasons);

        return reasons;
    }

    if (is<RenderListItem>(renderer)) {
        if (!isSupportedFloatingOrPositioned(renderer))
            SET_REASON_AND_RETURN_IF_NEEDED(FlowIsUnsupportedListItem, reasons, includeReasons);
        return reasons;
    }

    if (is<RenderListMarker>(renderer) && is<RenderListItem>(renderer.parent()) && !is<RenderListMarker>(renderer.nextSibling()))
        return reasons;

    if (is<RenderTable>(renderer)) {
        if (!isSupportedFloatingOrPositioned(renderer))
            SET_REASON_AND_RETURN_IF_NEEDED(ChildBoxIsFloatingOrPositioned, reasons, includeReasons)
        return reasons;
    }

    if (is<RenderBlockFlow>(renderer) || is<RenderGrid>(renderer) || is<RenderFlexibleBox>(renderer) || is<RenderDeprecatedFlexibleBox>(renderer)) {
        if (!isSupportedFloatingOrPositioned(renderer))
            SET_REASON_AND_RETURN_IF_NEEDED(ChildBoxIsFloatingOrPositioned, reasons, includeReasons)
        if (renderer.isRubyRun())
            SET_REASON_AND_RETURN_IF_NEEDED(ContentIsRuby, reasons, includeReasons);

        return reasons;
    }

    if (is<RenderInline>(renderer)) {
        auto renderInlineReasons = canUseForRenderInlineChild(downcast<RenderInline>(renderer), includeReasons);
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
    if (!DeprecatedGlobalSettings::inlineFormattingContextIntegrationEnabled())
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
    if (is<RenderListItem>(flow) && (flow.isPositioned() || flow.isFloating()))
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsUnsupportedListItem, reasons, includeReasons);

    // Printing does pagination without a flow thread.
    if (flow.document().paginated())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsPaginated, reasons, includeReasons);

    for (auto walker = InlineWalker(flow); !walker.atEnd(); walker.advance()) {
        auto& child = *walker.current();
        if (auto childReasons = canUseForChild(child, includeReasons))
            ADD_REASONS_AND_RETURN_IF_NEEDED(childReasons, reasons, includeReasons);
    }
    auto styleReasons = canUseForStyle(flow, includeReasons);
    if (styleReasons)
        ADD_REASONS_AND_RETURN_IF_NEEDED(styleReasons, reasons, includeReasons);

    if (flow.containsFloats()) {
        for (auto& floatingObject : *flow.floatingObjectSet()) {
            ASSERT(floatingObject);
            // if a float has a shape, we cannot tell if content will need to be shifted until after we lay it out,
            // since the amount of space is not uniform for the height of the float.
            if (floatingObject->renderer().shapeOutsideInfo())
                SET_REASON_AND_RETURN_IF_NEEDED(FlowHasUnsupportedFloat, reasons, includeReasons);
        }
    }
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

    if (!flexBox.firstInFlowChild())
        return false;

    auto& flexBoxStyle = flexBox.style();
    // FIXME: Needs baseline support.
    if (flexBoxStyle.display() == DisplayType::InlineFlex)
        return false;

    // FIXME: Flex subclasses are not supported yet.
    if (flexBoxStyle.display() != DisplayType::Flex)
        return false;

    if (!flexBoxStyle.isHorizontalWritingMode() || !flexBoxStyle.isLeftToRightDirection())
        return false;

    if (flexBoxStyle.flexDirection() == FlexDirection::Column || flexBoxStyle.flexDirection() == FlexDirection::ColumnReverse)
        return false;

    if (flexBoxStyle.logicalHeight().isPercent())
        return false;

    if (flexBoxStyle.overflowY() == Overflow::Scroll || flexBoxStyle.overflowY() == Overflow::Auto)
        return false;

    auto alignItemValue = flexBoxStyle.alignItems().position();
    if (alignItemValue == ItemPosition::Baseline || alignItemValue == ItemPosition::LastBaseline || alignItemValue == ItemPosition::SelfStart || alignItemValue == ItemPosition::SelfEnd)
        return false;
    if (flexBoxStyle.alignContent().position() != ContentPosition::Normal || flexBoxStyle.alignContent().distribution() != ContentDistribution::Default || flexBoxStyle.alignContent().overflow() != OverflowAlignment::Default)
        return false;
    if (!flexBoxStyle.rowGap().isNormal() || !flexBoxStyle.columnGap().isNormal())
        return false;

    for (auto& flexItem : childrenOfType<RenderElement>(flexBox)) {
        if (!is<RenderBlock>(flexItem))
            return false;
        if (flexItem.isFloating() || flexItem.isOutOfFlowPositioned())
            return false;
        if (flexItem.isSVGRootOrLegacySVGRoot())
            return false;
        // FIXME: No nested flexbox support.
        if (flexItem.isFlexibleBoxIncludingDeprecated())
            return false;
        if (flexItem.isFieldset() || flexItem.isTextControl())
            return false;
        if (flexItem.isTable())
            return false;
        auto& flexItemStyle = flexItem.style();
        if (!flexItemStyle.isHorizontalWritingMode() || !flexItemStyle.isLeftToRightDirection())
            return false;
        if (!flexItemStyle.height().isFixed())
            return false;
        if (!flexItemStyle.flexBasis().isAuto() && !flexItemStyle.flexBasis().isFixed())
            return false;
        if (flexItemStyle.flexShrink() > 0 && flexItemStyle.flexShrink() < 1)
            return false;
        if (flexItemStyle.flexGrow() > 0 && flexItemStyle.flexGrow() < 1)
            return false;
        if (flexItemStyle.containsSize())
            return false;
        if (flexItemStyle.overflowX() == Overflow::Scroll || flexItemStyle.overflowY() == Overflow::Scroll)
            return false;
        if (flexItem.hasIntrinsicAspectRatio() || flexItemStyle.hasAspectRatio())
            return false;
        auto alignSelfValue = flexItemStyle.alignSelf().position();
        if (alignSelfValue == ItemPosition::Baseline || alignSelfValue == ItemPosition::LastBaseline || alignSelfValue == ItemPosition::SelfStart || alignSelfValue == ItemPosition::SelfEnd)
            return false;
    }
    return true;
}

}
}

