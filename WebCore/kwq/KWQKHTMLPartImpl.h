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

#ifndef KWQKHTMLPARTIMPL_H
#define KWQKHTMLPARTIMPL_H

#include <qobject.h>
#include <kurl.h>

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
#else
class NSView;
class WebCoreBridge;
#endif

enum KWQSelectionDirection {
    KWQSelectingNext,
    KWQSelectingPrevious
};

class KWQKHTMLPartImpl : public QObject
{
public:
    KWQKHTMLPartImpl(KHTMLPart *);
    ~KWQKHTMLPartImpl();
    
    void setBridge(WebCoreBridge *p) { _bridge = p; }
    WebCoreBridge *bridge() const { return _bridge; }
    void setView(KHTMLView *view);
    KHTMLView *view() const;

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

    void overURL(const QString &url, const QString &target, int modifierState);
    
    void layout();
    
    QString userAgent() const;
    
    NSView *nextKeyView(DOM::NodeImpl *startingPoint, KWQSelectionDirection);
    NSView *nextKeyViewInFrameHierarchy(DOM::NodeImpl *startingPoint, KWQSelectionDirection);
    static NSView *nextKeyViewForWidget(QWidget *startingPoint, KWQSelectionDirection);
    
    static WebCoreBridge *bridgeForWidget(QWidget *);
    
    // Incoming calls, used by the bridge.
    
    DOM::DocumentImpl *document();
    khtml::RenderObject *renderer();

    // Used internally, but need to be public because they are used by non-member functions.

    void redirectionTimerStartedOrStopped();
    
    static NSString *referrer(const KParts::URLArgs &);
    
    static const QPtrList<KWQKHTMLPartImpl> &instances() { return mutableInstances(); }

private:
    WebCoreBridge *bridgeForFrameName(const QString &frameName);

    NSView *nextKeyViewInFrame(DOM::NodeImpl *startingPoint, KWQSelectionDirection);
    static DOM::NodeImpl *nodeForWidget(QWidget *);
    static KWQKHTMLPartImpl *partForNode(DOM::NodeImpl *);
    
    KHTMLPart *part;
    KHTMLPartPrivate *d;
    
    WebCoreBridge *_bridge;

    static QPtrList<KWQKHTMLPartImpl> &mutableInstances();

    friend class KHTMLPart;
};

#endif
