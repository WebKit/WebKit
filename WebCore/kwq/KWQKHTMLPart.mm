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

#import <KWQKHTMLPartImpl.h>

#import <htmltokenizer.h>
#import <html_documentimpl.h>
#import <render_root.h>
#import <render_frames.h>
#import <render_text.h>
#import <khtmlpart_p.h>
#import <khtmlview.h>

#import <WebCoreBridge.h>
#import <WebCoreViewFactory.h>

#import <KWQLogging.h>

#undef _KWQ_TIMING

using khtml::ChildFrame;
using khtml::Decoder;
using khtml::RenderObject;
using khtml::RenderPart;
using khtml::RenderText;
using khtml::RenderWidget;

using KIO::Job;

using KParts::ReadOnlyPart;
using KParts::URLArgs;

void KHTMLPart::completed()
{
    impl->_completed.call();
}

void KHTMLPart::completed(bool arg)
{
    impl->_completed.call(arg);
}

void KHTMLPart::nodeActivated(const DOM::Node &aNode)
{
}

void KHTMLPart::onURL(const QString &)
{
}

void KHTMLPart::setStatusBarText(const QString &status)
{
    impl->setStatusBarText(status);
}

void KHTMLPart::started(Job *j)
{
    impl->_started.call(j);
}

static void redirectionTimerMonitor(void *context)
{
    KWQKHTMLPartImpl *impl = static_cast<KWQKHTMLPartImpl *>(context);
    impl->redirectionTimerStartedOrStopped();
}

KWQKHTMLPartImpl::KWQKHTMLPartImpl(KHTMLPart *p)
    : part(p), d(part->d)
    , _started(p, SIGNAL(started(KIO::Job *)))
    , _completed(p, SIGNAL(completed()))
    , _completedWithBool(p, SIGNAL(completed(bool)))
    , _needsToSetWidgetsAside(false)
{
    mutableInstances().prepend(this);
    d->m_redirectionTimer.setMonitor(redirectionTimerMonitor, this);
}

KWQKHTMLPartImpl::~KWQKHTMLPartImpl()
{
    mutableInstances().remove();
}

WebCoreBridge *KWQKHTMLPartImpl::bridgeForFrameName(const QString &frameName)
{
    WebCoreBridge *frame;
    if (frameName.isEmpty()) {
        // If we're the only frame in a frameset then pop the frame.
        KHTMLPart *parentPart = part->parentPart();
        frame = parentPart ? parentPart->impl->_bridge : nil;
        if ([[frame childFrames] count] != 1) {
            frame = _bridge;
        }
    } else {
        frame = [_bridge descendantFrameNamed:frameName.getNSString()];
	if (frame == nil) {
	    frame = [_bridge frameNamed:frameName.getNSString()];
	}
        if (frame == nil) {
	    frame = [bridge() openNewWindowWithURL:nil referrer:nil frameName:frameName.getNSString()];
        }
    }
    
    return frame;
}

NSString *KWQKHTMLPartImpl::referrer(const URLArgs &args)
{
    return args.metaData()["referrer"].getNSString();
}

void KWQKHTMLPartImpl::openURLRequest(const KURL &url, const URLArgs &args)
{
    NSURL *cocoaURL = url.getNSURL();
    if (cocoaURL == nil) {
        // FIXME: We need to report this error to someone.
        return;
    }

    [bridgeForFrameName(args.frameName) loadURL:cocoaURL referrer:referrer(args)];
}

void KWQKHTMLPartImpl::slotData(NSString *encoding, bool forceEncoding, const char *bytes, int length, bool complete)
{
// NOTE: This code emulates the interface used by the original khtml part  
    if (!d->m_workingURL.isEmpty()) {
        part->begin(d->m_workingURL, 0, 0);
        d->m_workingURL = KURL();
    }

    if (encoding) {
        part->setEncoding(QString::fromNSString(encoding), forceEncoding);
    } else {
        part->setEncoding(QString::null, false);
    }
    
    ASSERT(d->m_doc != NULL);

    part->write(bytes, length);
}

void KWQKHTMLPartImpl::urlSelected(const KURL &url, int button, int state, const URLArgs &args)
{
    NSURL *cocoaURL = url.getNSURL();
    if (cocoaURL == nil) {
        // FIXME: Do we need to report an error to someone?
        return;
    }
    
    // Open new window on command-click
    if (state & MetaButton) {
        [_bridge openNewWindowWithURL:cocoaURL referrer:referrer(args) frameName:nil];
        return;
    }
    
    WebCoreBridge *targetBridge = bridgeForFrameName(args.frameName);

    // FIXME: KHTML does this in openURL -- we should do this at that level so we don't
    // have the complexity of dealing with the target here.
    KHTMLPart *targetPart = [targetBridge part];
    if (targetPart) {
        KURL refLess(url);
        targetPart->m_url.setRef("");
        refLess.setRef("");
        if (refLess.url() == targetPart->m_url.url()) {
            targetPart->m_url = url;
            targetPart->gotoAnchor(url.ref());
            // This URL needs to be added to the back/forward list.
            [targetBridge addBackForwardItemWithURL:cocoaURL anchor:url.ref().getNSString()];
            return;
        }
    }
    
    [targetBridge loadURL:cocoaURL referrer:referrer(args)];
}

class KWQPluginPart : public ReadOnlyPart
{
    virtual bool openURL(const KURL &) { return true; }
    virtual bool closeURL() { return true; }
};

ReadOnlyPart *KWQKHTMLPartImpl::createPart(const ChildFrame &child, const KURL &url, const QString &mimeType)
{
    NSURL *childURL = url.getNSURL();
    if (childURL == nil) {
        // FIXME: Do we need to report an error to someone?
        return 0;
    }
    
    if (child.m_type == ChildFrame::Object) {
        NSMutableArray *paramsArray = [NSMutableArray arrayWithCapacity:child.m_params.count()];
        for (uint i = 0; i < child.m_params.count(); i++) {
            [paramsArray addObject:child.m_params[i].getNSString()];
        }
        
        KWQPluginPart *newPart = new KWQPluginPart;
        newPart->setWidget(new QWidget([[WebCoreViewFactory sharedFactory]
            viewForPluginWithURL:childURL
                     serviceType:child.m_args.serviceType.getNSString()
                       arguments:paramsArray
                         baseURL:KURL(d->m_doc->baseURL()).getNSURL()]));
        return newPart;
    } else {
        LOG(Frames, "name %s", child.m_name.ascii());
        HTMLIFrameElementImpl *o = static_cast<HTMLIFrameElementImpl *>(child.m_frame->element());
        WebCoreBridge *childBridge = [_bridge createChildFrameNamed:child.m_name.getNSString()
            withURL:childURL referrer:referrer(child.m_args)
            renderPart:child.m_frame allowsScrolling:o->scrollingMode() != QScrollView::AlwaysOff
            marginWidth:o->getMarginWidth() marginHeight:o->getMarginHeight()];
        return [childBridge part];
    }
}
    
void KWQKHTMLPartImpl::submitForm(const KURL &u, const URLArgs &args)
{
    if (!args.doPost()) {
	[bridgeForFrameName(args.frameName) loadURL:u.getNSURL() referrer:referrer(args)];
    } else {
	NSData *postData = [NSData dataWithBytes:args.postData.data() length:args.postData.size()];
	[bridgeForFrameName(args.frameName) postWithURL:u.getNSURL() referrer:referrer(args) data:postData];
    }
}

void KWQKHTMLPartImpl::setView(KHTMLView *view)
{
    d->m_view = view;
    part->setWidget(view);
}

KHTMLView *KWQKHTMLPartImpl::view() const
{
    return d->m_view;
}

void KWQKHTMLPartImpl::setTitle(const DOMString &title)
{
    [_bridge setTitle:title.string().getNSString()];
}

void KWQKHTMLPartImpl::setStatusBarText(const QString &status)
{
    [_bridge setStatusText:status.getNSString()];
}

void KWQKHTMLPartImpl::scheduleClose()
{
    [[_bridge window] performSelector:@selector(close) withObject:nil afterDelay:0.0];
}

void KWQKHTMLPartImpl::unfocusWindow()
{
    [_bridge unfocusWindow];
}

void KWQKHTMLPartImpl::overURL(const QString &url, const QString &_target, int modifierState)
{
    // FIXME: The rules about what string does what should not be separate from the code that
    // actually implements these rules. It's particularly bad with strings.

    if (url.isEmpty()) {
        [_bridge setStatusText:@""];
        return;
    }

    int position = url.find("javascript:", 0, false);
    if (position == 0) {
        // FIXME: Is it worthwhile to special-case scripts that do a window.open and nothing else?
        const QString scriptName = url.mid(strlen("javascript:"));
        [_bridge setStatusText:[NSString stringWithFormat:@"Run script \"%@\"", scriptName.getNSString()]];
        return;
    }
    
    KURL u = part->completeURL(url);
    
    if (u.protocol() == "mailto") {
        // FIXME: Add address book integration so we show the real name instead?
        const QString address = KURL::decode_string(u.path());
        [_bridge setStatusText:[NSString stringWithFormat:@"Send email to %@", address.getNSString()]];
        return;
    }
    
    NSString *format;
    
    QString target = _target;
    if (target.isEmpty() && d->m_doc) {
        target = d->m_doc->baseTarget();
    }
    
    if (target == "_blank") {
        format = @"Open \"%@\" in a new window";
    } else {
        WebCoreBridge *targetFrame;
        if (target.isEmpty() || target != "_self" || target == "_top" || target != "_parent") {
            targetFrame = _bridge;
        } else {
            targetFrame = [_bridge frameNamed:target.getNSString()];
        }
        if (targetFrame == _bridge) {
            format = @"Go to \"%@\"";
        } else if (targetFrame == nil) {
            format = @"Open \"%@\" in a new window";
        } else if ([targetFrame mainFrame] == [_bridge mainFrame]) {
            format = @"Go to \"%@\" in another frame";
        } else {
            format = @"Go to \"%@\" in another window";
        }
    }
    
    if ([_bridge modifierTrackingEnabled]) {
        if (modifierState & MetaButton) {
            // FIXME 2935687: We are waffling about support for command-shift for open-behind,
            // so I'm commenting out this message until this is addressed.
            if (modifierState & ShiftButton && NO) {
                format = @"Open \"%@\" in a new window, behind the current window";
            } else {
                format = @"Open \"%@\" in a new window";
            }
        } else if (modifierState & AltButton) {
            format = @"Download \"%@\"";
        }
    }
    
    [_bridge setStatusText:[NSString stringWithFormat:format, u.url().getNSString()]];
}

void KWQKHTMLPartImpl::jumpToSelection()
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

void KWQKHTMLPartImpl::redirectionTimerStartedOrStopped()
{
    if (d->m_redirectionTimer.isActive()) {
        [_bridge reportClientRedirectTo:KURL(d->m_redirectURL).getNSURL()
                                  delay:d->m_delayRedirect
                               fireDate:[d->m_redirectionTimer.getNSTimer() fireDate]];
    } else {
        [_bridge reportClientRedirectCancelled];
    }
}

static void moveWidgetsAside(RenderObject *object)
{
    // Would use dynamic_cast, but a virtual function call is faster.
    if (object->isWidget()) {
        QWidget *widget = static_cast<RenderWidget *>(object)->widget();
        if (widget) {
            widget->move(999999, 0);
        }
    }
    
    for (RenderObject *child = object->firstChild(); child; child = child->nextSibling()) {
        moveWidgetsAside(child);
    }
}

void KWQKHTMLPartImpl::layout()
{
    // Since not all widgets will get a print call, it's important to move them away
    // so that they won't linger in an old position left over from a previous print.
    _needsToSetWidgetsAside = true;
}

void KWQKHTMLPartImpl::paint(QPainter *p, const QRect &rect)
{
#ifdef DEBUG_DRAWING
    [[NSColor redColor] set];
    [NSBezierPath fillRect:[view()->getView() visibleRect]];
#endif

    if (renderer()) {
        if (_needsToSetWidgetsAside) {
            moveWidgetsAside(renderer());
            _needsToSetWidgetsAside = false;
        }
        renderer()->layer()->paint(p, rect.x(), rect.y(), rect.width(), rect.height());
    }
}

DocumentImpl *KWQKHTMLPartImpl::document()
{
    return part->xmlDocImpl();
}

RenderObject *KWQKHTMLPartImpl::renderer()
{
    DocumentImpl *doc = part->xmlDocImpl();
    return doc ? doc->renderer() : 0;
}

QString KWQKHTMLPartImpl::userAgent() const
{
    return QString::fromNSString([_bridge userAgentForURL:part->m_url.getNSURL()]);
}

NSView *KWQKHTMLPartImpl::nextKeyViewInFrame(NodeImpl *node, KWQSelectionDirection direction)
{
    DocumentImpl *doc = document();
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
                NSView *view = childFrameWidget->part()->impl->nextKeyViewInFrame(0, direction);
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

NSView *KWQKHTMLPartImpl::nextKeyViewInFrameHierarchy(NodeImpl *node, KWQSelectionDirection direction)
{
    NSView *next = nextKeyViewInFrame(node, direction);
    if (next) {
        return next;
    }
    
    KHTMLPart *parentPart = part->parentPart();
    if (parentPart) {
        next = parentPart->impl->nextKeyView(parentPart->frame(part)->m_frame->element(), direction);
        if (next) {
            return next;
        }
    }
    
    return nil;
}

NSView *KWQKHTMLPartImpl::nextKeyView(NodeImpl *node, KWQSelectionDirection direction)
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

NSView *KWQKHTMLPartImpl::nextKeyViewForWidget(QWidget *startingWidget, KWQSelectionDirection direction)
{
    // Use the event filter object to figure out which RenderWidget owns this QWidget and get to the DOM.
    // Then get the next key view in the order determined by the DOM.
    NodeImpl *node = nodeForWidget(startingWidget);
    return partForNode(node)->nextKeyView(node, direction);
}

WebCoreBridge *KWQKHTMLPartImpl::bridgeForWidget(QWidget *widget)
{
    return partForNode(nodeForWidget(widget))->bridge();
}

KWQKHTMLPartImpl *KWQKHTMLPartImpl::partForNode(NodeImpl *node)
{
    return node->getDocument()->view()->part()->impl;
}

NodeImpl *KWQKHTMLPartImpl::nodeForWidget(QWidget *widget)
{
    return static_cast<const RenderWidget *>(widget->eventFilterObject())->element();
}

void KWQKHTMLPartImpl::setDocumentFocus(QWidget *widget)
{
    NodeImpl *node = nodeForWidget(widget);
    node->getDocument()->setFocusNode(node);
}

void KWQKHTMLPartImpl::clearDocumentFocus(QWidget *widget)
{
    nodeForWidget(widget)->getDocument()->setFocusNode(0);
}

void KWQKHTMLPartImpl::saveDocumentState()
{
    [_bridge saveDocumentState];
}

void KWQKHTMLPartImpl::restoreDocumentState()
{
    [_bridge restoreDocumentState];
}

QPtrList<KWQKHTMLPartImpl> &KWQKHTMLPartImpl::mutableInstances()
{
    static QPtrList<KWQKHTMLPartImpl> instancesList;
    return instancesList;
}
