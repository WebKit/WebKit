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

#ifndef MacFrame_H
#define MacFrame_H

#include "Frame.h"
#include "IntRect.h"
#include "KWQClipboard.h"
#include "KWQScrollBar.h"
#include "NodeImpl.h"
#include "WebCoreKeyboardAccess.h"
#include "text_affinity.h"
#include <kxmlcore/HashSet.h>
#include "BrowserExtensionMac.h"

#import <CoreFoundation/CoreFoundation.h>

class KWQWindowWidget;
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

class DocumentFragmentImpl;
class FramePrivate;
class HTMLTableCellElementImpl;
class RenderObject;
class RenderStyle;
class VisiblePosition;

struct DashboardRegionValue;

enum KWQSelectionDirection {
    KWQSelectingNext,
    KWQSelectingPrevious
};

class MacFrame : public Frame
{
public:
    MacFrame(RenderPart*);
    ~MacFrame();
    
    void clear();

    void setBridge(WebCoreFrameBridge *p);
    WebCoreFrameBridge *bridge() const { return _bridge; }
    void setView(FrameView *view);
    virtual void frameDetached();

    virtual bool openURL(const KURL &);
    
    virtual void openURLRequest(const KURL &, const URLArgs &);
    virtual void submitForm(const KURL &, const URLArgs &);

    void scheduleHistoryNavigation( int steps );
    
    QString advanceToNextMisspelling(bool startBeforeSelection = false);
    
    virtual void setTitle(const DOMString &);
    void setStatusBarText(const QString &status);

    virtual void urlSelected(const KURL &url, int button, int state, const URLArgs &args);

    virtual ObjectContentType objectContentType(const KURL& url, const QString& mimeType);
    virtual Plugin* createPlugin(const KURL& url, const QStringList& paramNames, const QStringList& paramValues, const QString& mimeType);
    virtual Frame* createFrame(const KURL& url, const QString& name, RenderPart* renderer, const DOMString& referrer);

    virtual void scheduleClose();

    virtual void unfocusWindow();
    
    void openURLFromPageCache(KWQPageState *state);

    virtual void saveDocumentState();
    virtual void restoreDocumentState();
    
    virtual void addMessageToConsole(const DOMString& message,  unsigned int lineNumber, const DOMString& sourceID);
    void setDisplaysWithFocusAttributes(bool flag);
    
    NSView *nextKeyView(NodeImpl *startingPoint, KWQSelectionDirection);
    NSView *nextKeyViewInFrameHierarchy(NodeImpl *startingPoint, KWQSelectionDirection);
    static NSView *nextKeyViewForWidget(QWidget *startingPoint, KWQSelectionDirection);
    static bool currentEventIsKeyboardOptionTab();
    static bool handleKeyboardOptionTabInView(NSView *view);
    
    virtual bool tabsToLinks() const;
    virtual bool tabsToAllControls() const;
    
    static bool currentEventIsMouseDownInWidget(QWidget *candidate);
    
    virtual void runJavaScriptAlert(const DOMString& message);
    virtual bool runJavaScriptConfirm(const DOMString& message);
    virtual bool runJavaScriptPrompt(const DOMString& message, const DOMString& defaultValue, DOMString& result);
    virtual bool locationbarVisible();
    virtual bool menubarVisible();
    virtual bool personalbarVisible();
    virtual bool statusbarVisible();
    virtual bool toolbarVisible();

    bool shouldClose();

    virtual void createEmptyDocument();

    virtual BrowserExtension* createBrowserExtension() { return new BrowserExtensionMac(this); }

    static WebCoreFrameBridge *bridgeForWidget(const QWidget *);
    
    QString requestedURLString() const;
    virtual QString incomingReferrer() const;
    virtual QString userAgent() const;

    virtual QString mimeTypeForFileName(const QString &) const;

    NSRect visibleSelectionRect() const;
    NSImage *selectionImage() const;
    NSImage *snapshotDragImage(NodeImpl *node, NSRect *imageRect, NSRect *elementRect) const;

    bool dispatchDragSrcEvent(const AtomicString &eventType, const IntPoint &loc) const;

    NSFont *fontForSelection(bool *hasMultipleFonts) const;
    NSDictionary *fontAttributesForSelectionStart() const;
    
    NSWritingDirection baseWritingDirectionForSelectionStart() const;

    virtual void markMisspellingsInAdjacentWords(const VisiblePosition &);
    virtual void markMisspellings(const SelectionController &);

    NSFileWrapper *fileWrapperForElement(ElementImpl *);
    NSAttributedString *attributedString(NodeImpl *startNode, int startOffset, NodeImpl *endNode, int endOffset);

    void mouseDown(NSEvent *);
    void mouseDragged(NSEvent *);
    void mouseUp(NSEvent *);
    void mouseMoved(NSEvent *);
    bool keyEvent(NSEvent *);
    bool wheelEvent(NSEvent *);

    void sendFakeEventsAfterWidgetTracking(NSEvent *initiatingEvent);

    virtual bool lastEventIsMouseUp() const;
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
    virtual bool passSubframeEventToSubframe(NodeImpl::MouseEvent &);
    virtual bool passWheelEventToChildWidget(NodeImpl *);
    
    virtual void clearRecordedFormValues();
    virtual void recordFormValue(const QString &name, const QString &value, HTMLFormElementImpl *element);

    NSString *searchForLabelsAboveCell(QRegExp *regExp, HTMLTableCellElementImpl *cell);
    NSString *searchForLabelsBeforeElement(NSArray *labels, ElementImpl *element);
    NSString *matchLabelsAgainstElement(NSArray *labels, ElementImpl *element);

    bool findString(NSString *str, bool forward, bool caseFlag, bool wrapFlag);

    KWQWindowWidget *topLevelWidget();
    
    void tokenizerProcessedData();

    virtual QString overrideMediaType() const;
    
    NSColor *bodyBackgroundColor() const;
    
    WebCoreKeyboardUIMode keyboardUIMode() const;

    void didTellBridgeAboutLoad(const DOMString& URL);
    bool haveToldBridgeAboutLoad(const DOMString& URL);

    virtual KJS::Bindings::Instance *getEmbedInstanceForWidget(QWidget*);
    virtual KJS::Bindings::Instance *getObjectInstanceForWidget(QWidget*);
    virtual KJS::Bindings::Instance *getAppletInstanceForWidget(QWidget*);
    void addPluginRootObject(const KJS::Bindings::RootObject *root);
    void cleanupPluginRootObjects();
    
    virtual void registerCommandForUndo(const EditCommandPtr &);
    virtual void registerCommandForRedo(const EditCommandPtr &);
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
    virtual bool shouldBeginEditing(const RangeImpl *) const;
    virtual bool shouldEndEditing(const RangeImpl *) const;
    virtual void didBeginEditing() const;
    virtual void didEndEditing() const;

    KJS::Bindings::RootObject *executionContextForDOM();
    KJS::Bindings::RootObject *bindingRootObject();
    
    WebScriptObject *windowScriptObject();
    NPObject *windowScriptNPObject();
    
    virtual void partClearedInBegin();
    
    // Implementation of CSS property -khtml-user-drag == auto
    bool shouldDragAutoNode(NodeImpl*, int x, int y) const;

    void setMarkedTextRange(const RangeImpl *, NSArray *attributes, NSArray *ranges);
    virtual RangeImpl *markedTextRange() const { return m_markedTextRange.get(); }

    virtual bool canGoBackOrForward(int distance) const;

    void didFirstLayout();
    
    NSMutableDictionary *dashboardRegionsDictionary();
    void dashboardRegionsChanged();
    
    virtual bool isCharacterSmartReplaceExempt(const QChar &, bool);
    
    virtual bool mouseDownMayStartSelect() const { return _mouseDownMayStartSelect; }
    
    virtual void handledOnloadEvents();

protected:
    virtual DOMString generateFrameName();

    virtual void startRedirectionTimer();
    virtual void stopRedirectionTimer();
    virtual void redirectionTimerFired(Timer<Frame>*);

private:
    virtual void khtmlMousePressEvent(MousePressEvent *);
    virtual void khtmlMouseMoveEvent(MouseMoveEvent *);
    virtual void khtmlMouseReleaseEvent(MouseReleaseEvent *);
    
    NSView *mouseDownViewIfStillGood();

    NSView *nextKeyViewInFrame(NodeImpl *startingPoint, KWQSelectionDirection);
    static NSView *documentViewForNode(NodeImpl *);
    
    bool dispatchCPPEvent(const AtomicString &eventType, KWQClipboard::AccessPolicy policy);

    NSImage *imageFromRect(NSRect rect) const;

    void freeClipboard();

    void registerCommandForUndoOrRedo(const EditCommandPtr &cmd, bool isRedo);

    virtual void detachFromView();

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
    
    HashSet<RefPtr<DOMStringImpl> > urlsBridgeKnowsAbout;

    friend class Frame;

    KJS::Bindings::RootObject *_bindingRoot;  // The root object used for objects
                                            // bound outside the context of a plugin.
    QPtrList<KJS::Bindings::RootObject> rootObjects;
    WebScriptObject *_windowScriptObject;
    NPObject *_windowScriptNPObject;
    
    RefPtr<NodeImpl> _dragSrc;     // element that may be a drag source, for the current mouse gesture
    bool _dragSrcIsLink;
    bool _dragSrcIsImage;
    bool _dragSrcInSelection;
    bool _dragSrcMayBeDHTML, _dragSrcMayBeUA;   // Are DHTML and/or the UserAgent allowed to drag out?
    bool _dragSrcIsDHTML;
    KWQClipboard *_dragClipboard;   // used on only the source side of dragging
    
    RefPtr<RangeImpl> m_markedTextRange;
};

inline MacFrame *Mac(Frame *frame) { return static_cast<MacFrame *>(frame); }
inline const MacFrame *Mac(const Frame *frame) { return static_cast<const MacFrame *>(frame); }

}

#endif
