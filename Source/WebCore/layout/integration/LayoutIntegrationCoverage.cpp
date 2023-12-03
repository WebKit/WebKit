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

#include "GapLength.h"
#include "HTMLTextFormControlElement.h"
#include "InlineWalker.h"
#include "LayoutIntegrationLineLayout.h"
#include "RenderBlockFlow.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderFlexibleBox.h"
#include "RenderFrameSet.h"
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
#include "RenderView.h"
#include "Settings.h"
#include "StyleContentAlignmentData.h"
#include "StyleSelfAlignmentData.h"

namespace WebCore {
namespace LayoutIntegration {

static std::optional<AvoidanceReason> canUseForChild(const RenderObject& child)
{
    if (is<RenderText>(child)) {
        if (child.isRenderSVGInlineText())
            return AvoidanceReason::ContentIsSVG;
        return { };
    }

    if (is<RenderLineBreak>(child))
        return { };

    auto& renderer = downcast<RenderElement>(child);
    if (renderer.isRenderRubyRun())
        return AvoidanceReason::ContentIsRuby;

    if (is<RenderBlockFlow>(renderer)
        || is<RenderGrid>(renderer)
        || is<RenderFrameSet>(renderer)
        || is<RenderFlexibleBox>(renderer)
        || is<RenderDeprecatedFlexibleBox>(renderer)
        || is<RenderReplaced>(renderer)
        || is<RenderListItem>(renderer)
        || is<RenderTable>(renderer)
#if ENABLE(MATHML)
        || is<RenderMathMLBlock>(renderer)
#endif
        || is<RenderListMarker>(renderer))
        return { };

    if (is<RenderInline>(renderer)) {
        if (renderer.isRenderSVGInline())
            return AvoidanceReason::ContentIsSVG;
        if (renderer.isRenderRubyAsInline())
            return AvoidanceReason::ContentIsRuby;
        return { };
    }

    ASSERT_NOT_REACHED();
    return { };
}

static std::optional<AvoidanceReason> canUseForLineLayoutWithReason(const RenderBlockFlow& flow)
{
    if (!flow.document().settings().inlineFormattingContextIntegrationEnabled())
        return AvoidanceReason::FeatureIsDisabled;
    if (!flow.firstChild()) {
        // Non-SVG code does not call into layoutInlineChildren with no children anymore.
        ASSERT(is<RenderSVGBlock>(flow));
        return AvoidanceReason::ContentIsSVG;
    }
    if (flow.isRenderRubyText() || flow.isRenderRubyBase())
        return AvoidanceReason::ContentIsRuby;
    for (auto walker = InlineWalker(flow); !walker.atEnd(); walker.advance()) {
        auto& child = *walker.current();
        if (auto childReason = canUseForChild(child))
            return *childReason;
    }
    return { };
}

bool canUseForLineLayout(const RenderBlockFlow& flow)
{
    return !canUseForLineLayoutWithReason(flow);
}

bool canUseForPreferredWidthComputation(const RenderBlockFlow& blockContainer)
{
    for (auto walker = InlineWalker(blockContainer); !walker.atEnd(); walker.advance()) {
        auto& renderer = *walker.current();
        if (renderer.isRenderText())
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
    auto hasFirstLetter = [&] {
        // FIXME: RenderTreeUpdater::updateTextRenderer produces odd values for offset/length when first-letter is present webkit.org/b/263343
        if (rootBlockContainer.style().hasPseudoStyle(PseudoId::FirstLetter))
            return true;
        if (rootBlockContainer.isAnonymous())
            return rootBlockContainer.containingBlock() && rootBlockContainer.containingBlock()->style().hasPseudoStyle(PseudoId::FirstLetter);
        return false;
    };
    if (hasFirstLetter())
        return true;

    if (lineLayout.isDamaged()) {
        auto previousDamages = lineLayout.damageReasons();
        if (previousDamages && previousDamages != Layout::InlineDamage::Reason::Append) {
            // Only support subsequent append operations.
            return true;
        }
    }

    bool shouldBalance = rootBlockContainer.style().textWrapMode() == TextWrapMode::Wrap && rootBlockContainer.style().textWrapStyle() == TextWrapStyle::Balance;
    if (rootBlockContainer.style().direction() == TextDirection::RTL || shouldBalance)
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
        if (flexItem.isRenderOrLegacyRenderSVGRoot())
            return false;
        // FIXME: No nested flexbox support.
        if (flexItem.isFlexibleBoxIncludingDeprecated())
            return false;
        if (flexItem.isFieldset() || flexItem.isRenderTextControl())
            return false;
        if (flexItem.isRenderTable())
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

