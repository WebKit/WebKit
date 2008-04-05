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

    bool isAnchor() const;
    bool isAttachment() const;
    bool isEnabled() const;
    bool isFocused() const;
    bool isHeading() const;
    bool isImageButton() const;
    bool isImage() const;
    bool isLoaded() const;
    bool isPasswordField() const;
    bool isTextControl() const;
    bool isVisited() const;
    bool isWebArea() const;

    bool canSetFocusAttribute() const;
    bool canSetTextRangeAttributes() const;
    bool canSetValueAttribute() const;

    bool hasIntValue() const;
    bool hasChildren() const;

    bool accessibilityShouldUseUniqueId() const;
    bool accessibilityIsIgnored() const;

    static int headingLevel(Node*);
    int intValue() const;
    int layoutCount() const;
    int textLength() const;
    unsigned axObjectID() const;
    AccessibilityObject* doAccessibilityHitTest(const IntPoint&) const;
    AccessibilityObject* focusedUIElement() const;
    AccessibilityObject* firstChild() const;
    AccessibilityObject* lastChild() const;
    AccessibilityObject* previousSibling() const;
    AccessibilityObject* nextSibling() const;
    AccessibilityObject* parentObject() const;
    AccessibilityObject* parentObjectUnignored() const;
    AccessibilityObject* observableObject() const;
    AccessibilityObject* linkedUIElement() const;
    AccessibilityRole roleValue() const;
    AXObjectCache* axObjectCache() const;
    Element* actionElement() const;
    Element* mouseButtonListener() const;
    FrameView* frameViewIfRenderView() const;
    FrameView* documentFrameView() const;
    HTMLAnchorElement* anchorElement() const;
    HTMLAreaElement* areaElement() const { return m_areaElement.get(); }
    IntRect boundingBoxRect() const;
    IntSize size() const;
    KURL url() const;
    PlainTextRange selectedTextRange() const;
    RenderObject* renderer() const { return m_renderer; }
    RenderObject* topRenderer() const;
    RenderTextControl* textControl() const;
    Selection selection() const;
    String stringValue() const;
    String title() const;
    String accessibilityDescription() const;
    String helpText() const;
    String textUnderElement() const;
    String selectedText() const;
    Widget* widget() const;
    Widget* widgetForAttachmentView() const;
    void getDocumentLinks(Vector< RefPtr<AccessibilityObject> >&) const;
    const Vector<RefPtr<AccessibilityObject> >& children() const { return m_children; }

    void setAXObjectID(unsigned);
    void setFocused(bool);
    void setSelectedText(const String&);
    void setSelectedTextRange(const PlainTextRange&);
    void setValue(const String&);

    void detach();
    void makeRangeVisible(const PlainTextRange&);
    void press();
    void childrenChanged();
    void addChildren();

    VisiblePositionRange visiblePositionRange() const;
    VisiblePositionRange doAXTextMarkerRangeForLine(unsigned) const;
    VisiblePositionRange doAXTextMarkerRangeForUnorderedTextMarkers(const VisiblePosition&, const VisiblePosition&) const;
    VisiblePositionRange doAXLeftWordTextMarkerRangeForTextMarker(const VisiblePosition&) const;
    VisiblePositionRange doAXRightWordTextMarkerRangeForTextMarker(const VisiblePosition&) const;
    VisiblePositionRange doAXLeftLineTextMarkerRangeForTextMarker(const VisiblePosition&) const;
    VisiblePositionRange doAXRightLineTextMarkerRangeForTextMarker(const VisiblePosition&) const;
    VisiblePositionRange doAXSentenceTextMarkerRangeForTextMarker(const VisiblePosition&) const;
    VisiblePositionRange doAXParagraphTextMarkerRangeForTextMarker(const VisiblePosition&) const;
    VisiblePositionRange doAXStyleTextMarkerRangeForTextMarker(const VisiblePosition&) const;
    VisiblePositionRange textMarkerRangeForRange(const PlainTextRange&, RenderTextControl*) const;

    String doAXStringForTextMarkerRange(const VisiblePositionRange&) const;
    IntRect doAXBoundsForTextMarkerRange(const VisiblePositionRange&) const;
    int doAXLengthForTextMarkerRange(const VisiblePositionRange&) const;
    void doSetAXSelectedTextMarkerRange(const VisiblePositionRange&) const;
    PlainTextRange rangeForTextMarkerRange(const VisiblePositionRange&) const;

    VisiblePosition doAXTextMarkerForPosition(const IntPoint&) const;
    VisiblePosition doAXNextTextMarkerForTextMarker(const VisiblePosition&) const;
    VisiblePosition doAXPreviousTextMarkerForTextMarker(const VisiblePosition&) const;
    VisiblePosition doAXNextWordEndTextMarkerForTextMarker(const VisiblePosition&) const;
    VisiblePosition doAXPreviousWordStartTextMarkerForTextMarker(const VisiblePosition&) const;
    VisiblePosition doAXNextLineEndTextMarkerForTextMarker(const VisiblePosition&) const;
    VisiblePosition doAXPreviousLineStartTextMarkerForTextMarker(const VisiblePosition&) const;
    VisiblePosition doAXNextSentenceEndTextMarkerForTextMarker(const VisiblePosition&) const;
    VisiblePosition doAXPreviousSentenceStartTextMarkerForTextMarker(const VisiblePosition&) const;
    VisiblePosition doAXNextParagraphEndTextMarkerForTextMarker(const VisiblePosition&) const;
    VisiblePosition doAXPreviousParagraphStartTextMarkerForTextMarker(const VisiblePosition&) const;
    VisiblePosition textMarkerForIndex(unsigned indexValue, bool lastIndexOK) const;

    AccessibilityObject* doAXUIElementForTextMarker(const VisiblePosition&) const;
    unsigned doAXLineForTextMarker(const VisiblePosition&) const;
    int indexForTextMarker(const VisiblePosition&) const;

    PlainTextRange doAXRangeForLine(unsigned) const;
    PlainTextRange doAXRangeForPosition(const IntPoint&) const;
    PlainTextRange doAXRangeForIndex(unsigned) const;
    PlainTextRange doAXStyleRangeForIndex(unsigned) const;

    String doAXStringForRange(const PlainTextRange&) const;
    IntRect doAXBoundsForRange(const PlainTextRange&) const;

    unsigned doAXLineForIndex(unsigned);

#if PLATFORM(MAC)
    AccessibilityObjectWrapper* wrapper() const { return m_wrapper.get(); }
    void setWrapper(AccessibilityObjectWrapper* wrapper) { m_wrapper = wrapper; }
#endif

private:
    RenderObject* m_renderer;
    RefPtr<HTMLAreaElement> m_areaElement;
    Vector<RefPtr<AccessibilityObject> > m_children;
    unsigned m_id;

    void clearChildren();
    void removeAXObjectID();

    bool isDetached() const { return !m_renderer; }

#if PLATFORM(MAC)
    RetainPtr<AccessibilityObjectWrapper> m_wrapper;
#endif
};

} // namespace WebCore

#endif // AccessibilityObject_h
