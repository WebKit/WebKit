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

#import "ClipboardMac.h"
#import "Frame.h"
#import "IntRect.h"
#import "PlatformMouseEvent.h"
#import "StringHash.h"
#import "WebCoreKeyboardAccess.h"

class NPObject;

namespace KJS {
    class PausedTimeouts;
    class SavedProperties;
    class SavedBuiltins;
    class ScheduledAction;
    namespace Bindings {
        class Instance;
        class RootObject;
    }
}

#ifdef __OBJC__

// Avoid clashes with KJS::DOMElement in KHTML code.
@class DOMElement;
typedef DOMElement ObjCDOMElement;

@class KWQPageState;
@class NSArray;
@class NSAttributedString;
@class NSColor;
@class NSDictionary;
@class NSEvent;
@class NSFileWrapper;
@class NSFont;
@class NSImage;
@class NSMutableDictionary;
@class NSResponder;
@class NSString;
@class NSView;
@class WebCoreFrameBridge;
@class WebScriptObject;

#else

// Avoid clashes with KJS::DOMElement in KHTML code.
class ObjCDOMElement;

class KWQPageState;
class NSArray;
class NSAttributedString;
class NSColor;
class NSDictionary;
class NSEvent;
class NSFileWrapper;
class NSFont;
class NSImage;
class NSMutableDictionary;
class NSResponder;
class NSString;
class NSView;
class WebCoreFrameBridge;
class WebScriptObject;

typedef int NSWritingDirection;

#endif

namespace WebCore {

class DocumentFragment;
class FramePrivate;
class HTMLTableCellElement;
class RenderObject;
class RenderStyle;
class VisiblePosition;

struct DashboardRegionValue;

enum KWQSelectionDirection {
    KWQSelectingNext,
    KWQSelectingPrevious
};

class FrameMac : public Frame
{
public:
    FrameMac(Page*, RenderPart*);
    ~FrameMac();
    
    void clear();

    void setBridge(WebCoreFrameBridge* p);
    WebCoreFrameBridge* bridge() const { return _bridge; }
    virtual void setView(FrameView*);
    virtual void frameDetached();

    virtual bool openURL(const KURL&);
    
    virtual void openURLRequest(const ResourceRequest&);
    virtual void submitForm(const ResourceRequest&);

    String advanceToNextMisspelling(bool startBeforeSelection = false);
    
    virtual void setTitle(const String&);
    virtual void setStatusBarText(const String&);

    virtual void urlSelected(const ResourceRequest&);

    virtual ObjectContentType objectContentType(const KURL& url, const String& mimeType);
    virtual Plugin* createPlugin(const KURL& url, const Vector<String>& paramNames, const Vector<String>& paramValues, const String& mimeType);
    virtual Frame* createFrame(const KURL& url, const String& name, RenderPart* renderer, const String& referrer);

    virtual void scheduleClose();

    virtual void unfocusWindow();
    
    void openURLFromPageCache(KWQPageState*);

    virtual void saveDocumentState();
    virtual void restoreDocumentState();
    
    virtual void addMessageToConsole(const String& message,  unsigned int lineNumber, const String& sourceID);
    virtual void setDisplaysWithFocusAttributes(bool);
    
    NSView* nextKeyView(Node* startingPoint, KWQSelectionDirection);
    NSView* nextKeyViewInFrameHierarchy(Node* startingPoint, KWQSelectionDirection);
    static NSView* nextKeyViewForWidget(Widget* startingPoint, KWQSelectionDirection);
    static bool currentEventIsKeyboardOptionTab();
    static bool handleKeyboardOptionTabInView(NSView* view);
    
    virtual bool tabsToLinks() const;
    virtual bool tabsToAllControls() const;
    
    static bool currentEventIsMouseDownInWidget(Widget* candidate);
    
    virtual void runJavaScriptAlert(const String&);
    virtual bool runJavaScriptConfirm(const String&);
    virtual bool runJavaScriptPrompt(const String& message, const String& defaultValue, String& result);
    virtual bool locationbarVisible();
    virtual bool menubarVisible();
    virtual bool personalbarVisible();
    virtual bool statusbarVisible();
    virtual bool toolbarVisible();

    bool shouldClose();

    virtual void createEmptyDocument();

    static WebCoreFrameBridge* bridgeForWidget(const Widget*);
    
    virtual String incomingReferrer() const;
    virtual String userAgent() const;

    virtual String mimeTypeForFileName(const String&) const;

    NSImage* selectionImage() const;
    NSImage* snapshotDragImage(Node* node, NSRect* imageRect, NSRect* elementRect) const;

    bool dispatchDragSrcEvent(const AtomicString &eventType, const PlatformMouseEvent&) const;

    NSFont* fontForSelection(bool* hasMultipleFonts) const;
    NSDictionary* fontAttributesForSelectionStart() const;
    
    NSWritingDirection baseWritingDirectionForSelectionStart() const;

    virtual void markMisspellingsInAdjacentWords(const VisiblePosition&);
    virtual void markMisspellings(const SelectionController&);

    NSFileWrapper* fileWrapperForElement(Element*);
    NSAttributedString* attributedString(Node* startNode, int startOffset, Node* endNode, int endOffset);

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

    bool mayCut();
    bool mayCopy();
    bool mayPaste();
    bool tryCut();
    bool tryCopy();
    bool tryPaste();
    
    bool sendContextMenuEvent(NSEvent*);

    virtual bool passMouseDownEventToWidget(Widget*);
    virtual bool passSubframeEventToSubframe(MouseEventWithHitTestResults&, Frame* subframePart);
    virtual bool passWheelEventToChildWidget(Node*);
    
    NSString* searchForLabelsAboveCell(RegularExpression* regExp, HTMLTableCellElement* cell);
    NSString* searchForLabelsBeforeElement(NSArray* labels, Element* element);
    NSString* matchLabelsAgainstElement(NSArray* labels, Element* element);

    bool findString(NSString* str, bool forward, bool caseFlag, bool wrapFlag);

    virtual void tokenizerProcessedData();

    virtual String overrideMediaType() const;
    
    NSColor* bodyBackgroundColor() const;
    
    WebCoreKeyboardUIMode keyboardUIMode() const;

    void didTellBridgeAboutLoad(const String& URL);
    bool haveToldBridgeAboutLoad(const String& URL);

    virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getAppletInstanceForWidget(Widget*);
    void addPluginRootObject(KJS::Bindings::RootObject* root);
    void cleanupPluginRootObjects();
    
    virtual void registerCommandForUndo(const EditCommandPtr&);
    virtual void registerCommandForRedo(const EditCommandPtr&);
    virtual void clearUndoRedoOperations();
    virtual void issueUndoCommand();
    virtual void issueRedoCommand();
    virtual void issueCutCommand();
    virtual void issueCopyCommand();
    virtual void issuePasteCommand();
    virtual void issuePasteAndMatchStyleCommand();
    virtual void issueTransposeCommand();
    virtual void respondToChangedSelection(const SelectionController &oldSelection, bool closeTyping);
    virtual void respondToChangedContents();
    virtual bool isContentEditable() const;
    virtual bool shouldChangeSelection(const SelectionController &oldSelection, const SelectionController &newSelection, EAffinity affinity, bool stillSelecting) const;
    virtual bool shouldBeginEditing(const Range*) const;
    virtual bool shouldEndEditing(const Range*) const;
    virtual void didBeginEditing() const;
    virtual void didEndEditing() const;
    
    virtual void textFieldDidBeginEditing(Element*);
    virtual void textFieldDidEndEditing(Element*);
    virtual void textDidChangeInTextField(Element*);
    virtual bool doTextFieldCommandFromEvent(Element*, const PlatformKeyboardEvent*);
    virtual void textWillBeDeletedInTextField(Element* input);

    KJS::Bindings::RootObject* executionContextForDOM();
    KJS::Bindings::RootObject* bindingRootObject();
    
    WebScriptObject* windowScriptObject();
    NPObject* windowScriptNPObject();
    
    virtual void partClearedInBegin();
    
    // Implementation of CSS property -webkit-user-drag == auto
    virtual bool shouldDragAutoNode(Node*, const IntPoint&) const;

    void setMarkedTextRange(const Range* , NSArray* attributes, NSArray* ranges);
    virtual Range* markedTextRange() const { return m_markedTextRange.get(); }

    virtual bool canGoBackOrForward(int distance) const;

    virtual void didFirstLayout();
    
    NSMutableDictionary* dashboardRegionsDictionary();
    void dashboardRegionsChanged();
    
    virtual bool isCharacterSmartReplaceExempt(const QChar &, bool);
    
    virtual bool mouseDownMayStartSelect() const { return _mouseDownMayStartSelect; }
    
    virtual void handledOnloadEvents();

    virtual bool canPaste() const;
    virtual bool canRedo() const;
    virtual bool canUndo() const;
    virtual void print();

protected:
    virtual void startRedirectionTimer();
    virtual void stopRedirectionTimer();

private:
    virtual void handleMousePressEvent(const MouseEventWithHitTestResults&);
    virtual void handleMouseMoveEvent(const MouseEventWithHitTestResults&);
    virtual void handleMouseReleaseEvent(const MouseEventWithHitTestResults&);
    
    NSView* mouseDownViewIfStillGood();

    NSView* nextKeyViewInFrame(Node* startingPoint, KWQSelectionDirection);
    static NSView* documentViewForNode(Node*);
    
    bool dispatchCPPEvent(const AtomicString &eventType, ClipboardMac::AccessPolicy policy);

    NSImage* imageFromRect(NSRect) const;

    void freeClipboard();

    void registerCommandForUndoOrRedo(const EditCommandPtr &cmd, bool isRedo);

    WebCoreFrameBridge* _bridge;
    
    NSView* _mouseDownView;
    bool _mouseDownWasInSubframe;
    bool _sendingEventToSubview;
    bool _mouseDownMayStartDrag;
    bool _mouseDownMayStartSelect;
    PlatformMouseEvent m_mouseDown;
    // in our view's coords
    IntPoint m_mouseDownPos;
    float _mouseDownTimestamp;
    int _activationEventNumber;
    
    static NSEvent* _currentEvent;

    bool _haveUndoRedoOperations;
    
    HashSet<RefPtr<StringImpl> > urlsBridgeKnowsAbout;

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
