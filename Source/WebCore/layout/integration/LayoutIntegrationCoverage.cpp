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
#include "RenderSVGForeignObject.h"
#include "RenderStyleInlines.h"
#include "RenderTable.h"
#include "RenderTextControl.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleContentAlignmentData.h"
#include "StyleSelfAlignmentData.h"

namespace WebCore {
namespace LayoutIntegration {

bool canUseForLineLayout(const RenderBlockFlow& rootContainer)
{
    return !is<RenderSVGBlock>(rootContainer) || rootContainer.isRenderOrLegacyRenderSVGForeignObject();
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
        if (is<RenderListMarker>(renderer))
            continue;
        if (renderer.isInFlow() && renderer.style().isHorizontalWritingMode() && renderer.style().logicalWidth().isFixed()) {
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
            continue;
        }
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
        if (auto* inlineRenderer = dynamicDowncast<RenderInline>(renderer))
            return typeOfChange == TypeOfChangeForInvalidation::NodeInsertion && !inlineRenderer->firstChild();
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
        if (auto* textRenderer = dynamicDowncast<RenderText>(renderer)) {
            auto hasStrongDirectionalityContent = textRenderer->hasStrongDirectionalityContent();
            if (!hasStrongDirectionalityContent) {
                hasStrongDirectionalityContent = Layout::TextUtil::containsStrongDirectionalityText(textRenderer->text());
                const_cast<RenderText*>(textRenderer)->setHasStrongDirectionalityContent(*hasStrongDirectionalityContent);
            }
            return *hasStrongDirectionalityContent;
        }
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

    if (auto* previousDamage = lineLayout.damage(); previousDamage && (previousDamage->reasons() != Layout::InlineDamage::Reason::Append || !previousDamage->layoutStartPosition())) {
        // Only support subsequent append operations where we managed to invalidate the content for partial layout.
        return true;
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

