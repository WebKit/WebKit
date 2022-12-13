/*
 * This file is part of the select element renderer in WebCore.
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2015 Apple Inc. All rights reserved.
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

#include "AXObjectCache.h"
#include "AccessibilityMenuList.h"
#include "CSSFontSelector.h"
#include "Chrome.h"
#include "ColorBlending.h"
#include "DocumentInlines.h"
#include "ElementInlines.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLSelectElement.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PopupMenu.h"
#include "RenderScrollbar.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "StyleResolver.h"
#include "TextRun.h"
#include <math.h>
#include <wtf/IsoMallocInlines.h>

#if PLATFORM(IOS_FAMILY)
#include "LocalizedStrings.h"
#include "RenderThemeIOS.h"
#endif

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderMenuList);

#if PLATFORM(IOS_FAMILY)
static size_t selectedOptionCount(const RenderMenuList& renderMenuList)
{
    const auto& listItems = renderMenuList.selectElement().listItems();
    size_t numberOfItems = listItems.size();

    size_t count = 0;
    for (size_t i = 0; i < numberOfItems; ++i) {
        if (is<HTMLOptionElement>(*listItems[i]) && downcast<HTMLOptionElement>(*listItems[i]).selected())
            ++count;
    }
    return count;
}
#endif

RenderMenuList::RenderMenuList(HTMLSelectElement& element, RenderStyle&& style)
    : RenderFlexibleBox(element, WTFMove(style))
    , m_needsOptionsWidthUpdate(true)
    , m_optionsWidth(0)
#if !PLATFORM(IOS_FAMILY)
    , m_popupIsVisible(false)
#endif
{
}

RenderMenuList::~RenderMenuList()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
}

void RenderMenuList::willBeDestroyed()
{
#if !PLATFORM(IOS_FAMILY)
    if (m_popup)
        m_popup->disconnectClient();
    m_popup = nullptr;
#endif

    RenderFlexibleBox::willBeDestroyed();
}

void RenderMenuList::setInnerRenderer(RenderBlock& innerRenderer)
{
    ASSERT(!m_innerBlock.get());
    m_innerBlock = innerRenderer;
    adjustInnerStyle();
}

void RenderMenuList::adjustInnerStyle()
{
    auto& innerStyle = m_innerBlock->mutableStyle();
    innerStyle.setFlexGrow(1);
    innerStyle.setFlexShrink(1);
    // min-width: 0; is needed for correct shrinking.
    innerStyle.setMinWidth(Length(0, LengthType::Fixed));
    // Use margin:auto instead of align-items:center to get safe centering, i.e.
    // when the content overflows, treat it the same as align-items: flex-start.
    // But we only do that for the cases where html.css would otherwise use center.
    if (style().alignItems().position() == ItemPosition::Center) {
        innerStyle.setMarginTop(Length());
        innerStyle.setMarginBottom(Length());
        innerStyle.setAlignSelfPosition(ItemPosition::FlexStart);
    }

    innerStyle.setPaddingBox(theme().popupInternalPaddingBox(style(), document().settings()));

    if (document().page()->chrome().selectItemWritingDirectionIsNatural()) {
        // Items in the popup will not respect the CSS text-align and direction properties,
        // so we must adjust our own style to match.
        innerStyle.setTextAlign(TextAlignMode::Left);
        TextDirection direction = (m_buttonText && m_buttonText->text().defaultWritingDirection() == U_RIGHT_TO_LEFT) ? TextDirection::RTL : TextDirection::LTR;
        innerStyle.setDirection(direction);
#if PLATFORM(IOS_FAMILY)
    } else if (document().page()->chrome().selectItemAlignmentFollowsMenuWritingDirection()) {
        innerStyle.setTextAlign(style().direction() == TextDirection::LTR ? TextAlignMode::Left : TextAlignMode::Right);
        TextDirection direction;
        UnicodeBidi unicodeBidi;
        if (multiple() && selectedOptionCount(*this) != 1) {
            direction = (m_buttonText && m_buttonText->text().defaultWritingDirection() == U_RIGHT_TO_LEFT) ? TextDirection::RTL : TextDirection::LTR;
            unicodeBidi = UnicodeBidi::Normal;
        } else if (m_optionStyle) {
            direction = m_optionStyle->direction();
            unicodeBidi = m_optionStyle->unicodeBidi();
        } else {
            direction = style().direction();
            unicodeBidi = style().unicodeBidi();
        }

        innerStyle.setDirection(direction);
        innerStyle.setUnicodeBidi(unicodeBidi);
    }
#else
    } else if (m_optionStyle && document().page()->chrome().selectItemAlignmentFollowsMenuWritingDirection()) {
        if ((m_optionStyle->direction() != innerStyle.direction() || m_optionStyle->unicodeBidi() != innerStyle.unicodeBidi()))
            m_innerBlock->setNeedsLayoutAndPrefWidthsRecalc();
        innerStyle.setTextAlign(style().isLeftToRightDirection() ? TextAlignMode::Left : TextAlignMode::Right);
        innerStyle.setDirection(m_optionStyle->direction());
        innerStyle.setUnicodeBidi(m_optionStyle->unicodeBidi());
    }
#endif // !PLATFORM(IOS_FAMILY)
}

HTMLSelectElement& RenderMenuList::selectElement() const
{
    return downcast<HTMLSelectElement>(nodeForNonAnonymous());
}

void RenderMenuList::didAttachChild(RenderObject& child, RenderObject*)
{
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->childrenChanged(this, &child);
}

void RenderMenuList::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);

    if (m_innerBlock) // RenderBlock handled updating the anonymous block's style.
        adjustInnerStyle();

    bool fontChanged = !oldStyle || oldStyle->fontCascade() != style().fontCascade();
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
        RefPtr element = listItems[i].get();
        if (!is<HTMLOptionElement>(*element))
            continue;

        String text = downcast<HTMLOptionElement>(*element).textIndentedToRespectGroupLabel();
        text = applyTextTransform(style(), text, ' ');
        if (theme().popupOptionSupportsTextIndent()) {
            // Add in the option's text indent.  We can't calculate percentage values for now.
            float optionWidth = 0;
            if (auto* optionStyle = element->computedStyle())
                optionWidth += minimumValueForLength(optionStyle->textIndent(), 0);
            if (!text.isEmpty()) {
                const FontCascade& font = style().fontCascade();
                TextRun run = RenderBlock::constructTextRun(text, style());
                optionWidth += font.width(run);
            }
            maxOptionWidth = std::max(maxOptionWidth, optionWidth);
        } else if (!text.isEmpty()) {
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
        setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderMenuList::updateFromElement()
{
    if (m_needsOptionsWidthUpdate) {
        updateOptionsWidth();
        m_needsOptionsWidthUpdate = false;
    }

#if !PLATFORM(IOS_FAMILY)
    if (m_popupIsVisible)
        m_popup->updateFromElement();
    else
#endif
        setTextFromOption(selectElement().selectedIndex());
}

void RenderMenuList::setTextFromOption(int optionIndex)
{
    const auto& listItems = selectElement().listItems();
    int size = listItems.size();

    int i = selectElement().optionToListIndex(optionIndex);
    String text = emptyString();
    if (i >= 0 && i < size) {
        RefPtr element = listItems[i].get();
        if (is<HTMLOptionElement>(*element)) {
            text = downcast<HTMLOptionElement>(*element).textIndentedToRespectGroupLabel();
            auto* style = element->computedStyle();
            m_optionStyle = style ? RenderStyle::clonePtr(*style) : nullptr;
        }
    }

#if PLATFORM(IOS_FAMILY)
    if (multiple()) {
        size_t count = selectedOptionCount(*this);
        if (count != 1)
            text = htmlSelectMultipleItems(count);
    }
#endif

    setText(text.stripWhiteSpace());
    didUpdateActiveOption(optionIndex);
}

void RenderMenuList::setText(const String& s)
{
    String textToUse = s.isEmpty() ? "\n"_str : s;

    if (m_buttonText) {
        m_buttonText->setText(textToUse.impl(), true);
        m_buttonText->dirtyLineBoxes(false);
    } else {
        auto newButtonText = createRenderer<RenderText>(document(), textToUse);
        m_buttonText = *newButtonText;
        // FIXME: This mutation should go through the normal RenderTreeBuilder path.
        if (RenderTreeBuilder::current())
            RenderTreeBuilder::current()->attach(*this, WTFMove(newButtonText));
        else
            RenderTreeBuilder(*document().renderView()).attach(*this, WTFMove(newButtonText));
    }

    adjustInnerStyle();
}

String RenderMenuList::text() const
{
    return m_buttonText ? m_buttonText->text() : String();
}

LayoutRect RenderMenuList::controlClipRect(const LayoutPoint& additionalOffset) const
{
    // Clip to the intersection of the content box and the content box for the inner box
    // This will leave room for the arrows which sit in the inner box padding,
    // and if the inner box ever spills out of the outer box, that will get clipped too.
    LayoutRect outerBox(additionalOffset.x() + borderLeft() + paddingLeft(), 
                   additionalOffset.y() + borderTop() + paddingTop(),
                   contentWidth(), 
                   contentHeight());
    
    LayoutRect innerBox(additionalOffset.x() + m_innerBlock->x() + m_innerBlock->paddingLeft(), 
                   additionalOffset.y() + m_innerBlock->y() + m_innerBlock->paddingTop(),
                   m_innerBlock->contentWidth(), 
                   m_innerBlock->contentHeight());

    return intersection(outerBox, innerBox);
}

void RenderMenuList::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    maxLogicalWidth = shouldApplySizeContainment() ? theme().minimumMenuListSize(style()) : std::max(m_optionsWidth, theme().minimumMenuListSize(style()));
    maxLogicalWidth += m_innerBlock->paddingLeft() + m_innerBlock->paddingRight();
    if (shouldApplySizeContainment()) {
        if (auto width = explicitIntrinsicInnerLogicalWidth())
            maxLogicalWidth = width.value();
    }
    if (!style().width().isPercentOrCalculated())
        minLogicalWidth = maxLogicalWidth;
}

void RenderMenuList::computePreferredLogicalWidths()
{
    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;
    
    if (style().width().isFixed() && style().width().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(style().width());
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    RenderBox::computePreferredLogicalWidths(style().minWidth(), style().maxWidth(), horizontalBorderAndPaddingExtent());

    setPreferredLogicalWidthsDirty(false);
}

#if PLATFORM(IOS_FAMILY)
NO_RETURN_DUE_TO_ASSERT
void RenderMenuList::showPopup()
{
    ASSERT_NOT_REACHED();
}
#else
void RenderMenuList::showPopup()
{
    if (m_popupIsVisible)
        return;

    ASSERT(m_innerBlock);
    if (!m_popup)
        m_popup = document().page()->chrome().createPopupMenu(*this);
    m_popupIsVisible = true;

    // Compute the top left taking transforms into account, but use
    // the actual width of the element to size the popup.
    FloatPoint absTopLeft = localToAbsolute(FloatPoint(), UseTransforms);
    IntRect absBounds = absoluteBoundingBoxRectIgnoringTransforms();
    absBounds.setLocation(roundedIntPoint(absTopLeft));
    m_popup->show(absBounds, &view().frameView(), selectElement().optionToListIndex(selectElement().selectedIndex()));
}
#endif

void RenderMenuList::hidePopup()
{
#if !PLATFORM(IOS_FAMILY)
    if (m_popup)
        m_popup->hide();
#endif
}

void RenderMenuList::valueChanged(unsigned listIndex, bool fireOnChange)
{
    // Check to ensure a page navigation has not occurred while
    // the popup was up.
    if (&document() != document().frame()->document())
        return;

    selectElement().optionSelectedByUser(selectElement().listToOptionIndex(listIndex), fireOnChange);
}

void RenderMenuList::listBoxSelectItem(int listIndex, bool allowMultiplySelections, bool shift, bool fireOnChangeNow)
{
    selectElement().listBoxSelectItem(listIndex, allowMultiplySelections, shift, fireOnChangeNow);
}

bool RenderMenuList::multiple() const
{
    return selectElement().multiple();
}

void RenderMenuList::didSetSelectedIndex(int listIndex)
{
    didUpdateActiveOption(selectElement().listToOptionIndex(listIndex));
}

void RenderMenuList::didUpdateActiveOption(int optionIndex)
{
    if (!AXObjectCache::accessibilityEnabled())
        return;

    auto* axCache = document().existingAXObjectCache();
    if (!axCache)
        return;

    if (m_lastActiveIndex == optionIndex)
        return;
    m_lastActiveIndex = optionIndex;

    int listIndex = selectElement().optionToListIndex(optionIndex);
    if (listIndex < 0 || listIndex >= static_cast<int>(selectElement().listItems().size()))
        return;

    auto* axObject = axCache->get(this);
    if (is<AccessibilityMenuList>(axObject))
        downcast<AccessibilityMenuList>(*axObject).didUpdateActiveOption(optionIndex);
}

String RenderMenuList::itemText(unsigned listIndex) const
{
    auto& listItems = selectElement().listItems();
    if (listIndex >= listItems.size())
        return String();

    String itemString;
    auto& element = *listItems[listIndex];
    if (is<HTMLOptGroupElement>(element))
        itemString = downcast<HTMLOptGroupElement>(element).groupLabelText();
    else if (is<HTMLOptionElement>(element))
        itemString = downcast<HTMLOptionElement>(element).textIndentedToRespectGroupLabel();

    return applyTextTransform(style(), itemString, ' ');
}

String RenderMenuList::itemLabel(unsigned) const
{
    return String();
}

String RenderMenuList::itemIcon(unsigned) const
{
    return String();
}

String RenderMenuList::itemAccessibilityText(unsigned listIndex) const
{
    // Allow the accessible name be changed if necessary.
    const auto& listItems = selectElement().listItems();
    if (listIndex >= listItems.size())
        return String();
    return listItems[listIndex]->attributeWithoutSynchronization(aria_labelAttr);
}
    
String RenderMenuList::itemToolTip(unsigned listIndex) const
{
    const auto& listItems = selectElement().listItems();
    if (listIndex >= listItems.size())
        return String();
    return listItems[listIndex]->title();
}

bool RenderMenuList::itemIsEnabled(unsigned listIndex) const
{
    const auto& listItems = selectElement().listItems();
    if (listIndex >= listItems.size())
        return false;
    HTMLElement* element = listItems[listIndex].get();
    if (!is<HTMLOptionElement>(*element))
        return false;

    bool groupEnabled = true;
    if (Element* parentElement = element->parentElement()) {
        if (is<HTMLOptGroupElement>(*parentElement))
            groupEnabled = !parentElement->isDisabledFormControl();
    }
    if (!groupEnabled)
        return false;

    return !element->isDisabledFormControl();
}

PopupMenuStyle RenderMenuList::itemStyle(unsigned listIndex) const
{
    const auto& listItems = selectElement().listItems();
    if (listIndex >= listItems.size()) {
        // If we are making an out of bounds access, then we want to use the style
        // of a different option element (index 0). However, if there isn't an option element
        // before at index 0, we fall back to the menu's style.
        if (!listIndex)
            return menuStyle();

        // Try to retrieve the style of an option element we know exists (index 0).
        listIndex = 0;
    }
    HTMLElement* element = listItems[listIndex].get();

    Color itemBackgroundColor;
    bool itemHasCustomBackgroundColor;
    getItemBackgroundColor(listIndex, itemBackgroundColor, itemHasCustomBackgroundColor);

    auto& style = *element->computedStyle();
    return PopupMenuStyle(style.visitedDependentColorWithColorFilter(CSSPropertyColor), itemBackgroundColor, style.fontCascade(), style.visibility() == Visibility::Visible,
        style.display() == DisplayType::None, true, style.textIndent(), style.direction(), isOverride(style.unicodeBidi()),
        itemHasCustomBackgroundColor ? PopupMenuStyle::CustomBackgroundColor : PopupMenuStyle::DefaultBackgroundColor);
}

void RenderMenuList::getItemBackgroundColor(unsigned listIndex, Color& itemBackgroundColor, bool& itemHasCustomBackgroundColor) const
{
    const auto& listItems = selectElement().listItems();
    if (listIndex >= listItems.size()) {
        itemBackgroundColor = style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
        itemHasCustomBackgroundColor = false;
        return;
    }
    HTMLElement* element = listItems[listIndex].get();

    Color backgroundColor = element->computedStyle()->visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
    itemHasCustomBackgroundColor = backgroundColor.isValid() && backgroundColor.isVisible();
    // If the item has an opaque background color, return that.
    if (backgroundColor.isOpaque()) {
        itemBackgroundColor = backgroundColor;
        return;
    }

    // Otherwise, the item's background is overlayed on top of the menu background.
    backgroundColor = blendSourceOver(style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor), backgroundColor);
    if (backgroundColor.isOpaque()) {
        itemBackgroundColor = backgroundColor;
        return;
    }

    // If the menu background is not opaque, then add an opaque white background behind.
    itemBackgroundColor = blendSourceOver(Color::white, backgroundColor);
}

PopupMenuStyle RenderMenuList::menuStyle() const
{
    const RenderStyle& styleToUse = m_innerBlock ? m_innerBlock->style() : style();
    IntRect absBounds = absoluteBoundingBoxRectIgnoringTransforms();
    return PopupMenuStyle(styleToUse.visitedDependentColorWithColorFilter(CSSPropertyColor), styleToUse.visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor),
        styleToUse.fontCascade(), styleToUse.visibility() == Visibility::Visible, styleToUse.display() == DisplayType::None,
        style().hasEffectiveAppearance() && style().effectiveAppearance() == ControlPartType::Menulist, styleToUse.textIndent(),
        style().direction(), isOverride(style().unicodeBidi()), PopupMenuStyle::DefaultBackgroundColor,
        PopupMenuStyle::SelectPopup, theme().popupMenuSize(styleToUse, absBounds));
}

HostWindow* RenderMenuList::hostWindow() const
{
    return RenderFlexibleBox::hostWindow();
}

Ref<Scrollbar> RenderMenuList::createScrollbar(ScrollableArea& scrollableArea, ScrollbarOrientation orientation, ScrollbarControlSize controlSize)
{
    bool hasCustomScrollbarStyle = style().hasPseudoStyle(PseudoId::Scrollbar);
    if (hasCustomScrollbarStyle)
        return RenderScrollbar::createCustomScrollbar(scrollableArea, orientation, &selectElement());
    return Scrollbar::createNativeScrollbar(scrollableArea, orientation, controlSize);
}

int RenderMenuList::clientInsetLeft() const
{
    return 0;
}

int RenderMenuList::clientInsetRight() const
{
    return 0;
}

const int endOfLinePadding = 2;

LayoutUnit RenderMenuList::clientPaddingLeft() const
{
    if ((style().effectiveAppearance() == ControlPartType::Menulist || style().effectiveAppearance() == ControlPartType::MenulistButton) && style().direction() == TextDirection::RTL) {
        // For these appearance values, the theme applies padding to leave room for the
        // drop-down button. But leaving room for the button inside the popup menu itself
        // looks strange, so we return a small default padding to avoid having a large empty
        // space appear on the side of the popup menu.
        return endOfLinePadding;
    }
    // If the appearance isn't MenulistPart, then the select is styled (non-native), so
    // we want to return the user specified padding.
    return paddingLeft() + m_innerBlock->paddingLeft();
}

LayoutUnit RenderMenuList::clientPaddingRight() const
{
    if ((style().effectiveAppearance() == ControlPartType::Menulist || style().effectiveAppearance() == ControlPartType::MenulistButton) && style().direction() == TextDirection::LTR)
        return endOfLinePadding;

    return paddingRight() + m_innerBlock->paddingRight();
}

int RenderMenuList::listSize() const
{
    return selectElement().listItems().size();
}

int RenderMenuList::selectedIndex() const
{
    return selectElement().optionToListIndex(selectElement().selectedIndex());
}

void RenderMenuList::popupDidHide()
{
#if !PLATFORM(IOS_FAMILY)
    // PopupMenuMac::show in WebKitLegacy can call this callback even when popup had already been dismissed.
    m_popupIsVisible = false;
#endif
}

bool RenderMenuList::itemIsSeparator(unsigned listIndex) const
{
    const auto& listItems = selectElement().listItems();
    return listIndex < listItems.size() && listItems[listIndex]->hasTagName(hrTag);
}

bool RenderMenuList::itemIsLabel(unsigned listIndex) const
{
    const auto& listItems = selectElement().listItems();
    return listIndex < listItems.size() && is<HTMLOptGroupElement>(*listItems[listIndex]);
}

bool RenderMenuList::itemIsSelected(unsigned listIndex) const
{
    const auto& listItems = selectElement().listItems();
    if (listIndex >= listItems.size())
        return false;
    HTMLElement* element = listItems[listIndex].get();
    return is<HTMLOptionElement>(*element) && downcast<HTMLOptionElement>(*element).selected();
}

void RenderMenuList::setTextFromItem(unsigned listIndex)
{
    setTextFromOption(selectElement().listToOptionIndex(listIndex));
}

FontSelector* RenderMenuList::fontSelector() const
{
    return &document().fontSelector();
}

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
