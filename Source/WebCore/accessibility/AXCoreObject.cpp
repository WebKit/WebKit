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
#include "LocalFrameView.h"

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
    if (isNonNativeTextControl())
        return true;

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

#if PLATFORM(COCOA)
static bool isVisibleText(AccessibilityTextSource textSource)
{
    switch (textSource) {
    case AccessibilityTextSource::Visible:
    case AccessibilityTextSource::Children:
    case AccessibilityTextSource::LabelByElement:
        return true;
    case AccessibilityTextSource::Alternative:
    case AccessibilityTextSource::Summary:
    case AccessibilityTextSource::Help:
    case AccessibilityTextSource::TitleTag:
    case AccessibilityTextSource::Placeholder:
    case AccessibilityTextSource::Title:
    case AccessibilityTextSource::Subtitle:
    case AccessibilityTextSource::Action:
        return false;
    }
}

static bool isDescriptiveText(AccessibilityTextSource textSource)
{
    switch (textSource) {
    case AccessibilityTextSource::Alternative:
    case AccessibilityTextSource::Visible:
    case AccessibilityTextSource::Children:
    case AccessibilityTextSource::LabelByElement:
        return true;
    case AccessibilityTextSource::Summary:
    case AccessibilityTextSource::Help:
    case AccessibilityTextSource::TitleTag:
    case AccessibilityTextSource::Placeholder:
    case AccessibilityTextSource::Title:
    case AccessibilityTextSource::Subtitle:
    case AccessibilityTextSource::Action:
        return false;
    }
}

String AXCoreObject::descriptionAttributeValue() const
{
    if (!shouldComputeDescriptionAttributeValue())
        return { };

    Vector<AccessibilityText> textOrder;
    accessibilityText(textOrder);

    // Determine if any visible text is available, which influences our usage of title tag.
    bool visibleTextAvailable = false;
    for (const auto& text : textOrder) {
        if (isVisibleText(text.textSource) && !text.text.isEmpty()) {
            visibleTextAvailable = true;
            break;
        }
    }

    StringBuilder returnText;
    for (const auto& text : textOrder) {
        if (text.textSource == AccessibilityTextSource::Alternative) {
            returnText.append(text.text);
            break;
        }

        switch (text.textSource) {
        // These are sub-components of one element (Attachment) that are re-combined in OSX and iOS.
        case AccessibilityTextSource::Title:
        case AccessibilityTextSource::Subtitle:
        case AccessibilityTextSource::Action: {
            if (!text.text.length())
                break;
            if (returnText.length())
                returnText.append(", "_s);
            returnText.append(text.text);
            break;
        }
        default:
            break;
        }

        if (text.textSource == AccessibilityTextSource::TitleTag && !visibleTextAvailable) {
            returnText.append(text.text);
            break;
        }
    }

    return returnText.toString();
}

String AXCoreObject::titleAttributeValue() const
{
    // Meter elements should communicate their content via AXValueDescription.
    if (!shouldComputeTitleAttributeValue() || isMeter())
        return { };

    // A file upload button presents a challenge because it has button text and a value, but the
    // API doesn't support this paradigm.
    // The compromise is to return the button type in the role description and the value of the file path in the title
    if (isFileUploadButton() && fileUploadButtonReturnsValueInTitle())
        return stringValue();

    Vector<AccessibilityText> textOrder;
    accessibilityText(textOrder);

    for (const auto& text : textOrder) {
        // If we have alternative text, then we should not expose a title.
        if (text.textSource == AccessibilityTextSource::Alternative)
            break;

        // Once we encounter visible text, or the text from our children that should be used foremost.
        if (text.textSource == AccessibilityTextSource::Visible || text.textSource == AccessibilityTextSource::Children)
            return text.text;

        // If there's an element that labels this object and it's not exposed, then we should use
        // that text as our title.
        if (text.textSource == AccessibilityTextSource::LabelByElement)
            return text.text;
    }

    return { };
}

String AXCoreObject::helpTextAttributeValue() const
{
    Vector<AccessibilityText> textOrder;
    accessibilityText(textOrder);

    // Determine if any descriptive text is available, which influences our usage of title tag.
    bool descriptiveTextAvailable = false;
    for (const auto& text : textOrder) {
        if (isDescriptiveText(text.textSource) && !text.text.isEmpty()) {
            descriptiveTextAvailable = true;
            break;
        }
    }

    for (const auto& text : textOrder) {
        if (text.textSource == AccessibilityTextSource::Help || text.textSource == AccessibilityTextSource::Summary)
            return text.text;

        // If an element does NOT have other descriptive text the title tag should be used as its descriptive text.
        // But, if those ARE available, then the title tag should be used for help text instead.
        if (text.textSource == AccessibilityTextSource::TitleTag && descriptiveTextAvailable)
            return text.text;
    }

    return { };
}
#endif // PLATFORM(COCOA)

AXCoreObject* AXCoreObject::titleUIElement() const
{
    auto labels = relatedObjects(AXRelationType::LabeledBy);
#if PLATFORM(COCOA)
    // We impose the restriction that if there is more than one label, then we should return none.
    // FIXME: the behavior should be the same in all platforms.
    return labels.size() == 1 ? labels.first().get() : nullptr;
#else
    return labels.size() ? labels.first().get() : nullptr;
#endif
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::linkedObjects() const
{
    auto linkedObjects = flowToObjects();

    if (isLink()) {
        if (auto* linkedAXElement = internalLinkElement())
            linkedObjects.append(linkedAXElement);
    } else if (isRadioButton())
        appendRadioButtonGroupMembers(linkedObjects);

    linkedObjects.appendVector(controlledObjects());
    linkedObjects.appendVector(ownedObjects());

    return linkedObjects;
}

void AXCoreObject::appendRadioButtonDescendants(AXCoreObject& parent, AccessibilityChildrenVector& linkedUIElements) const
{
    for (const auto& child : parent.children()) {
        if (child->isRadioButton())
            linkedUIElements.append(child);
        else
            appendRadioButtonDescendants(*child, linkedUIElements);
    }
}

void AXCoreObject::appendRadioButtonGroupMembers(AccessibilityChildrenVector& linkedUIElements) const
{
    if (!isRadioButton())
        return;

    if (isRadioInput()) {
        for (auto& radioSibling : radioButtonGroup())
            linkedUIElements.append(radioSibling);
    } else {
        // If we didn't find any radio button siblings with the traditional naming, lets search for a radio group role and find its children.
        for (auto* parent = parentObject(); parent; parent = parent->parentObject()) {
            if (parent->roleValue() == AccessibilityRole::RadioGroup) {
                appendRadioButtonDescendants(*parent, linkedUIElements);
                break;
            }
        }
    }
}

namespace Accessibility {

bool inRenderTreeOrStyleUpdate(const Document& document)
{
    if (document.inStyleRecalc() || document.inRenderTreeUpdate())
        return true;
    auto* view = document.view();
    return view && view->layoutContext().isInRenderTreeLayout();
}

} // namespace Accessibility

} // namespace WebCore
