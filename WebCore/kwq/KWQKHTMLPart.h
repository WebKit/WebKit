/*
 * Copyright (C) 2002 Apple Computer, Inc.  All rights reserved.
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

#include "KWQObject.h"
#include "KWQKURL.h"
#include "KWQSignal.h"

class KHTMLPart;
class KHTMLPartPrivate;
class KHTMLView;

namespace KParts {
    class ReadOnlyPart;
    class URLArgs;
}

namespace DOM {
    class DOMString;
    class DocumentImpl;
    class NodeImpl;
}

namespace khtml {
    class ChildFrame;
    class RenderObject;
    class RenderPart;
}

#ifdef __OBJC__
@class NSView;
@class WebCoreBridge;
@class NSEvent;
#else
class NSView;
class WebCoreBridge;
class NSEvent;
#endif

enum KWQSelectionDirection {
    KWQSelectingNext,
    KWQSelectingPrevious
};

class KWQKHTMLPart : public QObject
{
public:
    KWQKHTMLPart(KHTMLPart *);
    ~KWQKHTMLPart();
    
    void setBridge(WebCoreBridge *p) { _bridge = p; }
    WebCoreBridge *bridge() const { return _bridge; }
    void setView(KHTMLView *view);
    KHTMLView *view() const;

    void openURL(const KURL &);
    void openURLRequest(const KURL &, const KParts::URLArgs &);
    
    void slotData(NSString *, bool forceEncoding, const char *bytes, int length, bool complete = false);

    void setTitle(const DOM::DOMString &);
    void setStatusBarText(const QString &status);

    void urlSelected(const KURL &url, int button, int state, const KParts::URLArgs &args);
    bool requestObject(khtml::RenderPart *frame, const QString &url, const QString &serviceType, const QStringList &args);
    KParts::ReadOnlyPart *createPart(const khtml::ChildFrame &child, const KURL &url, const QString &mimeType);

    void submitForm(const KURL &url, const KParts::URLArgs &);

    void scheduleClose();

    void unfocusWindow();

    void saveDocumentState();
    void restoreDocumentState();
    
    bool isFrameSet();

    void jumpToSelection();

    void layout();
    
    QString userAgent() const;

    void updatePolicyBaseURL();

    NSView *nextKeyView(DOM::NodeImpl *startingPoint, KWQSelectionDirection);
    NSView *nextKeyViewInFrameHierarchy(DOM::NodeImpl *startingPoint, KWQSelectionDirection);
    static NSView *nextKeyViewForWidget(QWidget *startingPoint, KWQSelectionDirection);
    
    static void setDocumentFocus(QWidget *);
    static void clearDocumentFocus(QWidget *);
    
    static void runJavaScriptAlert(const QString &message);
    static bool runJavaScriptConfirm(const QString &message);
    static bool runJavaScriptPrompt(const QString &message, const QString &defaultValue, QString &result);

    static WebCoreBridge *bridgeForWidget(QWidget *);
    
    // Incoming calls, used by the bridge.
    
    DOM::DocumentImpl *document();
    khtml::RenderObject *renderer();
    void forceLayout();
    void paint(QPainter *, const QRect &);

    void createDummyDocument();

    // Used internally, but need to be public because they are used by non-member functions.

    void redirectionTimerStartedOrStopped();
    
    QString referrer() const;
    
    static const QPtrList<KWQKHTMLPart> &instances() { return mutableInstances(); }

    QString requestedURLString() const;
    
    int selectionStartOffset() const;
    int selectionEndOffset() const;
    DOM::NodeImpl *selectionStart() const;
    DOM::NodeImpl *selectionEnd() const;

    void setCurrentEvent(NSEvent *event);
private:
    void setPolicyBaseURL(const DOM::DOMString &);

    WebCoreBridge *bridgeForFrameName(const QString &frameName);

    NSView *nextKeyViewInFrame(DOM::NodeImpl *startingPoint, KWQSelectionDirection);
    static DOM::NodeImpl *nodeForWidget(QWidget *);
    static KWQKHTMLPart *partForNode(DOM::NodeImpl *);
    
    KHTMLPart *part;
    KHTMLPartPrivate *d;
    
    WebCoreBridge *_bridge;
    
    KWQSignal _started;
    KWQSignal _completed;
    KWQSignal _completedWithBool;
    
    bool _needsToSetWidgetsAside;

    NSEvent *_currentEvent;

    static QPtrList<KWQKHTMLPart> &mutableInstances();

    friend class KHTMLPart;
};

#endif
