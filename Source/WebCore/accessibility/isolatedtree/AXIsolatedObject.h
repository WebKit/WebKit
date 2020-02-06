/*
* Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)

#include "AXObjectCache.h"
#include "AccessibilityObjectInterface.h"
#include "IntPoint.h"
#include "LayoutRect.h"
#include "Path.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AXIsolatedTree;

class AXIsolatedObject final : public AXCoreObject {
public:
    static Ref<AXIsolatedObject> create(AXCoreObject&, bool isRoot);
    ~AXIsolatedObject();

    void setObjectID(AXID id) override { m_id = id; }
    AXID objectID() const override { return m_id; }
    void init() override { }

    bool isDetached() const override;

    void setTreeIdentifier(AXIsolatedTreeID);
    void setParent(AXID);
    void appendChild(AXID);

private:
    void detachRemoteParts(AccessibilityDetachmentType) override;
    void detachPlatformWrapper(AccessibilityDetachmentType) override;

    AXID parent() const { return m_parent; }

    AXIsolatedTreeID treeIdentifier() const { return m_treeIdentifier; }
    AXIsolatedTree* tree() const { return m_cachedTree.get(); }

    AXIsolatedObject() = default;
    AXIsolatedObject(AXCoreObject&, bool isRoot);
    void initializeAttributeData(AXCoreObject&, bool isRoot);
    AXCoreObject* associatedAXObject() const
    {
        ASSERT(isMainThread());
        return axObjectCache()->objectFromAXID(objectID());
    }

    enum class AXPropertyName : uint8_t {
        None = 0,
        AccessKey,
        ActionVerb,
        AccessibilityButtonState,
        AccessibilityText,
        AutoCompleteValue,
        ARIAControlsElements,
        ARIADetailsElements,
        ARIADropEffects,
        ARIAErrorMessageElements,
        ARIAIsMultiline,
        ARIAFlowToElements,
        ARIALandmarkRoleDescription,
        ARIATreeItemContent,
        ARIATreeItemDisclosedRows,
        ARIATreeRows,
        ARIARoleAttribute,
        ARIAOwnsElements,
        BlockquoteLevel,
        BoundingBoxRect,
        CanHaveSelectedChildren,
        CanSetExpandedAttribute,
        CanSetFocusAttribute,
        CanSetNumericValue,
        CanSetSelectedAttribute,
        CanSetSelectedChildrenAttribute,
        CanSetTextRangeAttributes,
        CanSetValueAttribute,
        CanvasHasFallbackContent,
#if PLATFORM(COCOA) && !PLATFORM(IOS_FAMILY)
        CaretBrowsingEnabled,
#endif
        ClassList,
        ClickPoint,
        ColorValue,
        ComputedLabel,
        ComputedRoleString,
        CurrentState,
        CurrentValue,
        DatetimeAttributeValue,
        DecrementButton,
        Description,
        DocumentEncoding,
        DocumentURI,
        EditableAncestor,
        ElementRect,
        EstimatedLoadingProgress,
        ExpandedTextValue,
        ExposesTitleUIElement,
        FileUploadButtonReturnsValueInTitle,
        FocusableAncestor,
        HasARIAValueNow,
        HasPopup,
        HeadingLevel,
        HelpText,
        HierarchicalLevel,
        HighestEditableAncestor,
        HorizontalScrollBar,
        IdentifierAttribute,
        InvalidStatus,
        IncrementButton,
        IsAccessibilityIgnored,
        IsActiveDescendantOfFocusedContainer,
        IsAnonymousMathOperator,
        IsARIAGrabbed,
        IsARIATreeGridRow,
        IsAttachment,
        IsButton,
        IsBusy,
        IsChecked,
        IsCollapsed,
        IsControl,
        IsDescriptionList,
        IsEnabled,
        IsExpanded,
        IsFieldset,
        IsFileUploadButton,
        IsFocused,
        IsGroup,
        IsImage,
        IsImageMapLink,
        IsIncrementor,
        IsIndeterminate,
        IsInlineText,
        IsInputImage,
        IsInsideLiveRegion,
        IsHeading,
        IsHovered,
        IsLandmark,
        IsLink,
        IsLinked,
        IsList,
        IsListBox,
        IsLoaded,
        IsMathElement,
        IsMathFraction,
        IsMathFenced,
        IsMathSubscriptSuperscript,
        IsMathRow,
        IsMathUnderOver,
        IsMathRoot,
        IsMathSquareRoot,
        IsMathText,
        IsMathNumber,
        IsMathOperator,
        IsMathFenceOperator,
        IsMathSeparatorOperator,
        IsMathIdentifier,
        IsMathTable,
        IsMathTableRow,
        IsMathTableCell,
        IsMathMultiscript,
        IsMathToken,
        IsMathScriptObject,
        IsMediaTimeline,
        IsMenu,
        IsMenuBar,
        IsMenuButton,
        IsMenuItem,
        IsMenuList,
        IsMenuListOption,
        IsMenuListPopup,
        IsMenuRelated,
        IsMeter,
        IsMultiSelectable,
        IsOffScreen,
        IsOnScreen,
        IsOrderedList,
        IsOutput,
        IsPasswordField,
        IsPressed,
        IsProgressIndicator,
        IsRangeControl,
        IsRequired,
        IsScrollbar,
        IsSearchField,
        IsSelected,
        IsSelectedOptionActive,
        IsShowingValidationMessage,
        IsSlider,
        IsStyleFormatGroup,
        IsTableCell,
        IsTableColumn,
        IsTableRow,
        IsTextControl,
        IsTree,
        IsTreeItem,
        IsUnorderedList,
        IsUnvisited,
        IsValueAutofilled,
        IsValueAutofillAvailable,
        IsVisible,
        IsVisited,
        KeyShortcutsValue,
        Language,
        LayoutCount,
        LinkRelValue,
        LinkedUIElements,
        LiveRegionAtomic,
        LiveRegionRelevant,
        LiveRegionStatus,
        MathFencedOpenString,
        MathFencedCloseString,
        MathLineThickness,
        MathPrescripts,
        MathPostscripts,
        MathRadicandObject,
        MathRootIndexObject,
        MathUnderObject,
        MathOverObject,
        MathNumeratorObject,
        MathDenominatorObject,
        MathBaseObject,
        MathSubscriptObject,
        MathSuperscriptObject,
        MaxValueForRange,
        MinValueForRange,
        Orientation,
        PlaceholderValue,
        PressedIsPresent,
        PopupValue,
        PosInSet,
        PreventKeyboardDOMEventDispatch,
        ReadOnlyValue,
        RelativeFrame,
        RoleValue,
        RolePlatformString,
        RoleDescription,
        SelectedChildren,
        SelectedRadioButton,
        SelectedTabItem,
        SessionID,
        SetSize,
        SortDirection,
        SpeakAs,
        SpeechHint,
        StringValue,
        SupportsARIADragging,
        SupportsARIADropping,
        SupportsARIAOwns,
        SupportsCurrent,
        SupportsDatetimeAttribute,
        SupportsExpanded,
        SupportsExpandedTextValue,
        SupportsLiveRegion,
        SupportsPath,
        SupportsPosInSet,
        SupportsPressAction,
        SupportsRangeValue,
        SupportsRequiredAttribute,
        SupportsSetSize,
        TabChildren,
        TableLevel,
        TagName,
        TextLength,
        Title,
        TitleUIElement,
        URL,
        ValueAutofillButtonType,
        ValueDescription,
        ValueForRange,
        ValidationMessage,
        VerticalScrollBar,
        VisibleChildren,
    };
    
    typedef std::pair<AXID, AXID> AccessibilityIsolatedTreeMathMultiscriptPair;
    struct AccessibilityIsolatedTreeText {
        String text;
        AccessibilityTextSource textSource;
        Vector<AXID> textElements;
    };
    
    using AttributeValueVariant = Variant<std::nullptr_t, String, bool, int, unsigned, double, float, uint64_t, Color, URL, LayoutRect, AXID, IntPoint, OptionSet<SpeakAs>, Vector<AccessibilityIsolatedTreeText>, Vector<AXID>, Vector<AccessibilityIsolatedTreeMathMultiscriptPair>, Vector<String>>;
    void setProperty(AXPropertyName, AttributeValueVariant&&, bool shouldRemove = false);
    void setObjectProperty(AXPropertyName, AXCoreObject*);
    void setObjectVectorProperty(AXPropertyName, AccessibilityChildrenVector&);

    bool boolAttributeValue(AXPropertyName) const;
    const String stringAttributeValue(AXPropertyName) const;
    int intAttributeValue(AXPropertyName) const;
    unsigned unsignedAttributeValue(AXPropertyName) const;
    double doubleAttributeValue(AXPropertyName) const;
    float floatAttributeValue(AXPropertyName) const;
    AXCoreObject* objectAttributeValue(AXPropertyName) const;
    IntPoint intPointAttributeValue(AXPropertyName) const;
    Color colorAttributeValue(AXPropertyName) const;
    URL urlAttributeValue(AXPropertyName) const;
    uint64_t uint64AttributeValue(AXPropertyName) const;
    template<typename T> T rectAttributeValue(AXPropertyName) const;
    template<typename T> Vector<T> vectorAttributeValue(AXPropertyName) const;
    template<typename T> OptionSet<T> optionSetAttributeValue(AXPropertyName) const;

    void fillChildrenVectorForProperty(AXPropertyName, AccessibilityChildrenVector&) const;
    void setMathscripts(AXPropertyName, AXCoreObject&);
    void insertMathPairs(Vector<AccessibilityIsolatedTreeMathMultiscriptPair>&, AccessibilityMathMultiscriptPairs&);
    template<typename U> void performFunctionOnMainThread(U&&);

    // Attribute retrieval overrides.
    bool isHeading() const override { return boolAttributeValue(AXPropertyName::IsHeading); }
    bool isLandmark() const override { return boolAttributeValue(AXPropertyName::IsLandmark); }
    bool isLink() const override { return boolAttributeValue(AXPropertyName::IsLink); }
    bool isImage() const override { return boolAttributeValue(AXPropertyName::IsImage); }
    bool isPasswordField() const override { return boolAttributeValue(AXPropertyName::IsPasswordField); }
    bool isSearchField() const override { return boolAttributeValue(AXPropertyName::IsSearchField); }
    bool isAttachment() const override { return boolAttributeValue(AXPropertyName::IsAttachment); }
    bool isMediaTimeline() const override { return boolAttributeValue(AXPropertyName::IsMediaTimeline); }
    bool isMenuRelated() const override { return boolAttributeValue(AXPropertyName::IsMenuRelated); }
    bool isMenu() const override { return boolAttributeValue(AXPropertyName::IsMenu); }
    bool isMenuBar() const override { return boolAttributeValue(AXPropertyName::IsMenuBar); }
    bool isMenuButton() const override { return boolAttributeValue(AXPropertyName::IsMenuButton); }
    bool isMenuItem() const override { return boolAttributeValue(AXPropertyName::IsMenuItem); }
    bool isFileUploadButton() const override { return boolAttributeValue(AXPropertyName::IsFileUploadButton); }
    bool isInputImage() const override { return boolAttributeValue(AXPropertyName::IsInputImage); }
    bool isProgressIndicator() const override { return boolAttributeValue(AXPropertyName::IsProgressIndicator); }
    bool isSlider() const override { return boolAttributeValue(AXPropertyName::IsSlider); }
    bool isControl() const override { return boolAttributeValue(AXPropertyName::IsControl); }

    bool isList() const override { return boolAttributeValue(AXPropertyName::IsList); }
    bool isUnorderedList() const override { return boolAttributeValue(AXPropertyName::IsUnorderedList); }
    bool isOrderedList() const override { return boolAttributeValue(AXPropertyName::IsOrderedList); }
    bool isDescriptionList() const override { return boolAttributeValue(AXPropertyName::IsDescriptionList); }

    bool isTable() const override { return false; }
    bool isTableRow() const override { return boolAttributeValue(AXPropertyName::IsTableRow); }
    bool isTableColumn() const override { return boolAttributeValue(AXPropertyName::IsTableColumn); }
    bool isTableCell() const override { return boolAttributeValue(AXPropertyName::IsTableCell); }
    bool isFieldset() const override { return boolAttributeValue(AXPropertyName::IsFieldset); }
    bool isGroup() const override { return boolAttributeValue(AXPropertyName::IsGroup); }
    bool isARIATreeGridRow() const override { return boolAttributeValue(AXPropertyName::IsARIATreeGridRow); }
    bool isMenuList() const override { return boolAttributeValue(AXPropertyName::IsMenuList); }
    bool isMenuListPopup() const override { return boolAttributeValue(AXPropertyName::IsMenuListPopup); }
    bool isMenuListOption() const override { return boolAttributeValue(AXPropertyName::IsMenuListOption); }
    bool isTextControl() const override { return boolAttributeValue(AXPropertyName::IsTextControl); }
    bool isButton() const override { return boolAttributeValue(AXPropertyName::IsButton); }
    bool isRangeControl() const override { return boolAttributeValue(AXPropertyName::IsRangeControl); }
    bool isMeter() const override { return boolAttributeValue(AXPropertyName::IsMeter); }
    bool isStyleFormatGroup() const override { return boolAttributeValue(AXPropertyName::IsStyleFormatGroup); }
    bool isOutput() const override { return boolAttributeValue(AXPropertyName::IsOutput); }
    bool isChecked() const override { return boolAttributeValue(AXPropertyName::IsChecked); }
    bool isEnabled() const override { return boolAttributeValue(AXPropertyName::IsEnabled); }
    bool isSelected() const override { return boolAttributeValue(AXPropertyName::IsSelected); }
    bool isFocused() const override { return boolAttributeValue(AXPropertyName::IsFocused); }
    bool isMultiSelectable() const override { return boolAttributeValue(AXPropertyName::IsMultiSelectable); }
    bool isVisited() const override { return boolAttributeValue(AXPropertyName::IsVisited); }
    bool isRequired() const override { return boolAttributeValue(AXPropertyName::IsRequired); }
    bool supportsRequiredAttribute() const override { return boolAttributeValue(AXPropertyName::SupportsRequiredAttribute); }
    bool isExpanded() const override { return boolAttributeValue(AXPropertyName::IsExpanded); }
    FloatRect relativeFrame() const override { return rectAttributeValue<FloatRect>(AXPropertyName::RelativeFrame); }
    bool supportsDatetimeAttribute() const override { return boolAttributeValue(AXPropertyName::SupportsDatetimeAttribute); }
    String datetimeAttributeValue() const override { return stringAttributeValue(AXPropertyName::DatetimeAttributeValue); }
    bool canSetFocusAttribute() const override { return boolAttributeValue(AXPropertyName::CanSetFocusAttribute); }
    bool canSetTextRangeAttributes() const override { return boolAttributeValue(AXPropertyName::CanSetTextRangeAttributes); }
    bool canSetValueAttribute() const override { return boolAttributeValue(AXPropertyName::CanSetValueAttribute); }
    bool canSetNumericValue() const override { return boolAttributeValue(AXPropertyName::CanSetNumericValue); }
    bool canSetSelectedAttribute() const override { return boolAttributeValue(AXPropertyName::CanSetSelectedAttribute); }
    bool canSetSelectedChildrenAttribute() const override { return boolAttributeValue(AXPropertyName::CanSetSelectedChildrenAttribute); }
    bool canSetExpandedAttribute() const override { return boolAttributeValue(AXPropertyName::CanSetExpandedAttribute); }
    bool accessibilityIsIgnored() const override { return boolAttributeValue(AXPropertyName::IsAccessibilityIgnored); }
    bool isShowingValidationMessage() const override { return boolAttributeValue(AXPropertyName::IsShowingValidationMessage); }
    String validationMessage() const override { return stringAttributeValue(AXPropertyName::ValidationMessage); }
    unsigned blockquoteLevel() const override { return unsignedAttributeValue(AXPropertyName::BlockquoteLevel); }
    int headingLevel() const override { return intAttributeValue(AXPropertyName::HeadingLevel); }
    int tableLevel() const override { return intAttributeValue(AXPropertyName::TableLevel); }
    AccessibilityButtonState checkboxOrRadioValue() const override { return static_cast<AccessibilityButtonState>(intAttributeValue(AXPropertyName::AccessibilityButtonState)); }
    String valueDescription() const override { return stringAttributeValue(AXPropertyName::ValueDescription); }
    float valueForRange() const override { return floatAttributeValue(AXPropertyName::ValueForRange); }
    float maxValueForRange() const override { return floatAttributeValue(AXPropertyName::MaxValueForRange); }
    float minValueForRange() const override { return floatAttributeValue(AXPropertyName::MinValueForRange); }
    AXCoreObject* selectedRadioButton() override { return objectAttributeValue(AXPropertyName::SelectedRadioButton); }
    AXCoreObject* selectedTabItem() override { return objectAttributeValue(AXPropertyName::SelectedTabItem); }
    int layoutCount() const override { return intAttributeValue(AXPropertyName::LayoutCount); }
    double estimatedLoadingProgress() const override { return doubleAttributeValue(AXPropertyName::EstimatedLoadingProgress); }
    bool supportsARIAOwns() const override { return boolAttributeValue(AXPropertyName::SupportsARIAOwns); }
    bool isActiveDescendantOfFocusedContainer() const override { return boolAttributeValue(AXPropertyName::IsActiveDescendantOfFocusedContainer); }
    void ariaControlsElements(AccessibilityChildrenVector& children) const override { fillChildrenVectorForProperty(AXPropertyName::ARIAControlsElements, children); }
    void ariaDetailsElements(AccessibilityChildrenVector& children) const override { fillChildrenVectorForProperty(AXPropertyName::ARIADetailsElements, children); }
    void ariaErrorMessageElements(AccessibilityChildrenVector& children) const override { fillChildrenVectorForProperty(AXPropertyName::ARIAErrorMessageElements, children); }
    void ariaFlowToElements(AccessibilityChildrenVector& children) const override { fillChildrenVectorForProperty(AXPropertyName::ARIAFlowToElements, children); }
    void ariaOwnsElements(AccessibilityChildrenVector& children) const override { fillChildrenVectorForProperty(AXPropertyName::ARIAOwnsElements, children); }
    bool hasPopup() const override { return boolAttributeValue(AXPropertyName::HasPopup); }
    String popupValue() const override { return stringAttributeValue(AXPropertyName::PopupValue); }
    bool pressedIsPresent() const override { return boolAttributeValue(AXPropertyName::PressedIsPresent); }
    bool ariaIsMultiline() const override { return boolAttributeValue(AXPropertyName::ARIAIsMultiline); }
    String invalidStatus() const override { return stringAttributeValue(AXPropertyName::InvalidStatus); }
    bool supportsExpanded() const override { return boolAttributeValue(AXPropertyName::SupportsExpanded); }
    AccessibilitySortDirection sortDirection() const override { return static_cast<AccessibilitySortDirection>(intAttributeValue(AXPropertyName::SortDirection)); }
    bool canvasHasFallbackContent() const override { return boolAttributeValue(AXPropertyName::CanvasHasFallbackContent); }
    bool supportsRangeValue() const override { return boolAttributeValue(AXPropertyName::SupportsRangeValue); }
    String identifierAttribute() const override { return stringAttributeValue(AXPropertyName::IdentifierAttribute); }
    String linkRelValue() const override { return stringAttributeValue(AXPropertyName::LinkRelValue); }
    void classList(Vector<String>&) const override;
    AccessibilityCurrentState currentState() const override { return static_cast<AccessibilityCurrentState>(intAttributeValue(AXPropertyName::CurrentState)); }
    String currentValue() const override { return stringAttributeValue(AXPropertyName::CurrentValue); }
    bool supportsCurrent() const override { return boolAttributeValue(AXPropertyName::SupportsCurrent); }
    const String keyShortcutsValue() const override { return stringAttributeValue(AXPropertyName::KeyShortcutsValue); }
    bool supportsSetSize() const override { return boolAttributeValue(AXPropertyName::SupportsSetSize); }
    bool supportsPosInSet() const override { return boolAttributeValue(AXPropertyName::SupportsPosInSet); }
    int setSize() const override { return intAttributeValue(AXPropertyName::SetSize); }
    int posInSet() const override { return intAttributeValue(AXPropertyName::PosInSet); }
    bool supportsARIADropping() const override { return boolAttributeValue(AXPropertyName::SupportsARIADropping); }
    bool supportsARIADragging() const override { return boolAttributeValue(AXPropertyName::SupportsARIADragging); }
    bool isARIAGrabbed() override { return boolAttributeValue(AXPropertyName::IsARIAGrabbed); }
    Vector<String> determineARIADropEffects() override { return vectorAttributeValue<String>(AXPropertyName::ARIADropEffects); }
    AXCoreObject* accessibilityHitTest(const IntPoint&) const override;
    AXCoreObject* focusedUIElement() const override;
    AXCoreObject* parentObject() const override { return parentObjectUnignored(); }
    AXCoreObject* parentObjectUnignored() const override;
    void linkedUIElements(AccessibilityChildrenVector& children) const override { fillChildrenVectorForProperty(AXPropertyName::LinkedUIElements, children); }
    AXCoreObject* titleUIElement() const override { return objectAttributeValue(AXPropertyName::TitleUIElement); }
    bool exposesTitleUIElement() const override { return boolAttributeValue(AXPropertyName::ExposesTitleUIElement); }
    AXCoreObject* scrollBar(AccessibilityOrientation) override;
    AccessibilityRole ariaRoleAttribute() const override { return static_cast<AccessibilityRole>(intAttributeValue(AXPropertyName::ARIARoleAttribute)); }
    String computedLabel() override { return stringAttributeValue(AXPropertyName::ComputedLabel); }
    int textLength() const override { return intAttributeValue(AXPropertyName::TextLength); }
    const String placeholderValue() const override { return stringAttributeValue(AXPropertyName::PlaceholderValue); }
    String expandedTextValue() const override { return stringAttributeValue(AXPropertyName::ExpandedTextValue); }
    bool supportsExpandedTextValue() const override { return boolAttributeValue(AXPropertyName::SupportsExpandedTextValue); }
    void colorValue(int&, int&, int&) const override;
    AccessibilityRole roleValue() const override { return static_cast<AccessibilityRole>(intAttributeValue(AXPropertyName::RoleValue)); }
    String rolePlatformString() const override { return stringAttributeValue(AXPropertyName::RolePlatformString); }
    String roleDescription() const override { return stringAttributeValue(AXPropertyName::RoleDescription); }
    String ariaLandmarkRoleDescription() const override { return stringAttributeValue(AXPropertyName::ARIALandmarkRoleDescription); }
    bool supportsPressAction() const override { return boolAttributeValue(AXPropertyName::SupportsPressAction); }
    LayoutRect boundingBoxRect() const override { return rectAttributeValue<LayoutRect>(AXPropertyName::BoundingBoxRect); }
    LayoutRect elementRect() const override { return rectAttributeValue<LayoutRect>(AXPropertyName::ElementRect); }
    IntPoint clickPoint() override { return intPointAttributeValue(AXPropertyName::ClickPoint); }
    void accessibilityText(Vector<AccessibilityText>& texts) const override;
    String computedRoleString() const override { return stringAttributeValue(AXPropertyName::ComputedRoleString); }
    bool isValueAutofilled() const override { return boolAttributeValue(AXPropertyName::IsValueAutofilled); }
    bool isValueAutofillAvailable() const override { return boolAttributeValue(AXPropertyName::IsValueAutofillAvailable); }
    AutoFillButtonType valueAutofillButtonType() const override { return static_cast<AutoFillButtonType>(intAttributeValue(AXPropertyName::ValueAutofillButtonType)); }
    void ariaTreeRows(AccessibilityChildrenVector& children) override { fillChildrenVectorForProperty(AXPropertyName::ARIATreeRows, children); }
    void ariaTreeItemDisclosedRows(AccessibilityChildrenVector& children) override { fillChildrenVectorForProperty(AXPropertyName::ARIATreeItemDisclosedRows, children); }
    void ariaTreeItemContent(AccessibilityChildrenVector& children) override { fillChildrenVectorForProperty(AXPropertyName::ARIATreeItemContent, children); }
    URL url() const override { return urlAttributeValue(AXPropertyName::URL); }
    String accessKey() const override { return stringAttributeValue(AXPropertyName::AccessKey); }
    String actionVerb() const override { return stringAttributeValue(AXPropertyName::ActionVerb); }
    String readOnlyValue() const override { return stringAttributeValue(AXPropertyName::ReadOnlyValue); }
    String autoCompleteValue() const override { return stringAttributeValue(AXPropertyName::AutoCompleteValue); }
    OptionSet<SpeakAs> speakAsProperty() const override { return optionSetAttributeValue<SpeakAs>(AXPropertyName::SpeakAs); }
    bool isMathElement() const override { return boolAttributeValue(AXPropertyName::IsMathElement); }
    bool isMathFraction() const override { return boolAttributeValue(AXPropertyName::IsMathFraction); }
    bool isMathFenced() const override { return boolAttributeValue(AXPropertyName::IsMathFenced); }
    bool isMathSubscriptSuperscript() const override { return boolAttributeValue(AXPropertyName::IsMathSubscriptSuperscript); }
    bool isMathRow() const override { return boolAttributeValue(AXPropertyName::IsMathRow); }
    bool isMathUnderOver() const override { return boolAttributeValue(AXPropertyName::IsMathUnderOver); }
    bool isMathRoot() const override { return boolAttributeValue(AXPropertyName::IsMathRoot); }
    bool isMathSquareRoot() const override { return boolAttributeValue(AXPropertyName::IsMathSquareRoot); }
    bool isMathText() const override { return boolAttributeValue(AXPropertyName::IsMathText); }
    bool isMathNumber() const override { return boolAttributeValue(AXPropertyName::IsMathNumber); }
    bool isMathOperator() const override { return boolAttributeValue(AXPropertyName::IsMathOperator); }
    bool isMathFenceOperator() const override { return boolAttributeValue(AXPropertyName::IsMathFenceOperator); }
    bool isMathSeparatorOperator() const override { return boolAttributeValue(AXPropertyName::IsMathSeparatorOperator); }
    bool isMathIdentifier() const override { return boolAttributeValue(AXPropertyName::IsMathIdentifier); }
    bool isMathTable() const override { return boolAttributeValue(AXPropertyName::IsMathTable); }
    bool isMathTableRow() const override { return boolAttributeValue(AXPropertyName::IsMathTableRow); }
    bool isMathTableCell() const override { return boolAttributeValue(AXPropertyName::IsMathTableCell); }
    bool isMathMultiscript() const override { return boolAttributeValue(AXPropertyName::IsMathMultiscript); }
    bool isMathToken() const override { return boolAttributeValue(AXPropertyName::IsMathToken); }
    AXCoreObject* mathRadicandObject() override { return objectAttributeValue(AXPropertyName::MathRadicandObject); }
    AXCoreObject* mathRootIndexObject() override { return objectAttributeValue(AXPropertyName::MathRootIndexObject); }
    AXCoreObject* mathUnderObject() override { return objectAttributeValue(AXPropertyName::MathUnderObject); }
    AXCoreObject* mathOverObject() override { return objectAttributeValue(AXPropertyName::MathOverObject); }
    AXCoreObject* mathNumeratorObject() override { return objectAttributeValue(AXPropertyName::MathNumeratorObject); }
    AXCoreObject* mathDenominatorObject() override { return objectAttributeValue(AXPropertyName::MathDenominatorObject); }
    AXCoreObject* mathBaseObject() override { return objectAttributeValue(AXPropertyName::MathBaseObject); }
    AXCoreObject* mathSubscriptObject() override { return objectAttributeValue(AXPropertyName::MathSubscriptObject); }
    AXCoreObject* mathSuperscriptObject() override { return objectAttributeValue(AXPropertyName::MathSuperscriptObject); }
    String mathFencedOpenString() const override { return stringAttributeValue(AXPropertyName::MathFencedOpenString); }
    String mathFencedCloseString() const override { return stringAttributeValue(AXPropertyName::MathFencedCloseString); }
    int mathLineThickness() const override { return intAttributeValue(AXPropertyName::MathLineThickness); }
    bool isAnonymousMathOperator() const override { return boolAttributeValue(AXPropertyName::IsAnonymousMathOperator); }
    void mathPrescripts(AccessibilityMathMultiscriptPairs&) override;
    void mathPostscripts(AccessibilityMathMultiscriptPairs&) override;
    bool fileUploadButtonReturnsValueInTitle() const override { return boolAttributeValue(AXPropertyName::FileUploadButtonReturnsValueInTitle); }
    String speechHintAttributeValue() const override { return stringAttributeValue(AXPropertyName::SpeechHint); }
    String descriptionAttributeValue() const override { return stringAttributeValue(AXPropertyName::Description); }
    String helpTextAttributeValue() const override { return stringAttributeValue(AXPropertyName::HelpText); }
    String titleAttributeValue() const override { return stringAttributeValue(AXPropertyName::Title); }
#if PLATFORM(COCOA) && !PLATFORM(IOS_FAMILY)
    bool caretBrowsingEnabled() const override { return boolAttributeValue(AXPropertyName::CaretBrowsingEnabled); }
#endif
    AXCoreObject* focusableAncestor() override { return objectAttributeValue(AXPropertyName::FocusableAncestor); }
    AXCoreObject* editableAncestor() override { return objectAttributeValue(AXPropertyName::EditableAncestor); }
    AXCoreObject* highestEditableAncestor() override { return objectAttributeValue(AXPropertyName::HighestEditableAncestor); }
    AccessibilityOrientation orientation() const override { return static_cast<AccessibilityOrientation>(intAttributeValue(AXPropertyName::Orientation)); }
    unsigned hierarchicalLevel() const override { return unsignedAttributeValue(AXPropertyName::HierarchicalLevel); }
    String language() const override { return stringAttributeValue(AXPropertyName::Language); }
    bool canHaveSelectedChildren() const override { return boolAttributeValue(AXPropertyName::CanHaveSelectedChildren); }
    void selectedChildren(AccessibilityChildrenVector& children) override { fillChildrenVectorForProperty(AXPropertyName::SelectedChildren, children); }
    void visibleChildren(AccessibilityChildrenVector& children) override { fillChildrenVectorForProperty(AXPropertyName::VisibleChildren, children); }
    void tabChildren(AccessibilityChildrenVector& children) override { fillChildrenVectorForProperty(AXPropertyName::TabChildren, children); }
    bool hasARIAValueNow() const override { return boolAttributeValue(AXPropertyName::HasARIAValueNow); }
    String tagName() const override { return stringAttributeValue(AXPropertyName::TagName); }
    const AccessibilityChildrenVector& children(bool updateChildrenIfNeeded = true) override;
    void updateChildrenIfNecessary() override { }
    bool isDetachedFromParent() override;
    bool supportsLiveRegion(bool = true) const override { return boolAttributeValue(AXPropertyName::SupportsLiveRegion); }
    bool isInsideLiveRegion(bool = true) const override { return boolAttributeValue(AXPropertyName::IsInsideLiveRegion); }
    const String liveRegionStatus() const override { return stringAttributeValue(AXPropertyName::LiveRegionStatus); }
    const String liveRegionRelevant() const override { return stringAttributeValue(AXPropertyName::LiveRegionRelevant); }
    bool liveRegionAtomic() const override { return boolAttributeValue(AXPropertyName::LiveRegionAtomic); }
    bool isBusy() const override { return boolAttributeValue(AXPropertyName::IsBusy); }
    bool isInlineText() const override { return boolAttributeValue(AXPropertyName::IsInlineText); }
    // Spin button support.
    AXCoreObject* incrementButton() override { return objectAttributeValue(AXPropertyName::IncrementButton); }
    AXCoreObject* decrementButton() override { return objectAttributeValue(AXPropertyName::DecrementButton); }
    bool isIncrementor() const override { return boolAttributeValue(AXPropertyName::IsIncrementor); }

    String stringValue() const override { return stringAttributeValue(AXPropertyName::StringValue); }

    // Parameterized attribute retrieval.
    Vector<RefPtr<Range>> findTextRanges(AccessibilitySearchTextCriteria const&) const override;
    Vector<String> performTextOperation(AccessibilityTextOperation const&) override;
    void findMatchingObjects(AccessibilitySearchCriteria*, AccessibilityChildrenVector&) override;

    // Attributes retrieved from the root node only so that the data isn't duplicated on each node.
    uint64_t sessionID() const override;
    String documentURI() const override;
    String documentEncoding() const override;
    bool preventKeyboardDOMEventDispatch() const override;

    // TODO: Text ranges and selection.
    PlainTextRange selectedTextRange() const override { return PlainTextRange(); }
    unsigned selectionStart() const override { return 0; }
    unsigned selectionEnd() const override { return 0; }
    VisibleSelection selection() const override { return VisibleSelection(); }
    String selectedText() const override { return String(); }
    VisiblePositionRange visiblePositionRange() const override { return VisiblePositionRange(); }
    VisiblePositionRange visiblePositionRangeForLine(unsigned) const override { return VisiblePositionRange(); }
    RefPtr<Range> elementRange() const override { return nullptr; }
    VisiblePositionRange visiblePositionRangeForUnorderedPositions(const VisiblePosition&, const VisiblePosition&) const override { return VisiblePositionRange(); }
    VisiblePositionRange positionOfLeftWord(const VisiblePosition&) const override { return VisiblePositionRange(); }
    VisiblePositionRange positionOfRightWord(const VisiblePosition&) const override { return VisiblePositionRange(); }
    VisiblePositionRange leftLineVisiblePositionRange(const VisiblePosition&) const override { return VisiblePositionRange(); }
    VisiblePositionRange rightLineVisiblePositionRange(const VisiblePosition&) const override { return VisiblePositionRange(); }
    VisiblePositionRange sentenceForPosition(const VisiblePosition&) const override { return VisiblePositionRange(); }
    VisiblePositionRange paragraphForPosition(const VisiblePosition&) const override { return VisiblePositionRange(); }
    VisiblePositionRange styleRangeForPosition(const VisiblePosition&) const override { return VisiblePositionRange(); }
    VisiblePositionRange visiblePositionRangeForRange(const PlainTextRange&) const override { return VisiblePositionRange(); }
    VisiblePositionRange lineRangeForPosition(const VisiblePosition&) const override { return VisiblePositionRange(); }
    RefPtr<Range> rangeForPlainTextRange(const PlainTextRange&) const override { return nullptr; }
    String stringForRange(RefPtr<Range>) const override { return String(); }
    IntRect boundsForVisiblePositionRange(const VisiblePositionRange&) const override { return IntRect(); }
    IntRect boundsForRange(const RefPtr<Range>) const override { return IntRect(); }
    int lengthForVisiblePositionRange(const VisiblePositionRange&) const override { return 0; }
    void setSelectedVisiblePositionRange(const VisiblePositionRange&) const override { }
    VisiblePosition visiblePositionForBounds(const IntRect&, AccessibilityVisiblePositionForBounds) const override { return VisiblePosition(); }
    VisiblePosition visiblePositionForPoint(const IntPoint&) const override { return VisiblePosition(); }
    VisiblePosition nextVisiblePosition(const VisiblePosition&) const override { return VisiblePosition(); }
    VisiblePosition previousVisiblePosition(const VisiblePosition&) const override { return VisiblePosition(); }
    VisiblePosition nextWordEnd(const VisiblePosition&) const override { return VisiblePosition(); }
    VisiblePosition previousWordStart(const VisiblePosition&) const override { return VisiblePosition(); }
    VisiblePosition nextLineEndPosition(const VisiblePosition&) const override { return VisiblePosition(); }
    VisiblePosition previousLineStartPosition(const VisiblePosition&) const override { return VisiblePosition(); }
    VisiblePosition nextSentenceEndPosition(const VisiblePosition&) const override { return VisiblePosition(); }
    VisiblePosition previousSentenceStartPosition(const VisiblePosition&) const override { return VisiblePosition(); }
    VisiblePosition nextParagraphEndPosition(const VisiblePosition&) const override { return VisiblePosition(); }
    VisiblePosition previousParagraphStartPosition(const VisiblePosition&) const override { return VisiblePosition(); }
    VisiblePosition visiblePositionForIndex(unsigned, bool /*lastIndexOK */) const override { return VisiblePosition(); }
    VisiblePosition visiblePositionForIndex(int) const override { return VisiblePosition(); }
    int indexForVisiblePosition(const VisiblePosition&) const override { return 0; }
    AXCoreObject* accessibilityObjectForPosition(const VisiblePosition&) const override { return nullptr; }
    int lineForPosition(const VisiblePosition&) const override { return 0; }
    PlainTextRange plainTextRangeForVisiblePositionRange(const VisiblePositionRange&) const override { return PlainTextRange(); }
    int index(const VisiblePosition&) const override { return 0; }
    void lineBreaks(Vector<int>&) const override { }
    PlainTextRange doAXRangeForLine(unsigned) const override { return PlainTextRange(); }
    PlainTextRange doAXRangeForPosition(const IntPoint&) const override { return PlainTextRange(); }
    PlainTextRange doAXRangeForIndex(unsigned) const override { return PlainTextRange(); }
    PlainTextRange doAXStyleRangeForIndex(unsigned) const override { return PlainTextRange(); }
    String doAXStringForRange(const PlainTextRange&) const override { return String(); }
    IntRect doAXBoundsForRange(const PlainTextRange&) const override { return IntRect(); }
    IntRect doAXBoundsForRangeUsingCharacterOffset(const PlainTextRange&) const override { return IntRect(); }
    unsigned doAXLineForIndex(unsigned) override { return 0; }

    // Attribute setters.
    void setARIAGrabbed(bool) override;
    void setIsExpanded(bool) override;
    void setValue(float) override;
    void setSelected(bool) override;
    void setSelectedRows(AccessibilityChildrenVector&) override;
    void setFocused(bool) override;
    void setSelectedText(const String&) override;
    void setSelectedTextRange(const PlainTextRange&) override;
    void setValue(const String&) override;
#if PLATFORM(COCOA) && !PLATFORM(IOS_FAMILY)
    void setCaretBrowsingEnabled(bool) override;
#endif
    void setPreventKeyboardDOMEventDispatch(bool) override;

    // TODO: Functions
    String textUnderElement(AccessibilityTextUnderElementMode = AccessibilityTextUnderElementMode()) const override { return String(); }
    RefPtr<Range> getMisspellingRange(RefPtr<Range> const&, AccessibilitySearchDirection) const override { return nullptr; }
    FloatRect convertFrameToSpace(const FloatRect&, AccessibilityConversionSpace) const override { return FloatRect(); }
    void increment() override { }
    void decrement() override { }
    void scrollToMakeVisible() const override { }
    void scrollToMakeVisibleWithSubFocus(const IntRect&) const override { }
    void scrollToGlobalPoint(const IntPoint&) const override { }
    bool replaceTextInRange(const String&, const PlainTextRange&) override;
    bool insertText(const String&) override;
    void makeRangeVisible(const PlainTextRange&) override { }
    bool press() override;
    bool performDefaultAction() override;

    // Functions that should never be called on an isolated tree object. ASSERT that these are not reached;
    bool isAccessibilityObject() const override;
    bool isAccessibilityNodeObject() const override;
    bool isAccessibilityRenderObject() const override;
    bool isAccessibilityScrollbar() const override;
    bool isAccessibilityScrollView() const override;
    bool isAccessibilitySVGRoot() const override;
    bool isAccessibilitySVGElement() const override;
    bool isAttachmentElement() const override;
    bool isNativeImage() const override;
    bool isImageButton() const override;
    bool isContainedByPasswordField() const override;
    AXCoreObject* passwordFieldOrContainingPasswordField() override;
    bool isNativeTextControl() const override;
    bool isNativeListBox() const override;
    bool isListBoxOption() const override;
    bool isSliderThumb() const override;
    bool isInputSlider() const override;
    bool isLabel() const override;
    bool isDataTable() const override;
    bool isImageMapLink() const override;
    bool isNativeSpinButton() const override;
    bool isSpinButtonPart() const override;
    bool isMockObject() const override;
    bool isMediaObject() const override;
    bool isARIATextControl() const override;
    bool isNonNativeTextControl() const override;
    bool isBlockquote() const override;
    bool isFigureElement() const override;
    bool isKeyboardFocusable() const override;
    bool isHovered() const override;
    bool isIndeterminate() const override;
    bool isLoaded() const override { return boolAttributeValue(AXPropertyName::IsLoaded); }
    bool isOnScreen() const override;
    bool isOffScreen() const override;
    bool isPressed() const override;
    bool isUnvisited() const override;
    bool isLinked() const override;
    bool isVisible() const override;
    bool isCollapsed() const override;
    bool isSelectedOptionActive() const override;
    bool hasBoldFont() const override;
    bool hasItalicFont() const override;
    bool hasMisspelling() const override;
    bool hasPlainText() const override;
    bool hasSameFont(RenderObject*) const override;
    bool hasSameFontColor(RenderObject*) const override;
    bool hasSameStyle(RenderObject*) const override;
    bool hasUnderline() const override;
    bool hasHighlighting() const override;
    Element* element() const override;
    Node* node() const override;
    RenderObject* renderer() const override;
    AccessibilityObjectInclusion defaultObjectInclusion() const override;
    bool accessibilityIsIgnoredByDefault() const override;
    float stepValueForRange() const override;
    AXCoreObject* selectedListItem() override;
    void ariaActiveDescendantReferencingElements(AccessibilityChildrenVector&) const override;
    void ariaControlsReferencingElements(AccessibilityChildrenVector&) const override;
    void ariaDescribedByElements(AccessibilityChildrenVector&) const override;
    void ariaDescribedByReferencingElements(AccessibilityChildrenVector&) const override;
    void ariaDetailsReferencingElements(AccessibilityChildrenVector&) const override;
    void ariaErrorMessageReferencingElements(AccessibilityChildrenVector&) const override;
    void ariaFlowToReferencingElements(AccessibilityChildrenVector&) const override;
    void ariaLabelledByElements(AccessibilityChildrenVector&) const override;
    void ariaLabelledByReferencingElements(AccessibilityChildrenVector&) const override;
    void ariaOwnsReferencingElements(AccessibilityChildrenVector&) const override;
    bool hasDatalist() const override;
    bool supportsHasPopup() const override;
    bool supportsPressed() const override;
    bool supportsChecked() const override;
    bool ignoredFromModalPresence() const override;
    bool isModalDescendant(Node*) const override;
    bool isModalNode() const override;
    AXCoreObject* elementAccessibilityHitTest(const IntPoint&) const override;
    AXCoreObject* firstChild() const override;
    AXCoreObject* lastChild() const override;
    AXCoreObject* previousSibling() const override;
    AXCoreObject* nextSibling() const override;
    AXCoreObject* nextSiblingUnignored(int limit) const override;
    AXCoreObject* previousSiblingUnignored(int limit) const override;
    AXCoreObject* parentObjectIfExists() const override;
    bool isDescendantOfBarrenParent() const override;
    bool isDescendantOfRole(AccessibilityRole) const override;
    AXCoreObject* observableObject() const override;
    AXCoreObject* correspondingLabelForControlElement() const override;
    AXCoreObject* correspondingControlForLabelElement() const override;
    bool isPresentationalChildOfAriaRole() const override;
    bool ariaRoleHasPresentationalChildren() const override;
    bool inheritsPresentationalRole() const override;
    void setAccessibleName(const AtomString&) override;
    bool hasAttributesRequiredForInclusion() const override;
    String accessibilityDescription() const override;
    String title() const override;
    String helpText() const override;
    bool isARIAStaticText() const override;
    String text() const override;
    String ariaLabeledByAttribute() const override;
    String ariaDescribedByAttribute() const override;
    bool accessibleNameDerivesFromContent() const override;
    void elementsFromAttribute(Vector<Element*>&, const QualifiedName&) const override;
    AXObjectCache* axObjectCache() const override;
    Element* anchorElement() const override;
    Element* actionElement() const override;
    Path elementPath() const override;
    bool supportsPath() const override { return boolAttributeValue(AXPropertyName::SupportsPath); }
    TextIteratorBehavior textIteratorBehaviorForTextRange() const override;
    Widget* widget() const override;
    Widget* widgetForAttachmentView() const override;
    Page* page() const override;
    Document* document() const override;
    FrameView* documentFrameView() const override;
    Frame* frame() const override;
    Frame* mainFrame() const override;
    Document* topDocument() const override;
    ScrollView* scrollViewAncestor() const override;
    void childrenChanged() override;
    void textChanged() override;
    void updateAccessibilityRole() override;
    void addChildren() override;
    void addChild(AXCoreObject*) override;
    void insertChild(AXCoreObject*, unsigned) override;
    bool shouldIgnoreAttributeRole() const override;
    bool canHaveChildren() const override;
    bool hasChildren() const override;
    void setNeedsToUpdateChildren() override;
    void setNeedsToUpdateSubtree() override;
    void clearChildren() override;
    bool needsToUpdateChildren() const override;
    void detachFromParent() override;
    bool shouldFocusActiveDescendant() const override;
    AXCoreObject* activeDescendant() const override;
    void handleActiveDescendantChanged() override;
    void handleAriaExpandedChanged() override;
    bool isDescendantOfObject(const AXCoreObject*) const override;
    bool isAncestorOfObject(const AXCoreObject*) const override;
    AXCoreObject* firstAnonymousBlockChild() const override;
    bool hasAttribute(const QualifiedName&) const override;
    const AtomString& getAttribute(const QualifiedName&) const override;
    bool hasTagName(const QualifiedName&) const override;
    String stringValueForMSAA() const override;
    String stringRoleForMSAA() const override;
    String nameForMSAA() const override;
    String descriptionForMSAA() const override;
    AccessibilityRole roleValueForMSAA() const override;
    String passwordFieldValue() const override;
    AXCoreObject* liveRegionAncestor(bool excludeIfOff = true) const override;
    bool hasContentEditableAttributeSet() const override;
    bool supportsReadOnly() const override;
    bool supportsAutoComplete() const override;
    bool supportsARIAAttributes() const override;
    bool scrollByPage(ScrollByPageDirection) const override;
    IntPoint scrollPosition() const override;
    IntSize scrollContentsSize() const override;
    IntRect scrollVisibleContentRect() const override;
    void scrollToMakeVisible(const ScrollRectToVisibleOptions&) const override;
    bool lastKnownIsIgnoredValue() override;
    void setLastKnownIsIgnoredValue(bool) override;
    void notifyIfIgnoredValueChanged() override;
    bool isMathScriptObject(AccessibilityMathScriptObjectType) const override;
    bool isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType) const override;
    bool isAXHidden() const override;
    bool isDOMHidden() const override;
    bool isHidden() const override;
    void overrideAttachmentParent(AXCoreObject* parent) override;
    bool accessibilityIgnoreAttachment() const override;
    AccessibilityObjectInclusion accessibilityPlatformIncludesObject() const override;
    bool hasApplePDFAnnotationAttribute() const override;
    const AccessibilityScrollView* ancestorAccessibilityScrollView(bool includeSelf) const override;
    void setIsIgnoredFromParentData(AccessibilityIsIgnoredFromParentData&) override;
    void clearIsIgnoredFromParentData() override;
    void setIsIgnoredFromParentDataForChild(AXCoreObject*) override;

    void updateBackingStore() override;

    AXID m_parent { InvalidAXID };
    AXID m_id { InvalidAXID };
    bool m_initialized { false };
    AXIsolatedTreeID m_treeIdentifier;
    RefPtr<AXIsolatedTree> m_cachedTree;
    Vector<AXID> m_childrenIDs;
    Vector<RefPtr<AXCoreObject>> m_children;

    HashMap<AXPropertyName, AttributeValueVariant, WTF::IntHash<AXPropertyName>, WTF::StrongEnumHashTraits<AXPropertyName>> m_attributeMap;
    Lock m_attributeMapLock;
};

} // namespace WebCore

#endif // ENABLE((ACCESSIBILITY_ISOLATED_TREE))
