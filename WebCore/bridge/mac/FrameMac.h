/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef FrameMac_h
#define FrameMac_h

#import "ClipboardAccessPolicy.h"
#import "Frame.h"
#import "PlatformMouseEvent.h"
#import "StringHash.h"
#import "WebCoreKeyboardAccess.h"

class NPObject;

namespace KJS {
    namespace Bindings {
        class Instance;
        class RootObject;
    }
}

#ifdef __OBJC__

@class NSArray;
@class NSAttributedString;
@class NSDictionary;
@class NSEvent;
@class NSFileWrapper;
@class NSFont;
@class NSImage;
@class NSMenu;
@class NSMutableDictionary;
@class NSString;
@class NSView;
@class WebCoreFrameBridge;
@class WebScriptObject;

#else

class NSArray;
class NSAttributedString;
class NSDictionary;
class NSEvent;
class NSFileWrapper;
class NSFont;
class NSImage;
class NSMenu;
class NSMutableDictionary;
class NSString;
class NSView;
class WebCoreFrameBridge;
class WebScriptObject;

typedef unsigned int NSDragOperation;
typedef int NSWritingDirection;

#endif

namespace WebCore {

class ClipboardMac;
class EditorClient;
class HTMLTableCellElement;
class VisiblePosition;

enum SelectionDirection {
    SelectingNext,
    SelectingPrevious
};

class FrameMac : public Frame
{
public:
    FrameMac(Page*, Element*, PassRefPtr<EditorClient>);
    ~FrameMac();

    // FIXME: Merge these and move them into FrameLoader.
    virtual void urlSelected(const FrameLoadRequest&, const Event* triggeringEvent);
    virtual Frame* createFrame(const KURL&, const String& name, Element* ownerElement, const String& referrer);
    virtual void submitForm(const FrameLoadRequest&);

    virtual Plugin* createPlugin(Element*, const KURL&,
        const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType);

    virtual void clear(bool clearWindowProperties = true);

    void setBridge(WebCoreFrameBridge* p);
    WebCoreFrameBridge* bridge() const { return _bridge; }

    virtual void setView(FrameView*);
    virtual void frameDetached();

    void advanceToNextMisspelling(bool startBeforeSelection = false);
    
    virtual void setTitle(const String&);
    virtual void setStatusBarText(const String&);

    virtual ObjectContentType objectContentType(const KURL& url, const String& mimeType);
    virtual void redirectDataToPlugin(Widget* pluginWidget);

    virtual void scheduleClose();

    virtual void focusWindow();
    virtual void unfocusWindow();

    virtual void saveDocumentState();
    virtual void restoreDocumentState();
    
    virtual void addMessageToConsole(const String& message,  unsigned int lineNumber, const String& sourceID);
    
    NSView* nextKeyView(Node* startingPoint, SelectionDirection);
    NSView* nextKeyViewInFrameHierarchy(Node* startingPoint, SelectionDirection);
    static NSView* nextKeyViewForWidget(Widget* startingPoint, SelectionDirection);
    static bool currentEventIsKeyboardOptionTab();
    static bool handleKeyboardOptionTabInView(NSView* view);
    
    virtual bool tabsToLinks() const;
    virtual bool tabsToAllControls() const;
    
    static bool currentEventIsMouseDownInWidget(Widget* candidate);
    
    virtual void runJavaScriptAlert(const String&);
    virtual bool runJavaScriptConfirm(const String&);
    virtual bool runJavaScriptPrompt(const String& message, const String& defaultValue, String& result);
    virtual bool shouldInterruptJavaScript();    
    virtual bool locationbarVisible();
    virtual bool menubarVisible();
    virtual bool personalbarVisible();
    virtual bool statusbarVisible();
    virtual bool toolbarVisible();

    bool shouldClose();

    virtual void createEmptyDocument();

    static WebCoreFrameBridge* bridgeForWidget(const Widget*);
    
    virtual String userAgent() const;

    virtual String mimeTypeForFileName(const String&) const;

    NSImage* selectionImage(bool forceWhiteText = false) const;
    NSImage* snapshotDragImage(Node* node, NSRect* imageRect, NSRect* elementRect) const;

    bool dispatchDragSrcEvent(const AtomicString &eventType, const PlatformMouseEvent&) const;

    NSFont* fontForSelection(bool* hasMultipleFonts) const;
    NSDictionary* fontAttributesForSelectionStart() const;
    
    NSWritingDirection baseWritingDirectionForSelectionStart() const;

    virtual void markMisspellingsInAdjacentWords(const VisiblePosition&);
    virtual void markMisspellings(const Selection&);

    void mouseDown(NSEvent*);
    void mouseDragged(NSEvent*);
    void mouseUp(NSEvent*);
    void mouseMoved(NSEvent*);
    bool keyEvent(NSEvent*);
    bool wheelEvent(NSEvent*);

    void sendFakeEventsAfterWidgetTracking(NSEvent* initiatingEvent);

    virtual bool lastEventIsMouseUp() const;
    void setActivationEventNumber(int num) { _activationEventNumber = num; }

    bool dragHysteresisExceeded(float dragLocationX, float dragLocationY) const;
    bool eventMayStartDrag(NSEvent*) const;
    void dragSourceMovedTo(const PlatformMouseEvent&);
    void dragSourceEndedAt(const PlatformMouseEvent&, NSDragOperation);

    bool canDHTMLCut();
    bool canDHTMLCopy();
    bool canDHTMLPaste();
    bool tryDHTMLCut();
    bool tryDHTMLCopy();
    bool tryDHTMLPaste();
    
    bool sendContextMenuEvent(NSEvent*);

    bool passWidgetMouseDownEventToWidget(const MouseEventWithHitTestResults&);
    bool passWidgetMouseDownEventToWidget(RenderWidget*);
    bool passMouseDownEventToWidget(Widget*);
    bool passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame* subframePart);
    bool passWheelEventToWidget(Widget*);
    
    NSString* searchForLabelsAboveCell(RegularExpression*, HTMLTableCellElement*);
    NSString* searchForLabelsBeforeElement(NSArray* labels, Element* element);
    NSString* matchLabelsAgainstElement(NSArray* labels, Element* element);

    virtual void tokenizerProcessedData();

    virtual String overrideMediaType() const;
    
    WebCoreKeyboardUIMode keyboardUIMode() const;

    void didTellBridgeAboutLoad(const String& URL);
    bool haveToldBridgeAboutLoad(const String& URL);

    virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getAppletInstanceForWidget(Widget*);
    virtual KJS::Bindings::RootObject* bindingRootObject();
    virtual Widget* createJavaAppletWidget(const IntSize&, Element*, const HashMap<String, String>& args);

    void addPluginRootObject(KJS::Bindings::RootObject* root);
    
    virtual void registerCommandForUndo(PassRefPtr<EditCommand>);
    virtual void registerCommandForRedo(PassRefPtr<EditCommand>);
    virtual void clearUndoRedoOperations();
    virtual void issueUndoCommand();
    virtual void issueRedoCommand();
    virtual void issueCutCommand();
    virtual void issueCopyCommand();
    virtual void issuePasteCommand();
    virtual void issuePasteAndMatchStyleCommand();
    virtual void issueTransposeCommand();
    virtual void respondToChangedSelection(const Selection& oldSelection, bool closeTyping);
    virtual void respondToChangedContents(const Selection&);
    virtual bool isContentEditable() const;
    virtual bool shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity affinity, bool stillSelecting) const;
    virtual bool shouldDeleteSelection(const Selection&) const;
    virtual bool shouldBeginEditing(const Range*) const;
    virtual bool shouldEndEditing(const Range*) const;
    virtual void didBeginEditing() const;
    virtual void didEndEditing() const;
    
    virtual void textFieldDidBeginEditing(Element*);
    virtual void textFieldDidEndEditing(Element*);
    virtual void textDidChangeInTextField(Element*);
    virtual bool doTextFieldCommandFromEvent(Element*, const PlatformKeyboardEvent*);
    virtual void textWillBeDeletedInTextField(Element*);
    virtual void textDidChangeInTextArea(Element*);
    
    virtual bool inputManagerHasMarkedText() const;
    
    virtual void setSecureKeyboardEntry(bool);
    virtual bool isSecureKeyboardEntry();

    KJS::Bindings::RootObject* executionContextForDOM();
    
    WebScriptObject* windowScriptObject();
    NPObject* windowScriptNPObject();
    
    virtual void partClearedInBegin();
    
    // Implementation of CSS property -webkit-user-drag == auto
    virtual bool shouldDragAutoNode(Node*, const IntPoint&) const;

    void setMarkedTextRange(const Range* , NSArray* attributes, NSArray* ranges);
    virtual Range* markedTextRange() const { return m_markedTextRange.get(); }

    virtual void didFirstLayout();
    
    NSMutableDictionary* dashboardRegionsDictionary();
    void dashboardRegionsChanged();

    void willPopupMenu(NSMenu *);
    
    virtual bool isCharacterSmartReplaceExempt(UChar, bool);
    
    virtual bool mouseDownMayStartSelect() const { return _mouseDownMayStartSelect; }
    
    virtual void handledOnloadEvents();

    virtual bool canRedo() const;
    virtual bool canUndo() const;
    virtual void print();

    FloatRect customHighlightLineRect(const AtomicString& type, const FloatRect& lineRect);
    void paintCustomHighlight(const AtomicString& type, const FloatRect& boxRect, const FloatRect& lineRect, bool text, bool line);
    
    NSEvent* currentEvent() { return _currentEvent; }
    virtual KURL originalRequestURL() const;

    virtual bool canGoBackOrForward(int distance) const;
    virtual void goBackOrForward(int distance);
    virtual int getHistoryLength();
    virtual KURL historyURL(int distance);

protected:
    virtual void startRedirectionTimer();
    virtual void stopRedirectionTimer();
    virtual void cleanupPluginObjects();
    virtual bool isLoadTypeReload();
    
private:
    virtual void handleMousePressEvent(const MouseEventWithHitTestResults&);
    virtual void handleMouseMoveEvent(const MouseEventWithHitTestResults&);
    virtual void handleMouseReleaseEvent(const MouseEventWithHitTestResults&);
      
    NSView* mouseDownViewIfStillGood();

    NSView* nextKeyViewInFrame(Node* startingPoint, SelectionDirection, bool* focusCallResultedInViewBeingCreated = 0);
    static NSView* documentViewForNode(Node*);
    
    bool dispatchCPPEvent(const AtomicString &eventType, ClipboardAccessPolicy policy);

    NSImage* imageFromRect(NSRect) const;

    void freeClipboard();

    void registerCommandForUndoOrRedo(PassRefPtr<EditCommand>, bool isRedo);

    WebCoreFrameBridge* _bridge;
    
    NSView* _mouseDownView;
    bool _mouseDownWasInSubframe;
    bool _sendingEventToSubview;
    bool _mouseDownMayStartSelect;
    PlatformMouseEvent m_mouseDown;
    // in our view's coords
    IntPoint m_mouseDownPos;
    float _mouseDownTimestamp;
    int _activationEventNumber;
    
    static NSEvent* _currentEvent;

    bool _haveUndoRedoOperations;
    
    HashSet<String> urlsBridgeKnowsAbout;

    friend class Frame;

    KJS::Bindings::RootObject* _bindingRoot;  // The root object used for objects
                                            // bound outside the context of a plugin.
    Vector<KJS::Bindings::RootObject*> m_rootObjects;
    WebScriptObject* _windowScriptObject;
    NPObject* _windowScriptNPObject;
    
    RefPtr<Node> _dragSrc;     // element that may be a drag source, for the current mouse gesture
    bool _dragSrcIsLink;
    bool _dragSrcIsImage;
    bool _dragSrcInSelection;
    bool _dragSrcMayBeDHTML, _dragSrcMayBeUA;   // Are DHTML and/or the UserAgent allowed to drag out?
    bool _dragSrcIsDHTML;
    RefPtr<ClipboardMac> _dragClipboard;   // used on only the source side of dragging
    
    RefPtr<Range> m_markedTextRange;
};

inline FrameMac* Mac(Frame* frame) { return static_cast<FrameMac*>(frame); }
inline const FrameMac* Mac(const Frame* frame) { return static_cast<const FrameMac*>(frame); }

}

#endif
