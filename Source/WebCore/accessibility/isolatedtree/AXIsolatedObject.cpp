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

#include "config.h"

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
#include "AXIsolatedObject.h"

#include "AXIsolatedTree.h"

namespace WebCore {

AXIsolatedObject::AXIsolatedObject(AXCoreObject& object, bool isRoot)
    : m_id(object.objectID())
{
    ASSERT(isMainThread());
    initializeAttributeData(object, isRoot);
    m_initialized = true;
}

Ref<AXIsolatedObject> AXIsolatedObject::create(AXCoreObject& object, bool isRoot)
{
    return adoptRef(*new AXIsolatedObject(object, isRoot));
}

AXIsolatedObject::~AXIsolatedObject() = default;

void AXIsolatedObject::initializeAttributeData(AXCoreObject& object, bool isRoot)
{
    setProperty(AXPropertyName::ARIALandmarkRoleDescription, object.ariaLandmarkRoleDescription().isolatedCopy());
    setProperty(AXPropertyName::BoundingBoxRect, object.boundingBoxRect());
    setProperty(AXPropertyName::Description, object.descriptionAttributeValue().isolatedCopy());
    setProperty(AXPropertyName::ElementRect, object.elementRect());
    setProperty(AXPropertyName::HelpText, object.helpTextAttributeValue().isolatedCopy());
    setProperty(AXPropertyName::IsARIATreeGridRow, object.isARIATreeGridRow());
    setProperty(AXPropertyName::IsAccessibilityIgnored, object.accessibilityIsIgnored());
    setProperty(AXPropertyName::IsActiveDescendantOfFocusedContainer, object.isActiveDescendantOfFocusedContainer());
    setProperty(AXPropertyName::IsAttachment, object.isAttachment());
    setProperty(AXPropertyName::IsBusy, object.isBusy());
    setProperty(AXPropertyName::IsButton, object.isButton());
    setProperty(AXPropertyName::IsChecked, object.isChecked());
    setProperty(AXPropertyName::IsCollapsed, object.isCollapsed());
    setProperty(AXPropertyName::IsControl, object.isControl());
    setProperty(AXPropertyName::IsDescriptionList, object.isDescriptionList());
    setProperty(AXPropertyName::IsEnabled, object.isEnabled());
    setProperty(AXPropertyName::IsExpanded, object.isExpanded());
    setProperty(AXPropertyName::IsFieldset, object.isFieldset());
    setProperty(AXPropertyName::IsFileUploadButton, object.isFileUploadButton());
    setProperty(AXPropertyName::IsFocused, object.isFocused());
    setProperty(AXPropertyName::IsGroup, object.isGroup());
    setProperty(AXPropertyName::IsHeading, object.isHeading());
    setProperty(AXPropertyName::IsHovered, object.isHovered());
    setProperty(AXPropertyName::IsImage, object.isImage());
    setProperty(AXPropertyName::IsImageMapLink, object.isImageMapLink());
    setProperty(AXPropertyName::IsIndeterminate, object.isIndeterminate());
    setProperty(AXPropertyName::IsInlineText, object.isInlineText());
    setProperty(AXPropertyName::IsInputImage, object.isInputImage());
    setProperty(AXPropertyName::IsLandmark, object.isLandmark());
    setProperty(AXPropertyName::IsLink, object.isLink());
    setProperty(AXPropertyName::IsLinked, object.isLinked());
    setProperty(AXPropertyName::IsList, object.isList());
    setProperty(AXPropertyName::IsLoaded, object.isLoaded());
    setProperty(AXPropertyName::IsMediaTimeline, object.isMediaTimeline());
    setProperty(AXPropertyName::IsMenu, object.isMenu());
    setProperty(AXPropertyName::IsMenuBar, object.isMenuBar());
    setProperty(AXPropertyName::IsMenuButton, object.isMenuButton());
    setProperty(AXPropertyName::IsMenuItem, object.isMenuItem());
    setProperty(AXPropertyName::IsMenuList, object.isMenuList());
    setProperty(AXPropertyName::IsMenuListOption, object.isMenuListOption());
    setProperty(AXPropertyName::IsMenuListPopup, object.isMenuListPopup());
    setProperty(AXPropertyName::IsMenuRelated, object.isMenuRelated());
    setProperty(AXPropertyName::IsMultiSelectable, object.isMultiSelectable());
    setProperty(AXPropertyName::IsOffScreen, object.isOffScreen());
    setProperty(AXPropertyName::IsOnScreen, object.isOnScreen());
    setProperty(AXPropertyName::IsOrderedList, object.isOrderedList());
    setProperty(AXPropertyName::IsOutput, object.isOutput());
    setProperty(AXPropertyName::IsPasswordField, object.isPasswordField());
    setProperty(AXPropertyName::IsPressed, object.isPressed());
    setProperty(AXPropertyName::IsProgressIndicator, object.isProgressIndicator());
    setProperty(AXPropertyName::IsRangeControl, object.isRangeControl());
    setProperty(AXPropertyName::IsRequired, object.isRequired());
    setProperty(AXPropertyName::IsScrollbar, object.isScrollbar());
    setProperty(AXPropertyName::IsSearchField, object.isSearchField());
    setProperty(AXPropertyName::IsSelected, object.isSelected());
    setProperty(AXPropertyName::IsSelectedOptionActive, object.isSelectedOptionActive());
    setProperty(AXPropertyName::IsSlider, object.isSlider());
    setProperty(AXPropertyName::IsStyleFormatGroup, object.isStyleFormatGroup());
    setProperty(AXPropertyName::IsTableCell, object.isTableCell());
    setProperty(AXPropertyName::IsTableColumn, object.isTableColumn());
    setProperty(AXPropertyName::IsTableRow, object.isTableRow());
    setProperty(AXPropertyName::IsTextControl, object.isTextControl());
    setProperty(AXPropertyName::IsTree, object.isTree());
    setProperty(AXPropertyName::IsUnorderedList, object.isUnorderedList());
    setProperty(AXPropertyName::IsUnvisited, object.isUnvisited());
    setProperty(AXPropertyName::IsValueAutofillAvailable, object.isValueAutofillAvailable());
    setProperty(AXPropertyName::IsValueAutofilled, object.isValueAutofilled());
    setProperty(AXPropertyName::IsVisible, object.isVisible());
    setProperty(AXPropertyName::IsVisited, object.isVisited());
    setProperty(AXPropertyName::RelativeFrame, object.relativeFrame());
    setProperty(AXPropertyName::RoleDescription, object.roleDescription().isolatedCopy());
    setProperty(AXPropertyName::RolePlatformString, object.rolePlatformString().isolatedCopy());
    setProperty(AXPropertyName::RoleValue, static_cast<int>(object.roleValue()));
    setProperty(AXPropertyName::SpeechHint, object.speechHintAttributeValue().isolatedCopy());
    setProperty(AXPropertyName::Title, object.titleAttributeValue().isolatedCopy());
    setProperty(AXPropertyName::SupportsDatetimeAttribute, object.supportsDatetimeAttribute());
    setProperty(AXPropertyName::DatetimeAttributeValue, object.datetimeAttributeValue());
    setProperty(AXPropertyName::CanSetFocusAttribute, object.canSetFocusAttribute());
    setProperty(AXPropertyName::CanSetTextRangeAttributes, object.canSetTextRangeAttributes());
    setProperty(AXPropertyName::CanSetValueAttribute, object.canSetValueAttribute());
    setProperty(AXPropertyName::CanSetNumericValue, object.canSetNumericValue());
    setProperty(AXPropertyName::SupportsRequiredAttribute, object.supportsRequiredAttribute());
    setProperty(AXPropertyName::CanSetSelectedAttribute, object.canSetSelectedAttribute());
    setProperty(AXPropertyName::CanSetSelectedChildrenAttribute, object.canSetSelectedChildrenAttribute());
    setProperty(AXPropertyName::CanSetExpandedAttribute, object.canSetExpandedAttribute());
    setProperty(AXPropertyName::IsShowingValidationMessage, object.isShowingValidationMessage());
    setProperty(AXPropertyName::ValidationMessage, object.validationMessage());
    setProperty(AXPropertyName::BlockquoteLevel, object.blockquoteLevel());
    setProperty(AXPropertyName::HeadingLevel, object.headingLevel());
    setProperty(AXPropertyName::TableLevel, object.tableLevel());
    setProperty(AXPropertyName::AccessibilityButtonState, static_cast<int>(object.checkboxOrRadioValue()));
    setProperty(AXPropertyName::ValueDescription, object.valueDescription());
    setProperty(AXPropertyName::ValueForRange, object.valueForRange());
    setProperty(AXPropertyName::MaxValueForRange, object.maxValueForRange());
    setProperty(AXPropertyName::MinValueForRange, object.minValueForRange());
    setProperty(AXPropertyName::SelectedRadioButton, object.selectedRadioButton());
    setProperty(AXPropertyName::SelectedTabItem, object.selectedTabItem());
    setProperty(AXPropertyName::LayoutCount, object.layoutCount());
    setProperty(AXPropertyName::EstimatedLoadingProgress, object.estimatedLoadingProgress());
    setProperty(AXPropertyName::SupportsARIAOwns, object.supportsARIAOwns());
    setProperty(AXPropertyName::HasPopup, object.hasPopup());
    setProperty(AXPropertyName::PopupValue, object.popupValue());
    setProperty(AXPropertyName::PressedIsPresent, object.pressedIsPresent());
    setProperty(AXPropertyName::ARIAIsMultiline, object.ariaIsMultiline());
    setProperty(AXPropertyName::InvalidStatus, object.invalidStatus());
    setProperty(AXPropertyName::SupportsExpanded, object.supportsExpanded());
    setProperty(AXPropertyName::SortDirection, static_cast<int>(object.sortDirection()));
    setProperty(AXPropertyName::CanvasHasFallbackContent, object.canvasHasFallbackContent());
    setProperty(AXPropertyName::SupportsRangeValue, object.supportsRangeValue());
    setProperty(AXPropertyName::IdentifierAttribute, object.identifierAttribute());
    setProperty(AXPropertyName::LinkRelValue, object.linkRelValue());
    setProperty(AXPropertyName::CurrentState, static_cast<int>(object.currentState()));
    setProperty(AXPropertyName::CurrentValue, object.currentValue());
    setProperty(AXPropertyName::SupportsCurrent, object.supportsCurrent());
    setProperty(AXPropertyName::KeyShortcutsValue, object.keyShortcutsValue());
    setProperty(AXPropertyName::SupportsSetSize, object.supportsSetSize());
    setProperty(AXPropertyName::SupportsPath, object.supportsPath());
    setProperty(AXPropertyName::SupportsPosInSet, object.supportsPosInSet());
    setProperty(AXPropertyName::SetSize, object.setSize());
    setProperty(AXPropertyName::PosInSet, object.posInSet());
    setProperty(AXPropertyName::SupportsARIADropping, object.supportsARIADropping());
    setProperty(AXPropertyName::SupportsARIADragging, object.supportsARIADragging());
    setProperty(AXPropertyName::IsARIAGrabbed, object.isARIAGrabbed());
    setProperty(AXPropertyName::ARIADropEffects, object.determineARIADropEffects());
    setObjectProperty(AXPropertyName::TitleUIElement, object.titleUIElement());
    setProperty(AXPropertyName::ExposesTitleUIElement, object.exposesTitleUIElement());
    setObjectProperty(AXPropertyName::VerticalScrollBar, object.scrollBar(AccessibilityOrientation::Vertical));
    setObjectProperty(AXPropertyName::HorizontalScrollBar, object.scrollBar(AccessibilityOrientation::Horizontal));
    setProperty(AXPropertyName::ARIARoleAttribute, static_cast<int>(object.ariaRoleAttribute()));
    setProperty(AXPropertyName::ComputedLabel, object.computedLabel());
    setProperty(AXPropertyName::PlaceholderValue, object.placeholderValue());
    setProperty(AXPropertyName::ExpandedTextValue, object.expandedTextValue());
    setProperty(AXPropertyName::SupportsExpandedTextValue, object.supportsExpandedTextValue());
    setProperty(AXPropertyName::SupportsPressAction, object.supportsPressAction());
    setProperty(AXPropertyName::ClickPoint, object.clickPoint());
    setProperty(AXPropertyName::ComputedRoleString, object.computedRoleString());
    setProperty(AXPropertyName::ValueAutofillButtonType, static_cast<int>(object.valueAutofillButtonType()));
    setProperty(AXPropertyName::URL, object.url());
    setProperty(AXPropertyName::AccessKey, object.accessKey());
    setProperty(AXPropertyName::ActionVerb, object.actionVerb());
    setProperty(AXPropertyName::ReadOnlyValue, object.readOnlyValue());
    setProperty(AXPropertyName::AutoCompleteValue, object.autoCompleteValue());
    setProperty(AXPropertyName::SpeakAs, object.speakAsProperty());
    setProperty(AXPropertyName::StringValue, object.stringValue().isolatedCopy());
#if PLATFORM(COCOA) && !PLATFORM(IOS_FAMILY)
    setProperty(AXPropertyName::CaretBrowsingEnabled, object.caretBrowsingEnabled());
#endif
    setObjectProperty(AXPropertyName::FocusableAncestor, object.focusableAncestor());
    setObjectProperty(AXPropertyName::EditableAncestor, object.editableAncestor());
    setObjectProperty(AXPropertyName::HighestEditableAncestor, object.highestEditableAncestor());
    setProperty(AXPropertyName::Orientation, static_cast<int>(object.orientation()));
    setProperty(AXPropertyName::HierarchicalLevel, object.hierarchicalLevel());
    setProperty(AXPropertyName::Language, object.language());
    setProperty(AXPropertyName::CanHaveSelectedChildren, object.canHaveSelectedChildren());
    setProperty(AXPropertyName::HasARIAValueNow, object.hasARIAValueNow());
    setProperty(AXPropertyName::TagName, object.tagName().isolatedCopy());
    setProperty(AXPropertyName::SupportsLiveRegion, object.supportsLiveRegion());
    setProperty(AXPropertyName::IsInsideLiveRegion, object.isInsideLiveRegion());
    setProperty(AXPropertyName::LiveRegionStatus, object.liveRegionStatus());
    setProperty(AXPropertyName::LiveRegionRelevant, object.liveRegionRelevant());
    setProperty(AXPropertyName::LiveRegionAtomic, object.liveRegionAtomic());
    if (object.isTextControl())
        setProperty(AXPropertyName::TextLength, object.textLength());

    AccessibilityChildrenVector selectedChildren;
    object.selectedChildren(selectedChildren);
    setObjectVectorProperty(AXPropertyName::SelectedChildren, selectedChildren);

    AccessibilityChildrenVector visibleChildren;
    object.visibleChildren(visibleChildren);
    setObjectVectorProperty(AXPropertyName::VisibleChildren, visibleChildren);

    AccessibilityChildrenVector tabChildren;
    object.tabChildren(tabChildren);
    setObjectVectorProperty(AXPropertyName::TabChildren, tabChildren);

    AccessibilityChildrenVector ariaTreeRows;
    object.ariaTreeRows(ariaTreeRows);
    setObjectVectorProperty(AXPropertyName::ARIATreeRows, ariaTreeRows);

    AccessibilityChildrenVector ariaTreeItemDisclosedRows;
    object.ariaTreeItemDisclosedRows(ariaTreeItemDisclosedRows);
    setObjectVectorProperty(AXPropertyName::ARIATreeItemDisclosedRows, ariaTreeItemDisclosedRows);

    AccessibilityChildrenVector ariaTreeItemContent;
    object.ariaTreeItemContent(ariaTreeItemContent);
    setObjectVectorProperty(AXPropertyName::ARIATreeItemContent, ariaTreeItemContent);

    AccessibilityChildrenVector linkedUIElements;
    object.linkedUIElements(linkedUIElements);
    setObjectVectorProperty(AXPropertyName::LinkedUIElements, linkedUIElements);

    AccessibilityChildrenVector ariaControlsElements;
    object.ariaControlsElements(ariaControlsElements);
    setObjectVectorProperty(AXPropertyName::ARIAControlsElements, ariaControlsElements);

    AccessibilityChildrenVector ariaDetailsElements;
    object.ariaDetailsElements(ariaDetailsElements);
    setObjectVectorProperty(AXPropertyName::ARIADetailsElements, ariaDetailsElements);

    AccessibilityChildrenVector ariaErrorMessageElements;
    object.ariaErrorMessageElements(ariaErrorMessageElements);
    setObjectVectorProperty(AXPropertyName::ARIAErrorMessageElements, ariaErrorMessageElements);

    AccessibilityChildrenVector ariaFlowToElements;
    object.ariaFlowToElements(ariaFlowToElements);
    setObjectVectorProperty(AXPropertyName::ARIAFlowToElements, ariaFlowToElements);

    AccessibilityChildrenVector ariaOwnsElements;
    object.ariaOwnsElements(ariaOwnsElements);
    setObjectVectorProperty(AXPropertyName::ARIAOwnsElements, ariaOwnsElements);

    Vector<AccessibilityText> texts;
    object.accessibilityText(texts);
    Vector<AccessibilityIsolatedTreeText> isolatedTexts;
    isolatedTexts.reserveCapacity(texts.size());
    for (auto text : texts) {
        AccessibilityIsolatedTreeText isolatedText;
        isolatedText.text = text.text;
        isolatedText.textSource = text.textSource;
        for (auto object : text.textElements)
            isolatedText.textElements.append(object->objectID());
        isolatedTexts.uncheckedAppend(isolatedText);
    }
    setProperty(AXPropertyName::AccessibilityText, isolatedTexts);

    // Spin button support.
    setObjectProperty(AXPropertyName::DecrementButton, object.decrementButton());
    setObjectProperty(AXPropertyName::IncrementButton, object.incrementButton());
    setProperty(AXPropertyName::IsIncrementor, object.isIncrementor());

    Vector<String> classList;
    object.classList(classList);
    String combinedClassList;
    for (auto it = classList.begin(), end = classList.end(); it != end; ++it) {
        combinedClassList.append(*it);
        combinedClassList.append(" ");
    }
    setProperty(AXPropertyName::ClassList, combinedClassList);
    
    int r, g, b;
    object.colorValue(r, g, b);
    setProperty(AXPropertyName::ColorValue, Color(r, g, b));
    
    if (bool isMathElement = object.isMathElement()) {
        setProperty(AXPropertyName::IsMathElement, isMathElement);
        setProperty(AXPropertyName::IsAnonymousMathOperator, object.isAnonymousMathOperator());
        setProperty(AXPropertyName::IsMathFraction, object.isMathFraction());
        setProperty(AXPropertyName::IsMathFenced, object.isMathFenced());
        setProperty(AXPropertyName::IsMathSubscriptSuperscript, object.isMathSubscriptSuperscript());
        setProperty(AXPropertyName::IsMathRow, object.isMathRow());
        setProperty(AXPropertyName::IsMathUnderOver, object.isMathUnderOver());
        setProperty(AXPropertyName::IsMathRoot, object.isMathRoot());
        setProperty(AXPropertyName::IsMathSquareRoot, object.isMathSquareRoot());
        setProperty(AXPropertyName::IsMathText, object.isMathText());
        setProperty(AXPropertyName::IsMathNumber, object.isMathNumber());
        setProperty(AXPropertyName::IsMathOperator, object.isMathOperator());
        setProperty(AXPropertyName::IsMathFenceOperator, object.isMathFenceOperator());
        setProperty(AXPropertyName::IsMathSeparatorOperator, object.isMathSeparatorOperator());
        setProperty(AXPropertyName::IsMathIdentifier, object.isMathIdentifier());
        setProperty(AXPropertyName::IsMathTable, object.isMathTable());
        setProperty(AXPropertyName::IsMathTableRow, object.isMathTableRow());
        setProperty(AXPropertyName::IsMathTableCell, object.isMathTableCell());
        setProperty(AXPropertyName::IsMathMultiscript, object.isMathMultiscript());
        setProperty(AXPropertyName::IsMathToken, object.isMathToken());
        setProperty(AXPropertyName::MathFencedOpenString, object.mathFencedOpenString());
        setProperty(AXPropertyName::MathFencedCloseString, object.mathFencedCloseString());
        setProperty(AXPropertyName::MathLineThickness, object.mathLineThickness());
        setObjectProperty(AXPropertyName::MathRadicandObject, object.mathRadicandObject());
        setObjectProperty(AXPropertyName::MathRootIndexObject, object.mathRootIndexObject());
        setObjectProperty(AXPropertyName::MathUnderObject, object.mathUnderObject());
        setObjectProperty(AXPropertyName::MathOverObject, object.mathOverObject());
        setObjectProperty(AXPropertyName::MathNumeratorObject, object.mathNumeratorObject());
        setObjectProperty(AXPropertyName::MathDenominatorObject, object.mathDenominatorObject());
        setObjectProperty(AXPropertyName::MathBaseObject, object.mathBaseObject());
        setObjectProperty(AXPropertyName::MathSubscriptObject, object.mathSubscriptObject());
        setObjectProperty(AXPropertyName::MathSuperscriptObject, object.mathSuperscriptObject());
        setMathscripts(AXPropertyName::MathPrescripts, object);
        setMathscripts(AXPropertyName::MathPostscripts, object);
    }
    
    if (isRoot) {
        setProperty(AXPropertyName::PreventKeyboardDOMEventDispatch, object.preventKeyboardDOMEventDispatch());
        setProperty(AXPropertyName::SessionID, object.sessionID());
        setProperty(AXPropertyName::DocumentURI, object.documentURI());
        setProperty(AXPropertyName::DocumentEncoding, object.documentEncoding());
    }
}

void AXIsolatedObject::setMathscripts(AXPropertyName propertyName, AXCoreObject& object)
{
    AccessibilityMathMultiscriptPairs pairs;
    if (propertyName == AXPropertyName::MathPrescripts)
        object.mathPrescripts(pairs);
    else if (propertyName == AXPropertyName::MathPostscripts)
        object.mathPostscripts(pairs);
    
    size_t mathSize = pairs.size();
    if (!mathSize)
        return;
    
    Vector<AccessibilityIsolatedTreeMathMultiscriptPair> idPairs;
    idPairs.reserveCapacity(mathSize);
    for (auto mathPair : pairs) {
        AccessibilityIsolatedTreeMathMultiscriptPair idPair;
        if (mathPair.first)
            idPair.first = mathPair.first->objectID();
        if (mathPair.second)
            idPair.second = mathPair.second->objectID();
        idPairs.uncheckedAppend(idPair);
    }
    setProperty(propertyName, idPairs);
}

void AXIsolatedObject::setObjectProperty(AXPropertyName propertyName, AXCoreObject* object)
{
    if (object)
        setProperty(propertyName, object->objectID());
    else
        setProperty(propertyName, nullptr, true);
}

void AXIsolatedObject::setObjectVectorProperty(AXPropertyName propertyName, AccessibilityChildrenVector& children)
{
    size_t childrenSize = children.size();
    if (!childrenSize)
        return;
    
    Vector<AXID> childrenVector;
    childrenVector.reserveCapacity(childrenSize);
    for (auto childObject : children)
        childrenVector.uncheckedAppend(childObject->objectID());
    setProperty(propertyName, childrenVector);
}

void AXIsolatedObject::setProperty(AXPropertyName propertyName, AttributeValueVariant&& value, bool shouldRemove)
{
    ASSERT(!m_initialized);
    ASSERT(isMainThread());

    if (shouldRemove)
        m_attributeMap.remove(propertyName);
    else
        m_attributeMap.set(propertyName, value);
}

void AXIsolatedObject::appendChild(AXID axID)
{
    ASSERT(isMainThread());
    m_childrenIDs.append(axID);
}

void AXIsolatedObject::setParent(AXID parent)
{
    ASSERT(isMainThread());
    m_parent = parent;
}

void AXIsolatedObject::detachRemoteParts(AccessibilityDetachmentType detachmentType)
{
    ASSERT_UNUSED(detachmentType, isMainThread() ? detachmentType == AccessibilityDetachmentType::CacheDestroyed : detachmentType != AccessibilityDetachmentType::CacheDestroyed);
    for (const auto& childID : m_childrenIDs) {
        if (auto child = tree()->nodeForID(childID))
            child->detachFromParent();
    }
    m_childrenIDs.clear();
}

bool AXIsolatedObject::isDetached() const
{
    ASSERT_NOT_REACHED();
    return false;
}

void AXIsolatedObject::detachFromParent()
{
    m_parent = InvalidAXID;
}

void AXIsolatedObject::setTreeIdentifier(AXIsolatedTreeID treeIdentifier)
{
    m_treeIdentifier = treeIdentifier;
    if (auto tree = AXIsolatedTree::treeForID(m_treeIdentifier))
        m_cachedTree = tree;
}

const AXCoreObject::AccessibilityChildrenVector& AXIsolatedObject::children(bool)
{
    if (!isMainThread()) {
        m_children.clear();
        m_children.reserveInitialCapacity(m_childrenIDs.size());
        for (const auto& childID : m_childrenIDs) {
            if (auto child = tree()->nodeForID(childID))
                m_children.uncheckedAppend(child);
        }
    }
    return m_children;
}

bool AXIsolatedObject::isDetachedFromParent()
{
    if (parent() != InvalidAXID)
        return false;

    // Check whether this is the root node, in which case we should return false.
    if (auto root = tree()->rootNode())
        return root->objectID() != m_id;
    return false;
}

void AXIsolatedObject::accessibilityText(Vector<AccessibilityText>& texts) const
{
    auto isolatedTexts = vectorAttributeValue<AccessibilityIsolatedTreeText>(AXPropertyName::AccessibilityText);
    for (const auto& isolatedText : isolatedTexts) {
        AccessibilityText text(isolatedText.text, isolatedText.textSource);
        for (const auto& axID : isolatedText.textElements) {
            if (auto object = tree()->nodeForID(axID))
                text.textElements.append(object);
        }
        texts.append(text);
    }
}

void AXIsolatedObject::classList(Vector<String>& list) const
{
    String classList = stringAttributeValue(AXPropertyName::ClassList);
    list.appendVector(classList.split(" "));
}

uint64_t AXIsolatedObject::sessionID() const
{
    if (auto root = tree()->rootNode())
        return root->uint64AttributeValue(AXPropertyName::SessionID);
    return 0;
}

String AXIsolatedObject::documentURI() const
{
    if (auto root = tree()->rootNode())
        return root->stringAttributeValue(AXPropertyName::DocumentURI);
    return String();
}

bool AXIsolatedObject::preventKeyboardDOMEventDispatch() const
{
    if (auto root = tree()->rootNode())
        return root->boolAttributeValue(AXPropertyName::PreventKeyboardDOMEventDispatch);
    return false;
}

String AXIsolatedObject::documentEncoding() const
{
    if (auto root = tree()->rootNode())
        return root->stringAttributeValue(AXPropertyName::DocumentEncoding);
    return String();
}

void AXIsolatedObject::insertMathPairs(Vector<AccessibilityIsolatedTreeMathMultiscriptPair>& isolatedPairs, AccessibilityMathMultiscriptPairs& pairs)
{
    for (const auto& pair : isolatedPairs) {
        AccessibilityMathMultiscriptPair prescriptPair;
        if (auto coreObject = tree()->nodeForID(pair.first).get())
            prescriptPair.first = coreObject;
        if (auto coreObject = tree()->nodeForID(pair.second).get())
            prescriptPair.second = coreObject;
        pairs.append(prescriptPair);
    }
}

void AXIsolatedObject::mathPrescripts(AccessibilityMathMultiscriptPairs& pairs)
{
    auto isolatedPairs = vectorAttributeValue<AccessibilityIsolatedTreeMathMultiscriptPair>(AXPropertyName::MathPrescripts);
    insertMathPairs(isolatedPairs, pairs);
}

void AXIsolatedObject::mathPostscripts(AccessibilityMathMultiscriptPairs& pairs)
{
    auto isolatedPairs = vectorAttributeValue<AccessibilityIsolatedTreeMathMultiscriptPair>(AXPropertyName::MathPostscripts);
    insertMathPairs(isolatedPairs, pairs);
}

AXCoreObject* AXIsolatedObject::focusedUIElement() const
{
    if (auto focusedElement = tree()->focusedUIElement())
        return focusedElement.get();
    return nullptr;
}
    
AXCoreObject* AXIsolatedObject::parentObjectUnignored() const
{
    return tree()->nodeForID(parent()).get();
}

AXCoreObject* AXIsolatedObject::scrollBar(AccessibilityOrientation orientation)
{
    return objectAttributeValue(orientation == AccessibilityOrientation::Vertical ? AXPropertyName::VerticalScrollBar : AXPropertyName::HorizontalScrollBar);
}

template<typename U>
void AXIsolatedObject::performFunctionOnMainThread(U&& lambda)
{
    Accessibility::performFunctionOnMainThread([&lambda, this] () {
        if (auto* object = associatedAXObject())
            lambda(object);
    });
}

void AXIsolatedObject::setARIAGrabbed(bool value)
{
    performFunctionOnMainThread([&value](AXCoreObject* object) {
        object->setARIAGrabbed(value);
    });
}

void AXIsolatedObject::setIsExpanded(bool value)
{
    performFunctionOnMainThread([&value](AXCoreObject* object) {
        object->setIsExpanded(value);
    });
}
void AXIsolatedObject::setValue(float value)
{
    performFunctionOnMainThread([&value](AXCoreObject* object) {
        object->setValue(value);
    });
}

void AXIsolatedObject::setSelected(bool value)
{
    performFunctionOnMainThread([&value](AXCoreObject* object) {
        object->setSelected(value);
    });
}
void AXIsolatedObject::setSelectedRows(AccessibilityChildrenVector& value)
{
    performFunctionOnMainThread([&value](AXCoreObject* object) {
        object->setSelectedRows(value);
    });
}

void AXIsolatedObject::setFocused(bool value)
{
    performFunctionOnMainThread([&value](AXCoreObject* object) {
        object->setFocused(value);
    });
}

void AXIsolatedObject::setSelectedText(const String& value)
{
    performFunctionOnMainThread([&value](AXCoreObject* object) {
        object->setSelectedText(value);
    });
}

void AXIsolatedObject::setSelectedTextRange(const PlainTextRange& value)
{
    performFunctionOnMainThread([&value](AXCoreObject* object) {
        object->setSelectedTextRange(value);
    });
}

void AXIsolatedObject::setValue(const String& value)
{
    performFunctionOnMainThread([&value](AXCoreObject* object) {
        object->setValue(value);
    });
}

#if PLATFORM(COCOA) && !PLATFORM(IOS_FAMILY)
void AXIsolatedObject::setCaretBrowsingEnabled(bool value)
{
    performFunctionOnMainThread([&value](AXCoreObject* object) {
        object->setCaretBrowsingEnabled(value);
    });
}
#endif

void AXIsolatedObject::setPreventKeyboardDOMEventDispatch(bool value)
{
    performFunctionOnMainThread([&value](AXCoreObject* object) {
        object->setPreventKeyboardDOMEventDispatch(value);
    });
}

void AXIsolatedObject::colorValue(int& r, int& g, int& b) const
{
    auto color = colorAttributeValue(AXPropertyName::ColorValue);
    r = color.red();
    g = color.green();
    b = color.blue();
}

AXCoreObject* AXIsolatedObject::accessibilityHitTest(const IntPoint& point) const
{
    if (!relativeFrame().contains(point))
        return nullptr;
    for (const auto& childID : m_childrenIDs) {
        auto child = tree()->nodeForID(childID);
        ASSERT(child);
        if (child && child->relativeFrame().contains(point))
            return child->accessibilityHitTest(point);
    }
    return const_cast<AXIsolatedObject*>(this);
}

IntPoint AXIsolatedObject::intPointAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (IntPoint& typedValue) { return typedValue; },
        [] (auto&) { return IntPoint(); }
    );
}

AXCoreObject* AXIsolatedObject::objectAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    AXID nodeID = WTF::switchOn(value,
        [] (AXID& typedValue) { return typedValue; },
        [] (auto&) { return InvalidAXID; }
    );

    return tree()->nodeForID(nodeID).get();
}

template<typename T>
T AXIsolatedObject::rectAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (T& typedValue) { return typedValue; },
        [] (auto&) { return T { }; }
    );
}

template<typename T>
Vector<T> AXIsolatedObject::vectorAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (Vector<T>& typedValue) { return typedValue; },
        [] (auto&) { return Vector<T>(); }
    );
}

template<typename T>
OptionSet<T> AXIsolatedObject::optionSetAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (T& typedValue) { return typedValue; },
        [] (auto&) { return OptionSet<T>(); }
    );
}

uint64_t AXIsolatedObject::uint64AttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (uint64_t& typedValue) { return typedValue; },
        [] (auto&) { return 0; }
    );
}

URL AXIsolatedObject::urlAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (URL& typedValue) { return typedValue; },
        [] (auto&) { return URL(); }
    );
}

Color AXIsolatedObject::colorAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (Color& typedValue) { return typedValue; },
        [] (auto&) { return Color(); }
    );
}

float AXIsolatedObject::floatAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (float& typedValue) { return typedValue; },
        [] (auto&) { return 0; }
    );
}

double AXIsolatedObject::doubleAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (double& typedValue) { return typedValue; },
        [] (auto&) { return 0; }
    );
}

unsigned AXIsolatedObject::unsignedAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (unsigned& typedValue) { return typedValue; },
        [] (auto&) { return 0; }
    );
}

bool AXIsolatedObject::boolAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (bool& typedValue) { return typedValue; },
        [] (auto&) { return false; }
    );
}

const String AXIsolatedObject::stringAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (String& typedValue) { return typedValue; },
        [] (auto&) { return emptyString(); }
    );
}

int AXIsolatedObject::intAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (int& typedValue) { return typedValue; },
        [] (auto&) { return 0; }
    );
}

void AXIsolatedObject::fillChildrenVectorForProperty(AXPropertyName propertyName, AccessibilityChildrenVector& children) const
{
    Vector<AXID> childIDs = vectorAttributeValue<AXID>(propertyName);
    children.reserveCapacity(childIDs.size());
    for (const auto& childID : childIDs) {
        if (auto object = tree()->nodeForID(childID))
            children.uncheckedAppend(object);
    }
}

void AXIsolatedObject::updateBackingStore()
{
    // This method can be called on either the main or the AX threads.
    // It can be called in the main thread from [WebAccessibilityObjectWrapper accessibilityFocusedUIElement].
    // Update the IsolatedTree only if it is called on the AX thread.
    if (!isMainThread()) {
        if (auto tree = this->tree())
            tree->applyPendingChanges();
    }
}

Vector<RefPtr<Range>> AXIsolatedObject::findTextRanges(AccessibilitySearchTextCriteria const& criteria) const
{
    return Accessibility::retrieveValueFromMainThread<Vector<RefPtr<Range>>>([&criteria, this] () -> Vector<RefPtr<Range>> {
        if (auto* object = associatedAXObject())
            return object->findTextRanges(criteria);
        return Vector<RefPtr<Range>>();
    });
}

Vector<String> AXIsolatedObject::performTextOperation(AccessibilityTextOperation const& textOperation)
{
    return Accessibility::retrieveValueFromMainThread<Vector<String>>([&textOperation, this] () -> Vector<String> {
        if (auto* object = associatedAXObject())
            return object->performTextOperation(textOperation);
        return Vector<String>();
    });
}

void AXIsolatedObject::findMatchingObjects(AccessibilitySearchCriteria* criteria, AccessibilityChildrenVector& results)
{
    ASSERT(criteria);
    if (!criteria)
        return;

    criteria->anchorObject = this;
    Accessibility::findMatchingObjects(*criteria, results);
}

bool AXIsolatedObject::replaceTextInRange(const String&, const PlainTextRange&)
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::insertText(const String&)
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::press()
{
    if (auto* object = associatedAXObject())
        return object->press();
    return false;
}

bool AXIsolatedObject::performDefaultAction()
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isAccessibilityObject() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isAccessibilityNodeObject() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isAccessibilityRenderObject() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isAccessibilityScrollbar() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isAccessibilityScrollView() const
{
    // FIXME: this should be ASSERT_NOT_REACHED, but it is called by
    // AXObjectCache::rootWebArea, that in turn is called by
    // AXObjectCache::postTextStateChangePlatformNotification.
    return false;
}

bool AXIsolatedObject::isAccessibilitySVGRoot() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isAccessibilitySVGElement() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isAttachmentElement() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isNativeImage() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isImageButton() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isContainedByPasswordField() const
{
    ASSERT_NOT_REACHED();
    return false;
}

AXCoreObject* AXIsolatedObject::passwordFieldOrContainingPasswordField()
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

bool AXIsolatedObject::isNativeTextControl() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isNativeListBox() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isListBoxOption() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isSliderThumb() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isInputSlider() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isLabel() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isDataTable() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isImageMapLink() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isNativeSpinButton() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isSpinButtonPart() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isMockObject() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isMediaObject() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isARIATextControl() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isNonNativeTextControl() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isBlockquote() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isFigureElement() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isKeyboardFocusable() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isHovered() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isIndeterminate() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isOnScreen() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isOffScreen() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isPressed() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isUnvisited() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isLinked() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isVisible() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isCollapsed() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isSelectedOptionActive() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::hasBoldFont() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::hasItalicFont() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::hasMisspelling() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::hasPlainText() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::hasSameFont(RenderObject*) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::hasSameFontColor(RenderObject*) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::hasSameStyle(RenderObject*) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::hasUnderline() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::hasHighlighting() const
{
    ASSERT_NOT_REACHED();
    return false;
}

Element* AXIsolatedObject::element() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

Node* AXIsolatedObject::node() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

RenderObject* AXIsolatedObject::renderer() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

AccessibilityObjectInclusion AXIsolatedObject::defaultObjectInclusion() const
{
    ASSERT_NOT_REACHED();
    return AccessibilityObjectInclusion::DefaultBehavior;
}

bool AXIsolatedObject::accessibilityIsIgnoredByDefault() const
{
    ASSERT_NOT_REACHED();
    return false;
}

float AXIsolatedObject::stepValueForRange() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

AXCoreObject* AXIsolatedObject::selectedListItem()
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

void AXIsolatedObject::ariaActiveDescendantReferencingElements(AccessibilityChildrenVector&) const
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::ariaControlsReferencingElements(AccessibilityChildrenVector&) const
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::ariaDescribedByElements(AccessibilityChildrenVector&) const
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::ariaDescribedByReferencingElements(AccessibilityChildrenVector&) const
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::ariaDetailsReferencingElements(AccessibilityChildrenVector&) const
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::ariaErrorMessageReferencingElements(AccessibilityChildrenVector&) const
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::ariaFlowToReferencingElements(AccessibilityChildrenVector&) const
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::ariaLabelledByElements(AccessibilityChildrenVector&) const
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::ariaLabelledByReferencingElements(AccessibilityChildrenVector&) const
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::ariaOwnsReferencingElements(AccessibilityChildrenVector&) const
{
    ASSERT_NOT_REACHED();
}

bool AXIsolatedObject::hasDatalist() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::supportsHasPopup() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::supportsPressed() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::supportsChecked() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::ignoredFromModalPresence() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isModalDescendant(Node*) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isModalNode() const
{
    ASSERT_NOT_REACHED();
    return false;
}

AXCoreObject* AXIsolatedObject::elementAccessibilityHitTest(const IntPoint&) const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

AXCoreObject* AXIsolatedObject::firstChild() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

AXCoreObject* AXIsolatedObject::lastChild() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

AXCoreObject* AXIsolatedObject::previousSibling() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

AXCoreObject* AXIsolatedObject::nextSibling() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

AXCoreObject* AXIsolatedObject::nextSiblingUnignored(int) const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

AXCoreObject* AXIsolatedObject::previousSiblingUnignored(int) const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

AXCoreObject* AXIsolatedObject::parentObjectIfExists() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

bool AXIsolatedObject::isDescendantOfBarrenParent() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isDescendantOfRole(AccessibilityRole) const
{
    ASSERT_NOT_REACHED();
    return false;
}

AXCoreObject* AXIsolatedObject::observableObject() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

AXCoreObject* AXIsolatedObject::correspondingLabelForControlElement() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

AXCoreObject* AXIsolatedObject::correspondingControlForLabelElement() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

bool AXIsolatedObject::isPresentationalChildOfAriaRole() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::ariaRoleHasPresentationalChildren() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::inheritsPresentationalRole() const
{
    ASSERT_NOT_REACHED();
    return false;
}

void AXIsolatedObject::setAccessibleName(const AtomString&)
{
    ASSERT_NOT_REACHED();
}

bool AXIsolatedObject::hasAttributesRequiredForInclusion() const
{
    ASSERT_NOT_REACHED();
    return false;
}

String AXIsolatedObject::accessibilityDescription() const
{
    ASSERT_NOT_REACHED();
    return String();
}

String AXIsolatedObject::title() const
{
    ASSERT_NOT_REACHED();
    return String();
}

String AXIsolatedObject::helpText() const
{
    ASSERT_NOT_REACHED();
    return String();
}

bool AXIsolatedObject::isARIAStaticText() const
{
    ASSERT_NOT_REACHED();
    return false;
}

String AXIsolatedObject::text() const
{
    ASSERT_NOT_REACHED();
    return String();
}

String AXIsolatedObject::ariaLabeledByAttribute() const
{
    ASSERT_NOT_REACHED();
    return String();
}

String AXIsolatedObject::ariaDescribedByAttribute() const
{
    ASSERT_NOT_REACHED();
    return String();
}

bool AXIsolatedObject::accessibleNameDerivesFromContent() const
{
    ASSERT_NOT_REACHED();
    return false;
}

void AXIsolatedObject::elementsFromAttribute(Vector<Element*>&, const QualifiedName&) const
{
    ASSERT_NOT_REACHED();
}

AXObjectCache* AXIsolatedObject::axObjectCache() const
{
    ASSERT(isMainThread());
    return tree()->axObjectCache();
}

Element* AXIsolatedObject::anchorElement() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

Element* AXIsolatedObject::actionElement() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

Path AXIsolatedObject::elementPath() const
{
    ASSERT_NOT_REACHED();
    return Path();
}

TextIteratorBehavior AXIsolatedObject::textIteratorBehaviorForTextRange() const
{
    ASSERT_NOT_REACHED();
    return false;
}

Widget* AXIsolatedObject::widget() const
{
    if (auto* object = associatedAXObject())
        return object->widget();
    return nullptr;
}

Widget* AXIsolatedObject::widgetForAttachmentView() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

Page* AXIsolatedObject::page() const
{
    if (auto* object = associatedAXObject())
        return object->page();
    ASSERT_NOT_REACHED();
    return nullptr;
}

Document* AXIsolatedObject::document() const
{
    if (auto* object = associatedAXObject())
        return object->document();
    ASSERT_NOT_REACHED();
    return nullptr;
}

FrameView* AXIsolatedObject::documentFrameView() const
{
    if (auto* object = associatedAXObject())
        return object->documentFrameView();
    return nullptr;
}

Frame* AXIsolatedObject::frame() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

Frame* AXIsolatedObject::mainFrame() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

Document* AXIsolatedObject::topDocument() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

ScrollView* AXIsolatedObject::scrollViewAncestor() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

void AXIsolatedObject::childrenChanged()
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::textChanged()
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::updateAccessibilityRole()
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::addChildren()
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::addChild(AXCoreObject*)
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::insertChild(AXCoreObject*, unsigned)
{
    ASSERT_NOT_REACHED();
}

bool AXIsolatedObject::shouldIgnoreAttributeRole() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::canHaveChildren() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::hasChildren() const
{
    ASSERT_NOT_REACHED();
    return false;
}

void AXIsolatedObject::setNeedsToUpdateChildren()
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::setNeedsToUpdateSubtree()
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::clearChildren()
{
    ASSERT_NOT_REACHED();
}

bool AXIsolatedObject::needsToUpdateChildren() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::shouldFocusActiveDescendant() const
{
    ASSERT_NOT_REACHED();
    return false;
}

AXCoreObject* AXIsolatedObject::activeDescendant() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

void AXIsolatedObject::handleActiveDescendantChanged()
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::handleAriaExpandedChanged()
{
    ASSERT_NOT_REACHED();
}

bool AXIsolatedObject::isDescendantOfObject(const AXCoreObject*) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isAncestorOfObject(const AXCoreObject*) const
{
    ASSERT_NOT_REACHED();
    return false;
}

AXCoreObject* AXIsolatedObject::firstAnonymousBlockChild() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

bool AXIsolatedObject::hasAttribute(const QualifiedName&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

const AtomString& AXIsolatedObject::getAttribute(const QualifiedName&) const
{
    ASSERT_NOT_REACHED();
    return nullAtom();
}

bool AXIsolatedObject::hasTagName(const QualifiedName&) const
{
    ASSERT_NOT_REACHED();
    return false;
}

String AXIsolatedObject::stringValueForMSAA() const
{
    ASSERT_NOT_REACHED();
    return String();
}

String AXIsolatedObject::stringRoleForMSAA() const
{
    ASSERT_NOT_REACHED();
    return String();
}

String AXIsolatedObject::nameForMSAA() const
{
    ASSERT_NOT_REACHED();
    return String();
}

String AXIsolatedObject::descriptionForMSAA() const
{
    ASSERT_NOT_REACHED();
    return String();
}

AccessibilityRole AXIsolatedObject::roleValueForMSAA() const
{
    ASSERT_NOT_REACHED();
    return AccessibilityRole::Unknown;
}

String AXIsolatedObject::passwordFieldValue() const
{
    ASSERT_NOT_REACHED();
    return String();
}

AXCoreObject* AXIsolatedObject::liveRegionAncestor(bool) const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

bool AXIsolatedObject::hasContentEditableAttributeSet() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::supportsReadOnly() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::supportsAutoComplete() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::supportsARIAAttributes() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::scrollByPage(ScrollByPageDirection) const
{
    ASSERT_NOT_REACHED();
    return false;
}

IntPoint AXIsolatedObject::scrollPosition() const
{
    ASSERT_NOT_REACHED();
    return IntPoint();
}

IntSize AXIsolatedObject::scrollContentsSize() const
{
    ASSERT_NOT_REACHED();
    return IntSize();
}

IntRect AXIsolatedObject::scrollVisibleContentRect() const
{
    ASSERT_NOT_REACHED();
    return IntRect();
}

void AXIsolatedObject::scrollToMakeVisible(const ScrollRectToVisibleOptions&) const
{
    ASSERT_NOT_REACHED();
}

bool AXIsolatedObject::lastKnownIsIgnoredValue()
{
    ASSERT_NOT_REACHED();
    return false;
}

void AXIsolatedObject::setLastKnownIsIgnoredValue(bool)
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::notifyIfIgnoredValueChanged()
{
    ASSERT_NOT_REACHED();
}

bool AXIsolatedObject::isMathScriptObject(AccessibilityMathScriptObjectType) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isAXHidden() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isDOMHidden() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isHidden() const
{
    ASSERT_NOT_REACHED();
    return false;
}

void AXIsolatedObject::overrideAttachmentParent(AXCoreObject*)
{
    ASSERT_NOT_REACHED();
}

bool AXIsolatedObject::accessibilityIgnoreAttachment() const
{
    ASSERT_NOT_REACHED();
    return false;
}

AccessibilityObjectInclusion AXIsolatedObject::accessibilityPlatformIncludesObject() const
{
    ASSERT_NOT_REACHED();
    return AccessibilityObjectInclusion::DefaultBehavior;
}

bool AXIsolatedObject::hasApplePDFAnnotationAttribute() const
{
    ASSERT_NOT_REACHED();
    return false;
}

const AccessibilityScrollView* AXIsolatedObject::ancestorAccessibilityScrollView(bool) const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

void AXIsolatedObject::setIsIgnoredFromParentData(AccessibilityIsIgnoredFromParentData&)
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::clearIsIgnoredFromParentData()
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::setIsIgnoredFromParentDataForChild(AXCoreObject*)
{
    ASSERT_NOT_REACHED();
}

} // namespace WebCore

#endif // ENABLE((ACCESSIBILITY_ISOLATED_TREE)
