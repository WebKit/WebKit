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

#ifndef Frame_H
#define Frame_H

#include "Color.h"
#include "EditAction.h"
#include "RenderLayer.h"
#include "TextAffinity.h"
#include "TextGranularity.h"
#include "UChar.h"
#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace KJS {
    class JSValue;
    class PausedTimeouts;
    class SavedBuiltins;
    class SavedProperties;

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
class DrawContentsEvent;
class EditCommand;
class Editor;
class EditorClient;
class Element;
class FloatRect;
class FormData;
class FrameLoader;
class FramePrivate;
class FrameTree;
class FrameView;
class GraphicsContext;
class HTMLFormElement;
class HitTestResult;
class IntRect;
class KJSProxy;
class KURL;
class MouseEventWithHitTestResults;
class Node;
class Page;
class PlatformKeyboardEvent;
class Plugin;
class Range;
class RenderLayer;
class RenderObject;
class RenderPart;
class RenderStyle;
class RenderWidget;
class ResourceRequest;
class Selection;
class SelectionController;
class Settings;
class VisiblePosition;
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
    Frame(Page*, Element*, PassRefPtr<EditorClient>);
    virtual void setView(FrameView*);
    virtual ~Frame();

    Page* page() const;
    Element* ownerElement();

    void pageDestroyed();
    void disconnectOwnerElement();

    Document* document() const;
    FrameView* view() const;

    CommandByName* command() const;
    DOMWindow* domWindow() const;
    Editor* editor() const;
    FrameLoader* loader() const;
    SelectionController* selectionController() const;
    const Settings* settings() const;
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
    bool javaScriptEnabled() const;
    bool javaEnabled() const;
    bool pluginsEnabled() const;

    void setSettings(Settings*);
    void reparseConfiguration();

    void paint(GraphicsContext*, const IntRect&);

    void setUserStyleSheetLocation(const KURL&);
    void setUserStyleSheet(const String& styleSheetData);

    void setStandardFont(const String& name);
    void setFixedFont(const String& name);

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
    static void endAllLifeSupport();

    virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*) = 0;
    virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*) = 0;
    virtual KJS::Bindings::Instance* getAppletInstanceForWidget(Widget*) = 0;
    virtual KJS::Bindings::RootObject* bindingRootObject() = 0;

    void setDocument(Document*);

    KJSProxy* scriptProxy();

    bool isFrameSet() const;

    bool scrollOverflow(ScrollDirection, ScrollGranularity);

    void adjustPageHeight(float* newBottom, float oldTop, float oldBottom, float bottomLimit);

    static void clearDocumentFocus(Widget*);

    void forceLayout();
    void forceLayoutWithPageWidthRange(float minPageWidth, float maxPageWidth);

    void sendResizeEvent();
    void sendScrollEvent();
    bool canMouseDownStartSelect(Node*);

    void clearTimers();
    static void clearTimers(FrameView*);

    bool isActive() const;
    virtual void setIsActive(bool flag);
    void setWindowHasFocus(bool flag);

    // Convenience, to avoid repeating the code to dig down to get this.
    UChar backslashAsCurrencySymbol() const;

    void setNeedsReapplyStyles();
    String documentTypeString() const;

protected:
    virtual void cleanupPluginObjects() { }

private:
    void lifeSupportTimerFired(Timer<Frame>*);
    void endLifeSupport();

// === to be moved into Chrome

public:
    virtual void addMessageToConsole(const String& message,  unsigned int lineNumber, const String& sourceID) = 0;

    virtual void runJavaScriptAlert(const String& message) = 0;
    virtual bool runJavaScriptConfirm(const String& message) = 0;
    virtual bool runJavaScriptPrompt(const String& message, const String& defaultValue, String& result) = 0;  
    virtual bool shouldInterruptJavaScript() = 0;
    virtual void scheduleClose() = 0;
    virtual void focusWindow() = 0;
    virtual void unfocusWindow() = 0;
    virtual void print() = 0;

private:
    virtual void setStatusBarText(const String&);

// === to be moved into Editor

public:
    virtual String selectedText() const;  
    bool findString(const String&, bool, bool, bool);

    const Selection& mark() const; // Mark, to be used as emacs uses it.
    void setMark(const Selection&);

    void transpose();

    void copyToPasteboard();
    void cutToPasteboard();
    void pasteFromPasteboard();
    void pasteAndMatchStyle();
    void redo();
    void undo();
    virtual bool canRedo() const = 0;
    virtual bool canUndo() const = 0;
    void computeAndSetTypingStyle(CSSStyleDeclaration* , EditAction = EditActionUnspecified);
    enum TriState { falseTriState, trueTriState, mixedTriState };
    TriState selectionHasStyle(CSSStyleDeclaration*) const;
    String selectionStartStylePropertyValue(int stylePropertyID) const;
    void applyEditingStyleToBodyElement() const;
    void removeEditingStyleFromBodyElement() const;
    void applyEditingStyleToElement(Element*) const;
    void removeEditingStyleFromElement(Element*) const;

    virtual void markMisspellingsInAdjacentWords(const VisiblePosition&) = 0;
    virtual void markMisspellings(const Selection&) = 0;
    virtual Range* markedTextRange() const = 0;
    virtual void registerCommandForUndo(PassRefPtr<EditCommand>) = 0;
    virtual void registerCommandForRedo(PassRefPtr<EditCommand>) = 0;
    virtual void clearUndoRedoOperations() = 0;
    virtual void issueUndoCommand() = 0;
    virtual void issueRedoCommand() = 0;
    virtual void issueCutCommand() = 0;
    virtual void issueCopyCommand() = 0;
    virtual void issuePasteCommand() = 0;
    virtual void issuePasteAndMatchStyleCommand() = 0;
    virtual void issueTransposeCommand() = 0;
    virtual void respondToChangedSelection(const Selection& oldSelection, bool closeTyping) = 0;
    virtual void respondToChangedContents(const Selection& endingSelection) = 0;
    virtual bool shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity, bool stillSelecting) const = 0;

    RenderStyle* styleForSelectionStart(Node* &nodeToRemove) const;

    const Vector<MarkedTextUnderline>& markedTextUnderlines() const;  
    bool markedTextUsesUnderlines() const;
  
    unsigned markAllMatchesForText(const String&, bool caseFlag, unsigned limit);
    bool markedTextMatchesAreHighlighted() const;
    void setMarkedTextMatchesAreHighlighted(bool flag);
  
    CSSComputedStyleDeclaration* selectionComputedStyle(Node*& nodeToRemove) const;

// === to be moved into EventHandler

public:
    virtual bool shouldDragAutoNode(Node*, const IntPoint&) const; // -webkit-user-drag == auto

    virtual bool lastEventIsMouseUp() const = 0;

    virtual bool tabsToLinks() const;
    virtual bool tabsToAllControls() const;
    virtual void handleMousePressEvent(const MouseEventWithHitTestResults&);
    virtual void handleMouseMoveEvent(const MouseEventWithHitTestResults&);
    virtual void handleMouseReleaseEvent(const MouseEventWithHitTestResults&);

    void updateSelectionForMouseDragOverPosition(const VisiblePosition&);

    void selectClosestWordFromMouseEvent(const PlatformMouseEvent&, Node* innerNode);

    virtual bool mouseDownMayStartSelect() const { return true; }
    bool mouseDownMayStartAutoscroll() const;
    void setMouseDownMayStartAutoscroll(bool b);

    bool mouseDownMayStartDrag() const;
    void setMouseDownMayStartDrag(bool b);

    static Frame* frameForWidget(const Widget*);
    static Node* nodeForWidget(const Widget*);
    static Frame* frameForNode(Node*);

    // Call this method before handling a new user action, like on a mouse down or key down.
    // Currently, all this does is clear the "don't submit form twice" data member.
    void prepareForUserAction();
    Node* mousePressNode();

    void stopAutoscrollTimer(bool rendererIsBeingDestroyed = false);
    RenderObject* autoscrollRenderer() const;

    HitTestResult hitTestResultAtPoint(const IntPoint&, bool allowShadowContent);

    bool prohibitsScrolling() const;
    void setProhibitsScrolling(const bool);
  
private:
    void handleAutoscroll(RenderObject*);
    void startAutoscrollTimer();
    void setAutoscrollRenderer(RenderObject*);

    void autoscrollTimerFired(Timer<Frame>*);

    void handleMousePressEventSingleClick(const MouseEventWithHitTestResults&);
    void handleMousePressEventDoubleClick(const MouseEventWithHitTestResults&);
    void handleMousePressEventTripleClick(const MouseEventWithHitTestResults&);

// === to be moved into SelectionController

public:
    TextGranularity selectionGranularity() const;
    void setSelectionGranularity(TextGranularity) const;

    bool shouldChangeSelection(const Selection&) const;
    virtual bool shouldDeleteSelection(const Selection&) const;
    void clearCaretRectIfNeeded();
    void setFocusNodeIfNeeded();
    void selectionLayoutChanged();
    void notifyRendererOfSelectionChange(bool userTriggered);

    void invalidateSelection();

    void setCaretVisible(bool = true);
    void paintCaret(GraphicsContext*, const IntRect&) const;  
    void paintDragCaret(GraphicsContext*, const IntRect&) const;

    void setXPosForVerticalArrowNavigation(int);
    int xPosForVerticalArrowNavigation() const;

    virtual bool isContentEditable() const; // if true, everything in frame is editable

    virtual void textFieldDidBeginEditing(Element*);
    virtual void textFieldDidEndEditing(Element*);
    virtual void textDidChangeInTextField(Element*);
    virtual bool doTextFieldCommandFromEvent(Element*, const PlatformKeyboardEvent*);
    virtual void textWillBeDeletedInTextField(Element* input);
    virtual void textDidChangeInTextArea(Element*);

    virtual bool inputManagerHasMarkedText() const { return false; }

    virtual void setSecureKeyboardEntry(bool) { }
    virtual bool isSecureKeyboardEntry() { return false; }

    void appliedEditing(PassRefPtr<EditCommand>);
    void unappliedEditing(PassRefPtr<EditCommand>);
    void reappliedEditing(PassRefPtr<EditCommand>);

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

#endif // Frame_H
