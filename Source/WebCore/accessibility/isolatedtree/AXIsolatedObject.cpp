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

#include "AXGeometryManager.h"
#include "AXIsolatedTree.h"
#include "AXLogger.h"
#include <pal/SessionID.h>

#if PLATFORM(MAC)
#import <pal/spi/mac/HIServicesSPI.h>
#endif

#if PLATFORM(COCOA)
#include <pal/spi/cocoa/AccessibilitySupportSoftLink.h>
#endif

namespace WebCore {

AXIsolatedObject::AXIsolatedObject(const Ref<AccessibilityObject>& axObject, AXIsolatedTree* tree)
    : AXCoreObject(axObject->objectID())
    , m_cachedTree(tree)
{
    ASSERT(isMainThread());
    ASSERT(objectID().isValid());

    auto* axParent = axObject->parentObjectUnignored();
    m_parentID = axParent ? axParent->objectID() : AXID();

    auto isRoot = !axParent && axObject->isScrollView() ? IsRoot::Yes : IsRoot::No;

    // Every object will have at least this many properties. We can shrink this number
    // to some estimated average once we implement sparse property storage (i.e. only storing
    // a property if it's not the default value for its type).
    m_propertyMap.reserveInitialCapacity(94);

    initializeProperties(axObject, isRoot);
}

Ref<AXIsolatedObject> AXIsolatedObject::create(const Ref<AccessibilityObject>& object, AXIsolatedTree* tree)
{
    return adoptRef(*new AXIsolatedObject(object, tree));
}

AXIsolatedObject::~AXIsolatedObject()
{
    ASSERT(!wrapper());
}

void AXIsolatedObject::initializeProperties(const Ref<AccessibilityObject>& axObject, IsRoot isRoot)
{
    auto& object = axObject.get();

    if (object.ancestorFlagsAreInitialized())
        setProperty(AXPropertyName::AncestorFlags, object.ancestorFlags());
    else
        setProperty(AXPropertyName::AncestorFlags, object.computeAncestorFlagsWithTraversal());

    setProperty(AXPropertyName::IsAttachment, object.isAttachment());
    setProperty(AXPropertyName::IsBusy, object.isBusy());
    setProperty(AXPropertyName::IsControl, object.isControl());
    setProperty(AXPropertyName::IsEnabled, object.isEnabled());
    setProperty(AXPropertyName::IsExpanded, object.isExpanded());
    setProperty(AXPropertyName::IsFocused, object.isFocused());
    setProperty(AXPropertyName::IsIndeterminate, object.isIndeterminate());
    setProperty(AXPropertyName::IsInlineText, object.isInlineText());
    setProperty(AXPropertyName::IsInputImage, object.isInputImage());
    setProperty(AXPropertyName::IsLink, object.isLink());
    setProperty(AXPropertyName::IsList, object.isList());
    setProperty(AXPropertyName::IsMultiSelectable, object.isMultiSelectable());
    setProperty(AXPropertyName::IsPressed, object.isPressed());
    setProperty(AXPropertyName::IsRequired, object.isRequired());
    setProperty(AXPropertyName::IsSecureField, object.isSecureField());
    setProperty(AXPropertyName::IsSelected, object.isSelected());
    setProperty(AXPropertyName::InsideLink, object.insideLink());
    setProperty(AXPropertyName::IsValueAutofillAvailable, object.isValueAutofillAvailable());
    setProperty(AXPropertyName::RoleDescription, object.roleDescription().isolatedCopy());
    setProperty(AXPropertyName::RolePlatformString, object.rolePlatformString().isolatedCopy());
    setProperty(AXPropertyName::RoleValue, static_cast<int>(object.roleValue()));
    setProperty(AXPropertyName::SubrolePlatformString, object.subrolePlatformString().isolatedCopy());
    setProperty(AXPropertyName::SupportsDatetimeAttribute, object.supportsDatetimeAttribute());
    setProperty(AXPropertyName::DatetimeAttributeValue, object.datetimeAttributeValue().isolatedCopy());
    setProperty(AXPropertyName::CanSetFocusAttribute, object.canSetFocusAttribute());
    setProperty(AXPropertyName::CanSetValueAttribute, object.canSetValueAttribute());
    setProperty(AXPropertyName::SupportsRequiredAttribute, object.supportsRequiredAttribute());
    setProperty(AXPropertyName::CanSetSelectedAttribute, object.canSetSelectedAttribute());
    setProperty(AXPropertyName::CanSetSelectedChildren, object.canSetSelectedChildren());
    setProperty(AXPropertyName::BlockquoteLevel, object.blockquoteLevel());
    setProperty(AXPropertyName::HeadingLevel, object.headingLevel());
    setProperty(AXPropertyName::ValueDescription, object.valueDescription().isolatedCopy());
    setProperty(AXPropertyName::ValueForRange, object.valueForRange());
    setProperty(AXPropertyName::MaxValueForRange, object.maxValueForRange());
    setProperty(AXPropertyName::MinValueForRange, object.minValueForRange());
    setProperty(AXPropertyName::SupportsARIAOwns, object.supportsARIAOwns());
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
    setProperty(AXPropertyName::CurrentState, static_cast<int>(object.currentState()));
    setProperty(AXPropertyName::SupportsCurrent, object.supportsCurrent());
    setProperty(AXPropertyName::SupportsKeyShortcuts, object.supportsKeyShortcuts());
    setProperty(AXPropertyName::KeyShortcuts, object.keyShortcuts().isolatedCopy());
    setProperty(AXPropertyName::SupportsSetSize, object.supportsSetSize());
    setProperty(AXPropertyName::SupportsPath, object.supportsPath());
    setProperty(AXPropertyName::SupportsPosInSet, object.supportsPosInSet());
    setProperty(AXPropertyName::SetSize, object.setSize());
    setProperty(AXPropertyName::PosInSet, object.posInSet());
    setProperty(AXPropertyName::SupportsDropping, object.supportsDropping());
    setProperty(AXPropertyName::SupportsDragging, object.supportsDragging());
    setProperty(AXPropertyName::SupportsPressAction, object.supportsPressAction());
    setProperty(AXPropertyName::IsGrabbed, object.isGrabbed());
    setObjectProperty(AXPropertyName::TitleUIElement, object.titleUIElement());
    setProperty(AXPropertyName::PlaceholderValue, object.placeholderValue().isolatedCopy());
    setProperty(AXPropertyName::ExpandedTextValue, object.expandedTextValue().isolatedCopy());
    setProperty(AXPropertyName::SupportsExpandedTextValue, object.supportsExpandedTextValue());
    setProperty(AXPropertyName::ValueAutofillButtonType, static_cast<int>(object.valueAutofillButtonType()));
    setProperty(AXPropertyName::URL, object.url().isolatedCopy());
    setProperty(AXPropertyName::AccessKey, object.accessKey().isolatedCopy());
    setProperty(AXPropertyName::ReadOnlyValue, object.readOnlyValue().isolatedCopy());
    setProperty(AXPropertyName::AutoCompleteValue, object.autoCompleteValue().isolatedCopy());
    setProperty(AXPropertyName::ColorValue, object.colorValue());
    setProperty(AXPropertyName::Orientation, static_cast<int>(object.orientation()));
    setProperty(AXPropertyName::HierarchicalLevel, object.hierarchicalLevel());
    setProperty(AXPropertyName::Language, object.language().isolatedCopy());
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

    RefPtr geometryManager = tree()->geometryManager();
    std::optional frame = geometryManager ? geometryManager->cachedRectForID(object.objectID()) : std::nullopt;
    if (frame)
        setProperty(AXPropertyName::RelativeFrame, WTFMove(*frame));
    else if (object.isScrollView() || object.isWebArea() || object.isScrollbar()) {
        // The GeometryManager does not have a relative frame for ScrollViews, WebAreas, or scrollbars yet. We need to get it from the
        // live object so that we don't need to hit the main thread in the case a request comes in while the whole isolated tree is being built.
        setProperty(AXPropertyName::RelativeFrame, enclosingIntRect(object.relativeFrame()));
    } else if (!object.renderer() && object.node() && is<AccessibilityNodeObject>(object)) {
        // The frame of node-only AX objects is made up of their children.
        m_getsGeometryFromChildren = true;
    } else if (object.isMenuListPopup()) {
        // AccessibilityMenuListPopup's elementRect is hardcoded to return an empty rect, so preserve that behavior.
        setProperty(AXPropertyName::RelativeFrame, IntRect());
    }

    if (object.supportsCheckedState()) {
        setProperty(AXPropertyName::SupportsCheckedState, true);
        setProperty(AXPropertyName::IsChecked, object.isChecked());
        setProperty(AXPropertyName::ButtonState, object.checkboxOrRadioValue());
    }

    if (object.isTable()) {
        setProperty(AXPropertyName::IsTable, true);
        setProperty(AXPropertyName::IsExposable, object.isExposable());
        setProperty(AXPropertyName::SupportsSelectedRows, object.supportsSelectedRows());
        setObjectVectorProperty(AXPropertyName::Columns, object.columns());
        setObjectVectorProperty(AXPropertyName::Rows, object.rows());
        setObjectVectorProperty(AXPropertyName::Cells, object.cells());
        setObjectVectorProperty(AXPropertyName::VisibleRows, object.visibleRows());
        setObjectProperty(AXPropertyName::HeaderContainer, object.headerContainer());
        setProperty(AXPropertyName::AXColumnCount, object.axColumnCount());
        setProperty(AXPropertyName::AXRowCount, object.axRowCount());
        setProperty(AXPropertyName::CellSlots, object.cellSlots());
    }

    if (object.isExposedTableCell()) {
        setProperty(AXPropertyName::IsExposedTableCell, true);
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

    if (object.isRadioButton()) {
        if (auto nameAttribute = object.attributeValue("name"_s))
            setProperty(AXPropertyName::NameAttribute, nameAttribute->isolatedCopy());
    }

    if (object.canHaveSelectedChildren())
        setObjectVectorProperty(AXPropertyName::SelectedChildren, object.selectedChildren());

    if (object.canHaveSelectedCells())
        setObjectVectorProperty(AXPropertyName::SelectedCells, object.selectedCells());

    if (object.isImage())
        setProperty(AXPropertyName::EmbeddedImageDescription, object.embeddedImageDescription().isolatedCopy());

    // On macOS, we only advertise support for the visible children attribute for listboxes.
    if (object.isListBox())
        setObjectVectorProperty(AXPropertyName::VisibleChildren, object.visibleChildren());

    setObjectVectorProperty(AXPropertyName::LinkedObjects, object.linkedObjects());

    if (object.isSpinButton()) {
        // FIXME: These properties get out of date every time AccessibilitySpinButton::{clearChildren, addChildren} is called. We should probably just not cache these properties.
        setObjectProperty(AXPropertyName::DecrementButton, object.decrementButton());
        setObjectProperty(AXPropertyName::IncrementButton, object.incrementButton());
    }

    if (object.isMathElement()) {
        setProperty(AXPropertyName::IsMathElement, true);
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

    if (object.isScrollView() || object.isWebArea()) {
        // For the ScrollView and WebArea objects, cache the TitleAttributeValue and DescriptionAttributeValue eagerly to avoid hitting the main thread in getOrRetrievePropertyValue during the construction of the isolated tree.
        setProperty(AXPropertyName::TitleAttributeValue, object.titleAttributeValue().isolatedCopy());
        setProperty(AXPropertyName::DescriptionAttributeValue, object.descriptionAttributeValue().isolatedCopy());
        setProperty(AXPropertyName::HelpText, object.helpTextAttributeValue().isolatedCopy());
    }

    if (isRoot == IsRoot::Yes) {
        setProperty(AXPropertyName::SessionID, object.sessionID().isolatedCopy());
        setProperty(AXPropertyName::DocumentURI, object.documentURI().isolatedCopy());
        setProperty(AXPropertyName::DocumentEncoding, object.documentEncoding().isolatedCopy());
    }

    if (object.isScrollView()) {
        setObjectProperty(AXPropertyName::VerticalScrollBar, object.scrollBar(AccessibilityOrientation::Vertical));
        setObjectProperty(AXPropertyName::HorizontalScrollBar, object.scrollBar(AccessibilityOrientation::Horizontal));
    } else if (object.isWebArea()) {
        // We only expose document links in web area objects.
        setObjectVectorProperty(AXPropertyName::DocumentLinks, object.documentLinks());
    }

    if (object.isWidget())
        setProperty(AXPropertyName::IsWidget, true);

    auto descriptor = object.title();
    if (descriptor.length())
        setProperty(AXPropertyName::Title, descriptor.isolatedCopy());

    descriptor = object.description();
    if (descriptor.length())
        setProperty(AXPropertyName::Description, descriptor.isolatedCopy());

    descriptor = object.extendedDescription();
    if (descriptor.length())
        setProperty(AXPropertyName::ExtendedDescription, descriptor.isolatedCopy());

    // These properties are only needed on the AXCoreObject interface due to their use in ATSPI,
    // so only cache them for ATSPI.
#if PLATFORM(ATSPI)
    setProperty(AXPropertyName::ActionVerb, object.actionVerb().isolatedCopy());
    setProperty(AXPropertyName::IsFieldset, object.isFieldset());
    setProperty(AXPropertyName::IsSelectedOptionActive, object.isSelectedOptionActive());
    setProperty(AXPropertyName::IsVisible, object.isVisible());
    setProperty(AXPropertyName::LocalizedActionVerb, object.localizedActionVerb().isolatedCopy());
#endif

    initializePlatformProperties(axObject);
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
        if (RefPtr child = tree()->objectForID(childID))
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
            if (RefPtr child = tree()->objectForID(childID))
                m_children.uncheckedAppend(child);
        }
        ASSERT(m_children.size() == m_childrenIDs.size());
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

    auto childrenIDs = axIDs(selectedChildren);
    performFunctionOnMainThread([selectedChildrenIDs = WTFMove(childrenIDs), protectedThis = Ref { *this }] (auto* object) {
        if (selectedChildrenIDs.isEmpty()) {
            object->setSelectedChildren({ });
            return;
        }

        auto* axObjectCache = protectedThis->axObjectCache();
        if (!axObjectCache)
            return;

        object->setSelectedChildren(axObjectCache->objectsForIDs(selectedChildrenIDs));
    });
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
    // AXPropertyName::CellSlots can be big, so make sure not to copy it.
    auto cellSlotsIterator = m_propertyMap.find(AXPropertyName::CellSlots);
    if (cellSlotsIterator == m_propertyMap.end())
        return nullptr;

    AXID cellID = WTF::switchOn(cellSlotsIterator->value,
        [&] (Vector<Vector<AXID>>& cellSlots) {
            if (rowIndex >= cellSlots.size() || columnIndex >= cellSlots[rowIndex].size())
                return AXID();
            return cellSlots[rowIndex][columnIndex];
        },
        [] (auto&) { return AXID(); }
    );
    return cellID ? tree()->objectForID(cellID).get() : nullptr;
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
        if (RefPtr coreObject = tree()->objectForID(pair.first))
            prescriptPair.first = coreObject.get();
        if (RefPtr coreObject = tree()->objectForID(pair.second))
            prescriptPair.second = coreObject.get();
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
    
AXIsolatedObject* AXIsolatedObject::parentObjectUnignored() const
{
    return tree()->objectForID(parent()).get();
}

AXCoreObject* AXIsolatedObject::scrollBar(AccessibilityOrientation orientation)
{
    return objectAttributeValue(orientation == AccessibilityOrientation::Vertical ? AXPropertyName::VerticalScrollBar : AXPropertyName::HorizontalScrollBar);
}

void AXIsolatedObject::setARIAGrabbed(bool value)
{
    performFunctionOnMainThread([value] (auto* object) {
        object->setARIAGrabbed(value);
    });
}

void AXIsolatedObject::setIsExpanded(bool value)
{
    performFunctionOnMainThread([value] (auto* object) {
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

void AXIsolatedObject::performDismissActionIgnoringResult()
{
    performFunctionOnMainThread([] (auto* axObject) {
        axObject->performDismissActionIgnoringResult();
    });
}

void AXIsolatedObject::scrollToMakeVisible() const
{
    performFunctionOnMainThread([] (auto* axObject) {
        axObject->scrollToMakeVisible();
    });
}

void AXIsolatedObject::scrollToMakeVisibleWithSubFocus(IntRect&& rect) const
{
    performFunctionOnMainThread([rect = WTFMove(rect)] (auto* axObject) mutable {
        axObject->scrollToMakeVisibleWithSubFocus(WTFMove(rect));
    });
}

void AXIsolatedObject::scrollToGlobalPoint(IntPoint&& point) const
{
    performFunctionOnMainThread([point = WTFMove(point)] (auto* axObject) mutable {
        axObject->scrollToGlobalPoint(WTFMove(point));
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

void AXIsolatedObject::setValueIgnoringResult(float value)
{
    performFunctionOnMainThread([value] (auto* object) {
        object->setValueIgnoringResult(value);
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

void AXIsolatedObject::setValueIgnoringResult(const String& value)
{
    performFunctionOnMainThread([value = value.isolatedCopy()] (auto* object) {
        object->setValueIgnoringResult(value);
    });
}

void AXIsolatedObject::setSelected(bool value)
{
    performFunctionOnMainThread([value] (auto* object) {
        object->setSelected(value);
    });
}

void AXIsolatedObject::setSelectedRows(AccessibilityChildrenVector&& selectedRows)
{
    auto rowIDs = axIDs(selectedRows);
    performFunctionOnMainThread([selectedRowIDs = WTFMove(rowIDs), protectedThis = Ref { *this }] (auto* object) {
        if (selectedRowIDs.isEmpty()) {
            object->setSelectedRows({ });
            return;
        }

        auto* axObjectCache = protectedThis->axObjectCache();
        if (!axObjectCache)
            return;

        object->setSelectedRows(axObjectCache->objectsForIDs(selectedRowIDs));
    });
}

void AXIsolatedObject::setFocused(bool value)
{
    performFunctionOnMainThread([value] (auto* object) {
        object->setFocused(value);
    });
}

String AXIsolatedObject::selectedText() const
{
    return Accessibility::retrieveValueFromMainThread<String>([this] () -> String {
        if (auto* object = associatedAXObject())
            return object->selectedText().isolatedCopy();
        return { };
    });
}

void AXIsolatedObject::setSelectedText(const String& value)
{
    performFunctionOnMainThread([value = value.isolatedCopy()] (auto* object) {
        object->setSelectedText(value);
    });
}

void AXIsolatedObject::setSelectedTextRange(CharacterRange&& range)
{
    performFunctionOnMainThread([range = WTFMove(range)] (auto* object) mutable {
        object->setSelectedTextRange(WTFMove(range));
    });
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

    return tree()->objectForID(axID).get();
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

    return tree()->objectForID(nodeID).get();
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

template<typename T>
std::optional<T> AXIsolatedObject::optionalAttributeValue(AXPropertyName propertyName) const
{
    auto it = m_propertyMap.find(propertyName);
    if (it == m_propertyMap.end())
        return std::nullopt;

    return WTF::switchOn(it->value,
        [] (const T& typedValue) -> std::optional<T> { return typedValue; },
        [] (const auto&) -> std::optional<T> {
            ASSERT_NOT_REACHED();
            return std::nullopt;
        }
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

    Accessibility::performFunctionOnMainThreadAndWait([&propertyName, this] () {
        auto* axObject = associatedAXObject();
        if (!axObject)
            return;

        AXPropertyValueVariant value;
        switch (propertyName) {
#if PLATFORM(COCOA)
        case AXPropertyName::DescriptionAttributeValue:
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
        if (RefPtr object = tree()->objectForID(childID))
            children.uncheckedAppend(object);
    }
}

void AXIsolatedObject::updateBackingStore()
{
    // This method can be called on either the main or the AX threads.
    if (RefPtr tree = this->tree())
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

std::optional<SimpleRange> AXIsolatedObject::rangeForCharacterRange(const CharacterRange& axRange) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->rangeForCharacterRange(axRange) : std::nullopt;
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

LayoutRect AXIsolatedObject::elementRect() const
{
#if PLATFORM(MAC)
    // It is not expected for elementRect to be called directly or indirectly when serving a request for VoiceOver.
    // If this does happen, we should either see if AXIsolatedObject::relativeFrame can be used instead, or do the
    // work to cache the correct elementRect value.
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);
#endif

    return Accessibility::retrieveValueFromMainThread<LayoutRect>([&, this] () -> LayoutRect {
        if (auto* axObject = associatedAXObject())
            return axObject->elementRect();
        return { };
    });
}

FloatPoint AXIsolatedObject::screenRelativePosition() const
{
    if (auto point = optionalAttributeValue<FloatPoint>(AXPropertyName::ScreenRelativePosition))
        return *point;

    return Accessibility::retrieveValueFromMainThread<FloatPoint>([&, this] () -> FloatPoint {
        if (auto* axObject = associatedAXObject())
            return axObject->screenRelativePosition();
        return { };
    });
}

FloatRect AXIsolatedObject::relativeFrame() const
{
    if (auto cachedRelativeFrame = optionalAttributeValue<IntRect>(AXPropertyName::RelativeFrame)) {
        // We should not have cached a relative frame for elements that get their geometry from their children.
        ASSERT(!m_getsGeometryFromChildren);
        return *cachedRelativeFrame;
    }

    if (m_getsGeometryFromChildren) {
        auto frame = enclosingIntRect(relativeFrameFromChildren());
        if (!frame.isEmpty())
            return frame;
        // Either we had no children, or our children had empty frames. The right thing to do would be to return
        // a rect at the position of the nearest render tree ancestor with some made-up size (AccessibilityNodeObject::boundingBoxRect does this).
        // However, we don't have access to the render tree in this context (only the AX isolated tree, which is too sparse for this purpose), so
        // until we cache the necessary information let's go to the main-thread.
    } else if (roleValue() == AccessibilityRole::Column || roleValue() == AccessibilityRole::TableHeaderContainer)
        return exposedTableAncestor() ? relativeFrameFromChildren() : FloatRect();

    return Accessibility::retrieveValueFromMainThread<FloatRect>([this] () -> FloatRect {
        if (auto* axObject = associatedAXObject())
            return axObject->relativeFrame();
        return { };
    });
}

FloatRect AXIsolatedObject::relativeFrameFromChildren() const
{
    FloatRect rect;
    for (const auto& child : const_cast<AXIsolatedObject*>(this)->children())
        rect.unite(child->relativeFrame());
    return rect;
}

FloatRect AXIsolatedObject::convertFrameToSpace(const FloatRect& rect, AccessibilityConversionSpace space) const
{
    return Accessibility::retrieveValueFromMainThread<FloatRect>([&rect, &space, this] () -> FloatRect {
        if (auto* axObject = associatedAXObject())
            return axObject->convertFrameToSpace(rect, space);
        return { };
    });
}

bool AXIsolatedObject::replaceTextInRange(const String& replacementText, const CharacterRange& textRange)
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

bool AXIsolatedObject::press()
{
    if (auto* object = associatedAXObject())
        return object->press();
    return false;
}

void AXIsolatedObject::increment()
{
    performFunctionOnMainThread([] (auto* axObject) {
        axObject->increment();
    });
}

void AXIsolatedObject::decrement()
{
    performFunctionOnMainThread([] (auto* axObject) {
        axObject->decrement();
    });
}

AtomString AXIsolatedObject::tagName() const
{
    ASSERT_NOT_REACHED();
    return AtomString();
}

bool AXIsolatedObject::isAccessibilityRenderObject() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isAccessibilityTableInstance() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isNativeTextControl() const
{
    ASSERT_NOT_REACHED();
    return false;
}

CharacterRange AXIsolatedObject::selectedTextRange() const
{
    if (shouldReturnEmptySelectedText())
        return { };

    return Accessibility::retrieveValueFromMainThread<CharacterRange>([this] () -> CharacterRange {
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

CharacterRange AXIsolatedObject::doAXRangeForLine(unsigned lineIndex) const
{
    return Accessibility::retrieveValueFromMainThread<CharacterRange>([&lineIndex, this] () -> CharacterRange {
        if (auto* object = associatedAXObject())
            return object->doAXRangeForLine(lineIndex);
        return { };
    });
}

String AXIsolatedObject::doAXStringForRange(const CharacterRange& axRange) const
{
    return Accessibility::retrieveValueFromMainThread<String>([&axRange, this] () -> String {
        if (auto* object = associatedAXObject())
            return object->doAXStringForRange(axRange).isolatedCopy();
        return { };
    });
}

CharacterRange AXIsolatedObject::characterRangeForPoint(const IntPoint& point) const
{
    return Accessibility::retrieveValueFromMainThread<CharacterRange>([&point, this] () -> CharacterRange {
        if (auto* object = associatedAXObject())
            return object->characterRangeForPoint(point);
        return { };
    });
}

CharacterRange AXIsolatedObject::doAXRangeForIndex(unsigned index) const
{
    return Accessibility::retrieveValueFromMainThread<CharacterRange>([&index, this] () -> CharacterRange {
        if (auto* object = associatedAXObject())
            return object->doAXRangeForIndex(index);
        return { };
    });
}

CharacterRange AXIsolatedObject::doAXStyleRangeForIndex(unsigned index) const
{
    return Accessibility::retrieveValueFromMainThread<CharacterRange>([&index, this] () -> CharacterRange {
        if (auto* object = associatedAXObject())
            return object->doAXStyleRangeForIndex(index);
        return { };
    });
}

IntRect AXIsolatedObject::doAXBoundsForRange(const CharacterRange& axRange) const
{
    return Accessibility::retrieveValueFromMainThread<IntRect>([&axRange, this] () -> IntRect {
        if (auto* object = associatedAXObject())
            return object->doAXBoundsForRange(axRange);
        return { };
    });
}

IntRect AXIsolatedObject::doAXBoundsForRangeUsingCharacterOffset(const CharacterRange& axRange) const
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

std::optional<SimpleRange> AXIsolatedObject::simpleRange() const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->simpleRange() : std::nullopt;
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

VisiblePositionRange AXIsolatedObject::visiblePositionRangeForRange(const CharacterRange& plainTextRange) const
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

bool AXIsolatedObject::isMockObject() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isNonNativeTextControl() const
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
    return Accessibility::retrieveValueFromMainThread<bool>([this] () {
        auto* axObject = associatedAXObject();
        return axObject ? axObject->hasMisspelling() : false;
    });
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

AXTextMarkerRange AXIsolatedObject::textInputMarkedTextMarkerRange() const
{
    auto value = optionalAttributeValue<std::pair<AXID, CharacterRange>>(AXPropertyName::TextInputMarkedTextMarkerRange);
    if (!value)
        return { };

    auto start = static_cast<unsigned>(value->second.location);
    auto end = start + static_cast<unsigned>(value->second.length);
    return { tree()->treeID(), value->first, start, end };
}

// The attribute this value is exposed as is not used by VoiceOver or any other AX client on macOS, so we intentionally don't cache it.
// Re-visit if ITM expands to more platforms, or if AX clients need to start using this.
String AXIsolatedObject::linkRelValue() const
{
    return Accessibility::retrieveValueFromMainThread<String>([this] () -> String {
        if (auto* object = associatedAXObject())
            return object->linkRelValue().isolatedCopy();
        return { };
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

bool AXIsolatedObject::supportsHasPopup() const
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

bool AXIsolatedObject::isTableCell() const
{
    ASSERT_NOT_REACHED();
    return false;
}

AXCoreObject* AXIsolatedObject::elementAccessibilityHitTest(const IntPoint&) const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

bool AXIsolatedObject::isDescendantOfRole(AccessibilityRole) const
{
    ASSERT_NOT_REACHED();
    return false;
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

String AXIsolatedObject::stringValue() const
{
    if (m_propertyMap.contains(AXPropertyName::StringValue))
        return stringAttributeValue(AXPropertyName::StringValue);
    if (auto value = platformStringValue())
        return *value;
    return { };
}

String AXIsolatedObject::text() const
{
    ASSERT_NOT_REACHED();
    return String();
}

#if !PLATFORM(COCOA)
unsigned AXIsolatedObject::textLength() const
{
    ASSERT_NOT_REACHED();
    return 0;
}
#endif

AXObjectCache* AXIsolatedObject::axObjectCache() const
{
    ASSERT(isMainThread());
    return tree()->axObjectCache();
}

Element* AXIsolatedObject::actionElement() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
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

LocalFrameView* AXIsolatedObject::documentFrameView() const
{
    ASSERT(isMainThread());

    if (auto* axObject = associatedAXObject())
        return axObject->documentFrameView();

    ASSERT_NOT_REACHED();
    return nullptr;
}

ScrollView* AXIsolatedObject::scrollView() const
{
    if (auto* object = associatedAXObject())
        return object->scrollView();
    return nullptr;
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

String AXIsolatedObject::computedRoleString() const
{
    ASSERT_NOT_REACHED();
    return { };
}
#endif

} // namespace WebCore

#endif // ENABLE((ACCESSIBILITY_ISOLATED_TREE)
