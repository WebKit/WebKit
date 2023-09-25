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
#include "GapLength.h"
#include "HTMLTextFormControlElement.h"
#include "InlineWalker.h"
#include "LayoutIntegrationLineLayout.h"
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
#include "RenderMathMLBlock.h"
#include "RenderSVGBlock.h"
#include "RenderStyleInlines.h"
#include "RenderTable.h"
#include "RenderTextControl.h"
#include "RenderVTTCue.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleContentAlignmentData.h"
#include "StyleSelfAlignmentData.h"
#include "TextUtil.h"
#include <pal/Logging.h>
#include <wtf/OptionSet.h>

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
    case AvoidanceReason::ContentIsRuby:
        stream << "ruby";
        break;
    case AvoidanceReason::FlowIsInitialContainingBlock:
        stream << "flow is ICB";
        break;
    case AvoidanceReason::FlowHasLineAlignEdges:
        stream << "-webkit-line-align edges";
        break;
    case AvoidanceReason::FlowHasLineSnap:
        stream << "-webkit-line-snap property";
        break;
    case AvoidanceReason::ContentIsSVG:
        stream << "SVG content";
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
            auto textView = StringView(text).trim(isUnicodeCompatibleASCIIWhitespace<UChar>);
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

static OptionSet<AvoidanceReason> canUseForBlockStyle(const RenderBlockFlow& blockContainer, IncludeReasons includeReasons)
{
    ASSERT(is<RenderBlockFlow>(blockContainer));

    OptionSet<AvoidanceReason> reasons;
    auto& style = blockContainer.style();
    if (style.lineAlign() != LineAlign::None)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineAlignEdges, reasons, includeReasons);
    if (style.lineSnap() != LineSnap::None)
        SET_REASON_AND_RETURN_IF_NEEDED(FlowHasLineSnap, reasons, includeReasons);
    return reasons;
}

static OptionSet<AvoidanceReason> canUseForChild(const RenderObject& child, IncludeReasons includeReasons)
{
    OptionSet<AvoidanceReason> reasons;

    if (is<RenderText>(child)) {
        if (child.isSVGInlineText())
            SET_REASON_AND_RETURN_IF_NEEDED(ContentIsSVG, reasons, includeReasons);
        return reasons;
    }

    if (is<RenderLineBreak>(child))
        return reasons;

    auto& renderer = downcast<RenderElement>(child);
    if (renderer.isRubyRun())
        SET_REASON_AND_RETURN_IF_NEEDED(ContentIsRuby, reasons, includeReasons);

    if (is<RenderBlockFlow>(renderer)
        || is<RenderGrid>(renderer)
        || is<RenderFlexibleBox>(renderer)
        || is<RenderDeprecatedFlexibleBox>(renderer)
        || is<RenderReplaced>(renderer)
        || is<RenderListItem>(renderer)
        || is<RenderTable>(renderer)
#if ENABLE(MATHML)
        || is<RenderMathMLBlock>(renderer)
#endif
        || is<RenderListMarker>(renderer))
        return reasons;

    if (is<RenderInline>(renderer)) {
        if (renderer.isSVGInline())
            SET_REASON_AND_RETURN_IF_NEEDED(ContentIsSVG, reasons, includeReasons);
        if (renderer.isRubyInline())
            SET_REASON_AND_RETURN_IF_NEEDED(ContentIsRuby, reasons, includeReasons);
        return reasons;
    }

    ASSERT_NOT_REACHED();
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
    if (flow.isRenderView())
        SET_REASON_AND_RETURN_IF_NEEDED(FlowIsInitialContainingBlock, reasons, includeReasons);
    if (!flow.firstChild()) {
        // Non-SVG code does not call into layoutInlineChildren with no children anymore.
        ASSERT(is<RenderSVGBlock>(flow));
        SET_REASON_AND_RETURN_IF_NEEDED(ContentIsSVG, reasons, includeReasons);
    }
    if (flow.isRubyText() || flow.isRubyBase())
        SET_REASON_AND_RETURN_IF_NEEDED(ContentIsRuby, reasons, includeReasons);
    for (auto walker = InlineWalker(flow); !walker.atEnd(); walker.advance()) {
        auto& child = *walker.current();
        if (auto childReasons = canUseForChild(child, includeReasons))
            ADD_REASONS_AND_RETURN_IF_NEEDED(childReasons, reasons, includeReasons);
    }
    auto styleReasons = canUseForBlockStyle(flow, includeReasons);
    if (styleReasons)
        ADD_REASONS_AND_RETURN_IF_NEEDED(styleReasons, reasons, includeReasons);

    return reasons;
}

bool canUseForLineLayout(const RenderBlockFlow& flow)
{
    return canUseForLineLayoutWithReason(flow, IncludeReasons::First).isEmpty();
}

bool canUseForLineLayoutAfterBlockStyleChange(const RenderBlockFlow& blockContainer, StyleDifference diff)
{
    return diff == StyleDifference::Layout ? canUseForLineLayout(blockContainer) : true;
}

bool canUseForPreferredWidthComputation(const RenderBlockFlow& blockContainer)
{
    for (auto walker = InlineWalker(blockContainer); !walker.atEnd(); walker.advance()) {
        auto& renderer = *walker.current();
        if (renderer.isText())
            continue;
        if (is<RenderLineBreak>(renderer))
            continue;
        if (is<RenderInline>(renderer))
            continue;
        return false;
    }
    return true;
}

bool shouldInvalidateLineLayoutPathAfterChangeFor(const RenderBlockFlow& rootBlockContainer, const RenderObject& renderer, const LineLayout& lineLayout, TypeOfChangeForInvalidation typeOfChange)
{
    auto isSupportedRendererWithChange = [&](auto& renderer) {
        if (is<RenderText>(renderer))
            return true;
        if (!renderer.isInFlow())
            return false;
        if (is<RenderLineBreak>(renderer))
            return true;
        if (is<RenderReplaced>(renderer))
            return typeOfChange == TypeOfChangeForInvalidation::NodeInsertion;
        if (is<RenderInline>(renderer))
            return typeOfChange == TypeOfChangeForInvalidation::NodeInsertion && !downcast<RenderInline>(renderer).firstChild();
        return false;
    };
    if (!isSupportedRendererWithChange(renderer))
        return true;

    auto isSupportedParent = [&] {
        auto* parent = renderer.parent();
        // Content append under existing inline box is not yet supported.
        return is<RenderBlockFlow>(parent) || (is<RenderInline>(parent) && !parent->everHadLayout());
    };
    if (!isSupportedParent())
        return true;
    if (lineLayout.hasOutOfFlowContent())
        return true;
    if (rootBlockContainer.containsFloats())
        return true;

    auto isBidiContent = [&] {
        if (lineLayout.contentNeedsVisualReordering())
            return true;
        if (is<RenderText>(renderer))
            return Layout::TextUtil::containsStrongDirectionalityText(downcast<RenderText>(renderer).text());
        if (is<RenderInline>(renderer)) {
            auto& style = renderer.style();
            return !style.isLeftToRightDirection() || (style.rtlOrdering() == Order::Logical && style.unicodeBidi() != UnicodeBidi::Normal);
        }
        return false;
    };
    if (isBidiContent()) {
        // FIXME: InlineItemsBuilder needs some work to support paragraph level bidi handling.
        return true;
    }
    if (lineLayout.isDamaged()) {
        auto previousDamages = lineLayout.damageReasons();
        if (previousDamages && previousDamages != Layout::InlineDamage::Reason::Append) {
            // Only support subsequent append operations.
            return true;
        }
    }
    if (rootBlockContainer.style().direction() == TextDirection::RTL || rootBlockContainer.style().textWrap() == TextWrap::Balance)
        return true;

    auto rootHasNonSupportedRenderer = [&] {
        for (auto* sibling = rootBlockContainer.firstChild(); sibling; sibling = sibling->nextSibling()) {
            if (!is<RenderText>(*sibling) && !is<RenderLineBreak>(*sibling) && !is<RenderReplaced>(*sibling))
                return true;
        }
        return !canUseForLineLayout(rootBlockContainer);
    };
    switch (typeOfChange) {
    case TypeOfChangeForInvalidation::NodeRemoval:
        return (!renderer.previousSibling() && !renderer.nextSibling()) || rootHasNonSupportedRenderer();
    case TypeOfChangeForInvalidation::NodeInsertion:
        return renderer.nextSibling() && rootHasNonSupportedRenderer();
    case TypeOfChangeForInvalidation::NodeMutation:
        return rootHasNonSupportedRenderer();
    default:
        ASSERT_NOT_REACHED();
        return true;
    }
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

