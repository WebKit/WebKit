/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#ifndef KWQKHTMLPart_H
#define KWQKHTMLPart_H

#include "khtml_part.h"

#include "dom_nodeimpl.h"
#include "html_formimpl.h"
#include "html_tableimpl.h"

#import "WebCoreKeyboardAccess.h"

#include <CoreFoundation/CoreFoundation.h>

#include <JavaVM/jni.h>
#include <JavaScriptCore/jni_jsobject.h>
#include <JavaScriptCore/runtime.h>

#include "KWQDict.h"

class KHTMLPartPrivate;
class KWQWindowWidget;

namespace khtml {
    class RenderObject;
}

namespace KJS {
    class SavedProperties;
    class SavedBuiltins;
    class ScheduledAction;
}

#ifdef __OBJC__
@class NSAttributedString;
@class NSEvent;
@class NSFileWrapper;
@class NSResponder;
@class NSView;
@class WebCoreBridge;
@class KWQPageState;
@class NSString;
@class NSArray;
@class NSMutableDictionary;
@class WebCoreDOMElement;
@class NSColor;
#else
class NSAttributedString;
class NSEvent;
class NSFileWrapper;
class NSResponder;
class NSView;
class WebCoreBridge;
class KWQPageState;
class NSString;
class NSArray;
class NSMutableDictionary;
class WebCoreDOMElement;
class NSColor;
#endif

enum KWQSelectionDirection {
    KWQSelectingNext,
    KWQSelectingPrevious
};

class KWQKHTMLPart : public KHTMLPart
{
public:
    KWQKHTMLPart();
    ~KWQKHTMLPart();
    
    void clear();

    void setBridge(WebCoreBridge *p);
    WebCoreBridge *bridge() const { return _bridge; }
    void setView(KHTMLView *view);
    KHTMLView *view() const;

    void provisionalLoadStarted();

    virtual bool openURL(const KURL &);
    virtual bool closeURL();
    void didNotOpenURL(const KURL &);
    
    void openURLRequest(const KURL &, const KParts::URLArgs &);
    void submitForm(const KURL &, const KParts::URLArgs &);

    void scheduleHistoryNavigation( int steps );
    
    void scrollToAnchor(const KURL &);
    void jumpToSelection();
    
    void setEncoding(const QString &encoding, bool userChosen);
    void addData(const char *bytes, int length);

    void setTitle(const DOM::DOMString &);
    void setStatusBarText(const QString &status);

    void urlSelected(const KURL &url, int button, int state, const KParts::URLArgs &args);
    bool requestObject(khtml::RenderPart *frame, const QString &url, const QString &serviceType, const QStringList &args);
    KParts::ReadOnlyPart *createPart(const khtml::ChildFrame &child, const KURL &url, const QString &mimeType);

    void scheduleClose();

    void unfocusWindow();

    QMap<int, KJS::ScheduledAction*> *pauseActions(const void *key);
    void resumeActions(QMap<int, KJS::ScheduledAction*> *actions, const void *key);
    
    bool canCachePage();
    void saveWindowProperties(KJS::SavedProperties *windowProperties);
    void saveLocationProperties(KJS::SavedProperties *locationProperties);
    void restoreWindowProperties(KJS::SavedProperties *windowProperties);
    void restoreLocationProperties(KJS::SavedProperties *locationProperties);
    void saveInterpreterBuiltins(KJS::SavedBuiltins &interpreterBuiltins);
    void restoreInterpreterBuiltins(const KJS::SavedBuiltins &interpreterBuiltins);

    void openURLFromPageCache(KWQPageState *state);

    void saveDocumentState();
    void restoreDocumentState();
    
    bool isFrameSet() const;

    void updatePolicyBaseURL();

    NSView *nextKeyView(DOM::NodeImpl *startingPoint, KWQSelectionDirection);
    NSView *nextKeyViewInFrameHierarchy(DOM::NodeImpl *startingPoint, KWQSelectionDirection);
    static NSView *nextKeyViewForWidget(QWidget *startingPoint, KWQSelectionDirection);
    static bool currentEventIsKeyboardOptionTab();
    static bool handleKeyboardOptionTabInView(NSView *view);
    
    virtual bool tabsToLinks() const;
    virtual bool tabsToAllControls() const;
    
    static bool currentEventIsMouseDownInWidget(QWidget *candidate);
    
    static void setDocumentFocus(QWidget *);
    static void clearDocumentFocus(QWidget *);
    
    void runJavaScriptAlert(const QString &message);
    bool runJavaScriptConfirm(const QString &message);
    bool runJavaScriptPrompt(const QString &message, const QString &defaultValue, QString &result);

    using KHTMLPart::xmlDocImpl;
    khtml::RenderObject *renderer();
    void forceLayout();
    void forceLayoutWithPageWidthRange(float minPageWidth, float maxPageWidth);
    void sendResizeEvent();
    void paint(QPainter *, const QRect &);
    void paintSelectionOnly(QPainter *p, const QRect &rect);

    void adjustPageHeight(float *newBottom, float oldTop, float oldBottom, float bottomLimit);

    void createEmptyDocument();

    static WebCoreBridge *bridgeForWidget(const QWidget *);
    
    QString requestedURLString() const;
    QString incomingReferrer() const;
    QString userAgent() const;

    QString mimeTypeForFileName(const QString &) const;
    
    DOM::NodeImpl *selectionStart() const;
    int selectionStartOffset() const;
    DOM::NodeImpl *selectionEnd() const;
    int selectionEndOffset() const;

    QRect selectionRect() const;

    NSFileWrapper *fileWrapperForElement(DOM::ElementImpl *);
    NSAttributedString *attributedString(DOM::NodeImpl *startNode, int startOffset, DOM::NodeImpl *endNode, int endOffset);

    void addMetaData(const QString &key, const QString &value);

    void mouseDown(NSEvent *);
    void mouseDragged(NSEvent *);
    void mouseUp(NSEvent *);
    void sendFakeEventsAfterWidgetTracking(NSEvent *initiatingEvent);
    void mouseMoved(NSEvent *);
    bool keyEvent(NSEvent *);
    bool lastEventIsMouseUp();

    bool sendContextMenuEvent(NSEvent *);

    void clearTimers();
    static void clearTimers(KHTMLView *);
    
    bool passSubframeEventToSubframe(DOM::NodeImpl::MouseEvent &);
    
    void redirectionTimerStartedOrStopped();
    
    static const QPtrList<KWQKHTMLPart> &instances() { return mutableInstances(); }

    void clearRecordedFormValues();
    void recordFormValue(const QString &name, const QString &value, DOM::HTMLFormElementImpl *element);
    DOM::HTMLFormElementImpl *currentForm() const;

    NSString *searchForLabelsAboveCell(QRegExp *regExp, DOM::HTMLTableCellElementImpl *cell);
    NSString *searchForLabelsBeforeElement(NSArray *labels, DOM::ElementImpl *element);
    NSString *matchLabelsAgainstElement(NSArray *labels, DOM::ElementImpl *element);

    bool findString(NSString *str, bool forward, bool caseFlag, bool wrapFlag);

    void setSettings(KHTMLSettings *);

    KWQWindowWidget *topLevelWidget();
    
    void setMediaType(const QString &);

    void setUsesInactiveTextBackgroundColor(bool u) { _usesInactiveTextBackgroundColor = u; }
    bool usesInactiveTextBackgroundColor() const { return _usesInactiveTextBackgroundColor; }

    void setShowsFirstResponder(bool flag);
    bool showsFirstResponder() const { return _showsFirstResponder; }
    
    // Convenience, to avoid repeating the code to dig down to get this.
    QChar backslashAsCurrencySymbol() const;

    NSColor *bodyBackgroundColor() const;
    
    WebCoreKeyboardUIMode keyboardUIMode() const;

    void setName(const QString &name);

    void didTellBridgeAboutLoad(const QString &urlString);
    bool haveToldBridgeAboutLoad(const QString &urlString);
    void print();

    KJS::Bindings::Instance *getAppletInstanceForView (NSView *aView);
    void addPluginRootObject(const KJS::Bindings::RootObject *root);
    void cleanupPluginRootObjects();
    
private:
    virtual void khtmlMousePressEvent(khtml::MousePressEvent *);
    virtual void khtmlMouseDoubleClickEvent(khtml::MouseDoubleClickEvent *);
    virtual void khtmlMouseMoveEvent(khtml::MouseMoveEvent *);
    virtual void khtmlMouseReleaseEvent(khtml::MouseReleaseEvent *);
    
    bool passWidgetMouseDownEventToWidget(khtml::MouseEvent *);
    bool passWidgetMouseDownEventToWidget(khtml::RenderWidget *);
    bool passWidgetMouseDownEventToWidget(QWidget *);
    
    void setPolicyBaseURL(const DOM::DOMString &);
    
    NSView *mouseDownViewIfStillGood();

    QString generateFrameName();

    NSView *nextKeyViewInFrame(DOM::NodeImpl *startingPoint, KWQSelectionDirection);
    static DOM::NodeImpl *nodeForWidget(const QWidget *);
    static KWQKHTMLPart *partForNode(DOM::NodeImpl *);
    static NSView *documentViewForNode(DOM::NodeImpl *);
    
    WebCoreBridge *_bridge;
    
    KWQSignal _started;
    KWQSignal _completed;
    KWQSignal _completedWithBool;
    
    NSView *_mouseDownView;
    bool _mouseDownWasInSubframe;
    bool _sendingEventToSubview;
    bool _mouseDownMayStartDrag;
    bool _mouseDownMayStartSelect;
    
    static NSEvent *_currentEvent;
    static NSResponder *_firstResponderAtMouseDownTime;

    KURL _submittedFormURL;

    NSMutableDictionary *_formValuesAboutToBeSubmitted;
    WebCoreDOMElement *_formAboutToBeSubmitted;

    static QPtrList<KWQKHTMLPart> &mutableInstances();

    KWQWindowWidget *_windowWidget;

    bool _usesInactiveTextBackgroundColor;
    bool _showsFirstResponder;

    QDict<char> urlsBridgeKnowsAbout;

    friend class KHTMLPart;

    QPtrList<KJS::Bindings::RootObject> rootObjects;
};

inline KWQKHTMLPart *KWQ(KHTMLPart *part) { return static_cast<KWQKHTMLPart *>(part); }
inline const KWQKHTMLPart *KWQ(const KHTMLPart *part) { return static_cast<const KWQKHTMLPart *>(part); }

#endif
