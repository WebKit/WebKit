/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AXCoreObject.h"

namespace WebCore {

bool AXCoreObject::isMenuRelated() const
{
    switch (roleValue()) {
    case AccessibilityRole::Menu:
    case AccessibilityRole::MenuBar:
    case AccessibilityRole::MenuButton:
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
        return true;
    default:
        return false;
    }
}

bool AXCoreObject::isMenuItem() const
{
    switch (roleValue()) {
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::MenuItemCheckbox:
        return true;
    default:
        return false;
    }
}

bool AXCoreObject::isLandmark() const
{
    switch (roleValue()) {
    case AccessibilityRole::LandmarkBanner:
    case AccessibilityRole::LandmarkComplementary:
    case AccessibilityRole::LandmarkContentInfo:
    case AccessibilityRole::LandmarkDocRegion:
    case AccessibilityRole::LandmarkMain:
    case AccessibilityRole::LandmarkNavigation:
    case AccessibilityRole::LandmarkRegion:
    case AccessibilityRole::LandmarkSearch:
        return true;
    default:
        return false;
    }
}

bool AXCoreObject::isGroup() const
{
    switch (roleValue()) {
    case AccessibilityRole::Group:
    case AccessibilityRole::TextGroup:
    case AccessibilityRole::ApplicationGroup:
    case AccessibilityRole::ApplicationTextGroup:
        return true;
    default:
        return false;
    }
}

bool AXCoreObject::isButton() const
{
    switch (roleValue()) {
    case AccessibilityRole::Button:
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::ToggleButton:
        return true;
    default:
        return false;
    }
}

bool AXCoreObject::isTextControl() const
{
    switch (roleValue()) {
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::SearchField:
    case AccessibilityRole::TextArea:
    case AccessibilityRole::TextField:
        return true;
    default:
        return false;
    }
}

bool AXCoreObject::canHaveSelectedChildren() const
{
    switch (roleValue()) {
    // These roles are containers whose children support aria-selected:
    case AccessibilityRole::Grid:
    case AccessibilityRole::ListBox:
    case AccessibilityRole::TabList:
    case AccessibilityRole::Tree:
    case AccessibilityRole::TreeGrid:
    case AccessibilityRole::List:
    // These roles are containers whose children are treated as selected by assistive
    // technologies. We can get the "selected" item via aria-activedescendant or the
    // focused element.
    case AccessibilityRole::Menu:
    case AccessibilityRole::MenuBar:
#if USE(ATSPI)
    case AccessibilityRole::MenuListPopup:
#endif
        return true;
    default:
        return false;
    }
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::tabChildren()
{
    if (roleValue() != AccessibilityRole::TabList)
        return { };

    AXCoreObject::AccessibilityChildrenVector result;
    for (const auto& child : children()) {
        if (child->isTabItem())
            result.append(child);
    }
    return result;
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::contents()
{
    if (isTabList())
        return tabChildren();

    if (isScrollView()) {
        // A scroll view's contents are everything except the scroll bars.
        AccessibilityChildrenVector nonScrollbarChildren;
        for (const auto& child : children()) {
            if (child && !child->isScrollbar())
                nonScrollbarChildren.append(child);
        }
        return nonScrollbarChildren;
    }
    return { };
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::ariaTreeItemContent()
{
    AccessibilityChildrenVector result;
    // The content of a treeitem excludes other treeitems or their containing groups.
    for (const auto& child : children()) {
        if (!child->isGroup() && child->roleValue() != AccessibilityRole::TreeItem)
            result.append(child);
    }
    return result;
}

String AXCoreObject::currentValue() const
{
    switch (currentState()) {
    case AccessibilityCurrentState::False:
        return "false"_s;
    case AccessibilityCurrentState::Page:
        return "page"_s;
    case AccessibilityCurrentState::Step:
        return "step"_s;
    case AccessibilityCurrentState::Location:
        return "location"_s;
    case AccessibilityCurrentState::Time:
        return "time"_s;
    case AccessibilityCurrentState::Date:
        return "date"_s;
    default:
    case AccessibilityCurrentState::True:
        return "true"_s;
    }
}

AXCoreObject::AXValue AXCoreObject::value()
{
    if (supportsRangeValue())
        return valueForRange();

    if (roleValue() == AccessibilityRole::SliderThumb)
        return parentObject()->valueForRange();

    if (isHeading())
        return headingLevel();

    if (supportsCheckedState())
        return checkboxOrRadioValue();

    if (roleValue() == AccessibilityRole::Summary)
        return isExpanded();

    // Radio groups return the selected radio button as the AXValue.
    if (isRadioGroup())
        return selectedRadioButton();

    if (isTabList())
        return selectedTabItem();

    if (isTabItem())
        return isSelected();

    if (isColorWell()) {
        auto color = convertColor<SRGBA<float>>(colorValue()).resolved();
        return makeString("rgb ", String::numberToStringFixedPrecision(color.red, 6, TrailingZerosPolicy::Keep), " ", String::numberToStringFixedPrecision(color.green, 6, TrailingZerosPolicy::Keep), " ", String::numberToStringFixedPrecision(color.blue, 6, TrailingZerosPolicy::Keep), " 1");
    }

    return stringValue();
}

AXCoreObject* AXCoreObject::selectedRadioButton()
{
    if (!isRadioGroup())
        return nullptr;

    // Find the child radio button that is selected (ie. the intValue == 1).
    for (const auto& child : children()) {
        if (child->roleValue() == AccessibilityRole::RadioButton && child->checkboxOrRadioValue() == AccessibilityButtonState::On)
            return child.get();
    }
    return nullptr;
}

AXCoreObject* AXCoreObject::selectedTabItem()
{
    if (!isTabList())
        return nullptr;

    // FIXME: Is this valid? ARIA tab items support aria-selected; not aria-checked.
    // Find the child tab item that is selected (ie. the intValue == 1).
    for (const auto& child : children()) {
        if (child->isTabItem() && (child->isChecked() || child->isSelected()))
            return child.get();
    }
    return nullptr;
}

bool AXCoreObject::hasPopup() const
{
    if (!equalLettersIgnoringASCIICase(popupValue(), "false"_s))
        return true;

    for (auto* ancestor = parentObject(); ancestor; ancestor = ancestor->parentObject()) {
        if (!ancestor->isLink())
            continue;

        if (!equalLettersIgnoringASCIICase(ancestor->popupValue(), "false"_s))
            return true;
    }
    return false;
}

unsigned AXCoreObject::tableLevel() const
{
    if (!isTable())
        return 0;

    unsigned level = 0;
    auto* current = exposedTableAncestor(true /* includeSelf */);
    while (current) {
        level++;
        current = current->exposedTableAncestor(false);
    }
    return level;
}

bool AXCoreObject::isTableCellInSameRowGroup(AXCoreObject* otherTableCell)
{
    if (!otherTableCell)
        return false;

    AXID ancestorID = rowGroupAncestorID();
    return ancestorID.isValid() && ancestorID == otherTableCell->rowGroupAncestorID();
}

bool AXCoreObject::isTableCellInSameColGroup(AXCoreObject* tableCell)
{
    if (!tableCell)
        return false;

    auto columnRange = columnIndexRange();
    auto otherColumnRange = tableCell->columnIndexRange();

    return columnRange.first <= otherColumnRange.first + otherColumnRange.second;
}

String AXCoreObject::ariaLandmarkRoleDescription() const
{
    switch (roleValue()) {
    case AccessibilityRole::LandmarkBanner:
        return AXARIAContentGroupText("ARIALandmarkBanner"_s);
    case AccessibilityRole::LandmarkComplementary:
        return AXARIAContentGroupText("ARIALandmarkComplementary"_s);
    case AccessibilityRole::LandmarkContentInfo:
        return AXARIAContentGroupText("ARIALandmarkContentInfo"_s);
    case AccessibilityRole::LandmarkMain:
        return AXARIAContentGroupText("ARIALandmarkMain"_s);
    case AccessibilityRole::LandmarkNavigation:
        return AXARIAContentGroupText("ARIALandmarkNavigation"_s);
    case AccessibilityRole::LandmarkDocRegion:
    case AccessibilityRole::LandmarkRegion:
        return AXARIAContentGroupText("ARIALandmarkRegion"_s);
    case AccessibilityRole::LandmarkSearch:
        return AXARIAContentGroupText("ARIALandmarkSearch"_s);
    case AccessibilityRole::ApplicationAlert:
        return AXARIAContentGroupText("ARIAApplicationAlert"_s);
    case AccessibilityRole::ApplicationAlertDialog:
        return AXARIAContentGroupText("ARIAApplicationAlertDialog"_s);
    case AccessibilityRole::ApplicationDialog:
        return AXARIAContentGroupText("ARIAApplicationDialog"_s);
    case AccessibilityRole::ApplicationLog:
        return AXARIAContentGroupText("ARIAApplicationLog"_s);
    case AccessibilityRole::ApplicationMarquee:
        return AXARIAContentGroupText("ARIAApplicationMarquee"_s);
    case AccessibilityRole::ApplicationStatus:
        return AXARIAContentGroupText("ARIAApplicationStatus"_s);
    case AccessibilityRole::ApplicationTimer:
        return AXARIAContentGroupText("ARIAApplicationTimer"_s);
    case AccessibilityRole::Document:
        return AXARIAContentGroupText("ARIADocument"_s);
    case AccessibilityRole::DocumentArticle:
        return AXARIAContentGroupText("ARIADocumentArticle"_s);
    case AccessibilityRole::DocumentMath:
        return AXARIAContentGroupText("ARIADocumentMath"_s);
    case AccessibilityRole::DocumentNote:
        return AXARIAContentGroupText("ARIADocumentNote"_s);
    case AccessibilityRole::UserInterfaceTooltip:
        return AXARIAContentGroupText("ARIAUserInterfaceTooltip"_s);
    case AccessibilityRole::TabPanel:
        return AXARIAContentGroupText("ARIATabPanel"_s);
    case AccessibilityRole::WebApplication:
        return AXARIAContentGroupText("ARIAWebApplication"_s);
    default:
        return { };
    }
}

AXCoreObject* AXCoreObject::activeDescendant() const
{
    auto activeDescendants = relatedObjects(AXRelationType::ActiveDescendant);
    ASSERT(activeDescendants.size() <= 1);
    return activeDescendants.size() ? activeDescendants[0].get() : nullptr;
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::selectedCells()
{
    if (!isTable())
        return { };

    AXCoreObject::AccessibilityChildrenVector selectedCells;
    for (auto& cell : cells()) {
        if (cell->isSelected())
            selectedCells.append(cell);
    }

    if (auto* activeDescendant = this->activeDescendant()) {
        if (activeDescendant->isExposedTableCell() && !selectedCells.contains(activeDescendant))
            selectedCells.append(activeDescendant);
    }
    return selectedCells;
}

} // namespace WebCore
