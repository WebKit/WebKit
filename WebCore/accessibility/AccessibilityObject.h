/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "VisiblePosition.h"
#include <wtf/Platform.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#elif PLATFORM(WIN)
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
class VisibleSelection;
class String;
class Widget;

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
    AnnotationRole
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
    
    bool isNull() const { return start == 0 && length == 0; }
};

class AccessibilityObject : public RefCounted<AccessibilityObject> {
protected:
    AccessibilityObject();
public:
    virtual ~AccessibilityObject();
        
    typedef Vector<RefPtr<AccessibilityObject> > AccessibilityChildrenVector;
    
    virtual bool isAccessibilityRenderObject() const { return false; };
    virtual bool isAnchor() const { return false; };
    virtual bool isAttachment() const { return false; };
    virtual bool isHeading() const { return false; };
    virtual bool isLink() const { return false; };
    virtual bool isImage() const { return false; };
    virtual bool isNativeImage() const { return false; };
    virtual bool isImageButton() const { return false; };
    virtual bool isPasswordField() const { return false; };
    virtual bool isTextControl() const { return false; };
    virtual bool isNativeTextControl() const { return false; };
    virtual bool isWebArea() const { return false; };
    virtual bool isCheckboxOrRadio() const { return false; };
    virtual bool isListBox() const { return roleValue() == ListBoxRole; };
    virtual bool isMenuRelated() const { return false; }
    virtual bool isMenu() const { return false; }
    virtual bool isMenuBar() const { return false; }
    virtual bool isMenuButton() const { return false; }
    virtual bool isMenuItem() const { return false; }
    virtual bool isFileUploadButton() const { return false; };
    virtual bool isInputImage() const { return false; }
    virtual bool isProgressIndicator() const { return false; };
    virtual bool isSlider() const { return false; };
    virtual bool isControl() const { return false; };
    virtual bool isList() const { return false; };
    virtual bool isDataTable() const { return false; };
    virtual bool isTableRow() const { return false; };
    virtual bool isTableColumn() const { return false; };
    virtual bool isTableCell() const { return false; };
    virtual bool isFieldset() const { return false; };
    virtual bool isGroup() const { return false; };
    
    virtual bool isChecked() const { return false; };
    virtual bool isEnabled() const { return false; };
    virtual bool isSelected() const { return false; };
    virtual bool isFocused() const { return false; };
    virtual bool isHovered() const { return false; };
    virtual bool isIndeterminate() const { return false; };
    virtual bool isLoaded() const { return false; };
    virtual bool isMultiSelect() const { return false; };
    virtual bool isOffScreen() const { return false; };
    virtual bool isPressed() const { return false; };
    virtual bool isReadOnly() const { return false; };
    virtual bool isVisited() const { return false; };

    virtual bool canSetFocusAttribute() const { return false; };
    virtual bool canSetTextRangeAttributes() const { return false; };
    virtual bool canSetValueAttribute() const { return false; };
    virtual bool canSetSelectedAttribute() const { return false; }
    virtual bool canSetSelectedChildrenAttribute() const { return false; }
    
    virtual bool hasIntValue() const { return false; };

    bool accessibilityShouldUseUniqueId() const { return true; };
    virtual bool accessibilityIsIgnored() const  { return true; };

    virtual int intValue() const;
    virtual float valueForRange() const { return 0.0f; }
    virtual float maxValueForRange() const { return 0.0f; }
    virtual float minValueForRange() const { return 0.0f; }
    virtual int layoutCount() const;
    static bool isARIAControl(AccessibilityRole);
    static bool isARIAInput(AccessibilityRole);
    unsigned axObjectID() const;
    
    virtual AccessibilityObject* doAccessibilityHitTest(const IntPoint&) const;
    virtual AccessibilityObject* focusedUIElement() const;
    virtual AccessibilityObject* firstChild() const;
    virtual AccessibilityObject* lastChild() const;
    virtual AccessibilityObject* previousSibling() const;
    virtual AccessibilityObject* nextSibling() const;
    virtual AccessibilityObject* parentObject() const;
    virtual AccessibilityObject* parentObjectUnignored() const;
    virtual AccessibilityObject* parentObjectIfExists() const;
    virtual AccessibilityObject* observableObject() const;
    virtual void linkedUIElements(AccessibilityChildrenVector&) const;
    virtual AccessibilityObject* titleUIElement() const;
    virtual bool exposesTitleUIElement() const { return true; }
    virtual AccessibilityRole ariaRoleAttribute() const;
    virtual bool isPresentationalChildOfAriaRole() const;
    virtual bool ariaRoleHasPresentationalChildren() const;

    virtual AccessibilityRole roleValue() const;
    virtual AXObjectCache* axObjectCache() const;
    
    virtual Element* anchorElement() const;
    virtual Element* actionElement() const;
    virtual IntRect boundingBoxRect() const;
    virtual IntRect elementRect() const;
    virtual IntSize size() const;
    virtual IntPoint clickPoint() const;
    
    virtual KURL url() const;
    virtual PlainTextRange selectedTextRange() const;
    virtual VisibleSelection selection() const;
    unsigned selectionStart() const;
    unsigned selectionEnd() const;
    virtual String stringValue() const;
    virtual String ariaAccessiblityName(const String&) const;
    virtual String ariaLabeledByAttribute() const;
    virtual String title() const;
    virtual String ariaDescribedByAttribute() const;
    virtual String accessibilityDescription() const;
    virtual String helpText() const;
    virtual String textUnderElement() const;
    virtual String text() const;
    virtual int textLength() const;
    virtual PassRefPtr<Range> ariaSelectedTextDOMRange() const;
    virtual String selectedText() const;
    virtual const AtomicString& accessKey() const;
    const String& actionVerb() const;
    virtual Widget* widget() const;
    virtual Widget* widgetForAttachmentView() const;
    virtual Document* document() const { return 0; }
    virtual FrameView* topDocumentFrameView() const { return 0; }
    virtual FrameView* documentFrameView() const;
    virtual String language() const;
    
    void setAXObjectID(unsigned);
    virtual void setFocused(bool);
    virtual void setSelectedText(const String&);
    virtual void setSelectedTextRange(const PlainTextRange&);
    virtual void setValue(const String&);
    virtual void setSelected(bool);
    
    virtual void detach();
    virtual void makeRangeVisible(const PlainTextRange&);
    virtual bool press() const;
    bool performDefaultAction() const { return press(); }
    
    virtual void childrenChanged();
    virtual const AccessibilityChildrenVector& children() { return m_children; }
    virtual void addChildren();
    virtual bool canHaveChildren() const { return true; }
    virtual bool hasChildren() const { return m_haveChildren; };
    virtual void selectedChildren(AccessibilityChildrenVector&);
    virtual void visibleChildren(AccessibilityChildrenVector&);
    virtual bool shouldFocusActiveDescendant() const { return false; }
    virtual AccessibilityObject* activeDescendant() const { return 0; }    
    virtual void handleActiveDescendantChanged() { }

    virtual VisiblePositionRange visiblePositionRange() const;
    virtual VisiblePositionRange visiblePositionRangeForLine(unsigned) const;
    
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
    virtual IntRect boundsForVisiblePositionRange(const VisiblePositionRange&) const;    
    int lengthForVisiblePositionRange(const VisiblePositionRange&) const;
    virtual void setSelectedVisiblePositionRange(const VisiblePositionRange&) const;

    virtual VisiblePosition visiblePositionForPoint(const IntPoint&) const;
    VisiblePosition nextVisiblePosition(const VisiblePosition&) const;
    VisiblePosition previousVisiblePosition(const VisiblePosition&) const;
    VisiblePosition nextWordEnd(const VisiblePosition&) const;
    VisiblePosition previousWordStart(const VisiblePosition&) const;
    VisiblePosition nextLineEndPosition(const VisiblePosition&) const;
    VisiblePosition previousLineStartPosition(const VisiblePosition&) const;
    VisiblePosition nextSentenceEndPosition(const VisiblePosition&) const;
    VisiblePosition previousSentenceStartPosition(const VisiblePosition&) const;
    VisiblePosition nextParagraphEndPosition(const VisiblePosition&) const;
    VisiblePosition previousParagraphStartPosition(const VisiblePosition&) const;
    virtual VisiblePosition visiblePositionForIndex(unsigned indexValue, bool lastIndexOK) const;
    
    virtual VisiblePosition visiblePositionForIndex(int) const;
    virtual int indexForVisiblePosition(const VisiblePosition&) const;

    AccessibilityObject* accessibilityObjectForPosition(const VisiblePosition&) const;
    int lineForPosition(const VisiblePosition&) const;
    PlainTextRange plainTextRangeForVisiblePositionRange(const VisiblePositionRange&) const;
    virtual int index(const VisiblePosition&) const;

    virtual PlainTextRange doAXRangeForLine(unsigned) const;
    PlainTextRange doAXRangeForPosition(const IntPoint&) const;
    virtual PlainTextRange doAXRangeForIndex(unsigned) const;
    PlainTextRange doAXStyleRangeForIndex(unsigned) const;

    virtual String doAXStringForRange(const PlainTextRange&) const;
    virtual IntRect doAXBoundsForRange(const PlainTextRange&) const;

    unsigned doAXLineForIndex(unsigned);

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

    // allows for an AccessibilityObject to update its render tree or perform
    // other operations update type operations
    virtual void updateBackingStore();
    
protected:
    unsigned m_id;
    AccessibilityChildrenVector m_children;
    mutable bool m_haveChildren;
    
    virtual void clearChildren();
    virtual bool isDetached() const { return true; }

#if PLATFORM(MAC)
    RetainPtr<AccessibilityObjectWrapper> m_wrapper;
#elif PLATFORM(WIN)
    COMPtr<AccessibilityObjectWrapper> m_wrapper;
#elif PLATFORM(GTK)
    AtkObject* m_wrapper;
#elif PLATFORM(CHROMIUM)
    RefPtr<AccessibilityObjectWrapper> m_wrapper;
#endif
};

} // namespace WebCore

#endif // AccessibilityObject_h
