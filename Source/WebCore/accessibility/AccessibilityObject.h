/*
 * Copyright (C) 2008, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nuanti Ltd.
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

#include "AccessibilityObjectInterface.h"
#include "FloatQuad.h"
#include "LayoutRect.h"
#include "Path.h"
#include "Range.h"
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#endif

#if USE(ATK)
#include <wtf/glib/GRefPtr.h>
#endif

#if PLATFORM(COCOA)

OBJC_CLASS NSArray;
OBJC_CLASS NSAttributedString;
OBJC_CLASS NSData;
OBJC_CLASS NSMutableAttributedString;
OBJC_CLASS NSString;
OBJC_CLASS NSValue;
OBJC_CLASS NSView;

#endif

namespace WebCore {

class AccessibilityObject;
class IntPoint;
class IntSize;
class ScrollableArea;

struct AccessibilityText {
    String text;
    AccessibilityTextSource textSource;
    
    AccessibilityText(const String& t, const AccessibilityTextSource& s)
        : text(t)
        , textSource(s)
    { }
};

bool nodeHasPresentationRole(Node*);

class AccessibilityObject : public AXCoreObject {
public:
    virtual ~AccessibilityObject();

    // After constructing an AccessibilityObject, it must be given a
    // unique ID, then added to AXObjectCache, and finally init() must
    // be called last.
    void setObjectID(AXID id) override { m_id = id; }
    void init() override { }

    bool isDetached() const override;

    bool isAccessibilityNodeObject() const override { return false; }
    bool isAccessibilityRenderObject() const override { return false; }
    bool isAccessibilityScrollbar() const override { return false; }
    bool isAccessibilityScrollViewInstance() const override { return false; }
    bool isAccessibilitySVGRoot() const override { return false; }
    bool isAccessibilitySVGElement() const override { return false; }
    bool isAccessibilityTableInstance() const override { return false; }
    bool isAccessibilityTableColumnInstance() const override { return false; }
    bool isAccessibilityProgressIndicatorInstance() const override { return false; }
    bool isAXIsolatedObjectInstance() const override { return false; }

    bool isAttachmentElement() const override { return false; }
    bool isHeading() const override { return false; }
    bool isLink() const override { return false; }
    bool isNativeImage() const override { return false; }
    bool isImageButton() const override { return false; }
    bool isPasswordField() const override { return false; }
    bool isContainedByPasswordField() const override;
    AccessibilityObject* passwordFieldOrContainingPasswordField() override { return nullptr; }
    bool isNativeTextControl() const override { return false; }
    bool isSearchField() const override { return false; }
    bool isNativeListBox() const override { return false; }
    bool isListBoxOption() const override { return false; }
    bool isAttachment() const override { return false; }
    bool isMediaTimeline() const override { return false; }
    bool isMenuRelated() const override { return false; }
    bool isMenu() const override { return false; }
    bool isMenuBar() const override { return false; }
    bool isMenuButton() const override { return false; }
    bool isMenuItem() const override { return false; }
    bool isFileUploadButton() const override { return false; }
    bool isInputImage() const override { return false; }
    bool isProgressIndicator() const override { return false; }
    bool isSlider() const override { return false; }
    bool isSliderThumb() const override { return false; }
    bool isInputSlider() const override { return false; }
    bool isControl() const override { return false; }
    bool isLabel() const override { return false; }

    bool isList() const override { return false; }
    bool isUnorderedList() const override { return false; }
    bool isOrderedList() const override { return false; }
    bool isDescriptionList() const override { return false; }

    // Table support.
    bool isTable() const override { return false; }
    bool isExposable() const override { return true; }
    bool isDataTable() const override { return false; }
    int tableLevel() const override { return 0; }
    bool supportsSelectedRows() const override { return false; }
    AccessibilityChildrenVector columns() override { return AccessibilityChildrenVector(); }
    AccessibilityChildrenVector rows() override { return AccessibilityChildrenVector(); }
    unsigned columnCount() override { return 0; }
    unsigned rowCount() override { return 0; }
    AccessibilityChildrenVector cells() override { return AccessibilityChildrenVector(); }
    AXCoreObject* cellForColumnAndRow(unsigned, unsigned) override { return nullptr; }
    AccessibilityChildrenVector columnHeaders() override { return AccessibilityChildrenVector(); }
    AccessibilityChildrenVector rowHeaders() override { return AccessibilityChildrenVector(); }
    AccessibilityChildrenVector visibleRows() override { return AccessibilityChildrenVector(); }
    AXCoreObject* headerContainer() override { return nullptr; }
    int axColumnCount() const override { return 0; }
    int axRowCount() const override { return 0; }

    // Table cell support.
    bool isTableCell() const override { return false; }
    // Returns the start location and row span of the cell.
    std::pair<unsigned, unsigned> rowIndexRange() const override { return { 0, 1 }; }
    // Returns the start location and column span of the cell.
    std::pair<unsigned, unsigned> columnIndexRange() const override { return { 0, 1 }; }
    bool isColumnHeaderCell() const override { return false; }
    bool isRowHeaderCell() const override { return false; }
    int axColumnIndex() const override { return -1; }
    int axRowIndex() const override { return -1; }

    // Table column support.
    bool isTableColumn() const override { return false; }
    unsigned columnIndex() const override { return 0; }
    AXCoreObject* columnHeader() override { return nullptr; }

    // Table row support.
    bool isTableRow() const override { return false; }
    unsigned rowIndex() const override { return 0; }

    // ARIA tree/grid row support.
    bool isARIATreeGridRow() const override { return false; }
    AccessibilityChildrenVector disclosedRows() override; // ARIATreeItem implementation. AccessibilityARIAGridRow overrides this method.
    AXCoreObject* disclosedByRow() const override { return nullptr; }

    bool isFieldset() const override { return false; }
    bool isGroup() const override { return false; }
    bool isImageMapLink() const override { return false; }
    bool isMenuList() const override { return false; }
    bool isMenuListPopup() const override { return false; }
    bool isMenuListOption() const override { return false; }
    bool isNativeSpinButton() const override { return false; }
    AXCoreObject* incrementButton() override { return nullptr; }
    AXCoreObject* decrementButton() override { return nullptr; }
    bool isSpinButtonPart() const override { return false; }
    bool isIncrementor() const override { return false; }
    bool isMockObject() const override { return false; }
    virtual bool isMediaControlLabel() const { return false; }
    bool isMediaObject() const override { return false; }
    bool isTextControl() const override;
    bool isARIATextControl() const override;
    bool isNonNativeTextControl() const override;
    bool isButton() const override;
    bool isLandmark() const override;
    bool isRangeControl() const override;
    bool isMeter() const override;
    bool isStyleFormatGroup() const override;
    bool isFigureElement() const override;
    bool isKeyboardFocusable() const override;
    bool isOutput() const override;

    bool isChecked() const override { return false; }
    bool isEnabled() const override { return false; }
    bool isSelected() const override { return false; }
    bool isFocused() const override { return false; }
    bool isHovered() const override { return false; }
    bool isIndeterminate() const override { return false; }
    bool isLoaded() const override { return false; }
    bool isMultiSelectable() const override { return false; }
    bool isOffScreen() const override { return false; }
    bool isPressed() const override { return false; }
    bool isUnvisited() const override { return false; }
    bool isVisited() const override { return false; }
    bool isRequired() const override { return false; }
    bool supportsRequiredAttribute() const override { return false; }
    bool isLinked() const override { return false; }
    bool isExpanded() const override;
    bool isVisible() const override { return true; }
    bool isCollapsed() const override { return false; }
    void setIsExpanded(bool) override { }
    FloatRect relativeFrame() const override;
    FloatRect convertFrameToSpace(const FloatRect&, AccessibilityConversionSpace) const override;

    // In a multi-select list, many items can be selected but only one is active at a time.
    bool isSelectedOptionActive() const override { return false; }

    bool hasBoldFont() const override { return false; }
    bool hasItalicFont() const override { return false; }
    bool hasMisspelling() const override;
    RefPtr<Range> getMisspellingRange(RefPtr<Range> const& start, AccessibilitySearchDirection) const override;
    bool hasPlainText() const override { return false; }
    bool hasSameFont(const AXCoreObject&) const override { return false; }
    bool hasSameFontColor(const AXCoreObject&) const override { return false; }
    bool hasSameStyle(const AXCoreObject&) const override { return false; }
    bool hasUnderline() const override { return false; }
    bool hasHighlighting() const override;

    bool supportsDatetimeAttribute() const override;
    String datetimeAttributeValue() const override;

    bool canSetFocusAttribute() const override { return false; }
    bool canSetTextRangeAttributes() const override { return false; }
    bool canSetValueAttribute() const override { return false; }
    bool canSetNumericValue() const override { return false; }
    bool canSetSelectedAttribute() const override { return false; }
    bool canSetSelectedChildrenAttribute() const override { return false; }
    bool canSetExpandedAttribute() const override { return false; }

    Element* element() const override;
    Node* node() const override { return nullptr; }
    RenderObject* renderer() const override { return nullptr; }
    bool accessibilityIsIgnored() const override;
    AccessibilityObjectInclusion defaultObjectInclusion() const override;
    bool accessibilityIsIgnoredByDefault() const override;

    bool isShowingValidationMessage() const override;
    String validationMessage() const override;

    unsigned blockquoteLevel() const override;
    int headingLevel() const override { return 0; }
    AccessibilityButtonState checkboxOrRadioValue() const override;
    String valueDescription() const override { return String(); }
    float valueForRange() const override { return 0.0f; }
    float maxValueForRange() const override { return 0.0f; }
    float minValueForRange() const override { return 0.0f; }
    float stepValueForRange() const override { return 0.0f; }
    AXCoreObject* selectedRadioButton() override { return nullptr; }
    AXCoreObject* selectedTabItem() override { return nullptr; }
    AXCoreObject* selectedListItem() override;
    int layoutCount() const override { return 0; }
    double estimatedLoadingProgress() const override { return 0; }
    WEBCORE_EXPORT static bool isARIAControl(AccessibilityRole);
    WEBCORE_EXPORT static bool isARIAInput(AccessibilityRole);

    bool supportsARIAOwns() const override { return false; }
    bool isActiveDescendantOfFocusedContainer() const override;
    void ariaActiveDescendantReferencingElements(AccessibilityChildrenVector&) const override;
    void ariaControlsElements(AccessibilityChildrenVector&) const override;
    void ariaControlsReferencingElements(AccessibilityChildrenVector&) const override;
    void ariaDescribedByElements(AccessibilityChildrenVector&) const override;
    void ariaDescribedByReferencingElements(AccessibilityChildrenVector&) const override;
    void ariaDetailsElements(AccessibilityChildrenVector&) const override;
    void ariaDetailsReferencingElements(AccessibilityChildrenVector&) const override;
    void ariaErrorMessageElements(AccessibilityChildrenVector&) const override;
    void ariaErrorMessageReferencingElements(AccessibilityChildrenVector&) const override;
    void ariaFlowToElements(AccessibilityChildrenVector&) const override;
    void ariaFlowToReferencingElements(AccessibilityChildrenVector&) const override;
    void ariaLabelledByElements(AccessibilityChildrenVector&) const override;
    void ariaLabelledByReferencingElements(AccessibilityChildrenVector&) const override;
    void ariaOwnsElements(AccessibilityChildrenVector&) const override;
    void ariaOwnsReferencingElements(AccessibilityChildrenVector&) const override;

    bool hasPopup() const override { return false; }
    String popupValue() const override;
    bool hasDatalist() const override;
    bool supportsHasPopup() const override;
    bool pressedIsPresent() const override;
    bool ariaIsMultiline() const override;
    String invalidStatus() const override;
    bool supportsPressed() const override;
    bool supportsExpanded() const override;
    bool supportsChecked() const override;
    AccessibilitySortDirection sortDirection() const override;
    bool canvasHasFallbackContent() const override { return false; }
    bool supportsRangeValue() const override;
    String identifierAttribute() const override;
    String linkRelValue() const override;
    void classList(Vector<String>&) const override;
    AccessibilityCurrentState currentState() const override;
    String currentValue() const override;
    bool supportsCurrent() const override;
    const String keyShortcutsValue() const override;

    // This function checks if the object should be ignored when there's a modal dialog displayed.
    bool ignoredFromModalPresence() const override;
    bool isModalDescendant(Node*) const override;
    bool isModalNode() const override;

    bool supportsSetSize() const override;
    bool supportsPosInSet() const override;
    int setSize() const override;
    int posInSet() const override;

    // ARIA drag and drop
    bool supportsDropping() const override { return false; }
    bool supportsDragging() const override { return false; }
    bool isGrabbed() override { return false; }
    void setARIAGrabbed(bool) override { }
    Vector<String> determineDropEffects() const override { return { }; }

    // Called on the root AX object to return the deepest available element.
    AXCoreObject* accessibilityHitTest(const IntPoint&) const override { return nullptr; }
    // Called on the AX object after the render tree determines which is the right AccessibilityRenderObject.
    AXCoreObject* elementAccessibilityHitTest(const IntPoint&) const override;

    AXCoreObject* focusedUIElement() const override;

    AccessibilityObject* firstChild() const override { return nullptr; }
    AccessibilityObject* lastChild() const override { return nullptr; }
    AccessibilityObject* previousSibling() const override { return nullptr; }
    AccessibilityObject* nextSibling() const override { return nullptr; }
    AccessibilityObject* nextSiblingUnignored(int limit) const override;
    AccessibilityObject* previousSiblingUnignored(int limit) const override;
    AccessibilityObject* parentObject() const override { return nullptr; }
    AXCoreObject* parentObjectUnignored() const override;
    AccessibilityObject* parentObjectIfExists() const override { return nullptr; }
    static AccessibilityObject* firstAccessibleObjectFromNode(const Node*);
    void findMatchingObjects(AccessibilitySearchCriteria*, AccessibilityChildrenVector&) override;
    bool isDescendantOfBarrenParent() const override { return false; }

    bool isDescendantOfRole(AccessibilityRole) const override;

    // Text selection
private:
    RefPtr<Range> rangeOfStringClosestToRangeInDirection(Range*, AccessibilitySearchDirection, Vector<String> const&) const;
    Optional<SimpleRange> selectionRange() const;
    RefPtr<Range> findTextRange(Vector<String> const& searchStrings, RefPtr<Range> const& start, AccessibilitySearchTextDirection) const;
public:
    Vector<RefPtr<Range>> findTextRanges(AccessibilitySearchTextCriteria const&) const override;
    Vector<String> performTextOperation(AccessibilityTextOperation const&) override;

    AccessibilityObject* observableObject() const override { return nullptr; }
    void linkedUIElements(AccessibilityChildrenVector&) const override { }
    AccessibilityObject* titleUIElement() const override { return nullptr; }
    bool exposesTitleUIElement() const override { return true; }
    AccessibilityObject* correspondingLabelForControlElement() const override { return nullptr; }
    AccessibilityObject* correspondingControlForLabelElement() const override { return nullptr; }
    AccessibilityObject* scrollBar(AccessibilityOrientation) override { return nullptr; }

    AccessibilityRole ariaRoleAttribute() const override { return AccessibilityRole::Unknown; }
    bool isPresentationalChildOfAriaRole() const override { return false; }
    bool ariaRoleHasPresentationalChildren() const override { return false; }
    bool inheritsPresentationalRole() const override { return false; }

    // Accessibility Text
    void accessibilityText(Vector<AccessibilityText>&) const override { };
    // A single method for getting a computed label for an AXObject. It condenses the nuances of accessibilityText. Used by Inspector.
    String computedLabel() override;

    // A programmatic way to set a name on an AccessibleObject.
    void setAccessibleName(const AtomString&) override { }
    bool hasAttributesRequiredForInclusion() const override;

    // Accessibility Text - (To be deprecated).
    String accessibilityDescription() const override { return String(); }
    String title() const override { return String(); }
    String helpText() const override { return String(); }

    // Methods for determining accessibility text.
    bool isARIAStaticText() const override { return ariaRoleAttribute() == AccessibilityRole::StaticText; }
    String stringValue() const override { return String(); }
    String textUnderElement(AccessibilityTextUnderElementMode = AccessibilityTextUnderElementMode()) const override { return String(); }
    String text() const override { return String(); }
    int textLength() const override { return 0; }
    String ariaLabeledByAttribute() const override { return String(); }
    String ariaDescribedByAttribute() const override { return String(); }
    const String placeholderValue() const override;
    bool accessibleNameDerivesFromContent() const override;

    // Abbreviations
    String expandedTextValue() const override { return String(); }
    bool supportsExpandedTextValue() const override { return false; }

    void elementsFromAttribute(Vector<Element*>&, const QualifiedName&) const override;

    // Only if isColorWell()
    void colorValue(int& r, int& g, int& b) const override { r = 0; g = 0; b = 0; }

    AccessibilityRole roleValue() const override { return m_role; }
    String rolePlatformString() const override;
    String roleDescription() const override;
    String ariaLandmarkRoleDescription() const override;

    AXObjectCache* axObjectCache() const override;
    AXID objectID() const override { return m_id; }

    static AccessibilityObject* anchorElementForNode(Node*);
    static AccessibilityObject* headingElementForNode(Node*);
    Element* anchorElement() const override { return nullptr; }
    bool supportsPressAction() const override;
    Element* actionElement() const override { return nullptr; }
    LayoutRect boundingBoxRect() const override { return LayoutRect(); }
    LayoutRect elementRect() const override = 0;
    IntPoint clickPoint() override;
    static IntRect boundingBoxForQuads(RenderObject*, const Vector<FloatQuad>&);
    Path elementPath() const override { return Path(); }
    bool supportsPath() const override { return false; }

    TextIteratorBehavior textIteratorBehaviorForTextRange() const override;
    PlainTextRange selectedTextRange() const override { return PlainTextRange(); }
    unsigned selectionStart() const override { return selectedTextRange().start; }
    unsigned selectionEnd() const override { return selectedTextRange().length; }

    URL url() const override { return URL(); }
    VisibleSelection selection() const override { return VisibleSelection(); }
    String selectedText() const override { return String(); }
    String accessKey() const override { return nullAtom(); }
    String actionVerb() const override;
    Widget* widget() const override { return nullptr; }
    PlatformWidget platformWidget() const override { return nullptr; }
#if PLATFORM(COCOA)
    RemoteAXObjectRef remoteParentObject() const override;
#endif
    Widget* widgetForAttachmentView() const override { return nullptr; }
    Page* page() const override;
    Document* document() const override;
    FrameView* documentFrameView() const override;
    Frame* frame() const override;
    Frame* mainFrame() const override;
    Document* topDocument() const override;
    ScrollView* scrollView() const override { return nullptr; }
    ScrollView* scrollViewAncestor() const override;
    String language() const override;
    // 1-based, to match the aria-level spec.
    unsigned hierarchicalLevel() const override { return 0; }
    bool isInlineText() const override;

    void setFocused(bool) override { }
    void setSelectedText(const String&) override { }
    void setSelectedTextRange(const PlainTextRange&) override { }
    void setValue(const String&) override { }
    bool replaceTextInRange(const String&, const PlainTextRange&) override;
    bool insertText(const String&) override;

    void setValue(float) override { }
    void setSelected(bool) override { }
    void setSelectedRows(AccessibilityChildrenVector&) override { }

    void makeRangeVisible(const PlainTextRange&) override { }
    bool press() override;
    bool performDefaultAction() override { return press(); }

    AccessibilityOrientation orientation() const override;
    void increment() override { }
    void decrement() override { }

    void childrenChanged() override { }
    void textChanged() override { }
    void updateAccessibilityRole() override { }
    const AccessibilityChildrenVector& children(bool updateChildrenIfNeeded = true) override;
    void addChildren() override { }
    void addChild(AXCoreObject*) override;
    void insertChild(AXCoreObject*, unsigned) override;

    bool shouldIgnoreAttributeRole() const override { return false; }

    bool canHaveChildren() const override { return true; }
    bool hasChildren() const override { return m_haveChildren; }
    void updateChildrenIfNecessary() override;
    void setNeedsToUpdateChildren() override { }
    void setNeedsToUpdateSubtree() override { }
    void clearChildren() override;
    bool needsToUpdateChildren() const override { return false; }
#if PLATFORM(COCOA)
    void detachFromParent() override;
#else
    void detachFromParent() override { }
#endif
    bool isDetachedFromParent() override { return false; }

    bool canHaveSelectedChildren() const override { return false; }
    void selectedChildren(AccessibilityChildrenVector&) override { }
    void visibleChildren(AccessibilityChildrenVector&) override { }
    void tabChildren(AccessibilityChildrenVector&) override { }
    bool shouldFocusActiveDescendant() const override { return false; }
    AccessibilityObject* activeDescendant() const override { return nullptr; }
    void handleActiveDescendantChanged() override { }
    void handleAriaExpandedChanged() override { }
    AccessibilityObject* firstAnonymousBlockChild() const override;

    WEBCORE_EXPORT static AccessibilityRole ariaRoleToWebCoreRole(const String&);
    bool hasAttribute(const QualifiedName&) const override;
    const AtomString& getAttribute(const QualifiedName&) const override;
    bool hasTagName(const QualifiedName&) const override;
    String tagName() const override;

    VisiblePositionRange visiblePositionRange() const override { return VisiblePositionRange(); }
    VisiblePositionRange visiblePositionRangeForLine(unsigned) const override { return VisiblePositionRange(); }

    RefPtr<Range> elementRange() const override;
    static bool replacedNodeNeedsCharacter(Node* replacedNode);

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

    RefPtr<Range> rangeForPlainTextRange(const PlainTextRange&) const override;

    static String stringForVisiblePositionRange(const VisiblePositionRange&);
    String stringForRange(RefPtr<Range>) const override;
    IntRect boundsForVisiblePositionRange(const VisiblePositionRange&) const override { return IntRect(); }
    IntRect boundsForRange(const RefPtr<Range>) const override { return IntRect(); }
    int lengthForVisiblePositionRange(const VisiblePositionRange&) const override;
    void setSelectedVisiblePositionRange(const VisiblePositionRange&) const override { }

    VisiblePosition visiblePositionForBounds(const IntRect&, AccessibilityVisiblePositionForBounds) const override;
    VisiblePosition visiblePositionForPoint(const IntPoint&) const override { return VisiblePosition(); }
    VisiblePosition nextVisiblePosition(const VisiblePosition& visiblePos) const override { return visiblePos.next(); }
    VisiblePosition previousVisiblePosition(const VisiblePosition& visiblePos) const override { return visiblePos.previous(); }
    VisiblePosition nextWordEnd(const VisiblePosition&) const override;
    VisiblePosition previousWordStart(const VisiblePosition&) const override;
    VisiblePosition nextLineEndPosition(const VisiblePosition&) const override;
    VisiblePosition previousLineStartPosition(const VisiblePosition&) const override;
    VisiblePosition nextSentenceEndPosition(const VisiblePosition&) const override;
    VisiblePosition previousSentenceStartPosition(const VisiblePosition&) const override;
    VisiblePosition nextParagraphEndPosition(const VisiblePosition&) const override;
    VisiblePosition previousParagraphStartPosition(const VisiblePosition&) const override;
    VisiblePosition visiblePositionForIndex(unsigned, bool /*lastIndexOK */) const override { return VisiblePosition(); }

    VisiblePosition visiblePositionForIndex(int) const override { return VisiblePosition(); }
    int indexForVisiblePosition(const VisiblePosition&) const override { return 0; }

    AccessibilityObject* accessibilityObjectForPosition(const VisiblePosition&) const override;
    int lineForPosition(const VisiblePosition&) const override;
    PlainTextRange plainTextRangeForVisiblePositionRange(const VisiblePositionRange&) const override;
    int index(const VisiblePosition&) const override { return -1; }

    void lineBreaks(Vector<int>&) const override { }
    PlainTextRange doAXRangeForLine(unsigned) const override { return PlainTextRange(); }
    PlainTextRange doAXRangeForPosition(const IntPoint&) const override;
    PlainTextRange doAXRangeForIndex(unsigned) const override { return PlainTextRange(); }
    PlainTextRange doAXStyleRangeForIndex(unsigned) const override;

    String doAXStringForRange(const PlainTextRange&) const override { return String(); }
    IntRect doAXBoundsForRange(const PlainTextRange&) const override { return IntRect(); }
    IntRect doAXBoundsForRangeUsingCharacterOffset(const PlainTextRange&) const override { return IntRect(); }
    static String listMarkerTextForNodeAndPosition(Node*, const VisiblePosition&);

    unsigned doAXLineForIndex(unsigned) override;

    String computedRoleString() const override;

    String stringValueForMSAA() const override { return String(); }
    String stringRoleForMSAA() const override { return String(); }
    String nameForMSAA() const override { return String(); }
    String descriptionForMSAA() const override { return String(); }
    AccessibilityRole roleValueForMSAA() const override { return roleValue(); }

    String passwordFieldValue() const override { return String(); }
    bool isValueAutofilled() const override;
    bool isValueAutofillAvailable() const override;
    AutoFillButtonType valueAutofillButtonType() const override;

    // Used by an ARIA tree to get all its rows.
    void ariaTreeRows(AccessibilityChildrenVector&) override;
    // Used by an ARIA tree item to get only its content, and not its child tree items and groups.
    void ariaTreeItemContent(AccessibilityChildrenVector&) override;

    // ARIA live-region features.
    bool supportsLiveRegion(bool excludeIfOff = true) const override;
    bool isInsideLiveRegion(bool excludeIfOff = true) const override;
    AccessibilityObject* liveRegionAncestor(bool excludeIfOff = true) const override;
    const String liveRegionStatus() const override { return String(); }
    const String liveRegionRelevant() const override { return nullAtom(); }
    bool liveRegionAtomic() const override { return false; }
    bool isBusy() const override { return false; }
    static const String defaultLiveRegionStatusForRole(AccessibilityRole);
    static bool liveRegionStatusIsEnabled(const AtomString&);
    static bool contentEditableAttributeIsEnabled(Element*);
    bool hasContentEditableAttributeSet() const override;

    bool supportsReadOnly() const override;
    String readOnlyValue() const override;

    bool supportsAutoComplete() const override;
    String autoCompleteValue() const override;

    bool hasARIAValueNow() const override { return hasAttribute(HTMLNames::aria_valuenowAttr); }
    bool supportsARIAAttributes() const override;

    // CSS3 Speech properties.
    OptionSet<SpeakAs> speakAsProperty() const override { return OptionSet<SpeakAs> { }; }

    // Make this object visible by scrolling as many nested scrollable views as needed.
    void scrollToMakeVisible() const override;
    // Same, but if the whole object can't be made visible, try for this subrect, in local coordinates.
    void scrollToMakeVisibleWithSubFocus(const IntRect&) const override;
    // Scroll this object to a given point in global coordinates of the top-level window.
    void scrollToGlobalPoint(const IntPoint&) const override;

    bool scrollByPage(ScrollByPageDirection) const override;
    IntPoint scrollPosition() const override;
    IntSize scrollContentsSize() const override;
    IntRect scrollVisibleContentRect() const override;
    void scrollToMakeVisible(const ScrollRectToVisibleOptions&) const override;

    bool lastKnownIsIgnoredValue() override;
    void setLastKnownIsIgnoredValue(bool) override;

    // Fires a children changed notification on the parent if the isIgnored value changed.
    void notifyIfIgnoredValueChanged() override;

    // All math elements return true for isMathElement().
    bool isMathElement() const override { return false; }
    bool isMathFraction() const override { return false; }
    bool isMathFenced() const override { return false; }
    bool isMathSubscriptSuperscript() const override { return false; }
    bool isMathRow() const override { return false; }
    bool isMathUnderOver() const override { return false; }
    bool isMathRoot() const override { return false; }
    bool isMathSquareRoot() const override { return false; }
    bool isMathText() const override { return false; }
    bool isMathNumber() const override { return false; }
    bool isMathOperator() const override { return false; }
    bool isMathFenceOperator() const override { return false; }
    bool isMathSeparatorOperator() const override { return false; }
    bool isMathIdentifier() const override { return false; }
    bool isMathTable() const override { return false; }
    bool isMathTableRow() const override { return false; }
    bool isMathTableCell() const override { return false; }
    bool isMathMultiscript() const override { return false; }
    bool isMathToken() const override { return false; }
    bool isMathScriptObject(AccessibilityMathScriptObjectType) const override { return false; }
    bool isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType) const override { return false; }

    // Root components.
    AXCoreObject* mathRadicandObject() override { return nullptr; }
    AXCoreObject* mathRootIndexObject() override { return nullptr; }

    // Under over components.
    AXCoreObject* mathUnderObject() override { return nullptr; }
    AXCoreObject* mathOverObject() override { return nullptr; }

    // Fraction components.
    AXCoreObject* mathNumeratorObject() override { return nullptr; }
    AXCoreObject* mathDenominatorObject() override { return nullptr; }

    // Subscript/superscript components.
    AXCoreObject* mathBaseObject() override { return nullptr; }
    AXCoreObject* mathSubscriptObject() override { return nullptr; }
    AXCoreObject* mathSuperscriptObject() override { return nullptr; }

    // Fenced components.
    String mathFencedOpenString() const override { return String(); }
    String mathFencedCloseString() const override { return String(); }
    int mathLineThickness() const override { return 0; }
    bool isAnonymousMathOperator() const override { return false; }

    // Multiscripts components.
    void mathPrescripts(AccessibilityMathMultiscriptPairs&) override { }
    void mathPostscripts(AccessibilityMathMultiscriptPairs&) override { }

    // Visibility.
    bool isAXHidden() const override;
    bool isDOMHidden() const override;
    bool isHidden() const override { return isAXHidden() || isDOMHidden(); }

#if PLATFORM(COCOA)
    void overrideAttachmentParent(AXCoreObject* parent) override;
#else
    void overrideAttachmentParent(AXCoreObject*) override { }
#endif

#if ENABLE(ACCESSIBILITY)
    // a platform-specific method for determining if an attachment is ignored
    bool accessibilityIgnoreAttachment() const override;
    // gives platforms the opportunity to indicate if and how an object should be included
    AccessibilityObjectInclusion accessibilityPlatformIncludesObject() const override;
#else
    bool accessibilityIgnoreAttachment() const override { return true; }
    AccessibilityObjectInclusion accessibilityPlatformIncludesObject() const override { return AccessibilityObjectInclusion::DefaultBehavior; }
#endif

#if PLATFORM(IOS_FAMILY)
    int accessibilityPasswordFieldLength() override;
    bool hasTouchEventListener() const override;
    bool isInputTypePopupButton() const override;
#endif

    // allows for an AccessibilityObject to update its render tree or perform
    // other operations update type operations
    void updateBackingStore() override;

#if PLATFORM(COCOA)
    bool preventKeyboardDOMEventDispatch() const override;
    void setPreventKeyboardDOMEventDispatch(bool) override;
    bool fileUploadButtonReturnsValueInTitle() const override;
    String speechHintAttributeValue() const override;
    String descriptionAttributeValue() const override;
    String helpTextAttributeValue() const override;
    String titleAttributeValue() const override;
    bool hasApplePDFAnnotationAttribute() const override { return hasAttribute(HTMLNames::x_apple_pdf_annotationAttr); }
#endif

#if PLATFORM(COCOA) && !PLATFORM(IOS_FAMILY)
    bool caretBrowsingEnabled() const override;
    void setCaretBrowsingEnabled(bool) override;
#endif

    AccessibilityObject* focusableAncestor() override;
    AccessibilityObject* editableAncestor() override;
    AccessibilityObject* highestEditableAncestor() override;

    const AccessibilityScrollView* ancestorAccessibilityScrollView(bool includeSelf) const override;
    AccessibilityObject* webAreaObject() const override { return nullptr; }

    void clearIsIgnoredFromParentData() override { m_isIgnoredFromParentData = AccessibilityIsIgnoredFromParentData(); }
    void setIsIgnoredFromParentDataForChild(AXCoreObject*) override;

    uint64_t sessionID() const override;
    String documentURI() const override;
    String documentEncoding() const override;
    AccessibilityChildrenVector documentLinks() override { return AccessibilityChildrenVector(); }

protected:
    AccessibilityObject() = default;
    void detachRemoteParts(AccessibilityDetachmentType) override;
    void detachPlatformWrapper(AccessibilityDetachmentType) override;

    AXID m_id { 0 };
    AccessibilityChildrenVector m_children;
    mutable bool m_haveChildren { false };
    AccessibilityRole m_role { AccessibilityRole::Unknown };
    AccessibilityObjectInclusion m_lastKnownIsIgnoredValue { AccessibilityObjectInclusion::DefaultBehavior };
    AccessibilityIsIgnoredFromParentData m_isIgnoredFromParentData { };
    bool m_childrenDirty { false };
    bool m_subtreeDirty { false };
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    bool m_isolatedTreeNodeInitialized { false };
#endif

    void setIsIgnoredFromParentData(AccessibilityIsIgnoredFromParentData& data) override { m_isIgnoredFromParentData = data; }

    virtual bool computeAccessibilityIsIgnored() const { return true; }
    bool isAccessibilityObject() const override { return true; }

    // If this object itself scrolls, return its ScrollableArea.
    virtual ScrollableArea* getScrollableAreaIfScrollable() const { return nullptr; }
    virtual void scrollTo(const IntPoint&) const { }
    ScrollableArea* scrollableAreaAncestor() const;
    void scrollAreaAndAncestor(std::pair<ScrollableArea*, AccessibilityObject*>&) const;

    virtual AccessibilityRole buttonRoleType() const;
    String rolePlatformDescription() const;
    bool isOnScreen() const override;
    bool dispatchTouchEvent();

    void ariaElementsFromAttribute(AccessibilityChildrenVector&, const QualifiedName&) const;
    void ariaElementsReferencedByAttribute(AccessibilityChildrenVector&, const QualifiedName&) const;

    AccessibilityObject* radioGroupAncestor() const;

#if ENABLE(ACCESSIBILITY) && USE(ATK)
    bool allowsTextRanges() const;
    unsigned getLengthForTextRange() const;
#else
    bool allowsTextRanges() const { return isTextControl(); }
    unsigned getLengthForTextRange() const { return text().length(); }
#endif
};

#if !ENABLE(ACCESSIBILITY)
inline const AccessibilityObject::AccessibilityChildrenVector& AccessibilityObject::children(bool) { return m_children; }
inline String AccessibilityObject::actionVerb() const { return emptyString(); }
inline int AccessibilityObject::lineForPosition(const VisiblePosition&) const { return -1; }
inline void AccessibilityObject::updateBackingStore() { }
inline void AccessibilityObject::detachPlatformWrapper(AccessibilityDetachmentType) { }
#endif

AccessibilityObject* firstAccessibleObjectFromNode(const Node*, const WTF::Function<bool(const AccessibilityObject&)>& isAccessible);

namespace Accessibility {

using PlatformRoleMap = HashMap<AccessibilityRole, String, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;

PlatformRoleMap createPlatformRoleMap();
String roleToPlatformString(AccessibilityRole);

} // namespace Accessibility

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::AXCoreObject& object) { return object.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AccessibilityObject)
static bool isType(const WebCore::AXCoreObject& context) { return context.isAccessibilityObject(); }
SPECIALIZE_TYPE_TRAITS_END()
