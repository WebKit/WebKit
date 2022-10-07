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
#include "AXLogger.h"
#include <pal/SessionID.h>

#if PLATFORM(COCOA)
#include <pal/spi/cocoa/AccessibilitySupportSoftLink.h>
#endif

namespace WebCore {

AXIsolatedObject::AXIsolatedObject(const Ref<AXCoreObject>& axObject, AXIsolatedTree* tree)
    : AXCoreObject(axObject->objectID())
    , m_cachedTree(tree)
{
    ASSERT(isMainThread());
    ASSERT(is<AccessibilityObject>(axObject));
    ASSERT(objectID().isValid());

    auto* axParent = axObject->parentObjectUnignored();
    m_parentID = axParent ? axParent->objectID() : AXID();

    auto isRoot = !axParent && axObject->isScrollView() ? IsRoot::Yes : IsRoot::No;
    initializeProperties(axObject, isRoot);
}

Ref<AXIsolatedObject> AXIsolatedObject::create(const Ref<AXCoreObject>& object, AXIsolatedTree* tree)
{
    return adoptRef(*new AXIsolatedObject(object, tree));
}

AXIsolatedObject::~AXIsolatedObject()
{
    ASSERT(!wrapper());
}

void AXIsolatedObject::init()
{
    ASSERT_NOT_REACHED();
}

void AXIsolatedObject::initializeProperties(const Ref<AXCoreObject>& coreObject, IsRoot isRoot)
{
    ASSERT(is<AccessibilityObject>(coreObject));
    auto& object = downcast<AccessibilityObject>(coreObject.get());

    setProperty(AXPropertyName::ARIALandmarkRoleDescription, object.ariaLandmarkRoleDescription().isolatedCopy());
    setProperty(AXPropertyName::AccessibilityDescription, object.accessibilityDescription().isolatedCopy());

    if (object.ancestorFlagsAreInitialized())
        setProperty(AXPropertyName::AncestorFlags, object.ancestorFlags());
    else
        setProperty(AXPropertyName::AncestorFlags, object.computeAncestorFlagsWithTraversal());

    setProperty(AXPropertyName::HasARIAValueNow, object.hasARIAValueNow());
    setProperty(AXPropertyName::IsAttachment, object.isAttachment());
    setProperty(AXPropertyName::IsBusy, object.isBusy());
    setProperty(AXPropertyName::IsButton, object.isButton());
    setProperty(AXPropertyName::IsControl, object.isControl());
    setProperty(AXPropertyName::IsEnabled, object.isEnabled());
    setProperty(AXPropertyName::IsExpanded, object.isExpanded());
    setProperty(AXPropertyName::IsFocused, object.isFocused());
    setProperty(AXPropertyName::IsGroup, object.isGroup());
    setProperty(AXPropertyName::IsHeading, object.isHeading());
    setProperty(AXPropertyName::IsInlineText, object.isInlineText());
    setProperty(AXPropertyName::IsInputImage, object.isInputImage());
    setProperty(AXPropertyName::IsLandmark, object.isLandmark());
    setProperty(AXPropertyName::IsLink, object.isLink());
    setProperty(AXPropertyName::IsList, object.isList());
    setProperty(AXPropertyName::IsMenu, object.isMenu());
    setProperty(AXPropertyName::IsMenuBar, object.isMenuBar());
    setProperty(AXPropertyName::IsMenuButton, object.isMenuButton());
    setProperty(AXPropertyName::IsMenuItem, object.isMenuItem());
    setProperty(AXPropertyName::IsMenuList, object.isMenuList());
    setProperty(AXPropertyName::IsMenuListOption, object.isMenuListOption());
    setProperty(AXPropertyName::IsMenuListPopup, object.isMenuListPopup());
    setProperty(AXPropertyName::IsMenuRelated, object.isMenuRelated());
    setProperty(AXPropertyName::IsMultiSelectable, object.isMultiSelectable());
    setProperty(AXPropertyName::IsPasswordField, object.isPasswordField());
    setProperty(AXPropertyName::IsPressed, object.isPressed());
    setProperty(AXPropertyName::IsProgressIndicator, object.isProgressIndicator());
    setProperty(AXPropertyName::IsRequired, object.isRequired());
    setProperty(AXPropertyName::IsScrollbar, object.isScrollbar());
    setProperty(AXPropertyName::IsSelected, object.isSelected());
    setProperty(AXPropertyName::IsSelectedOptionActive, object.isSelectedOptionActive());
    setProperty(AXPropertyName::IsSlider, object.isSlider());
    setProperty(AXPropertyName::IsStyleFormatGroup, object.isStyleFormatGroup());
    setProperty(AXPropertyName::IsTextControl, object.isTextControl());
    setProperty(AXPropertyName::IsUnvisited, object.isUnvisited());
    setProperty(AXPropertyName::IsValueAutofillAvailable, object.isValueAutofillAvailable());
    setProperty(AXPropertyName::IsVisited, object.isVisited());
    setProperty(AXPropertyName::RoleDescription, object.roleDescription().isolatedCopy());
    setProperty(AXPropertyName::RolePlatformString, object.rolePlatformString().isolatedCopy());
    setProperty(AXPropertyName::RoleValue, static_cast<int>(object.roleValue()));
    setProperty(AXPropertyName::SubrolePlatformString, object.subrolePlatformString().isolatedCopy());
    setProperty(AXPropertyName::SupportsDatetimeAttribute, object.supportsDatetimeAttribute());
    setProperty(AXPropertyName::Title, object.title().isolatedCopy());
    setProperty(AXPropertyName::DatetimeAttributeValue, object.datetimeAttributeValue().isolatedCopy());
    setProperty(AXPropertyName::CanSetFocusAttribute, object.canSetFocusAttribute());
    setProperty(AXPropertyName::CanSetTextRangeAttributes, object.canSetTextRangeAttributes());
    setProperty(AXPropertyName::CanSetValueAttribute, object.canSetValueAttribute());
    setProperty(AXPropertyName::CanSetNumericValue, object.canSetNumericValue());
    setProperty(AXPropertyName::SupportsRequiredAttribute, object.supportsRequiredAttribute());
    setProperty(AXPropertyName::CanSetSelectedAttribute, object.canSetSelectedAttribute());
    setProperty(AXPropertyName::CanSetSelectedChildren, object.canSetSelectedChildren());
    setProperty(AXPropertyName::CanSetExpandedAttribute, object.canSetExpandedAttribute());
    setProperty(AXPropertyName::BlockquoteLevel, object.blockquoteLevel());
    setProperty(AXPropertyName::HeadingLevel, object.headingLevel());
    setProperty(AXPropertyName::ValueDescription, object.valueDescription().isolatedCopy());
    setProperty(AXPropertyName::ValueForRange, object.valueForRange());
    setProperty(AXPropertyName::MaxValueForRange, object.maxValueForRange());
    setProperty(AXPropertyName::MinValueForRange, object.minValueForRange());
    setObjectProperty(AXPropertyName::SelectedRadioButton, object.selectedRadioButton());
    setObjectProperty(AXPropertyName::SelectedTabItem, object.selectedTabItem());
    setProperty(AXPropertyName::SupportsARIAOwns, object.supportsARIAOwns());
    setProperty(AXPropertyName::HasPopup, object.hasPopup());
    setProperty(AXPropertyName::PopupValue, object.popupValue().isolatedCopy());
    setProperty(AXPropertyName::ARIAIsMultiline, object.ariaIsMultiline());
    setProperty(AXPropertyName::InvalidStatus, object.invalidStatus().isolatedCopy());
    setProperty(AXPropertyName::SupportsExpanded, object.supportsExpanded());
    setProperty(AXPropertyName::SortDirection, static_cast<int>(object.sortDirection()));
    setProperty(AXPropertyName::SupportsRangeValue, object.supportsRangeValue());
#if !LOG_DISABLED
    // Eagerly cache ID when logging is enabled so that we can log isolated objects without constant deadlocks.
    // Don't cache ID when logging is disabled because we don't expect non-test AX clients to actually request it.
    setProperty(AXPropertyName::IdentifierAttribute, object.identifierAttribute().isolatedCopy());
#endif
    setProperty(AXPropertyName::LinkRelValue, object.linkRelValue().isolatedCopy());
    setProperty(AXPropertyName::CurrentState, static_cast<int>(object.currentState()));
    setProperty(AXPropertyName::CurrentValue, object.currentValue().isolatedCopy());
    setProperty(AXPropertyName::SupportsCurrent, object.supportsCurrent());
    setProperty(AXPropertyName::KeyShortcutsValue, object.keyShortcutsValue().isolatedCopy());
    setProperty(AXPropertyName::SupportsSetSize, object.supportsSetSize());
    setProperty(AXPropertyName::SupportsPath, object.supportsPath());
    setProperty(AXPropertyName::SupportsPosInSet, object.supportsPosInSet());
    setProperty(AXPropertyName::SetSize, object.setSize());
    setProperty(AXPropertyName::PosInSet, object.posInSet());
    setProperty(AXPropertyName::SupportsDropping, object.supportsDropping());
    setProperty(AXPropertyName::SupportsDragging, object.supportsDragging());
    setProperty(AXPropertyName::IsGrabbed, object.isGrabbed());
    setObjectProperty(AXPropertyName::TitleUIElement, object.titleUIElement());
    setObjectProperty(AXPropertyName::VerticalScrollBar, object.scrollBar(AccessibilityOrientation::Vertical));
    setObjectProperty(AXPropertyName::HorizontalScrollBar, object.scrollBar(AccessibilityOrientation::Horizontal));
    setProperty(AXPropertyName::ARIARoleAttribute, static_cast<int>(object.ariaRoleAttribute()));
    setProperty(AXPropertyName::PlaceholderValue, object.placeholderValue().isolatedCopy());
    setProperty(AXPropertyName::ExpandedTextValue, object.expandedTextValue().isolatedCopy());
    setProperty(AXPropertyName::SupportsExpandedTextValue, object.supportsExpandedTextValue());
    setProperty(AXPropertyName::ComputedRoleString, object.computedRoleString().isolatedCopy());
    setProperty(AXPropertyName::ValueAutofillButtonType, static_cast<int>(object.valueAutofillButtonType()));
    setProperty(AXPropertyName::URL, object.url().isolatedCopy());
    setProperty(AXPropertyName::AccessKey, object.accessKey().isolatedCopy());
    setProperty(AXPropertyName::ReadOnlyValue, object.readOnlyValue().isolatedCopy());
    setProperty(AXPropertyName::AutoCompleteValue, object.autoCompleteValue().isolatedCopy());
    setProperty(AXPropertyName::StringValue, object.stringValue().isolatedCopy());
    setObjectProperty(AXPropertyName::FocusableAncestor, object.focusableAncestor());
    setObjectProperty(AXPropertyName::EditableAncestor, object.editableAncestor());
    setObjectProperty(AXPropertyName::HighestEditableAncestor, object.highestEditableAncestor());
    setProperty(AXPropertyName::Orientation, static_cast<int>(object.orientation()));
    setProperty(AXPropertyName::HierarchicalLevel, object.hierarchicalLevel());
    setProperty(AXPropertyName::Language, object.language().isolatedCopy());
    setProperty(AXPropertyName::TagName, object.tagName().string().isolatedCopy());
    setProperty(AXPropertyName::SupportsLiveRegion, object.supportsLiveRegion());
    setProperty(AXPropertyName::IsInsideLiveRegion, object.isInsideLiveRegion());
    setProperty(AXPropertyName::LiveRegionStatus, object.liveRegionStatus().isolatedCopy());
    setProperty(AXPropertyName::LiveRegionRelevant, object.liveRegionRelevant().isolatedCopy());
    setProperty(AXPropertyName::LiveRegionAtomic, object.liveRegionAtomic());
    setProperty(AXPropertyName::Path, object.elementPath());
    setProperty(AXPropertyName::HasHighlighting, object.hasHighlighting());
    setProperty(AXPropertyName::HasBoldFont, object.hasBoldFont());
    setProperty(AXPropertyName::HasItalicFont, object.hasItalicFont());
    setProperty(AXPropertyName::HasPlainText, object.hasPlainText());
    setProperty(AXPropertyName::HasUnderline, object.hasUnderline());
    setProperty(AXPropertyName::IsKeyboardFocusable, object.isKeyboardFocusable());
    setProperty(AXPropertyName::BrailleRoleDescription, object.brailleRoleDescription().isolatedCopy());
    setProperty(AXPropertyName::BrailleLabel, object.brailleLabel().isolatedCopy());

    if (object.supportsCheckedState()) {
        setProperty(AXPropertyName::SupportsCheckedState, true);
        setProperty(AXPropertyName::IsChecked, object.isChecked());
        setProperty(AXPropertyName::ButtonState, object.checkboxOrRadioValue());
    }

    if (object.isTable()) {
        setProperty(AXPropertyName::IsTable, true);
        setProperty(AXPropertyName::IsExposable, object.isExposable());
        setProperty(AXPropertyName::TableLevel, object.tableLevel());
        setProperty(AXPropertyName::SupportsSelectedRows, object.supportsSelectedRows());
        setObjectVectorProperty(AXPropertyName::Columns, object.columns());
        setObjectVectorProperty(AXPropertyName::Rows, object.rows());
        setProperty(AXPropertyName::ColumnCount, object.columnCount());
        setProperty(AXPropertyName::RowCount, object.rowCount());
        setObjectVectorProperty(AXPropertyName::Cells, object.cells());
        setObjectVectorProperty(AXPropertyName::VisibleRows, object.visibleRows());
        setObjectProperty(AXPropertyName::HeaderContainer, object.headerContainer());
        setProperty(AXPropertyName::AXColumnCount, object.axColumnCount());
        setProperty(AXPropertyName::AXRowCount, object.axRowCount());
    }

    if (object.isTableCell()) {
        setProperty(AXPropertyName::IsTableCell, true);
        setProperty(AXPropertyName::ColumnIndexRange, object.columnIndexRange());
        setProperty(AXPropertyName::RowIndexRange, object.rowIndexRange());
        setProperty(AXPropertyName::AXColumnIndex, object.axColumnIndex());
        setProperty(AXPropertyName::AXRowIndex, object.axRowIndex());
    }

    if (object.isTableColumn()) {
        setProperty(AXPropertyName::IsTableColumn, true);
        setProperty(AXPropertyName::ColumnIndex, object.columnIndex());
        setObjectProperty(AXPropertyName::ColumnHeader, object.columnHeader());
    } else if (object.isTableRow()) {
        setProperty(AXPropertyName::IsTableRow, true);
        setProperty(AXPropertyName::RowIndex, object.rowIndex());
    }

    if (object.isARIATreeGridRow()) {
        setProperty(AXPropertyName::IsARIATreeGridRow, true);
        setObjectVectorProperty(AXPropertyName::DisclosedRows, object.disclosedRows());
        setObjectProperty(AXPropertyName::DisclosedByRow, object.disclosedByRow());
    }

    if (object.isTreeItem()) {
        setProperty(AXPropertyName::IsTreeItem, true);
        setObjectVectorProperty(AXPropertyName::ARIATreeItemContent, object.ariaTreeItemContent());
        setObjectVectorProperty(AXPropertyName::DisclosedRows, object.disclosedRows());
    }

    if (object.isTree()) {
        setProperty(AXPropertyName::IsTree, true);

        AccessibilityChildrenVector ariaTreeRows;
        object.ariaTreeRows(ariaTreeRows);
        setObjectVectorProperty(AXPropertyName::ARIATreeRows, ariaTreeRows);
    }

    if (object.isTextControl())
        setProperty(AXPropertyName::TextLength, object.textLength());

    if (object.isRadioButton()) {
        if (auto nameAttribute = object.attributeValue("name"_s))
            setProperty(AXPropertyName::NameAttribute, nameAttribute->isolatedCopy());
    }

    if (object.canHaveSelectedChildren()) {
        setProperty(AXPropertyName::CanHaveSelectedChildren, true);

        AccessibilityChildrenVector selectedChildren;
        object.selectedChildren(selectedChildren);
        setObjectVectorProperty(AXPropertyName::SelectedChildren, selectedChildren);
    }
    
    if (object.isImage())
        setProperty(AXPropertyName::EmbeddedImageDescription, object.embeddedImageDescription().isolatedCopy());

    setObjectVectorProperty(AXPropertyName::Contents, object.contents());

    AccessibilityChildrenVector visibleChildren;
    object.visibleChildren(visibleChildren);
    setObjectVectorProperty(AXPropertyName::VisibleChildren, visibleChildren);

    AccessibilityChildrenVector tabChildren;
    object.tabChildren(tabChildren);
    setObjectVectorProperty(AXPropertyName::TabChildren, tabChildren);

    setObjectVectorProperty(AXPropertyName::LinkedObjects, object.linkedObjects());

    // Spin button support.
    setObjectProperty(AXPropertyName::DecrementButton, object.decrementButton());
    setObjectProperty(AXPropertyName::IncrementButton, object.incrementButton());

    setProperty(AXPropertyName::ColorValue, object.colorValue());

    if (bool isMathElement = object.isMathElement()) {
        setProperty(AXPropertyName::IsMathElement, isMathElement);
        setProperty(AXPropertyName::IsMathFraction, object.isMathFraction());
        setProperty(AXPropertyName::IsMathFenced, object.isMathFenced());
        setProperty(AXPropertyName::IsMathSubscriptSuperscript, object.isMathSubscriptSuperscript());
        setProperty(AXPropertyName::IsMathRow, object.isMathRow());
        setProperty(AXPropertyName::IsMathUnderOver, object.isMathUnderOver());
        setProperty(AXPropertyName::IsMathTable, object.isMathTable());
        setProperty(AXPropertyName::IsMathTableRow, object.isMathTableRow());
        setProperty(AXPropertyName::IsMathTableCell, object.isMathTableCell());
        setProperty(AXPropertyName::IsMathMultiscript, object.isMathMultiscript());
        setProperty(AXPropertyName::IsMathToken, object.isMathToken());
        setProperty(AXPropertyName::MathFencedOpenString, object.mathFencedOpenString().isolatedCopy());
        setProperty(AXPropertyName::MathFencedCloseString, object.mathFencedCloseString().isolatedCopy());
        setProperty(AXPropertyName::MathLineThickness, object.mathLineThickness());

        bool isMathRoot = object.isMathRoot();
        setProperty(AXPropertyName::IsMathRoot, isMathRoot);
        setProperty(AXPropertyName::IsMathSquareRoot, object.isMathSquareRoot());
        if (isMathRoot) {
            if (auto radicand = object.mathRadicand())
                setObjectVectorProperty(AXPropertyName::MathRadicand, *radicand);

            setObjectProperty(AXPropertyName::MathRootIndexObject, object.mathRootIndexObject());
        }

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

    if (isRoot == IsRoot::Yes) {
        setProperty(AXPropertyName::SessionID, object.sessionID().isolatedCopy());
        setProperty(AXPropertyName::DocumentURI, object.documentURI().isolatedCopy());
        setProperty(AXPropertyName::DocumentEncoding, object.documentEncoding().isolatedCopy());
    }

    // We only expose document links in web area objects.
    if (object.isWebArea())
        setObjectVectorProperty(AXPropertyName::DocumentLinks, object.documentLinks());

    if (object.isWidget())
        setProperty(AXPropertyName::IsWidget, true);

    auto extendedDescription = object.extendedDescription();
    if (extendedDescription.length())
        setProperty(AXPropertyName::ExtendedDescription, extendedDescription.isolatedCopy());

    // These properties are only needed on the AXCoreObject interface due to their use in ATSPI,
    // so only cache them for ATSPI.
#if PLATFORM(ATSPI)
    setProperty(AXPropertyName::ActionVerb, object.actionVerb().isolatedCopy());
    setProperty(AXPropertyName::IsFieldset, object.isFieldset());
    setProperty(AXPropertyName::IsVisible, object.isVisible());
    setProperty(AXPropertyName::LocalizedActionVerb, object.localizedActionVerb().isolatedCopy());
#endif

    initializePlatformProperties(coreObject, isRoot);
}

AccessibilityObject* AXIsolatedObject::associatedAXObject() const
{
    ASSERT(isMainThread());
    ASSERT(objectID().isValid());

    auto* axObjectCache = this->axObjectCache();
    return axObjectCache ? axObjectCache->objectForID(objectID()) : nullptr;
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

    auto idPairs = pairs.map([](auto& mathPair) {
        return std::pair { mathPair.first ? mathPair.first->objectID() : AXID(), mathPair.second ? mathPair.second->objectID() : AXID() };
    });
    setProperty(propertyName, WTFMove(idPairs));
}

void AXIsolatedObject::setObjectProperty(AXPropertyName propertyName, AXCoreObject* object)
{
    if (object)
        setProperty(propertyName, object->objectID());
    else
        setProperty(propertyName, nullptr, true);
}

void AXIsolatedObject::setObjectVectorProperty(AXPropertyName propertyName, const AccessibilityChildrenVector& objects)
{
    if (objects.isEmpty())
        return;
    setProperty(propertyName, axIDs(objects));
}

void AXIsolatedObject::setProperty(AXPropertyName propertyName, AXPropertyValueVariant&& value, bool shouldRemove)
{
    if (shouldRemove)
        m_propertyMap.remove(propertyName);
    else
        m_propertyMap.set(propertyName, value);
}

void AXIsolatedObject::detachRemoteParts(AccessibilityDetachmentType)
{
    for (const auto& childID : m_childrenIDs) {
        if (auto child = tree()->nodeForID(childID))
            child->detachFromParent();
    }
    m_childrenIDs.clear();
}

#if !PLATFORM(MAC)
bool AXIsolatedObject::isDetached() const
{
    ASSERT_NOT_REACHED();
    return false;
}
#endif

void AXIsolatedObject::detachFromParent()
{
    m_parentID = { };
}

const AXCoreObject::AccessibilityChildrenVector& AXIsolatedObject::children(bool updateChildrenIfNeeded)
{
#if USE(APPLE_INTERNAL_SDK)
    ASSERT(_AXSIsolatedTreeModeFunctionIsAvailable() && ((_AXSIsolatedTreeMode_Soft() == AXSIsolatedTreeModeSecondaryThread && !isMainThread())
        || (_AXSIsolatedTreeMode_Soft() == AXSIsolatedTreeModeMainThread && isMainThread())));
#elif USE(ATSPI)
    ASSERT(!isMainThread());
#endif
    RefPtr<AXIsolatedObject> protectedThis;
    if (updateChildrenIfNeeded) {
        // updateBackingStore can delete `this`, so protect it until the end of this function.
        protectedThis = this;
        updateBackingStore();

        m_children.clear();
        m_children.reserveInitialCapacity(m_childrenIDs.size());
        for (const auto& childID : m_childrenIDs) {
            if (auto child = tree()->nodeForID(childID))
                m_children.uncheckedAppend(child);
        }
    }
    return m_children;
}

void AXIsolatedObject::updateChildrenIfNecessary()
{
    // FIXME: this is a no-op for isolated objects and should be removed from
    // the public interface. It is used in the mac implementation of
    // [WebAccessibilityObjectWrapper accessibilityHitTest].
}

void AXIsolatedObject::setSelectedChildren(const AccessibilityChildrenVector& selectedChildren)
{
    ASSERT(selectedChildren.isEmpty() || (selectedChildren[0] && selectedChildren[0]->isAXIsolatedObjectInstance()));

    performFunctionOnMainThread([&] (AXCoreObject* object) {
        if (selectedChildren.isEmpty()) {
            // No selection, no need to convert objects from isolated to live.
            object->setSelectedChildren(selectedChildren);
            return;
        }

        auto* axObjectCache = this->axObjectCache();
        if (!axObjectCache)
            return;

        auto axSelectedChildren = axObjectCache->objectsForIDs(axIDs(selectedChildren));
        object->setSelectedChildren(axSelectedChildren);
    });
}

AXCoreObject::AccessibilityChildrenVector AXIsolatedObject::contents()
{
    AccessibilityChildrenVector result;
    fillChildrenVectorForProperty(AXPropertyName::Contents, result);
    return result;
}

bool AXIsolatedObject::isDetachedFromParent()
{
    if (parent().isValid())
        return false;

    // Check whether this is the root node, in which case we should return false.
    if (auto root = tree()->rootNode())
        return root->objectID() != objectID();
    return false;
}

AXCoreObject* AXIsolatedObject::cellForColumnAndRow(unsigned columnIndex, unsigned rowIndex)
{
    AXID cellID = Accessibility::retrieveValueFromMainThread<AXID>([&columnIndex, &rowIndex, this] () -> AXID {
        if (auto* object = associatedAXObject()) {
            if (auto cell = object->cellForColumnAndRow(columnIndex, rowIndex))
                return cell->objectID();
        }
        return { };
    });

    return tree()->nodeForID(cellID).get();
}

void AXIsolatedObject::accessibilityText(Vector<AccessibilityText>& texts) const
{
    texts = const_cast<AXIsolatedObject*>(this)->getOrRetrievePropertyValue<Vector<AccessibilityText>>(AXPropertyName::AccessibilityText);
}

PAL::SessionID AXIsolatedObject::sessionID() const
{
    if (auto root = tree()->rootNode())
        return root->sessionIDAttributeValue(AXPropertyName::SessionID);
    return PAL::SessionID(PAL::SessionID::SessionConstants::HashTableEmptyValueID);
}

String AXIsolatedObject::documentURI() const
{
    if (auto root = tree()->rootNode())
        return root->stringAttributeValue(AXPropertyName::DocumentURI);
    return String();
}

String AXIsolatedObject::documentEncoding() const
{
    if (auto root = tree()->rootNode())
        return root->stringAttributeValue(AXPropertyName::DocumentEncoding);
    return String();
}

void AXIsolatedObject::insertMathPairs(Vector<std::pair<AXID, AXID>>& isolatedPairs, AccessibilityMathMultiscriptPairs& pairs)
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
    auto isolatedPairs = vectorAttributeValue<std::pair<AXID, AXID>>(AXPropertyName::MathPrescripts);
    insertMathPairs(isolatedPairs, pairs);
}

void AXIsolatedObject::mathPostscripts(AccessibilityMathMultiscriptPairs& pairs)
{
    auto isolatedPairs = vectorAttributeValue<std::pair<AXID, AXID>>(AXPropertyName::MathPostscripts);
    insertMathPairs(isolatedPairs, pairs);
}

std::optional<AXCoreObject::AccessibilityChildrenVector> AXIsolatedObject::mathRadicand()
{
    if (m_propertyMap.contains(AXPropertyName::MathRadicand)) {
        Vector<RefPtr<AXCoreObject>> radicand;
        fillChildrenVectorForProperty(AXPropertyName::MathRadicand, radicand);
        return { radicand };
    }
    return std::nullopt;
}

AXCoreObject* AXIsolatedObject::focusedUIElement() const
{
    return tree()->focusedNode().get();
}
    
AXCoreObject* AXIsolatedObject::parentObjectUnignored() const
{
    return tree()->nodeForID(parent()).get();
}

AXCoreObject* AXIsolatedObject::scrollBar(AccessibilityOrientation orientation)
{
    return objectAttributeValue(orientation == AccessibilityOrientation::Vertical ? AXPropertyName::VerticalScrollBar : AXPropertyName::HorizontalScrollBar);
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

bool AXIsolatedObject::performDismissAction()
{
    return Accessibility::retrieveValueFromMainThread<bool>([this] () -> bool {
        if (auto* axObject = associatedAXObject())
            return axObject->performDismissAction();
        return false;
    });
}

void AXIsolatedObject::scrollToMakeVisible() const
{
    performFunctionOnMainThread([] (AXCoreObject* axObject) {
        axObject->scrollToMakeVisible();
    });
}

void AXIsolatedObject::scrollToMakeVisibleWithSubFocus(const IntRect& rect) const
{
    performFunctionOnMainThread([&rect] (AXCoreObject* axObject) {
        axObject->scrollToMakeVisibleWithSubFocus(rect);
    });
}

void AXIsolatedObject::scrollToGlobalPoint(const IntPoint& point) const
{
    performFunctionOnMainThread([&point] (AXCoreObject* axObject) {
        axObject->scrollToGlobalPoint(point);
    });
}

bool AXIsolatedObject::setValue(float value)
{
    return Accessibility::retrieveValueFromMainThread<bool>([&value, this] () -> bool {
        if (auto* axObject = associatedAXObject())
            return axObject->setValue(value);
        return false;
    });
}

bool AXIsolatedObject::setValue(const String& value)
{
    return Accessibility::retrieveValueFromMainThread<bool>([&value, this] () -> bool {
        if (auto* axObject = associatedAXObject())
            return axObject->setValue(value);
        return false;
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

#if PLATFORM(COCOA) && !PLATFORM(IOS_FAMILY)
void AXIsolatedObject::setCaretBrowsingEnabled(bool value)
{
    performFunctionOnMainThread([&value](AXCoreObject* object) {
        object->setCaretBrowsingEnabled(value);
    });
}
#endif

String AXIsolatedObject::computedLabel()
{
    // This is only used by the web inspector that calls AccessibilityObject::computedLabel().
    ASSERT_NOT_REACHED();
    return { };
}

SRGBA<uint8_t> AXIsolatedObject::colorValue() const
{
    return colorAttributeValue(AXPropertyName::ColorValue).toColorTypeLossy<SRGBA<uint8_t>>();
}

AXCoreObject* AXIsolatedObject::accessibilityHitTest(const IntPoint& point) const
{
    AXID axID = Accessibility::retrieveValueFromMainThread<AXID>([&point, this] () -> AXID {
        if (auto* object = associatedAXObject()) {
            object->updateChildrenIfNecessary();
            if (auto* axObject = object->accessibilityHitTest(point))
                return axObject->objectID();
        }

        return { };
    });

    return tree()->nodeForID(axID).get();
}

IntPoint AXIsolatedObject::intPointAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (IntPoint& typedValue) -> IntPoint { return typedValue; },
        [] (auto&) { return IntPoint(); }
    );
}

AXCoreObject* AXIsolatedObject::objectAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    AXID nodeID = WTF::switchOn(value,
        [] (AXID& typedValue) -> AXID { return typedValue; },
        [] (auto&) { return AXID(); }
    );

    return tree()->nodeForID(nodeID).get();
}

PAL::SessionID AXIsolatedObject::sessionIDAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (PAL::SessionID& typedValue) -> PAL::SessionID { return typedValue; },
        [] (auto&) { return PAL::SessionID(PAL::SessionID::SessionConstants::HashTableEmptyValueID); }
    );
}

template<typename T>
T AXIsolatedObject::rectAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (T& typedValue) -> T { return typedValue; },
        [] (auto&) { return T { }; }
    );
}

template<typename T>
Vector<T> AXIsolatedObject::vectorAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (Vector<T>& typedValue) -> Vector<T> { return typedValue; },
        [] (auto&) { return Vector<T>(); }
    );
}

template<typename T>
OptionSet<T> AXIsolatedObject::optionSetAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (T& typedValue) -> OptionSet<T> { return typedValue; },
        [] (auto&) { return OptionSet<T>(); }
    );
}

template<typename T>
std::pair<T, T> AXIsolatedObject::pairAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (std::pair<T, T>& typedValue) -> std::pair<T, T> { return typedValue; },
        [] (auto&) { return std::pair<T, T>(0, 1); }
    );
}

uint64_t AXIsolatedObject::uint64AttributeValue(AXPropertyName propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (uint64_t& typedValue) -> uint64_t { return typedValue; },
        [] (auto&) -> uint64_t { return 0; }
    );
}

URL AXIsolatedObject::urlAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (URL& typedValue) -> URL { return typedValue; },
        [] (auto&) { return URL(); }
    );
}

Path AXIsolatedObject::pathAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (Path& typedValue) -> Path { return typedValue; },
        [] (auto&) { return Path(); }
    );
}

Color AXIsolatedObject::colorAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (Color& typedValue) -> Color { return typedValue; },
        [] (auto&) { return Color(); }
    );
}

float AXIsolatedObject::floatAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (float& typedValue) -> float { return typedValue; },
        [] (auto&) { return 0.0f; }
    );
}

double AXIsolatedObject::doubleAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (double& typedValue) -> double { return typedValue; },
        [] (auto&) { return 0.0; }
    );
}

unsigned AXIsolatedObject::unsignedAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (unsigned& typedValue) -> unsigned { return typedValue; },
        [] (auto&) { return 0u; }
    );
}

bool AXIsolatedObject::boolAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (bool& typedValue) { return typedValue; },
        [] (auto&) { return false; }
    );
}

String AXIsolatedObject::stringAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (String& typedValue) { return typedValue; },
        [] (auto&) { return emptyString(); }
    );
}

int AXIsolatedObject::intAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (int& typedValue) { return typedValue; },
        [] (auto&) { return 0; }
    );
}

template<typename T>
T AXIsolatedObject::getOrRetrievePropertyValue(AXPropertyName propertyName)
{
    if (m_propertyMap.contains(propertyName))
        return propertyValue<T>(propertyName);

    Accessibility::performFunctionOnMainThread([&propertyName, this] () {
        auto* axObject = associatedAXObject();
        if (!axObject)
            return;

        AXPropertyValueVariant value;
        switch (propertyName) {
#if PLATFORM(COCOA)
        case AXPropertyName::Description:
            value = axObject->descriptionAttributeValue().isolatedCopy();
            break;
        case AXPropertyName::HelpText:
            value = axObject->helpTextAttributeValue().isolatedCopy();
            break;
        case AXPropertyName::TitleAttributeValue:
            value = axObject->titleAttributeValue().isolatedCopy();
            break;
#endif
        case AXPropertyName::AccessibilityText: {
            Vector<AccessibilityText> texts;
            axObject->accessibilityText(texts);
            value = texts.map([] (const auto& text) -> AccessibilityText {
                return { text.text.isolatedCopy(), text.textSource };
            });
            break;
        }
        case AXPropertyName::ColumnHeaders:
            value = axIDs(axObject->columnHeaders());
            break;
        case AXPropertyName::InnerHTML:
            value = axObject->innerHTML().isolatedCopy();
            break;
        case AXPropertyName::OuterHTML:
            value = axObject->outerHTML().isolatedCopy();
            break;
        case AXPropertyName::RowHeaders:
            value = axIDs(axObject->rowHeaders());
            break;
        case AXPropertyName::SupportsPressAction:
            value = axObject->supportsPressAction();
            break;
        default:
            break;
        }

        // Cache value so that there is no need to access the main thread in subsequent calls.
        setProperty(propertyName, WTFMove(value));
    });

    return propertyValue<T>(propertyName);
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
    if (auto tree = this->tree())
        tree->applyPendingChanges();
    // AXIsolatedTree::applyPendingChanges can cause this object and / or the AXIsolatedTree to be destroyed.
    // Make sure to protect `this` with a Ref before adding more logic to this function.
}

std::optional<SimpleRange> AXIsolatedObject::visibleCharacterRange() const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->visibleCharacterRange() : std::nullopt;
}

std::optional<SimpleRange> AXIsolatedObject::rangeForPlainTextRange(const PlainTextRange& axRange) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->rangeForPlainTextRange(axRange) : std::nullopt;
}

String AXIsolatedObject::stringForRange(const SimpleRange& range) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->stringForRange(range).isolatedCopy() : String();
}

IntRect AXIsolatedObject::boundsForRange(const SimpleRange& range) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->boundsForRange(range) : IntRect();
}

IntRect AXIsolatedObject::boundsForVisiblePositionRange(const VisiblePositionRange&) const
{
    ASSERT_NOT_REACHED();
    return { };
}

VisiblePosition AXIsolatedObject::visiblePositionForPoint(const IntPoint& point) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->visiblePositionForPoint(point) : VisiblePosition();
}

VisiblePosition AXIsolatedObject::nextLineEndPosition(const VisiblePosition&) const
{
    ASSERT_NOT_REACHED();
    return { };
}

VisiblePosition AXIsolatedObject::previousLineStartPosition(const VisiblePosition&) const
{
    ASSERT_NOT_REACHED();
    return { };
}

VisiblePosition AXIsolatedObject::nextSentenceEndPosition(const VisiblePosition&) const
{
    ASSERT_NOT_REACHED();
    return { };
}

VisiblePosition AXIsolatedObject::previousSentenceStartPosition(const VisiblePosition&) const
{
    ASSERT_NOT_REACHED();
    return { };
}

VisiblePosition AXIsolatedObject::nextParagraphEndPosition(const VisiblePosition&) const
{
    ASSERT_NOT_REACHED();
    return { };
}

VisiblePosition AXIsolatedObject::previousParagraphStartPosition(const VisiblePosition&) const
{
    ASSERT_NOT_REACHED();
    return { };
}

VisiblePosition AXIsolatedObject::visiblePositionForIndex(int index) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->visiblePositionForIndex(index) : VisiblePosition();
}

int AXIsolatedObject::indexForVisiblePosition(const VisiblePosition&) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

PlainTextRange AXIsolatedObject::plainTextRangeForVisiblePositionRange(const VisiblePositionRange&) const
{
    ASSERT_NOT_REACHED();
    return { };
}

int AXIsolatedObject::index(const VisiblePosition&) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

Vector<SimpleRange> AXIsolatedObject::findTextRanges(const AccessibilitySearchTextCriteria& criteria) const
{
    return Accessibility::retrieveValueFromMainThread<Vector<SimpleRange>>([&criteria, this] () -> Vector<SimpleRange> {
        if (auto* object = associatedAXObject())
            return object->findTextRanges(criteria);
        return { };
    });
}

Vector<String> AXIsolatedObject::performTextOperation(const AccessibilityTextOperation& textOperation)
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

String AXIsolatedObject::textUnderElement(AccessibilityTextUnderElementMode) const
{
    ASSERT_NOT_REACHED();
    return { };
}

std::optional<SimpleRange> AXIsolatedObject::misspellingRange(const SimpleRange& range, AccessibilitySearchDirection direction) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->misspellingRange(range, direction) : std::nullopt;
}

LayoutRect AXIsolatedObject::boundingBoxRect() const
{
    return Accessibility::retrieveValueFromMainThread<LayoutRect>([this] () -> LayoutRect {
        if (auto* axObject = associatedAXObject())
            return axObject->boundingBoxRect();
        return { };
    });
}

LayoutRect AXIsolatedObject::elementRect() const
{
    return Accessibility::retrieveValueFromMainThread<LayoutRect>([this] () -> LayoutRect {
        if (auto* axObject = associatedAXObject())
            return axObject->elementRect();
        return { };
    });
}

FloatRect AXIsolatedObject::relativeFrame() const
{
    // Retrieve this on the main thread because we require the scroll ancestor to convert to the right scroll offset.
    return Accessibility::retrieveValueFromMainThread<FloatRect>([this] () -> FloatRect {
        if (auto* axObject = associatedAXObject())
            return axObject->relativeFrame();
        return { };
    });
}

FloatRect AXIsolatedObject::convertFrameToSpace(const FloatRect& rect, AccessibilityConversionSpace space) const
{
    return Accessibility::retrieveValueFromMainThread<FloatRect>([&rect, &space, this] () -> FloatRect {
        if (auto* axObject = associatedAXObject())
            return axObject->convertFrameToSpace(rect, space);
        return { };
    });
}

bool AXIsolatedObject::replaceTextInRange(const String& replacementText, const PlainTextRange& textRange)
{
    return Accessibility::retrieveValueFromMainThread<bool>([text = replacementText.isolatedCopy(), &textRange, this] () -> bool {
        if (auto* axObject = associatedAXObject())
            return axObject->replaceTextInRange(text, textRange);
        return false;
    });
}

bool AXIsolatedObject::insertText(const String& text)
{
    AXTRACE(makeString("AXIsolatedObject::insertText text = ", text));

    // Dispatch to the main thread without waiting since AXObject::insertText waits for the UI process that can be waiting resulting in a deadlock. That is the case when running LayoutTests.
    // The return value of insertText is not used, so not waiting does not result in any loss of functionality.
    callOnMainThread([text = text.isolatedCopy(), this] () {
        if (auto* axObject = associatedAXObject())
            axObject->insertText(text);
    });
    return true;
}

void AXIsolatedObject::makeRangeVisible(const PlainTextRange& axRange)
{
    performFunctionOnMainThread([&axRange] (AXCoreObject* axObject) {
        axObject->makeRangeVisible(axRange);
    });
}

bool AXIsolatedObject::press()
{
    if (auto* object = associatedAXObject())
        return object->press();
    return false;
}

void AXIsolatedObject::increment()
{
    performFunctionOnMainThread([](AXCoreObject* axObject) {
        axObject->increment();
    });
}

void AXIsolatedObject::decrement()
{
    performFunctionOnMainThread([](AXCoreObject* axObject) {
        axObject->decrement();
    });
}

bool AXIsolatedObject::performDefaultAction()
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

bool AXIsolatedObject::isAccessibilityScrollViewInstance() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isAccessibilityTableInstance() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isAccessibilityTableColumnInstance() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isAccessibilityProgressIndicatorInstance() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isAccessibilityListBoxInstance() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isNativeTextControl() const
{
    ASSERT_NOT_REACHED();
    return false;
}

PlainTextRange AXIsolatedObject::selectedTextRange() const
{
    return Accessibility::retrieveValueFromMainThread<PlainTextRange>([this] () -> PlainTextRange {
        if (auto* object = associatedAXObject())
            return object->selectedTextRange();
        return { };
    });
}

int AXIsolatedObject::insertionPointLineNumber() const
{
    return Accessibility::retrieveValueFromMainThread<int>([this] () -> int {
        if (auto* axObject = associatedAXObject())
            return axObject->insertionPointLineNumber();
        return -1;
    });
}

String AXIsolatedObject::identifierAttribute() const
{
#if !LOG_DISABLED
    return stringAttributeValue(AXPropertyName::IdentifierAttribute);
#else
    return Accessibility::retrieveValueFromMainThread<String>([this] () -> String {
        if (auto* object = associatedAXObject())
            return object->identifierAttribute().isolatedCopy();
        return { };
    });
#endif
}

PlainTextRange AXIsolatedObject::doAXRangeForLine(unsigned lineIndex) const
{
    return Accessibility::retrieveValueFromMainThread<PlainTextRange>([&lineIndex, this] () -> PlainTextRange {
        if (auto* object = associatedAXObject())
            return object->doAXRangeForLine(lineIndex);
        return { };
    });
}

String AXIsolatedObject::doAXStringForRange(const PlainTextRange& axRange) const
{
    return Accessibility::retrieveValueFromMainThread<String>([&axRange, this] () -> String {
        if (auto* object = associatedAXObject())
            return object->doAXStringForRange(axRange).isolatedCopy();
        return { };
    });
}

PlainTextRange AXIsolatedObject::doAXRangeForPosition(const IntPoint& point) const
{
    return Accessibility::retrieveValueFromMainThread<PlainTextRange>([&point, this] () -> PlainTextRange {
        if (auto* object = associatedAXObject())
            return object->doAXRangeForPosition(point);
        return { };
    });
}

PlainTextRange AXIsolatedObject::doAXRangeForIndex(unsigned index) const
{
    return Accessibility::retrieveValueFromMainThread<PlainTextRange>([&index, this] () -> PlainTextRange {
        if (auto* object = associatedAXObject())
            return object->doAXRangeForIndex(index);
        return { };
    });
}

PlainTextRange AXIsolatedObject::doAXStyleRangeForIndex(unsigned index) const
{
    return Accessibility::retrieveValueFromMainThread<PlainTextRange>([&index, this] () -> PlainTextRange {
        if (auto* object = associatedAXObject())
            return object->doAXStyleRangeForIndex(index);
        return { };
    });
}

IntRect AXIsolatedObject::doAXBoundsForRange(const PlainTextRange& axRange) const
{
    return Accessibility::retrieveValueFromMainThread<IntRect>([&axRange, this] () -> IntRect {
        if (auto* object = associatedAXObject())
            return object->doAXBoundsForRange(axRange);
        return { };
    });
}

IntRect AXIsolatedObject::doAXBoundsForRangeUsingCharacterOffset(const PlainTextRange& axRange) const
{
    return Accessibility::retrieveValueFromMainThread<IntRect>([&axRange, this] () -> IntRect {
        if (auto* object = associatedAXObject())
            return object->doAXBoundsForRangeUsingCharacterOffset(axRange);
        return { };
    });
}


unsigned AXIsolatedObject::doAXLineForIndex(unsigned index)
{
    return Accessibility::retrieveValueFromMainThread<unsigned>([&index, this] () -> unsigned {
        if (auto* object = associatedAXObject())
            return object->doAXLineForIndex(index);
        return 0;
    });
}

VisibleSelection AXIsolatedObject::selection() const
{
    ASSERT(isMainThread());

    auto* object = associatedAXObject();
    return object ? object->selection() : VisibleSelection();
}

VisiblePositionRange AXIsolatedObject::selectedVisiblePositionRange() const
{
    ASSERT(isMainThread());

    if (auto* axObject = associatedAXObject())
        return axObject->selectedVisiblePositionRange();
    return { };
}

void AXIsolatedObject::setSelectedVisiblePositionRange(const VisiblePositionRange& visiblePositionRange) const
{
    ASSERT(isMainThread());

    if (auto* object = associatedAXObject())
        object->setSelectedVisiblePositionRange(visiblePositionRange);
}

#if PLATFORM(COCOA) && ENABLE(MODEL_ELEMENT)
Vector<RetainPtr<id>> AXIsolatedObject::modelElementChildren()
{
    return Accessibility::retrieveValueFromMainThread<Vector<RetainPtr<id>>>([this] () -> Vector<RetainPtr<id>> {
        if (auto* object = associatedAXObject())
            return object->modelElementChildren();
        return { };
    });
}
#endif

FloatRect AXIsolatedObject::unobscuredContentRect() const
{
    return Accessibility::retrieveValueFromMainThread<FloatRect>([this] () -> FloatRect {
        if (auto* object = associatedAXObject())
            return object->unobscuredContentRect();
        return { };
    });
}

std::optional<SimpleRange> AXIsolatedObject::elementRange() const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->elementRange() : std::nullopt;
}

String AXIsolatedObject::selectedText() const
{
    return Accessibility::retrieveValueFromMainThread<String>([this] () -> String {
        if (auto* object = associatedAXObject())
            return object->selectedText().isolatedCopy();
        return { };
    });
}

VisiblePositionRange AXIsolatedObject::visiblePositionRange() const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->visiblePositionRange() : VisiblePositionRange();
}

VisiblePositionRange AXIsolatedObject::visiblePositionRangeForLine(unsigned index) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->visiblePositionRangeForLine(index) : VisiblePositionRange();
}

VisiblePositionRange AXIsolatedObject::visiblePositionRangeForUnorderedPositions(const VisiblePosition& position1, const VisiblePosition& position2) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->visiblePositionRangeForUnorderedPositions(position1, position2) : visiblePositionRange();
}

VisiblePositionRange AXIsolatedObject::positionOfLeftWord(const VisiblePosition& position) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->positionOfLeftWord(position) : VisiblePositionRange();
}

VisiblePositionRange AXIsolatedObject::positionOfRightWord(const VisiblePosition& position) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->positionOfRightWord(position) : VisiblePositionRange();
}

VisiblePositionRange AXIsolatedObject::leftLineVisiblePositionRange(const VisiblePosition& position) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->leftLineVisiblePositionRange(position) : VisiblePositionRange();
}

VisiblePositionRange AXIsolatedObject::rightLineVisiblePositionRange(const VisiblePosition& position) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->rightLineVisiblePositionRange(position) : VisiblePositionRange();
}

VisiblePositionRange AXIsolatedObject::sentenceForPosition(const VisiblePosition& position) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->sentenceForPosition(position) : VisiblePositionRange();
}

VisiblePositionRange AXIsolatedObject::paragraphForPosition(const VisiblePosition& position) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->paragraphForPosition(position) : VisiblePositionRange();
}

VisiblePositionRange AXIsolatedObject::styleRangeForPosition(const VisiblePosition& position) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->styleRangeForPosition(position) : VisiblePositionRange();
}

VisiblePositionRange AXIsolatedObject::visiblePositionRangeForRange(const PlainTextRange& plainTextRange) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->visiblePositionRangeForRange(plainTextRange) : VisiblePositionRange();
}

VisiblePositionRange AXIsolatedObject::lineRangeForPosition(const VisiblePosition& position) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->lineRangeForPosition(position) : VisiblePositionRange();
}

VisiblePosition AXIsolatedObject::visiblePositionForIndex(unsigned index, bool lastIndexOK) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->visiblePositionForIndex(index, lastIndexOK) : VisiblePosition();
}

int AXIsolatedObject::lineForPosition(const VisiblePosition& position) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->lineForPosition(position) : -1;
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

bool AXIsolatedObject::isIndeterminate() const
{
    ASSERT_NOT_REACHED();
    return false;
}

HashMap<String, AXEditingStyleValueVariant> AXIsolatedObject::resolvedEditingStyles() const
{
    return Accessibility::retrieveValueFromMainThread<HashMap<String, AXEditingStyleValueVariant>>([this] () -> HashMap<String, AXEditingStyleValueVariant> {
        if (auto* object = associatedAXObject())
            return object->resolvedEditingStyles();
        return { };
    });
}

bool AXIsolatedObject::isOnScreen() const
{
    return Accessibility::retrieveValueFromMainThread<bool>([this] () -> bool {
        if (auto* object = associatedAXObject())
            return object->isOnScreen();
        return false;
    });
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

bool AXIsolatedObject::isSelectedOptionActive() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::hasMisspelling() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::hasSameFont(const AXCoreObject& otherObject) const
{
    if (!is<AXIsolatedObject>(otherObject))
        return false;

    return Accessibility::retrieveValueFromMainThread<bool>([&otherObject, this] () -> bool {
        if (auto* axObject = associatedAXObject()) {
            if (auto* axOtherObject = downcast<AXIsolatedObject>(otherObject).associatedAXObject())
                return axObject->hasSameFont(*axOtherObject);
        }
        return false;
    });
}

bool AXIsolatedObject::hasSameFontColor(const AXCoreObject& otherObject) const
{
    if (!is<AXIsolatedObject>(otherObject))
        return false;

    return Accessibility::retrieveValueFromMainThread<bool>([&otherObject, this] () -> bool {
        if (auto* axObject = associatedAXObject()) {
            if (auto* axOtherObject = downcast<AXIsolatedObject>(otherObject).associatedAXObject())
                return axObject->hasSameFontColor(*axOtherObject);
        }
        return false;
    });
}

bool AXIsolatedObject::hasSameStyle(const AXCoreObject& otherObject) const
{
    if (!is<AXIsolatedObject>(otherObject))
        return false;

    return Accessibility::retrieveValueFromMainThread<bool>([&otherObject, this] () -> bool {
        if (auto* axObject = associatedAXObject()) {
            if (auto* axOtherObject = downcast<AXIsolatedObject>(otherObject).associatedAXObject())
                return axObject->hasSameStyle(*axOtherObject);
        }
        return false;
    });
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

AXCoreObject* AXIsolatedObject::parentObjectIfExists() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
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

TextIteratorBehaviors AXIsolatedObject::textIteratorBehaviorForTextRange() const
{
    ASSERT_NOT_REACHED();
    return { };
}

Widget* AXIsolatedObject::widget() const
{
    auto* object = associatedAXObject();
    return object ? object->widget() : nullptr;
}

PlatformWidget AXIsolatedObject::platformWidget() const
{
#if PLATFORM(COCOA)
    return m_platformWidget.get();
#else
    return m_platformWidget;
#endif
}

Widget* AXIsolatedObject::widgetForAttachmentView() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

Page* AXIsolatedObject::page() const
{
    ASSERT(isMainThread());

    if (auto* axObject = associatedAXObject())
        return axObject->page();

    ASSERT_NOT_REACHED();
    return nullptr;
}

Document* AXIsolatedObject::document() const
{
    ASSERT(isMainThread());

    if (auto* axObject = associatedAXObject())
        return axObject->document();

    ASSERT_NOT_REACHED();
    return nullptr;
}

FrameView* AXIsolatedObject::documentFrameView() const
{
    ASSERT(isMainThread());

    if (auto* axObject = associatedAXObject())
        return axObject->documentFrameView();

    ASSERT_NOT_REACHED();
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

ScrollView* AXIsolatedObject::scrollView() const
{
    if (auto* object = associatedAXObject())
        return object->scrollView();
    return nullptr;
}

ScrollView* AXIsolatedObject::scrollViewAncestor() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

bool AXIsolatedObject::shouldFocusActiveDescendant() const
{
    ASSERT_NOT_REACHED();
    return false;
}

AXCoreObject::AccessibilityChildrenVector AXIsolatedObject::relatedObjects(AXRelationType relationType) const
{
    if (auto relatedObjectIDs = tree()->relatedObjectIDsFor(*this, relationType))
        return tree()->objectsForIDs(*relatedObjectIDs);
    return { };
}

AXCoreObject* AXIsolatedObject::activeDescendant() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

OptionSet<AXAncestorFlag> AXIsolatedObject::ancestorFlags() const
{
    auto value = m_propertyMap.get(AXPropertyName::AncestorFlags);
    return WTF::switchOn(value,
        [] (OptionSet<AXAncestorFlag>& typedValue) -> OptionSet<AXAncestorFlag> { return typedValue; },
        [] (auto&) { return OptionSet<AXAncestorFlag>(); }
    );
}

AXCoreObject* AXIsolatedObject::firstAnonymousBlockChild() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

std::optional<String> AXIsolatedObject::attributeValue(const String& attributeName) const
{
    if (attributeName == "name"_s) {
        if (m_propertyMap.contains(AXPropertyName::NameAttribute))
            return stringAttributeValue(AXPropertyName::NameAttribute);
    }
    return std::nullopt;
}

AXCoreObject::AccessibilityChildrenVector AXIsolatedObject::columnHeaders()
{
    return tree()->objectsForIDs(const_cast<AXIsolatedObject*>(this)->getOrRetrievePropertyValue<Vector<AXID>>(AXPropertyName::ColumnHeaders));
}

String AXIsolatedObject::innerHTML() const
{
    return const_cast<AXIsolatedObject*>(this)->getOrRetrievePropertyValue<String>(AXPropertyName::InnerHTML);
}

String AXIsolatedObject::outerHTML() const
{
    return const_cast<AXIsolatedObject*>(this)->getOrRetrievePropertyValue<String>(AXPropertyName::OuterHTML);
}

AXCoreObject::AccessibilityChildrenVector AXIsolatedObject::rowHeaders()
{
    return tree()->objectsForIDs(const_cast<AXIsolatedObject*>(this)->getOrRetrievePropertyValue<Vector<AXID>>(AXPropertyName::RowHeaders));
}

// FIXME: Rather than lazily retrieving this property, we should make it performant enough to retrieve up-front in AXIsolatedObject::initializeProperties.
bool AXIsolatedObject::supportsPressAction() const
{
    return const_cast<AXIsolatedObject*>(this)->getOrRetrievePropertyValue<bool>(AXPropertyName::SupportsPressAction);
}

#if !PLATFORM(MAC)
IntPoint AXIsolatedObject::clickPoint()
{
    ASSERT_NOT_REACHED();
    return { };
}

Vector<String> AXIsolatedObject::determineDropEffects() const
{
    ASSERT_NOT_REACHED();
    return { };
}

bool AXIsolatedObject::pressedIsPresent() const
{
    ASSERT_NOT_REACHED();
    return false;
}

int AXIsolatedObject::layoutCount() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

Vector<String> AXIsolatedObject::classList() const
{
    ASSERT_NOT_REACHED();
    return { };
}
#endif

} // namespace WebCore

#endif // ENABLE((ACCESSIBILITY_ISOLATED_TREE)
