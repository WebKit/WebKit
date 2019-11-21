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

#include "AXIsolatedTree.h"
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
    static Ref<AXIsolatedObject> create(AXCoreObject&);
    ~AXIsolatedObject();

    void setObjectID(AXID id) override { m_id = id; }
    AXID objectID() const override { return m_id; }
    void init() override { }

    AccessibilityObjectWrapper* wrapper() const override { return m_wrapper.get(); }
    void detach(AccessibilityDetachmentType, AXObjectCache* = nullptr) override { }
    bool isDetached() const override { return false; }

    void setTreeIdentifier(AXIsolatedTreeID);
    void setParent(AXID);
    void appendChild(AXID);

private:
    bool isAccessibilityObject() const override { return false; }
    bool isAccessibilityNodeObject() const override { return false; }
    bool isAccessibilityRenderObject() const override { return false; }
    bool isAccessibilityScrollbar() const override { return false; }
    bool isAccessibilityScrollView() const override { return false; }
    bool isAccessibilitySVGRoot() const override { return false; }
    bool isAccessibilitySVGElement() const override { return false; }

    bool containsText(String *) const override { return false; }
    bool isAttachmentElement() const override { return false; }
    bool isHeading() const override { return false; }
    bool isLink() const override { return boolAttributeValue(AXPropertyName::IsLink); }
    bool isImage() const override { return boolAttributeValue(AXPropertyName::IsImage); }
    bool isImageMap() const override { return false; }
    bool isNativeImage() const override { return false; }
    bool isImageButton() const override { return false; }
    bool isPasswordField() const override { return false; }
    bool isContainedByPasswordField() const override { return false; }
    AXCoreObject* passwordFieldOrContainingPasswordField() override { return nullptr; }
    bool isNativeTextControl() const override { return false; }
    bool isSearchField() const override { return false; }
    bool isWebArea() const override { return false; }
    bool isCheckbox() const override { return false; }
    bool isRadioButton() const override { return false; }
    bool isNativeListBox() const override { return false; }
    bool isListBox() const override { return false; }
    bool isListBoxOption() const override { return false; }
    bool isAttachment() const override { return boolAttributeValue(AXPropertyName::IsAttachment); }
    bool isMediaTimeline() const override { return false; }
    bool isMenuRelated() const override { return false; }
    bool isMenu() const override { return false; }
    bool isMenuBar() const override { return false; }
    bool isMenuButton() const override { return false; }
    bool isMenuItem() const override { return false; }
    bool isFileUploadButton() const override { return boolAttributeValue(AXPropertyName::IsFileUploadButton); }
    bool isInputImage() const override { return false; }
    bool isProgressIndicator() const override { return false; }
    bool isSlider() const override { return false; }
    bool isSliderThumb() const override { return false; }
    bool isInputSlider() const override { return false; }
    bool isControl() const override { return false; }
    bool isLabel() const override { return false; }
    bool isList() const override { return false; }
    bool isTable() const override { return false; }
    bool isDataTable() const override { return false; }
    bool isTableRow() const override { return false; }
    bool isTableColumn() const override { return false; }
    bool isTableCell() const override { return false; }
    bool isFieldset() const override { return false; }
    bool isGroup() const override { return false; }
    bool isARIATreeGridRow() const override { return false; }
    bool isImageMapLink() const override { return boolAttributeValue(AXPropertyName::IsImageMapLink); }
    bool isMenuList() const override { return false; }
    bool isMenuListPopup() const override { return false; }
    bool isMenuListOption() const override { return false; }
    bool isSpinButton() const override { return false; }
    bool isNativeSpinButton() const override { return false; }
    bool isSpinButtonPart() const override { return false; }
    bool isMockObject() const override { return false; }
    bool isMediaControlLabel() const override { return boolAttributeValue(AXPropertyName::IsMediaControlLabel); }
    bool isMediaObject() const override { return false; }
    bool isSwitch() const override { return false; }
    bool isToggleButton() const override { return false; }
    bool isTextControl() const override { return false; }
    bool isARIATextControl() const override { return false; }
    bool isNonNativeTextControl() const override { return false; }
    bool isTabList() const override { return false; }
    bool isTabItem() const override { return false; }
    bool isRadioGroup() const override { return false; }
    bool isComboBox() const override { return false; }
    bool isTree() const override { return boolAttributeValue(AXPropertyName::IsTree); }
    bool isTreeGrid() const override { return false; }
    bool isTreeItem() const override { return boolAttributeValue(AXPropertyName::IsTreeItem); }
    bool isScrollbar() const override { return boolAttributeValue(AXPropertyName::IsScrollbar); }
    bool isButton() const override { return false; }
    bool isListItem() const override { return false; }
    bool isCheckboxOrRadio() const override { return false; }
    bool isScrollView() const override { return false; }
    bool isCanvas() const override { return false; }
    bool isPopUpButton() const override { return false; }
    bool isBlockquote() const override { return false; }
    bool isLandmark() const override { return false; }
    bool isColorWell() const override { return false; }
    bool isRangeControl() const override { return false; }
    bool isMeter() const override { return false; }
    bool isSplitter() const override { return false; }
    bool isToolbar() const override { return false; }
    bool isStyleFormatGroup() const override { return false; }
    bool isFigureElement() const override { return false; }
    bool isKeyboardFocusable() const override { return false; }
    bool isSummary() const override { return false; }
    bool isOutput() const override { return false; }

    bool isChecked() const override { return boolAttributeValue(AXPropertyName::IsChecked); }
    bool isEnabled() const override { return boolAttributeValue(AXPropertyName::IsEnabled); }
    bool isSelected() const override { return boolAttributeValue(AXPropertyName::IsSelected); }
    bool isFocused() const override { return boolAttributeValue(AXPropertyName::IsFocused); }
    bool isHovered() const override { return boolAttributeValue(AXPropertyName::IsHovered); }
    bool isIndeterminate() const override { return boolAttributeValue(AXPropertyName::IsIndeterminate); }
    bool isLoaded() const override { return boolAttributeValue(AXPropertyName::IsLoaded); }
    bool isMultiSelectable() const override { return boolAttributeValue(AXPropertyName::IsMultiSelectable); }
    bool isOnScreen() const override { return boolAttributeValue(AXPropertyName::IsOnScreen); }
    bool isOffScreen() const override { return boolAttributeValue(AXPropertyName::IsOffScreen); }
    bool isPressed() const override { return boolAttributeValue(AXPropertyName::IsPressed); }
    bool isUnvisited() const override { return boolAttributeValue(AXPropertyName::IsUnvisited); }
    bool isVisited() const override { return boolAttributeValue(AXPropertyName::IsVisited); }
    bool isRequired() const override { return boolAttributeValue(AXPropertyName::IsRequired); }
    bool supportsRequiredAttribute() const override { return boolAttributeValue(AXPropertyName::SupportsRequiredAttribute); }
    bool isLinked() const override { return boolAttributeValue(AXPropertyName::IsLinked); }
    bool isExpanded() const override { return boolAttributeValue(AXPropertyName::IsExpanded); }
    bool isVisible() const override { return boolAttributeValue(AXPropertyName::IsVisible); }
    bool isCollapsed() const override { return boolAttributeValue(AXPropertyName::IsCollapsed); }
    FloatRect relativeFrame() const override { return rectAttributeValue<FloatRect>(AXPropertyName::RelativeFrame); }
    FloatRect convertFrameToSpace(const FloatRect&, AccessibilityConversionSpace) const override { return FloatRect(); }

    bool isSelectedOptionActive() const override { return boolAttributeValue(AXPropertyName::IsSelectedOptionActive); }

    void setIsExpanded(bool) override { }
    bool hasBoldFont() const override { return false; }
    bool hasItalicFont() const override { return false; }
    bool hasMisspelling() const override { return false; }
    RefPtr<Range> getMisspellingRange(RefPtr<Range> const&, AccessibilitySearchDirection) const override { return nullptr; }
    bool hasPlainText() const override { return false; }
    bool hasSameFont(RenderObject*) const override { return false; }
    bool hasSameFontColor(RenderObject*) const override { return false; }
    bool hasSameStyle(RenderObject*) const override { return false; }
    bool isStaticText() const override { return false; }
    bool hasUnderline() const override { return false; }
    bool hasHighlighting() const override { return false; }

    bool supportsDatetimeAttribute() const override { return false; }
    const AtomString& datetimeAttributeValue() const override { return nullAtom(); }

    bool canSetFocusAttribute() const override { return false; }
    bool canSetTextRangeAttributes() const override { return false; }
    bool canSetValueAttribute() const override { return false; }
    bool canSetNumericValue() const override { return false; }
    bool canSetSelectedAttribute() const override { return false; }
    bool canSetSelectedChildrenAttribute() const override { return false; }
    bool canSetExpandedAttribute() const override { return false; }

    Element* element() const override { return nullptr; }
    Node* node() const override { return nullptr; }
    RenderObject* renderer() const override { return nullptr; }

    bool accessibilityIsIgnored() const override { return boolAttributeValue(AXPropertyName::IsAccessibilityIgnored); }
    AccessibilityObjectInclusion defaultObjectInclusion() const override { return AccessibilityObjectInclusion::DefaultBehavior; }
    bool accessibilityIsIgnoredByDefault() const override { return false; }

    bool isShowingValidationMessage() const override { return false; }
    String validationMessage() const override { return String(); }

    unsigned blockquoteLevel() const override { return 0; }
    int headingLevel() const override { return 0; }
    int tableLevel() const override { return 0; }
    AccessibilityButtonState checkboxOrRadioValue() const override { return AccessibilityButtonState::Off; }
    String valueDescription() const override { return String(); }
    float valueForRange() const override { return 0; }
    float maxValueForRange() const override { return 0; }
    float minValueForRange() const override { return 0; }
    float stepValueForRange() const override { return 0; }
    AXCoreObject* selectedRadioButton() override { return nullptr; }
    AXCoreObject* selectedTabItem() override { return nullptr; }
    AXCoreObject* selectedListItem() override { return nullptr; }
    int layoutCount() const override { return 0; }
    double estimatedLoadingProgress() const override { return 0; }

    bool supportsARIAOwns() const override { return false; }
    bool isActiveDescendantOfFocusedContainer() const override { return false; }
    void ariaActiveDescendantReferencingElements(AccessibilityChildrenVector&) const override { }
    void ariaControlsElements(AccessibilityChildrenVector&) const override { }
    void ariaControlsReferencingElements(AccessibilityChildrenVector&) const override { }
    void ariaDescribedByElements(AccessibilityChildrenVector&) const override { }
    void ariaDescribedByReferencingElements(AccessibilityChildrenVector&) const override { }
    void ariaDetailsElements(AccessibilityChildrenVector&) const override { }
    void ariaDetailsReferencingElements(AccessibilityChildrenVector&) const override { }
    void ariaErrorMessageElements(AccessibilityChildrenVector&) const override { }
    void ariaErrorMessageReferencingElements(AccessibilityChildrenVector&) const override { }
    void ariaFlowToElements(AccessibilityChildrenVector&) const override { }
    void ariaFlowToReferencingElements(AccessibilityChildrenVector&) const override { }
    void ariaLabelledByElements(AccessibilityChildrenVector&) const override { }
    void ariaLabelledByReferencingElements(AccessibilityChildrenVector&) const override { }
    void ariaOwnsElements(AccessibilityChildrenVector&) const override { }
    void ariaOwnsReferencingElements(AccessibilityChildrenVector&) const override { }

    bool hasPopup() const override { return false; }
    String popupValue() const override { return String(); }
    bool hasDatalist() const override { return false; }
    bool supportsHasPopup() const override { return false; }
    bool pressedIsPresent() const override { return false; }
    bool ariaIsMultiline() const override { return false; }
    String invalidStatus() const override { return String(); }
    bool supportsPressed() const override { return false; }
    bool supportsExpanded() const override { return false; }
    bool supportsChecked() const override { return false; }
    AccessibilitySortDirection sortDirection() const override { return AccessibilitySortDirection::None; }
    bool canvasHasFallbackContent() const override { return false; }
    bool supportsRangeValue() const override { return false; }
    const AtomString& identifierAttribute() const override { return nullAtom(); }
    const AtomString& linkRelValue() const override { return nullAtom(); }
    void classList(Vector<String>&) const override { }
    AccessibilityCurrentState currentState() const override { return AccessibilityCurrentState::False; }
    String currentValue() const override { return String(); }
    bool supportsCurrent() const override { return false; }
    const String keyShortcutsValue() const override { return String(); }

    // This function checks if the object should be ignored when there's a modal dialog displayed.
    bool ignoredFromModalPresence() const override { return false; }
    bool isModalDescendant(Node*) const override { return false; }
    bool isModalNode() const override { return false; }

    bool supportsSetSize() const override { return false; }
    bool supportsPosInSet() const override { return false; }
    int setSize() const override { return 0; }
    int posInSet() const override { return 0; }

    bool supportsARIADropping() const override { return false; }
    bool supportsARIADragging() const override { return false; }
    bool isARIAGrabbed() override { return false; }
    void setARIAGrabbed(bool) override { }
    Vector<String> determineARIADropEffects() override { return Vector<String>(); }

    AXCoreObject* accessibilityHitTest(const IntPoint&) const override;
    AXCoreObject* elementAccessibilityHitTest(const IntPoint&) const override { return nullptr; }

    AXCoreObject* focusedUIElement() const override;

    AXCoreObject* firstChild() const override { return nullptr; }
    AXCoreObject* lastChild() const override { return nullptr; }
    AXCoreObject* previousSibling() const override { return nullptr; }
    AXCoreObject* nextSibling() const override { return nullptr; }
    AXCoreObject* nextSiblingUnignored(int) const override { return nullptr; }
    AXCoreObject* previousSiblingUnignored(int) const override { return nullptr; }
    AXCoreObject* parentObject() const override { return nullptr; }
    AXCoreObject* parentObjectUnignored() const override;
    AXCoreObject* parentObjectIfExists() const override { return nullptr; }
    void findMatchingObjects(AccessibilitySearchCriteria*, AccessibilityChildrenVector&) override { }
    bool isDescendantOfBarrenParent() const override { return false; }
    bool isDescendantOfRole(AccessibilityRole) const override { return false; }

    Vector<RefPtr<Range>> findTextRanges(AccessibilitySearchTextCriteria const&) const override { return Vector<RefPtr<Range>>(); }
    Vector<String> performTextOperation(AccessibilityTextOperation const&) override { return Vector<String>(); }

    AXCoreObject* observableObject() const override { return nullptr; }
    void linkedUIElements(AccessibilityChildrenVector&) const override { }
    AXCoreObject* titleUIElement() const override { return nullptr; }
    bool exposesTitleUIElement() const override { return false; }
    AXCoreObject* correspondingLabelForControlElement() const override { return nullptr; }
    AXCoreObject* correspondingControlForLabelElement() const override { return nullptr; }
    AXCoreObject* scrollBar(AccessibilityOrientation) override { return nullptr; }

    AccessibilityRole ariaRoleAttribute() const override { return AccessibilityRole::Unknown; }
    bool isPresentationalChildOfAriaRole() const override { return false; }
    bool ariaRoleHasPresentationalChildren() const override { return false; }
    bool inheritsPresentationalRole() const override { return false; }

    void accessibilityText(Vector<AccessibilityText>&) const override { }
    String computedLabel() override { return String(); }

    void setAccessibleName(const AtomString&) override { }
    bool hasAttributesRequiredForInclusion() const override { return false; }

    String accessibilityDescription() const override { return String(); }
    String title() const override { return String(); }
    String helpText() const override { return String(); }

    bool isARIAStaticText() const override { return false; }
    String stringValue() const override { return String(); }
    String textUnderElement(AccessibilityTextUnderElementMode = AccessibilityTextUnderElementMode()) const override { return String(); }
    String text() const override { return String(); }
    int textLength() const override { return 0; }
    String ariaLabeledByAttribute() const override { return String(); }
    String ariaDescribedByAttribute() const override { return String(); }
    const String placeholderValue() const override { return String(); }
    bool accessibleNameDerivesFromContent() const override { return false; }

    String expandedTextValue() const override { return String(); }
    bool supportsExpandedTextValue() const override { return false; }

    void elementsFromAttribute(Vector<Element*>&, const QualifiedName&) const override { }

    void colorValue(int&, int&, int&) const override { }

    AccessibilityRole roleValue() const override { return static_cast<AccessibilityRole>(intAttributeValue(AXPropertyName::RoleValue)); }
    String rolePlatformString() const override { return stringAttributeValue(AXPropertyName::RolePlatformString); }
    String roleDescription() const override { return stringAttributeValue(AXPropertyName::RoleDescription); }
    String ariaLandmarkRoleDescription() const override { return stringAttributeValue(AXPropertyName::ARIALandmarkRoleDescription); }

    AXObjectCache* axObjectCache() const override { return nullptr; }

    Element* anchorElement() const override { return nullptr; }
    bool supportsPressAction() const override { return false; }
    Element* actionElement() const override { return nullptr; }
    LayoutRect boundingBoxRect() const override { return rectAttributeValue<LayoutRect>(AXPropertyName::BoundingBoxRect); }
    LayoutRect elementRect() const override { return rectAttributeValue<LayoutRect>(AXPropertyName::ElementRect); }
    IntPoint clickPoint() override { return IntPoint(); }
    Path elementPath() const override { return Path(); }
    bool supportsPath() const override { return false; }

    TextIteratorBehavior textIteratorBehaviorForTextRange() const override { return TextIteratorDefaultBehavior; }
    PlainTextRange selectedTextRange() const override { return PlainTextRange(); }
    unsigned selectionStart() const override { return 0; }
    unsigned selectionEnd() const override { return 0; }

    URL url() const override { return URL(); }
    VisibleSelection selection() const override { return VisibleSelection(); }
    String selectedText() const override { return String(); }
    const AtomString& accessKey() const override { return nullAtom(); }
    const String& actionVerb() const override { return nullAtom(); }
    Widget* widget() const override { return nullptr; }
    Widget* widgetForAttachmentView() const override { return nullptr; }
    Page* page() const override { return nullptr; }
    Document* document() const override { return nullptr; }
    FrameView* documentFrameView() const override { return nullptr; }
    Frame* frame() const override { return nullptr; }
    Frame* mainFrame() const override { return nullptr; }
    Document* topDocument() const override { return nullptr; }
    ScrollView* scrollViewAncestor() const override { return nullptr; }
    String language() const override { return String(); }
    unsigned hierarchicalLevel() const override { return 0; }

    void setFocused(bool) override { }
    void setSelectedText(const String&) override { }
    void setSelectedTextRange(const PlainTextRange&) override { }
    void setValue(const String&) override { }
    bool replaceTextInRange(const String&, const PlainTextRange&) override { return false; }
    bool insertText(const String&) override { return false; }

    void setValue(float) override { }
    void setSelected(bool) override { }
    void setSelectedRows(AccessibilityChildrenVector&) override { }

    void makeRangeVisible(const PlainTextRange&) override { }
    bool press() override { return false; }
    bool performDefaultAction() override { return false; }

    AccessibilityOrientation orientation() const override { return AccessibilityOrientation::Undefined; }
    void increment() override { }
    void decrement() override { }

    void childrenChanged() override { }
    void textChanged() override { }
    void updateAccessibilityRole() override { }
    const AccessibilityChildrenVector& children(bool updateChildrenIfNeeded = true) override;
    void addChildren() override { }
    void addChild(AXCoreObject*) override { }
    void insertChild(AXCoreObject*, unsigned) override { }

    bool shouldIgnoreAttributeRole() const override { return false; }

    bool canHaveChildren() const override { return false; }
    bool hasChildren() const override { return false; }
    void updateChildrenIfNecessary() override { }
    void setNeedsToUpdateChildren() override { }
    void setNeedsToUpdateSubtree() override { };
    void clearChildren() override { }
    bool needsToUpdateChildren() const override { return false; }
    void detachFromParent() override { }
    bool isDetachedFromParent() override { return false; }

    bool canHaveSelectedChildren() const override { return false; }
    void selectedChildren(AccessibilityChildrenVector&) override { }
    void visibleChildren(AccessibilityChildrenVector&) override { }
    void tabChildren(AccessibilityChildrenVector&) override { }
    bool shouldFocusActiveDescendant() const override { return false; }
    AXCoreObject* activeDescendant() const override { return nullptr; }
    void handleActiveDescendantChanged() override { }
    void handleAriaExpandedChanged() override { }
    bool isDescendantOfObject(const AXCoreObject*) const override { return false; }
    bool isAncestorOfObject(const AXCoreObject*) const override { return false; }
    AXCoreObject* firstAnonymousBlockChild() const override { return nullptr; }

    bool hasAttribute(const QualifiedName&) const override { return false; }
    const AtomString& getAttribute(const QualifiedName&) const override { return nullAtom(); }
    bool hasTagName(const QualifiedName&) const override { return false; }

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

    String computedRoleString() const override { return String(); }

    // MSAA
    String stringValueForMSAA() const override { return String(); }
    String stringRoleForMSAA() const override { return String(); }
    String nameForMSAA() const override { return String(); }
    String descriptionForMSAA() const override { return String(); }
    AccessibilityRole roleValueForMSAA() const override { return AccessibilityRole::Unknown; }

    String passwordFieldValue() const override { return String(); }
    bool isValueAutofilled() const override { return false; }
    bool isValueAutofillAvailable() const override { return false; }
    AutoFillButtonType valueAutofillButtonType() const override { return AutoFillButtonType::None; }

    void ariaTreeRows(AccessibilityChildrenVector&) override { }
    void ariaTreeItemDisclosedRows(AccessibilityChildrenVector&) override { }
    void ariaTreeItemContent(AccessibilityChildrenVector&) override { }

    bool supportsLiveRegion(bool = true) const override { return false; }
    bool isInsideLiveRegion(bool = true) const override { return false; }
    AXCoreObject* liveRegionAncestor(bool = true) const override { return nullptr; }
    const String liveRegionStatus() const override { return String(); }
    const String liveRegionRelevant() const override { return String(); }
    bool liveRegionAtomic() const override { return false; }
    bool isBusy() const override { return false; }

    bool hasContentEditableAttributeSet() const override { return false; }

    bool supportsReadOnly() const override { return false; }
    String readOnlyValue() const override { return String(); }

    bool supportsAutoComplete() const override { return false; }
    String autoCompleteValue() const override { return String(); }

    bool supportsARIAAttributes() const override { return false; }

    OptionSet<SpeakAs> speakAsProperty() const override { return OptionSet<SpeakAs>(); }

    void scrollToMakeVisible() const override { }
    void scrollToMakeVisibleWithSubFocus(const IntRect&) const override { }
    void scrollToGlobalPoint(const IntPoint&) const override { }

    bool scrollByPage(ScrollByPageDirection) const override { return false; }
    IntPoint scrollPosition() const override { return IntPoint(); }
    IntSize scrollContentsSize() const override { return IntSize(); }
    IntRect scrollVisibleContentRect() const override { return IntRect(); }
    void scrollToMakeVisible(const ScrollRectToVisibleOptions&) const override { }

    bool lastKnownIsIgnoredValue() override { return false; }
    void setLastKnownIsIgnoredValue(bool) override { }

    void notifyIfIgnoredValueChanged() override { }

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
    bool isMathScriptObject(AccessibilityMathScriptObjectType) const override { return false; }
    bool isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType) const override { return false; }

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

    void mathPrescripts(AccessibilityMathMultiscriptPairs&) override { }
    void mathPostscripts(AccessibilityMathMultiscriptPairs&) override { }

    bool isAXHidden() const override { return false; }
    bool isDOMHidden() const override { return false; }
    bool isHidden() const override { return false; }

    void setWrapper(AccessibilityObjectWrapper* wrapper) override { m_wrapper = wrapper; }

    void overrideAttachmentParent(AXCoreObject*) override { }

    bool accessibilityIgnoreAttachment() const override { return false; }
    AccessibilityObjectInclusion accessibilityPlatformIncludesObject() const override { return AccessibilityObjectInclusion::DefaultBehavior; }

#if PLATFORM(IOS_FAMILY)
    int accessibilityPasswordFieldLength() override { return 0; }
    bool hasTouchEventListener() const override { return false; }
    bool isInputTypePopupButton() const override { return false; }
#endif

    void updateBackingStore() override;

#if PLATFORM(COCOA)
    bool preventKeyboardDOMEventDispatch() const override { return false; }
    void setPreventKeyboardDOMEventDispatch(bool) override { }
    bool fileUploadButtonReturnsValueInTitle() const override { return false; }
    String speechHintAttributeValue() const override { return stringAttributeValue(AXPropertyName::SpeechHint); }
    String descriptionAttributeValue() const override { return stringAttributeValue(AXPropertyName::Description); }
    String helpTextAttributeValue() const override { return stringAttributeValue(AXPropertyName::HelpText); }
    String titleAttributeValue() const override { return stringAttributeValue(AXPropertyName::Title); }
#endif

#if PLATFORM(COCOA) && !PLATFORM(IOS_FAMILY)
    bool caretBrowsingEnabled() const override { return false; }
    void setCaretBrowsingEnabled(bool) override { }
#endif

    AXCoreObject* focusableAncestor() override { return nullptr; }
    AXCoreObject* editableAncestor() override { return nullptr; }
    AXCoreObject* highestEditableAncestor() override { return nullptr; }

    const AccessibilityScrollView* ancestorAccessibilityScrollView(bool) const override { return nullptr; }

    void setIsIgnoredFromParentData(AccessibilityIsIgnoredFromParentData&) override { };
    void clearIsIgnoredFromParentData() override { }
    void setIsIgnoredFromParentDataForChild(AXCoreObject*) override { }

    enum class AXPropertyName : uint8_t {
        None = 0,
        BoundingBoxRect,
        Description,
        ElementRect,
        HelpText,
        IsAccessibilityIgnored,
        IsAnonymousMathOperator,
        IsAttachment,
        IsChecked,
        IsCollapsed,
        IsEnabled,
        IsExpanded,
        IsFileUploadButton,
        IsFocused,
        IsImage,
        IsImageMapLink,
        IsIndeterminate,
        IsLink,
        IsLinked,
        IsLoaded,
        IsHovered,
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
        IsMediaControlLabel,
        IsMultiSelectable,
        IsOffScreen,
        IsOnScreen,
        IsPressed,
        IsRequired,
        IsScrollbar,
        IsSelected,
        IsSelectedOptionActive,
        IsTree,
        IsTreeItem,
        IsUnvisited,
        IsVisible,
        IsVisited,
        MathFencedOpenString,
        MathFencedCloseString,
        MathLineThickness,
        MathRadicandObject,
        MathRootIndexObject,
        MathUnderObject,
        MathOverObject,
        MathNumeratorObject,
        MathDenominatorObject,
        MathBaseObject,
        MathSubscriptObject,
        MathSuperscriptObject,
        RelativeFrame,
        RoleValue,
        RolePlatformString,
        RoleDescription,
        ARIALandmarkRoleDescription,
        SpeechHint,
        SupportsRequiredAttribute,
        Title,
    };

    AXID parent() const { return m_parent; }

    AXIsolatedTreeID treeIdentifier() const { return m_treeIdentifier; }
    AXIsolatedTree* tree() const;

    AXIsolatedObject() = default;
    AXIsolatedObject(AXCoreObject&);
    void initializeAttributeData(AXCoreObject&);

    using AttributeValueVariant = Variant<std::nullptr_t, String, bool, int, unsigned, double, LayoutRect, AXID>;
    void setProperty(AXPropertyName, AttributeValueVariant&&, bool shouldRemove = false);
    void setObjectProperty(AXPropertyName, AXCoreObject*);
    
    bool boolAttributeValue(AXPropertyName) const;
    const String stringAttributeValue(AXPropertyName) const;
    int intAttributeValue(AXPropertyName) const;
    unsigned unsignedAttributeValue(AXPropertyName) const;
    double doubleAttributeValue(AXPropertyName) const;
    template<typename T> T rectAttributeValue(AXPropertyName) const;
    AXCoreObject* objectAttributeValue(AXPropertyName) const;

    AXID m_parent;
    AXID m_id;
    bool m_initialized { false };
    AXIsolatedTreeID m_treeIdentifier;
    RefPtr<AXIsolatedTree> m_cachedTree;
    Vector<AXID> m_childrenIDs;
    Vector<RefPtr<AXCoreObject>> m_children;

#if PLATFORM(COCOA)
    RetainPtr<WebAccessibilityObjectWrapper> m_wrapper;
#endif

    HashMap<AXPropertyName, AttributeValueVariant, WTF::IntHash<AXPropertyName>, WTF::StrongEnumHashTraits<AXPropertyName>> m_attributeMap;
};

} // namespace WebCore

#endif // ENABLE((ACCESSIBILITY_ISOLATED_TREE))
