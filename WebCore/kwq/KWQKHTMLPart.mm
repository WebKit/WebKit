/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#import "KWQKHTMLPart.h"

#import "htmltokenizer.h"
#import "html_documentimpl.h"
#import "render_root.h"
#import "render_frames.h"
#import "render_text.h"
#import "khtmlpart_p.h"
#import "khtmlview.h"
#import "kjs_window.h"

#import "WebCoreBridge.h"
#import "WebCoreViewFactory.h"

#import "KWQDummyView.h"
#import "KWQKJobClasses.h"
#import "KWQLogging.h"

#import "xml/dom2_eventsimpl.h"

#import <JavaScriptCore/property_map.h>

#undef _KWQ_TIMING

using DOM::DocumentImpl;
using DOM::DOMString;
using DOM::EventImpl;
using DOM::Node;

using khtml::Cache;
using khtml::ChildFrame;
using khtml::Decoder;
using khtml::MouseDoubleClickEvent;
using khtml::MousePressEvent;
using khtml::MouseReleaseEvent;
using khtml::RenderObject;
using khtml::RenderPart;
using khtml::RenderText;
using khtml::RenderWidget;

using KIO::Job;

using KJS::SavedProperties;
using KJS::Window;

using KParts::ReadOnlyPart;
using KParts::URLArgs;

NSEvent *KWQKHTMLPart::_currentEvent = nil;

void KHTMLPart::completed()
{
    KWQ(this)->_completed.call();
}

void KHTMLPart::completed(bool arg)
{
    KWQ(this)->_completed.call(arg);
}

void KHTMLPart::nodeActivated(const Node &)
{
}

bool KHTMLPart::openURL(const KURL &URL)
{
    ASSERT_NOT_REACHED();
    return true;
}

void KHTMLPart::onURL(const QString &)
{
}

void KHTMLPart::setStatusBarText(const QString &status)
{
    KWQ(this)->setStatusBarText(status);
}

void KHTMLPart::started(Job *j)
{
    KWQ(this)->_started.call(j);
}

static void redirectionTimerMonitor(void *context)
{
    KWQKHTMLPart *kwq = static_cast<KWQKHTMLPart *>(context);
    kwq->redirectionTimerStartedOrStopped();
}

KWQKHTMLPart::KWQKHTMLPart()
    : _started(this, SIGNAL(started(KIO::Job *)))
    , _completed(this, SIGNAL(completed()))
    , _completedWithBool(this, SIGNAL(completed(bool)))
    , _ownsView(false)
    , _mouseDownView(nil)
    , _sendingEventToSubview(false)
{
    // Must init the cache before connecting to any signals
    Cache::init();

    // The widget is made outside this class in our case.
    KHTMLPart::init( 0, DefaultGUI );

    mutableInstances().prepend(this);
    d->m_redirectionTimer.setMonitor(redirectionTimerMonitor, this);
}

KWQKHTMLPart::~KWQKHTMLPart()
{
    mutableInstances().remove(this);
    if (_ownsView) {
        delete d->m_view;
    }
}

WebCoreBridge *KWQKHTMLPart::bridgeForFrameName(const QString &frameName)
{
    WebCoreBridge *frame;
    if (frameName.isEmpty()) {
        // If we're the only frame in a frameset then pop the frame.
        KHTMLPart *parent = parentPart();
        frame = parent ? KWQ(parent)->_bridge : nil;
        if ([[frame childFrames] count] != 1) {
            frame = _bridge;
        }
    } else {
        frame = [_bridge findOrCreateFramedNamed:frameName.getNSString()];
    }
    
    return frame;
}

QString KWQKHTMLPart::generateFrameName()
{
    return QString::fromNSString([_bridge generateFrameName]);
}

bool KWQKHTMLPart::openURL(const KURL &url)
{
    // FIXME: The lack of args here to get the reload flag from
    // indicates a problem in how we use KHTMLPart::processObjectRequest,
    // where we are opening the URL before the args are set up.
    [_bridge loadURL:url.url().getNSString() reload:NO triggeringEvent:nil isFormSubmission:NO];
    return true;
}

void KWQKHTMLPart::openURLRequest(const KURL &url, const URLArgs &args)
{
    [bridgeForFrameName(args.frameName) loadURL:url.url().getNSString() reload:args.reload triggeringEvent:nil isFormSubmission:NO];
}

void KWQKHTMLPart::submitForm(const KURL &url, const URLArgs &args)
{
    if (!args.doPost()) {
        [bridgeForFrameName(args.frameName) loadURL:url.url().getNSString() reload:args.reload
            triggeringEvent:_currentEvent isFormSubmission:YES];
    } else {
        QString contentType = args.contentType();
        ASSERT(contentType.startsWith("Content-Type: "));
        [bridgeForFrameName(args.frameName) postWithURL:url.url().getNSString()
            data:[NSData dataWithBytes:args.postData.data() length:args.postData.size()]
            contentType:contentType.mid(14).getNSString() triggeringEvent:_currentEvent];
    }
}

void KWQKHTMLPart::slotData(NSString *encoding, bool forceEncoding, const char *bytes, int length, bool complete)
{
    if (!d->m_workingURL.isEmpty()) {
        receivedFirstData();
    }
    
    ASSERT(d->m_doc);
    ASSERT(d->m_doc->parsing());
    
    if (encoding) {
        setEncoding(QString::fromNSString(encoding), forceEncoding);
    } else {
        setEncoding(QString::null, false);
    }
    
    write(bytes, length);
}

void KWQKHTMLPart::urlSelected(const KURL &url, int button, int state, const URLArgs &args)
{
    [bridgeForFrameName(args.frameName) loadURL:url.url().getNSString() reload:args.reload
        triggeringEvent:_currentEvent isFormSubmission:NO];
}

class KWQPluginPart : public ReadOnlyPart
{
    virtual bool openURL(const KURL &) { return true; }
    virtual bool closeURL() { return true; }
};

ReadOnlyPart *KWQKHTMLPart::createPart(const ChildFrame &child, const KURL &url, const QString &mimeType)
{
    if (child.m_type == ChildFrame::Object) {
        NSMutableArray *attributesArray = [NSMutableArray arrayWithCapacity:child.m_params.count()];
        for (uint i = 0; i < child.m_params.count(); i++) {
            [attributesArray addObject:child.m_params[i].getNSString()];
        }
        
        KWQPluginPart *newPart = new KWQPluginPart;
        newPart->setWidget(new QWidget([_bridge viewForPluginWithURL:url.url().getNSString()
                                                          attributes:attributesArray
                                                             baseURL:d->m_doc->baseURL().getNSString()
                                                            MIMEType:child.m_args.serviceType.getNSString()]));
        return newPart;
    } else {
        LOG(Frames, "name %s", child.m_name.ascii());
        HTMLIFrameElementImpl *o = static_cast<HTMLIFrameElementImpl *>(child.m_frame->element());
        WebCoreBridge *childBridge = [_bridge createChildFrameNamed:child.m_name.getNSString()
                                                            withURL:url.url().getNSString()
                                                         renderPart:child.m_frame
                                                    allowsScrolling:o->scrollingMode() != QScrollView::AlwaysOff
                                                        marginWidth:o->getMarginWidth()
                                                       marginHeight:o->getMarginHeight()];
        return [childBridge part];
    }
}
    
void KWQKHTMLPart::setView(KHTMLView *view, bool weOwnIt)
{
    if (_ownsView) {
        if (!(d->m_doc && d->m_doc->inPageCache()))
            delete d->m_view;
    }
    d->m_view = view;
    setWidget(view);
    _ownsView = weOwnIt;
}

KHTMLView *KWQKHTMLPart::view() const
{
    return d->m_view;
}

void KWQKHTMLPart::setTitle(const DOMString &title)
{
    [_bridge setTitle:title.string().getNSString()];
}

void KWQKHTMLPart::setStatusBarText(const QString &status)
{
    [_bridge setStatusText:status.getNSString()];
}

void KWQKHTMLPart::scheduleClose()
{
    [[_bridge window] performSelector:@selector(close) withObject:nil afterDelay:0.0];
}

void KWQKHTMLPart::unfocusWindow()
{
    [_bridge unfocusWindow];
}

void KWQKHTMLPart::jumpToSelection()
{
    // Assumes that selection will only ever be text nodes. This is currently
    // true, but will it always be so?
    if (!d->m_selectionStart.isNull()) {
        RenderText *rt = dynamic_cast<RenderText *>(d->m_selectionStart.handle()->renderer());
        if (rt) {
            int x = 0, y = 0;
            rt->posOfChar(d->m_startOffset, x, y);
            // The -50 offset is copied from KHTMLPart::findTextNext, which sets the contents position
            // after finding a matched text string.
            d->m_view->setContentsPos(x - 50, y - 50);
        }
    }
}

void KWQKHTMLPart::redirectionTimerStartedOrStopped()
{
    if (d->m_redirectionTimer.isActive()) {
        [_bridge reportClientRedirectToURL:d->m_redirectURL.getNSString()
                                     delay:d->m_delayRedirect
                                 fireDate:[d->m_redirectionTimer.getNSTimer() fireDate]];
    } else {
        [_bridge reportClientRedirectCancelled];
    }
}

void KWQKHTMLPart::paint(QPainter *p, const QRect &rect)
{
#ifndef NDEBUG
    p->fillRect(rect.x(), rect.y(), rect.width(), rect.height(), QColor(0xFF, 0, 0));
#endif

    if (renderer()) {
        renderer()->layer()->paint(p, rect.x(), rect.y(), rect.width(), rect.height());
    } else {
        ERROR("called KWQKHTMLPart::paint with nil renderer");
    }
}

RenderObject *KWQKHTMLPart::renderer()
{
    DocumentImpl *doc = xmlDocImpl();
    return doc ? doc->renderer() : 0;
}

QString KWQKHTMLPart::userAgent() const
{
    NSString *us = [_bridge userAgentForURL:m_url.url().getNSString()];
         
    if (us)
        return QString::fromNSString(us);
    return QString();
}

NSView *KWQKHTMLPart::nextKeyViewInFrame(NodeImpl *node, KWQSelectionDirection direction)
{
    DocumentImpl *doc = xmlDocImpl();
    if (!doc) {
        return nil;
    }
    for (;;) {
        node = direction == KWQSelectingNext
            ? doc->nextFocusNode(node) : doc->previousFocusNode(node);
        if (!node) {
            return nil;
        }
        RenderWidget *renderWidget = dynamic_cast<RenderWidget *>(node->renderer());
        if (renderWidget) {
            QWidget *widget = renderWidget->widget();
            KHTMLView *childFrameWidget = dynamic_cast<KHTMLView *>(widget);
            if (childFrameWidget) {
                NSView *view = KWQ(childFrameWidget->part())->nextKeyViewInFrame(0, direction);
                if (view) {
                    return view;
                }
            } else if (widget) {
                NSView *view = widget->getView();
                // AppKit won't be able to handle scrolling and making us the first responder
                // well unless we are actually installed in the correct place. KHTML only does
                // that for visible widgets, so we need to do it explicitly here.
                int x, y;
                if (view && renderWidget->absolutePosition(x, y)) {
                    renderWidget->view()->addChild(widget, x, y);
                    return view;
                }
            }
        }
    }
}

NSView *KWQKHTMLPart::nextKeyViewInFrameHierarchy(NodeImpl *node, KWQSelectionDirection direction)
{
    NSView *next = nextKeyViewInFrame(node, direction);
    if (next) {
        return next;
    }
    
    KHTMLPart *parent = parentPart();
    if (parent) {
        next = KWQ(parent)->nextKeyView(parent->frame(this)->m_frame->element(), direction);
        if (next) {
            return next;
        }
    }
    
    return nil;
}

NSView *KWQKHTMLPart::nextKeyView(NodeImpl *node, KWQSelectionDirection direction)
{
    NSView *next = nextKeyViewInFrameHierarchy(node, direction);
    if (next) {
        return next;
    }

    // Look at views from the top level part up, looking for a next key view that we can use.
    next = direction == KWQSelectingNext
        ? [_bridge nextKeyViewOutsideWebViews]
        : [_bridge previousKeyViewOutsideWebViews];
    if (next) {
        return next;
    }
    
    // If all else fails, make a loop by starting from 0.
    return nextKeyViewInFrameHierarchy(0, direction);
}

NSView *KWQKHTMLPart::nextKeyViewForWidget(QWidget *startingWidget, KWQSelectionDirection direction)
{
    // Use the event filter object to figure out which RenderWidget owns this QWidget and get to the DOM.
    // Then get the next key view in the order determined by the DOM.
    NodeImpl *node = nodeForWidget(startingWidget);
    return partForNode(node)->nextKeyView(node, direction);
}

bool KWQKHTMLPart::canCachePage()
{
    // Only save page state if:
    // 1.  We're not a frame or frameset.
    // 2.  The page has no javascript timers.
    // 3.  The page has no unload handler.
    // 4.  The page has no plugins.
    if (d->m_doc &&
        (d->m_frames.count() ||
        parentPart() ||
        d->m_objects.count() ||
        d->m_doc->getWindowEventListener (EventImpl::UNLOAD_EVENT) ||
        (d->m_jscript && Window::retrieveWindow(this)->hasTimeouts()))){
        return false;
    }
    return true;
}

void KWQKHTMLPart::saveWindowProperties(SavedProperties *windowProperties)
{
    Window::retrieveWindow(this)->saveProperties(*windowProperties);
}

void KWQKHTMLPart::saveLocationProperties(SavedProperties *locationProperties)
{
    Window::retrieveWindow(this)->location()->saveProperties(*locationProperties);
}

void KWQKHTMLPart::restoreWindowProperties(SavedProperties *windowProperties)
{
    Window::retrieveWindow(this)->restoreProperties(*windowProperties);
}

void KWQKHTMLPart::restoreLocationProperties(SavedProperties *locationProperties)
{
    Window::retrieveWindow(this)->location()->restoreProperties(*locationProperties);
}

void KWQKHTMLPart::openURLFromPageCache(DocumentImpl *doc, RenderObject *renderer, KURL *url, SavedProperties *windowProperties, SavedProperties *locationProperties)
{
    d->m_redirectionTimer.stop();

    // We still have to close the previous part page.
    if (!d->m_restored){
        closeURL();
    }
            
    d->m_bComplete = false;
    
    // Don't re-emit the load event.
    d->m_bLoadEventEmitted = true;
    
    // delete old status bar msg's from kjs (if it _was_ activated on last URL)
    if( d->m_bJScriptEnabled )
    {
        d->m_kjsStatusBarText = QString::null;
        d->m_kjsDefaultStatusBarText = QString::null;
    }

    m_url = *url;
    
    // set the javascript flags according to the current url
    d->m_bJScriptEnabled = KHTMLFactory::defaultHTMLSettings()->isJavaScriptEnabled(m_url.host());
    d->m_bJScriptDebugEnabled = KHTMLFactory::defaultHTMLSettings()->isJavaScriptDebugEnabled();
    d->m_bJavaEnabled = KHTMLFactory::defaultHTMLSettings()->isJavaEnabled(m_url.host());
    d->m_bPluginsEnabled = KHTMLFactory::defaultHTMLSettings()->isPluginsEnabled(m_url.host());
    
    // initializing m_url to the new url breaks relative links when opening such a link after this call and _before_ begin() is called (when the first
    // data arrives) (Simon)
    if(m_url.protocol().startsWith( "http" ) && !m_url.host().isEmpty() && m_url.path().isEmpty()) {
        m_url.setPath("/");
        emit d->m_extension->setLocationBarURL( m_url.prettyURL() );
    }
    
    // copy to m_workingURL after fixing m_url above
    d->m_workingURL = m_url;
        
    emit started( 0L );
    
    // -----------begin-----------
    clear();

    doc->restoreRenderer(renderer);
    
    d->m_bCleared = false;
    d->m_cacheId = 0;
    d->m_bComplete = false;
    d->m_bLoadEventEmitted = false;
    d->m_referrer = m_url.url();
    
    d->m_doc = doc;
    d->m_doc->ref();

    setView (doc->view(), true);
    
    updatePolicyBaseURL();
        
    restoreWindowProperties (windowProperties);
    restoreLocationProperties (locationProperties);
}

WebCoreBridge *KWQKHTMLPart::bridgeForWidget(QWidget *widget)
{
    return partForNode(nodeForWidget(widget))->bridge();
}

KWQKHTMLPart *KWQKHTMLPart::partForNode(NodeImpl *node)
{
    return KWQ(node->getDocument()->view()->part());
}

NodeImpl *KWQKHTMLPart::nodeForWidget(QWidget *widget)
{
    return static_cast<const RenderWidget *>(widget->eventFilterObject())->element();
}

void KWQKHTMLPart::setDocumentFocus(QWidget *widget)
{
    NodeImpl *node = nodeForWidget(widget);
    node->getDocument()->setFocusNode(node);
}

void KWQKHTMLPart::clearDocumentFocus(QWidget *widget)
{
    nodeForWidget(widget)->getDocument()->setFocusNode(0);
}

void KWQKHTMLPart::saveDocumentState()
{
    [_bridge saveDocumentState];
}

void KWQKHTMLPart::restoreDocumentState()
{
    [_bridge restoreDocumentState];
}

QPtrList<KWQKHTMLPart> &KWQKHTMLPart::mutableInstances()
{
    static QPtrList<KWQKHTMLPart> instancesList;
    return instancesList;
}

void KWQKHTMLPart::updatePolicyBaseURL()
{
    if (parentPart()) {
        setPolicyBaseURL(parentPart()->docImpl()->policyBaseURL());
    } else {
        setPolicyBaseURL(m_url.url());
    }
}

void KWQKHTMLPart::setPolicyBaseURL(const DOMString &s)
{
    // XML documents will cause this to return null.  docImpl() is
    // an HTMLdocument only. -dwh
    if (docImpl())
        docImpl()->setPolicyBaseURL(s);
    ConstFrameIt end = d->m_frames.end();
    for (ConstFrameIt it = d->m_frames.begin(); it != end; ++it) {
        ReadOnlyPart *subpart = (*it).m_part;
        static_cast<KWQKHTMLPart *>(subpart)->setPolicyBaseURL(s);
    }
}

QString KWQKHTMLPart::requestedURLString() const
{
    return QString::fromNSString([_bridge requestedURL]);
}

void KWQKHTMLPart::forceLayout()
{
    KHTMLView *v = d->m_view;
    if (v) {
        v->layout();
        // We cannot unschedule a pending relayout, since the force can be called with
        // a tiny rectangle from a drawRect update.  By unscheduling we in effect
        // "validate" and stop the necessary full repaint from occurring.  Basically any basic
        // append/remove DHTML is broken by this call.  For now, I have removed the optimization
        // until we have a better invalidation stategy. -dwh
        //v->unscheduleRelayout();
    }
}

QString KWQKHTMLPart::referrer() const
{
    return d->m_referrer;
}

void KWQKHTMLPart::runJavaScriptAlert(const QString &message)
{
    [[WebCoreViewFactory sharedFactory] runJavaScriptAlertPanelWithMessage:message.getNSString()];
}

bool KWQKHTMLPart::runJavaScriptConfirm(const QString &message)
{
    return [[WebCoreViewFactory sharedFactory] runJavaScriptConfirmPanelWithMessage:message.getNSString()];
}

bool KWQKHTMLPart::runJavaScriptPrompt(const QString &prompt, const QString &defaultValue, QString &result)
{
    NSString *returnedText;
    bool ok = [[WebCoreViewFactory sharedFactory] runJavaScriptTextInputPanelWithPrompt:prompt.getNSString()
        defaultText:defaultValue.getNSString() returningText:&returnedText];
    if (ok)
        result = QString::fromNSString(returnedText);
    return ok;
}

void KWQKHTMLPart::createDummyDocument()
{
    if (d->m_doc) {
        ASSERT(d->m_view);
    } else {
        d->m_doc = DOMImplementationImpl::instance()->createHTMLDocument(d->m_view);
        d->m_doc->ref();
        
        ASSERT(d->m_view == 0);
        KHTMLView *kview = new KHTMLView(this, 0);
        setView(kview, true);
        
        NSView *view = [[KWQDummyView alloc] initWithWindow:[_bridge window]];
        kview->setView(view);
        [view release];
    }
}

void KWQKHTMLPart::addMetaData(const QString &key, const QString &value)
{
    d->m_job->addMetaData(key, value);
}

bool KWQKHTMLPart::keyEvent(NSEvent *event)
{
    ASSERT([event type] == NSKeyDown || [event type] == NSKeyUp);

    // Check for cases where we are too early for events -- possible unmatched key up
    // from pressing return in the location bar.
    DocumentImpl *doc = xmlDocImpl();
    if (!doc) {
        return false;
    }
    NodeImpl *node = doc->focusNode();
    if (!node) {
	return false;
    }
    
    NSEvent *oldCurrentEvent = _currentEvent;
    _currentEvent = event;

    const char *characters = [[event characters] lossyCString];
    int ascii = (characters != nil && strlen(characters) == 1) ? characters[0] : 0;

    QKeyEvent qEvent([event type] == NSKeyDown ? QEvent::KeyPress : QEvent::KeyRelease,
		     [event keyCode],
		     ascii,
		     stateForCurrentEvent(),
		     QString::fromNSString([event characters]),
		     [event isARepeat]);

    bool result = node->dispatchKeyEvent(&qEvent);

    // We want to send both a down and a press for the initial key event
    if (![event isARepeat]) {
	QKeyEvent qEvent([event type] == NSKeyDown ? QEvent::KeyPress : QEvent::KeyRelease,
			 [event keyCode],
			 ascii,
			 stateForCurrentEvent(),
			 QString::fromNSString([event characters]),
			 true);
	
	result = result && node->dispatchKeyEvent(&qEvent);
    }

    _currentEvent = oldCurrentEvent;

    return result;
}

// This does the same kind of work that KHTMLPart::openURL does, except it relies on the fact
// that a higher level already checked that the URLs match and the scrolling is the right thing to do.
void KWQKHTMLPart::scrollToAnchor(const KURL &URL)
{
    m_url = URL;
    started(0);

    if (!gotoAnchor(URL.encodedHtmlRef()))
        gotoAnchor(URL.htmlRef());

    d->m_bComplete = true;
    d->m_doc->setParsing(false);

    completed();
}

bool KWQKHTMLPart::closeURL()
{
    saveDocumentState();
    return KHTMLPart::closeURL();
}

void KWQKHTMLPart::khtmlMousePressEvent(MousePressEvent *event)
{
    if (!passWidgetMouseDownEventToWidget(event)) {
        // We don't do this at the start of mouse down handling, because we don't want to do it until
        // we know we didn't hit a widget.
        NSView *documentView = view()->getDocumentView();
        [[documentView window] makeFirstResponder:documentView];
        
        KHTMLPart::khtmlMousePressEvent(event);
    }
}

void KWQKHTMLPart::khtmlMouseDoubleClickEvent(MouseDoubleClickEvent *event)
{
    if (!passWidgetMouseDownEventToWidget(event)) {
        KHTMLPart::khtmlMouseDoubleClickEvent(event);
    }
}

bool KWQKHTMLPart::passWidgetMouseDownEventToWidget(khtml::MouseEvent *event)
{
    _mouseDownView = nil;
    
    // Figure out which view to send the event to.
    RenderObject *target = event->innerNode().handle()->renderer();
    if (!target || !target->isWidget()) {
        return false;
    }
    return passWidgetMouseDownEventToWidget(static_cast<RenderWidget *>(target));
}

bool KWQKHTMLPart::passWidgetMouseDownEventToWidget(RenderWidget *renderWidget)
{
    _mouseDownView = nil;
    
    NSView *nodeView = renderWidget->widget()->getView();
    ASSERT(nodeView);
    ASSERT([nodeView superview]);
    NSView *topView = nodeView;
    NSView *superview;
    while ((superview = [topView superview])) {
        topView = superview;
    }
    NSView *view = [nodeView hitTest:[[nodeView superview] convertPoint:[_currentEvent locationInWindow] fromView:topView]];
    if (view == nil) {
        ERROR("KHTML says we hit a RenderWidget, but AppKit doesn't agree we hit the corresponding NSView");
        return false;
    }
    
    ASSERT(!_sendingEventToSubview);
    _sendingEventToSubview = true;
    [view mouseDown:_currentEvent];
    _sendingEventToSubview = false;
    
    // Remember which view we sent the event to, so we can direct the release event properly.
    _mouseDownView = view;
    _mouseDownWasInSubframe = false;
    
    return true;
}

void KWQKHTMLPart::khtmlMouseReleaseEvent(MouseReleaseEvent *event)
{
    if (!_mouseDownView) {
        KHTMLPart::khtmlMouseReleaseEvent(event);
        return;
    }
    
    _sendingEventToSubview = true;
    [_mouseDownView mouseUp:_currentEvent];
    _sendingEventToSubview = false;
    _mouseDownView = nil;
}

void KWQKHTMLPart::widgetWillReleaseView(NSView *view)
{
    if (view == nil) {
        return;
    }
    for (QPtrListIterator<KWQKHTMLPart> it(instances()); it.current(); ++it) {
        if ([it.current()->_mouseDownView isDescendantOf:view]) {
            it.current()->_mouseDownView = nil;
        }
    }
}

void KWQKHTMLPart::clearTimers()
{
    KHTMLView *v = d->m_view;
    if (v) {
        v->unscheduleRelayout();
        v->unscheduleRepaint();
    }
}

bool KWQKHTMLPart::passSubframeEventToSubframe(DOM::NodeImpl::MouseEvent &event)
{
    switch ([_currentEvent type]) {
    	case NSLeftMouseDown: {
            NodeImpl *node = event.innerNode.handle();
            if (!node) {
                return false;
            }
            RenderPart *renderPart = dynamic_cast<RenderPart *>(node->renderer());
            if (!renderPart) {
                return false;
            }
            if (!passWidgetMouseDownEventToWidget(renderPart)) {
                return false;
            }
            _mouseDownWasInSubframe = true;
            return true;
        }
        case NSLeftMouseUp:
            if (!(_mouseDownView && _mouseDownWasInSubframe)) {
                return false;
            }
            ASSERT(!_sendingEventToSubview);
            _sendingEventToSubview = true;
            [_mouseDownView mouseUp:_currentEvent];
            _sendingEventToSubview = false;
            _mouseDownView = nil;
            return true;
        case NSLeftMouseDragged:
            if (!(_mouseDownView && _mouseDownWasInSubframe)) {
                return false;
            }
            ASSERT(!_sendingEventToSubview);
            _sendingEventToSubview = true;
            [_mouseDownView mouseDragged:_currentEvent];
            _sendingEventToSubview = false;
            return true;
        default:
            return false;
    }
}

int KWQKHTMLPart::buttonForCurrentEvent()
{
    switch ([_currentEvent type]) {
    case NSLeftMouseDown:
    case NSLeftMouseUp:
        return Qt::LeftButton;
    case NSRightMouseDown:
    case NSRightMouseUp:
        return Qt::RightButton;
    case NSOtherMouseDown:
    case NSOtherMouseUp:
        return Qt::MidButton;
    default:
        return 0;
    }
}

int KWQKHTMLPart::stateForCurrentEvent()
{
    int state = buttonForCurrentEvent();
    
    unsigned modifiers = [_currentEvent modifierFlags];

    if (modifiers & NSControlKeyMask)
        state |= Qt::ControlButton;
    if (modifiers & NSShiftKeyMask)
        state |= Qt::ShiftButton;
    if (modifiers & NSAlternateKeyMask)
        state |= Qt::AltButton;
    if (modifiers & NSCommandKeyMask)
        state |= Qt::MetaButton;
    
    return state;
}

void KWQKHTMLPart::mouseDown(NSEvent *event)
{
    if (!d->m_view || _sendingEventToSubview) {
        return;
    }

    NSEvent *oldCurrentEvent = _currentEvent;
    _currentEvent = event;

    QMouseEvent kEvent(QEvent::MouseButtonPress, QPoint([event locationInWindow]),
        buttonForCurrentEvent(), stateForCurrentEvent(), [event clickCount]);
    d->m_view->viewportMousePressEvent(&kEvent);
    
    _currentEvent = oldCurrentEvent;
}

void KWQKHTMLPart::mouseDragged(NSEvent *event)
{
    if (!d->m_view || _sendingEventToSubview) {
        return;
    }

    NSEvent *oldCurrentEvent = _currentEvent;
    _currentEvent = event;

    QMouseEvent kEvent(QEvent::MouseMove, QPoint([event locationInWindow]), Qt::LeftButton, Qt::LeftButton);
    d->m_view->viewportMouseMoveEvent(&kEvent);
    
    _currentEvent = oldCurrentEvent;
}

void KWQKHTMLPart::mouseUp(NSEvent *event)
{
    if (!d->m_view || _sendingEventToSubview) {
        return;
    }
    
    NSEvent *oldCurrentEvent = _currentEvent;
    _currentEvent = event;

    // Our behavior here is a little different that Qt. Qt always sends
    // a mouse release event, even for a double click. To correct problems
    // in khtml's DOM click event handling we do not send a release here
    // for a double click. Instead we send that event from KHTMLView's
    // viewportMouseDoubleClickEvent.
    int clickCount = [event clickCount];
    if (clickCount > 0 && clickCount % 2 == 0) {
        QMouseEvent doubleClickEvent(QEvent::MouseButtonDblClick, QPoint([event locationInWindow]),
            buttonForCurrentEvent(), stateForCurrentEvent(), clickCount);
        d->m_view->viewportMouseDoubleClickEvent(&doubleClickEvent);
    } else {
        QMouseEvent releaseEvent(QEvent::MouseButtonRelease, QPoint([event locationInWindow]),
            buttonForCurrentEvent(), stateForCurrentEvent(), clickCount);
        d->m_view->viewportMouseReleaseEvent(&releaseEvent);
    }
    
    _currentEvent = oldCurrentEvent;
}

void KWQKHTMLPart::mouseMoved(NSEvent *event)
{
    if (!d->m_view) {
        return;
    }
    
    NSEvent *oldCurrentEvent = _currentEvent;
    _currentEvent = event;
    
    QMouseEvent kEvent(QEvent::MouseMove, QPoint([event locationInWindow]), 0, stateForCurrentEvent());
    d->m_view->viewportMouseMoveEvent(&kEvent);
    
    _currentEvent = oldCurrentEvent;
}
