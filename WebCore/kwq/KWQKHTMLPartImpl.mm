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

#import <WebFoundation/WebNSURLExtras.h>

#import <kwqdebug.h>

#undef _KWQ_TIMING

using khtml::ChildFrame;
using khtml::Decoder;
using khtml::RenderObject;
using khtml::RenderPart;
using khtml::RenderText;
using khtml::RenderWidget;

using KIO::Job;

using KParts::URLArgs;

void KHTMLPart::onURL(const QString &)
{
}

void KHTMLPart::nodeActivated(const DOM::Node &aNode)
{
}

void KHTMLPart::setStatusBarText(const QString &status)
{
    impl->setStatusBarText(status);
}

static void redirectionTimerMonitor(void *context)
{
    KWQKHTMLPartImpl *impl = static_cast<KWQKHTMLPartImpl *>(context);
    impl->redirectionTimerStartedOrStopped();
}

void KHTMLPart::started(Job *j)
{
    KWQObjectSenderScope senderScope(this);
    
    if (parentPart()) {
	parentPart()->slotChildStarted(j);
    }
}

void KHTMLPart::completed()
{
    completed(false);
}

void KHTMLPart::completed(bool arg)
{
    KWQObjectSenderScope senderScope(this);
    
    if (parentPart()) {
	parentPart()->slotChildCompleted(arg);
    }
    
    QValueList<ChildFrame>::ConstIterator it = d->m_frames.begin();
    QValueList<ChildFrame>::ConstIterator end = d->m_frames.end();
    for (; it != end; ++it ) {
        KHTMLPart *part = dynamic_cast<KHTMLPart *>((*it).m_part.pointer());
        if (part) {
            part->slotParentCompleted();
        }
    }
}

KWQKHTMLPartImpl::KWQKHTMLPartImpl(KHTMLPart *p)
    : part(p), d(part->d)
{
    d->m_redirectionTimer.setMonitor(redirectionTimerMonitor, this);
}

KWQKHTMLPartImpl::~KWQKHTMLPartImpl()
{
}

WebCoreBridge *KWQKHTMLPartImpl::getBridgeForFrameName(const QString &frameName)
{
    WebCoreBridge *frame;
    if (frameName.isEmpty()) {
        // If we're the only frame in a frameset then pop the frame.
        KHTMLPart *parentPart = part->parentPart();
        frame = parentPart ? parentPart->impl->bridge : nil;
        if ([[frame childFrames] count] != 1) {
            frame = bridge;
        }
    } else {
        frame = [bridge descendantFrameNamed:frameName.getNSString()];
        if (frame == nil) {
            NSLog (@"WARNING: unable to find frame named %@, creating new window with \"_blank\" name. New window will not be named until 2959902 is fixed.\n", frameName.getNSString());
            frame = [bridge descendantFrameNamed:@"_blank"];
        }
    }
    
    return frame;
}

void KWQKHTMLPartImpl::openURLRequest(const KURL &url, const URLArgs &args)
{
    NSURL *cocoaURL = url.getNSURL();
    if (cocoaURL == nil) {
        // FIXME: We need to report this error to someone.
        return;
    }

    [getBridgeForFrameName(args.frameName) loadURL:cocoaURL];
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
    
    KWQ_ASSERT(d->m_doc != NULL);

    part->write(bytes, length);
}

void KWQKHTMLPartImpl::urlSelected(const QString &url, int button, int state, const QString &_target, const URLArgs &args)
{
    QString target = _target;
    if (target.isEmpty() && d->m_doc) {
        target = d->m_doc->baseTarget();
    }

    if (url.find("javascript:", 0, false) == 0) {
        part->executeScript( url.right( url.length() - 11) );
        return;
    }

    KURL clickedURL(part->completeURL(url));
    NSURL *cocoaURL = clickedURL.getNSURL();
    if (cocoaURL == nil) {
        // FIXME: Do we need to report an error to someone?
        return;
    }
    
    // Open new window on command-click
    if (state & MetaButton) {
        [bridge openNewWindowWithURL:cocoaURL];
        return;
    }

    // FIXME: KHTML does this in openURL -- we should consider doing that too, because
    // this won't work when there's targeting involved.
    KURL refLess(clickedURL);
    part->m_url.setRef("");
    refLess.setRef("");
    if (refLess.url() == part->m_url.url()) {
        part->m_url = clickedURL;
        part->gotoAnchor(clickedURL.ref());
        // This URL needs to be added to the back/forward list.
        [bridge addBackForwardItemWithURL:cocoaURL anchor:clickedURL.ref().getNSString()];
        return;
    }
    
    [getBridgeForFrameName(target) loadURL:cocoaURL];
}

bool KWQKHTMLPartImpl::requestFrame( RenderPart *frame, const QString &url, const QString &frameName,
                                     const QStringList &params, bool isIFrame )
{
    KWQ_ASSERT(!frameExists(frameName));

    NSURL *childURL = part->completeURL(url).getNSURL();
    if (childURL == nil) {
        // FIXME: Do we need to report an error to someone?
        return false;
    }
    
    KWQDEBUGLEVEL(KWQ_LOG_FRAMES, "name %s\n", frameName.ascii());
    
    HTMLIFrameElementImpl *o = static_cast<HTMLIFrameElementImpl *>(frame->element());
    WebCoreBridge *childBridge = [bridge createChildFrameNamed:frameName.getNSString() withURL:childURL
				  renderPart:frame allowsScrolling:o->scrollingMode() != QScrollView::AlwaysOff
				  marginWidth:o->getMarginWidth() marginHeight:o->getMarginHeight()];

    if (!childBridge) {
        return false;
    }
    
    ChildFrame childFrame;
    childFrame.m_name = frameName;
    childFrame.m_type = isIFrame ? khtml::ChildFrame::IFrame : khtml::ChildFrame::Frame;
    childFrame.m_frame = frame;
    childFrame.m_params = params;
    childFrame.m_part = [childBridge part];
    d->m_frames.append(childFrame);

#ifdef _SUPPORT_JAVASCRIPT_URL_    
    if ( url.find( QString::fromLatin1( "javascript:" ), 0, false ) == 0 && !isIFrame )
    {
        // static cast is safe as of isIFrame being false.
        // but: shouldn't we support this javascript hack for iframes aswell?
        RenderFrame* rf = static_cast<RenderFrame*>(frame);
        assert(rf);
        QVariant res = executeScript( DOM::Node(rf->frameImpl()), url.right( url.length() - 11) );
        if ( res.type() == QVariant::String ) {
            KURL myurl;
            myurl.setProtocol("javascript");
            myurl.setPath(res.asString());
            return processObjectRequest(&(*it), myurl, QString("text/html") );
        }
        return false;
    }
#endif

    return true;
}

bool KWQKHTMLPartImpl::requestObject(RenderPart *frame, const QString &url, const QString &serviceType, const QStringList &args)
{
    if (url.isEmpty()) {
        return false;
    }
    NSURL *cocoaURL = part->completeURL(url).getNSURL();
    if (cocoaURL == nil) {
        // FIXME: We need to report an error to someone.
        return false;
    }

    if (frame->widget()) {
        return true;
    }
    
    NSMutableArray *argsArray = [NSMutableArray arrayWithCapacity:args.count()];
    for (uint i = 0; i < args.count(); i++) {
        [argsArray addObject:args[i].getNSString()];
    }
    
    QWidget *widget = new QWidget();
    widget->setView([[WebCoreViewFactory sharedFactory]
        viewForPluginWithURL:cocoaURL
                 serviceType:serviceType.getNSString()
                   arguments:argsArray
                     baseURL:KURL(d->m_doc->baseURL()).getNSURL()]);
    frame->setWidget(widget);
    return true;
}

void KWQKHTMLPartImpl::submitForm(const char *action, const QString &url, const QByteArray &formData, const QString &_target, const QString& contentType, const QString& boundary)
{
    QString target = _target;
    if (target.isEmpty() && d->m_doc) {
        target = d->m_doc->baseTarget();
    }

    KURL u = part->completeURL( url );
    if (u.isMalformed()) {
        // ### ERROR HANDLING!
        return;
    }

    QString urlstring = u.url();
    if (urlstring.find("javascript:", 0, false) == 0) {
        urlstring = KURL::decode_string(urlstring);
        part->executeScript(urlstring.right(urlstring.length() - 11));
        return;
    }

#ifdef NEED_THIS
  if (!checkLinkSecurity(u,
			 i18n( "<qt>The form will be submitted to <BR><B>%1</B><BR>on your local filesystem.<BR>Do you want to submit the form?" ),
			 i18n( "Submit" )))
    return;
#endif

#ifdef NEED_THIS
  if (!d->m_referrer.isEmpty())
     args.metaData()["referrer"] = d->m_referrer;
  args.metaData().insert("main_frame_request",
                         parentPart() == 0 ? "TRUE":"FALSE");
  args.metaData().insert("ssl_was_in_use", d->m_ssl_in_use ? "TRUE":"FALSE");
  args.metaData().insert("ssl_activate_warnings", "TRUE");
#endif

    if (strcmp(action, "get") == 0) {
	u.setQuery(QString(formData.data(), formData.size()));
	[getBridgeForFrameName(target) loadURL:u.getNSURL()];
    } else {
#ifdef NEED_THIS
    // construct some user headers if necessary
    if (contentType.isNull() || contentType == "application/x-www-form-urlencoded")
      args.setContentType( "Content-Type: application/x-www-form-urlencoded" );
    else // contentType must be "multipart/form-data"
      args.setContentType( "Content-Type: " + contentType + "; boundary=" + boundary );
#endif
	NSData *postData = [NSData dataWithBytes:formData.data() length:formData.size()];
	[getBridgeForFrameName(target) postWithURL:u.getNSURL() data:postData];
    }

#ifdef NEED_THIS
  if ( d->m_bParsing || d->m_runningScripts > 0 ) {
    if( d->m_submitForm ) {
        return;
    }
    d->m_submitForm = new KHTMLPartPrivate::SubmitForm;
    d->m_submitForm->submitAction = action;
    d->m_submitForm->submitUrl = url;
    d->m_submitForm->submitFormData = formData;
    d->m_submitForm->target = _target;
    d->m_submitForm->submitContentType = contentType;
    d->m_submitForm->submitBoundary = boundary;
    connect(this, SIGNAL(completed()), this, SLOT(submitFormAgain()));
  }
  else
    emit d->m_extension->openURLRequest( u, args );
#endif
}

bool KWQKHTMLPartImpl::frameExists(const QString &frameName)
{
    return [bridge frameNamed:frameName.getNSString()] != nil;
}

KHTMLPart *KWQKHTMLPartImpl::findFrame(const QString &frameName)
{
    return [[bridge frameNamed:frameName.getNSString()] part];
}

QPtrList<KParts::ReadOnlyPart> KWQKHTMLPartImpl::frames() const
{
    QPtrList<KParts::ReadOnlyPart> parts;
    NSEnumerator *e = [[bridge childFrames] objectEnumerator];
    WebCoreBridge *childFrame;
    while ((childFrame = [e nextObject])) {
        KHTMLPart *childPart = [childFrame part];
        if (childPart)
            parts.append(childPart);
    }
    return parts;
}

void KWQKHTMLPartImpl::setView(KHTMLView *view)
{
    d->m_view = view;
    part->setWidget(view);
}

KHTMLView *KWQKHTMLPartImpl::getView() const
{
    return d->m_view;
}

void KWQKHTMLPartImpl::setTitle(const DOMString &title)
{
    [bridge setTitle:title.string().getNSString()];
}

void KWQKHTMLPartImpl::setStatusBarText(const QString &status)
{
    [bridge setStatusText:status.getNSString()];
}

void KWQKHTMLPartImpl::scheduleClose()
{
    [[bridge window] performSelector:@selector(close) withObject:nil afterDelay:0.0];
}

void KWQKHTMLPartImpl::unfocusWindow()
{
    [bridge unfocusWindow];
}

void KWQKHTMLPartImpl::overURL(const QString &url, const QString &_target, int modifierState)
{
    if (url.isEmpty()) {
        [bridge setStatusText:@""];
        return;
    }

    int position = url.find("javascript:", 0, false);
    if (position == 0) {
        // FIXME: Is it worthwhile to special-case scripts that do a window.open and nothing else?
        const QString scriptName = url.mid(strlen("javascript:"));
        [bridge setStatusText:[NSString stringWithFormat:@"Run script \"%@\"", scriptName.getNSString()]];
        return;
    }
    
    KURL u = part->completeURL(url);
    
    if (u.protocol() == QString("mailto")) {
        // FIXME: Add address book integration so we show the real name instead?
        const QString address = KURL::decode_string(u.path());
        [bridge setStatusText:[NSString stringWithFormat:@"Send email to %@", address.getNSString()]];
        return;
    }
    
    NSString *format;
    
    QString target = _target;
    if (target.isEmpty() && d->m_doc) {
        target = d->m_doc->baseTarget();
    }

    if (target == "_blank") {
        format = @"Open \"%@\" in a new window";
    } else if (!target.isEmpty() && target != "_self" && target == "_top" && target != "_parent") {
        if (frameExists(target)) {
            // FIXME: Distinguish existing frame in same window from existing frame in other window.
            format = @"Go to \"%@\" in another frame";
        } else {
            format = @"Open \"%@\" in a new window";
        }
    } else {
        format = @"Go to \"%@\"";
    }
    
    if ([bridge modifierTrackingEnabled]) {
        if (modifierState & MetaButton) {
            if (modifierState & ShiftButton) {
                format = @"Open \"%@\" in a new window, behind the current window";
            } else {
                format = @"Open \"%@\" in a new window";
            }
        } else if (modifierState & AltButton) {
            format = @"Download \"%@\"";
        }
    }
    
    [bridge setStatusText:[NSString stringWithFormat:format, u.url().getNSString()]];
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
        [bridge reportClientRedirectTo:[NSURL _web_URLWithString:d->m_redirectURL.getNSString()]
                                 delay:d->m_delayRedirect
                              fireDate:[d->m_redirectionTimer.getNSTimer() fireDate]];
    } else {
        [bridge reportClientRedirectCancelled];
    }
}

static void moveWidgetsAside(RenderObject *object)
{
    RenderWidget *renderWidget = dynamic_cast<RenderWidget *>(object);
    if (renderWidget) {
        QWidget *widget = renderWidget->widget();
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
    if (getRenderer()) {
        moveWidgetsAside(getRenderer());
    }
}

DocumentImpl *KWQKHTMLPartImpl::getDocument()
{
    return part->xmlDocImpl();
}

RenderObject *KWQKHTMLPartImpl::getRenderer()
{
    DocumentImpl *doc = part->xmlDocImpl();
    return doc ? doc->renderer() : 0;
}
