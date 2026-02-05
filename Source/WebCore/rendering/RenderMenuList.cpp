/*
 * This file is part of the select element renderer in WebCore.
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2006-2026 Apple Inc. All rights reserved.
 *               2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderMenuList.h"

#include "CSSFontSelector.h"
#include "ColorBlending.h"
#include "DocumentInlines.h"
#include "DocumentPage.h"
#include "ElementInlines.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "LayoutIntegrationLineLayout.h"
#include "NodeRenderStyle.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderElementInlines.h"
#include "RenderObjectInlines.h"
#include "RenderStyle+SettersInlines.h"
#include "RenderTheme.h"
#include "TextRun.h"
#include <math.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderMenuList);

RenderMenuList::RenderMenuList(HTMLSelectElement& element, RenderStyle&& style)
    : RenderFlexibleBox(Type::MenuList, element, WTF::move(style))
    , m_needsOptionsWidthUpdate(true)
    , m_optionsWidth(0)
{
    ASSERT(isRenderMenuList());
}

RenderMenuList::~RenderMenuList() = default;

HTMLSelectElement& RenderMenuList::selectElement() const
{
    return downcast<HTMLSelectElement>(nodeForNonAnonymous());
}

void RenderMenuList::styleDidChange(Style::Difference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);

    bool fontChanged = !oldStyle || !oldStyle->fontCascadeEqual(style());
    if (fontChanged) {
        updateOptionsWidth();
        m_needsOptionsWidthUpdate = false;
    }
}

void RenderMenuList::updateOptionsWidth()
{
    float maxOptionWidth = 0;
    const auto& listItems = selectElement().listItems();
    int size = listItems.size();    

    for (int i = 0; i < size; ++i) {
        RefPtr option = dynamicDowncast<HTMLOptionElement>(listItems[i].get());
        if (!option)
            continue;

        String text = option->textIndentedToRespectGroupLabel();
        text = applyTextTransform(style(), text);
        if (!text.isEmpty()) {
            const FontCascade& font = style().fontCascade();
            TextRun run = RenderBlock::constructTextRun(text, style());
            maxOptionWidth = std::max(maxOptionWidth, font.width(run));
        }
    }

    int width = static_cast<int>(ceilf(maxOptionWidth));
    if (m_optionsWidth == width)
        return;

    m_optionsWidth = width;
    if (parent())
        setNeedsLayoutAndPreferredWidthsUpdate();
}

void RenderMenuList::updateFromElement()
{
    if (m_needsOptionsWidthUpdate) {
        updateOptionsWidth();
        m_needsOptionsWidthUpdate = false;
    }
#if PLATFORM(IOS_FAMILY)
    // iOS border-radius hack.
    setNeedsLayout();
#endif
}

LayoutRect RenderMenuList::controlClipRect(const LayoutPoint& additionalOffset) const
{
    auto internalPadding = theme().popupInternalPaddingBox(style());
    auto zoom = style().usedZoomForLength();

    float paddingBoxTop = 0;
    float paddingBoxBottom = 0;
    float paddingBoxLeft = 0;
    float paddingBoxRight = 0;

    if (auto top = internalPadding.top().tryFixed())
        paddingBoxTop = top->resolveZoom(zoom);
    if (auto bottom = internalPadding.bottom().tryFixed())
        paddingBoxBottom = bottom->resolveZoom(zoom);
    if (auto left = internalPadding.left().tryFixed())
        paddingBoxLeft = left->resolveZoom(zoom);
    if (auto right = internalPadding.right().tryFixed())
        paddingBoxRight = right->resolveZoom(zoom);

    LayoutRect clipRect(additionalOffset.x() + borderLeft() + paddingLeft() + paddingBoxLeft,
        additionalOffset.y() + borderTop() + paddingTop() + paddingBoxTop,
        contentBoxWidth() - paddingBoxLeft - paddingBoxRight,
        contentBoxHeight() - paddingBoxTop - paddingBoxBottom);
    return clipRect;
}

void RenderMenuList::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    if (style().fieldSizing() == FieldSizing::Content)
        return RenderFlexibleBox::computeIntrinsicLogicalWidths(minLogicalWidth, maxLogicalWidth);

    LayoutUnit minimumSize = theme().minimumMenuListSize(style());
    maxLogicalWidth = shouldApplySizeContainment() ? minimumSize : std::max(LayoutUnit(m_optionsWidth), minimumSize);

    auto internalPadding = theme().popupInternalPaddingBox(style());
    if (auto left = internalPadding.left().tryFixed())
        maxLogicalWidth += LayoutUnit(left->resolveZoom(style().usedZoomForLength()));
    if (auto right = internalPadding.right().tryFixed())
        maxLogicalWidth += LayoutUnit(right->resolveZoom(style().usedZoomForLength()));

    if (shouldApplySizeOrInlineSizeContainment()) {
        if (auto logicalWidth = explicitIntrinsicInnerLogicalWidth())
            maxLogicalWidth = logicalWidth.value();
    }
    auto& logicalWidth = style().logicalWidth();
    if (logicalWidth.isCalculated())
        minLogicalWidth = std::max(0_lu, Style::evaluate<LayoutUnit>(logicalWidth, 0_lu, style().usedZoomForLength()));
    else if (!logicalWidth.isPercent())
        minLogicalWidth = maxLogicalWidth;
}

void RenderMenuList::computePreferredLogicalWidths()
{
    if (style().fieldSizing() == FieldSizing::Content) {
        RenderFlexibleBox::computePreferredLogicalWidths();
        return;
    }

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;
    
    if (auto fixedLogicalWidth = style().logicalWidth().tryFixed(); fixedLogicalWidth && fixedLogicalWidth->isPositive())
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(*fixedLogicalWidth);
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    RenderBox::computePreferredLogicalWidths(style().logicalMinWidth(), style().logicalMaxWidth(), writingMode().isHorizontal() ? horizontalBorderAndPaddingExtent() : verticalBorderAndPaddingExtent());

    clearNeedsPreferredWidthsUpdate();
}

void RenderMenuList::getItemBackgroundColor(unsigned listIndex, Color& itemBackgroundColor, bool& itemHasCustomBackgroundColor) const
{
    const auto& listItems = selectElement().listItems();
    if (listIndex >= listItems.size()) {
        itemBackgroundColor = style().visitedDependentBackgroundColorApplyingColorFilter();
        itemHasCustomBackgroundColor = false;
        return;
    }
    RefPtr element = listItems[listIndex].get();

    Color backgroundColor;
    if (auto* style = element->computedStyleForEditability())
        backgroundColor = style->visitedDependentBackgroundColorApplyingColorFilter();

    itemHasCustomBackgroundColor = backgroundColor.isValid() && backgroundColor.isVisible();
    // If the item has an opaque background color, return that.
    if (backgroundColor.isOpaque()) {
        itemBackgroundColor = backgroundColor;
        return;
    }

    // Otherwise, the item's background is overlayed on top of the menu background.
    backgroundColor = blendSourceOver(style().visitedDependentBackgroundColorApplyingColorFilter(), backgroundColor);
    if (backgroundColor.isOpaque()) {
        itemBackgroundColor = backgroundColor;
        return;
    }

    // If the menu background is not opaque, then add an opaque white background behind.
    itemBackgroundColor = blendSourceOver(Color::white, backgroundColor);
}

#if PLATFORM(WIN)
const int endOfLinePadding = 2;

LayoutUnit RenderMenuList::clientPaddingLeft() const
{
    if ((style().usedAppearance() == StyleAppearance::Menulist || style().usedAppearance() == StyleAppearance::MenulistButton) && writingMode().isBidiRTL()) {
        // For these appearance values, the theme applies padding to leave room for the
        // drop-down button. But leaving room for the button inside the popup menu itself
        // looks strange, so we return a small default padding to avoid having a large empty
        // space appear on the side of the popup menu.
        return endOfLinePadding;
    }
    // If the appearance isn't MenulistPart, then the select is styled (non-native), so
    // we want to return the user specified padding.
    return paddingLeft();
}

LayoutUnit RenderMenuList::clientPaddingRight() const
{
    if ((style().usedAppearance() == StyleAppearance::Menulist || style().usedAppearance() == StyleAppearance::MenulistButton) && style().writingMode().isBidiLTR())
        return endOfLinePadding;

    return paddingRight();
}
#endif

#if PLATFORM(IOS_FAMILY)
void RenderMenuList::layout()
{
    RenderFlexibleBox::layout();

    // Ideally, we should not be adjusting styles during layout. However, for a
    // pill-shaped appearance, the horizontal border radius is dependent on the
    // computed height of the box. This means that the appearance cannot be declared
    // prior to layout, since CSS only allows the horizontal border radius to be
    // dependent on the computed width of the box.
    //
    // Ignoring the style's border radius and forcing a pill-shaped appearance at
    // paint time is not an option, since focus rings and tap highlights will not
    // use the correct border radius. Consequently, we need to adjust the border
    // radius here.
    //
    // Note that similar adjustments are made in RenderSliderThumb, RenderButton
    // and RenderTextControlSingleLine.
    RenderThemeIOS::adjustRoundBorderRadius(mutableStyle(), *this);
}
#endif

}
