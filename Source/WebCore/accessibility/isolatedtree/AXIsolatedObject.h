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
#include <variant>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AXIsolatedTree;

class AXIsolatedObject final : public AXCoreObject {
    friend class AXIsolatedTree;
public:
    static Ref<AXIsolatedObject> create(AXCoreObject&, AXIsolatedTree*, AXID parentID);
    ~AXIsolatedObject();

    void setObjectID(AXID id) override { m_id = id; }
    AXID objectID() const override { return m_id; }
    void init() override;

    void attachPlatformWrapper(AccessibilityObjectWrapper*);
    bool isDetached() const override;

    void setParent(AXID);

private:
    void detachRemoteParts(AccessibilityDetachmentType) override;
    void detachPlatformWrapper(AccessibilityDetachmentType) override;

    AXID parent() const { return m_parentID; }

    AXIsolatedTree* tree() const { return m_cachedTree.get(); }

    AXIsolatedObject() = default;
    AXIsolatedObject(AXCoreObject&, AXIsolatedTree*, AXID parentID);
    bool isAXIsolatedObjectInstance() const override { return true; }
    void initializeAttributeData(AXCoreObject&, bool isRoot);
    void initializePlatformProperties(const AXCoreObject&, bool isRoot);
    AXCoreObject* associatedAXObject() const;

    void setProperty(AXPropertyName, AXPropertyValueVariant&&, bool shouldRemove = false);
    void setObjectProperty(AXPropertyName, AXCoreObject*);
    void setObjectVectorProperty(AXPropertyName, const AccessibilityChildrenVector&);

    // FIXME: consolidate all AttributeValue retrieval in a single template method.
    bool boolAttributeValue(AXPropertyName) const;
    String stringAttributeValue(AXPropertyName) const;
    int intAttributeValue(AXPropertyName) const;
    unsigned unsignedAttributeValue(AXPropertyName) const;
    double doubleAttributeValue(AXPropertyName) const;
    float floatAttributeValue(AXPropertyName) const;
    AXCoreObject* objectAttributeValue(AXPropertyName) const;
    PAL::SessionID sessionIDAttributeValue(AXPropertyName) const;
    IntPoint intPointAttributeValue(AXPropertyName) const;
    Color colorAttributeValue(AXPropertyName) const;
    URL urlAttributeValue(AXPropertyName) const;
    uint64_t uint64AttributeValue(AXPropertyName) const;
    Path pathAttributeValue(AXPropertyName) const;
    template<typename T> T rectAttributeValue(AXPropertyName) const;
    template<typename T> Vector<T> vectorAttributeValue(AXPropertyName) const;
    template<typename T> OptionSet<T> optionSetAttributeValue(AXPropertyName) const;
    template<typename T> std::pair<T, T> pairAttributeValue(AXPropertyName) const;
    template<typename T> T propertyValue(AXPropertyName) const;

    // The following method performs a lazy caching of the given property.
    // If the property is already in m_propertyMap, returns the existing value.
    // If not, retrieves the property from the main thread and cache it for later use.
    template<typename T> T getOrRetrievePropertyValue(AXPropertyName);

    void fillChildrenVectorForProperty(AXPropertyName, AccessibilityChildrenVector&) const;
    void setMathscripts(AXPropertyName, AXCoreObject&);
    void insertMathPairs(Vector<std::pair<AXID, AXID>>&, AccessibilityMathMultiscriptPairs&);
    template<typename U> void performFunctionOnMainThread(U&& lambda) const
    {
        Accessibility::performFunctionOnMainThread([&lambda, this]() {
            if (auto* object = associatedAXObject())
                lambda(object);
        });
    }

    // Attribute retrieval overrides.
    bool isHeading() const override { return boolAttributeValue(AXPropertyName::IsHeading); }
    bool isLandmark() const override { return boolAttributeValue(AXPropertyName::IsLandmark); }
    bool isLink() const override { return boolAttributeValue(AXPropertyName::IsLink); }
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
    bool isKeyboardFocusable() const override { return boolAttributeValue(AXPropertyName::IsKeyboardFocusable); }
    
    // Table support.
    bool isTable() const override { return boolAttributeValue(AXPropertyName::IsTable); }
    bool isExposable() const override { return boolAttributeValue(AXPropertyName::IsExposable); }
    bool isDataTable() const override { return boolAttributeValue(AXPropertyName::IsDataTable); }
    int tableLevel() const override { return intAttributeValue(AXPropertyName::TableLevel); }
    bool supportsSelectedRows() const override { return boolAttributeValue(AXPropertyName::SupportsSelectedRows); }
    AccessibilityChildrenVector columns() override { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXPropertyName::Columns)); }
    AccessibilityChildrenVector rows() override { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXPropertyName::Rows)); }
    unsigned columnCount() override { return unsignedAttributeValue(AXPropertyName::ColumnCount); }
    unsigned rowCount() override { return unsignedAttributeValue(AXPropertyName::RowCount); }
    AccessibilityChildrenVector cells() override { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXPropertyName::Cells)); }
    AXCoreObject* cellForColumnAndRow(unsigned, unsigned) override;
    AccessibilityChildrenVector columnHeaders() override { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXPropertyName::ColumnHeaders)); }
    AccessibilityChildrenVector rowHeaders() override { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXPropertyName::RowHeaders)); }
    AccessibilityChildrenVector visibleRows() override { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXPropertyName::VisibleRows)); }
    AXCoreObject* headerContainer() override { return objectAttributeValue(AXPropertyName::HeaderContainer); }
    int axColumnCount() const override { return intAttributeValue(AXPropertyName::AXColumnCount); }
    int axRowCount() const override { return intAttributeValue(AXPropertyName::AXRowCount); }

    // Table cell support.
    bool isTableCell() const override { return boolAttributeValue(AXPropertyName::IsTableCell); }
    // Returns the start location and row span of the cell.
    std::pair<unsigned, unsigned> rowIndexRange() const override { return pairAttributeValue<unsigned>(AXPropertyName::RowIndexRange); }
    // Returns the start location and column span of the cell.
    std::pair<unsigned, unsigned> columnIndexRange() const override { return pairAttributeValue<unsigned>(AXPropertyName::ColumnIndexRange); }
    bool isColumnHeaderCell() const override { return boolAttributeValue(AXPropertyName::IsColumnHeaderCell); }
    bool isRowHeaderCell() const override { return boolAttributeValue(AXPropertyName::IsRowHeaderCell); }
    int axColumnIndex() const override { return intAttributeValue(AXPropertyName::AXColumnIndex); }
    int axRowIndex() const override { return intAttributeValue(AXPropertyName::AXRowIndex); }

    // Table column support.
    bool isTableColumn() const override { return boolAttributeValue(AXPropertyName::IsTableColumn); }
    unsigned columnIndex() const override { return unsignedAttributeValue(AXPropertyName::ColumnIndex); }
    AXCoreObject* columnHeader() override { return objectAttributeValue(AXPropertyName::ColumnHeader); }

    // Table row support.
    bool isTableRow() const override { return boolAttributeValue(AXPropertyName::IsTableRow); }
    unsigned rowIndex() const override { return unsignedAttributeValue(AXPropertyName::RowIndex); }

    // ARIA tree/grid row support.
    bool isARIATreeGridRow() const override { return boolAttributeValue(AXPropertyName::IsARIATreeGridRow); }
    AccessibilityChildrenVector disclosedRows() override { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXPropertyName::DisclosedRows)); }
    AXCoreObject* disclosedByRow() const override { return objectAttributeValue(AXPropertyName::DisclosedByRow); }

    bool isFieldset() const override { return boolAttributeValue(AXPropertyName::IsFieldset); }
    bool isGroup() const override { return boolAttributeValue(AXPropertyName::IsGroup); }
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
    FloatRect relativeFrame() const override;
    bool supportsDatetimeAttribute() const override { return boolAttributeValue(AXPropertyName::SupportsDatetimeAttribute); }
    String datetimeAttributeValue() const override { return stringAttributeValue(AXPropertyName::DatetimeAttributeValue); }
    bool canSetFocusAttribute() const override { return boolAttributeValue(AXPropertyName::CanSetFocusAttribute); }
    bool canSetTextRangeAttributes() const override { return boolAttributeValue(AXPropertyName::CanSetTextRangeAttributes); }
    bool canSetValueAttribute() const override { return boolAttributeValue(AXPropertyName::CanSetValueAttribute); }
    bool canSetNumericValue() const override { return boolAttributeValue(AXPropertyName::CanSetNumericValue); }
    bool canSetSelectedAttribute() const override { return boolAttributeValue(AXPropertyName::CanSetSelectedAttribute); }
    bool canSetSelectedChildren() const override { return boolAttributeValue(AXPropertyName::CanSetSelectedChildren); }
    bool canSetExpandedAttribute() const override { return boolAttributeValue(AXPropertyName::CanSetExpandedAttribute); }
    bool accessibilityIsIgnored() const override { return boolAttributeValue(AXPropertyName::IsAccessibilityIgnored); }
    bool isShowingValidationMessage() const override { return boolAttributeValue(AXPropertyName::IsShowingValidationMessage); }
    String validationMessage() const override { return stringAttributeValue(AXPropertyName::ValidationMessage); }
    unsigned blockquoteLevel() const override { return unsignedAttributeValue(AXPropertyName::BlockquoteLevel); }
    unsigned headingLevel() const override { return unsignedAttributeValue(AXPropertyName::HeadingLevel); }
    AccessibilityButtonState checkboxOrRadioValue() const override { return static_cast<AccessibilityButtonState>(intAttributeValue(AXPropertyName::AccessibilityButtonState)); }
    String valueDescription() const override { return stringAttributeValue(AXPropertyName::ValueDescription); }
    float valueForRange() const override { return floatAttributeValue(AXPropertyName::ValueForRange); }
    float maxValueForRange() const override { return floatAttributeValue(AXPropertyName::MaxValueForRange); }
    float minValueForRange() const override { return floatAttributeValue(AXPropertyName::MinValueForRange); }
    AXCoreObject* selectedRadioButton() override { return objectAttributeValue(AXPropertyName::SelectedRadioButton); }
    AXCoreObject* selectedTabItem() override { return objectAttributeValue(AXPropertyName::SelectedTabItem); }
    int layoutCount() const override { return intAttributeValue(AXPropertyName::LayoutCount); }
    double loadingProgress() const override { return tree()->loadingProgress(); }
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
    bool supportsDropping() const override { return boolAttributeValue(AXPropertyName::SupportsDropping); }
    bool supportsDragging() const override { return boolAttributeValue(AXPropertyName::SupportsDragging); }
    bool isGrabbed() override { return boolAttributeValue(AXPropertyName::IsGrabbed); }
    Vector<String> determineDropEffects() const override { return vectorAttributeValue<String>(AXPropertyName::DropEffects); }
    AXCoreObject* accessibilityHitTest(const IntPoint&) const override;
    AXCoreObject* focusedUIElement() const override;
    AXCoreObject* parentObject() const override { return parentObjectUnignored(); }
    AXCoreObject* parentObjectUnignored() const override;
    void linkedUIElements(AccessibilityChildrenVector& children) const override { fillChildrenVectorForProperty(AXPropertyName::LinkedUIElements, children); }
    AXCoreObject* titleUIElement() const override { return objectAttributeValue(AXPropertyName::TitleUIElement); }
    AXCoreObject* scrollBar(AccessibilityOrientation) override;
    AccessibilityRole ariaRoleAttribute() const override { return static_cast<AccessibilityRole>(intAttributeValue(AXPropertyName::ARIARoleAttribute)); }
    String computedLabel() override;
    int textLength() const override { return intAttributeValue(AXPropertyName::TextLength); }
    const String placeholderValue() const override { return stringAttributeValue(AXPropertyName::PlaceholderValue); }
    String expandedTextValue() const override { return stringAttributeValue(AXPropertyName::ExpandedTextValue); }
    bool supportsExpandedTextValue() const override { return boolAttributeValue(AXPropertyName::SupportsExpandedTextValue); }
    SRGBA<uint8_t> colorValue() const override;
    AccessibilityRole roleValue() const override { return static_cast<AccessibilityRole>(intAttributeValue(AXPropertyName::RoleValue)); }
    String rolePlatformString() const override { return stringAttributeValue(AXPropertyName::RolePlatformString); }
    String roleDescription() const override { return stringAttributeValue(AXPropertyName::RoleDescription); }
    String subrolePlatformString() const override { return stringAttributeValue(AXPropertyName::SubrolePlatformString); }
    String ariaLandmarkRoleDescription() const override { return stringAttributeValue(AXPropertyName::ARIALandmarkRoleDescription); }
    bool supportsPressAction() const override { return boolAttributeValue(AXPropertyName::SupportsPressAction); }
    LayoutRect boundingBoxRect() const override;
    LayoutRect elementRect() const override;
    IntPoint clickPoint() override { return intPointAttributeValue(AXPropertyName::ClickPoint); }
    void accessibilityText(Vector<AccessibilityText>& texts) const override;
    String brailleLabel() const override { return stringAttributeValue(AXPropertyName::BrailleLabel); }
    String brailleRoleDescription() const override { return stringAttributeValue(AXPropertyName::BrailleRoleDescription); }
    String embeddedImageDescription() const override { return stringAttributeValue(AXPropertyName::EmbeddedImageDescription); }
    std::optional<AccessibilityChildrenVector> imageOverlayElements() override { return std::nullopt; }

    String computedRoleString() const override { return stringAttributeValue(AXPropertyName::ComputedRoleString); }
    bool isValueAutofilled() const override { return boolAttributeValue(AXPropertyName::IsValueAutofilled); }
    bool isValueAutofillAvailable() const override { return boolAttributeValue(AXPropertyName::IsValueAutofillAvailable); }
    AutoFillButtonType valueAutofillButtonType() const override { return static_cast<AutoFillButtonType>(intAttributeValue(AXPropertyName::ValueAutofillButtonType)); }
    void ariaTreeRows(AccessibilityChildrenVector& children) override { fillChildrenVectorForProperty(AXPropertyName::ARIATreeRows, children); }
    void ariaTreeItemContent(AccessibilityChildrenVector& children) override { fillChildrenVectorForProperty(AXPropertyName::ARIATreeItemContent, children); }
    URL url() const override { return urlAttributeValue(AXPropertyName::URL); }
    String accessKey() const override { return stringAttributeValue(AXPropertyName::AccessKey); }
    String localizedActionVerb() const override { return stringAttributeValue(AXPropertyName::LocalizedActionVerb); }
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
    std::optional<AccessibilityChildrenVector> mathRadicand() override;
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
#if PLATFORM(COCOA)
    bool fileUploadButtonReturnsValueInTitle() const override { return boolAttributeValue(AXPropertyName::FileUploadButtonReturnsValueInTitle); }
    String speechHintAttributeValue() const override { return stringAttributeValue(AXPropertyName::SpeechHint); }
    String descriptionAttributeValue() const override;
    String helpTextAttributeValue() const override { return stringAttributeValue(AXPropertyName::HelpText); }
    String titleAttributeValue() const override;
#endif
#if PLATFORM(MAC)
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
    void setSelectedChildren(const AccessibilityChildrenVector&) override;
    void visibleChildren(AccessibilityChildrenVector& children) override { fillChildrenVectorForProperty(AXPropertyName::VisibleChildren, children); }
    void tabChildren(AccessibilityChildrenVector& children) override { fillChildrenVectorForProperty(AXPropertyName::TabChildren, children); }
    AccessibilityChildrenVector contents() override;
    bool hasARIAValueNow() const override { return boolAttributeValue(AXPropertyName::HasARIAValueNow); }
    String tagName() const override { return stringAttributeValue(AXPropertyName::TagName); }
    const AccessibilityChildrenVector& children(bool updateChildrenIfNeeded = true) override;
    void updateChildrenIfNecessary() override;
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
    AccessibilityChildrenVector documentLinks() override { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXPropertyName::DocumentLinks)); }
    bool supportsCheckedState() const override { return boolAttributeValue(AXPropertyName::SupportsCheckedState); }

    String stringValue() const override { return stringAttributeValue(AXPropertyName::StringValue); }

    // Parameterized attribute retrieval.
    Vector<SimpleRange> findTextRanges(const AccessibilitySearchTextCriteria&) const override;
    Vector<String> performTextOperation(AccessibilityTextOperation const&) override;
    void findMatchingObjects(AccessibilitySearchCriteria*, AccessibilityChildrenVector&) override;

    // Attributes retrieved from the root node only so that the data isn't duplicated on each node.
    PAL::SessionID sessionID() const override;
    String documentURI() const override;
    String documentEncoding() const override;
#if PLATFORM(COCOA)
    bool preventKeyboardDOMEventDispatch() const override;
#endif

    // PlainTextRange support.
    PlainTextRange selectedTextRange() const override;
    int insertionPointLineNumber() const override;
    PlainTextRange doAXRangeForLine(unsigned) const override;
    String doAXStringForRange(const PlainTextRange&) const override;
    PlainTextRange doAXRangeForPosition(const IntPoint&) const override;
    PlainTextRange doAXRangeForIndex(unsigned) const override;
    PlainTextRange doAXStyleRangeForIndex(unsigned) const override;
    IntRect doAXBoundsForRangeUsingCharacterOffset(const PlainTextRange&) const override;
    IntRect doAXBoundsForRange(const PlainTextRange&) const override;
    unsigned doAXLineForIndex(unsigned) override;

    VisibleSelection selection() const override;
    VisiblePositionRange selectedVisiblePositionRange() const override;
    void setSelectedVisiblePositionRange(const VisiblePositionRange&) const override;

    // TODO: Text ranges and selection.
    String selectedText() const override;
    VisiblePositionRange visiblePositionRange() const override;
    VisiblePositionRange visiblePositionRangeForLine(unsigned) const override;
    std::optional<SimpleRange> elementRange() const override;
    VisiblePositionRange visiblePositionRangeForUnorderedPositions(const VisiblePosition&, const VisiblePosition&) const override;
    VisiblePositionRange positionOfLeftWord(const VisiblePosition&) const override;
    VisiblePositionRange positionOfRightWord(const VisiblePosition&) const override;
    VisiblePositionRange leftLineVisiblePositionRange(const VisiblePosition&) const override;
    VisiblePositionRange rightLineVisiblePositionRange(const VisiblePosition&) const override;
    VisiblePositionRange sentenceForPosition(const VisiblePosition&) const override;
    VisiblePositionRange paragraphForPosition(const VisiblePosition&) const override;
    VisiblePositionRange styleRangeForPosition(const VisiblePosition&) const override;
    VisiblePositionRange visiblePositionRangeForRange(const PlainTextRange&) const override;
    VisiblePositionRange lineRangeForPosition(const VisiblePosition&) const override;
    std::optional<SimpleRange> rangeForPlainTextRange(const PlainTextRange&) const override;
#if PLATFORM(MAC)
    AXTextMarkerRangeRef textMarkerRangeForNSRange(const NSRange&) const override;
#endif
    String stringForRange(const SimpleRange&) const override;
    IntRect boundsForVisiblePositionRange(const VisiblePositionRange&) const override;
    IntRect boundsForRange(const SimpleRange&) const override;
    int lengthForVisiblePositionRange(const VisiblePositionRange&) const override;
    VisiblePosition visiblePositionForBounds(const IntRect&, AccessibilityVisiblePositionForBounds) const override;
    VisiblePosition visiblePositionForPoint(const IntPoint&) const override;
    VisiblePosition nextVisiblePosition(const VisiblePosition&) const override;
    VisiblePosition previousVisiblePosition(const VisiblePosition&) const override;
    VisiblePosition nextWordEnd(const VisiblePosition&) const override;
    VisiblePosition previousWordStart(const VisiblePosition&) const override;
    VisiblePosition nextLineEndPosition(const VisiblePosition&) const override;
    VisiblePosition previousLineStartPosition(const VisiblePosition&) const override;
    VisiblePosition nextSentenceEndPosition(const VisiblePosition&) const override;
    VisiblePosition previousSentenceStartPosition(const VisiblePosition&) const override;
    VisiblePosition nextParagraphEndPosition(const VisiblePosition&) const override;
    VisiblePosition previousParagraphStartPosition(const VisiblePosition&) const override;
    VisiblePosition visiblePositionForIndex(unsigned, bool lastIndexOK) const override;
    VisiblePosition visiblePositionForIndex(int) const override;
    int indexForVisiblePosition(const VisiblePosition&) const override;
    AXCoreObject* accessibilityObjectForPosition(const VisiblePosition&) const override;
    int lineForPosition(const VisiblePosition&) const override;
    PlainTextRange plainTextRangeForVisiblePositionRange(const VisiblePositionRange&) const override;
    std::optional<SimpleRange> visibleCharacterRange() const override;
    int index(const VisiblePosition&) const override;
    void lineBreaks(Vector<int>&) const override;
    FloatRect unobscuredContentRect() const override;
    
    // Attribute setters.
    void setARIAGrabbed(bool) override;
    void setIsExpanded(bool) override;
    bool setValue(float) override;
    void setSelected(bool) override;
    void setSelectedRows(AccessibilityChildrenVector&) override;
    void setFocused(bool) override;
    void setSelectedText(const String&) override;
    void setSelectedTextRange(const PlainTextRange&) override;
    bool setValue(const String&) override;
#if PLATFORM(MAC)
    void setCaretBrowsingEnabled(bool) override;
#endif
#if PLATFORM(COCOA)
    void setPreventKeyboardDOMEventDispatch(bool) override;
#endif

    String textUnderElement(AccessibilityTextUnderElementMode = AccessibilityTextUnderElementMode()) const override;
    std::optional<SimpleRange> misspellingRange(const SimpleRange&, AccessibilitySearchDirection) const override;
    FloatRect convertFrameToSpace(const FloatRect&, AccessibilityConversionSpace) const override;
    void increment() override;
    void decrement() override;
    bool performDismissAction() override;
    void scrollToMakeVisible() const override;
    void scrollToMakeVisibleWithSubFocus(const IntRect&) const override;
    void scrollToGlobalPoint(const IntPoint&) const override;
    bool replaceTextInRange(const String&, const PlainTextRange&) override;
    bool insertText(const String&) override;
    void makeRangeVisible(const PlainTextRange&) override;
    bool press() override;
    bool performDefaultAction() override;

    // Functions that should never be called on an isolated tree object. ASSERT that these are not reached;
    bool isAccessibilityObject() const override;
    bool isAccessibilityNodeObject() const override;
    bool isAccessibilityRenderObject() const override;
    bool isAccessibilityScrollbar() const override;
    bool isAccessibilityScrollViewInstance() const override;
    bool isAXImageInstance() const override;
    bool isAccessibilitySVGRoot() const override;
    bool isAccessibilitySVGElement() const override;
    bool isAccessibilityTableInstance() const override;
    bool isAccessibilityTableColumnInstance() const override;
    bool isAccessibilityProgressIndicatorInstance() const override;
    bool isAccessibilityListBoxInstance() const override;

    bool isAttachmentElement() const override;
    bool isNativeImage() const override;
    bool isImageButton() const override;
    bool isContainedByPasswordField() const override;
    AXCoreObject* passwordFieldOrContainingPasswordField() override;
    bool isNativeTextControl() const override;
    bool isListBoxOption() const override;
    bool isSliderThumb() const override;
    bool isInputSlider() const override;
    bool isLabel() const override;
    bool isImageMapLink() const override;
    bool isNativeSpinButton() const override;
    bool isSpinButtonPart() const override;
    bool isMockObject() const override;
    bool isMediaObject() const override;
    bool isARIATextControl() const override;
    bool isNonNativeTextControl() const override;
    bool isFigureElement() const override;
    bool isHovered() const override;
    bool isIndeterminate() const override;
    bool isLoaded() const override { return loadingProgress() >= 1; }
    bool isOnScreen() const override;
    bool isOffScreen() const override;
    bool isPressed() const override;
    bool isUnvisited() const override { return boolAttributeValue(AXPropertyName::IsUnvisited); }
    bool isLinked() const override;
    bool isVisible() const override { return boolAttributeValue(AXPropertyName::IsVisible); }
    bool isCollapsed() const override;
    bool isSelectedOptionActive() const override;
    bool hasBoldFont() const override { return boolAttributeValue(AXPropertyName::HasBoldFont); }
    bool hasItalicFont() const override { return boolAttributeValue(AXPropertyName::HasItalicFont); }
    bool hasMisspelling() const override;
    bool hasPlainText() const override { return boolAttributeValue(AXPropertyName::HasPlainText); }
    bool hasSameFont(const AXCoreObject&) const override;
    bool hasSameFontColor(const AXCoreObject&) const override;
    bool hasSameStyle(const AXCoreObject&) const override;
    bool hasUnderline() const override { return boolAttributeValue(AXPropertyName::HasUnderline); }
    bool hasHighlighting() const override { return boolAttributeValue(AXPropertyName::HasHighlighting); }
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
    bool supportsRowCountChange() const override { return boolAttributeValue(AXPropertyName::SupportsRowCountChange); }
    bool isModalDescendant(Node*) const override;
    bool isModalNode() const override;
    AXCoreObject* elementAccessibilityHitTest(const IntPoint&) const override;
    AXCoreObject* firstChild() const override;
    AXCoreObject* lastChild() const override;
    AXCoreObject* previousSibling() const override { return objectAttributeValue(AXPropertyName::PreviousSibling); }
    AXCoreObject* nextSibling() const override { return objectAttributeValue(AXPropertyName::NextSibling); }
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
    String accessibilityDescription() const override { return stringAttributeValue(AXPropertyName::AccessibilityDescription); }
    String title() const override { return stringAttributeValue(AXPropertyName::Title); }
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
    Path elementPath() const override { return pathAttributeValue(AXPropertyName::Path); };
    bool supportsPath() const override { return boolAttributeValue(AXPropertyName::SupportsPath); }
    TextIteratorBehaviors textIteratorBehaviorForTextRange() const override;

    bool isWidget() const override { return boolAttributeValue(AXPropertyName::IsWidget); }
    Widget* widget() const override;
    PlatformWidget platformWidget() const override;
    Widget* widgetForAttachmentView() const override;

    HashMap<String, AXEditingStyleValueVariant> resolvedEditingStyles() const override;
#if PLATFORM(COCOA)
    RemoteAXObjectRef remoteParentObject() const override;
    FloatRect convertRectToPlatformSpace(const FloatRect&, AccessibilityConversionSpace) const override;
#endif
    Page* page() const override;
    Document* document() const override;
    FrameView* documentFrameView() const override;
    Frame* frame() const override;
    Frame* mainFrame() const override;
    Document* topDocument() const override;
    ScrollView* scrollView() const override;
    ScrollView* scrollViewAncestor() const override;
    void addChildren() override;
    void addChild(AXCoreObject*, DescendIfIgnored = DescendIfIgnored::Yes) override;
    void insertChild(AXCoreObject*, unsigned, DescendIfIgnored = DescendIfIgnored::Yes) override;
    bool canHaveChildren() const override;
    void setNeedsToUpdateChildren() override;
    void setNeedsToUpdateSubtree() override;
    void clearChildren() override;
    bool needsToUpdateChildren() const override;
    void detachFromParent() override;
    bool shouldFocusActiveDescendant() const override;
    AXCoreObject* activeDescendant() const override;
    void handleActiveDescendantChanged() override;

    OptionSet<AXAncestorFlag> ancestorFlags() const;

    bool hasDocumentRoleAncestor() const override { return ancestorFlags().contains(AXAncestorFlag::HasDocumentRoleAncestor); }
    bool hasWebApplicationAncestor() const override { return ancestorFlags().contains(AXAncestorFlag::HasWebApplicationAncestor); }
    bool isInDescriptionListDetail() const override { return ancestorFlags().contains(AXAncestorFlag::IsInDescriptionListDetail); }
    bool isInDescriptionListTerm() const override { return ancestorFlags().contains(AXAncestorFlag::IsInDescriptionListTerm); }
    bool isInCell() const override { return ancestorFlags().contains(AXAncestorFlag::IsInCell); }

    AXCoreObject* firstAnonymousBlockChild() const override;
    std::optional<String> attributeValue(const String&) const override;
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
    bool isMathScriptObject(AccessibilityMathScriptObjectType) const override;
    bool isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType) const override;
    bool isAXHidden() const override;
    bool isDOMHidden() const override;
    bool isHidden() const override;
    void overrideAttachmentParent(AXCoreObject* parent) override;
    bool accessibilityIgnoreAttachment() const override;
    AccessibilityObjectInclusion accessibilityPlatformIncludesObject() const override;
#if PLATFORM(COCOA)
    bool hasApplePDFAnnotationAttribute() const override { return boolAttributeValue(AXPropertyName::HasApplePDFAnnotationAttribute); }
#endif
    const AccessibilityScrollView* ancestorAccessibilityScrollView(bool includeSelf) const override;
    AXCoreObject* webAreaObject() const override { return objectAttributeValue(AXPropertyName::WebArea); }
    void setIsIgnoredFromParentData(AccessibilityIsIgnoredFromParentData&) override;
    void clearIsIgnoredFromParentData() override;
    void setIsIgnoredFromParentDataForChild(AXCoreObject*) override;

#if PLATFORM(COCOA) && ENABLE(MODEL_ELEMENT)
    Vector<RetainPtr<id>> modelElementChildren() override;
#endif
    
    void updateBackingStore() override;

    String innerHTML() const override;
    String outerHTML() const override;

    RefPtr<AXIsolatedTree> m_cachedTree;
    AXID m_parentID;
    AXID m_id;
    Vector<AXID> m_childrenIDs;
    Vector<RefPtr<AXCoreObject>> m_children;
    AXPropertyMap m_propertyMap;

#if PLATFORM(COCOA)
    RetainPtr<NSView> m_platformWidget;
    RetainPtr<RemoteAXObjectRef> m_remoteParent;
#else
    PlatformWidget m_platformWidget;
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AXIsolatedObject, isAXIsolatedObjectInstance())

#endif // ENABLE((ACCESSIBILITY_ISOLATED_TREE))
