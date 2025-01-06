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
#include "AXSearchManager.h"
#include "AXTextRun.h"
#include "AccessibilityNodeObject.h"
#include "DateComponents.h"
#include "HTMLNames.h"
#include "RenderObject.h"
#include <wtf/text/MakeString.h>

#if PLATFORM(MAC)
#import <pal/spi/mac/HIServicesSPI.h>
#endif

#if PLATFORM(COCOA)
#include <pal/spi/cocoa/AccessibilitySupportSoftLink.h>
#endif

namespace WebCore {

using namespace HTMLNames;

AXIsolatedObject::AXIsolatedObject(const Ref<AccessibilityObject>& axObject, AXIsolatedTree* tree)
    : AXCoreObject(axObject->objectID())
    , m_cachedTree(tree)
{
    ASSERT(isMainThread());

    if (auto* axParent = axObject->parentInCoreTree())
        m_parentID = axParent->objectID();
    m_role = axObject->roleValue();

    initializeProperties(axObject);
}

Ref<AXIsolatedObject> AXIsolatedObject::create(const Ref<AccessibilityObject>& object, AXIsolatedTree* tree)
{
    return adoptRef(*new AXIsolatedObject(object, tree));
}

AXIsolatedObject::~AXIsolatedObject()
{
    ASSERT(!wrapper());
}

String AXIsolatedObject::dbgInternal(bool verbose, OptionSet<AXDebugStringOption> debugOptions) const
{
    StringBuilder result;
    result.append("{"_s);
    result.append("role: "_s, accessibilityRoleToString(roleValue()));
    result.append(", ID "_s, objectID().loggingString());

    if (verbose || debugOptions & AXDebugStringOption::Ignored)
        result.append(isIgnored() ? ", ignored"_s : emptyString());

    if (verbose || debugOptions & AXDebugStringOption::RelativeFrame) {
        FloatRect frame = relativeFrame();
        result.append(", relativeFrame ((x: "_s, frame.x(), ", y: "_s, frame.y(), "), (w: "_s, frame.width(), ", h: "_s, frame.height(), "))"_s);
    }

    if (verbose || debugOptions & AXDebugStringOption::RemoteFrameOffset)
        result.append(", remoteFrameOffset ("_s, remoteFrameOffset().x(), ", "_s, remoteFrameOffset().y(), ")"_s);

    result.append("}"_s);
    return result.toString();
}

void AXIsolatedObject::initializeProperties(const Ref<AccessibilityObject>& axObject)
{
    AXTRACE("AXIsolatedObject::initializeProperties"_s);
    auto& object = axObject.get();

    auto reserveCapacityAndCacheBaseProperties = [&] (unsigned sizeToReserve) {
        if (sizeToReserve)
            m_propertyMap.reserveInitialCapacity(sizeToReserve);

        // These properties are cached for all objects, ignored and unignored.
        setProperty(AXProperty::HasClickHandler, object.hasClickHandler());
        auto tag = object.tagName();
        if (tag == bodyTag)
            setProperty(AXProperty::HasBodyTag, true);
#if ENABLE(AX_THREAD_TEXT_APIS)
        else if (tag == markTag)
            setProperty(AXProperty::HasMarkTag, true);
#endif // ENABLE(AX_THREAD_TEXT_APIS)
    };

    // Allocate a capacity based on the minimum properties an object has (based on measurements from a real webpage).
    constexpr unsigned unignoredSizeToReserve = 11;
#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    if (object.includeIgnoredInCoreTree()) {
        bool isIgnored = object.isIgnored();
        setProperty(AXProperty::IsIgnored, isIgnored);
        // Maintain full properties for objects meeting this criteria:
        //   - Unconnected objects, which are involved in relations or outgoing notifications
        //   - Static text. We sometimes ignore static text (e.g. because it descends from a text field),
        //     but need full properties for proper text marker behavior.
        // FIXME: We shouldn't cache all properties for empty / non-rendered text?
        bool needsAllProperties = !isIgnored || tree()->isUnconnectedNode(axObject->objectID()) || is<RenderText>(axObject->renderer());
        if (!needsAllProperties) {
            // FIXME: If isIgnored, we should only cache a small subset of necessary properties, e.g. those used in the text marker APIs.
            reserveCapacityAndCacheBaseProperties(0);
            return;
        }
        reserveCapacityAndCacheBaseProperties(unignoredSizeToReserve);
    }
#else
    reserveCapacityAndCacheBaseProperties(unignoredSizeToReserve);
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)

    if (object.ancestorFlagsAreInitialized())
        setProperty(AXProperty::AncestorFlags, object.ancestorFlags());
    else
        setProperty(AXProperty::AncestorFlags, object.computeAncestorFlagsWithTraversal());

    setProperty(AXProperty::IsAttachment, object.isAttachment());
    setProperty(AXProperty::IsBusy, object.isBusy());
    setProperty(AXProperty::IsEnabled, object.isEnabled());
    setProperty(AXProperty::IsExpanded, object.isExpanded());
    setProperty(AXProperty::IsFileUploadButton, object.isFileUploadButton());
    setProperty(AXProperty::IsIndeterminate, object.isIndeterminate());
    setProperty(AXProperty::IsInlineText, object.isInlineText());
    setProperty(AXProperty::IsInputImage, object.isInputImage());
    setProperty(AXProperty::IsMultiSelectable, object.isMultiSelectable());
    setProperty(AXProperty::IsRequired, object.isRequired());
    setProperty(AXProperty::IsSecureField, object.isSecureField());
    setProperty(AXProperty::IsSelected, object.isSelected());
    setProperty(AXProperty::InsideLink, object.insideLink());
    setProperty(AXProperty::IsValueAutofillAvailable, object.isValueAutofillAvailable());
    setProperty(AXProperty::RoleDescription, object.roleDescription().isolatedCopy());
    setProperty(AXProperty::RolePlatformString, object.rolePlatformString().isolatedCopy());
    setProperty(AXProperty::SubrolePlatformString, object.subrolePlatformString().isolatedCopy());
    setProperty(AXProperty::CanSetFocusAttribute, object.canSetFocusAttribute());
    setProperty(AXProperty::CanSetValueAttribute, object.canSetValueAttribute());
    setProperty(AXProperty::CanSetSelectedAttribute, object.canSetSelectedAttribute());
    setProperty(AXProperty::BlockquoteLevel, object.blockquoteLevel());
    setProperty(AXProperty::HeadingLevel, object.headingLevel());
    setProperty(AXProperty::ValueDescription, object.valueDescription().isolatedCopy());
    setProperty(AXProperty::ValueForRange, object.valueForRange());
    setProperty(AXProperty::MaxValueForRange, object.maxValueForRange());
    setProperty(AXProperty::MinValueForRange, object.minValueForRange());
    setProperty(AXProperty::SupportsARIAOwns, object.supportsARIAOwns());
    setProperty(AXProperty::PopupValue, object.popupValue().isolatedCopy());
    setProperty(AXProperty::InvalidStatus, object.invalidStatus().isolatedCopy());
    setProperty(AXProperty::SupportsExpanded, object.supportsExpanded());
    setProperty(AXProperty::SortDirection, static_cast<int>(object.sortDirection()));
    setProperty(AXProperty::SupportsRangeValue, object.supportsRangeValue());
#if !LOG_DISABLED
    // Eagerly cache ID when logging is enabled so that we can log isolated objects without constant deadlocks.
    // Don't cache ID when logging is disabled because we don't expect non-test AX clients to actually request it.
    setProperty(AXProperty::IdentifierAttribute, object.identifierAttribute().isolatedCopy());
#endif
    // FIXME: We never update AXProperty::SupportsDropping.
    setProperty(AXProperty::SupportsDropping, object.supportsDropping());
    setProperty(AXProperty::SupportsDragging, object.supportsDragging());
    setProperty(AXProperty::IsGrabbed, object.isGrabbed());
    setProperty(AXProperty::PlaceholderValue, object.placeholderValue().isolatedCopy());
    setProperty(AXProperty::ValueAutofillButtonType, static_cast<int>(object.valueAutofillButtonType()));
    setProperty(AXProperty::URL, std::make_shared<URL>(object.url().isolatedCopy()));
    setProperty(AXProperty::AccessKey, object.accessKey().isolatedCopy());
    setProperty(AXProperty::AutoCompleteValue, object.autoCompleteValue().isolatedCopy());
    setProperty(AXProperty::ColorValue, object.colorValue());
    setProperty(AXProperty::Orientation, static_cast<int>(object.orientation()));
    setProperty(AXProperty::HierarchicalLevel, object.hierarchicalLevel());
    setProperty(AXProperty::Language, object.language().isolatedCopy());
    setProperty(AXProperty::LiveRegionStatus, object.liveRegionStatus().isolatedCopy());
    setProperty(AXProperty::LiveRegionRelevant, object.liveRegionRelevant().isolatedCopy());
    setProperty(AXProperty::LiveRegionAtomic, object.liveRegionAtomic());
    setProperty(AXProperty::HasHighlighting, object.hasHighlighting());
    setProperty(AXProperty::HasBoldFont, object.hasBoldFont());
    setProperty(AXProperty::HasItalicFont, object.hasItalicFont());
    setProperty(AXProperty::HasPlainText, object.hasPlainText());
#if !ENABLE(AX_THREAD_TEXT_APIS)
    setProperty(AXProperty::HasUnderline, object.hasUnderline());
#endif
    setProperty(AXProperty::IsKeyboardFocusable, object.isKeyboardFocusable());
    setProperty(AXProperty::BrailleRoleDescription, object.brailleRoleDescription().isolatedCopy());
    setProperty(AXProperty::BrailleLabel, object.brailleLabel().isolatedCopy());
    setProperty(AXProperty::IsNonLayerSVGObject, object.isNonLayerSVGObject());

    bool isWebArea = axObject->isWebArea();
    bool isScrollArea = axObject->isScrollView();
    if (isScrollArea && !axObject->parentObject()) {
        // Eagerly cache the screen relative position for the root. AXIsolatedObject::screenRelativePosition()
        // of non-root objects depend on the root object's screen relative position, so make sure it's there
        // from the start. We keep this up-to-date via AXIsolatedTree::updateRootScreenRelativePosition().
        setProperty(AXProperty::ScreenRelativePosition, axObject->screenRelativePosition());
        // FIXME: We never update this property, e.g. when the iframe is moved in the hosting web content process.
        setProperty(AXProperty::RemoteFrameOffset, object.remoteFrameOffset());
    }

    RefPtr geometryManager = tree()->geometryManager();
    std::optional frame = geometryManager ? geometryManager->cachedRectForID(object.objectID()) : std::nullopt;
    if (frame)
        setProperty(AXProperty::RelativeFrame, WTFMove(*frame));
    else if (isScrollArea || isWebArea || object.isScrollbar()) {
        // The GeometryManager does not have a relative frame for ScrollViews, WebAreas, or scrollbars yet. We need to get it from the
        // live object so that we don't need to hit the main thread in the case a request comes in while the whole isolated tree is being built.
        setProperty(AXProperty::RelativeFrame, enclosingIntRect(object.relativeFrame()));
    } else if (!object.renderer() && object.node() && is<AccessibilityNodeObject>(object)) {
        // The frame of node-only AX objects is made up of their children.
        m_getsGeometryFromChildren = true;
    } else if (object.isMenuListPopup()) {
        // AccessibilityMenuListPopup's elementRect is hardcoded to return an empty rect, so preserve that behavior.
        setProperty(AXProperty::RelativeFrame, IntRect());
    } else
        setProperty(AXProperty::InitialFrameRect, object.frameRect());

    if (object.supportsPath()) {
        setProperty(AXProperty::SupportsPath, true);
        setProperty(AXProperty::Path, std::make_shared<Path>(object.elementPath()));
    }

    if (object.supportsKeyShortcuts()) {
        setProperty(AXProperty::SupportsKeyShortcuts, true);
        setProperty(AXProperty::KeyShortcuts, object.keyShortcuts().isolatedCopy());
    }

    if (object.supportsCurrent()) {
        setProperty(AXProperty::SupportsCurrent, true);
        setProperty(AXProperty::CurrentState, static_cast<int>(object.currentState()));
    }

    if (object.supportsSetSize()) {
        setProperty(AXProperty::SupportsSetSize, true);
        setProperty(AXProperty::SetSize, object.setSize());
    }

    if (object.supportsPosInSet()) {
        setProperty(AXProperty::SupportsPosInSet, true);
        setProperty(AXProperty::PosInSet, object.posInSet());
    }

    if (object.supportsExpandedTextValue()) {
        setProperty(AXProperty::SupportsExpandedTextValue, true);
        setProperty(AXProperty::ExpandedTextValue, object.expandedTextValue().isolatedCopy());
    }

    if (object.supportsDatetimeAttribute()) {
        setProperty(AXProperty::SupportsDatetimeAttribute, true);
        setProperty(AXProperty::DatetimeAttributeValue, object.datetimeAttributeValue().isolatedCopy());
    }

    if (object.supportsCheckedState()) {
        setProperty(AXProperty::SupportsCheckedState, true);
        setProperty(AXProperty::IsChecked, object.isChecked());
        setProperty(AXProperty::ButtonState, object.checkboxOrRadioValue());
    }

    if (object.isTable()) {
        setProperty(AXProperty::IsTable, true);
        setProperty(AXProperty::IsExposable, object.isExposable());
        setObjectVectorProperty(AXProperty::Columns, object.columns());
        setObjectVectorProperty(AXProperty::Rows, object.rows());
        setObjectVectorProperty(AXProperty::Cells, object.cells());
        setObjectVectorProperty(AXProperty::VisibleRows, object.visibleRows());
        setProperty(AXProperty::AXColumnCount, object.axColumnCount());
        setProperty(AXProperty::AXRowCount, object.axRowCount());
        setProperty(AXProperty::CellSlots, object.cellSlots());
    }

    if (object.isExposedTableCell()) {
        setProperty(AXProperty::IsExposedTableCell, true);
        setProperty(AXProperty::ColumnIndexRange, object.columnIndexRange());
        setProperty(AXProperty::RowIndexRange, object.rowIndexRange());
        setProperty(AXProperty::AXColumnIndex, object.axColumnIndex());
        setProperty(AXProperty::AXRowIndex, object.axRowIndex());
        setProperty(AXProperty::IsColumnHeader, object.isColumnHeader());
        setProperty(AXProperty::IsRowHeader, object.isRowHeader());
        setProperty(AXProperty::CellScope, object.cellScope().isolatedCopy());
        setProperty(AXProperty::RowGroupAncestorID, object.rowGroupAncestorID());
    }

    if (object.isTableColumn())
        setProperty(AXProperty::ColumnIndex, object.columnIndex());
    else if (object.isTableRow()) {
        setProperty(AXProperty::IsTableRow, true);
        setProperty(AXProperty::RowIndex, object.rowIndex());
    }

    if (object.isARIATreeGridRow()) {
        setProperty(AXProperty::IsARIATreeGridRow, true);
        setObjectVectorProperty(AXProperty::DisclosedRows, object.disclosedRows());
        setObjectProperty(AXProperty::DisclosedByRow, object.disclosedByRow());
    }

    if (object.isARIATreeGridRow() || object.isTableRow())
        setObjectProperty(AXProperty::RowHeader, object.rowHeader());

    if (object.isTreeItem()) {
        setProperty(AXProperty::IsTreeItem, true);
        setObjectVectorProperty(AXProperty::DisclosedRows, object.disclosedRows());
    }

    setProperty(AXProperty::IsTree, object.isTree());
    if (object.isRadioButton()) {
        setProperty(AXProperty::NameAttribute, object.nameAttribute().isolatedCopy());
        // FIXME: This property doesn't get updated when a page changes dynamically.
        setObjectVectorProperty(AXProperty::RadioButtonGroup, object.radioButtonGroup());
        setProperty(AXProperty::IsRadioInput, object.isRadioInput());
    }

    if (object.isImage())
        setProperty(AXProperty::EmbeddedImageDescription, object.embeddedImageDescription().isolatedCopy());

    // On macOS, we only advertise support for the visible children attribute for lists and listboxes.
    if (object.isList() || object.isListBox())
        setObjectVectorProperty(AXProperty::VisibleChildren, object.visibleChildren());

    if (object.isDateTime()) {
        setProperty(AXProperty::DateTimeValue, object.dateTimeValue().isolatedCopy());
        setProperty(AXProperty::DateTimeComponentsType, object.dateTimeComponentsType());
    }

    if (object.isSpinButton()) {
        setObjectProperty(AXProperty::DecrementButton, object.decrementButton());
        setObjectProperty(AXProperty::IncrementButton, object.incrementButton());
    }

    if (object.isMathElement()) {
        setProperty(AXProperty::IsMathElement, true);
        setProperty(AXProperty::IsMathFraction, object.isMathFraction());
        setProperty(AXProperty::IsMathFenced, object.isMathFenced());
        setProperty(AXProperty::IsMathSubscriptSuperscript, object.isMathSubscriptSuperscript());
        setProperty(AXProperty::IsMathRow, object.isMathRow());
        setProperty(AXProperty::IsMathUnderOver, object.isMathUnderOver());
        setProperty(AXProperty::IsMathTable, object.isMathTable());
        setProperty(AXProperty::IsMathTableRow, object.isMathTableRow());
        setProperty(AXProperty::IsMathTableCell, object.isMathTableCell());
        setProperty(AXProperty::IsMathMultiscript, object.isMathMultiscript());
        setProperty(AXProperty::IsMathToken, object.isMathToken());
        setProperty(AXProperty::MathFencedOpenString, object.mathFencedOpenString().isolatedCopy());
        setProperty(AXProperty::MathFencedCloseString, object.mathFencedCloseString().isolatedCopy());
        setProperty(AXProperty::MathLineThickness, object.mathLineThickness());

        bool isMathRoot = object.isMathRoot();
        setProperty(AXProperty::IsMathRoot, isMathRoot);
        setProperty(AXProperty::IsMathSquareRoot, object.isMathSquareRoot());
        if (isMathRoot) {
            if (auto radicand = object.mathRadicand())
                setObjectVectorProperty(AXProperty::MathRadicand, *radicand);

            setObjectProperty(AXProperty::MathRootIndexObject, object.mathRootIndexObject());
        }

        setObjectProperty(AXProperty::MathUnderObject, object.mathUnderObject());
        setObjectProperty(AXProperty::MathOverObject, object.mathOverObject());
        setObjectProperty(AXProperty::MathNumeratorObject, object.mathNumeratorObject());
        setObjectProperty(AXProperty::MathDenominatorObject, object.mathDenominatorObject());
        setObjectProperty(AXProperty::MathBaseObject, object.mathBaseObject());
        setObjectProperty(AXProperty::MathSubscriptObject, object.mathSubscriptObject());
        setObjectProperty(AXProperty::MathSuperscriptObject, object.mathSuperscriptObject());
        setMathscripts(AXProperty::MathPrescripts, object);
        setMathscripts(AXProperty::MathPostscripts, object);
    }

    Vector<AccessibilityText> texts;
    object.accessibilityText(texts);
    auto axTextValue = texts.map([] (const auto& text) -> AccessibilityText {
        return { text.text.isolatedCopy(), text.textSource };
    });
    setProperty(AXProperty::AccessibilityText, axTextValue);

    if (isScrollArea) {
        setObjectProperty(AXProperty::VerticalScrollBar, object.scrollBar(AccessibilityOrientation::Vertical));
        setObjectProperty(AXProperty::HorizontalScrollBar, object.scrollBar(AccessibilityOrientation::Horizontal));
        setProperty(AXProperty::HasRemoteFrameChild, object.hasRemoteFrameChild());
    } else if (isWebArea && !tree()->isEmptyContentTree()) {
        // We expose DocumentLinks only for the web area objects when the tree is not an empty content tree. This property is expensive and makes no sense in an empty content tree.
        // FIXME: compute DocumentLinks on the AX thread instead of caching it.
        setObjectVectorProperty(AXProperty::DocumentLinks, object.documentLinks());
    }

    if (object.isWidget()) {
        if (object.isPlugin()) {
            // Plugins are a subclass of widget, so we only need to cache IsPlugin, and we implicitly know
            // this is also a widget (see AXIsolatedObject::isWidget).
            setProperty(AXProperty::IsPlugin, true);
        } else
            setProperty(AXProperty::IsWidget, true);

        setProperty(AXProperty::IsVisible, object.isVisible());
    }

    auto descriptor = object.title();
    if (descriptor.length())
        setProperty(AXProperty::Title, descriptor.isolatedCopy());

    descriptor = object.description();
    if (descriptor.length())
        setProperty(AXProperty::Description, descriptor.isolatedCopy());

    descriptor = object.extendedDescription();
    if (descriptor.length())
        setProperty(AXProperty::ExtendedDescription, descriptor.isolatedCopy());

    if (object.isTextControl()) {
        // FIXME: We don't keep this property up-to-date, and we can probably just compute it using
        // AXIsolatedObject::selectedTextMarkerRange() (which does stay up-to-date).
        setProperty(AXProperty::SelectedTextRange, object.selectedTextRange());

        auto range = object.textInputMarkedTextMarkerRange();
        if (auto characterRange = range.characterRange(); range && characterRange)
            setProperty(AXProperty::TextInputMarkedTextMarkerRange, std::pair<Markable<AXID>, CharacterRange>(range.start().objectID(), *characterRange));

        setProperty(AXProperty::CanBeMultilineTextField, canBeMultilineTextField(object));
    }

#if ENABLE(AX_THREAD_TEXT_APIS)
    setProperty(AXProperty::TextRuns, object.textRuns());
    setProperty(AXProperty::EmitTextAfterBehavior, object.emitTextAfterBehavior());
#endif

    // These properties are only needed on the AXCoreObject interface due to their use in ATSPI,
    // so only cache them for ATSPI.
#if USE(ATSPI)
    // We cache IsVisible on all platforms just for Widgets above. In ATSPI, this should be cached on all objects.
    if (!object.isWidget())
        setProperty(AXProperty::IsVisible, object.isVisible());

    setProperty(AXProperty::ActionVerb, object.actionVerb().isolatedCopy());
    setProperty(AXProperty::IsFieldset, object.isFieldset());
    setProperty(AXProperty::IsPressed, object.isPressed());
    setProperty(AXProperty::IsSelectedOptionActive, object.isSelectedOptionActive());
    setProperty(AXProperty::LocalizedActionVerb, object.localizedActionVerb().isolatedCopy());
#endif

    setObjectProperty(AXProperty::InternalLinkElement, object.internalLinkElement());

    initializePlatformProperties(axObject);
}

bool AXIsolatedObject::canBeMultilineTextField(AccessibilityObject& object)
{
    if (object.isNonNativeTextControl())
        return !object.hasAttribute(aria_multilineAttr) || object.ariaIsMultiline();

    auto* renderer = object.renderer();
    if (renderer && renderer->isRenderTextControl())
        return renderer->isRenderTextControlMultiLine();

    // If we're not sure, return true, it means we can't use this as an optimization to avoid computing the line index.
    return true;
}

AccessibilityObject* AXIsolatedObject::associatedAXObject() const
{
    ASSERT(isMainThread());

    auto* axObjectCache = this->axObjectCache();
    return axObjectCache ? axObjectCache->objectForID(objectID()) : nullptr;
}

void AXIsolatedObject::setMathscripts(AXProperty propertyName, AccessibilityObject& object)
{
    AccessibilityMathMultiscriptPairs pairs;
    if (propertyName == AXProperty::MathPrescripts)
        object.mathPrescripts(pairs);
    else if (propertyName == AXProperty::MathPostscripts)
        object.mathPostscripts(pairs);
    
    size_t mathSize = pairs.size();
    if (!mathSize)
        return;

    auto idPairs = pairs.map([](auto& mathPair) {
        return std::pair { mathPair.first ? Markable { mathPair.first->objectID() } : std::nullopt, mathPair.second ? Markable { mathPair.second->objectID() } : std::nullopt };
    });
    setProperty(propertyName, WTFMove(idPairs));
}

void AXIsolatedObject::setObjectProperty(AXProperty propertyName, AXCoreObject* object)
{
    setProperty(propertyName, object ? Markable { object->objectID() } : std::nullopt);
}

void AXIsolatedObject::setObjectVectorProperty(AXProperty propertyName, const AccessibilityChildrenVector& objects)
{
    setProperty(propertyName, axIDs(objects));
}

void AXIsolatedObject::setProperty(AXProperty propertyName, AXPropertyValueVariant&& value)
{
    if (std::holds_alternative<bool>(value)) {
        switch (propertyName) {
        case AXProperty::CanSetFocusAttribute:
            setPropertyFlag(AXPropertyFlag::CanSetFocusAttribute, std::get<bool>(value));
            return;
        case AXProperty::CanSetSelectedAttribute:
            setPropertyFlag(AXPropertyFlag::CanSetSelectedAttribute, std::get<bool>(value));
            return;
        case AXProperty::CanSetValueAttribute:
            setPropertyFlag(AXPropertyFlag::CanSetValueAttribute, std::get<bool>(value));
            return;
        case AXProperty::HasBoldFont:
            setPropertyFlag(AXPropertyFlag::HasBoldFont, std::get<bool>(value));
            return;
        case AXProperty::HasItalicFont:
            setPropertyFlag(AXPropertyFlag::HasItalicFont, std::get<bool>(value));
            return;
        case AXProperty::HasPlainText:
            setPropertyFlag(AXPropertyFlag::HasPlainText, std::get<bool>(value));
            return;
        case AXProperty::IsEnabled:
            setPropertyFlag(AXPropertyFlag::IsEnabled, std::get<bool>(value));
            return;
        case AXProperty::IsExposedTableCell:
            setPropertyFlag(AXPropertyFlag::IsExposedTableCell, std::get<bool>(value));
            return;
        case AXProperty::IsGrabbed:
            setPropertyFlag(AXPropertyFlag::IsGrabbed, std::get<bool>(value));
            return;
        case AXProperty::IsIgnored:
            setPropertyFlag(AXPropertyFlag::IsIgnored, std::get<bool>(value));
            return;
        case AXProperty::IsInlineText:
            setPropertyFlag(AXPropertyFlag::IsInlineText, std::get<bool>(value));
            return;
        case AXProperty::IsKeyboardFocusable:
            setPropertyFlag(AXPropertyFlag::IsKeyboardFocusable, std::get<bool>(value));
            return;
        case AXProperty::IsNonLayerSVGObject:
            setPropertyFlag(AXPropertyFlag::IsNonLayerSVGObject, std::get<bool>(value));
            return;
        case AXProperty::IsTableRow:
            setPropertyFlag(AXPropertyFlag::IsTableRow, std::get<bool>(value));
            return;
        case AXProperty::SupportsCheckedState:
            setPropertyFlag(AXPropertyFlag::SupportsCheckedState, std::get<bool>(value));
            return;
        case AXProperty::SupportsDragging:
            setPropertyFlag(AXPropertyFlag::SupportsDragging, std::get<bool>(value));
            return;
        case AXProperty::SupportsExpanded:
            setPropertyFlag(AXPropertyFlag::SupportsExpanded, std::get<bool>(value));
            return;
        case AXProperty::SupportsPath:
            setPropertyFlag(AXPropertyFlag::SupportsPath, std::get<bool>(value));
            return;
        case AXProperty::SupportsPosInSet:
            setPropertyFlag(AXPropertyFlag::SupportsPosInSet, std::get<bool>(value));
            return;
        case AXProperty::SupportsSetSize:
            setPropertyFlag(AXPropertyFlag::SupportsSetSize, std::get<bool>(value));
            return;
        default:
            break;
        }
    }

    bool isDefaultValue = WTF::switchOn(value,
        [](std::nullptr_t&) { return true; },
        [](Markable<AXID> typedValue) { return !typedValue; },
        [&](String& typedValue) {
            // We use a null stringValue to indicate when the string value is different than the text content.
            if (propertyName == AXProperty::StringValue)
                return typedValue == emptyString(); // Only compares empty, not null
            return typedValue.isEmpty(); // null or empty
        },
        [](bool typedValue) { return !typedValue; },
        [](int typedValue) { return !typedValue; },
        [](unsigned typedValue) { return !typedValue; },
        [](double typedValue) { return typedValue == 0.0; },
        [](float typedValue) { return typedValue == 0.0; },
        [](uint64_t typedValue) { return !typedValue; },
        [](AccessibilityButtonState& typedValue) { return typedValue == AccessibilityButtonState::Off; },
        [](Color& typedValue) { return typedValue == Color(); },
        [](std::shared_ptr<URL>& typedValue) { return !typedValue || *typedValue == URL(); },
        [](LayoutRect& typedValue) { return typedValue == LayoutRect(); },
        [](IntPoint& typedValue) { return typedValue == IntPoint(); },
        [](IntRect& typedValue) { return typedValue == IntRect(); },
        [](FloatPoint& typedValue) { return typedValue == FloatPoint(); },
        [](FloatRect& typedValue) { return typedValue == FloatRect(); },
        [](std::pair<unsigned, unsigned>& typedValue) {
            // (0, 1) is the default for an index range.
            return typedValue == std::pair<unsigned, unsigned>(0, 1);
        },
        [](Vector<AccessibilityText>& typedValue) { return typedValue.isEmpty(); },
        [](Vector<AXID>& typedValue) { return typedValue.isEmpty(); },
        [](Vector<std::pair<Markable<AXID>, Markable<AXID>>>& typedValue) { return typedValue.isEmpty(); },
        [](Vector<String>& typedValue) { return typedValue.isEmpty(); },
        [](std::shared_ptr<Path>& typedValue) { return !typedValue || typedValue->isEmpty(); },
        [](OptionSet<AXAncestorFlag>& typedValue) { return typedValue.isEmpty(); },
#if PLATFORM(COCOA)
        [](RetainPtr<NSAttributedString>& typedValue) { return !typedValue; },
        [](RetainPtr<id>& typedValue) { return !typedValue; },
#endif
        [](InsideLink& typedValue) { return typedValue == InsideLink(); },
        [](Vector<Vector<Markable<AXID>>>& typedValue) { return typedValue.isEmpty(); },
        [](CharacterRange& typedValue) { return !typedValue.location && !typedValue.length; },
        [](std::pair<Markable<AXID>, CharacterRange>& typedValue) {
            return !typedValue.first && !typedValue.second.location && !typedValue.second.length;
        },
#if ENABLE(AX_THREAD_TEXT_APIS)
        [](AXTextRuns& runs) { return !runs.size(); },
        [](RetainPtr<CTFontRef>& typedValue) { return !typedValue; },
        [](TextEmissionBehavior typedValue) { return typedValue == TextEmissionBehavior::None; },
#endif // ENABLE(AX_THREAD_TEXT_APIS)
        [] (WallTime& time) { return !time; },
        [] (DateComponentsType& typedValue) { return typedValue == DateComponentsType::Invalid; },
        [](auto&) {
            ASSERT_NOT_REACHED();
            return false;
        }
    );
    if (isDefaultValue)
        m_propertyMap.remove(propertyName);
    else
        m_propertyMap.set(propertyName, value);
}

void AXIsolatedObject::detachRemoteParts(AccessibilityDetachmentType)
{
    ASSERT(!isMainThread());

    for (const auto& childID : m_childrenIDs) {
        if (RefPtr child = tree()->objectForID(childID))
            child->detachFromParent();
    }
    m_childrenIDs.clear();
    m_childrenDirty = true;
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

void AXIsolatedObject::setChildrenIDs(Vector<AXID>&& ids)
{
    m_childrenIDs = WTFMove(ids);
    m_childrenDirty = true;
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

        if (m_childrenDirty) {
            m_children = WTF::compactMap(m_childrenIDs, [&](auto& childID) -> std::optional<Ref<AXCoreObject>> {
                if (RefPtr child = tree()->objectForID(childID))
                    return child.releaseNonNull();
                return std::nullopt;
            });
            m_childrenDirty = false;
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
    ASSERT(selectedChildren.isEmpty() || selectedChildren[0]->isAXIsolatedObjectInstance());

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
    ASSERT(!isMainThread());

    if (parent())
        return false;

    // Check whether this is the root node, in which case we should return false.
    if (RefPtr root = tree()->rootNode())
        return root->objectID() != objectID();
    return false;
}

AXIsolatedObject* AXIsolatedObject::cellForColumnAndRow(unsigned columnIndex, unsigned rowIndex)
{
    // AXProperty::CellSlots can be big, so make sure not to copy it.
    auto cellSlotsIterator = m_propertyMap.find(AXProperty::CellSlots);
    if (cellSlotsIterator == m_propertyMap.end())
        return nullptr;

    auto cellID = WTF::switchOn(cellSlotsIterator->value,
        [&] (Vector<Vector<Markable<AXID>>>& cellSlots) -> std::optional<AXID> {
            if (rowIndex >= cellSlots.size() || columnIndex >= cellSlots[rowIndex].size())
                return std::nullopt;
            return cellSlots[rowIndex][columnIndex];
        },
        [] (auto&) -> std::optional<AXID> { return std::nullopt; }
    );
    return tree()->objectForID(cellID);
}

void AXIsolatedObject::accessibilityText(Vector<AccessibilityText>& texts) const
{
    texts = vectorAttributeValue<AccessibilityText>(AXProperty::AccessibilityText);
}

void AXIsolatedObject::insertMathPairs(Vector<std::pair<Markable<AXID>, Markable<AXID>>>& isolatedPairs, AccessibilityMathMultiscriptPairs& pairs)
{
    for (const auto& pair : isolatedPairs) {
        AccessibilityMathMultiscriptPair prescriptPair;
        if (RefPtr object = tree()->objectForID(pair.first))
            prescriptPair.first = object.get();
        if (RefPtr object = tree()->objectForID(pair.second))
            prescriptPair.second = object.get();
        pairs.append(prescriptPair);
    }
}

void AXIsolatedObject::mathPrescripts(AccessibilityMathMultiscriptPairs& pairs)
{
    auto isolatedPairs = vectorAttributeValue<std::pair<Markable<AXID>, Markable<AXID>>>(AXProperty::MathPrescripts);
    insertMathPairs(isolatedPairs, pairs);
}

void AXIsolatedObject::mathPostscripts(AccessibilityMathMultiscriptPairs& pairs)
{
    auto isolatedPairs = vectorAttributeValue<std::pair<Markable<AXID>, Markable<AXID>>>(AXProperty::MathPostscripts);
    insertMathPairs(isolatedPairs, pairs);
}

std::optional<AXCoreObject::AccessibilityChildrenVector> AXIsolatedObject::mathRadicand()
{
    if (m_propertyMap.contains(AXProperty::MathRadicand)) {
        Vector<Ref<AXCoreObject>> radicand;
        fillChildrenVectorForProperty(AXProperty::MathRadicand, radicand);
        return { radicand };
    }
    return std::nullopt;
}

bool AXIsolatedObject::fileUploadButtonReturnsValueInTitle() const
{
#if PLATFORM(MAC)
    return true;
#else
    return false;
#endif
}

AXIsolatedObject* AXIsolatedObject::focusedUIElement() const
{
    return tree()->focusedNode().get();
}
    
AXIsolatedObject* AXIsolatedObject::scrollBar(AccessibilityOrientation orientation)
{
    return objectAttributeValue(orientation == AccessibilityOrientation::Vertical ? AXProperty::VerticalScrollBar : AXProperty::HorizontalScrollBar);
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
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis())
        return selectedTextMarkerRange().toString();
#endif // ENABLE(AX_THREAD_TEXT_APIS)

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
    return colorAttributeValue(AXProperty::ColorValue).toColorTypeLossy<SRGBA<uint8_t>>();
}

AXIsolatedObject* AXIsolatedObject::accessibilityHitTest(const IntPoint& point) const
{
    auto axID = Accessibility::retrieveValueFromMainThread<std::optional<AXID>>([&point, this] () -> std::optional<AXID> {
        if (auto* object = associatedAXObject()) {
            object->updateChildrenIfNecessary();
            if (auto* axObject = object->accessibilityHitTest(point))
                return axObject->objectID();
        }

        return std::nullopt;
    });

    return tree()->objectForID(axID);
}

IntPoint AXIsolatedObject::intPointAttributeValue(AXProperty propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (IntPoint& typedValue) -> IntPoint { return typedValue; },
        [] (auto&) { return IntPoint(); }
    );
}

AXIsolatedObject* AXIsolatedObject::objectAttributeValue(AXProperty propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    auto axID = WTF::switchOn(value,
        [] (Markable<AXID>& typedValue) -> std::optional<AXID> { return typedValue; },
        [] (auto&) { return std::optional<AXID> { }; }
    );

    return tree()->objectForID(axID);
}

template<typename T>
T AXIsolatedObject::rectAttributeValue(AXProperty propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (T& typedValue) -> T { return typedValue; },
        [] (auto&) { return T { }; }
    );
}

template<typename T>
Vector<T> AXIsolatedObject::vectorAttributeValue(AXProperty propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (Vector<T>& typedValue) -> Vector<T> { return typedValue; },
        [] (auto&) { return Vector<T>(); }
    );
}

template<typename T>
OptionSet<T> AXIsolatedObject::optionSetAttributeValue(AXProperty propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (T& typedValue) -> OptionSet<T> { return typedValue; },
        [] (auto&) { return OptionSet<T>(); }
    );
}

std::pair<unsigned, unsigned> AXIsolatedObject::indexRangePairAttributeValue(AXProperty propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (std::pair<unsigned, unsigned>& typedValue) -> std::pair<unsigned, unsigned> { return typedValue; },
        [] (auto&) { return std::pair<unsigned, unsigned>(0, 1); }
    );
}

template<typename T>
std::optional<T> AXIsolatedObject::optionalAttributeValue(AXProperty propertyName) const
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

uint64_t AXIsolatedObject::uint64AttributeValue(AXProperty propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (uint64_t& typedValue) -> uint64_t { return typedValue; },
        [] (auto&) -> uint64_t { return 0; }
    );
}

URL AXIsolatedObject::urlAttributeValue(AXProperty propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (std::shared_ptr<URL>& typedValue) -> URL {
            ASSERT(typedValue.get());
            return *typedValue.get();
        },
        [] (auto&) { return URL(); }
    );
}

Path AXIsolatedObject::pathAttributeValue(AXProperty propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (std::shared_ptr<Path>& typedValue) -> Path {
            ASSERT(typedValue.get());
            return *typedValue.get();
        },
        [] (auto&) { return Path(); }
    );
}

Color AXIsolatedObject::colorAttributeValue(AXProperty propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (Color& typedValue) -> Color { return typedValue; },
        [] (auto&) { return Color(); }
    );
}

float AXIsolatedObject::floatAttributeValue(AXProperty propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (float& typedValue) -> float { return typedValue; },
        [] (auto&) { return 0.0f; }
    );
}

double AXIsolatedObject::doubleAttributeValue(AXProperty propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (double& typedValue) -> double { return typedValue; },
        [] (auto&) { return 0.0; }
    );
}

unsigned AXIsolatedObject::unsignedAttributeValue(AXProperty propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (unsigned& typedValue) -> unsigned { return typedValue; },
        [] (auto&) { return 0u; }
    );
}

bool AXIsolatedObject::boolAttributeValue(AXProperty propertyName) const
{
    switch (propertyName) {
    case AXProperty::CanSetFocusAttribute:
        return hasPropertyFlag(AXPropertyFlag::CanSetFocusAttribute);
    case AXProperty::CanSetSelectedAttribute:
        return hasPropertyFlag(AXPropertyFlag::CanSetSelectedAttribute);
    case AXProperty::CanSetValueAttribute:
        return hasPropertyFlag(AXPropertyFlag::CanSetValueAttribute);
    case AXProperty::HasBoldFont:
        return hasPropertyFlag(AXPropertyFlag::HasBoldFont);
    case AXProperty::HasItalicFont:
        return hasPropertyFlag(AXPropertyFlag::HasItalicFont);
    case AXProperty::HasPlainText:
        return hasPropertyFlag(AXPropertyFlag::HasPlainText);
    case AXProperty::IsEnabled:
        return hasPropertyFlag(AXPropertyFlag::IsEnabled);
    case AXProperty::IsExposedTableCell:
        return hasPropertyFlag(AXPropertyFlag::IsExposedTableCell);
    case AXProperty::IsGrabbed:
        return hasPropertyFlag(AXPropertyFlag::IsGrabbed);
    case AXProperty::IsIgnored:
        return hasPropertyFlag(AXPropertyFlag::IsIgnored);
    case AXProperty::IsInlineText:
        return hasPropertyFlag(AXPropertyFlag::IsInlineText);
    case AXProperty::IsKeyboardFocusable:
        return hasPropertyFlag(AXPropertyFlag::IsKeyboardFocusable);
    case AXProperty::IsNonLayerSVGObject:
        return hasPropertyFlag(AXPropertyFlag::IsNonLayerSVGObject);
    case AXProperty::IsTableRow:
        return hasPropertyFlag(AXPropertyFlag::IsTableRow);
    case AXProperty::SupportsCheckedState:
        return hasPropertyFlag(AXPropertyFlag::SupportsCheckedState);
    case AXProperty::SupportsDragging:
        return hasPropertyFlag(AXPropertyFlag::SupportsDragging);
    case AXProperty::SupportsExpanded:
        return hasPropertyFlag(AXPropertyFlag::SupportsExpanded);
    case AXProperty::SupportsPath:
        return hasPropertyFlag(AXPropertyFlag::SupportsPath);
    case AXProperty::SupportsPosInSet:
        return hasPropertyFlag(AXPropertyFlag::SupportsPosInSet);
    case AXProperty::SupportsSetSize:
        return hasPropertyFlag(AXPropertyFlag::SupportsSetSize);
    default:
        break;
    }

    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (bool& typedValue) { return typedValue; },
        [] (auto&) { return false; }
    );
}

String AXIsolatedObject::stringAttributeValue(AXProperty propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (String& typedValue) { return typedValue; },
        [] (auto&) { return emptyString(); }
    );
}

String AXIsolatedObject::stringAttributeValueNullIfMissing(AXProperty propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (String& typedValue) { return typedValue; },
        [] (auto&) { return nullString(); }
    );
}

int AXIsolatedObject::intAttributeValue(AXProperty propertyName) const
{
    auto value = m_propertyMap.get(propertyName);
    return WTF::switchOn(value,
        [] (int& typedValue) { return typedValue; },
        [] (auto&) { return 0; }
    );
}

#if ENABLE(AX_THREAD_TEXT_APIS)
const AXTextRuns* AXIsolatedObject::textRuns() const
{
    auto entry = m_propertyMap.find(AXProperty::TextRuns);
    if (entry == m_propertyMap.end())
        return nullptr;
    return WTF::switchOn(entry->value,
        [] (const AXTextRuns& typedValue) -> const AXTextRuns* { return &typedValue; },
        [] (auto&) -> const AXTextRuns* { return nullptr; }
    );
}
#endif

template<typename T>
T AXIsolatedObject::getOrRetrievePropertyValue(AXProperty propertyName)
{
    if (m_propertyMap.contains(propertyName))
        return propertyValue<T>(propertyName);

    Accessibility::performFunctionOnMainThreadAndWait([&propertyName, this] () {
        auto* axObject = associatedAXObject();
        if (!axObject)
            return;

        AXPropertyValueVariant value;
        switch (propertyName) {
        case AXProperty::InnerHTML:
            value = axObject->innerHTML().isolatedCopy();
            break;
        case AXProperty::OuterHTML:
            value = axObject->outerHTML().isolatedCopy();
            break;
        default:
            break;
        }

        // Cache value so that there is no need to access the main thread in subsequent calls.
        m_propertyMap.set(propertyName, WTFMove(value));
    });

    return propertyValue<T>(propertyName);
}

void AXIsolatedObject::fillChildrenVectorForProperty(AXProperty propertyName, AccessibilityChildrenVector& children) const
{
    Vector<AXID> childIDs = vectorAttributeValue<AXID>(propertyName);
    children.reserveCapacity(childIDs.size());
    for (const auto& childID : childIDs) {
        if (RefPtr object = tree()->objectForID(childID))
            children.append(object.releaseNonNull());
    }
}

void AXIsolatedObject::updateBackingStore()
{
    ASSERT(!isMainThread());

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

#if PLATFORM(MAC)
AXTextMarkerRange AXIsolatedObject::selectedTextMarkerRange() const
{
    return tree()->selectedTextMarkerRange();
}
#endif // PLATFORM(MAC)

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

AXCoreObject::AccessibilityChildrenVector AXIsolatedObject::findMatchingObjects(AccessibilitySearchCriteria&& criteria)
{
    criteria.anchorObject = this;
    return AXSearchManager().findMatchingObjects(WTFMove(criteria));
}

String AXIsolatedObject::textUnderElement(TextUnderElementMode) const
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

IntPoint AXIsolatedObject::remoteFrameOffset() const
{
    RefPtr root = tree()->rootNode();
    return root ? root->propertyValue<IntPoint>(AXProperty::RemoteFrameOffset) : IntPoint();
}

FloatPoint AXIsolatedObject::screenRelativePosition() const
{
    if (auto point = optionalAttributeValue<FloatPoint>(AXProperty::ScreenRelativePosition))
        return *point;

    if (RefPtr rootNode = tree()->rootNode()) {
        auto rootPoint = rootNode->propertyValue<FloatPoint>(AXProperty::ScreenRelativePosition);
        auto rootRelativeFrame = rootNode->relativeFrame();
        auto relativeFrame = this->relativeFrame();
        // Relative frames are top-left origin, but screen relative positions are bottom-left origin.
        return { rootPoint.x() + relativeFrame.x(), rootPoint.y() + (rootRelativeFrame.maxY() - relativeFrame.maxY()) };
    }

    return Accessibility::retrieveValueFromMainThread<FloatPoint>([&, this] () -> FloatPoint {
        if (auto* axObject = associatedAXObject())
            return axObject->screenRelativePosition();
        return { };
    });
}

FloatRect AXIsolatedObject::relativeFrame() const
{
    FloatRect relativeFrame;

    if (auto cachedRelativeFrame = optionalAttributeValue<IntRect>(AXProperty::RelativeFrame)) {
        // We should not have cached a relative frame for elements that get their geometry from their children.
        ASSERT(!m_getsGeometryFromChildren);
        relativeFrame = *cachedRelativeFrame;
    } else if (m_getsGeometryFromChildren) {
        auto frame = enclosingIntRect(relativeFrameFromChildren());
        if (!frame.isEmpty())
            relativeFrame = frame;
        // Either we had no children, or our children had empty frames. The right thing to do would be to return
        // a rect at the position of the nearest render tree ancestor with some made-up size (AccessibilityNodeObject::boundingBoxRect does this).
        // However, we don't have access to the render tree in this context (only the AX isolated tree, which is too sparse for this purpose), so
        // until we cache the necessary information let's go to the main-thread.
    } else if (roleValue() == AccessibilityRole::Column || roleValue() == AccessibilityRole::TableHeaderContainer)
        relativeFrame = exposedTableAncestor() ? relativeFrameFromChildren() : FloatRect();

    // Mock objects and SVG objects need use the main thread since they do not have render nodes and are not painted with layers, respectively.
    // FIXME: Remove isNonLayerSVGObject when LBSE is enabled & SVG frames are cached.
    if (!AXObjectCache::shouldServeInitialCachedFrame() || isNonLayerSVGObject()) {
        return Accessibility::retrieveValueFromMainThread<FloatRect>([this] () -> FloatRect {
            if (auto* axObject = associatedAXObject())
                return axObject->relativeFrame();
            return { };
        });
    }

    // Having an empty relative frame at this point means a frame hasn't been cached yet.
    if (relativeFrame.isEmpty()) {
        // InitialFrameRect stores the correct size, but not position, of the element before it is painted.
        // We find the position of the nearest painted ancestor to use as the position until the object's frame
        // is cached during painting.
        auto* ancestor = Accessibility::findAncestor<AXIsolatedObject>(*this, false, [] (const auto& object) {
            return object.hasCachedRelativeFrame();
        });
        relativeFrame = rectAttributeValue<FloatRect>(AXProperty::InitialFrameRect);
        if (ancestor && relativeFrame.location() == FloatPoint())
            relativeFrame.setLocation(ancestor->relativeFrame().location());
    }

    relativeFrame.moveBy({ remoteFrameOffset() });
    return relativeFrame;
}

FloatRect AXIsolatedObject::relativeFrameFromChildren() const
{
    FloatRect rect;
    for (const auto& child : const_cast<AXIsolatedObject*>(this)->unignoredChildren())
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
    AXTRACE(makeString("AXIsolatedObject::insertText text = "_s, text));

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

int AXIsolatedObject::insertionPointLineNumber() const
{
    if (!boolAttributeValue(AXProperty::CanBeMultilineTextField))
        return 0;

    auto selectedMarkerRange = selectedTextMarkerRange();
    if (selectedMarkerRange.start().isNull() || !selectedMarkerRange.isCollapsed()) {
        // If the selection is not collapsed, we don't know whether the insertion point is at the start or the end, so return -1.
        return -1;
    }

#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis()) {
        RefPtr selectionObject = selectedMarkerRange.start().isolatedObject();
        if (isTextControl() && selectionObject && isAncestorOfObject(*selectionObject))
            return selectedMarkerRange.start().lineIndex();
        return -1;
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    return Accessibility::retrieveValueFromMainThread<int>([this] () -> int {
        if (auto* axObject = associatedAXObject())
            return axObject->insertionPointLineNumber();
        return -1;
    });
}

String AXIsolatedObject::identifierAttribute() const
{
#if !LOG_DISABLED
    return stringAttributeValue(AXProperty::IdentifierAttribute);
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
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis())
        return AXTextMarker { *this, 0 }.characterRangeForLine(lineIndex);
#endif

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
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis())
        return AXTextMarker { *this, 0 }.lineNumberForIndex(index);
#endif

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

VisiblePositionRange AXIsolatedObject::styleRangeForPosition(const VisiblePosition& position) const
{
    ASSERT(isMainThread());
    auto* axObject = associatedAXObject();
    return axObject ? axObject->styleRangeForPosition(position) : VisiblePositionRange();
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
#if PLATFORM(MAC)
    ASSERT_NOT_REACHED();
#endif
    return boolAttributeValue(AXProperty::IsPressed);
}

bool AXIsolatedObject::isSelectedOptionActive() const
{
    ASSERT_NOT_REACHED();
    return false;
}

Vector<AXTextMarkerRange> AXIsolatedObject::misspellingRanges() const
{
    return Accessibility::retrieveValueFromMainThread<Vector<AXTextMarkerRange>>([this] () -> Vector<AXTextMarkerRange> {
        if (auto* axObject = associatedAXObject())
            return axObject->misspellingRanges();
        return { };
    });
}

bool AXIsolatedObject::hasSameFont(AXCoreObject& otherObject)
{
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis()) {
        // Having a font only really makes sense for text, so if this or otherObject isn't text, find the first text descendant to compare.
        RefPtr thisText = selfOrFirstTextDescendant();
        RefPtr otherText = otherObject.selfOrFirstTextDescendant();

        if (!thisText || !otherText) {
            // We can't make a meaningful comparison unless we have two objects to compare, so return false.
            return false;
        }
        return thisText->font() == otherText->font();
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

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

bool AXIsolatedObject::hasSameFontColor(AXCoreObject& otherObject)
{
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis()) {
        RefPtr thisText = downcast<AXIsolatedObject>(selfOrFirstTextDescendant());
        RefPtr otherText = downcast<AXIsolatedObject>(otherObject.selfOrFirstTextDescendant());

        if (!thisText || !otherText)
            return false;
        return thisText->colorAttributeValue(AXProperty::TextColor) == otherText->colorAttributeValue(AXProperty::TextColor);
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

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

bool AXIsolatedObject::hasSameStyle(AXCoreObject& otherObject)
{
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis()) {
        RefPtr thisText = selfOrFirstTextDescendant();
        RefPtr otherText = otherObject.selfOrFirstTextDescendant();

        if (!thisText || !otherText)
            return false;
        return thisText->stylesForAttributedString() == otherText->stylesForAttributedString();
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

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
    auto value = optionalAttributeValue<std::pair<Markable<AXID>, CharacterRange>>(AXProperty::TextInputMarkedTextMarkerRange);
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

bool AXIsolatedObject::isDescendantOfRole(AccessibilityRole) const
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

String AXIsolatedObject::titleAttributeValue() const
{
    AXTRACE("AXIsolatedObject::titleAttributeValue"_s);

    if (m_propertyMap.contains(AXProperty::TitleAttributeValue))
        return propertyValue<String>(AXProperty::TitleAttributeValue);
    return AXCoreObject::titleAttributeValue();
}

String AXIsolatedObject::stringValue() const
{
    if (m_propertyMap.contains(AXProperty::StringValue))
        return stringAttributeValue(AXProperty::StringValue);
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

OptionSet<AXAncestorFlag> AXIsolatedObject::ancestorFlags() const
{
    auto value = m_propertyMap.get(AXProperty::AncestorFlags);
    return WTF::switchOn(value,
        [] (OptionSet<AXAncestorFlag>& typedValue) -> OptionSet<AXAncestorFlag> { return typedValue; },
        [] (auto&) { return OptionSet<AXAncestorFlag>(); }
    );
}

String AXIsolatedObject::innerHTML() const
{
    return const_cast<AXIsolatedObject*>(this)->getOrRetrievePropertyValue<String>(AXProperty::InnerHTML);
}

String AXIsolatedObject::outerHTML() const
{
    return const_cast<AXIsolatedObject*>(this)->getOrRetrievePropertyValue<String>(AXProperty::OuterHTML);
}

AXCoreObject::AccessibilityChildrenVector AXIsolatedObject::rowHeaders()
{
    AccessibilityChildrenVector headers;
    if (isTable()) {
        auto rowsCopy = rows();
        for (const auto& row : rowsCopy) {
            if (auto* header = row->rowHeader())
                headers.append(*header);
        }
    } else if (isExposedTableCell()) {
        RefPtr parent = exposedTableAncestor();
        if (!parent)
            return { };

        auto rowRange = rowIndexRange();
        auto colRange = columnIndexRange();
        for (unsigned column = 0; column < colRange.first; column++) {
            RefPtr tableCell = parent->cellForColumnAndRow(column, rowRange.first);
            if (!tableCell || tableCell == this || headers.contains(Ref { *tableCell }))
                continue;

            if (tableCell->cellScope() == "rowgroup"_s && isTableCellInSameRowGroup(*tableCell))
                headers.append(tableCell.releaseNonNull());
            else if (tableCell->isRowHeader())
                headers.append(tableCell.releaseNonNull());
        }
    }
    return headers;
}

AXIsolatedObject* AXIsolatedObject::headerContainer()
{
    for (const auto& child : unignoredChildren()) {
        if (child->roleValue() == AccessibilityRole::TableHeaderContainer)
            return downcast<AXIsolatedObject>(child.ptr());
    }
    return nullptr;
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
