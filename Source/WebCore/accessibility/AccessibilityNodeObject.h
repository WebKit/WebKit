/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#pragma once

#include "AXObjectRareData.h"
#include "AXTableHelpers.h"
#include "AXUtilities.h"
#include "AccessibilityObject.h"
#include "LayoutRect.h"
#include "RenderStyleConstants.h"
#include <wtf/Forward.h>

namespace WebCore {

class AXObjectCache;
class Element;
class HTMLLabelElement;
class Node;

class AccessibilityNodeObject : public AccessibilityObject {
public:
    static Ref<AccessibilityNodeObject> create(AXID, Node*, AXObjectCache&);
    virtual ~AccessibilityNodeObject();

    void init() override;
    void recomputeAriaRole() final { m_ariaRole = determineAriaRoleAttribute(); }

    bool hasElementDescendant() const final;

    bool isBusy() const final;
    bool isDetached() const override { return !m_node; }
    bool isFieldset() const final;
    bool isAccessibilityList() const final;
    bool isUnorderedList() const final;
    bool isOrderedList() const final;
    bool isDescriptionList() const final;
    bool isMultiSelectable() const override;
    bool isNativeImage() const;
    bool isNativeTextControl() const final;
    bool isSecureField() const final;
    bool isSearchField() const final;

    bool isChecked() const final;
    bool isEnabled() const override;
    bool isIndeterminate() const override;
    bool isPressed() const final;
    bool isRequired() const final;
    bool supportsARIAOwns() const final;

    bool supportsDropping() const final;
    bool supportsDragging() const final;
    bool isGrabbed() final;
    Vector<String> determineDropEffects() const final;

    bool canSetSelectedAttribute() const override;

    Node* node() const final { return m_node.get(); }
    CheckedPtr<Node> checkedNode() const { return node(); }
    Document* document() const override;
    LocalFrameView* documentFrameView() const override;

    // Start table-related methods.
    bool isTable() const final;
    bool isAriaTable() const final { return AXTableHelpers::isTableRole(ariaRoleAttribute()); }
    // isTable() check is last because it's the most expensive.
    bool isExposableTable() const final { return hasRareData() && rareData()->isExposableTable() && isTable(); }

    void recomputeIsExposableIfNecessary() final;

    AccessibilityChildrenVector columns() final;
    AccessibilityChildrenVector rows() final;

    unsigned columnCount() final;
    unsigned rowCount() final;

    AccessibilityChildrenVector cells() final;
    AccessibilityObject* cellForColumnAndRow(unsigned column, unsigned row) final;

    AccessibilityChildrenVector rowHeaders() override;
    AccessibilityChildrenVector visibleRows() final;

    // Returns an object that contains, as children, all the objects that act as headers.
    AccessibilityObject* tableHeaderContainer() final;

    int axColumnCount() const final;
    int axRowCount() const final;

    Vector<Vector<Markable<AXID>>> cellSlots() final;
    void setCellSlotsDirty();
    // End table-related methods.

    // Start table-row-related methods.
    // FIXME: this method (and all references) should be renamed to something more accurate, like "containingTable".
    AccessibilityObject* parentTable() const final;
    void setRowIndex(unsigned);
    unsigned rowIndex() const final { return hasRareData() ? rareData()->rowIndex() : 0; }

    std::optional<unsigned> axColumnIndex() const override;
    std::optional<unsigned> axRowIndex() const override;
    String axRowIndexText() const final;

    AccessibilityChildrenVector disclosedRows() final;
    AccessibilityObject* disclosedByRow() const final;

    AXCoreObject* parentTableIfExposedTableRow() const final;
    bool isExposedTableRow() const final { return parentTableIfExposedTableRow(); }
    bool isTableRow() const final;
    // End table-row-related methods.

    // Start table-cell-related methods.
    bool isTableCell() const final;
    bool isARIAGridCell() const;
    bool isExposedTableCell() const final;
    AccessibilityObject* parentTableIfTableCell() const final;
    bool isTableHeaderCell() const;
    bool isColumnHeader() const final;
    bool isRowHeader() const final;
    std::pair<unsigned, unsigned> rowIndexRange() const final;
    std::pair<unsigned, unsigned> columnIndexRange() const final;
    String axColumnIndexText() const final;
    unsigned colSpan() const;
    unsigned rowSpan() const;
    void incrementEffectiveRowSpan();
    void resetEffectiveRowSpan();
    void setAXColIndexFromRow(int);
    void setColumnIndex(unsigned);
#if USE(ATSPI)
    int axColumnSpan() const;
    int axRowSpan() const;
#endif
    // End table-cell-related methods.

    void setFocused(bool) override;
    bool isFocused() const final;
    bool canSetFocusAttribute() const override;

    bool canSetValueAttribute() const override;

    String valueDescription() const override;
    float valueForRange() const override;
    float maxValueForRange() const override;
    float minValueForRange() const override;
    float stepValueForRange() const override;

#if ENABLE(ATTACHMENT_ELEMENT)
    bool hasProgress() const final;
#endif

    std::optional<AccessibilityOrientation> orientationFromARIA() const;
    std::optional<AccessibilityOrientation> explicitOrientation() const override { return orientationFromARIA(); }

    AccessibilityButtonState checkboxOrRadioValue() const final;

    URL url() const override;
    String textUnderElement(TextUnderElementMode = TextUnderElementMode()) const override;
    String accessibilityDescriptionForChildren() const;
    String description() const override;
    String helpText() const override;
    String revealableText() const final;
    bool isHiddenUntilFoundContainer() const final;
    String text() const final;
    void alternativeText(Vector<AccessibilityText>&) const;
    void helpText(Vector<AccessibilityText>&) const;
    String stringValue() const override;

    bool isBlockFlow() const final;
    StitchState stitchState(IncludeStitchGroup = IncludeStitchGroup::Yes) const final;
    Vector<Vector<AXID>> stitchGroups() const final;

    WallTime dateTimeValue() const final;
    SRGBA<uint8_t> colorValue() const final;
    String ariaLabeledByAttribute() const final;
    bool hasAccNameAttribute() const;
    bool hasAttributesRequiredForInclusion() const final;
    bool hasClickHandler() const final;
    bool hasCursorPointer() const final
    {
        CheckedPtr style = this->style();
        return style && style->cursorType() == CursorType::Pointer && style->pointerEvents() != PointerEvents::None;
    }
    bool showsCursorOnHover() const final;
    bool hasPointerEventsNone() const final;

    void setIsExpanded(bool) final;

    Element* actionElement() const override;
    Element* anchorElement() const override;
    RefPtr<Element> popoverTargetElement() const final;
    RefPtr<Element> commandForElement() const final;
    CommandType commandType() const final;
    AccessibilityObject* internalLinkElement() const final;
    AccessibilityChildrenVector radioButtonGroup() const final;

    virtual void changeValueByPercent(float percentChange);

    AccessibilityObject* firstChild() const override;
    AccessibilityObject* lastChild() const override;
    AccessibilityObject* previousSibling() const override;
    AccessibilityObject* nextSibling() const override;
    AccessibilityObject* parentObject() const override;

    bool matchesTextAreaRole() const;

    void increment() override;
    void decrement() override;
    bool toggleDetailsAncestor() final;
    void revealAncestors() final;

    LayoutRect elementRect() const override;
    Path elementPath() const override;
    bool supportsPath() const override { return isImageMapLink(); }

    bool isLabelContainingOnlyStaticText() const;
    bool isNativeLabel() const override;

#if ENABLE(AX_THREAD_TEXT_APIS)
    TextEmissionBehavior textEmissionBehavior() const final;
#endif

protected:
    explicit AccessibilityNodeObject(AXID, Node*, AXObjectCache&);
    void detachRemoteParts(AccessibilityDetachmentType) override;

    AccessibilityRole m_ariaRole { AccessibilityRole::Unknown };

    // FIXME: These `is_` member variables should be replaced with an enum or be computed on demand.
    // Only used by AccessibilitySVGObject, but placed here to use space that would otherwise be taken by padding.
    bool m_isSVGRoot { false };

    // Only used by isNativeLabel() objects. Placed here to use space that would otherwise be taken by padding.
    mutable bool m_containsOnlyStaticTextDirty { false };
    mutable bool m_containsOnlyStaticText { false };

#ifndef NDEBUG
    bool m_initialized { false };
#endif

    AccessibilityRole determineAccessibilityRole() override;
    AccessibilityRole determineListRoleWithCleanChildren();
    enum class TreatStyleFormatGroupAsInline : bool { No, Yes };
    AccessibilityRole determineAccessibilityRoleFromNode(TreatStyleFormatGroupAsInline = TreatStyleFormatGroupAsInline::No) const;
    AccessibilityRole roleFromInputElement(const HTMLInputElement&) const;
    AccessibilityRole ariaRoleAttribute() const final { return m_ariaRole; }
    virtual AccessibilityRole determineAriaRoleAttribute() const;
    AccessibilityRole remapAriaRoleDueToParent(AccessibilityRole) const;

    bool computeIsIgnored() const override;
    void addChildren() override;
    void clearChildren() override;
    void updateChildrenIfNecessary() override;
    bool canHaveChildren() const override;
    void setSelectedChildren(const AccessibilityChildrenVector&) final;
    AccessibilityChildrenVector visibleChildren() override;
    bool isDescendantOfBarrenParent() const final;
    void updateOwnedChildrenIfNecessary();
    AccessibilityObject* ownerParentObject() const;

    enum class StepAction : bool { Decrement, Increment };
    void alterRangeValue(StepAction);
    void changeValueByStep(StepAction);
    // This returns true if it's focusable but it's not content editable and it's not a control or ARIA control.
    bool isGenericFocusableElement() const;

    VisiblePositionRange visiblePositionRange() const final;
    VisiblePositionRange selectedVisiblePositionRange() const final;
    VisiblePositionRange visiblePositionRangeForLine(unsigned) const final;
    VisiblePosition visiblePositionForIndex(int) const override;
    int indexForVisiblePosition(const VisiblePosition&) const override;

    bool elementAttributeValue(const QualifiedName&) const;

    const String explicitLiveRegionStatus() const final { return getAttribute(HTMLNames::aria_liveAttr); }
    const String explicitLiveRegionRelevant() const final { return getAttribute(HTMLNames::aria_relevantAttr); }
    bool liveRegionAtomic() const final;

    String accessKey() const final;
    bool isLabelable() const;
    AccessibilityObject* controlForLabelElement() const final;
    String textAsLabelFor(const AccessibilityObject&) const;
    String textForLabelElements(Vector<Ref<HTMLElement>>&&) const;
    HTMLLabelElement* labelElementContainer() const;

    String ariaAccessibilityDescription() const;
    Vector<Ref<Element>> ariaLabeledByElements() const;
    String descriptionForElements(const Vector<Ref<Element>>&) const;
    LayoutRect boundingBoxRect() const override;
    LayoutRect nonEmptyAncestorBoundingBox() const;
    String ariaDescribedByAttribute() const final;

    AccessibilityObject* captionForFigure() const;
    virtual void labelText(Vector<AccessibilityText>&) const;

#if PLATFORM(IOS_FAMILY)
    HTMLMediaElement* mediaElement() const;
    HTMLVideoElement* videoElement() const;
#endif
    void addTableChildrenAndCellSlots();

    // Start of table-cell-related methods.
    AccessibilityNodeObject* parentRow() const;
    // End of table-cell-related methods.

    bool isValidTree() const;
private:
    bool isAccessibilityNodeObject() const final { return true; }
    void accessibilityText(Vector<AccessibilityText>&) const override;
    void visibleText(Vector<AccessibilityText>&) const;
    String alternativeTextForWebArea() const;
    void ariaLabeledByText(Vector<AccessibilityText>&) const;
    bool usesAltForTextComputation() const;
    bool roleIgnoresTitle() const;
    bool postKeyboardKeysForValueChange(StepAction);
    void setNodeValue(StepAction, float);
    bool performDismissAction() final;
    bool hasTextAlternative() const;
    LayoutRect checkboxOrRadioRect() const;

    void setNeedsToUpdateChildren() override { m_childrenDirty = true; }
    bool needsToUpdateChildren() const final { return m_childrenDirty; }
    void setNeedsToUpdateSubtree() final { m_subtreeDirty = true; }

    bool isDescendantOfElementType(const HashSet<QualifiedName>&) const;

    AXObjectRareData* rareDataWithCleanTableChildren();
    // Returns the number of columns the table should have.
    unsigned computeCellSlots();
    // isDataTable / computeIsTableExposableThroughAccessibility perform heuristics to determine
    // if a table should be exposed as a "semantic" data table in the accessibility API, or if
    // this table is just used for layout and thus is not a "real" table.
    bool computeIsTableExposableThroughAccessibility() const { return isAriaTable() || isDataTable(); }
    bool isDataTable() const;
    void updateRowDescendantRoles();

    // Start of private table-row-related methods.
    bool isARIAGridRow() const final;
    bool isARIATreeGridRow() const final;
    // End of private table-row-related methods.

    // Start of private table-cell-related methods.
    void ensureIndexesUpToDate() const;
    // End of private table-cell-related methods.

protected:
    WeakPtr<Node, WeakPtrImplWithEventTargetData> m_node;
};

namespace Accessibility {

RefPtr<HTMLElement> controlForLabelElement(const HTMLLabelElement&);
Vector<Ref<HTMLElement>> labelsForElement(Element*);

} // namespace Accessibility

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(AccessibilityNodeObject, isAccessibilityNodeObject())
