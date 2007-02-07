// -*- c-basic-offset: 4 -*-
 /* This file is part of the KDE project
 *
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999-2001 Lars Knoll <knoll@kde.org>
 *                     1999-2001 Antti Koivisto <koivisto@kde.org>
 *                     2000-2001 Simon Hausmann <hausmann@kde.org>
 *                     2000-2001 Dirk Mueller <mueller@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef Frame_h
#define Frame_h

#include "Color.h"
#include "EditAction.h"
#include "RenderLayer.h"
#include "TextGranularity.h"
#include <wtf/unicode/Unicode.h>
#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace KJS {
    namespace Bindings {
        class Instance;
        class RootObject;
    }
}

namespace WebCore {

class CSSComputedStyleDeclaration;
class CSSMutableStyleDeclaration;
class CSSStyleDeclaration;
class CommandByName;
class DOMWindow;
class Document;
class Editor;
class Element;
class EventHandler;
class FloatRect;
class FrameLoader;
class FrameLoaderClient;
class HTMLFrameOwnerElement;
class FramePrivate;
class FrameTree;
class FrameView;
class GraphicsContext;
class HTMLFormElement;
class IntRect;
class KJSProxy;
class KURL;
class Node;
class Page;
class Range;
class RenderObject;
class RenderPart;
class RenderStyle;
class Selection;
class SelectionController;
class Settings;
class Widget;

struct FrameLoadRequest;

template <typename T> class Timer;

struct MarkedTextUnderline {
    MarkedTextUnderline() 
        : startOffset(0), endOffset(0), thick(false) { }
    MarkedTextUnderline(unsigned s, unsigned e, const Color& c, bool t) 
        : startOffset(s), endOffset(e), color(c), thick(t) { }
    unsigned startOffset;
    unsigned endOffset;
    Color color;
    bool thick;
};

class Frame : public Shared<Frame> {
public:
    Frame(Page*, HTMLFrameOwnerElement*, FrameLoaderClient*);
    virtual void setView(FrameView*);
    virtual ~Frame();
    
    Page* page() const;
    HTMLFrameOwnerElement* ownerElement() const;

    void pageDestroyed();
    void disconnectOwnerElement();

    Document* document() const;
    FrameView* view() const;

    CommandByName* command() const;
    DOMWindow* domWindow() const;
    Editor* editor() const;
    EventHandler* eventHandler() const;
    FrameLoader* loader() const;
    SelectionController* selectionController() const;
    FrameTree* tree() const;

    RenderObject* renderer() const; // root renderer for the document contained in this frame
    RenderPart* ownerRenderer(); // renderer for the element that contains this frame

    friend class FrameGdk;
    friend class FrameLoader;
    friend class FrameMac;
    friend class FramePrivate;
    friend class FrameQt;
    friend class FrameWin;

private:
    FramePrivate* d;

// === undecided, may or may not belong here

public:
    static Frame* frameForWidget(const Widget*);

    void setSettings(Settings*);
    const Settings* settings() const;
    void reparseConfiguration();

    void paint(GraphicsContext*, const IntRect&);

    void setUserStyleSheetLocation(const KURL&);
    void setUserStyleSheet(const String& styleSheetData);

    void setZoomFactor(int percent);
    int zoomFactor() const;

    bool inViewSourceMode() const;
    void setInViewSourceMode(bool = true) const;

    void setJSStatusBarText(const String&);
    void setJSDefaultStatusBarText(const String&);
    String jsStatusBarText() const;
    String jsDefaultStatusBarText() const;

    virtual void setupRootForPrinting(bool onOrOff) { }
    virtual Vector<IntRect> computePageRects(const IntRect& printRect, float userScaleFactor) { return Vector<IntRect>(); }

    void keepAlive(); // Used to keep the frame alive when running a script that might destroy it.
#ifndef NDEBUG
    static void cancelAllKeepAlive();
#endif

    virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*) = 0;
    virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*) = 0;
    virtual KJS::Bindings::Instance* getAppletInstanceForWidget(Widget*) = 0;
    virtual KJS::Bindings::RootObject* bindingRootObject() = 0;

    void setDocument(Document*);

    KJSProxy* scriptProxy();

    bool isFrameSet() const;

    void adjustPageHeight(float* newBottom, float oldTop, float oldBottom, float bottomLimit);

    void forceLayout();
    void forceLayoutWithPageWidthRange(float minPageWidth, float maxPageWidth);

    void sendResizeEvent();
    void sendScrollEvent();

    void clearTimers();
    static void clearTimers(FrameView*);

    bool isActive() const;
    virtual void setIsActive(bool flag);
    void setWindowHasFocus(bool flag);

    // Convenience, to avoid repeating the code to dig down to get this.
    UChar backslashAsCurrencySymbol() const;

    void setNeedsReapplyStyles();
    String documentTypeString() const;

    bool prohibitsScrolling() const;
    void setProhibitsScrolling(const bool);

protected:
    virtual void cleanupPluginObjects() { }

private:
    void lifeSupportTimerFired(Timer<Frame>*);

// === to be moved into Chrome

public:
    virtual bool shouldInterruptJavaScript() = 0;
    virtual void focusWindow() = 0;
    virtual void unfocusWindow() = 0;
    virtual void print() = 0;
    bool shouldClose();
    void scheduleClose();

// === to be moved into Editor

public:
    virtual String selectedText() const;  
    bool findString(const String&, bool forward, bool caseFlag, bool wrapFlag, bool startInSelection);

    const Selection& mark() const; // Mark, to be used as emacs uses it.
    void setMark(const Selection&);

    void transpose();

    void copyToPasteboard();
    void cutToPasteboard();
    void pasteFromPasteboard();
    void pasteAndMatchStyle();
    void computeAndSetTypingStyle(CSSStyleDeclaration* , EditAction = EditActionUnspecified);
    enum TriState { falseTriState, trueTriState, mixedTriState };
    TriState selectionHasStyle(CSSStyleDeclaration*) const;
    String selectionStartStylePropertyValue(int stylePropertyID) const;
    void applyEditingStyleToBodyElement() const;
    void removeEditingStyleFromBodyElement() const;
    void applyEditingStyleToElement(Element*) const;
    void removeEditingStyleFromElement(Element*) const;

    virtual Range* markedTextRange() const = 0;
    virtual void issueCutCommand() = 0;
    virtual void issueCopyCommand() = 0;
    virtual void issuePasteCommand() = 0;
    virtual void issuePasteAndMatchStyleCommand() = 0;
    virtual void issueTransposeCommand() = 0;
    virtual void respondToChangedSelection(const Selection& oldSelection, bool closeTyping) = 0;
    virtual bool shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity, bool stillSelecting) const = 0;

    RenderStyle* styleForSelectionStart(Node*& nodeToRemove) const;

    const Vector<MarkedTextUnderline>& markedTextUnderlines() const;  
    bool markedTextUsesUnderlines() const;
  
    unsigned markAllMatchesForText(const String&, bool caseFlag, unsigned limit);
    bool markedTextMatchesAreHighlighted() const;
    void setMarkedTextMatchesAreHighlighted(bool flag);

    CSSComputedStyleDeclaration* selectionComputedStyle(Node*& nodeToRemove) const;

    virtual void textFieldDidBeginEditing(Element*);
    virtual void textFieldDidEndEditing(Element*);
    virtual void textDidChangeInTextField(Element*);
    virtual bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*);
    virtual void textWillBeDeletedInTextField(Element* input);
    virtual void textDidChangeInTextArea(Element*);

// === to be moved into SelectionController

public:
    TextGranularity selectionGranularity() const;
    void setSelectionGranularity(TextGranularity) const;

    bool shouldChangeSelection(const Selection&) const;
    virtual bool shouldDeleteSelection(const Selection&) const;
    void clearCaretRectIfNeeded();
    void setFocusedNodeIfNeeded();
    void selectionLayoutChanged();
    void notifyRendererOfSelectionChange(bool userTriggered);

    void invalidateSelection();

    void setCaretVisible(bool = true);
    void paintCaret(GraphicsContext*, const IntRect&) const;  
    void paintDragCaret(GraphicsContext*, const IntRect&) const;

    void setXPosForVerticalArrowNavigation(int);
    int xPosForVerticalArrowNavigation() const;

    virtual bool isContentEditable() const; // if true, everything in frame is editable

    virtual void setSecureKeyboardEntry(bool) { }
    virtual bool isSecureKeyboardEntry() { return false; }

    CSSMutableStyleDeclaration* typingStyle() const;
    void setTypingStyle(CSSMutableStyleDeclaration*);
    void clearTypingStyle();

    IntRect selectionRect() const;
    FloatRect visibleSelectionRect() const;

    HTMLFormElement* currentForm() const;

    void revealSelection(const RenderLayer::ScrollAlignment& = RenderLayer::gAlignCenterIfNeeded) const;
    void revealCaret(const RenderLayer::ScrollAlignment& = RenderLayer::gAlignCenterIfNeeded) const;
    void setSelectionFromNone();

private:
    void caretBlinkTimerFired(Timer<Frame>*);

// === to be moved into the Platform directory

public:
    virtual String mimeTypeForFileName(const String&) const = 0;
    virtual bool isCharacterSmartReplaceExempt(UChar, bool);

// === to be deleted

public:
    SelectionController* dragCaretController() const;

};

} // namespace WebCore

#endif // Frame_h
