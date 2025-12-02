/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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

#include "AXUtilities.h"
#include "DocumentView.h"
#include "HTMLAreaElement.h"
#include "LocalFrameView.h"
#include "RenderObjectStyle.h"
#include "RenderStyleInlines.h"
#include "Settings.h"
#include "TextDecorationPainter.h"
#include <wtf/Deque.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

bool AXCoreObject::isList() const
{
    auto role = this->role();
    return role == AccessibilityRole::List || role == AccessibilityRole::DescriptionList;
}

bool AXCoreObject::isFileUploadButton() const
{
    std::optional type = inputType();
    return type ? *type == InputType::Type::File : false;
}

bool AXCoreObject::isMenuRelated() const
{
    switch (role()) {
    case AccessibilityRole::Menu:
    case AccessibilityRole::MenuBar:
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
    switch (role()) {
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::MenuItemCheckbox:
        return true;
    default:
        return false;
    }
}

bool AXCoreObject::isInputImage() const
{
    if (role() != AccessibilityRole::Button)
        return false;

    std::optional type = inputType();
    return type ?  *type == InputType::Type::Image : false;
}

bool AXCoreObject::isControl() const
{
    switch (role()) {
    case AccessibilityRole::Button:
    case AccessibilityRole::Checkbox:
    case AccessibilityRole::ColorWell:
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::DateTime:
    case AccessibilityRole::LandmarkSearch:
    case AccessibilityRole::ListBox:
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::SearchField:
    case AccessibilityRole::Slider:
    case AccessibilityRole::SliderThumb:
    case AccessibilityRole::Switch:
    case AccessibilityRole::TextArea:
    case AccessibilityRole::TextField:
    case AccessibilityRole::ToggleButton:
        return true;
    default:
        return isFieldset();
    }
}

bool AXCoreObject::isImplicitlyInteractive() const
{
    switch (role()) {
    case AccessibilityRole::Button:
    case AccessibilityRole::Checkbox:
    case AccessibilityRole::ColorWell:
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::DateTime:
    case AccessibilityRole::Details:
    case AccessibilityRole::LandmarkSearch:
    case AccessibilityRole::Link:
    case AccessibilityRole::ListBox:
    case AccessibilityRole::ListBoxOption:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::MenuListOption:
    case AccessibilityRole::MenuListPopup:
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::SearchField:
    case AccessibilityRole::Slider:
    case AccessibilityRole::SliderThumb:
    case AccessibilityRole::SpinButton:
    case AccessibilityRole::SpinButtonPart:
    case AccessibilityRole::Switch:
    case AccessibilityRole::Tab:
    case AccessibilityRole::TextArea:
    case AccessibilityRole::TextField:
    case AccessibilityRole::ToggleButton:
        return true;
    default:
        return false;
    }
}

bool AXCoreObject::isLandmark() const
{
    switch (role()) {
    case AccessibilityRole::Form:
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
    switch (role()) {
    case AccessibilityRole::Group:
    case AccessibilityRole::TextGroup:
        return true;
    default:
        return false;
    }
}

bool AXCoreObject::isImageMapLink() const
{
    RefPtr element = this->element();
    return element && is<HTMLAreaElement>(*element);
}

bool AXCoreObject::hasHighlighting() const
{
    for (RefPtr ancestor = this; ancestor; ancestor = ancestor->parentObject()) {
        if (ancestor->hasMarkTag())
            return true;
    }
    return false;
}

bool AXCoreObject::hasGridRole() const
{
    auto role = this->role();
    return role == AccessibilityRole::Grid || role == AccessibilityRole::TreeGrid;
}

bool AXCoreObject::hasCellRole() const
{
    auto role = this->role();
    return role == AccessibilityRole::Cell || role == AccessibilityRole::GridCell || role == AccessibilityRole::ColumnHeader || role == AccessibilityRole::RowHeader;
}

bool AXCoreObject::hasCellOrRowRole() const
{
    return hasCellRole() || role() == AccessibilityRole::Row;
}

bool AXCoreObject::isButton() const
{
    switch (role()) {
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
    switch (role()) {
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::SearchField:
    case AccessibilityRole::TextArea:
    case AccessibilityRole::TextField:
        return true;
    default:
        return false;
    }
}

ListBoxInterpretation AXCoreObject::listBoxInterpretation() const
{
    if (role() != AccessibilityRole::ListBox)
        return ListBoxInterpretation::NotListBox;

    Deque<Ref<AXCoreObject>, /* inlineCapacity */ 100> queue;
    for (Ref child : const_cast<AXCoreObject*>(this)->childrenIncludingIgnored())
        queue.append(WTFMove(child));

    unsigned iterations = 0;
    bool foundListItem = false;
    while (!queue.isEmpty()) {
        Ref current = queue.takeFirst();

        // Technically, per ARIA, the only valid children of listboxes are options, or groups containing options.
        // But be permissive and call this listbox valid if it has at least one option.
        if (current->isListBoxOption())
            return ListBoxInterpretation::ActuallyListBox;
        if (current->isListItem())
            foundListItem = true;

        // If we've checked 10 children and found a list item but no options, treat it as a static list.
        if (iterations > 10 && foundListItem)
            return ListBoxInterpretation::ActuallyStaticList;

        // Don't iterate forever in case someone added role="listbox" to some high-level element.
        // If we haven't found an option after checking 200 objects, this probably isn't valid anyways.
        if (iterations >= 250)
            break;
        ++iterations;

        if (current->isGroup() || current->isIgnored()) {
            for (Ref child : current->childrenIncludingIgnored())
                queue.append(WTFMove(child));
        }
    }
    return foundListItem ? ListBoxInterpretation::ActuallyStaticList : ListBoxInterpretation::InvalidListBox;
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::tabChildren()
{
    if (role() != AccessibilityRole::TabList)
        return { };

    AXCoreObject::AccessibilityChildrenVector result;
    for (const auto& child : unignoredChildren()) {
        if (child->isTabItem())
            result.append(child);
    }
    return result;
}

#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
static bool isValidChildForTable(AXCoreObject& object)
{
    auto role = object.role();
    // Tables can only have these roles as exposed-to-AT children.
    return role == AccessibilityRole::Row || role == AccessibilityRole::Column || role == AccessibilityRole::TableHeaderContainer || role == AccessibilityRole::Caption;
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::unignoredChildren(bool updateChildrenIfNeeded)
{
    if (onlyAddsUnignoredChildren())
        return children(updateChildrenIfNeeded);

    // The unignored children of this object are generated by iterating over its children, ignored or not,
    // and finding the first unignored descendant object of that child (or itself, if unignored).
    RefPtr<AXCoreObject> parent = nullptr;
    const AXCoreObject::AccessibilityChildrenVector* siblings = nullptr;
    bool isExposedTable = isExposableTable();
    AXCoreObject::AccessibilityChildrenVector unignoredChildren;
    const auto& children = childrenIncludingIgnored(updateChildrenIfNeeded);

    RefPtr descendant = children.size() ? children[0].ptr() : nullptr;
    while (descendant && descendant != this) {
        bool childIsValid = !isExposedTable || isValidChildForTable(*descendant);
        if (!childIsValid || descendant->isIgnored()) {
            descendant = descendant->nextInPreOrder(updateChildrenIfNeeded, /* stayWithin */ this);
            parent = nullptr;
            continue;
        }
        unignoredChildren.append(*descendant);

        while (descendant && descendant != this) {
            if (!parent) {
                parent = descendant->parentObject();
                if (!parent) {
                    siblings = nullptr;
                    break;
                }
                siblings = &parent->childrenIncludingIgnored();
            }

            unsigned nextSiblingIndex = descendant->indexInParent() + 1;
            if (RefPtr nextSibling = nextSiblingIndex < siblings->size() ? (*siblings)[nextSiblingIndex].ptr() : nullptr) {
                descendant = WTFMove(nextSibling);
                break;
            }
            // The descendant didn't have a next sibling, so ascend to its parent.
            descendant = WTFMove(parent);
            parent = nullptr;
        }
    }
    return unignoredChildren;
}

AXCoreObject* AXCoreObject::firstUnignoredChild()
{
    const auto& children = childrenIncludingIgnored(/* updateChildrenIfNeeded */ true);
    RefPtr descendant = children.size() ? children[0].ptr() : nullptr;
    if (onlyAddsUnignoredChildren())
        return descendant.unsafeGet();

    bool isExposedTable = isExposableTable();
    while (descendant && descendant != this) {
        bool childIsValid = !isExposedTable || isValidChildForTable(*descendant);
        if (childIsValid && !descendant->isIgnored())
            return descendant.unsafeGet();
        descendant = descendant->nextInPreOrder(/* updateChildrenIfNeeded */ true, /* stayWithin */ this);
    }
    return nullptr;
}

#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)

static AXCoreObject::AccessibilityChildrenVector childrenAfterStitching(AXCoreObject::AccessibilityChildrenVector&& children)
{
    children.removeAllMatching([] (const auto& child) {
        if (!child->hasStitchableRole())
            return false;

        std::optional stitchedIntoID = child->stitchedIntoID();
        return stitchedIntoID && *stitchedIntoID != child->objectID();
    });

    return children;
}


#if !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
static AXCoreObject::AccessibilityChildrenVector childrenAfterStitching(const AXCoreObject::AccessibilityChildrenVector& children)
{
    auto childrenCopy = children;
    return childrenAfterStitching(WTFMove(childrenCopy));
}
#endif // !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)

AXCoreObject::AccessibilityChildrenVector AXCoreObject::stitchedUnignoredChildren()
{
    return childrenAfterStitching(unignoredChildren());
}

AXCoreObject* AXCoreObject::blockFlowAncestor() const
{
    return Accessibility::findAncestor(*this, /* includeSelf */ false, [] (const auto& ancestor) {
        return ancestor.isBlockFlow();
    });
}

AXCoreObject::StitchState AXCoreObject::stitchStateFromGroups(const Vector<Vector<AXID>>* groups, IncludeStitchGroup includeStitchGroup) const
{
    if (!groups)
        return { };

    AXID thisAXID = objectID();
    for (const auto& group : *groups) {
        if (group.contains(thisAXID)) {
            if (includeStitchGroup == IncludeStitchGroup::No) {
                // If the caller doesn't need the group we belong to, don't bother doing the copy.
                return { group[0], { } };
            }

            // Stitching zero or one elements doesn't make sense, so ensure our group is two or larger.
            ASSERT(group.size() >= 2);
            return { group[0], group };
        }
    }
    return { };
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::crossFrameUnignoredChildren()
{
    AXCoreObject::AccessibilityChildrenVector result = stitchedUnignoredChildren();

#if ENABLE_ACCESSIBILITY_LOCAL_FRAME
    if (result.isEmpty()) {
        if (RefPtr crossFrameChild = crossFrameChildObject())
            result.append(*crossFrameChild);
    } else {
        for (size_t i = 0; i < result.size(); i++) {
            if (auto* crossFrameChild = result[i]->crossFrameChildObject())
                result[i] = *crossFrameChild;
        }
    }
#endif

    return result;
}

AXCoreObject* AXCoreObject::crossFrameParentObjectUnignored() const
{
    RefPtr result = parentObjectUnignored();

#if ENABLE_ACCESSIBILITY_LOCAL_FRAME
    if (!result) {
        if (auto* crossFrameParent = crossFrameParentObject())
            result = crossFrameParent;
    }
#endif

    return result.unsafeGet();
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::crossFrameChildrenIncludingIgnored(bool updateChildrenIfNeeded)
{
    AXCoreObject::AccessibilityChildrenVector result = childrenIncludingIgnored(updateChildrenIfNeeded);

#if ENABLE_ACCESSIBILITY_LOCAL_FRAME
    if (result.isEmpty()) {
        AXCoreObject* crossFrameChild = crossFrameChildObject();
        if (crossFrameChild)
            result.append(*crossFrameChild);
    }
#endif

    return result;
}

bool AXCoreObject::crossFrameIsAncestorOfObject(const AXCoreObject& axObject) const
{
    return this == &axObject || axObject.crossFrameIsDescendantOfObject(*this);
}

bool AXCoreObject::crossFrameIsDescendantOfObject(const AXCoreObject& axObject) const
{
    return Accessibility::crossFrameFindAncestor<AXCoreObject>(*this, false, [&axObject] (const AXCoreObject& object) {
        return &object == &axObject;
    }) != nullptr;
}

AXCoreObject* AXCoreObject::parentObjectIncludingCrossFrame() const
{
    RefPtr result = parentObject();

#if ENABLE_ACCESSIBILITY_LOCAL_FRAME
    if (!result) {
        if (auto* crossFrameParent = crossFrameParentObject())
            result = crossFrameParent;
    }
#endif

    return result.unsafeGet();
}


#ifndef NDEBUG
void AXCoreObject::verifyChildrenIndexInParent(const AccessibilityChildrenVector& children) const
{
    if (!shouldSetChildIndexInParent()) {
        // Due to known irregularities in how the accessibility tree is built, we don't want to
        // do this verification for some types of objects, as it will always fail. At the time this
        // was written, this is specifically table columns and table header containers, which insert
        // cells as their children despite not being their "true" parent.
        return;
    }

    for (unsigned i = 0; i < children.size(); i++)
        ASSERT(children[i]->indexInParent() == i);
}
#endif

AXCoreObject* AXCoreObject::nextInPreOrder(bool updateChildrenIfNeeded, AXCoreObject* stayWithin)
{
    return nextInPreOrder(updateChildrenIfNeeded, stayWithin, false);
}

AXCoreObject* AXCoreObject::nextInPreOrder(bool updateChildrenIfNeeded , AXCoreObject* stayWithin, bool includeCrossFrame)
{
    const auto& children = includeCrossFrame ? crossFrameChildrenIncludingIgnored(updateChildrenIfNeeded) : childrenIncludingIgnored(updateChildrenIfNeeded);

    if (!children.isEmpty()) {
        auto role = this->role();
        if (role != AccessibilityRole::Column && role != AccessibilityRole::TableHeaderContainer) {
            // Table columns and header containers add cells despite not being their "true" parent (which are the rows).
            // Don't allow a pre-order traversal of these object types to return cells to avoid an infinite loop.
            return children[0].unsafePtr();
        }
    }

    if (stayWithin == this)
        return nullptr;

    RefPtr current = this;
    RefPtr next = nextSiblingIncludingIgnored(updateChildrenIfNeeded, includeCrossFrame);
    for (; !next; next = current->nextSiblingIncludingIgnored(updateChildrenIfNeeded, includeCrossFrame)) {
        current = includeCrossFrame ? current->parentObjectIncludingCrossFrame() : current->parentObject();

        if (!current || stayWithin == current)
            return nullptr;
    }
    return next.unsafeGet();
}

AXCoreObject* AXCoreObject::previousInPreOrder(bool updateChildrenIfNeeded, AXCoreObject* stayWithin)
{
    if (stayWithin == this)
        return nullptr;

    if (RefPtr sibling = previousSiblingIncludingIgnored(updateChildrenIfNeeded)) {
        const auto& children = sibling->childrenIncludingIgnored(updateChildrenIfNeeded);
        if (children.size())
            return sibling->deepestLastChildIncludingIgnored(updateChildrenIfNeeded);
        return sibling.unsafeGet();
    }
    return parentObject();
}

AXCoreObject* AXCoreObject::deepestLastChildIncludingIgnored(bool updateChildrenIfNeeded)
{
    const auto& children = childrenIncludingIgnored(updateChildrenIfNeeded);
    if (children.isEmpty())
        return nullptr;

    Ref deepestChild = children[children.size() - 1];
    while (true) {
        const auto& descendants = deepestChild->childrenIncludingIgnored(updateChildrenIfNeeded);
        if (descendants.isEmpty())
            break;
        deepestChild = descendants[descendants.size() - 1];
    }
    return deepestChild.unsafePtr();
}

size_t AXCoreObject::indexInSiblings(const AccessibilityChildrenVector& siblings) const
{
    unsigned indexOfThis = indexInParent();
    if (indexOfThis >= siblings.size() || siblings[indexOfThis]->objectID() != objectID()) [[unlikely]] {
        // If this happens, the accessibility tree is an incorrect state.
        ASSERT_NOT_REACHED();

        return siblings.findIf([this] (const Ref<AXCoreObject>& object) {
            return object.ptr() == this;
        });
    }
    return indexOfThis;
}

AXCoreObject* AXCoreObject::nextSiblingIncludingIgnored(bool updateChildrenIfNeeded) const
{
    return nextSiblingIncludingIgnored(updateChildrenIfNeeded, /* crossFrame = */ false);
}

AXCoreObject* AXCoreObject::nextSiblingIncludingIgnored(bool updateChildrenIfNeeded, bool includeCrossFrame) const
{
    RefPtr parent = parentObject();
    if (!parent)
        return nullptr;

    const auto& siblings = includeCrossFrame ? parent->crossFrameChildrenIncludingIgnored(updateChildrenIfNeeded) : parent->childrenIncludingIgnored(updateChildrenIfNeeded);
    size_t indexOfThis = indexInSiblings(siblings);
    if (indexOfThis == notFound)
        return nullptr;

    return indexOfThis + 1 < siblings.size() ? siblings[indexOfThis + 1].unsafePtr() : nullptr;
}

AXCoreObject* AXCoreObject::previousSiblingIncludingIgnored(bool updateChildrenIfNeeded)
{
    RefPtr parent = parentObject();
    if (!parent)
        return nullptr;

    const auto& siblings = parent->childrenIncludingIgnored(updateChildrenIfNeeded);
    size_t indexOfThis = indexInSiblings(siblings);
    if (indexOfThis == notFound)
        return nullptr;

    return indexOfThis >= 1 ? siblings[indexOfThis - 1].ptr() : nullptr;
}

AXCoreObject* AXCoreObject::nextUnignoredSibling(bool updateChildrenIfNeeded, AXCoreObject* unignoredParent) const
{
    // In some contexts, we may have already computed the `unignoredParent`, which is what this parameter is.
    // In debug, ensure this is actually our parent.
    ASSERT(unignoredParent == parentObjectUnignored());

    RefPtr parent = unignoredParent ? unignoredParent : parentObjectUnignored();
    if (!parent)
        return nullptr;
    const auto& siblings = parent->unignoredChildren(updateChildrenIfNeeded);
    size_t indexOfThis = siblings.findIf([this] (const Ref<AXCoreObject>& object) {
        return object.ptr() == this;
    });
    if (indexOfThis == notFound)
        return nullptr;

    return indexOfThis + 1 < siblings.size() ? siblings[indexOfThis + 1].unsafePtr() : nullptr;
}

AXCoreObject* AXCoreObject::nextSiblingIncludingIgnoredOrParent() const
{
    RefPtr parent = parentObject();
    if (RefPtr nextSibling = nextSiblingIncludingIgnored(/* updateChildrenIfNeeded */ true))
        return nextSibling.unsafeGet();
    return parent.unsafeGet();
}

String AXCoreObject::autoCompleteValue() const
{
    String explicitValue = explicitAutoCompleteValue();
    return explicitValue.isEmpty() ? "none"_s : explicitValue;
}

String AXCoreObject::invalidStatus() const
{
    auto explicitValue = explicitInvalidStatus();
    // "false" is the default if no invalid status is explicitly provided (e.g. via aria-invalid).
    return explicitValue.isEmpty() ? "false"_s : explicitValue;
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::contents()
{
    if (isTabList())
        return tabChildren();

    if (isScrollView()) {
        // A scroll view's contents are everything except the scroll bars.
        AccessibilityChildrenVector nonScrollbarChildren;
        for (const auto& child : stitchedUnignoredChildren()) {
            if (!child->isScrollbar())
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
    for (const auto& child : unignoredChildren()) {
        if (!child->isGroup() && child->role() != AccessibilityRole::TreeItem)
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

    if (role() == AccessibilityRole::SliderThumb) {
        RefPtr parent = parentObject();
        return parent ? parent->valueForRange() : 0.0f;
    }

    if (isHeading())
        return headingLevel();

    if (supportsCheckedState())
        return checkboxOrRadioValue();

    if (role() == AccessibilityRole::Summary)
        return isExpanded();

    // Radio groups return the selected radio button as the AXValue.
    if (isRadioGroup())
        return selectedRadioButton();

    if (isTabList())
        return selectedTabItem();

    if (isTabItem())
        return isSelected();

    if (isDateTime())
        return dateTimeValue();

    if (isColorWell()) {
        auto color = convertColor<SRGBA<float>>(colorValue()).resolved();
        auto channel = [](float number) {
            return FormattedNumber::fixedPrecision(number, 6, TrailingZerosPolicy::Keep);
        };
        return color.alpha == 1
            ? makeString("rgb "_s, channel(color.red), ' ', channel(color.green), ' ', channel(color.blue), " 1"_s)
            : makeString("rgb "_s, channel(color.red), ' ', channel(color.green), ' ', channel(color.blue), ' ', channel(color.alpha));
    }

    return stringValue();
}

AXCoreObject* AXCoreObject::selectedRadioButton()
{
    if (!isRadioGroup())
        return nullptr;

    // Find the child radio button that is selected (ie. the intValue == 1).
    for (const auto& child : unignoredChildren()) {
        if (child->role() == AccessibilityRole::RadioButton && child->checkboxOrRadioValue() == AccessibilityButtonState::On)
            return child.ptr();
    }
    return nullptr;
}

AXCoreObject* AXCoreObject::selectedTabItem()
{
    if (!isTabList())
        return nullptr;

    // FIXME: Is this valid? ARIA tab items support aria-selected; not aria-checked.
    // Find the child tab item that is selected (ie. the intValue == 1).
    for (const auto& child : unignoredChildren()) {
        if (child->isTabItem() && (child->isChecked() || child->isSelected()))
            return child.ptr();
    }
    return nullptr;
}

bool AXCoreObject::canHaveSelectedChildren() const
{
    switch (role()) {
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
    case AccessibilityRole::ComboBox:
#if USE(ATSPI)
    case AccessibilityRole::MenuListPopup:
#endif
        return true;
    default:
        return false;
    }
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::selectedChildren()
{
    if (!canHaveSelectedChildren())
        return { };

    switch (role()) {
    case AccessibilityRole::ComboBox:
        if (RefPtr descendant = activeDescendant())
            return { { descendant.releaseNonNull() } };
        break;
    case AccessibilityRole::ListBox:
        return listboxSelectedChildren();
    case AccessibilityRole::Grid:
    case AccessibilityRole::Tree:
    case AccessibilityRole::TreeGrid:
        return selectedRows();
    case AccessibilityRole::TabList:
        if (RefPtr selectedTab = selectedTabItem())
            return { { selectedTab.releaseNonNull() } };
        break;
    case AccessibilityRole::List:
        return selectedListItems();
    case AccessibilityRole::Menu:
    case AccessibilityRole::MenuBar:
        if (RefPtr descendant = activeDescendant())
            return { { descendant.releaseNonNull() } };
        if (RefPtr focusedElement = focusedUIElement())
            return { { focusedElement.releaseNonNull() } };
        break;
    case AccessibilityRole::MenuListPopup: {
        AccessibilityChildrenVector selectedItems;
        for (const auto& child : unignoredChildren()) {
            if (child->isSelected())
                selectedItems.append(child);
        }
        return selectedItems;
    }
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return { };
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::listboxSelectedChildren()
{
    ASSERT(role() == AccessibilityRole::ListBox);

    AccessibilityChildrenVector result;
    bool isMulti = isMultiSelectable();
    for (const auto& child : unignoredChildren()) {
        if (!child->isListBoxOption() || !child->isSelected())
            continue;

        result.append(child);
        if (!isMulti)
            return result;
    }
    return result;
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::selectedRows()
{
    ASSERT(role() == AccessibilityRole::Grid || role() == AccessibilityRole::Tree || role() == AccessibilityRole::TreeGrid);

    bool isMulti = isMultiSelectable();

    AccessibilityChildrenVector result;
    // Prefer active descendant over aria-selected.
    RefPtr activeDescendant = this->activeDescendant();
    if (activeDescendant && (activeDescendant->isTreeItem() || activeDescendant->isExposedTableRow())) {
        result.append(*activeDescendant);
        if (!isMulti)
            return result;
    }

    auto rowsIteration = [&](const auto& rows) {
        for (auto& row : rows) {
            if (row->isSelected() || row->isActiveDescendantOfFocusedContainer()) {
                result.append(row);
                if (!isMulti)
                    break;
            }
        }
    };

    if (isTree())
        rowsIteration(ariaTreeRows());
    else if (isExposableTable() && supportsSelectedRows())
        rowsIteration(rows());
    return result;
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::selectedListItems()
{
    ASSERT(role() == AccessibilityRole::List);

    AccessibilityChildrenVector selectedListItems;
    for (const auto& child : unignoredChildren()) {
        if (child->isListItem() && child->isSelected())
            selectedListItems.append(child);
    }
    return selectedListItems;
}

void AXCoreObject::ariaTreeRows(AccessibilityChildrenVector& rows, AccessibilityChildrenVector& ancestors)
{
    auto ownedObjects = this->ownedObjects();
    ancestors.append(*this);

    // The ordering of rows is first DOM children *not* in aria-owns, followed by all specified
    // in aria-owns.
    for (const auto& child : unignoredChildren()) {
        // Add tree items as the rows.
        if (child->role() == AccessibilityRole::TreeItem) {
            // Child appears both as a direct child and aria-owns, we should use the ordering as
            // described in aria-owns for this child.
            if (ownedObjects.contains(child))
                continue;

            // The result set may already contain the child through aria-owns. For example,
            // a treeitem sitting under the tree root, which is owned elsewhere in the tree.
            if (rows.contains(child))
                continue;

            rows.append(child);
        }

        // Now see if this item also has rows hiding inside of it.
        child->ariaTreeRows(rows, ancestors);
    }

    // Now go through the aria-owns elements.
    for (const auto& child : ownedObjects) {
        // Avoid a circular reference via aria-owns by checking if our parent
        // path includes this child. Currently, looking up the aria-owns parent
        // path itself could be expensive, so we track it separately.
        if (ancestors.contains(child))
            continue;

        // Add tree items as the rows.
        if (child->role() == AccessibilityRole::TreeItem) {
            // Hopefully a flow that does not occur often in practice, but if someone were to include
            // the owned child ealier in the top level of the tree, then reference via aria-owns later,
            // move it to the right place.
            if (rows.contains(child))
                rows.removeFirst(child);

            rows.append(child);
        }

        // Now see if this item also has rows hiding inside of it.
        child->ariaTreeRows(rows, ancestors);
    }

    ancestors.removeLast();
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::ariaTreeRows()
{
    AccessibilityChildrenVector rows;
    AccessibilityChildrenVector ancestors;
    ariaTreeRows(rows, ancestors);
    return rows;
}

bool AXCoreObject::isActiveDescendantOfFocusedContainer() const
{
    auto containers = activeDescendantOfObjects();
    for (auto& container : containers) {
        if (container->isFocused())
            return true;
    }

    return false;
}

// ARIA spec: User agents must not expose the aria-roledescription property if the element to which aria-roledescription is applied does not have a valid WAI-ARIA role or does not have an implicit WAI-ARIA role semantic.
bool AXCoreObject::supportsARIARoleDescription() const
{
    switch (role()) {
    case AccessibilityRole::Generic:
    case AccessibilityRole::Unknown:
        return false;
    default:
        return true;
    }
}

bool AXCoreObject::supportsRangeValue() const
{
    return isProgressIndicator()
        || isSlider()
        || isScrollbar()
        || isSpinButton()
        || (isSplitter() && canSetFocusAttribute())
        || hasAttachmentTag();
}

bool AXCoreObject::supportsRequiredAttribute() const
{
    switch (role()) {
    case AccessibilityRole::Button:
        return isFileUploadButton();
    case AccessibilityRole::Cell:
    case AccessibilityRole::Checkbox:
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::Grid:
    case AccessibilityRole::GridCell:
    case AccessibilityRole::ListBox:
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::RadioGroup:
    case AccessibilityRole::Slider:
    case AccessibilityRole::SpinButton:
    case AccessibilityRole::Switch:
    case AccessibilityRole::TextArea:
    case AccessibilityRole::TextField:
    case AccessibilityRole::ToggleButton:
        return true;
    default:
        return isRowHeader() || isColumnHeader();
    }
}

bool AXCoreObject::isRootWebArea() const
{
    if (role() != AccessibilityRole::WebArea)
        return false;

    RefPtr parent = parentObject();
    // If the parent is a scroll area, and the scroll area has no parent, we are at the root web area.
    return parent && parent->role() == AccessibilityRole::ScrollArea && !parent->parentObject();
}

bool AXCoreObject::isRadioInput() const
{
    std::optional type = inputType();
    return type ? *type == InputType::Type::Radio : false;
}

String AXCoreObject::popupValue() const
{
    String explicitValue = explicitPopupValue();
    if (!explicitValue.isEmpty())
        return explicitValue;

    // In ARIA 1.1, the implicit value for combobox became "listbox."
    if (isComboBox())
        return "listbox"_s;

    // The spec states that "User agents must treat any value of aria-haspopup that is not
    // included in the list of allowed values, including an empty string, as if the value
    // false had been provided."
    return "false"_s;
}

bool AXCoreObject::hasPopup() const
{
    return !equalLettersIgnoringASCIICase(popupValue(), "false"_s);
}

bool AXCoreObject::selfOrAncestorLinkHasPopup() const
{
    if (hasPopup())
        return true;

    for (RefPtr ancestor = parentObject(); ancestor; ancestor = ancestor->parentObject()) {
        // If this logic gets updated (e.g. we no longer check isLink()), make sure to also update
        // -[WebAccessibilityObjectWrapperMac accessibilityAttributeNames].
        if (ancestor->isLink() && ancestor->hasPopup())
            return true;
    }
    return false;
}

std::optional<AccessibilityOrientation> AXCoreObject::defaultOrientation() const
{
    switch (role()) {
    case AccessibilityRole::DescriptionList:
    case AccessibilityRole::List:
    case AccessibilityRole::ListBox:
    case AccessibilityRole::Menu:
    case AccessibilityRole::ScrollBar:
    case AccessibilityRole::Tree:
        return { AccessibilityOrientation::Vertical };
    case AccessibilityRole::MenuBar:
    case AccessibilityRole::Slider:
    case AccessibilityRole::Splitter:
    case AccessibilityRole::TabList:
    case AccessibilityRole::Toolbar:
        return { AccessibilityOrientation::Horizontal };
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::RadioGroup:
    case AccessibilityRole::TreeGrid:
        return { AccessibilityOrientation::Undefined };
    default:
        return std::nullopt;
    }
}

AccessibilityOrientation AXCoreObject::orientation() const
{
    if (std::optional orientation = explicitOrientation())
        return *orientation;

    // In ARIA 1.1, the implicit value of aria-orientation changed from horizontal
    // to undefined on all roles that don't have their own role-specific values. In
    // addition, the implicit value of combobox became undefined.
    if (std::optional defaultOrientation = this->defaultOrientation())
        return *defaultOrientation;

    // Lacking concrete evidence of orientation, horizontal means width > height. vertical is height > width;
    auto size = this->size();
    if (size.width() > size.height())
        return AccessibilityOrientation::Horizontal;
    if (size.height() > size.width())
        return AccessibilityOrientation::Vertical;

    return AccessibilityOrientation::Undefined;
}

AccessibilitySortDirection AXCoreObject::sortDirectionIncludingAncestors() const
{
    for (RefPtr ancestor = this; ancestor; ancestor = ancestor->parentObject()) {
        auto direction = ancestor->sortDirection();
        if (direction != AccessibilitySortDirection::Invalid)
            return direction;
    }
    return AccessibilitySortDirection::Invalid;
}

unsigned AXCoreObject::tableLevel() const
{
    if (!isTable())
        return 0;

    unsigned level = 0;
    RefPtr current = exposedTableAncestor(true /* includeSelf */);
    while (current) {
        level++;
        current = current->exposedTableAncestor(false);
    }
    return level;
}

AXCoreObject* AXCoreObject::columnHeader()
{
    if (!isTableColumn())
        return nullptr;

    RefPtr parent = parentObject();
    if (!parent || !parent->isExposableTable())
        return nullptr;

    for (const auto& cell : unignoredChildren()) {
        if (cell->isColumnHeader())
            return cell.ptr();
    }
    return nullptr;
}

AXCoreObject* AXCoreObject::rowHeader()
{
    const auto& rowChildren = unignoredChildren();
    if (rowChildren.isEmpty())
        return nullptr;

    bool isARIAGridRow = this->isARIAGridRow();

    Ref firstCell = rowChildren[0].get();
    if (!isARIAGridRow && !firstCell->hasElementName(ElementName::HTML_th))
        return nullptr;

    // Verify that the row header is not part of an entire row of headers.
    // In that case, it is unlikely this is a row header (for non-grid rows).
    for (const auto& child : rowChildren) {
        // We found a non-header cell, so this is not an entire row of headers -- return the original header cell.
        if (!isARIAGridRow && !child->hasElementName(ElementName::HTML_th))
            return firstCell.unsafePtr();

        // For grid rows, the first header encountered is the row header.
        if (isARIAGridRow && child->isRowHeader())
            return child.ptr();
    }
    return nullptr;
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::columnHeaders()
{
    AccessibilityChildrenVector headers;
    if (hasCellRole()) {
        RefPtr parentTable = exposedTableAncestor();
        if (!parentTable)
            return { };

        // Choose columnHeaders as the place where the "headers" attribute is reported.
        headers = relatedObjects(AXRelation::Headers);
        // If the headers attribute returned valid values, then do not further search for column headers.
        if (!headers.isEmpty())
            return headers;

        auto rowRange = rowIndexRange();
        auto colRange = columnIndexRange();
        auto rowGroupAncestor = rowGroupAncestorID();
        for (unsigned row = 0; row < rowRange.first; row++) {
            RefPtr tableCell = parentTable->cellForColumnAndRow(colRange.first, row);
            if (!tableCell || tableCell == this || headers.contains(Ref { *tableCell }))
                continue;

            if (tableCell->cellScope() == "colgroup"_s && tableCell->rowGroupAncestorID() == rowGroupAncestor)
                headers.append(tableCell.releaseNonNull());
            else if (tableCell->isColumnHeader())
                headers.append(tableCell.releaseNonNull());
        }
    } else if (isTable()) {
        auto columns = this->columns();
        for (const auto& column : columns) {
            if (RefPtr header = column->columnHeader())
                headers.append(header.releaseNonNull());
        }
    }
    return headers;
}

std::optional<AXID> AXCoreObject::rowGroupAncestorID() const
{
    if (!hasCellRole())
        return { };

    RefPtr rowGroup = Accessibility::findAncestor<AXCoreObject>(*this, /* includeSelf */ false, [] (const auto& ancestor) {
        return ancestor.hasRowGroupTag();
    });

    if (!rowGroup)
        return std::nullopt;
    return rowGroup->objectID();
}

bool AXCoreObject::isTableCellInSameRowGroup(AXCoreObject& otherTableCell)
{
    auto ancestorID = rowGroupAncestorID();
    return ancestorID && *ancestorID == otherTableCell.rowGroupAncestorID();
}

bool AXCoreObject::isTableCellInSameColGroup(AXCoreObject* tableCell)
{
    if (!tableCell)
        return false;

    auto columnRange = columnIndexRange();
    auto otherColumnRange = tableCell->columnIndexRange();

    return columnRange.first <= otherColumnRange.first + otherColumnRange.second;
}

bool AXCoreObject::isReplacedElement() const
{
    // FIXME: Should this include <legend> and form control elements like TextIterator::isRendererReplacedElement does?
    switch (role()) {
    case AccessibilityRole::Audio:
    case AccessibilityRole::Image:
    case AccessibilityRole::Meter:
    case AccessibilityRole::ProgressIndicator:
    case AccessibilityRole::Video:
        return true;
    default:
        return isWidget() || hasAttachmentTag();
    }
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::revealableContainers()
{
    AXCoreObject::AccessibilityChildrenVector revealableContainers;

    auto isCollapsedDetails = [this] {
        return role() == AccessibilityRole::Details && !isExpanded();
    };
    if (isHiddenUntilFoundContainer() || isCollapsedDetails())
        revealableContainers.append(*this);

    if (role() == AccessibilityRole::Summary) {
        // When rendered, the summary element is the thing that assistive technologies can actually
        // navigate to. Return our containing details as the revealable container so we can search
        // inside the details subtree for revealable text.
        if (RefPtr details = detailsAncestor(); details && !details->isExpanded())
            revealableContainers.append(details.releaseNonNull());
    }
    return revealableContainers;
}

bool AXCoreObject::containsOnlyStaticText() const
{
    bool hasText = false;
    RefPtr nonTextDescendant = Accessibility::findUnignoredDescendant(const_cast<AXCoreObject&>(*this), /* includeSelf */ false, [&] (auto& descendant) {
        if (descendant.isGroup()) {
            // Skip through groups to keep looking for text.
            return false;
        }

        if (descendant.isStaticText()) {
            hasText = hasText || !descendant.isIgnored();
            return false;
        }
        return true;
    });
    return hasText && !nonTextDescendant;
}

String AXCoreObject::roleDescription()
{
    if (hasAttachmentTag())
        return AXAttachmentRoleText();

    // aria-roledescription takes precedence over any other rule.
    if (supportsARIARoleDescription()) {
        auto roleDescription = ariaRoleDescription();
        if (!roleDescription.isEmpty())
            return roleDescription;
    }

    auto roleDescription = rolePlatformDescription();
    if (!roleDescription.isEmpty())
        return roleDescription;

    if (role() == AccessibilityRole::Figure)
        return AXFigureText();

    if (role() == AccessibilityRole::Suggestion)
        return AXSuggestionRoleDescriptionText();

    return { };
}

String AXCoreObject::ariaLandmarkRoleDescription() const
{
    switch (role()) {
    case AccessibilityRole::Form:
        return AXARIAContentGroupText("ARIALandmarkForm"_s);
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
    case AccessibilityRole::SectionFooter:
        return AXARIAContentGroupText("ARIASectionFooter"_s);
    case AccessibilityRole::SectionHeader:
        return AXARIAContentGroupText("ARIASectionHeader"_s);
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

bool AXCoreObject::supportsDatetimeAttribute() const
{
    auto elementName = this->elementName();
    return elementName == ElementName::HTML_ins || elementName == ElementName::HTML_del || elementName == ElementName::HTML_time;
}

unsigned AXCoreObject::blockquoteLevel() const
{
    unsigned level = 0;
    for (RefPtr ancestor = parentObject(); ancestor; ancestor = ancestor->parentObject()) {
        if (ancestor->role() == AccessibilityRole::Blockquote)
            ++level;
    }
    return level;
}

unsigned AXCoreObject::headingLevel() const
{
    if (isHeading()) {
        unsigned level = ariaLevel();
        if (level > 0)
            return level;
    }

    auto elementName = this->elementName();
    if (elementName == ElementName::HTML_h1)
        return 1;
    if (elementName == ElementName::HTML_h2)
        return 2;
    if (elementName == ElementName::HTML_h3)
        return 3;
    if (elementName == ElementName::HTML_h4)
        return 4;
    if (elementName == ElementName::HTML_h5)
        return 5;
    if (elementName == ElementName::HTML_h6)
        return 6;
    return 0;
}

unsigned AXCoreObject::hierarchicalLevel() const
{
    unsigned level = ariaLevel();
    if (level > 0)
        return level;

    // Only tree item will calculate its level through the DOM currently.
    if (role() != AccessibilityRole::TreeItem)
        return 0;

    // Hierarchy leveling starts at 1, to match the aria-level spec.
    // We measure tree hierarchy by the number of groups that the item is within.
    level = 1;
    for (RefPtr ancestor = parentObject(); ancestor; ancestor = ancestor->parentObject()) {
        auto ancestorRole = ancestor->role();
        if (ancestorRole == AccessibilityRole::Group)
            level++;
        else if (ancestorRole == AccessibilityRole::Tree)
            break;
    }

    return level;
}

bool AXCoreObject::supportsPressAction() const
{
    if (role() == AccessibilityRole::Presentational || hasPointerEventsNone())
        return false;

    if (isImplicitlyInteractive() || hasClickHandler())
        return true;

    if ((isStaticText() || isImage()) && !isIgnored()) {
        // These roles are not interactive, but sometimes authors expose click handlers on them or ancestors without
        // other appropriate ARIA markup indicating interactivity (e.g. by applying role="button"). We can repair these
        // scenarios by checking for a clickable ancestor. But want to do so selectively, as naively exposing press on
        // every text can be annoying as some screenreaders read "clickable" for each static text.
        bool foundCursor = hasCursorPointer();

        RefPtr clickableAncestor = Accessibility::findAncestor(*this, /* includeSelf */ true, /* matchFunction */ [&foundCursor] (const auto& ancestor) {
            if (!foundCursor)
                foundCursor = ancestor.hasCursorPointer() || ancestor.showsCursorOnHover();
            return ancestor.hasClickHandler();
        }, /* stopTraversalFunction */ [] (const auto& ancestor) {
            // Stop traversing and return nullptr if we walk over an implicitly interactive element on our
            // way to the click handler, as we can rely on the semantics of that element to imply pressability.
            // Also stop when encountering the body or main to avoid exposing pressability for everything in
            // web apps that implement an event-delegation mechanism.
            auto role = ancestor.role();
            return ancestor.isImplicitlyInteractive() || role == AccessibilityRole::LandmarkMain || role == AccessibilityRole::Presentational || ancestor.hasBodyTag();
        });

        if (!clickableAncestor)
            return false;

        if (!foundCursor) {
            // If the author hasn't provided a pointer cursor, the visual experience also doesn't express
            // pressability, so return.
            return false;
        }

        unsigned matches = 0;
        unsigned candidatesChecked = 0;
        RefPtr candidate = clickableAncestor;
        while ((candidate = candidate->nextInPreOrder(/* updateChildren */ true, /* stayWithin */ clickableAncestor.get()))) {
            if (candidate->isStaticText() || candidate->isControl() || candidate->isImage() || candidate->isHeading() || candidate->isLink()) {
                if (!candidate->isIgnored()) {
                    if (!matches && this != candidate.get()) {
                        // Only support press action for the first descendant. Some ATs, like VoiceOver, use the result of this function
                        // to read "clickable", but reading it for every descendant of the clickable ancestor would be excessive.
                        return false;
                    }
                    ++matches;
                }

                static constexpr unsigned MAX_MATCHES = 6;
                if (matches >= MAX_MATCHES) {
                    // If something has more than the arbitrarily-chosen number of valid matches,
                    // this click handler is probably too coarse to be useful.
                    return false;
                }
            }

            ++candidatesChecked;
            static constexpr unsigned MAX_CANDIDATES = 256;
            if (candidatesChecked > MAX_CANDIDATES) {
                // If we've walked over the arbitrarily-chosen max number of potential candidates,
                // this click handler is probably too coarse to be useful, and too much traversing
                // can harm performance.
                return false;
            }
        }

        // If we get here, and matches is greater than zero, we can assume we were the first matching
        // candidate for the click handler, and that there weren't too many matches or candidates checked.
        return matches > 0;
    }
    return false;
}

bool AXCoreObject::supportsActiveDescendant() const
{
    switch (role()) {
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::Grid:
    case AccessibilityRole::List:
    case AccessibilityRole::ListBox:
    case AccessibilityRole::TextArea:
    case AccessibilityRole::TextField:
    case AccessibilityRole::Tree:
    case AccessibilityRole::TreeGrid:
        return true;
    default:
        return false;
    }
}

AXCoreObject* AXCoreObject::activeDescendant() const
{
    auto activeDescendants = relatedObjects(AXRelation::ActiveDescendant);
    ASSERT(activeDescendants.size() <= 1);
    if (!activeDescendants.isEmpty())
        return activeDescendants[0].unsafePtr();
    return nullptr;
}

AXCoreObject* AXCoreObject::selfOrFirstTextDescendant()
{
    return Accessibility::findUnignoredDescendant(*this, /* includeSelf */ true, [] (auto& descendant) {
        return descendant.isStaticText();
    });
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

    if (RefPtr activeDescendant = this->activeDescendant()) {
        if (activeDescendant->isExposedTableCell() && !selectedCells.contains(Ref { *activeDescendant }))
            selectedCells.append(activeDescendant.releaseNonNull());
    }
    return selectedCells;
}

String AXCoreObject::languageIncludingAncestors() const
{
    auto language = this->language();
    if (!language.isEmpty())
        return language;

    RefPtr parent = parentObject();
    return parent ? parent->languageIncludingAncestors() : nullAtom();
}

#if PLATFORM(COCOA)
static bool isVisibleText(AccessibilityTextSource textSource)
{
    switch (textSource) {
    case AccessibilityTextSource::Visible:
    case AccessibilityTextSource::Children:
    case AccessibilityTextSource::LabelByElement:
    case AccessibilityTextSource::Heading:
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
    case AccessibilityTextSource::Heading:
        return false;
    }
}

String AXCoreObject::descriptionAttributeValue() const
{
    if (isStaticText()) {
        // Static text objects shouldn't return a description. Their content is communicated via AXValue.
        return { };
    }

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
        if (text.textSource == AccessibilityTextSource::Alternative || text.textSource == AccessibilityTextSource::Heading) {
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

String AXCoreObject::title() const
{
    if (isWebArea()) {
        String title = webAreaTitle();
        if (!title.isEmpty())
            return title;
    }

    // Static text should communicate its content through AXValue.
    // Meter elements should communicate their content via AXValueDescription.
    if (isStaticText() || isMeter())
        return { };

    // A file upload button presents a challenge because it has button text and a value, but the
    // API doesn't support this paradigm.
    // The compromise is to return the button type in the role description and the value of the file path in the title
    if (isFileUploadButton() && fileUploadButtonReturnsValueInTitle())
        return stringValue();

    Vector<AccessibilityText> textOrder;
    accessibilityText(textOrder);

    if (elementName() == ElementName::HTML_area && isLink()) {
        String summaryText;
        for (auto& text : textOrder) {
            if (text.textSource == AccessibilityTextSource::TitleTag)
                return text.text;
            if (text.textSource == AccessibilityTextSource::Summary)
                summaryText = WTFMove(text.text);
        }
        return summaryText;
    }

    for (const auto& text : textOrder) {
        // If we have alternative text, then we should not expose a title.
        if (text.textSource == AccessibilityTextSource::Alternative || text.textSource == AccessibilityTextSource::Heading)
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

AXCoreObject* AXCoreObject::titleUIElement() const
{
    auto labels = relatedObjects(AXRelation::LabeledBy);
#if PLATFORM(COCOA)
    // We impose the restriction that if there is more than one label, then we should return none.
    // FIXME: the behavior should be the same in all platforms.
    return labels.size() == 1 ? labels.first().unsafePtr() : nullptr;
#else
    return labels.size() ? labels.first().unsafePtr() : nullptr;
#endif
}

String AXCoreObject::expandedTextValue() const
{
    if (isStaticText()) {
        if (RefPtr parent = parentObject()) {
            auto parentName = parent->elementName();
            if (parentName == ElementName::HTML_abbr || parentName == ElementName::HTML_acronym)
                return parent->titleAttribute();
        }
    }
    return hasCellRole() ? abbreviation() : String();
}

bool AXCoreObject::supportsExpandedTextValue() const
{
    if (isStaticText()) {
        if (RefPtr parent = parentObject()) {
            auto parentName = parent->elementName();
            return parentName == ElementName::HTML_abbr || parentName == ElementName::HTML_acronym;
        }
    }
    return hasCellRole() && !abbreviation().isEmpty();
}

AXCoreObject::AccessibilityChildrenVector AXCoreObject::linkedObjects() const
{
    auto linkedObjects = flowToObjects();

    if (isLink()) {
        if (RefPtr linkedAXElement = internalLinkElement())
            linkedObjects.append(linkedAXElement.releaseNonNull());
    } else if (isRadioButton())
        appendRadioButtonGroupMembers(linkedObjects);

    linkedObjects.appendVector(controlledObjects());
    linkedObjects.appendVector(ownedObjects());

    linkedObjects.removeAllMatching([] (const auto& object) {
        return object->isIgnored();
    });
    return linkedObjects;
}

void AXCoreObject::appendRadioButtonDescendants(AXCoreObject& parent, AccessibilityChildrenVector& linkedUIElements) const
{
    for (const auto& child : parent.unignoredChildren()) {
        if (child->isRadioButton())
            linkedUIElements.append(child);
        else
            appendRadioButtonDescendants(child.get(), linkedUIElements);
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
        for (RefPtr parent = parentObject(); parent; parent = parent->parentObject()) {
            if (parent->role() == AccessibilityRole::RadioGroup) {
                appendRadioButtonDescendants(*parent, linkedUIElements);
                break;
            }
        }
    }
}

AXCoreObject* AXCoreObject::parentObjectUnignored() const
{
    if (role() == AccessibilityRole::Row) {
        if (RefPtr table = exposedTableAncestor())
            return table.unsafeGet();
    }

    return Accessibility::findAncestor<AXCoreObject>(*this, false, [&] (const AXCoreObject& object) {
        return !object.isIgnored();
    });
}

AXCoreObject* AXCoreObject::detailsAncestor() const
{
    return Accessibility::findAncestor<AXCoreObject>(*this, false, [&] (const AXCoreObject& object) {
        return object.role() == AccessibilityRole::Details;
    });
}

// This function implements a fast way to determine our ordering relative to |other|: find the
// ancestor we share, then compare the index-in-parent of the next lowest descendant of each us
// and |other|. Take this example:
/*
     A
    / \
   B   C
  /   / \
 D   E   F
          \
           G
*/
// (A has two children, B and C. B has child D. C has children E and F. F has child G.)
//
// Imagine we want to determine the ordering of D relative to G. They share ancestor A. D's descends from B
// who is index 0 in shared ancestor A. G descends from C, who is index 1 in the shared ancestor. This lets
// us know B (who has ancestor index 0) comes before G (who has ancestor index 1).
//
// This is significantly more performant than simply doing a pre-order traversal from |this| to |other|.
// On html.spec.whatwg.org, 1800 runs of this method took 4.4ms total, while 1800 runs of a pre-order
// traversal to determine ordering took 48.6 seconds total. This algorithm is also faster on more "average",
// smaller-accessibility-tree pages:
//   - YouTube: 0.45ms vs 18.2ms in 237 comparisons
//   - Wikipedia: 3.8ms vs. 18.7ms in 270 comparisons
std::partial_ordering AXCoreObject::partialOrder(const AXCoreObject& other)
{
    if (objectID() == other.objectID())
        return std::partial_ordering::equivalent;

    RefPtr current = this;
    RefPtr otherCurrent = other;

    auto orderingFromIndices = [&] (unsigned ourAncestorIndex, unsigned otherAncestorIndex) {
        if (ourAncestorIndex < otherAncestorIndex)
            return std::partial_ordering::less;
        if (ourAncestorIndex > otherAncestorIndex)
            return std::partial_ordering::greater;

        ASSERT_NOT_REACHED();
        return std::partial_ordering::equivalent;
    };

    // I got into an infinite loop here that I wasn't able to reproduce again in order to debug.
    // For now, add a failsafe and ASSERT() so we can try to debug the root cause when it does
    // happen again.
    unsigned failsafeCounter = 0;
    // This variable is 2 times the max render tree depth because the accessibility tree can be
    // larger than the the render tree, e.g. when considering things like scroll views which don't
    // have renderers but are in the accessibility tree. This huge limit ensures that we don't
    // activate the failsafe loop exit unless something is 100% wrong.
    static constexpr unsigned maxIterations = Settings::defaultMaximumRenderTreeDepth * 2;

    // ListHashSet chosen intentionally because it has O(1) lookup time. This is important
    // because we need to repeatedly query these lists, once every time we find a new ancestor.
    ListHashSet<Ref<AXCoreObject>> ourAncestors;
    ListHashSet<Ref<AXCoreObject>> otherAncestors;
    while (failsafeCounter < maxIterations && (current || otherCurrent)) {
        ++failsafeCounter;

        if (RefPtr maybeParent = current ? current->parentObject() : nullptr) {
            ASSERT(current != maybeParent);

            if (maybeParent == &other) {
                // We are a descendant of the other object, so we come after it.
                return std::partial_ordering::greater;
            }

            Ref parent = maybeParent.releaseNonNull();
            if (auto iterator = otherAncestors.find(parent); iterator != otherAncestors.end()) {
                // If ourAncestors is empty (it has zero size), that means the shared ancestor is |current|'s
                // parent, and thus the index to use is |current| position in the shared parent's children.
                unsigned ourAncestorIndex = ourAncestors.size() ? ourAncestors.takeLast()->indexInParent() : current->indexInParent();
                --iterator;
                // Similarly, it's possible the shared ancestor was the direct parent of |otherCurrent| —
                // determine this by checking whether iterator == otherAncestors.end() after moving back one
                // element.
                unsigned otherAncestorIndex = iterator != otherAncestors.end() ? (*iterator)->indexInParent() : other.indexInParent();

                return orderingFromIndices(ourAncestorIndex, otherAncestorIndex);
            }
            current = parent.ptr();
            ASSERT(!ourAncestors.contains(parent));
            ourAncestors.appendOrMoveToLast(WTFMove(parent));
        }

        if (RefPtr maybeParent = otherCurrent ? otherCurrent->parentObject() : nullptr) {
            ASSERT(otherCurrent != maybeParent);

            if (maybeParent == this) {
                // The other object is a descendant of ours, so we come before it in tree-order.
                return std::partial_ordering::less;
            }

            Ref parent = maybeParent.releaseNonNull();
            if (auto iterator = ourAncestors.find(parent); iterator != ourAncestors.end()) {
                unsigned otherAncestorIndex = otherAncestors.size() ? otherAncestors.takeLast()->indexInParent() : otherCurrent->indexInParent();
                --iterator;
                unsigned ourAncestorIndex = iterator != ourAncestors.end() ? (*iterator)->indexInParent() : indexInParent();

                return orderingFromIndices(ourAncestorIndex, otherAncestorIndex);
            }
            otherCurrent = parent.ptr();
            ASSERT(!otherAncestors.contains(parent));
            otherAncestors.appendOrMoveToLast(WTFMove(parent));
        }
    }

    ASSERT(failsafeCounter < maxIterations);
    // If we pass the above ASSERT but hit this one, it means we didn't loop infinitely,
    // but also did not find a shared ancestor between the two objects, which shouldn't ever happen.
    ASSERT_NOT_REACHED();
    return std::partial_ordering::unordered;
}

// LineDecorationStyle implementations.

LineDecorationStyle::LineDecorationStyle(RenderObject& renderer)
{
    const CheckedRef style = renderer.style();
    auto decor = style->textDecorationLineInEffect();
    if (decor.containsAny({ Style::TextDecorationLine::Flag::Underline, Style::TextDecorationLine::Flag::LineThrough })) {
        auto decorationStyles = TextDecorationPainter::stylesForRenderer(renderer, decor);
        if (decor.hasUnderline()) {
            hasUnderline = true;
            underlineColor = decorationStyles.underline.color;
        }

        if (decor.hasLineThrough()) {
            hasLinethrough = true;
            linethroughColor = decorationStyles.linethrough.color;
        }
    }
}

String LineDecorationStyle::debugDescription() const
{
    return makeString(
        "{"_s,
        "hasUnderline: "_s, hasUnderline,
        ", underlineColor: "_s, underlineColor.debugDescription(),
        ", hasLinethrough: "_s, hasLinethrough,
        ", linethroughColor: "_s, linethroughColor.debugDescription(),
        "}"_s
    );
}

String AXCoreObject::infoStringForTesting()
{
    return makeString("Role: "_s, rolePlatformString(), ", Value: "_s, stringValue());
}

namespace Accessibility {

#if !PLATFORM(MAC) && !USE(ATSPI)
// FIXME: implement in other platforms.
PlatformRoleMap createPlatformRoleMap() { return PlatformRoleMap(); }
#endif

void initializeRoleMap()
{
    roleToPlatformString(AccessibilityRole::Button);
}

String roleToPlatformString(AccessibilityRole role)
{
    // This map can be read from multiple threads at once. This is fine,
    // as we don't mutate it after creation, and never allow it to be destroyed.
    // The only thing we need to make thread-safe is its initialization, accomplished
    // by expliciting initializing the map before the accessibility thread is started.
    static NeverDestroyed<PlatformRoleMap> roleMap = createPlatformRoleMap();
    return roleMap->get(enumToUnderlyingType(role));
}

bool inRenderTreeOrStyleUpdate(const Document& document)
{
    if (document.inStyleRecalc() || document.inRenderTreeUpdate())
        return true;
    auto* view = document.view();
    return view && view->layoutContext().isInRenderTreeLayout();
}

Color defaultColor()
{
    static NeverDestroyed<Color> color = Color().toColorTypeLossy<SRGBA<uint8_t>>();
    return color.get();
}

} // namespace Accessibility

} // namespace WebCore
