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

#include "Document.h"
#include "InlineWalker.h"
#include "RenderBlockFlow.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderFlexibleBox.h"
#include "RenderImage.h"
#include "RenderInline.h"
#include "RenderLineBreak.h"
#include "RenderListMarker.h"
#include "RenderSVGBlock.h"
#include "RenderSVGForeignObject.h"
#include "RenderStyleInlines.h"
#include "RenderTable.h"
#include "RenderTextControl.h"
#include "RenderView.h"
#include "StyleContentAlignmentData.h"
#include "StyleSelfAlignmentData.h"
#include <pal/Logging.h>
#include <wtf/OptionSet.h>

namespace WebCore {
namespace LayoutIntegration {

enum class AvoidanceReason : uint32_t {
    FeatureIsDisabled                   = 1U << 0,
    FlexBoxHasNonFixedHeightInMainAxis  = 1U << 1,
    FlexBoxNeedsBaseline                = 1U << 2,
    // Unused                           = 1U << 3,
    // Unused                           = 1U << 4,
    // Unused                           = 1U << 5,
    // Unused                           = 1U << 6,
    FlexBoxHasUnsupportedOverflow       = 1U << 7,
    // Unused                           = 1U << 8,
    // Unused                           = 1U << 9,
    // Unused                           = 1U << 10,
    // Unused                           = 1U << 11,
    FlexBoxHasUnsupportedTypeOfRenderer = 1U << 12,
    FlexBoxHasMarginTrim                = 1U << 13,
    FlexBoxHasOutOfFlowChild            = 1U << 14,
    FlexBoxHasSVGChild                  = 1U << 15,
    FlexBoxHasNestedFlex                = 1U << 16,
    // Unused                           = 1U << 17,
    // Unused                           = 1U << 18,
    FlexItemHasNonFixedHeight           = 1U << 19,
    FlexItemHasIntrinsicFlexBasis       = 1U << 20,
    // Unused                           = 1U << 21,
    // Unused                           = 1U << 22,
    FlexItemHasContainsSize             = 1U << 23,
    FlexItemHasUnsupportedOverflow      = 1U << 24,
    FlexItemHasAspectRatio              = 1U << 25,
    FlexItemHasBaselineAlign            = 1U << 26,
    EndOfReasons                        = 1U << 27
};

enum class IncludeReasons : bool {
    First,
    All
};

#ifndef NDEBUG
#define ADD_REASON_AND_RETURN_IF_NEEDED(reason, reasons, includeReasons) { \
        reasons.add(AvoidanceReason::reason); \
        if (includeReasons == IncludeReasons::First) \
            return reasons; \
    }
#else
#define ADD_REASON_AND_RETURN_IF_NEEDED(reason, reasons, includeReasons) { \
        ASSERT_UNUSED(includeReasons, includeReasons == IncludeReasons::First); \
        reasons.add(AvoidanceReason::reason); \
        return reasons; \
    }
#endif

static inline bool mayHaveScrollbarOrScrollableOverflow(const RenderStyle& style)
{
    return !style.isOverflowVisible() || style.scrollbarGutter() != RenderStyle::initialScrollbarGutter();
}

static OptionSet<AvoidanceReason> canUseForFlexLayoutWithReason(const RenderFlexibleBox& flexBox, IncludeReasons includeReasons)
{
    ASSERT(flexBox.firstInFlowChild());

    auto reasons = OptionSet<AvoidanceReason> { };

    if (!flexBox.document().settings().flexFormattingContextIntegrationEnabled())
        ADD_REASON_AND_RETURN_IF_NEEDED(FeatureIsDisabled, reasons, includeReasons);

    auto& flexBoxStyle = flexBox.style();
    if (flexBoxStyle.display() == DisplayType::InlineFlex)
        ADD_REASON_AND_RETURN_IF_NEEDED(FlexBoxNeedsBaseline, reasons, includeReasons);

    auto isColumnDirection = flexBoxStyle.flexDirection() == FlexDirection::Column || flexBoxStyle.flexDirection() == FlexDirection::ColumnReverse;
    auto isHorizontalWritingMode = flexBoxStyle.writingMode().isHorizontal();
    if (((isHorizontalWritingMode && isColumnDirection) || (!isHorizontalWritingMode && !isColumnDirection)) && !flexBoxStyle.height().isFixed())
        ADD_REASON_AND_RETURN_IF_NEEDED(FlexBoxHasNonFixedHeightInMainAxis, reasons, includeReasons);

    if (mayHaveScrollbarOrScrollableOverflow(flexBoxStyle))
        ADD_REASON_AND_RETURN_IF_NEEDED(FlexBoxHasUnsupportedOverflow, reasons, includeReasons);

    if (flexBoxStyle.marginTrim() != RenderStyle::initialMarginTrim())
        ADD_REASON_AND_RETURN_IF_NEEDED(FlexBoxHasMarginTrim, reasons, includeReasons);

    for (auto& flexItem : childrenOfType<RenderElement>(flexBox)) {
        if (!is<RenderBlock>(flexItem) || flexItem.isFieldset() || flexItem.isRenderTextControl() || flexItem.isRenderTable())
            ADD_REASON_AND_RETURN_IF_NEEDED(FlexBoxHasUnsupportedTypeOfRenderer, reasons, includeReasons);

        if (flexItem.isOutOfFlowPositioned())
            ADD_REASON_AND_RETURN_IF_NEEDED(FlexBoxHasOutOfFlowChild, reasons, includeReasons);

        if (flexItem.isRenderOrLegacyRenderSVGRoot())
            ADD_REASON_AND_RETURN_IF_NEEDED(FlexBoxHasSVGChild, reasons, includeReasons);

        if (flexItem.isFlexibleBoxIncludingDeprecated())
            ADD_REASON_AND_RETURN_IF_NEEDED(FlexBoxHasNestedFlex, reasons, includeReasons);

        auto& flexItemStyle = flexItem.style();
        if (!flexItemStyle.height().isFixed())
            ADD_REASON_AND_RETURN_IF_NEEDED(FlexItemHasNonFixedHeight, reasons, includeReasons);

        // Percentage values of flex-basis are resolved against the flex item's containing block and if that containing block's size is indefinite, the used value for flex-basis is content.
        if (flexItemStyle.flexBasis().isIntrinsic() || (flexItemStyle.flexBasis().isPercent() && isColumnDirection))
            ADD_REASON_AND_RETURN_IF_NEEDED(FlexItemHasIntrinsicFlexBasis, reasons, includeReasons);

        if (flexItemStyle.containsSize())
            ADD_REASON_AND_RETURN_IF_NEEDED(FlexItemHasContainsSize, reasons, includeReasons);

        if (mayHaveScrollbarOrScrollableOverflow(flexItemStyle))
            ADD_REASON_AND_RETURN_IF_NEEDED(FlexItemHasUnsupportedOverflow, reasons, includeReasons);

        if (flexItem.hasIntrinsicAspectRatio() || flexItemStyle.hasAspectRatio())
            ADD_REASON_AND_RETURN_IF_NEEDED(FlexItemHasAspectRatio, reasons, includeReasons);

        auto alignValue = flexItemStyle.alignSelf().position() != ItemPosition::Auto ? flexItemStyle.alignSelf().position() : flexBoxStyle.alignItems().position();
        if (alignValue == ItemPosition::Baseline || alignValue == ItemPosition::LastBaseline)
            ADD_REASON_AND_RETURN_IF_NEEDED(FlexItemHasBaselineAlign, reasons, includeReasons);
    }
    return reasons;
}

#ifndef NDEBUG
static void printTextForSubtree(const RenderElement& renderer, size_t& charactersLeft, TextStream& stream)
{
    for (auto& child : childrenOfType<RenderObject>(downcast<RenderElement>(renderer))) {
        if (is<RenderText>(child)) {
            auto text = downcast<RenderText>(child).text();
            auto textView = StringView { text }.trim(isASCIIWhitespace<UChar>);
            auto length = std::min<size_t>(charactersLeft, textView.length());
            stream << textView.left(length);
            charactersLeft -= length;
            continue;
        }
        printTextForSubtree(downcast<RenderElement>(child), charactersLeft, stream);
    }
}

static Vector<const RenderFlexibleBox*> collectFlexBoxesForCurrentPage()
{
    Vector<const RenderFlexibleBox*> flexBoxes;
    for (auto document : Document::allDocuments()) {
        if (!document->renderView() || document->backForwardCacheState() != Document::NotInBackForwardCache)
            continue;
        if (!document->isHTMLDocument() && !document->isXHTMLDocument())
            continue;
        for (auto& descendant : descendantsOfType<RenderFlexibleBox>(*document->renderView()))
            flexBoxes.append(&descendant);
    }
    return flexBoxes;
}

static void printReason(AvoidanceReason reason, TextStream& stream)
{
    switch (reason) {
    case AvoidanceReason::FeatureIsDisabled:
        stream << "modern flex layout is disabled";
        break;
    case AvoidanceReason::FlexBoxHasNonFixedHeightInMainAxis:
        stream << "flex container has non-fixed height";
        break;
    case AvoidanceReason::FlexBoxNeedsBaseline:
        stream << "inline flex box needs baseline";
        break;
    case AvoidanceReason::FlexBoxHasUnsupportedOverflow:
        stream << "flex box has non-hidden overflow";
        break;
    case AvoidanceReason::FlexBoxHasUnsupportedTypeOfRenderer:
        stream << "flex box has unsupported flex item renderer e.g. fieldset";
        break;
    case AvoidanceReason::FlexBoxHasMarginTrim:
        stream << "flex box has non-initial margin-trim";
        break;
    case AvoidanceReason::FlexBoxHasOutOfFlowChild:
        stream << "flex box has out-of-flow child";
        break;
    case AvoidanceReason::FlexBoxHasSVGChild:
        stream << "flex box has svg child";
        break;
    case AvoidanceReason::FlexBoxHasNestedFlex:
        stream << "flex box has nested flex";
        break;
    case AvoidanceReason::FlexItemHasNonFixedHeight:
        stream << "flex item has non-fixed height value";
        break;
    case AvoidanceReason::FlexItemHasIntrinsicFlexBasis:
        stream << "flex item has intrinsic flex basis value (e.g. min-content";
        break;
    case AvoidanceReason::FlexItemHasContainsSize:
        stream << "flex item has contains: size";
        break;
    case AvoidanceReason::FlexItemHasUnsupportedOverflow:
        stream << "flex item has non-hidden overflow";
        break;
    case AvoidanceReason::FlexItemHasAspectRatio:
        stream << "flex item has aspect-ratio ";
        break;
    case AvoidanceReason::FlexItemHasBaselineAlign:
        stream << "flex item has (last)baseline align";
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

static void printLegacyFlexReasons()
{
    auto flexBoxes = collectFlexBoxesForCurrentPage();
    if (!flexBoxes.size()) {
        WTFLogAlways("No flex box found in this document\n");
        return;
    }
    TextStream stream;
    stream << "---------------------------------------------------\n";
    for (auto* flexBox : flexBoxes) {
        auto reasons = canUseForFlexLayoutWithReason(*flexBox, IncludeReasons::All);
        if (reasons.isEmpty())
            continue;
        size_t printedLength = 30;
        stream << "\"";
        printTextForSubtree(*flexBox, printedLength, stream);
        stream << "...\"";
        for (;printedLength > 0; --printedLength)
            stream << " ";
        printReasons(reasons, stream);
        stream << "\n";
    }
    stream << "---------------------------------------------------\n";
    WTFLogAlways("%s", stream.release().utf8().data());
}
#endif

bool canUseForLineLayout(const RenderBlockFlow& rootContainer)
{
    return !is<RenderSVGBlock>(rootContainer) || rootContainer.isRenderOrLegacyRenderSVGForeignObject();
}

bool canUseForPreferredWidthComputation(const RenderBlockFlow& blockContainer)
{
    for (auto walker = InlineWalker(blockContainer); !walker.atEnd(); walker.advance()) {
        auto& renderer = *walker.current();

        auto isFullySupportedRenderer = renderer.isRenderText() || is<RenderLineBreak>(renderer) || is<RenderInline>(renderer) || is<RenderListMarker>(renderer);
        if (isFullySupportedRenderer)
            continue;

        if (!renderer.isInFlow() || !renderer.writingMode().isHorizontal() || !renderer.style().logicalWidth().isFixed())
            return false;

        auto isNonSupportedFixedWidthContent = [&] {
            // FIXME: Implement this image special in line builder.
            auto allowImagesToBreak = !blockContainer.document().inQuirksMode() || !blockContainer.isRenderTableCell();
            if (!allowImagesToBreak)
                return true;
            // FIXME: See RenderReplaced::computePreferredLogicalWidths where m_minPreferredLogicalWidth is set to 0.
            auto isReplacedWithSpecialIntrinsicWidth = is<RenderReplaced>(renderer) && renderer.style().logicalMaxWidth().isPercentOrCalculated();
            if (isReplacedWithSpecialIntrinsicWidth)
                return true;
            return false;
        };
        if (isNonSupportedFixedWidthContent())
            return false;
    }
    return true;
}

bool canUseForFlexLayout(const RenderFlexibleBox& flexBox)
{
#ifndef NDEBUG
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PAL::registerNotifyCallback("com.apple.WebKit.showLegacyFlexReasons"_s, Function<void()> { printLegacyFlexReasons });
    });
#endif
    return canUseForFlexLayoutWithReason(flexBox, IncludeReasons::First).isEmpty();
}

}
}

