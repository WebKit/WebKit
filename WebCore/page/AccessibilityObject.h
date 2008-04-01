/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
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
@class WebCoreTextMarker;
@class WebCoreTextMarkerRange;
#else
class AccessibilityObjectWrapper;
class NSArray;
class NSAttributedString;
class NSData;
class NSMutableAttributedString;
class NSString;
class NSValue;
class NSView;
class WebCoreTextMarker;
class WebCoreTextMarkerRange;
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
class RenderTextControl;
class Selection;
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
    SortButtonRole,
    LinkRole,
    DisclosureTriangleRole,
    GridRole,
    // WebCore-specific roles
    WebCoreLinkRole,
    ImageMapRole,
    ListMarkerRole,
    WebAreaRole,
    HeadingRole
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

class AccessibilityObject : public RefCounted<AccessibilityObject> {
private:
    AccessibilityObject(RenderObject*);
public:
    static PassRefPtr<AccessibilityObject> create(RenderObject*);
    ~AccessibilityObject();
    
    bool detached() const { return !m_renderer; };

    void detach();
    
    AccessibilityObject* firstChild();
    AccessibilityObject* lastChild();
    AccessibilityObject* previousSibling();
    AccessibilityObject* nextSibling();
    AccessibilityObject* parentObject();
    AccessibilityObject* parentObjectUnignored();
    AccessibilityObject* observableObject();
    
    void childrenChanged();

    unsigned axObjectID();
    void setAXObjectID(unsigned);
    void removeAXObjectID();

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

    bool isWebArea() const;
    bool isImageButton() const;
    bool isAnchor() const;
    bool isTextControl() const;
    bool isImage() const;
    bool isAttachment() const;
    bool isPasswordField() const;
    bool isHeading() const;
    int headingLevel(Node*) const;

    HTMLAnchorElement* anchorElement();
    Element* actionElement();    
    Element* mouseButtonListener();
    
    String helpText();
    String textUnderElement();
    String stringForReplacedNode(Node*);
    bool hasIntValue();
    int intValue();
    String stringValue();
    String title();
    String accessibilityDescription();
    IntRect boundingBoxRect();
    IntSize size();
    AccessibilityObject* linkedUIElement();
    bool accessibilityShouldUseUniqueId();
    bool accessibilityIsIgnored();
    bool loaded();
    int layoutCount();
    int textLength();
    String selectedText();
    Selection selection();
    PlainTextRange selectedTextRange();
    void setSelectedText(String);
    void setSelectedTextRange(PlainTextRange);
    void makeRangeVisible(PlainTextRange range);
    KURL url();
    bool isVisited();
    bool isFocused();
    void setFocused(bool);
    void setValue(String);
    bool isEnabled();
    void press();
    RenderObject* renderer() { return m_renderer; }
    RenderObject* topRenderer();
    RenderTextControl* textControl();
    Widget* widget();
    AXObjectCache* axObjectCache();
    void documentLinks(Vector<RefPtr<AccessibilityObject> >&);
    FrameView* frameViewIfRenderView();
    FrameView* documentFrameView();

    VisiblePositionRange visiblePositionRange();
    VisiblePositionRange doAXTextMarkerRangeForLine(unsigned);
    VisiblePositionRange doAXTextMarkerRangeForUnorderedTextMarkers(const VisiblePosition&, const VisiblePosition&);
    VisiblePositionRange doAXLeftWordTextMarkerRangeForTextMarker(const VisiblePosition&);
    VisiblePositionRange doAXRightWordTextMarkerRangeForTextMarker(const VisiblePosition&);
    VisiblePositionRange doAXLeftLineTextMarkerRangeForTextMarker(const VisiblePosition&);
    VisiblePositionRange doAXRightLineTextMarkerRangeForTextMarker(const VisiblePosition&);
    VisiblePositionRange doAXSentenceTextMarkerRangeForTextMarker(const VisiblePosition&);
    VisiblePositionRange doAXParagraphTextMarkerRangeForTextMarker(const VisiblePosition&);
    VisiblePositionRange doAXStyleTextMarkerRangeForTextMarker(const VisiblePosition&);
    VisiblePositionRange textMarkerRangeForRange(const PlainTextRange&, RenderTextControl*);

    IntRect convertViewRectToScreenCoords(const IntRect&);
    IntPoint convertAbsolutePointToViewCoords(const IntPoint&, const FrameView*);

    String doAXStringForTextMarkerRange(VisiblePositionRange&);
    IntRect doAXBoundsForTextMarkerRange(VisiblePositionRange);
    int doAXLengthForTextMarkerRange(VisiblePositionRange);
    void doSetAXSelectedTextMarkerRange(VisiblePositionRange);
    PlainTextRange rangeForTextMarkerRange(const VisiblePositionRange&);

    VisiblePosition doAXTextMarkerForPosition(const IntPoint&);
    VisiblePosition doAXNextTextMarkerForTextMarker(const VisiblePosition&);
    VisiblePosition doAXPreviousTextMarkerForTextMarker(const VisiblePosition&);
    VisiblePosition doAXNextWordEndTextMarkerForTextMarker(const VisiblePosition&);
    VisiblePosition doAXPreviousWordStartTextMarkerForTextMarker(const VisiblePosition&);
    VisiblePosition doAXNextLineEndTextMarkerForTextMarker(const VisiblePosition&);
    VisiblePosition doAXPreviousLineStartTextMarkerForTextMarker(const VisiblePosition&);
    VisiblePosition doAXNextSentenceEndTextMarkerForTextMarker(const VisiblePosition&);
    VisiblePosition doAXPreviousSentenceStartTextMarkerForTextMarker(const VisiblePosition&);
    VisiblePosition doAXNextParagraphEndTextMarkerForTextMarker(const VisiblePosition&);
    VisiblePosition doAXPreviousParagraphStartTextMarkerForTextMarker(const VisiblePosition&);
    VisiblePosition textMarkerForIndex(unsigned indexValue, bool lastIndexOK);

    AccessibilityObject* doAXUIElementForTextMarker(const VisiblePosition&);
    unsigned doAXLineForTextMarker(const VisiblePosition&);
    int indexForTextMarker(VisiblePosition);

    PlainTextRange doAXRangeForLine(unsigned);
    PlainTextRange doAXRangeForPosition(const IntPoint&);
    PlainTextRange doAXRangeForIndex(unsigned);
    PlainTextRange doAXStyleRangeForIndex(unsigned);

    String doAXStringForRange(const PlainTextRange&);
    IntRect doAXBoundsForRange(PlainTextRange);

    unsigned doAXLineForIndex(unsigned);
    AccessibilityObject* doAccessibilityHitTest(IntPoint);
    AccessibilityObject* focusedUIElement();
    AccessibilityRole roleValue();

    bool canSetFocusAttribute();
    bool canSetValueAttribute();
    bool canSetTextRangeAttributes();

    void clearChildren();
    void addChildren();
    bool hasChildren();
    const Vector<RefPtr<AccessibilityObject> >& children() const { return m_children; }

#if PLATFORM(MAC)
    AccessibilityObjectWrapper* wrapper() { return m_wrapper.get(); }
    void setWrapper(AccessibilityObjectWrapper* wrapper) { m_wrapper = wrapper; }

    NSView* attachmentView();
    void performPressActionForAttachment();
    NSArray* accessibilityAttributeNames();
    WebCoreTextMarkerRange* textMarkerRange(WebCoreTextMarker* fromMarker, WebCoreTextMarker* toMarker);
    WebCoreTextMarker* textMarkerForVisiblePosition(VisiblePosition);
    WebCoreTextMarker* startTextMarker();
    VisiblePositionRange visiblePositionRangeForTextMarkerRange(WebCoreTextMarkerRange*);
    VisiblePosition visiblePositionForTextMarker(WebCoreTextMarker*);
    VisiblePosition visiblePositionForStartOfTextMarkerRange(WebCoreTextMarkerRange*);
    VisiblePosition visiblePositionForEndOfTextMarkerRange(WebCoreTextMarkerRange*);
    WebCoreTextMarkerRange* textMarkerRangeFromVisiblePositions(VisiblePosition startPosition, VisiblePosition endPosition);
    WebCoreTextMarkerRange* textMarkerRangeForSelection();
    WebCoreTextMarkerRange* textMarkerRange();
    WebCoreTextMarkerRange* textMarkerRangeFromMarkers(WebCoreTextMarker*, WebCoreTextMarker*);
    NSAttributedString* doAXAttributedStringForTextMarkerRange(WebCoreTextMarkerRange*);
    NSAttributedString* doAXAttributedStringForRange(NSRange);
    NSData* doAXRTFForRange(NSRange);

    NSArray* convertChildrenToNSArray();
    NSArray* convertWidgetChildrenToNSArray();
   
    NSValue* position(); 
    NSString* role();
    NSString* subrole();
    NSString* roleDescription();
    
    void AXAttributeStringSetElement(NSMutableAttributedString*, NSString*, AccessibilityObject*, NSRange);
    void AXAttributeStringSetHeadingLevel(NSMutableAttributedString*, RenderObject*, NSRange);
    void AXAttributedStringAppendText(NSMutableAttributedString*, Node*, int offset, const UChar* chars, int length);
    AccessibilityObject* AXLinkElementForNode(Node*);
#endif
    
private:
    RenderObject* m_renderer;
    RefPtr<HTMLAreaElement> m_areaElement;
    Vector<RefPtr<AccessibilityObject> >m_children;
    unsigned m_id; 

#if PLATFORM(MAC)
    RetainPtr<AccessibilityObjectWrapper> m_wrapper;
#endif
};

bool isPasswordFieldElement(Node*);

} // namespace WebCore

#endif // AccessibilityObject_h
