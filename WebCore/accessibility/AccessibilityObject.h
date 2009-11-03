/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef AccessibilityObject_h
#define AccessibilityObject_h

#include "IntRect.h"
#include "Range.h"
#include "VisiblePosition.h"
#include "VisibleSelection.h"
#include <wtf/Platform.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#elif PLATFORM(WIN) && !PLATFORM(WINCE)
#include "AccessibilityObjectWrapperWin.h"
#include "COMPtr.h"
#elif PLATFORM(CHROMIUM)
#include "AccessibilityObjectWrapper.h"
#endif

typedef struct _NSRange NSRange;

#ifdef __OBJC__
@class AccessibilityObjectWrapper;
@class NSArray;
@class NSAttributedString;
@class NSData;
@class NSMutableAttributedString;
@class NSString;
@class NSValue;
@class NSView;
#else
class NSArray;
class NSAttributedString;
class NSData;
class NSMutableAttributedString;
class NSString;
class NSValue;
class NSView;
#if PLATFORM(GTK)
typedef struct _AtkObject AtkObject;
typedef struct _AtkObject AccessibilityObjectWrapper;
#else
class AccessibilityObjectWrapper;
#endif
#endif

namespace WebCore {

class AXObjectCache;
class Element;
class Frame;
class FrameView;
class HTMLAnchorElement;
class HTMLAreaElement;
class IntPoint;
class IntSize;
class Node;
class RenderObject;
class RenderListItem;
class VisibleSelection;
class String;
class Widget;

typedef unsigned AXID;

enum AccessibilityRole {
    UnknownRole = 1,
    ButtonRole,
    RadioButtonRole,
    CheckBoxRole,
    SliderRole,
    TabGroupRole,
    TextFieldRole,
    StaticTextRole,
    TextAreaRole,
    ScrollAreaRole,
    PopUpButtonRole,
    MenuButtonRole,
    TableRole,
    ApplicationRole,
    GroupRole,
    RadioGroupRole,
    ListRole,
    ScrollBarRole,
    ValueIndicatorRole,
    ImageRole,
    MenuBarRole,
    MenuRole,
    MenuItemRole,
    ColumnRole,
    RowRole,
    ToolbarRole,
    BusyIndicatorRole,
    ProgressIndicatorRole,
    WindowRole,
    DrawerRole,
    SystemWideRole,
    OutlineRole,
    IncrementorRole,
    BrowserRole,
    ComboBoxRole,
    SplitGroupRole,
    SplitterRole,
    ColorWellRole,
    GrowAreaRole,
    SheetRole,
    HelpTagRole,
    MatteRole,
    RulerRole,
    RulerMarkerRole,
    LinkRole,
    DisclosureTriangleRole,
    GridRole,
    CellRole, 
    ColumnHeaderRole,
    RowHeaderRole,
    // AppKit includes SortButtonRole but it is misnamed and really a subrole of ButtonRole so we do not include it here.

    // WebCore-specific roles
    WebCoreLinkRole,
    ImageMapLinkRole,
    ImageMapRole,
    ListMarkerRole,
    WebAreaRole,
    HeadingRole,
    ListBoxRole,
    ListBoxOptionRole,
    TableHeaderContainerRole,
    DefinitionListTermRole,
    DefinitionListDefinitionRole,
    AnnotationRole,
    SliderThumbRole,
    IgnoredRole,
    TabRole,
    TabListRole,
    TabPanelRole,
    
    // ARIA Grouping roles
    LandmarkApplicationRole,
    LandmarkBannerRole,
    LandmarkComplementaryRole,
    LandmarkContentInfoRole,
    LandmarkMainRole,
    LandmarkNavigationRole,
    LandmarkSearchRole,
    
    ApplicationLogRole,
    ApplicationMarqueeRole,
    ApplicationStatusRole,
    ApplicationTimerRole,
    
    DocumentRole,
    DocumentArticleRole,
    DocumentNoteRole,
    DocumentRegionRole,
    
    UserInterfaceTooltipRole
};

enum AccessibilityOrientation {
    AccessibilityOrientationVertical,
    AccessibilityOrientationHorizontal,
};
    
enum AccessibilityObjectPlatformInclusion {
    IncludeObject,
    IgnoreObject,
    DefaultBehavior,
};

struct VisiblePositionRange {

    VisiblePosition start;
    VisiblePosition end;

    VisiblePositionRange() {}

    VisiblePositionRange(const VisiblePosition& s, const VisiblePosition& e)
        : start(s)
        , end(e)
    { }

    bool isNull() const { return start.isNull() || end.isNull(); }
};

struct PlainTextRange {
        
    unsigned start;
    unsigned length;
    
    PlainTextRange()
        : start(0)
        , length(0)
    { }
    
    PlainTextRange(unsigned s, unsigned l)
        : start(s)
        , length(l)
    { }
    
    bool isNull() const { return !start && !length; }
};

class AccessibilityObject : public RefCounted<AccessibilityObject> {
protected:
    AccessibilityObject();
public:
    virtual ~AccessibilityObject();
    virtual void detach();
        
    typedef Vector<RefPtr<AccessibilityObject> > AccessibilityChildrenVector;
    
    virtual bool isAccessibilityRenderObject() const { return false; }
    virtual bool isAnchor() const { return false; }
    virtual bool isAttachment() const { return false; }
    virtual bool isHeading() const { return false; }
    virtual bool isLink() const { return false; }
    virtual bool isImage() const { return false; }
    virtual bool isNativeImage() const { return false; }
    virtual bool isImageButton() const { return false; }
    virtual bool isPasswordField() const { return false; }
    virtual bool isTextControl() const { return false; }
    virtual bool isNativeTextControl() const { return false; }
    virtual bool isWebArea() const { return false; }
    virtual bool isCheckboxOrRadio() const { return false; }
    virtual bool isListBox() const { return roleValue() == ListBoxRole; }
    virtual bool isMediaTimeline() const { return false; }
    virtual bool isMenuRelated() const { return false; }
    virtual bool isMenu() const { return false; }
    virtual bool isMenuBar() const { return false; }
    virtual bool isMenuButton() const { return false; }
    virtual bool isMenuItem() const { return false; }
    virtual bool isFileUploadButton() const { return false; }
    virtual bool isInputImage() const { return false; }
    virtual bool isProgressIndicator() const { return false; }
    virtual bool isSlider() const { return false; }
    virtual bool isControl() const { return false; }
    virtual bool isList() const { return false; }
    virtual bool isDataTable() const { return false; }
    virtual bool isTableRow() const { return false; }
    virtual bool isTableColumn() const { return false; }
    virtual bool isTableCell() const { return false; }
    virtual bool isFieldset() const { return false; }
    virtual bool isGroup() const { return false; }
    bool isTabList() const { return roleValue() == TabListRole; }
    bool isTabItem() const { return roleValue() == TabRole; }
    bool isRadioGroup() const { return roleValue() == RadioGroupRole; }
    
    virtual bool isChecked() const { return false; }
    virtual bool isEnabled() const { return false; }
    virtual bool isSelected() const { return false; }
    virtual bool isFocused() const { return false; }
    virtual bool isHovered() const { return false; }
    virtual bool isIndeterminate() const { return false; }
    virtual bool isLoaded() const { return false; }
    virtual bool isMultiSelect() const { return false; }
    virtual bool isOffScreen() const { return false; }
    virtual bool isPressed() const { return false; }
    virtual bool isReadOnly() const { return false; }
    virtual bool isVisited() const { return false; }
    virtual bool isRequired() const { return false; }
    virtual bool isLinked() const { return false; }

    virtual bool canSetFocusAttribute() const { return false; }
    virtual bool canSetTextRangeAttributes() const { return false; }
    virtual bool canSetValueAttribute() const { return false; }
    virtual bool canSetSelectedAttribute() const { return false; }
    virtual bool canSetSelectedChildrenAttribute() const { return false; }
    
    virtual bool hasIntValue() const { return false; }

    bool accessibilityShouldUseUniqueId() const { return true; }
    virtual bool accessibilityIsIgnored() const  { return true; }

    virtual int headingLevel() const { return 0; }
    virtual int intValue() const { return 0; }
    virtual String valueDescription() const { return String(); }
    virtual float valueForRange() const { return 0.0f; }
    virtual float maxValueForRange() const { return 0.0f; }
    virtual float minValueForRange() const { return 0.0f; }
    virtual AccessibilityObject* selectedRadioButton() { return 0; }
    virtual AccessibilityObject* selectedTabItem() { return 0; }    
    virtual int layoutCount() const { return 0; }
    static bool isARIAControl(AccessibilityRole);
    static bool isARIAInput(AccessibilityRole);
    
    virtual AccessibilityObject* doAccessibilityHitTest(const IntPoint&) const { return 0; }
    virtual AccessibilityObject* focusedUIElement() const { return 0; }

    virtual AccessibilityObject* firstChild() const { return 0; }
    virtual AccessibilityObject* lastChild() const { return 0; }
    virtual AccessibilityObject* previousSibling() const { return 0; }
    virtual AccessibilityObject* nextSibling() const { return 0; }
    virtual AccessibilityObject* parentObject() const = 0;
    virtual AccessibilityObject* parentObjectUnignored() const;
    virtual AccessibilityObject* parentObjectIfExists() const { return 0; }
    static AccessibilityObject* firstAccessibleObjectFromNode(const Node*);

    virtual AccessibilityObject* observableObject() const { return 0; }
    virtual void linkedUIElements(AccessibilityChildrenVector&) const { }
    virtual AccessibilityObject* titleUIElement() const { return 0; }
    virtual bool exposesTitleUIElement() const { return true; }
    virtual AccessibilityObject* correspondingControlForLabelElement() const { return 0; }

    virtual AccessibilityRole ariaRoleAttribute() const { return UnknownRole; }
    virtual bool isPresentationalChildOfAriaRole() const { return false; }
    virtual bool ariaRoleHasPresentationalChildren() const { return false; }

    void setRoleValue(AccessibilityRole role) { m_role = role; }
    virtual AccessibilityRole roleValue() const { return m_role; }
    virtual String ariaLabeledByAttribute() const { return String(); }
    virtual String ariaDescribedByAttribute() const { return String(); }
    virtual String accessibilityDescription() const { return String(); }
    virtual PassRefPtr<Range> ariaSelectedTextDOMRange() const { return 0; }

    virtual AXObjectCache* axObjectCache() const { return 0; }
    AXID axObjectID() const { return m_id; }
    void setAXObjectID(AXID axObjectID) { m_id = axObjectID; }
    
    static AccessibilityObject* anchorElementForNode(Node*);
    virtual Element* anchorElement() const { return 0; }
    virtual Element* actionElement() const { return 0; }
    virtual IntRect boundingBoxRect() const { return IntRect(); }
    virtual IntRect elementRect() const = 0;
    virtual IntSize size() const = 0;
    virtual IntPoint clickPoint() const;

    virtual PlainTextRange selectedTextRange() const { return PlainTextRange(); }
    unsigned selectionStart() const { return selectedTextRange().start; }
    unsigned selectionEnd() const { return selectedTextRange().length; }
    
    virtual KURL url() const { return KURL(); }
    virtual VisibleSelection selection() const { return VisibleSelection(); }
    virtual String stringValue() const { return String(); }
    virtual String title() const { return String(); }
    virtual String helpText() const { return String(); }
    virtual String textUnderElement() const { return String(); }
    virtual String text() const { return String(); }
    virtual int textLength() const { return 0; }
    virtual String selectedText() const { return String(); }
    virtual const AtomicString& accessKey() const { return nullAtom; }
    const String& actionVerb() const;
    virtual Widget* widget() const { return 0; }
    virtual Widget* widgetForAttachmentView() const { return 0; }
    virtual Document* document() const { return 0; }
    virtual FrameView* topDocumentFrameView() const { return 0; }
    virtual FrameView* documentFrameView() const;
    virtual String language() const;

    virtual void setFocused(bool) { }
    virtual void setSelectedText(const String&) { }
    virtual void setSelectedTextRange(const PlainTextRange&) { }
    virtual void setValue(const String&) { }
    virtual void setSelected(bool) { }

    virtual void makeRangeVisible(const PlainTextRange&) { }
    virtual bool press() const;
    bool performDefaultAction() const { return press(); }

    virtual AccessibilityOrientation orientation() const;
    virtual void increment() { };
    virtual void decrement() { };

    virtual void childrenChanged() { }
    virtual const AccessibilityChildrenVector& children() { return m_children; }
    virtual void addChildren() { }
    virtual bool canHaveChildren() const { return true; }
    virtual bool hasChildren() const { return m_haveChildren; }
    virtual void selectedChildren(AccessibilityChildrenVector&) { }
    virtual void visibleChildren(AccessibilityChildrenVector&) { }
    virtual void tabChildren(AccessibilityChildrenVector&) { }
    virtual bool shouldFocusActiveDescendant() const { return false; }
    virtual AccessibilityObject* activeDescendant() const { return 0; }    
    virtual void handleActiveDescendantChanged() { }

    virtual VisiblePositionRange visiblePositionRange() const { return VisiblePositionRange(); }
    virtual VisiblePositionRange visiblePositionRangeForLine(unsigned) const { return VisiblePositionRange(); }
    
    VisiblePositionRange visiblePositionRangeForUnorderedPositions(const VisiblePosition&, const VisiblePosition&) const;
    VisiblePositionRange positionOfLeftWord(const VisiblePosition&) const;
    VisiblePositionRange positionOfRightWord(const VisiblePosition&) const;
    VisiblePositionRange leftLineVisiblePositionRange(const VisiblePosition&) const;
    VisiblePositionRange rightLineVisiblePositionRange(const VisiblePosition&) const;
    VisiblePositionRange sentenceForPosition(const VisiblePosition&) const;
    VisiblePositionRange paragraphForPosition(const VisiblePosition&) const;
    VisiblePositionRange styleRangeForPosition(const VisiblePosition&) const;
    VisiblePositionRange visiblePositionRangeForRange(const PlainTextRange&) const;

    String stringForVisiblePositionRange(const VisiblePositionRange&) const;
    virtual IntRect boundsForVisiblePositionRange(const VisiblePositionRange&) const { return IntRect(); }
    int lengthForVisiblePositionRange(const VisiblePositionRange&) const;
    virtual void setSelectedVisiblePositionRange(const VisiblePositionRange&) const { }

    virtual VisiblePosition visiblePositionForPoint(const IntPoint&) const { return VisiblePosition(); }
    VisiblePosition nextVisiblePosition(const VisiblePosition& visiblePos) const { return visiblePos.next(); }
    VisiblePosition previousVisiblePosition(const VisiblePosition& visiblePos) const { return visiblePos.previous(); }
    VisiblePosition nextWordEnd(const VisiblePosition&) const;
    VisiblePosition previousWordStart(const VisiblePosition&) const;
    VisiblePosition nextLineEndPosition(const VisiblePosition&) const;
    VisiblePosition previousLineStartPosition(const VisiblePosition&) const;
    VisiblePosition nextSentenceEndPosition(const VisiblePosition&) const;
    VisiblePosition previousSentenceStartPosition(const VisiblePosition&) const;
    VisiblePosition nextParagraphEndPosition(const VisiblePosition&) const;
    VisiblePosition previousParagraphStartPosition(const VisiblePosition&) const;
    virtual VisiblePosition visiblePositionForIndex(unsigned, bool /*lastIndexOK */) const { return VisiblePosition(); }
    
    virtual VisiblePosition visiblePositionForIndex(int) const { return VisiblePosition(); }
    virtual int indexForVisiblePosition(const VisiblePosition&) const { return 0; }

    AccessibilityObject* accessibilityObjectForPosition(const VisiblePosition&) const;
    int lineForPosition(const VisiblePosition&) const;
    PlainTextRange plainTextRangeForVisiblePositionRange(const VisiblePositionRange&) const;
    virtual int index(const VisiblePosition&) const { return -1; }

    virtual PlainTextRange doAXRangeForLine(unsigned) const { return PlainTextRange(); }
    PlainTextRange doAXRangeForPosition(const IntPoint&) const;
    virtual PlainTextRange doAXRangeForIndex(unsigned) const { return PlainTextRange(); }
    PlainTextRange doAXStyleRangeForIndex(unsigned) const;

    virtual String doAXStringForRange(const PlainTextRange&) const { return String(); }
    virtual IntRect doAXBoundsForRange(const PlainTextRange&) const { return IntRect(); }
    String listMarkerTextForNodeAndPosition(Node*, const VisiblePosition&) const;

    unsigned doAXLineForIndex(unsigned);

    virtual String stringValueForMSAA() const { return String(); }
    virtual String stringRoleForMSAA() const { return String(); }
    virtual String nameForMSAA() const { return String(); }
    virtual String descriptionForMSAA() const { return String(); }

#if HAVE(ACCESSIBILITY)
#if PLATFORM(GTK)
    AccessibilityObjectWrapper* wrapper() const;
    void setWrapper(AccessibilityObjectWrapper*);
#else
    AccessibilityObjectWrapper* wrapper() const { return m_wrapper.get(); }
    void setWrapper(AccessibilityObjectWrapper* wrapper) 
    {
        m_wrapper = wrapper;
    }
#endif
#endif

    // a platform-specific method for determining if an attachment is ignored
#if HAVE(ACCESSIBILITY)
    bool accessibilityIgnoreAttachment() const;
#else
    bool accessibilityIgnoreAttachment() const { return true; }
#endif

    // gives platforms the opportunity to indicate if and how an object should be included
#if HAVE(ACCESSIBILITY)
    AccessibilityObjectPlatformInclusion accessibilityPlatformIncludesObject() const;
#else
    AccessibilityObjectPlatformInclusion accessibilityPlatformIncludesObject() const { return DefaultBehavior; }
#endif

    // allows for an AccessibilityObject to update its render tree or perform
    // other operations update type operations
    virtual void updateBackingStore() { }
    
protected:
    AXID m_id;
    AccessibilityChildrenVector m_children;
    mutable bool m_haveChildren;
    AccessibilityRole m_role;
    
    virtual void clearChildren();
    virtual bool isDetached() const { return true; }
    RenderListItem* renderListItemContainerForNode(Node* node) const;
    
#if PLATFORM(MAC)
    RetainPtr<AccessibilityObjectWrapper> m_wrapper;
#elif PLATFORM(WIN) && !PLATFORM(WINCE)
    COMPtr<AccessibilityObjectWrapper> m_wrapper;
#elif PLATFORM(GTK)
    AtkObject* m_wrapper;
#elif PLATFORM(CHROMIUM)
    RefPtr<AccessibilityObjectWrapper> m_wrapper;
#endif
};

} // namespace WebCore

#endif // AccessibilityObject_h
