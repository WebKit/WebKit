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

#include <WebCore/AXCoreObject.h>
#include <WebCore/AXTextRun.h>
#include <WebCore/CharacterRange.h>
#include <WebCore/FloatQuad.h>
#include <WebCore/LayoutRect.h>
#include <WebCore/Path.h>
#include <WebCore/TextIterator.h>
#include <iterator>
#include <wtf/CompactUniquePtrTuple.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/Platform.h>
#include <wtf/RefPtr.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/Vector.h>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSData;
OBJC_CLASS NSMutableAttributedString;
OBJC_CLASS NSString;
OBJC_CLASS NSValue;
OBJC_CLASS NSView;
#endif

namespace WebCore {
class AXObjectRareData;
}

WTF_ALLOW_COMPACT_POINTERS_TO_INCOMPLETE_TYPE(WebCore::AXObjectRareData);

namespace WebCore {

class IntPoint;
class IntSize;
class ScrollableArea;

enum class CommandType: uint8_t;

class AccessibilityObject : public AXCoreObject {
public:
    virtual ~AccessibilityObject();

    std::optional<AXID> treeID() const final;
    String debugDescriptionInternal(bool, std::optional<OptionSet<AXDebugStringOption>> = std::nullopt) const final;
    virtual String extraDebugInfo() const { return emptyString(); }

    // After constructing an AccessibilityObject, it must be given a
    // unique ID, then added to AXObjectCache, and finally init() must
    // be called last.
    virtual void init();

    // Prefer using the dedicated functions over consuming these flag values directly, as the flags can sometimes be uninitialized.
    // Also, the dedicated functions traverse for you if the flags aren't yet initialized.
    // For example, use `isInRow()` instead of `ancestorFlags().contains(AXAncestorFlag::IsInRow)`.
    OptionSet<AXAncestorFlag> ancestorFlags() const { return m_ancestorFlags; }

    void addAncestorFlags(const OptionSet<AXAncestorFlag>& flags) { m_ancestorFlags.add(flags); }
    bool ancestorFlagsAreInitialized() const { return m_ancestorFlags.contains(AXAncestorFlag::FlagsInitialized); }
    // Computes the flags that this object matches (no traversal is done).
    OptionSet<AXAncestorFlag> computeAncestorFlags() const;
    // Computes the flags that this object and all ancestors match, traversing all the way to the root.
    OptionSet<AXAncestorFlag> computeAncestorFlagsWithTraversal() const;
    void initializeAncestorFlags(const OptionSet<AXAncestorFlag>&);
    bool hasAncestorMatchingFlag(AXAncestorFlag) const;
    bool matchesAncestorFlag(AXAncestorFlag) const;

    bool hasRareData() const { return !!m_rareDataWithBitfields.pointer(); }
    AXObjectRareData* rareData() const { return m_rareDataWithBitfields.pointer(); }
    AXObjectRareData& ensureRareData();
    bool needsRareData() const { return isTable() || isExposedTableRow(); }

    bool hasDirtySubtree() const { return m_subtreeDirty; }

    bool isInDescriptionListDetail() const;
    bool isInDescriptionListTerm() const final;
    bool isInCell() const;
    bool isInRow() const;

    bool isDetached() const override;

    bool isAccessibilityNodeObject() const override { return false; }
    bool isAccessibilityRenderObject() const override { return false; }
    virtual bool isAccessibilityScrollbar() const { return false; }
    bool isAXLocalFrame() const override { return false; }
    bool isAXRemoteFrame() const override { return false; }
    virtual bool isAccessibilityScrollViewInstance() const { return false; }
    virtual bool isAccessibilitySVGRoot() const { return false; }
    virtual bool isAccessibilitySVGObjectInstance() const { return false; }
    virtual bool isAccessibilityTableColumnInstance() const { return false; }
    virtual bool isAccessibilityListBoxInstance() const { return false; }
    virtual bool isAccessibilityListBoxOptionInstance() const { return false; }
    bool isAXIsolatedObjectInstance() const final { return false; }

    bool isSecureField() const override { return false; }
    bool isContainedBySecureField() const;
    bool isNativeTextControl() const override { return false; }
    virtual bool isSearchField() const { return false; }
    bool isAttachment() const override { return false; }
#if ENABLE(ATTACHMENT_ELEMENT)
    virtual bool isAttachmentElement() const { return false; }
#endif
    bool isMediaTimeline() const { return false; }
    virtual bool isSliderThumb() const { return false; }
    virtual bool isNativeLabel() const { return false; }
    bool isLabelByRelation() const { return labelForObjects().size(); }
    bool isLabel() const { return isNativeLabel() || isLabelByRelation(); }
    // FIXME: Re-evaluate what this means when site isolation is enabled (is this method name accurate?)
    virtual bool isRoot() const { return false; }

    std::optional<InputType::Type> inputType() const final;

    virtual bool isAccessibilityList() const { return false; }
    virtual bool isUnorderedList() const { return false; }
    virtual bool isOrderedList() const { return false; }
    bool isDescriptionList() const override { return false; }

    bool hasTreeRole() const;
    bool hasTreeItemRole() const;

    // Table support.
    bool isTable() const override { return false; }
    virtual bool isAriaTable() const { return false; }
    bool isExposableTable() const override { return false; }
    virtual void recomputeIsExposableIfNecessary() { };
    AccessibilityChildrenVector columns() override { return AccessibilityChildrenVector(); }
    AccessibilityChildrenVector rows() override { return AccessibilityChildrenVector(); }
    unsigned columnCount() override { return 0; }
    unsigned rowCount() override { return 0; }
    AccessibilityChildrenVector cells() override { return AccessibilityChildrenVector(); }
    AccessibilityObject* cellForColumnAndRow(unsigned, unsigned) override { return nullptr; }
    AccessibilityChildrenVector rowHeaders() override { return AccessibilityChildrenVector(); }
    AccessibilityChildrenVector visibleRows() override { return AccessibilityChildrenVector(); }
    String cellScope() const final { return getAttribute(HTMLNames::scopeAttr); }
    AccessibilityObject* tableHeaderContainer() override { return nullptr; }
    int axColumnCount() const override { return 0; }
    int axRowCount() const override { return 0; }
    virtual Vector<Vector<Markable<AXID>>> cellSlots() { return { }; }
    // Cell indexes are assigned during child creation, so make sure children are up-to-date.
    void ensureCellIndexesUpToDate() { updateChildrenIfNecessary(); }

    // Table cell support.
    bool isTableCell() const override { return false; }
    bool isExposedTableCell() const override { return false; }
    AXCoreObject* parentTableIfTableCell() const override { return nullptr; }
    // Returns the start location and row span of the cell.
    std::pair<unsigned, unsigned> rowIndexRange() const override { return { 0, 1 }; }
    // Returns the start location and column span of the cell.
    std::pair<unsigned, unsigned> columnIndexRange() const override { return { 0, 1 }; }
    std::optional<unsigned> axColumnIndex() const override { return std::nullopt; }
    std::optional<unsigned> axRowIndex() const override { return std::nullopt; }
    String axColumnIndexText() const override { return { }; }
    String axRowIndexText() const override { return { }; }

    // Table column support.
    unsigned columnIndex() const override { return 0; }

    // Table row support.
    AccessibilityObject* parentTable() const override { return nullptr; }
    bool isTableRow() const override { return false; }
    AXCoreObject* parentTableIfExposedTableRow() const override { return nullptr; };
    bool isExposedTableRow() const override { return false; }
    unsigned rowIndex() const override { return 0; }
    bool ignoredByRowAncestor() const;

    // ARIA tree/grid row support.
    bool isARIAGridRow() const override { return false; }
    bool isARIATreeGridRow() const override { return false; }
    AccessibilityChildrenVector disclosedRows() override; // ARIATreeItem implementation. Table rows are handled in AccessibilityNodeObject();
    AccessibilityObject* disclosedByRow() const override { return nullptr; }

    void postMenuClosedNotificationIfNecessary() const;

    bool isFieldset() const override { return false; }
    virtual bool isMenuList() const { return false; }
    virtual bool isMenuListPopup() const { return false; }
    virtual bool isMenuListOption() const { return false; }
    virtual bool isNativeSpinButton() const { return false; }
    AccessibilityObject* incrementButton() override { return nullptr; }
    AccessibilityObject* decrementButton() override { return nullptr; }
    virtual bool isSpinButtonPart() const { return false; }
    virtual bool isIncrementor() const { return false; }
    bool isMockObject() const override { return false; }
    virtual bool isMediaObject() const { return false; }
    bool isARIATextControl() const;
    bool isEditableWebArea() const final;
    bool isNonNativeTextControl() const final;
    bool isRangeControl() const;
    bool isStyleFormatGroup() const;
    bool isFigureElement() const;
    bool isKeyboardFocusable() const final;
    bool isOutput() const final;

    bool isChecked() const override { return false; }
    bool isEnabled() const override { return false; }
    bool isSelected() const override;
    bool isTabItemSelected() const;
    bool isFocused() const override { return false; }
    bool isIndeterminate() const override { return false; }
    bool isLoaded() const final;
    bool isMultiSelectable() const override { return false; }
    bool isOffScreen() const override { return false; }
    bool isPressed() const override { return false; }
    bool isVisited() const final;
    bool isRequired() const override { return false; }
    bool isExpanded() const final;
    inline bool isVisible() const override;
    virtual bool isCollapsed() const { return false; }
    void setIsExpanded(bool) override { }
    FloatRect unobscuredContentRect() const;
    FloatRect relativeFrame() const final;
#if PLATFORM(MAC)
    FloatRect primaryScreenRect() const final;
#endif
    FloatRect convertFrameToSpace(const FloatRect&, AccessibilityConversionSpace) const final;
    HashMap<String, AXEditingStyleValueVariant> resolvedEditingStyles() const;

    // In a multi-select list, many items can be selected but only one is active at a time.
    bool isSelectedOptionActive() const override { return false; }

    bool hasBoldFont() const override { return false; }
    bool hasItalicFont() const override { return false; }
    Vector<AXTextMarkerRange> misspellingRanges() const final;
    std::optional<SimpleRange> misspellingRange(const SimpleRange& start, AccessibilitySearchDirection) const final;
    bool hasPlainText() const override { return false; }
    bool hasSameFont(AXCoreObject&) override { return false; }
    bool hasSameFontColor(AXCoreObject&) override { return false; }
    bool hasSameStyle(AXCoreObject&) override { return false; }
    bool hasUnderline() const override { return false; }
    AXTextMarkerRange textInputMarkedTextMarkerRange() const final;

    WallTime dateTimeValue() const override { return { }; }
    DateComponentsType dateTimeComponentsType() const final;
    String datetimeAttributeValue() const final;

    bool canSetFocusAttribute() const override { return false; }
    bool canSetValueAttribute() const override { return false; }
    bool canSetSelectedAttribute() const override { return false; }

    Element* element() const final;
    Node* node() const override { return nullptr; }
    RenderObject* renderer() const override { return nullptr; }
    RenderObject* rendererOrNearestAncestor() const;
    // Resolves the computed style if necessary (and safe to do so).
    const RenderStyle* style() const;

    // Note: computeIsIgnored does not consider whether an object is ignored due to presence of modals.
    // Use isIgnored as the word of law when determining if an object is ignored.
    virtual bool computeIsIgnored() const { return true; }
    bool isIgnored() const final;
    inline void recomputeIsIgnored();
    void recomputeIsIgnoredForDescendants(bool includeSelf = false);
    AccessibilityObjectInclusion defaultObjectInclusion() const;
    inline bool isIgnoredByDefault() const;
    bool includeIgnoredInCoreTree() const;
    bool isARIAHidden() const;

    bool isShowingValidationMessage() const;
    String validationMessage() const;

    AccessibilityButtonState checkboxOrRadioValue() const override;
    String valueDescription() const override { return String(); }
    float valueForRange() const override { return 0.0f; }
    float maxValueForRange() const override { return 0.0f; }
    float minValueForRange() const override { return 0.0f; }
    virtual float stepValueForRange() const { return 0.0f; }
    int layoutCount() const override { return 0; }
    double loadingProgress() const final;
    WEBCORE_EXPORT static bool isARIAControl(AccessibilityRole);
    bool supportsCheckedState() const override;

    bool supportsARIAOwns() const override { return false; }

    String explicitPopupValue() const final;
    bool hasDatalist() const;
    bool supportsHasPopup() const final;
    bool pressedIsPresent() const final;
    bool ariaIsMultiline() const;
    String explicitInvalidStatus() const final;
    bool supportsPressed() const;
    bool supportsExpanded() const final;
    bool supportsChecked() const final;
    bool supportsRowCountChange() const;
    AccessibilitySortDirection sortDirection() const final;
    virtual bool hasElementDescendant() const { return false; }
    String identifierAttribute() const final;
    String linkRelValue() const final;
    Vector<String> classList() const final;
    AccessibilityCurrentState currentState() const final;
    bool supportsCurrent() const final;
    bool supportsKeyShortcuts() const final;
    String keyShortcuts() const final;

    // This function checks if the object should be ignored when there's a modal dialog displayed.
    virtual bool ignoredFromModalPresence() const;
    bool isModalDescendant(Node&) const;
    bool isModalNode() const final;

    bool supportsSetSize() const final;
    bool supportsPosInSet() const final;
    int setSize() const final;
    int posInSet() const final;

    // ARIA drag and drop
    bool supportsDropping() const override { return false; }
    bool supportsDragging() const override { return false; }
    bool isGrabbed() override { return false; }
    void setARIAGrabbed(bool) override { }
    Vector<String> determineDropEffects() const override { return { }; }

    // Called on the root AX object to return the deepest available element.
    AccessibilityObject* accessibilityHitTest(const IntPoint&) const override { return nullptr; }
    // Called on the AX object after the render tree determines which is the right AccessibilityRenderObject.
    virtual AccessibilityObject* elementAccessibilityHitTest(const IntPoint&) const;

    AccessibilityObject* focusedUIElement() const final;
    AccessibilityObject* focusedUIElementInAnyLocalFrame() const final;

    virtual AccessibilityObject* firstChild() const { return nullptr; }
    virtual AccessibilityObject* lastChild() const { return nullptr; }
    virtual AccessibilityObject* previousSibling() const { return nullptr; }
    virtual AccessibilityObject* nextSibling() const { return nullptr; }
    AccessibilityObject* nextSiblingUnignored(unsigned limit = std::numeric_limits<unsigned>::max()) const;
    AccessibilityObject* previousSiblingUnignored(unsigned limit = std::numeric_limits<unsigned>::max()) const;
    AccessibilityObject* parentObject() const override { return nullptr; }
    AccessibilityObject* displayContentsParent() const;
    AccessibilityObject* parentObjectUnignored() const final { return downcast<AccessibilityObject>(AXCoreObject::parentObjectUnignored()); }
    static AccessibilityObject* firstAccessibleObjectFromNode(const Node*);
    AccessibilityChildrenVector findMatchingObjects(AccessibilitySearchCriteria&&) final;
    virtual bool isDescendantOfBarrenParent() const { return false; }

#if ENABLE_ACCESSIBILITY_LOCAL_FRAME
    AccessibilityObject* crossFrameParentObject() const override { return nullptr; }
    AccessibilityObject* crossFrameChildObject() const override { return nullptr; }
#endif

    bool isDescendantOfRole(AccessibilityRole) const final;

    // Text selection
    Vector<SimpleRange> findTextRanges(const AccessibilitySearchTextCriteria&) const final;
    Vector<String> performTextOperation(const AccessibilityTextOperation&) final;

    virtual AccessibilityObject* observableObject() const { return nullptr; }
    virtual AccessibilityObject* controlForLabelElement() const { return nullptr; }
    AccessibilityObject* scrollBar(AccessibilityOrientation) override { return nullptr; }
    AccessibilityObject* internalLinkElement() const override { return nullptr; }
    AccessibilityChildrenVector radioButtonGroup() const override { return { }; }

    virtual void recomputeAriaRole() { }
    virtual AccessibilityRole ariaRoleAttribute() const { return AccessibilityRole::Unknown; }
    bool hasExplicitGenericRole() const { return ariaRoleAttribute() == AccessibilityRole::Generic; }
    bool hasImplicitGenericRole() const { return role() == AccessibilityRole::Generic && !hasExplicitGenericRole(); }
    bool ariaRoleHasPresentationalChildren() const;
    bool inheritsPresentationalRole() const override { return false; }

    // Accessibility Text
    void accessibilityText(Vector<AccessibilityText>&) const override { };
    // A single method for getting a computed label for an AXObject. It condenses the nuances of accessibilityText. Used by Inspector.
    WEBCORE_EXPORT String computedLabel();

    // A programmatic way to set a name on an AccessibleObject.
    void setAccessibleName(const AtomString&) override { }
    virtual bool hasAttributesRequiredForInclusion() const { return false; }

    String description() const override { return { }; }
    virtual String helpText() const { return { }; }
    String altTextFromAttributeOrStyle() const;

    std::optional<String> textContent() const final;
    bool hasTextContent() const;
#if PLATFORM(COCOA)
    bool hasAttributedText() const;
#endif
    String textContentPrefixFromListMarker() const override;

    // Methods for determining accessibility text.
    bool isARIAStaticText() const { return ariaRoleAttribute() == AccessibilityRole::StaticText; }
    virtual Vector<Vector<AXID>> stitchGroups() const { return { }; }
    // Whether this object should cache a string value when an isolated object is created for it.
    bool shouldCacheStringValue() const;
    String stringValue() const override { return { }; }
    bool dependsOnTextUnderElement() const;
    String textUnderElement(TextUnderElementMode = { }) const override { return { }; }
    String text() const override { return { }; }
    unsigned textLength() const final;
    String revealableText() const override { return nullString(); }
    bool isHiddenUntilFoundContainer() const override { return false; }
#if ENABLE(AX_THREAD_TEXT_APIS)
    virtual AXTextRuns textRuns() { return { }; }
    bool hasTextRuns() final { return textRuns().size(); }
    TextEmissionBehavior textEmissionBehavior() const override { return TextEmissionBehavior::None; }
    AXTextRunLineID listMarkerLineID() const override { return { }; }
    String listMarkerText() const override { return { }; }
    FontOrientation fontOrientation() const final;
#endif // ENABLE(AX_THREAD_TEXT_APIS)
#if PLATFORM(COCOA)
    // Returns an array of strings and AXObject wrappers corresponding to the
    // textruns and replacement nodes included in the given range.
    RetainPtr<NSArray> contentForRange(const SimpleRange&, SpellCheck = SpellCheck::No) const;
    RetainPtr<NSAttributedString> attributedStringForRange(const SimpleRange&, SpellCheck) const;
    RetainPtr<NSAttributedString> attributedStringForTextMarkerRange(AXTextMarkerRange&&, SpellCheck = SpellCheck::No) const override;
    // The following functions are PLATFORM(COCOA) because these are currently only used to power a
    // Cocoa API (attributed strings).
    AttributedStringStyle stylesForAttributedString() const final;
    RetainPtr<CTFontRef> font() const final;
    Color textColor() const final;
    Color backgroundColor() const;
    bool isSubscript() const;
    bool isSuperscript() const;
    bool hasTextShadow() const;
    LineDecorationStyle lineDecorationStyle() const;
#endif
    virtual String ariaLabeledByAttribute() const { return String(); }
    virtual String ariaDescribedByAttribute() const { return String(); }
    const String placeholderValue() const final;
    bool accessibleNameDerivesFromContent() const;
    bool accessibleNameDerivesFromHeading() const;
    String brailleLabel() const override { return getAttribute(HTMLNames::aria_braillelabelAttr); }
    String brailleRoleDescription() const override { return getAttribute(HTMLNames::aria_brailleroledescriptionAttr); }
    String embeddedImageDescription() const final;
    std::optional<AccessibilityChildrenVector> imageOverlayElements() override { return std::nullopt; }
    String extendedDescription() const final;

    // Abbreviations
    String abbreviation() const final { return getAttribute(HTMLNames::abbrAttr); }

    Vector<Ref<Element>> elementsFromAttribute(const QualifiedName&) const;

    // Only if isColorWell()
    SRGBA<uint8_t> colorValue() const override;

    virtual AccessibilityRole determineAccessibilityRole() = 0;
    String subrolePlatformString() const final;

    String ariaRoleDescription() const final { return getAttributeTrimmed(HTMLNames::aria_roledescriptionAttr); };

    inline AXObjectCache* axObjectCache() const;

    static AccessibilityObject* anchorElementForNode(Node&);
    static AccessibilityObject* headingElementForNode(Node*);
    virtual Element* anchorElement() const { return nullptr; }
    virtual RefPtr<Element> popoverTargetElement() const { return nullptr; }
    virtual RefPtr<Element> commandForElement() const { return nullptr; }
    Element* actionElement() const override { return nullptr; }
    virtual LayoutRect boundingBoxRect() const { return { }; }
    LayoutRect elementRect() const override = 0;
#if PLATFORM(COCOA)
    FloatPoint screenRelativePosition() const final;
#else
    FloatPoint screenRelativePosition() const final { return convertFrameToSpace(elementRect(), AccessibilityConversionSpace::Screen).location(); }
#endif
    IntSize size() const final { return snappedIntRect(elementRect()).size(); }
    IntPoint clickPoint() final;
    IntPoint clickPointFromElementRect() const;
    static IntRect boundingBoxForQuads(RenderObject*, const Vector<FloatQuad>&);
    Path elementPath() const override { return Path(); }
    bool supportsPath() const override { return false; }

    TextIteratorBehaviors textIteratorBehaviorForTextRange() const;
    static TextIterator textIteratorIgnoringFullSizeKana(const SimpleRange&);
    CharacterRange selectedTextRange() const override { return { }; }
    int insertionPointLineNumber() const override { return -1; }

    URL url() const override { return URL(); }
    VisibleSelection selection() const final;
    String selectedText() const override { return String(); }
    String accessKey() const override { return nullAtom(); }
    String localizedActionVerb() const final;
    String actionVerb() const final;

    bool isWidget() const override { return false; }
    Widget* widget() const override { return nullptr; }
    PlatformWidget platformWidget() const override { return nullptr; }
    Widget* widgetForAttachmentView() const override { return nullptr; }
    bool isPlugin() const override { return false; }

    IntPoint remoteFrameOffset() const final;
#if PLATFORM(COCOA)
    RetainPtr<RemoteAXObjectRef> remoteParent() const final;
    FloatRect convertRectToPlatformSpace(const FloatRect&, AccessibilityConversionSpace) const final;
    RetainPtr<id> remoteFramePlatformElement() const override { return nil; }
#endif
    bool hasRemoteFrameChild() const override { return false; }

    Page* page() const final;
    Document* document() const override;
    RefPtr<Document> protectedDocument() const;
    LocalFrameView* documentFrameView() const override;
    inline LocalFrame* frame() const;
    RefPtr<LocalFrame> localMainFrame() const;
    Document* topDocument() const;
    RenderView* topRenderer() const;
    virtual ScrollView* scrollView() const { return nullptr; }
    unsigned ariaLevel() const final;
    String language() const final;
    // 1-based, to match the aria-level spec.
    bool isInlineText() const final;

    // Ensures that the view is focused and active before attempting to set focus to an AccessibilityObject.
    // Subclasses that override setFocused should call this base implementation first.
    void setFocused(bool) override;

    void setSelectedText(const String&) override { }
    void setSelectedTextRange(CharacterRange&&) override { }
    bool setValue(const String&) override { return false; }
    void setValueIgnoringResult(const String& value) final { setValue(value); }
    bool replaceTextInRange(const String&, const CharacterRange&) final;
    bool insertText(const String&) final;

    bool setValue(float) override { return false; }
    void setValueIgnoringResult(float value) final { setValue(value); }
    void setSelected(bool) override { }
    void setSelectedRows(AccessibilityChildrenVector&&) final;

    void performDismissActionIgnoringResult() final { performDismissAction(); }
    bool press() override;

    std::optional<AccessibilityOrientation> explicitOrientation() const override { return std::nullopt; }
    void increment() override { }
    void decrement() override { }
    virtual bool toggleDetailsAncestor() { return false; }
    // Reveals details elements and hidden="until-found" elements.
    virtual void revealAncestors() { }

    void updateRole();
    bool childrenInitialized() const { return m_childrenInitialized; }
    const AccessibilityChildrenVector& children(bool updateChildrenIfNeeded = true) final;
    virtual void addChildren() { }
    enum class DescendIfIgnored : bool { No, Yes };
    void insertChild(AccessibilityObject&, unsigned, DescendIfIgnored = DescendIfIgnored::Yes);
    void insertChild(AccessibilityObject* object, unsigned index, DescendIfIgnored descend = DescendIfIgnored::Yes)
    {
        if (object)
            insertChild(*object, index, descend);
    }
    void addChild(AccessibilityObject& object, DescendIfIgnored descend = DescendIfIgnored::Yes)
    {
        insertChild(object, m_children.size(), descend);
    }
    void addChild(AccessibilityObject* object, DescendIfIgnored descend = DescendIfIgnored::Yes)
    {
        if (object)
            addChild(*object, descend);
    }
    virtual bool canHaveChildren() const { return true; }
    virtual void updateChildrenIfNecessary();
    virtual void setNeedsToUpdateChildren() { }
    virtual void setNeedsToUpdateSubtree() { }
    virtual void clearChildren();
    virtual bool needsToUpdateChildren() const { return false; }
#if PLATFORM(COCOA)
    void detachFromParent() override;
#else
    void detachFromParent() override { }
#endif
    virtual bool isDetachedFromParent() { return false; }

    void setSelectedChildren(const AccessibilityChildrenVector&) override { }
    AccessibilityChildrenVector visibleChildren() override { return { }; }
    bool shouldFocusActiveDescendant() const;

    WEBCORE_EXPORT static AccessibilityRole ariaRoleToWebCoreRole(const String&);
    static AccessibilityRole ariaRoleToWebCoreRole(const String&, NOESCAPE const Function<bool(const AccessibilityRole&)>&);
    bool hasAttribute(const QualifiedName&) const;
    const AtomString& getAttribute(const QualifiedName&) const;
    String getAttributeTrimmed(const QualifiedName&) const;

    String nameAttribute() const final { return getAttribute(HTMLNames::nameAttr); }
    String titleAttribute() const final { return getAttribute(HTMLNames::titleAttr); }
    int integralAttribute(const QualifiedName&) const;
    bool hasElementName(const ElementName) const final;
    bool hasAttachmentTag() const final { return hasElementName(ElementName::HTML_attachment); }
    bool hasBodyTag() const final { return hasElementName(ElementName::HTML_body); }
    bool hasMarkTag() const final { return hasElementName(ElementName::HTML_mark); }
    bool hasRowGroupTag() const final;

    ElementName elementName() const final;
    bool hasDisplayContents() const;

    std::optional<SimpleRange> simpleRange() const final;
    VisiblePositionRange visiblePositionRange() const override { return { }; }
    AXTextMarkerRange textMarkerRange() const final;

#if PLATFORM(COCOA)
    std::optional<NSRange> visibleCharacterRange() const final;
#endif
    VisiblePositionRange visiblePositionRangeForLine(unsigned) const override { return VisiblePositionRange(); }

    static bool replacedNodeNeedsCharacter(Node& replacedNode);

    VisiblePositionRange visiblePositionRangeForUnorderedPositions(const VisiblePosition&, const VisiblePosition&) const final;
    VisiblePositionRange leftLineVisiblePositionRange(const VisiblePosition&) const final;
    VisiblePositionRange rightLineVisiblePositionRange(const VisiblePosition&) const final;
    VisiblePositionRange styleRangeForPosition(const VisiblePosition&) const final;
    VisiblePositionRange visiblePositionRangeForRange(const CharacterRange&) const;
    VisiblePositionRange lineRangeForPosition(const VisiblePosition&) const final;
    virtual VisiblePositionRange selectedVisiblePositionRange() const { return { }; }

    std::optional<SimpleRange> rangeForCharacterRange(const CharacterRange&) const final;
#if PLATFORM(COCOA)
    AXTextMarkerRange textMarkerRangeForNSRange(const NSRange&) const final;
#endif
#if PLATFORM(MAC)
    AXTextMarkerRange selectedTextMarkerRange() const final;
#endif
    static String stringForVisiblePositionRange(const VisiblePositionRange&);
    virtual IntRect boundsForVisiblePositionRange(const VisiblePositionRange&) const { return IntRect(); }
    IntRect boundsForRange(const SimpleRange&) const final;
    void setSelectedVisiblePositionRange(const VisiblePositionRange&) const override { }

    VisiblePosition visiblePositionForPoint(const IntPoint&) const final;
    VisiblePosition nextLineEndPosition(const VisiblePosition&) const final;
    VisiblePosition previousLineStartPosition(const VisiblePosition&) const final;
    VisiblePosition visiblePositionForIndex(unsigned, bool /* lastIndexOK */) const override { return VisiblePosition(); }

    VisiblePosition visiblePositionForIndex(int) const override { return VisiblePosition(); }
    int indexForVisiblePosition(const VisiblePosition&) const override { return 0; }

    int lineForPosition(const VisiblePosition&) const final;
    CharacterRange plainTextRangeForVisiblePositionRange(const VisiblePositionRange&) const;
    virtual int index(const VisiblePosition&) const { return -1; }

    CharacterRange doAXRangeForLine(unsigned) const override { return { }; }
    CharacterRange characterRangeForPoint(const IntPoint&) const override;
    CharacterRange doAXRangeForIndex(unsigned) const override { return { }; }
    CharacterRange doAXStyleRangeForIndex(unsigned) const override;

    String doAXStringForRange(const CharacterRange&) const override { return { }; }
    IntRect doAXBoundsForRange(const CharacterRange&) const override { return { }; }
    IntRect doAXBoundsForRangeUsingCharacterOffset(const CharacterRange&) const override { return { }; }
    static StringView listMarkerTextForNodeAndPosition(Node*, Position&&);

    unsigned doAXLineForIndex(unsigned) final;

    WEBCORE_EXPORT String computedRoleString() const final;

    virtual String secureFieldValue() const { return String(); }
    bool isValueAutofillAvailable() const final;
    AutoFillButtonType valueAutofillButtonType() const final;

    // ARIA live-region features.
    AccessibilityObject* liveRegionAncestor(bool excludeIfOff = true) const final { return Accessibility::liveRegionAncestor(*this, excludeIfOff); }
    const String explicitLiveRegionStatus() const override { return String(); }
    const String explicitLiveRegionRelevant() const override { return nullAtom(); }
    bool liveRegionAtomic() const override { return false; }
    bool isBusy() const override { return false; }
    static bool contentEditableAttributeIsEnabled(Element&);
    bool hasContentEditableAttributeSet() const;

    bool supportsReadOnly() const;
    virtual String readOnlyValue() const;

    bool supportsAutoComplete() const;
    String explicitAutoCompleteValue() const final;

    bool hasARIAValueNow() const { return hasAttribute(HTMLNames::aria_valuenowAttr); }
    bool supportsARIAAttributes() const;

    // Make this object visible by scrolling as many nested scrollable views as needed.
    void scrollToMakeVisible() const final;
    // Same, but if the whole object can't be made visible, try for this subrect, in local coordinates.
    void scrollToMakeVisibleWithSubFocus(IntRect&&) const final;
    // Scroll this object to a given point in global coordinates of the top-level window.
    void scrollToGlobalPoint(IntPoint&&) const final;

    enum class ScrollByPageDirection { Up, Down, Left, Right };
    bool scrollByPage(ScrollByPageDirection) const;
    IntPoint scrollPosition() const;
    IntSize scrollContentsSize() const;
    IntRect scrollVisibleContentRect() const;
    void scrollToMakeVisible(const ScrollRectToVisibleOptions&) const;

    // All math elements return true for isMathElement().
    bool isMathElement() const override { return false; }
    bool isMathFraction() const override { return false; }
    bool isMathFenced() const override { return false; }
    bool isMathSubscriptSuperscript() const override { return false; }
    bool isMathRow() const override { return false; }
    bool isMathUnderOver() const override { return false; }
    bool isMathRoot() const override { return false; }
    bool isMathSquareRoot() const override { return false; }
    virtual bool isMathText() const { return false; }
    virtual bool isMathNumber() const { return false; }
    virtual bool isMathOperator() const { return false; }
    virtual bool isMathFenceOperator() const { return false; }
    virtual bool isMathSeparatorOperator() const { return false; }
    virtual bool isMathIdentifier() const { return false; }
    bool isMathTable() const override { return false; }
    bool isMathTableRow() const override { return false; }
    bool isMathTableCell() const override { return false; }
    bool isMathMultiscript() const override { return false; }
    bool isMathToken() const override { return false; }
    virtual bool isMathScriptObject(AccessibilityMathScriptObjectType) const { return false; }
    virtual bool isMathMultiscriptObject(AccessibilityMathMultiscriptObjectType) const { return false; }

    // Root components.
    std::optional<AccessibilityChildrenVector> mathRadicand() override { return std::nullopt; }
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
    bool isAXHidden() const;
    bool isRenderHidden() const;
    inline bool isHidden() const;
    bool isOnScreen() const final;

#if PLATFORM(MAC)
    void overrideAttachmentParent(AccessibilityObject* parent);
#else
    void overrideAttachmentParent(AccessibilityObject*) { }
#endif

    // A platform-specific method for determining if an attachment is ignored.
    bool accessibilityIgnoreAttachment() const;
    // Gives platforms the opportunity to indicate if an object should be included.
    AccessibilityObjectInclusion accessibilityPlatformIncludesObject() const;

#if PLATFORM(IOS_FAMILY)
    unsigned accessibilitySecureFieldLength() final;
    bool hasTouchEventListener() const final;
#endif

    // allows for an AccessibilityObject to update its render tree or perform
    // other operations update type operations
    void updateBackingStore() final;

#if PLATFORM(COCOA)
    bool preventKeyboardDOMEventDispatch() const final;
    void setPreventKeyboardDOMEventDispatch(bool) final;
    Style::SpeakAs speakAs() const final;
    bool hasApplePDFAnnotationAttribute() const final { return hasAttribute(HTMLNames::x_apple_pdf_annotationAttr); }
#endif

#if PLATFORM(MAC)
    bool caretBrowsingEnabled() const final;
    void setCaretBrowsingEnabled(bool) final;

    AccessibilityChildrenVector allSortedLiveRegions() const final;
    AccessibilityChildrenVector allSortedNonRootWebAreas() const final;
#endif // PLATFORM(MAC)

    bool hasClickHandler() const override { return false; }
    bool hasCursorPointer() const override { return false; }
    bool hasPointerEventsNone() const override { return false; }
    bool showsCursorOnHover() const override { return false; }
    AccessibilityObject* clickableSelfOrAncestor(ClickHandlerFilter filter = ClickHandlerFilter::ExcludeBody) const final { return Accessibility::clickableSelfOrAncestor(*this, filter); };
    AccessibilityObject* focusableAncestor() final { return Accessibility::focusableAncestor(*this); }
    AccessibilityObject* editableAncestor() const final { return Accessibility::editableAncestor(*this); };
    AccessibilityObject* highestEditableAncestor() final { return Accessibility::highestEditableAncestor(*this); }
    AccessibilityObject* exposedTableAncestor(bool includeSelf = false) const final { return Accessibility::exposedTableAncestor(*this, includeSelf); }

    const AccessibilityScrollView* ancestorAccessibilityScrollView(bool includeSelf) const;
    virtual AccessibilityObject* webAreaObject() const { return nullptr; }
    bool isWithinHiddenWebArea() const;
    AccessibilityObject* containingWebArea() const;

    void clearIsIgnoredFromParentData() { m_isIgnoredFromParentData = { }; }
    void setIsIgnoredFromParentDataForChild(AccessibilityObject&);

    AccessibilityChildrenVector documentLinks() override { return AccessibilityChildrenVector(); }

    AccessibilityChildrenVector relatedObjects(AXRelation) const final;

    String innerHTML() const final;
    String outerHTML() const final;

#if ENABLE(MODEL_ELEMENT_ACCESSIBILITY)
    ModelPlayerAccessibilityChildren modelElementChildren() final;
#endif

#if PLATFORM(IOS_FAMILY)
    struct InlineTextPrediction {
        String text;
        size_t location { 0 };
        void reset()
        {
            text = ""_s;
            location = 0;
        }
    };

    InlineTextPrediction& lastPresentedTextPrediction() { return m_lastPresentedTextPrediction; }
    InlineTextPrediction& lastPresentedTextPredictionComplete() { return m_lastPresentedTextPredictionComplete; }
    void setLastPresentedTextPrediction(Node&, CompositionState, const String&, size_t, bool);
#endif // PLATFORM(IOS_FAMILY)

    virtual FloatRect localRect() const { return { }; }
    virtual bool isNonLayerSVGObject() const { return false; }

    // When using the previousSibling and nextSibling methods, we can alternate between walking the DOM and the
    // the render tree. There are complications with this, especially introduced by display:contents, which
    // removes the renderer for the given object, and moves its render tree children up one level higher than they
    // otherwise would've been on. This iterator abstracts over this complexity, ensuring each object is actually a
    // sibling of the last.
    class iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using element_type = AccessibilityObject;
        using pointer = AccessibilityObject*;
        using reference = AccessibilityObject&;

        iterator() = default;
        iterator(const AccessibilityObject* object, const AccessibilityObject* displayContentsParent)
            : m_current(object)
            , m_displayContentsParent(displayContentsParent)
        { }
        iterator(const AccessibilityObject* object)
            : iterator(object, object ? object->displayContentsParent() : nullptr)
        { }
        iterator(const AccessibilityObject* object, const AccessibilityObject& parent)
            : iterator(object, parent.hasDisplayContents() ? &parent : nullptr)
        { }

        explicit operator bool() const { return m_current; }
        AccessibilityObject& operator*() const { return const_cast<AccessibilityObject&>(*m_current); }
        AccessibilityObject* operator->() const { return const_cast<AccessibilityObject*>(m_current.get()); }
        AccessibilityObject* ptr() const { return const_cast<AccessibilityObject*>(m_current.get()); }
        bool operator==(const iterator& other) const { return other.m_current == m_current; }

        // Prefix increment operator (++iterator).
        iterator& operator++()
        {
            m_current = m_current->nextSibling();
            ensureContentsParentValidity();
            return *this;
        }

        // Postfix increment operator (iterator++).
        iterator operator++(int)
        {
            auto copy = *this;
            ++(*this);
            return copy;
        }

        // --iterator
        iterator& operator--()
        {
            m_current = m_current->previousSibling();
            ensureContentsParentValidity();
            return *this;
        }

        // iterator--
        iterator operator--(int)
        {
            auto copy = *this;
            --(*this);
            return copy;
        }
    private:
        void ensureContentsParentValidity()
        {
            RefPtr contentsParent = m_current ? m_current->displayContentsParent() : nullptr;
            if (contentsParent && m_displayContentsParent && contentsParent.get() != m_displayContentsParent.get())
                m_current = nullptr;
        }

        RefPtr<const AccessibilityObject> m_current;
        // If the original object had a display:contents parent, it is stored here. This is nullptr otherwise.
        const RefPtr<const AccessibilityObject> m_displayContentsParent { nullptr };
    }; // class iterator

protected:
    explicit AccessibilityObject(AXID, AXObjectCache&);

    // FIXME: Make more of these member functions private.

    void detachRemoteParts(AccessibilityDetachmentType) override;
    void detachPlatformWrapper(AccessibilityDetachmentType) final;
#if PLATFORM(IOS_FAMILY)
    void markPlatformWrapperIgnoredStateDirty() const;
#else
    void markPlatformWrapperIgnoredStateDirty() const { };
#endif

    void setIsIgnoredFromParentData(AccessibilityIsIgnoredFromParentData& data) { m_isIgnoredFromParentData = data; }
    bool ignoredFromPresentationalRole() const;

    bool isAccessibilityObject() const override { return true; }

    // If this object itself scrolls, return its ScrollableArea.
    virtual ScrollableArea* getScrollableAreaIfScrollable() const { return nullptr; }
    virtual void scrollTo(const IntPoint&) const { }
    ScrollableArea* scrollableAreaAncestor() const;
    void scrollAreaAndAncestor(std::pair<ScrollableArea*, AccessibilityObject*>&) const;

    virtual bool shouldIgnoreAttributeRole() const { return false; }
    virtual AccessibilityRole buttonRoleType() const;
    bool dispatchTouchEvent();

    static bool isARIAInput(AccessibilityRole);

    AccessibilityObject* radioGroupAncestor() const;

    bool allowsTextRanges() const;
    unsigned getLengthForTextRange() const;

#ifndef NDEBUG
    void verifyChildrenIndexInParent() const final { return AXCoreObject::verifyChildrenIndexInParent(m_children); }
#endif
    void resetChildrenIndexInParent() const;

private:
    ProcessID processID() const final { return legacyPresentingApplicationPID(); }
    bool hasAncestorFlag(AXAncestorFlag flag) const { return ancestorFlagsAreInitialized() && m_ancestorFlags.contains(flag); }
    std::optional<SimpleRange> rangeOfStringClosestToRangeInDirection(const SimpleRange&, AccessibilitySearchDirection, const Vector<String>&) const;
    std::optional<SimpleRange> selectionRange() const;
    std::optional<SimpleRange> findTextRange(const Vector<String>& searchStrings, const SimpleRange& start, AccessibilitySearchTextDirection) const;
    std::optional<SimpleRange> visibleCharacterRangeInternal(SimpleRange&, const FloatRect&, const IntRect&) const;
    Vector<BoundaryPoint> previousLineStartBoundaryPoints(const VisiblePosition&, const SimpleRange&, unsigned) const;
    std::optional<VisiblePosition> previousLineStartPositionInternal(const VisiblePosition&) const;
    bool boundaryPointsContainedInRect(const BoundaryPoint&, const BoundaryPoint&, const FloatRect&, bool isFlippedWritingMode) const;
    std::optional<BoundaryPoint> lastBoundaryPointContainedInRect(const Vector<BoundaryPoint>&, const BoundaryPoint&, const FloatRect&, int, int, bool isFlippedWritingMode) const;
    std::optional<BoundaryPoint> lastBoundaryPointContainedInRect(const Vector<BoundaryPoint>& boundaryPoints, const BoundaryPoint& startBoundaryPoint, const FloatRect& targetRect, bool isFlippedWritingMode) const;

    // Note that "withoutCache" refers to the lack of referencing AXComputedObjectAttributeCache in the function, not the AXObjectCache parameter we pass in here.
    bool isIgnoredWithoutCache(AXObjectCache*) const;
    void setLastKnownIsIgnoredValue(bool);

    // Special handling of click point for links.
    IntPoint linkClickPoint();

    virtual CommandType commandType() const;

protected: // FIXME: Make the data members private.
    AccessibilityChildrenVector m_children;
private:
    const WeakPtr<AXObjectCache> m_axObjectCache;
    CompactUniquePtrTuple<AXObjectRareData, uint16_t> m_rareDataWithBitfields;
#if PLATFORM(IOS_FAMILY)
    InlineTextPrediction m_lastPresentedTextPrediction;
    InlineTextPrediction m_lastPresentedTextPredictionComplete;
#endif
};

inline bool AccessibilityObject::hasDisplayContents() const
{
    RefPtr element = this->element();
    return element && element->hasDisplayContents();
}

inline std::optional<BoundaryPoint> AccessibilityObject::lastBoundaryPointContainedInRect(const Vector<BoundaryPoint>& boundaryPoints, const BoundaryPoint& startBoundaryPoint, const FloatRect& targetRect, bool isFlippedWritingMode) const
{
    return lastBoundaryPointContainedInRect(boundaryPoints, startBoundaryPoint, targetRect, 0, boundaryPoints.size() - 1, isFlippedWritingMode);
}

inline VisiblePosition AccessibilityObject::previousLineStartPosition(const VisiblePosition& position) const
{
    return previousLineStartPositionInternal(position).value_or(VisiblePosition());
}

#if !USE(ATSPI)
inline bool AccessibilityObject::allowsTextRanges() const { return true; }
inline unsigned AccessibilityObject::getLengthForTextRange() const { return text().length(); }
#endif

inline bool AccessibilityObject::hasTextContent() const
{
    return isStaticText()
        || role() == AccessibilityRole::Link
        || isTextControl() || isTabItem();
}

#if PLATFORM(COCOA)
inline bool AccessibilityObject::hasAttributedText() const
{
    return (isStaticText() && !isARIAStaticText())
        || role() == AccessibilityRole::Link
        || isTextControl() || isTabItem();
}
#endif

AccessibilityObject* firstAccessibleObjectFromNode(const Node*, NOESCAPE const Function<bool(const AccessibilityObject&)>& isAccessible);

class AXChildIterator {
public:
    AXChildIterator(const AccessibilityObject& parent)
        : m_parent(parent)
    { }

    AccessibilityObject::iterator begin()
    {
        return AccessibilityObject::iterator { m_parent->firstChild(), m_parent.get() };
    }
    AccessibilityObject::iterator end()
    {
        return AccessibilityObject::iterator { };
    }
private:
    const Ref<const AccessibilityObject> m_parent;
}; // class AXChildIterator

#if PLATFORM(COCOA)
// Helpers to extract information from RenderStyle needed for accessibility purposes.
RetainPtr<CTFontRef> fontFrom(const RenderStyle&);
Color textColorFrom(const RenderStyle&);
Color backgroundColorFrom(const RenderStyle&);
#endif // PLATFORM(COCOA)

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_ACCESSIBILITY(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::AXCoreObject& object) { return object.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AccessibilityObject)
static bool isType(const WebCore::AXCoreObject& context) { return context.isAccessibilityObject(); }
SPECIALIZE_TYPE_TRAITS_END()
