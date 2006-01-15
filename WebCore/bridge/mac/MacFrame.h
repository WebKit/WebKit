/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#ifndef MacFrame_H
#define MacFrame_H

#include "Frame.h"

#include "text_affinity.h"
#include "NodeImpl.h"

#include "WebCoreKeyboardAccess.h"

#if __APPLE__
#import <CoreFoundation/CoreFoundation.h>
#endif

#include "KWQDict.h"
#include "KWQClipboard.h"
#include "KWQScrollBar.h"

class FramePrivate;
class KWQWindowWidget;
class NPObject;

namespace DOM {
    class DocumentFragmentImpl;
    class HTMLTableCellElementImpl;
}

namespace khtml {
    class RenderObject;
    class RenderStyle;
    class VisiblePosition;
    struct DashboardRegionValue;
}

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

enum KWQSelectionDirection {
    KWQSelectingNext,
    KWQSelectingPrevious
};

class MacFrame : public Frame
{
public:
    MacFrame();
    ~MacFrame();
    
    void clear();

    void setBridge(WebCoreFrameBridge *p);
    WebCoreFrameBridge *bridge() const { return _bridge; }
    void setView(KHTMLView *view);

    virtual bool openURL(const KURL &);
    
    void openURLRequest(const KURL &, const KParts::URLArgs &);
    void submitForm(const KURL &, const KParts::URLArgs &);

    void scheduleHistoryNavigation( int steps );
    
    QString advanceToNextMisspelling(bool startBeforeSelection = false);
    
    void setTitle(const DOM::DOMString &);
    void setStatusBarText(const QString &status);

    void urlSelected(const KURL &url, int button, int state, const KParts::URLArgs &args);
    ObjectContents *createPart(const khtml::ChildFrame &child, const KURL &url, const QString &mimeType);

    void scheduleClose();

    void unfocusWindow();
    
    void openURLFromPageCache(KWQPageState *state);

    void saveDocumentState();
    void restoreDocumentState();
    
    void addMessageToConsole(const DOM::DOMString& message,  unsigned int lineNumber, const DOM::DOMString& sourceID);
    void setDisplaysWithFocusAttributes(bool flag);
    
    NSView *nextKeyView(DOM::NodeImpl *startingPoint, KWQSelectionDirection);
    NSView *nextKeyViewInFrameHierarchy(DOM::NodeImpl *startingPoint, KWQSelectionDirection);
    static NSView *nextKeyViewForWidget(QWidget *startingPoint, KWQSelectionDirection);
    static bool currentEventIsKeyboardOptionTab();
    static bool handleKeyboardOptionTabInView(NSView *view);
    
    virtual bool tabsToLinks() const;
    virtual bool tabsToAllControls() const;
    
    static bool currentEventIsMouseDownInWidget(QWidget *candidate);
    
    void runJavaScriptAlert(const DOM::DOMString& message);
    bool runJavaScriptConfirm(const DOM::DOMString& message);
    bool runJavaScriptPrompt(const DOM::DOMString& message, const DOM::DOMString& defaultValue, DOM::DOMString& result);
    bool locationbarVisible();
    bool menubarVisible();
    bool personalbarVisible();
    bool statusbarVisible();
    bool toolbarVisible();

    bool shouldClose();


    void createEmptyDocument();

    static WebCoreFrameBridge *bridgeForWidget(const QWidget *);
    
    QString requestedURLString() const;
    QString incomingReferrer() const;
    QString userAgent() const;

    QString mimeTypeForFileName(const QString &) const;
    
    NSRect visibleSelectionRect() const;
    NSImage *selectionImage() const;
    NSImage *snapshotDragImage(DOM::NodeImpl *node, NSRect *imageRect, NSRect *elementRect) const;
    bool dispatchDragSrcEvent(const DOM::AtomicString &eventType, const IntPoint &loc) const;

    NSFont *fontForSelection(bool *hasMultipleFonts) const;
    NSDictionary *fontAttributesForSelectionStart() const;
    
    NSWritingDirection baseWritingDirectionForSelectionStart() const;

    void markMisspellingsInAdjacentWords(const khtml::VisiblePosition &);
    void markMisspellings(const khtml::SelectionController &);

    NSFileWrapper *fileWrapperForElement(DOM::ElementImpl *);
    NSAttributedString *attributedString(DOM::NodeImpl *startNode, int startOffset, DOM::NodeImpl *endNode, int endOffset);

    void mouseDown(NSEvent *);
    void mouseDragged(NSEvent *);
    void mouseUp(NSEvent *);
    void mouseMoved(NSEvent *);
    bool keyEvent(NSEvent *);
    bool wheelEvent(NSEvent *);

    void sendFakeEventsAfterWidgetTracking(NSEvent *initiatingEvent);

    bool lastEventIsMouseUp() const;
    void setActivationEventNumber(int num) { _activationEventNumber = num; }

    bool dragHysteresisExceeded(float dragLocationX, float dragLocationY) const;
    bool eventMayStartDrag(NSEvent *) const;
    void dragSourceMovedTo(const IntPoint &loc);
    void dragSourceEndedAt(const IntPoint &loc, NSDragOperation operation);

    bool mayCut();
    bool mayCopy();
    bool mayPaste();
    bool tryCut();
    bool tryCopy();
    bool tryPaste();
    
    bool sendContextMenuEvent(NSEvent *);

    bool passMouseDownEventToWidget(QWidget *);
    bool passSubframeEventToSubframe(DOM::NodeImpl::MouseEvent &);
    bool passWheelEventToChildWidget(DOM::NodeImpl *);
    
    void redirectionTimerStartedOrStopped();
    
    void clearRecordedFormValues();
    void recordFormValue(const QString &name, const QString &value, DOM::HTMLFormElementImpl *element);

    NSString *searchForLabelsAboveCell(QRegExp *regExp, DOM::HTMLTableCellElementImpl *cell);
    NSString *searchForLabelsBeforeElement(NSArray *labels, DOM::ElementImpl *element);
    NSString *matchLabelsAgainstElement(NSArray *labels, DOM::ElementImpl *element);

    bool findString(NSString *str, bool forward, bool caseFlag, bool wrapFlag);

    KWQWindowWidget *topLevelWidget();
    
    void tokenizerProcessedData();

    QString overrideMediaType() const;
    
    NSColor *bodyBackgroundColor() const;
    
    WebCoreKeyboardUIMode keyboardUIMode() const;

    void didTellBridgeAboutLoad(const QString &urlString);
    bool haveToldBridgeAboutLoad(const QString &urlString);

    KJS::Bindings::Instance *getEmbedInstanceForWidget(QWidget*);
    KJS::Bindings::Instance *getObjectInstanceForWidget(QWidget*);
    KJS::Bindings::Instance *getAppletInstanceForWidget(QWidget*);
    void addPluginRootObject(const KJS::Bindings::RootObject *root);
    void cleanupPluginRootObjects();
    
    void registerCommandForUndo(const khtml::EditCommandPtr &);
    void registerCommandForRedo(const khtml::EditCommandPtr &);
    void clearUndoRedoOperations();
    void issueUndoCommand();
    void issueRedoCommand();
    void issueCutCommand();
    void issueCopyCommand();
    void issuePasteCommand();
    void issuePasteAndMatchStyleCommand();
    void issueTransposeCommand();
    void respondToChangedSelection(const khtml::SelectionController &oldSelection, bool closeTyping);
    void respondToChangedContents();
    virtual bool isContentEditable() const;
    virtual bool shouldChangeSelection(const khtml::SelectionController &oldSelection, const khtml::SelectionController &newSelection, khtml::EAffinity affinity, bool stillSelecting) const;
    virtual bool shouldBeginEditing(const DOM::RangeImpl *) const;
    virtual bool shouldEndEditing(const DOM::RangeImpl *) const;

    KJS::Bindings::RootObject *executionContextForDOM();
    KJS::Bindings::RootObject *bindingRootObject();
    
    WebScriptObject *windowScriptObject();
    NPObject *windowScriptNPObject();
    
    void partClearedInBegin();
    
    // Implementation of CSS property -khtml-user-drag == auto
    bool shouldDragAutoNode(DOM::NodeImpl*, int x, int y) const;

    void setMarkedTextRange(const DOM::RangeImpl *, NSArray *attributes, NSArray *ranges);
    DOM::RangeImpl *markedTextRange() const { return m_markedTextRange.get(); }

    bool canGoBackOrForward(int distance) const;

    void didFirstLayout();
    
    NSMutableDictionary *dashboardRegionsDictionary();
    void dashboardRegionsChanged();
    
    virtual bool isCharacterSmartReplaceExempt(const QChar &, bool);
    
    virtual bool mouseDownMayStartSelect() const { return _mouseDownMayStartSelect; }
    
    void handledOnloadEvents();
    
private:
    virtual void khtmlMousePressEvent(khtml::MousePressEvent *);
    virtual void khtmlMouseMoveEvent(khtml::MouseMoveEvent *);
    virtual void khtmlMouseReleaseEvent(khtml::MouseReleaseEvent *);
    
    NSView *mouseDownViewIfStillGood();

    QString generateFrameName();

    NSView *nextKeyViewInFrame(DOM::NodeImpl *startingPoint, KWQSelectionDirection);
    static NSView *documentViewForNode(DOM::NodeImpl *);
    
    bool dispatchCPPEvent(const DOM::AtomicString &eventType, KWQClipboard::AccessPolicy policy);

    NSImage *imageFromRect(NSRect rect) const;

    void freeClipboard();

    void registerCommandForUndoOrRedo(const khtml::EditCommandPtr &cmd, bool isRedo);

    WebCoreFrameBridge *_bridge;
    
    KWQSignal _started;
    KWQSignal _completed;
    KWQSignal _completedWithBool;
    
    NSView *_mouseDownView;
    bool _mouseDownWasInSubframe;
    bool _sendingEventToSubview;
    bool _mouseDownMayStartDrag;
    bool _mouseDownMayStartSelect;
    // in our window's coords
    int _mouseDownWinX, _mouseDownWinY;
    // in our view's coords
    int _mouseDownX, _mouseDownY;
    float _mouseDownTimestamp;
    int _activationEventNumber;
    
    static NSEvent *_currentEvent;

    NSMutableDictionary *_formValuesAboutToBeSubmitted;
    ObjCDOMElement *_formAboutToBeSubmitted;

    KWQWindowWidget *_windowWidget;

    bool _haveUndoRedoOperations;
    
    QDict<char> urlsBridgeKnowsAbout;

    friend class Frame;

    KJS::Bindings::RootObject *_bindingRoot;  // The root object used for objects
                                            // bound outside the context of a plugin.
    QPtrList<KJS::Bindings::RootObject> rootObjects;
    WebScriptObject *_windowScriptObject;
    NPObject *_windowScriptNPObject;
    
    RefPtr<DOM::NodeImpl> _dragSrc;     // element that may be a drag source, for the current mouse gesture
    bool _dragSrcIsLink;
    bool _dragSrcIsImage;
    bool _dragSrcInSelection;
    bool _dragSrcMayBeDHTML, _dragSrcMayBeUA;   // Are DHTML and/or the UserAgent allowed to drag out?
    bool _dragSrcIsDHTML;
    KWQClipboard *_dragClipboard;   // used on only the source side of dragging
    
    RefPtr<DOM::RangeImpl> m_markedTextRange;
};

inline MacFrame *Mac(Frame *frame) { return static_cast<MacFrame *>(frame); }
inline const MacFrame *Mac(const Frame *frame) { return static_cast<const MacFrame *>(frame); }

#endif
