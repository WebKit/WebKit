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

#include <wtf/Platform.h>
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)

#include <WebCore/AXCoreObject.h>
#include <WebCore/AXIsolatedTree.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/AXTreeStoreInlines.h>
#include <WebCore/IntPoint.h>
#include <WebCore/LayoutRect.h>
#include <WebCore/NodeName.h>
#include <WebCore/Path.h>
#include <WebCore/RenderStyleConstants.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AXIsolatedTree;
#if ENABLE(AX_THREAD_TEXT_APIS)
struct AXTextRuns;
#endif

class AXIsolatedObject final : public AXCoreObject {
    friend class AXIsolatedTree;
public:
    static Ref<AXIsolatedObject> create(IsolatedObjectData&&);
    ~AXIsolatedObject();

    // FIXME: tree()->treeID() is never optional, so this shouldn't return an optional either.
    std::optional<AXID> treeID() const final { return tree()->treeID(); }
    String debugDescriptionInternal(bool, std::optional<OptionSet<AXDebugStringOption>> = std::nullopt) const final;

    void updateFromData(IsolatedObjectData&&);

    void attachPlatformWrapper(AccessibilityObjectWrapper*);
    bool isDetached() const final;
    bool isTable() const final { return boolAttributeValue(AXProperty::IsTable); }
    bool isExposableTable() const final { return boolAttributeValue(AXProperty::IsExposableTable); }
    bool hasClickHandler() const final { return boolAttributeValue(AXProperty::HasClickHandler); }
    bool hasCursorPointer() const final { return boolAttributeValue(AXProperty::HasCursorPointer);  }
    bool hasPointerEventsNone() const final { return boolAttributeValue(AXProperty::HasPointerEventsNone);  }
    bool showsCursorOnHover() const final { return boolAttributeValue(AXProperty::ShowsCursorOnHover); }
    FloatRect relativeFrame() const final;

    bool hasAttachmentTag() const final { return elementName() == ElementName::HTML_attachment; }
    bool hasBodyTag() const final { return elementName() == ElementName::HTML_body; }
    bool hasMarkTag() const final { return elementName() == ElementName::HTML_mark; }
    bool hasRowGroupTag() const final;

    const AccessibilityChildrenVector& children(bool updateChildrenIfNeeded = true) final;
#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    AXIsolatedObject* parentObject() const final { return tree()->objectForID(parent()); }
    AXIsolatedObject* parentObjectUnignored() const final { return downcast<AXIsolatedObject>(AXCoreObject::parentObjectUnignored()); }
#else
    AXIsolatedObject* parentObject() const final { return parentObjectUnignored(); }
    AXIsolatedObject* parentObjectUnignored() const final { return tree()->objectForID(parent()); }
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    AXIsolatedObject* clickableSelfOrAncestor(ClickHandlerFilter filter = ClickHandlerFilter::ExcludeBody) const final { return Accessibility::clickableSelfOrAncestor(*this, filter); };
    AXIsolatedObject* editableAncestor() const final { return Accessibility::editableAncestor(*this); };
    bool isEditableWebArea() const final { return boolAttributeValue(AXProperty::IsEditableWebArea); }
    bool canSetFocusAttribute() const final { return boolAttributeValue(AXProperty::CanSetFocusAttribute); }
    AttributedStringStyle stylesForAttributedString() const final;
    Color textColor() const final { return colorAttributeValue(AXProperty::TextColor); }

#if ENABLE_ACCESSIBILITY_LOCAL_FRAME
    // Returns the child or parent object that crosses a local frame boundary.
    AXIsolatedObject* crossFrameParentObject() const final;
    AXIsolatedObject* crossFrameChildObject() const final;
#endif

#if ENABLE(AX_THREAD_TEXT_APIS)
    const AXTextRuns* textRuns() const;
    bool hasTextRuns() final
    {
        const auto* runs = textRuns();
        return runs && runs->size();
    }
    TextEmissionBehavior textEmissionBehavior() const final;
    AXTextRunLineID listMarkerLineID() const final { return propertyValue<AXTextRunLineID>(AXProperty::ListMarkerLineID); };
    String listMarkerText() const final { return stringAttributeValue(AXProperty::ListMarkerText); }
    FontOrientation fontOrientation() const final { return propertyValue<FontOrientation>(AXProperty::FontOrientation); }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    String description() const final { return stringAttributeValue(AXProperty::Description); }

#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    bool isIgnored() const final { return boolAttributeValue(AXProperty::IsIgnored); }
#else
    // When not including ignored objects in the core tree, we should never create an isolated object from
    // an ignored live object, so we can hardcode this to false.
    bool isIgnored() const final { return false; }
#endif // ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)

    AXTextMarkerRange textMarkerRange() const final;

#if PLATFORM(COCOA)
    RetainPtr<CTFontRef> font() const final { return fontAttributeValue(AXProperty::Font); }
#endif

private:
    constexpr ProcessID processID() const final { return tree()->processID(); }
    void detachRemoteParts(AccessibilityDetachmentType) final;
    void detachPlatformWrapper(AccessibilityDetachmentType) final;

    std::optional<AXID> parent() const { return m_parentID; }
    void setParent(std::optional<AXID> axID) { m_parentID = axID; }

    AXIsolatedTree* tree() const { return m_tree.ptr(); }

    AXIsolatedObject(const Ref<AccessibilityObject>&, AXIsolatedTree*);
    AXIsolatedObject(IsolatedObjectData&&);
    bool isAXIsolatedObjectInstance() const final { return true; }
    AccessibilityObject* associatedAXObject() const;

    void setProperty(AXProperty, AXPropertyValueVariant&&);
    void setPropertyInVector(AXProperty property, AXPropertyValueVariant&& value)
    {
        if (size_t existingIndex = indexOfProperty(property); existingIndex != notFound)
            m_properties[existingIndex].second = WTFMove(value);
        else
            m_properties.append(std::pair(property, WTFMove(value)));
    }
    void removePropertyInVector(AXProperty property)
    {
        m_properties.removeFirstMatching([&property] (const auto& propertyAndValue) {
            return propertyAndValue.first == property;
        });
    }
    size_t indexOfProperty(AXProperty property) const
    {
        return m_properties.findIf([&property] (const auto& propertyAndValue) {
            return propertyAndValue.first == property;
        });
    }
    void shrinkPropertiesAfterUpdates() { m_properties.shrinkToFit(); }
    void setObjectProperty(AXProperty, AXCoreObject*);
    void setObjectVectorProperty(AXProperty, const AccessibilityChildrenVector&);

    void setPropertyFlag(AXPropertyFlag, bool);
    bool hasPropertyFlag(AXPropertyFlag) const;
    bool hasPropertyFlag(AXProperty) const;

    // FIXME: consolidate all AttributeValue retrieval in a single template method.
    bool boolAttributeValue(AXProperty) const;
    String stringAttributeValue(AXProperty) const;
    String stringAttributeValueNullIfMissing(AXProperty) const;
    int intAttributeValue(AXProperty) const;
    unsigned unsignedAttributeValue(AXProperty) const;
    double doubleAttributeValue(AXProperty) const;
    float floatAttributeValue(AXProperty) const;
    AXIsolatedObject* objectAttributeValue(AXProperty) const;
    IntPoint intPointAttributeValue(AXProperty) const;
    Color colorAttributeValue(AXProperty) const;
    RetainPtr<CTFontRef> fontAttributeValue(AXProperty) const;
    URL urlAttributeValue(AXProperty) const;
    uint64_t uint64AttributeValue(AXProperty) const;
    Path pathAttributeValue(AXProperty) const;
    Style::SpeakAs speakAsAttributeValue(AXProperty) const;
    std::pair<unsigned, unsigned> indexRangePairAttributeValue(AXProperty) const;
    template<typename T> T rectAttributeValue(AXProperty) const;
    template<typename T> Vector<T> vectorAttributeValue(AXProperty) const;
    template<typename T> std::optional<T> optionalAttributeValue(AXProperty) const;
    template<typename T> T propertyValue(AXProperty) const;

#ifndef NDEBUG
    // The color of |this| without any ancestry traversal.
    Color cachedTextColor() const;
#if PLATFORM(COCOA)
    // The font of |this| without any ancestry traversal.
    RetainPtr<CTFontRef> cachedFont() const;
#endif // PLATFORM(COCOA)
#endif // NDEBUG

    // The following method performs a lazy caching of the given property.
    // If the property is already in m_properties, returns the existing value.
    // If not, retrieves the property from the main thread and cache it for later use.
    template<typename T> T getOrRetrievePropertyValue(AXProperty);

    void fillChildrenVectorForProperty(AXProperty, AccessibilityChildrenVector&) const;
    void setMathscripts(AXProperty, AccessibilityObject&);
    void insertMathPairs(Vector<std::pair<Markable<AXID>, Markable<AXID>>>&, AccessibilityMathMultiscriptPairs&);
    template<typename U> void performFunctionOnMainThreadAndWait(U&& lambda) const
    {
        Accessibility::performFunctionOnMainThreadAndWait([&lambda, this] {
            if (RefPtr object = associatedAXObject())
                lambda(object.get());
        });
    }
    template<typename U> void performFunctionOnMainThread(U&& lambda) const
    {
        // Because this is an async dispatch to the main-thread, there's no guarantee |this| will be kept
        // alive by the secondary thread. Avoid any issues by simply sending our object ID, and our associated
        // AXObjectCache ID, then having the main-thread re-hydrate the equivalent main-thread accessibility object,
        // if it's still alive by the time the dispatch is serviced.
        Accessibility::performFunctionOnMainThread([lambda = WTFMove(lambda), axID = objectID(), cacheID = treeID()] () mutable {
            WeakPtr cache = AXTreeStore<AXObjectCache>::axObjectCacheForID(cacheID);
            if (!cache)
                return;
            if (RefPtr object = cache->objectForID(axID))
                lambda(object.get());
        });
    }

    // Attribute retrieval overrides.
    bool isSecureField() const final { return boolAttributeValue(AXProperty::IsSecureField); }
    bool isAttachment() const final { return boolAttributeValue(AXProperty::IsAttachment); }

    bool isKeyboardFocusable() const final { return boolAttributeValue(AXProperty::IsKeyboardFocusable); }
    bool isOutput() const final { return elementName() == ElementName::HTML_output; }

    // Table support.
    AXIsolatedObject* exposedTableAncestor(bool includeSelf = false) const final { return Accessibility::exposedTableAncestor(*this, includeSelf); }
    AccessibilityChildrenVector columns() final { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXProperty::Columns)); }
    AccessibilityChildrenVector rows() final { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXProperty::Rows)); }
    unsigned columnCount() final { return static_cast<unsigned>(columns().size()); }
    unsigned rowCount() final { return static_cast<unsigned>(rows().size()); }
    AccessibilityChildrenVector cells() final { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXProperty::Cells)); }
    AXIsolatedObject* cellForColumnAndRow(unsigned, unsigned) final;
    AccessibilityChildrenVector rowHeaders() final;
    AccessibilityChildrenVector visibleRows() final { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXProperty::VisibleRows)); }
    AXIsolatedObject* tableHeaderContainer() final;
    int axColumnCount() const final { return intAttributeValue(AXProperty::AXColumnCount); }
    int axRowCount() const final { return intAttributeValue(AXProperty::AXRowCount); }

    // Table cell support.
    bool isTableCell() const final;
    bool isExposedTableCell() const final { return boolAttributeValue(AXProperty::IsExposedTableCell); }
    AXCoreObject* parentTableIfTableCell() const final;
    // Returns the start location and row span of the cell.
    std::pair<unsigned, unsigned> rowIndexRange() const final { return indexRangePairAttributeValue(AXProperty::RowIndexRange); }
    // Returns the start location and column span of the cell.
    std::pair<unsigned, unsigned> columnIndexRange() const final { return indexRangePairAttributeValue(AXProperty::ColumnIndexRange); }
    std::optional<unsigned> axColumnIndex() const final { return optionalAttributeValue<unsigned>(AXProperty::AXColumnIndex); }
    std::optional<unsigned> axRowIndex() const final { return optionalAttributeValue<unsigned>(AXProperty::AXRowIndex); }
    String axColumnIndexText() const final { return stringAttributeValueNullIfMissing(AXProperty::AXColumnIndexText); }
    String axRowIndexText() const final { return stringAttributeValueNullIfMissing(AXProperty::AXRowIndexText); }
    bool isColumnHeader() const final { return boolAttributeValue(AXProperty::IsColumnHeader); }
    bool isRowHeader() const final { return boolAttributeValue(AXProperty::IsRowHeader); }
    String cellScope() const final { return stringAttributeValue(AXProperty::CellScope); }

    // Table column support.
    unsigned columnIndex() const final { return unsignedAttributeValue(AXProperty::ColumnIndex); }

    // Table row support.
    AXCoreObject* parentTable() const final;
    bool isTableRow() const final;
    AXCoreObject* parentTableIfExposedTableRow() const final;
    bool isExposedTableRow() const final { return boolAttributeValue(AXProperty::IsExposedTableRow); }
    unsigned rowIndex() const final { return unsignedAttributeValue(AXProperty::RowIndex); }

    // ARIA tree/grid row support.
    bool isARIATreeGridRow() const final { return boolAttributeValue(AXProperty::IsARIATreeGridRow); }
    bool isARIAGridRow() const final { return boolAttributeValue(AXProperty::IsARIAGridRow) || isARIATreeGridRow(); }
    AccessibilityChildrenVector disclosedRows() final { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXProperty::DisclosedRows)); }
    AXIsolatedObject* disclosedByRow() const final { return objectAttributeValue(AXProperty::DisclosedByRow); }

    bool isFieldset() const final { return boolAttributeValue(AXProperty::IsFieldset); }
    bool isChecked() const final { return boolAttributeValue(AXProperty::IsChecked); }
    bool isEnabled() const final { return boolAttributeValue(AXProperty::IsEnabled); }
    bool isSelected() const final { return boolAttributeValue(AXProperty::IsSelected); }
    bool isFocused() const final { return tree()->focusedNodeID() == objectID(); }
    bool isMultiSelectable() const final { return boolAttributeValue(AXProperty::IsMultiSelectable); }
    bool isVisited() const final { return boolAttributeValue(AXProperty::IsVisited); }
    bool isRequired() const final { return boolAttributeValue(AXProperty::IsRequired); }
    bool isExpanded() const final { return boolAttributeValue(AXProperty::IsExpanded); }
    bool isDescriptionList() const final { return elementName() == ElementName::HTML_dl; }
    std::optional<InputType::Type> inputType() const final { return optionalAttributeValue<InputType::Type>(AXProperty::InputType); };
    FloatPoint screenRelativePosition() const final;
    IntPoint remoteFrameOffset() const final;
    std::optional<IntRect> cachedRelativeFrame() const { return optionalAttributeValue<IntRect>(AXProperty::RelativeFrame); }
#if PLATFORM(MAC)
    FloatRect primaryScreenRect() const final;
#endif
    IntSize size() const final { return snappedIntRect(LayoutRect(relativeFrame())).size(); }
    FloatRect relativeFrameFromChildren() const;
    WallTime dateTimeValue() const final { return propertyValue<WallTime>(AXProperty::DateTimeValue); }
    DateComponentsType dateTimeComponentsType() const final { return propertyValue<DateComponentsType>(AXProperty::DateTimeComponentsType); }
    String datetimeAttributeValue() const final { return stringAttributeValue(AXProperty::DatetimeAttributeValue); }
    bool canSetValueAttribute() const final { return boolAttributeValue(AXProperty::CanSetValueAttribute); }
    bool canSetSelectedAttribute() const final { return boolAttributeValue(AXProperty::CanSetSelectedAttribute); }
    AccessibilityButtonState checkboxOrRadioValue() const final { return propertyValue<AccessibilityButtonState>(AXProperty::ButtonState); }
    String valueDescription() const final { return stringAttributeValue(AXProperty::ValueDescription); }
    float valueForRange() const final { return floatAttributeValue(AXProperty::ValueForRange); }
    float maxValueForRange() const final { return floatAttributeValue(AXProperty::MaxValueForRange); }
    float minValueForRange() const final { return floatAttributeValue(AXProperty::MinValueForRange); }
    int layoutCount() const final;
    double loadingProgress() const final { return tree()->loadingProgress(); }
    bool supportsARIAOwns() const final { return boolAttributeValue(AXProperty::SupportsARIAOwns); }
    String explicitPopupValue() const final { return stringAttributeValue(AXProperty::ExplicitPopupValue); }
    bool pressedIsPresent() const final;
    String explicitInvalidStatus() const final { return stringAttributeValue(AXProperty::ExplicitInvalidStatus); }
    bool supportsExpanded() const final { return boolAttributeValue(AXProperty::SupportsExpanded); }
    AccessibilitySortDirection sortDirection() const final { return static_cast<AccessibilitySortDirection>(intAttributeValue(AXProperty::SortDirection)); }
    String identifierAttribute() const final;
    String linkRelValue() const final;
    Vector<String> classList() const final;
    AccessibilityCurrentState currentState() const final { return static_cast<AccessibilityCurrentState>(intAttributeValue(AXProperty::CurrentState)); }
    bool supportsCurrent() const final { return boolAttributeValue(AXProperty::SupportsCurrent); }
    bool supportsKeyShortcuts() const final { return boolAttributeValue(AXProperty::SupportsKeyShortcuts); }
    String keyShortcuts() const final { return stringAttributeValue(AXProperty::KeyShortcuts); }
    bool supportsSetSize() const final { return boolAttributeValue(AXProperty::SupportsSetSize); }
    bool supportsPosInSet() const final { return boolAttributeValue(AXProperty::SupportsPosInSet); }
    int setSize() const final { return intAttributeValue(AXProperty::SetSize); }
    int posInSet() const final { return intAttributeValue(AXProperty::PosInSet); }
    bool supportsDropping() const final { return boolAttributeValue(AXProperty::SupportsDropping); }
    bool supportsDragging() const final { return boolAttributeValue(AXProperty::SupportsDragging); }
    bool isGrabbed() final { return boolAttributeValue(AXProperty::IsGrabbed); }
    bool isHiddenUntilFoundContainer() const final { return boolAttributeValue(AXProperty::IsHiddenUntilFoundContainer); }
    Vector<String> determineDropEffects() const final;
    AXIsolatedObject* accessibilityHitTest(const IntPoint&) const final;
    AXIsolatedObject* focusedUIElement() const final
    {
        return tree()->focusedNode().unsafeGet();
    }
    AXIsolatedObject* focusedUIElementInAnyLocalFrame() const final
    {
        return tree()->focusedNode().unsafeGet();
    }
    AXIsolatedObject* internalLinkElement() const final { return objectAttributeValue(AXProperty::InternalLinkElement); }
    AccessibilityChildrenVector radioButtonGroup() const final { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXProperty::RadioButtonGroupMembers)); }
    AXIsolatedObject* scrollBar(AccessibilityOrientation) final;
    const String placeholderValue() const final { return stringAttributeValue(AXProperty::PlaceholderValue); }
    String abbreviation() const final { return stringAttributeValue(AXProperty::Abbreviation); }
    SRGBA<uint8_t> colorValue() const final;
    String subrolePlatformString() const final { return stringAttributeValue(AXProperty::SubrolePlatformString); }
    String ariaRoleDescription() const final { return stringAttributeValue(AXProperty::ARIARoleDescription); };
    LayoutRect elementRect() const final;
    IntPoint clickPoint() final;
    void accessibilityText(Vector<AccessibilityText>& texts) const final;
    String brailleLabel() const final { return stringAttributeValue(AXProperty::BrailleLabel); }
    String brailleRoleDescription() const final { return stringAttributeValue(AXProperty::BrailleRoleDescription); }
    String embeddedImageDescription() const final { return stringAttributeValue(AXProperty::EmbeddedImageDescription); }
    std::optional<AccessibilityChildrenVector> imageOverlayElements() final { return std::nullopt; }
    String extendedDescription() const final { return stringAttributeValue(AXProperty::ExtendedDescription); }
    String computedRoleString() const final;
    bool isValueAutofillAvailable() const final { return boolAttributeValue(AXProperty::IsValueAutofillAvailable); }
    AutoFillButtonType valueAutofillButtonType() const final { return static_cast<AutoFillButtonType>(intAttributeValue(AXProperty::ValueAutofillButtonType)); }
    URL url() const final { return urlAttributeValue(AXProperty::URL); }
    String accessKey() const final { return stringAttributeValueNullIfMissing(AXProperty::AccessKey); }
    String localizedActionVerb() const final { return stringAttributeValue(AXProperty::LocalizedActionVerb); }
    String actionVerb() const final { return stringAttributeValue(AXProperty::ActionVerb); }
    String explicitAutoCompleteValue() const final { return stringAttributeValue(AXProperty::ExplicitAutoCompleteValue); }
    bool isMathElement() const final { return boolAttributeValue(AXProperty::IsMathElement); }
    bool isMathFraction() const final { return boolAttributeValue(AXProperty::IsMathFraction); }
    bool isMathFenced() const final { return boolAttributeValue(AXProperty::IsMathFenced); }
    bool isMathSubscriptSuperscript() const final { return boolAttributeValue(AXProperty::IsMathSubscriptSuperscript); }
    bool isMathRow() const final { return boolAttributeValue(AXProperty::IsMathRow); }
    bool isMathUnderOver() const final { return boolAttributeValue(AXProperty::IsMathUnderOver); }
    bool isMathRoot() const final { return boolAttributeValue(AXProperty::IsMathRoot); }
    bool isMathSquareRoot() const final { return boolAttributeValue(AXProperty::IsMathSquareRoot); }
    bool isMathTable() const final { return boolAttributeValue(AXProperty::IsMathTable); }
    bool isMathTableRow() const final { return boolAttributeValue(AXProperty::IsMathTableRow); }
    bool isMathTableCell() const final { return boolAttributeValue(AXProperty::IsMathTableCell); }
    bool isMathMultiscript() const final { return boolAttributeValue(AXProperty::IsMathMultiscript); }
    bool isMathToken() const final { return boolAttributeValue(AXProperty::IsMathToken); }
    bool isAnonymousMathOperator() const final { return boolAttributeValue(AXProperty::IsAnonymousMathOperator); }
    std::optional<AccessibilityChildrenVector> mathRadicand() final;
    AXIsolatedObject* mathRootIndexObject() final { return objectAttributeValue(AXProperty::MathRootIndexObject); }
    AXIsolatedObject* mathUnderObject() final { return objectAttributeValue(AXProperty::MathUnderObject); }
    AXIsolatedObject* mathOverObject() final { return objectAttributeValue(AXProperty::MathOverObject); }
    AXIsolatedObject* mathNumeratorObject() final { return objectAttributeValue(AXProperty::MathNumeratorObject); }
    AXIsolatedObject* mathDenominatorObject() final { return objectAttributeValue(AXProperty::MathDenominatorObject); }
    AXIsolatedObject* mathBaseObject() final { return objectAttributeValue(AXProperty::MathBaseObject); }
    AXIsolatedObject* mathSubscriptObject() final { return objectAttributeValue(AXProperty::MathSubscriptObject); }
    AXIsolatedObject* mathSuperscriptObject() final { return objectAttributeValue(AXProperty::MathSuperscriptObject); }
    String mathFencedOpenString() const final { return stringAttributeValue(AXProperty::MathFencedOpenString); }
    String mathFencedCloseString() const final { return stringAttributeValue(AXProperty::MathFencedCloseString); }
    int mathLineThickness() const final { return intAttributeValue(AXProperty::MathLineThickness); }
    void mathPrescripts(AccessibilityMathMultiscriptPairs&) final;
    void mathPostscripts(AccessibilityMathMultiscriptPairs&) final;
#if PLATFORM(COCOA)
    Style::SpeakAs speakAs() const final { return speakAsAttributeValue(AXProperty::SpeakAs); }
#endif
#if PLATFORM(MAC)
    bool caretBrowsingEnabled() const final { return boolAttributeValue(AXProperty::CaretBrowsingEnabled); }
    AccessibilityChildrenVector allSortedLiveRegions() const final;
    AccessibilityChildrenVector allSortedNonRootWebAreas() const final;
#endif
    AXIsolatedObject* focusableAncestor() final { return Accessibility::focusableAncestor(*this); }
    AXIsolatedObject* highestEditableAncestor() final { return Accessibility::highestEditableAncestor(*this); }
    std::optional<AccessibilityOrientation> explicitOrientation() const { return optionalAttributeValue<AccessibilityOrientation>(AXProperty::ExplicitOrientation); }
    unsigned ariaLevel() const final { return unsignedAttributeValue(AXProperty::ARIALevel); }
    String language() const final { return stringAttributeValue(AXProperty::Language); }
    void setSelectedChildren(const AccessibilityChildrenVector&) final;
    AccessibilityChildrenVector visibleChildren() final { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXProperty::VisibleChildren)); }
    void setChildrenIDs(Vector<AXID>&&);
    AXIsolatedObject* liveRegionAncestor(bool excludeIfOff = true) const final { return Accessibility::liveRegionAncestor(*this, excludeIfOff); }
    const String explicitLiveRegionStatus() const final { return stringAttributeValue(AXProperty::ExplicitLiveRegionStatus); }
    const String explicitLiveRegionRelevant() const final { return stringAttributeValue(AXProperty::ExplicitLiveRegionRelevant); }
    bool liveRegionAtomic() const final { return boolAttributeValue(AXProperty::LiveRegionAtomic); }
    bool isBusy() const final { return boolAttributeValue(AXProperty::IsBusy); }
    bool isInlineText() const final { return boolAttributeValue(AXProperty::IsInlineText); }
    // Spin button support.
    AXIsolatedObject* incrementButton() final { return objectAttributeValue(AXProperty::IncrementButton); }
    AXIsolatedObject* decrementButton() final { return objectAttributeValue(AXProperty::DecrementButton); }
    AccessibilityChildrenVector documentLinks() final { return tree()->objectsForIDs(vectorAttributeValue<AXID>(AXProperty::DocumentLinks)); }
    bool supportsCheckedState() const final { return boolAttributeValue(AXProperty::SupportsCheckedState); }

    String stringValue() const final;
    std::optional<String> platformStringValue() const;

    // Parameterized attribute retrieval.
    Vector<SimpleRange> findTextRanges(const AccessibilitySearchTextCriteria&) const final;
    Vector<String> performTextOperation(const AccessibilityTextOperation&) final;
    AccessibilityChildrenVector findMatchingObjects(AccessibilitySearchCriteria&&) final;

#if PLATFORM(COCOA)
    bool preventKeyboardDOMEventDispatch() const final { return boolAttributeValue(AXProperty::PreventKeyboardDOMEventDispatch); }
#endif

    // CharacterRange support.
    CharacterRange selectedTextRange() const final { return propertyValue<CharacterRange>(AXProperty::SelectedTextRange); }
    int insertionPointLineNumber() const final;
    CharacterRange doAXRangeForLine(unsigned) const final;
    String doAXStringForRange(const CharacterRange&) const final;
    CharacterRange characterRangeForPoint(const IntPoint&) const final;
    CharacterRange doAXRangeForIndex(unsigned) const final;
    CharacterRange doAXStyleRangeForIndex(unsigned) const final;
    IntRect doAXBoundsForRangeUsingCharacterOffset(const CharacterRange&) const final;
    IntRect doAXBoundsForRange(const CharacterRange&) const final;
    unsigned doAXLineForIndex(unsigned) final;

    VisibleSelection selection() const final;
    void setSelectedVisiblePositionRange(const VisiblePositionRange&) const final;

    std::optional<SimpleRange> simpleRange() const final;
    VisiblePositionRange visiblePositionRange() const final;

    String selectedText() const final;
    VisiblePositionRange visiblePositionRangeForLine(unsigned) const final;
    VisiblePositionRange visiblePositionRangeForUnorderedPositions(const VisiblePosition&, const VisiblePosition&) const final;
    VisiblePositionRange leftLineVisiblePositionRange(const VisiblePosition&) const final;
    VisiblePositionRange rightLineVisiblePositionRange(const VisiblePosition&) const final;
    VisiblePositionRange styleRangeForPosition(const VisiblePosition&) const final;
    VisiblePositionRange lineRangeForPosition(const VisiblePosition&) const final;
    std::optional<SimpleRange> rangeForCharacterRange(const CharacterRange&) const final;
#if PLATFORM(COCOA)
    AXTextMarkerRange textMarkerRangeForNSRange(const NSRange&) const final;
#endif
#if PLATFORM(MAC)
    AXTextMarkerRange selectedTextMarkerRange() const final;
#endif
    IntRect boundsForRange(const SimpleRange&) const final;
    VisiblePosition visiblePositionForPoint(const IntPoint&) const final;
    VisiblePosition nextLineEndPosition(const VisiblePosition&) const final;
    VisiblePosition previousLineStartPosition(const VisiblePosition&) const final;
    VisiblePosition visiblePositionForIndex(unsigned, bool lastIndexOK) const final;
    VisiblePosition visiblePositionForIndex(int) const final;
    int indexForVisiblePosition(const VisiblePosition&) const final;
    int lineForPosition(const VisiblePosition&) const final;

    // Attribute setters.
    void setARIAGrabbed(bool) final;
    void setIsExpanded(bool) final;
    bool setValue(float) final;
    void setValueIgnoringResult(float) final;
    void setSelected(bool) final;
    void setSelectedRows(AccessibilityChildrenVector&&) final;
    void setFocused(bool) final;
    void setSelectedText(const String&) final;
    void setSelectedTextRange(CharacterRange&&) final;
    bool setValue(const String&) final;
    void setValueIgnoringResult(const String&) final;
#if PLATFORM(MAC)
    void setCaretBrowsingEnabled(bool) final;
#endif
#if PLATFORM(COCOA)
    void setPreventKeyboardDOMEventDispatch(bool) final;
    std::optional<NSRange> visibleCharacterRange() const final;
#endif

    String textUnderElement(TextUnderElementMode = { }) const final;
    std::optional<SimpleRange> misspellingRange(const SimpleRange&, AccessibilitySearchDirection) const final;
    FloatRect convertFrameToSpace(const FloatRect&, AccessibilityConversionSpace) const final;
    void increment() final;
    void decrement() final;
    bool performDismissAction() final;
    void performDismissActionIgnoringResult() final;
    void scrollToMakeVisible() const final;
    void scrollToMakeVisibleWithSubFocus(IntRect&&) const final;
    void scrollToGlobalPoint(IntPoint&&) const final;
    bool replaceTextInRange(const String&, const CharacterRange&) final;
    bool insertText(const String&) final;
    bool press() final;

    bool isAccessibilityObject() const final { return false; }

    // Functions that should never be called on an isolated tree object. ASSERT that these are not reached;
    bool isAccessibilityRenderObject() const final;
    bool isAccessibilityNodeObject() const final;
    bool isAXLocalFrame() const final { return false; }
    bool isAXRemoteFrame() const final { return false; }
    bool isNativeTextControl() const final;
    bool isMockObject() const final;
    bool isNonNativeTextControl() const final;
    bool isIndeterminate() const final { return boolAttributeValue(AXProperty::IsIndeterminate); }
    bool isLoaded() const final { return loadingProgress() >= 1; }
    bool isOnScreen() const final;
    bool isOffScreen() const final;
    bool isPressed() const final;
    bool isNonLayerSVGObject() const { return boolAttributeValue(AXProperty::IsNonLayerSVGObject); }
    // FIXME: isVisible should be accurate for all objects, not just widgets, on COCOA.
    bool isVisible() const final { return boolAttributeValue(AXProperty::IsVisible); }
    bool isSelectedOptionActive() const final;
    bool hasBoldFont() const final { return boolAttributeValue(AXProperty::HasBoldFont); }
    bool hasItalicFont() const final { return boolAttributeValue(AXProperty::HasItalicFont); }
    Vector<AXTextMarkerRange> misspellingRanges() const final;
    bool hasPlainText() const final { return boolAttributeValue(AXProperty::HasPlainText); }
    bool hasSameFont(AXCoreObject&) final;
    bool hasSameFontColor(AXCoreObject&) final;
    bool hasSameStyle(AXCoreObject&) final;
    bool hasUnderline() const final { return colorAttributeValue(AXProperty::UnderlineColor) != Accessibility::defaultColor(); }
    AXTextMarkerRange textInputMarkedTextMarkerRange() const final;
    Element* element() const final;
    Node* node() const final;
    RenderObject* renderer() const final;

    AccessibilityChildrenVector relatedObjects(AXRelation) const final;

    bool supportsHasPopup() const final;
    bool supportsChecked() const final;
    bool isModalNode() const final;
    bool isDescendantOfRole(AccessibilityRole) const final;
    bool inheritsPresentationalRole() const final;
    void setAccessibleName(const AtomString&) final;

    String textContentPrefixFromListMarker() const final;
    String webAreaTitle() const final { return stringAttributeValue(AXProperty::WebAreaTitle); }
    String titleAttribute() const final { return stringAttributeValue(AXProperty::TitleAttribute); }

    std::optional<String> textContent() const final;
    bool isBlockFlow() const final { return boolAttributeValue(AXProperty::IsBlockFlow); }
    StitchState stitchState(IncludeStitchGroup = IncludeStitchGroup::Yes) const final;
    const Vector<Vector<AXID>>* stitchGroupsView() const;

    String text() const final;
    unsigned textLength() const final;
    String revealableText() const final { return stringAttributeValue(AXProperty::RevealableText); }
#if PLATFORM(COCOA)
    RetainPtr<NSAttributedString> attributedStringForTextMarkerRange(AXTextMarkerRange&&, SpellCheck) const final;
#endif
    AXObjectCache* axObjectCache() const;
    Element* actionElement() const final;
    Path elementPath() const final { return pathAttributeValue(AXProperty::Path); };
    bool supportsPath() const final { return boolAttributeValue(AXProperty::SupportsPath); }

    bool isWidget() const final
    {
        // Plugins are a widget subclass.
        return boolAttributeValue(AXProperty::IsPlugin) || boolAttributeValue(AXProperty::IsWidget);
    }
    Widget* widget() const final;
    PlatformWidget platformWidget() const final;
    Widget* widgetForAttachmentView() const final;
    bool isPlugin() const final { return boolAttributeValue(AXProperty::IsPlugin); }

#if PLATFORM(COCOA)
    RetainPtr<RemoteAXObjectRef> remoteParent() const final;
    FloatRect convertRectToPlatformSpace(const FloatRect&, AccessibilityConversionSpace) const final;
#endif
    Page* page() const final;
    Document* document() const final;
    LocalFrameView* documentFrameView() const final;
    void detachFromParent() final;

    bool isInDescriptionListTerm() const final;

    String nameAttribute() const final { return stringAttributeValue(AXProperty::NameAttribute); }
    bool hasElementName(ElementName name) const final { return elementName() == name; };
    ElementName elementName() const final { return propertyValue<ElementName>(AXProperty::ElementName); }
#if PLATFORM(COCOA)
    bool hasApplePDFAnnotationAttribute() const final { return boolAttributeValue(AXProperty::HasApplePDFAnnotationAttribute); }
    RetainPtr<id> remoteFramePlatformElement() const final;
#endif
    bool hasRemoteFrameChild() const final { return boolAttributeValue(AXProperty::HasRemoteFrameChild); }

#if ENABLE(MODEL_ELEMENT_ACCESSIBILITY)
    ModelPlayerAccessibilityChildren modelElementChildren() final;
#endif

    void updateBackingStore() final;

    String innerHTML() const final;
    String outerHTML() const final;

#ifndef NDEBUG
    void verifyChildrenIndexInParent() const final { return AXCoreObject::verifyChildrenIndexInParent(m_children); }
#endif

    // IDs that haven't been resolved into actual objects in m_children.
    FixedVector<AXID> m_unresolvedChildrenIDs;
    Vector<Ref<AXCoreObject>> m_children;
    AXPropertyVector m_properties;

    // FIXME: Make this a ThreadSafeWeakPtr<AXIsolatedTree>.
    const Ref<AXIsolatedTree> m_tree;
    Markable<AXID> m_parentID;

    OptionSet<AXPropertyFlag> m_propertyFlags;

#if !PLATFORM(COCOA)
    PlatformWidget m_platformWidget;
#endif
};

void appendPlatformProperties(AXPropertyVector&, OptionSet<AXPropertyFlag>&, const Ref<AccessibilityObject>&);
void appendBasePlatformProperties(AXPropertyVector&, OptionSet<AXPropertyFlag>&, const Ref<AccessibilityObject>&);

template<typename T>
inline T AXIsolatedObject::propertyValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound)
        return { };

    return WTF::switchOn(m_properties[index].second,
        [] (const T& typedValue) { return typedValue; },
        [] (auto&) { ASSERT_NOT_REACHED();
            return T(); }
    );
}

inline void AXIsolatedObject::setPropertyFlag(AXPropertyFlag flag, bool set)
{
    if (set)
        m_propertyFlags.add(flag);
    else
        m_propertyFlags.remove(flag);
}

inline bool AXIsolatedObject::hasPropertyFlag(AXPropertyFlag flag) const
{
    return m_propertyFlags.contains(flag);
}

inline bool AXIsolatedObject::hasPropertyFlag(AXProperty property) const
{
    ASSERT(static_cast<uint16_t>(property) <= lastPropertyFlagIndex);
    uint16_t propertyIndex = static_cast<uint16_t>(property);
    return hasPropertyFlag(static_cast<AXPropertyFlag>(1 << propertyIndex));
}

bool isDefaultValue(AXProperty, AXPropertyValueVariant&);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AXIsolatedObject, isAXIsolatedObjectInstance())

#endif // ENABLE((ACCESSIBILITY_ISOLATED_TREE))
