// -*- c-basic-offset: 2 -*-
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

#include <kwqdebug.h>

#include <misc/decoder.h>
#include <qfont.h>
#include <qtextcodec.h>

#import <Foundation/Foundation.h>
#import <WebFoundation/WebFoundation.h>

#include <job.h>
#include <jobclasses.h>
#include <khtml_settings.h>
#include <khtml_factory.h>
#include <kcharsets.h>
#include <kglobal.h>
#include <html/htmltokenizer.h>
#include <html/html_imageimpl.h>
#include <xml/dom_docimpl.h>
#include <html/html_miscimpl.h>
#include <html/html_documentimpl.h>
#include <rendering/render_image.h>
#include <misc/loader.h>
#include <kjs/interpreter.h>
#include <kjs/collector.h>
#include <ecma/kjs_proxy.h>
#include <ecma/kjs_dom.h>
#include <dom/dom_doc.h>
#include <qcursor.h>
#include <kurl.h>
#include <khtmlview.h>

#include <qdatetime.h>
#include <khtml_part.h>
#include <khtmlpart_p.h>
#include <khtml_events.h>

#import <WCPluginWidget.h>

#include <rendering/render_frames.h>

#import <KWQView.h>

#include <WCWebDataSource.h>

#include <external.h>

#undef _KWQ_TIMING

WCIFWebDataSourceMakeFunc WCIFWebDataSourceMake;
void WCSetIFWebDataSourceMakeFunc(WCIFWebDataSourceMakeFunc func)
{
    WCIFWebDataSourceMake = func;
}

static void recursive(const DOM::Node &pNode, const DOM::Node &node)
{
    DOM::Node cur_child = node.lastChild();

    KWQDEBUG("cur_child: %s = %s", cur_child.nodeName().string().latin1(), cur_child.nodeValue().string().latin1());

    while(!cur_child.isNull())
    {
        recursive(node, cur_child);
        cur_child = cur_child.previousSibling();
    }
}

#if 0
static QString splitUrlTarget(const QString &url, QString *target=0)
{
    QString result = url;
    if(url.left(7) == "target:")
    {
#ifdef APPLE_CHANGES
        int pos, end;
        if ((pos = url.find ('#', 7)) != -1){
            result = url.mid(pos+1,url.length()-pos-1);
        }
        if (target){
            pos = url.find ("//", 7);
            if (pos > 0){
                end = url.find ('/', pos+2);
                if (end > 0)
                    *target = url.mid (pos+2, end-pos-2);
            }
        }
#else
        KURL u(url);
        result = u.ref();
        if (target)
            *target = u.host();
#endif
    }
    return result;
}
#endif

KHTMLPart::KHTMLPart(QWidget *, const char *, QObject *, const char *, GUIProfile prof)
{
    _ref = 1;
    init(0, prof);
}

bool KHTMLPart::openURL( const KURL &url )
{
    d->m_workingURL = url;
    m_url = url;

    d->m_documentSource = "";
    d->m_decodingStarted = 0;
    
    return true;
}

void KHTMLPart::slotData(NSString *encoding, const char *bytes, int length)
{
// NOTE: This code emulates the interface used by the original khtml part  
    QString enc;

    if (!d->m_workingURL.isEmpty()) {
        //begin(d->m_workingURL, d->m_extension->urlArgs().xOffset, d->m_extension->urlArgs().yOffset);
        begin(d->m_workingURL, 0, 0);

	//d->m_doc->docLoader()->setReloading(d->m_bReloading);
        d->m_workingURL = KURL();
    }

    if (encoding != NULL) {
      enc = QString::fromCFString((CFStringRef) encoding);
      setEncoding(enc, true);
    }
    
    // FIXME [rjw]:  Remove this log eventually.  Should never happen.  For debugging
    // purposes only.
    if (d->m_doc == 0){
        fprintf (stderr, "ERROR:  KHTMLPart::slotData m_doc == 0 IGNORING DATA, url = %s\n", m_url.url().latin1());
        return;
    }


    write(bytes, length);
}

void KHTMLPart::begin( const KURL &url, int xOffset, int yOffset )
{
  KParts::URLArgs args;
  args.xOffset = xOffset;
  args.yOffset = yOffset;
#ifndef APPLE_CHANGES
  d->m_extension->setURLArgs( args );
#endif

  // d->m_referrer = url.url();
  m_url = url;
  KURL baseurl;

  // ### not sure if XHTML documents served as text/xml should use DocumentImpl or HTMLDocumentImpl
  if (args.serviceType == "text/xml")
    d->m_doc = DOMImplementationImpl::instance()->createDocument( d->m_view );
  else
    d->m_doc = DOMImplementationImpl::instance()->createHTMLDocument( d->m_view );

    //DomShared::instanceToCheck = (void *)((DomShared *)d->m_doc);
    d->m_doc->ref();

    if (d->m_baseURL.isEmpty()) {
        d->m_baseURL = KURL();
    }
    else {
        // If we get here, this means the part has received a redirect before
        // m_doc was created. Update the document with the base URL.
        d->m_doc->setBaseURL(d->m_baseURL.url());
    }
    d->m_workingURL = url;

    /* FIXME: this is a pretty gross way to make sure the decoder gets reinitialized for each page. */
    if (d->m_decoder != NULL) {
	delete d->m_decoder;
	d->m_decoder = NULL;
    }

    //FIXME: do we need this? 
    if (!d->m_doc->attached())
	d->m_doc->attach();
    d->m_doc->setURL( url.url() );
    
    // do not set base URL if it has already been set
    if (!d->m_workingURL.isEmpty() && d->m_baseURL.isEmpty())
    {
	// We're not planning to support the KDE chained URL feature, AFAIK
#define KDE_CHAINED_URIS 0
#if KDE_CHAINED_URIS
        KURL::List lst = KURL::split( d->m_workingURL );
        KURL baseurl;
        if ( !lst.isEmpty() )
            baseurl = *lst.begin();
        // Use this for relative links.
        // We prefer m_baseURL over m_url because m_url changes when we are
        // about to load a new page.
        setBaseURL(baseurl);
#else
	setBaseURL(d->m_workingURL);
#endif
    }

    /* FIXME: we'll need to make this work....
    QString userStyleSheet = KHTMLFactory::defaultHTMLSettings()->userStyleSheet();
    if ( !userStyleSheet.isEmpty() ) {
        setUserStyleSheet( KURL( userStyleSheet ) );
    }
    */
    
    d->m_doc->open();    

    d->m_doc->setParsing(true);

#ifdef _KWQ_TIMING        
    d->totalWriteTime = 0;
#endif
}

void KHTMLPart::write( const char *str, int len )
{
    /* FIXME: hook this code back when we have decoders completely working */
#ifndef APPLE_CHANGES
  if(d->m_bFirstData) {
      // determine the parse mode
      d->m_doc->determineParseMode( decoded );
      d->m_bFirstData = false;
    
  //kdDebug(6050) << "KHTMLPart::write haveEnc = " << d->m_haveEncoding << endl;
      // ### this is still quite hacky, but should work a lot better than the old solution
      if(d->m_decoder->visuallyOrdered()) d->m_doc->setVisuallyOrdered();

      if (!d->m_haveCharset)
        {
            const QTextCodec *c = d->m_decoder->codec();
            //kdDebug(6005) << "setting up charset to " << (int) KGlobal::charsets()->charsetForEncoding(c->name()) << endl;
            d->m_charset = KGlobal::charsets()->charsetForEncoding(c->name());
            d->m_settings->setCharset( d->m_charset );
            d->m_settings->setScript( KGlobal::charsets()->charsetForEncoding(c->name(), true ));
            //kdDebug(6005) << "charset is " << (int)d->m_settings->charset() << endl;
        }
        d->m_doc->applyChanges(true, true);
    }

#endif APPLE_CHANGES

    // begin lines added in lieu of big fixme    
    if ( !d->m_decoder ) {
        d->m_decoder = new khtml::Decoder();
        if(!d->m_encoding.isNull())
            d->m_decoder->setEncoding(d->m_encoding.latin1(), d->m_haveEncoding);
        else {
            //FIXME: d->m_decoder->setEncoding(settings()->encoding().latin1(), d->m_haveEncoding);
        }
    }
    if ( len == 0 )
        return;
    
    if ( len == -1 )
        len = strlen( str );
    
#ifdef _KWQ_TIMING        
    double start = CFAbsoluteTimeGetCurrent();
#endif
    
    // FIX ME:  This is very expensive.  We should using the IFMutableData
    // that represents the document, and only constructing the complete
    // string when requested.
    d->m_documentSource += QString(str, len);

    QString decoded;
    if (d->m_decodingStarted)
        decoded = d->m_decoder->decode( str, len );
    else    
        decoded = d->m_decoder->decode( d->m_documentSource, d->m_documentSource.length() );

    if(decoded.isEmpty()){
        fprintf (stderr, "WARNING:  DECODER unable to decode string, length = %d, total length = %d\n", len, d->m_documentSource.length());
        return;
    }

    d->m_decodingStarted = 1;
        
    // Transition from provisional to committed data source at this point.
    
#if FIGURE_OUT_WHAT_APPLY_CHANGES_DOES
    d->m_doc->applyChanges();
#endif    

    // end lines added in lieu of big fixme
    
    if (jScript())
	jScript()->appendSourceFile(m_url.url(),decoded);
    Tokenizer* t = d->m_doc->tokenizer();
    if(t)
        t->write( decoded, true );

#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    d->totalWriteTime += thisTime;
    KWQDEBUGLEVEL (0x200, "%s bytes = %d, seconds = %f, total = %f\n", m_url.url().latin1(), len, thisTime, d->totalWriteTime);
#endif
}

void KHTMLPart::end()
{
    // FIXME [rjw]:  Remove this log eventually.  Should never happen.  For debugging
    // purposes only.
    if (d->m_doc == 0){
        fprintf (stderr, "ERROR:  KHTMLPart::end  m_doc == 0\n");
        return;
    }

    d->m_doc->setParsing(false);

    d->m_doc->close();
    KURL::clearCaches();
    
    d->m_view->complete();
}

bool KHTMLPart::gotoBaseAnchor()
{
    if ( !m_url.ref().isEmpty() )
        return gotoAnchor( m_url.ref() );
    return false;
}

void KHTMLPart::scheduleRedirection( int delay, const QString &url, bool )
{
	if( d->m_redirectURL.isEmpty() || delay < d->m_delayRedirect )
	{
		d->m_delayRedirect = delay;
		d->m_redirectURL = url;
		killTimer(d->m_redirectionTimer);
		d->m_redirectionTimer = startTimer( 1000 * d->m_delayRedirect);
	}
}

void KHTMLPart::timerEvent ( QTimerEvent *e )
{
    if (e->timerId()==d->m_redirectionTimer)
    {
    	redirectJS();
    	return;
    }
}

void KHTMLPart::redirectJS()
{
  QString u = d->m_redirectURL;
  d->m_delayRedirect = 0;
  d->m_redirectURL = QString::null;
  if ( u.find( QString::fromLatin1( "javascript:" ), 0, false ) == 0 )
  {
    QString script = KURL::decode_string( u.right( u.length() - 11 ) );
    //kdDebug( 6050 ) << "KHTMLPart::slotRedirect script=" << script << endl;
    QVariant res = executeScript( script );
    if ( res.type() == QVariant::String ) {
      begin( url() );
      write( res.asString() );
      end();
    }
    return;
  }

  KParts::URLArgs args;
#ifndef APPLE_CHANGES
  if ( urlcmp( u, m_url.url(), true, true ) )
    args.reload = true;

  args.setLockHistory( true );
#endif
  urlSelected( u, 0, 0, QString::null, args );
}



void KHTMLPart::urlSelected( const QString &url, int button, int state, const QString &_target, KParts::URLArgs )
{
	IFWebDataSource *oldDataSource, *newDataSource;
	KURL clickedURL(completeURL( url));
	IFWebFrame *frame;
	KURL refLess(clickedURL);
	
	m_url.setRef ("");
	refLess.setRef ("");
	if (refLess.url() == m_url.url()){
		m_url = clickedURL;
		gotoAnchor (clickedURL.ref());
		return;
	}
	
	if (_target.isEmpty()){
		oldDataSource = getDataSource();
		frame = [oldDataSource webFrame];
	}
	else {
		frame = [[getDataSource() controller] frameNamed: QSTRING_TO_NSSTRING(_target)];
		oldDataSource = [frame dataSource];
	}
	
	newDataSource = WCIFWebDataSourceMake(clickedURL.getNSURL(), nil, 0);
	[newDataSource _setParent: [oldDataSource parent]];
	
	[frame setProvisionalDataSource: newDataSource];
	[frame startLoading];
}

bool KHTMLPart::requestFrame( khtml::RenderPart *frame, const QString &url, const QString &frameName,
                              const QStringList &params, bool isIFrame )
{
    NSString *nsframeName = QSTRING_TO_NSSTRING(frameName);
    IFWebFrame *aFrame;
    IFWebDataSource *dataSource;
    
    dataSource = getDataSource();

    aFrame =[dataSource frameNamed: nsframeName];
    if (aFrame){
        if ([[aFrame view] _provisionalWidget])
            frame->setWidget ([[aFrame view] _provisionalWidget]);
        else
            frame->setWidget ([[aFrame view] _widget]);
    }
    else {        
        IFWebDataSource *oldDataSource, *newDataSource;
        NSURL *childURL;
        IFWebFrame *newFrame;
        id <IFWebController> controller;
        HTMLIFrameElementImpl *o = static_cast<HTMLIFrameElementImpl *>(frame->element());
                
        childURL = completeURL(url).getNSURL();
        if (childURL == nil || [childURL path] == nil) {
            NSLog (@"ERROR (probably need to fix CFURL): unable to create URL with path");
            return false;
        }
        
        oldDataSource = getDataSource();
        controller = [oldDataSource controller];
        newFrame = [controller createFrameNamed: nsframeName for: nil inParent: oldDataSource inScrollView: o->scrollingMode() == QScrollView::AlwaysOff ? NO : YES];
        if (newFrame == nil){
            // Controller return NO to location change, now what?
            return false;
        }
        [newFrame _setRenderFramePart: frame];
        
        newDataSource = WCIFWebDataSourceMake(childURL, nil, 0);
        [newDataSource _setParent: oldDataSource];
        [newFrame setProvisionalDataSource: newDataSource];
    
        KHTMLView *view = (KHTMLView *)[[newFrame view] _provisionalWidget];
        view->setMarginWidth (o->getMarginWidth());
        view->setMarginHeight (o->getMarginWidth());
        
        [newFrame startLoading];
    }

#ifdef _SUPPORT_JAVASCRIPT_URL_    
    if ( url.find( QString::fromLatin1( "javascript:" ), 0, false ) == 0 && !isIFrame )
    {
        // static cast is safe as of isIFrame being false.
        // but: shouldn't we support this javascript hack for iframes aswell?
        khtml::RenderFrame* rf = static_cast<khtml::RenderFrame*>(frame);
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

bool KHTMLPart::requestObject( khtml::RenderPart *frame, const QString &url, const QString &serviceType,
			       const QStringList &args )
{
  if (url.isEmpty()) {
    return false;
  }
  if (!frame->widget()) {
    frame->setWidget(IFPluginWidgetCreate(completeURL(url).url(), serviceType, args, d->m_baseURL.url()));
  }
  return true;
}

bool KHTMLPart::requestObject( khtml::ChildFrame *frame, const KURL &url, const KParts::URLArgs &args )
{
  _logNotYetImplemented();
  return false;
}

void KHTMLPart::submitForm( const char *action, const QString &url, const QByteArray &formData, const QString &_target, const QString& contentType, const QString& boundary )
{
  QString target = _target;
  
  //if ( target.isEmpty() )
  //  target = d->m_baseTarget;

  KURL u = completeURL( url );

  if ( u.isMalformed() )
  {
    // ### ERROR HANDLING!
    return;
  }

  QString urlstring = u.url();

  if ( urlstring.find( QString::fromLatin1( "javascript:" ), 0, false ) == 0 ) {
    urlstring = KURL::decode_string(urlstring);
    executeScript( urlstring.right( urlstring.length() - 11) );
    return;
  }

#ifdef NEED_THIS
  if (!checkLinkSecurity(u,
			 i18n( "<qt>The form will be submitted to <BR><B>%1</B><BR>on your local filesystem.<BR>Do you want to submit the form?" ),
			 i18n( "Submit" )))
    return;
#endif

  NSMutableDictionary *attributes = [NSMutableDictionary dictionary];

#ifdef NEED_THIS
  KParts::URLArgs args;

  if (!d->m_referrer.isEmpty())
     args.metaData()["referrer"] = d->m_referrer;

  args.metaData().insert("main_frame_request",
                         parentPart() == 0 ? "TRUE":"FALSE");
  args.metaData().insert("ssl_was_in_use", d->m_ssl_in_use ? "TRUE":"FALSE");
  args.metaData().insert("ssl_activate_warnings", "TRUE");
  args.frameName = _target.isEmpty() ? d->m_doc->baseTarget() : _target ;
#endif

  if ( strcmp( action, "get" ) == 0 )
  {
    u.setQuery( QString( formData.data(), formData.size() ) );
    [attributes setObject:@"GET" forKey:IFHTTPURLHandleRequestMethod];

#ifdef NEED_THIS
    args.frameName = target;
    args.setDoPost( false );
#endif
  }
  else
  {
#ifdef NEED_THIS
    args.postData = formData;
    args.frameName = target;
    args.setDoPost( true );

    // construct some user headers if necessary
    if (contentType.isNull() || contentType == "application/x-www-form-urlencoded")
      args.setContentType( "Content-Type: application/x-www-form-urlencoded" );
    else // contentType must be "multipart/form-data"
      args.setContentType( "Content-Type: " + contentType + "; boundary=" + boundary );
#endif
      NSData *postData = [NSData dataWithBytes:formData.data() length:formData.size()];
      [attributes setObject:postData forKey:IFHTTPURLHandleRequestData];
      [attributes setObject:@"POST" forKey:IFHTTPURLHandleRequestMethod];
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
    IFWebDataSource *oldDataSource, *newDataSource;
    IFWebFrame *frame;
    
    oldDataSource = getDataSource();
    frame = [oldDataSource webFrame];
    
    newDataSource = WCIFWebDataSourceMake(u.getNSURL(), attributes, 0);
    [newDataSource _setParent: [oldDataSource parent]];
    
    [frame setProvisionalDataSource: newDataSource];
    [frame startLoading];
}

KHTMLPart *KHTMLPart::findFrame( const QString &f )
{
    _logNeverImplemented();
    return this;
}

bool KHTMLPart::frameExists( const QString &frameName )
{
    return [getDataSource() frameExists: (NSString *)frameName.getCFMutableString()];
}

QPtrList<KParts::ReadOnlyPart> KHTMLPart::frames() const
{
    _logNeverImplemented();
    return QPtrList<KParts::ReadOnlyPart>(); 
}

bool KHTMLPart::event( QEvent *event )
{
  if ( khtml::MousePressEvent::test( event ) )
  {
    khtmlMousePressEvent( static_cast<khtml::MousePressEvent *>( event ) );
    return true;
  }

  if ( khtml::MouseDoubleClickEvent::test( event ) )
  {
    khtmlMouseDoubleClickEvent( static_cast<khtml::MouseDoubleClickEvent *>( event ) );
    return true;
  }

  if ( khtml::MouseMoveEvent::test( event ) )
  {
    khtmlMouseMoveEvent( static_cast<khtml::MouseMoveEvent *>( event ) );
    return true;
  }

  if ( khtml::MouseReleaseEvent::test( event ) )
  {
    khtmlMouseReleaseEvent( static_cast<khtml::MouseReleaseEvent *>( event ) );
    return true;
  }

  if ( khtml::DrawContentsEvent::test( event ) )
  {
    khtmlDrawContentsEvent( static_cast<khtml::DrawContentsEvent *>( event ) );
    return true;
  }

  return false;
}

void KHTMLPart::khtmlMousePressEvent( khtml::MousePressEvent *event )
{
    DOM::DOMString url = event->url();
    QMouseEvent *_mouse = event->qmouseEvent();
    DOM::Node innerNode = event->innerNode();
    d->m_mousePressNode = innerNode;
    
    d->m_dragStartPos = _mouse->pos();
    
    if ( !event->url().isNull() ){
        d->m_strSelectedURL = event->url().string();
        d->m_strSelectedURLTarget = event->target().string();
    }
    else
        d->m_strSelectedURL = d->m_strSelectedURLTarget = QString::null;
    
    if ( _mouse->button() == LeftButton ||
        _mouse->button() == MidButton )
    {
        d->m_bMousePressed = true;
        
#ifndef KHTML_NO_SELECTION
        if ( _mouse->button() == LeftButton )
        {
            if ( !innerNode.isNull()  && innerNode.handle()->renderer()) {
                int offset = 0;
                DOM::NodeImpl* node = 0;
                innerNode.handle()->renderer()->checkSelectionPoint( event->x(), event->y(),
                                                                    event->absX()-innerNode.handle()->renderer()->xPos(),
                                                                    event->absY()-innerNode.handle()->renderer()->yPos(), node, offset);
        
                d->m_selectionStart = node;
                d->m_startOffset = offset;
        //           kdDebug(6005) << "KHTMLPart::khtmlMousePressEvent selectionStart=" << d->m_selectionStart.handle()->renderer()
        //                         << " offset=" << d->m_startOffset << endl;
                d->m_selectionEnd = d->m_selectionStart;
                d->m_endOffset = d->m_startOffset;
                d->m_doc->clearSelection();
            }
            else
            {
            d->m_selectionStart = DOM::Node();
            d->m_selectionEnd = DOM::Node();
            }
            //emitSelectionChanged();
            //startAutoScroll();
        }
#else
        d->m_dragLastPos = _mouse->globalPos();
#endif
    }
    
    if ( _mouse->button() == RightButton )
    {
        //popupMenu( d->m_strSelectedURL );
        d->m_strSelectedURL = d->m_strSelectedURLTarget = QString::null;
    }
}

void KHTMLPart::khtmlMouseDoubleClickEvent( khtml::MouseDoubleClickEvent * )
{
}

void KHTMLPart::khtmlMouseMoveEvent( khtml::MouseMoveEvent *event )
{
// FIXME: need working implementation of this event
#if 0
  QMouseEvent *_mouse = event->qmouseEvent();
  DOM::DOMString url = event->url();
  DOM::Node innerNode = event->innerNode();

#define QT_NO_DRAGANDDROP 1
#ifndef QT_NO_DRAGANDDROP
  if( d->m_bMousePressed && (!d->m_strSelectedURL.isEmpty() || (!innerNode.isNull() && innerNode.elementId() == ID_IMG) ) &&
      ( d->m_dragStartPos - _mouse->pos() ).manhattanLength() > KGlobalSettings::dndEventDelay() &&
      d->m_bDnd && d->m_mousePressNode == innerNode ) {

      QPixmap p;
      QDragObject *drag = 0;
      if( !d->m_strSelectedURL.isEmpty() ) {
          KURL u( completeURL( splitUrlTarget(d->m_strSelectedURL)) );
          KURLDrag* urlDrag = KURLDrag::newDrag( u, d->m_view->viewport() );
          if ( !d->m_referrer.isEmpty() )
            urlDrag->metaData()["referrer"] = d->m_referrer;
          drag = urlDrag;
          p = KMimeType::pixmapForURL(u, 0, KIcon::Desktop, KIcon::SizeMedium);
      } else {
          HTMLImageElementImpl *i = static_cast<HTMLImageElementImpl *>(innerNode.handle());
          if( i ) {
            KMultipleDrag *mdrag = new KMultipleDrag( d->m_view->viewport() );
            mdrag->addDragObject( new QImageDrag( i->currentImage(), 0L ) );
            KURL u( completeURL( splitUrlTarget( khtml::parseURL(i->getAttribute(ATTR_SRC)).string() ) ) );
            KURLDrag* urlDrag = KURLDrag::newDrag( u, 0L );
            if ( !d->m_referrer.isEmpty() )
              urlDrag->metaData()["referrer"] = d->m_referrer;
            mdrag->addDragObject( urlDrag );
            drag = mdrag;
            p = KMimeType::mimeType("image/png")->pixmap(KIcon::Desktop);
          }
      }

    if ( !p.isNull() )
      drag->setPixmap(p);

    stopAutoScroll();
    if(drag)
        drag->drag();

    // when we finish our drag, we need to undo our mouse press
    d->m_bMousePressed = false;
    d->m_strSelectedURL = d->m_strSelectedURLTarget = QString::null;
    return;
  }
#endif
  DOM::DOMString url = event->url();
  DOM::DOMString target = event->target();

  // Not clicked -> mouse over stuff
  if ( !d->m_bMousePressed )
  {
    // The mouse is over something
    if ( url.length() )
    {
      bool shiftPressed = ( _mouse->state() & ShiftButton );

      // Image map
      if ( !innerNode.isNull() && innerNode.elementId() == ID_IMG )
      {
        HTMLImageElementImpl *i = static_cast<HTMLImageElementImpl *>(innerNode.handle());
        if ( i && i->isServerMap() )
        {
          khtml::RenderObject *r = i->renderer();
          if(r)
          {
            int absx, absy, vx, vy;
            r->absolutePosition(absx, absy);
            view()->contentsToViewport( absx, absy, vx, vy );

            int x(_mouse->x() - vx), y(_mouse->y() - vy);

            d->m_overURL = url.string() + QString("?%1,%2").arg(x).arg(y);
            d->m_overURLTarget = target.string();
            overURL( d->m_overURL, target.string(), shiftPressed );
            return;
          }
        }
      }

      // normal link
      if ( d->m_overURL.isEmpty() || d->m_overURL != url || d->m_overURLTarget != target )
      {
        d->m_overURL = url.string();
        d->m_overURLTarget = target.string();
        overURL( d->m_overURL, target.string(), shiftPressed );
      }
    }
    else  // Not over a link...
    {
      if( !d->m_overURL.isEmpty() ) // and we were over a link  -> reset to "default statusbar text"
      {
        d->m_overURL = d->m_overURLTarget = QString::null;
        emit onURL( QString::null );
        // Default statusbar text can be set from javascript. Otherwise it's empty.
        emit setStatusBarText( d->m_kjsDefaultStatusBarText );
      }
    }
  }
  else {
#ifndef KHTML_NO_SELECTION
    // selection stuff
    if( d->m_bMousePressed && innerNode.handle() && innerNode.handle()->renderer() &&
        ( _mouse->state() == LeftButton )) {
      int offset;
      //kdDebug(6000) << "KHTMLPart::khtmlMouseMoveEvent x=" << event->x() << " y=" << event->y()
      //              << " nodeAbsX=" << event->nodeAbsX() << " nodeAbsY=" << event->nodeAbsY()
      //              << endl;
      DOM::NodeImpl* node=0;
      innerNode.handle()->renderer()->checkSelectionPoint( event->x(), event->y(),
                                                          event->absX()-innerNode.handle()->renderer()->xPos(),
                                                           event->absY()-innerNode.handle()->renderer()->yPos(), node, offset);
       d->m_selectionEnd = node;
       d->m_endOffset = offset;
//        if (d->m_selectionEnd.handle() && d->m_selectionEnd.handle()->renderer())
//          kdDebug( 6000 ) << "setting end of selection to " << d->m_selectionEnd.handle()->renderer() << "/"
//                          << d->m_endOffset << endl;

      // we have to get to know if end is before start or not...
      DOM::Node n = d->m_selectionStart;
      d->m_startBeforeEnd = false;
      while(!n.isNull()) {
        if(n == d->m_selectionEnd) {
          d->m_startBeforeEnd = true;
          break;
        }
        DOM::Node next = n.firstChild();
        if(next.isNull()) next = n.nextSibling();
        while( next.isNull() && !n.parentNode().isNull() ) {
          n = n.parentNode();
          next = n.nextSibling();
        }
        n = next;
        //d->m_view->viewport()->repaint(false);
      }

      if ( !d->m_selectionStart.isNull() && !d->m_selectionEnd.isNull() )
      {
        if (d->m_selectionEnd == d->m_selectionStart && d->m_endOffset < d->m_startOffset)
          d->m_doc
            ->setSelection(d->m_selectionStart.handle(),d->m_endOffset,
                           d->m_selectionEnd.handle(),d->m_startOffset);
        else if (d->m_startBeforeEnd)
          d->m_doc
            ->setSelection(d->m_selectionStart.handle(),d->m_startOffset,
                           d->m_selectionEnd.handle(),d->m_endOffset);
        else
          d->m_doc
            ->setSelection(d->m_selectionEnd.handle(),d->m_endOffset,
                           d->m_selectionStart.handle(),d->m_startOffset);
      }
#else
      if ( d->m_doc && d->m_view ) {
        QPoint diff( _mouse->globalPos() - d->m_dragLastPos );

        if ( abs( diff.x() ) > 64 || abs( diff.y() ) > 64 ) {
          d->m_view->scrollBy( -diff.x(), -diff.y() );
          d->m_dragLastPos = _mouse->globalPos();
        }
#endif
    }
  }
#endif
}

void KHTMLPart::khtmlMouseReleaseEvent( khtml::MouseReleaseEvent *event )
{
  DOM::Node innerNode = event->innerNode();
  d->m_mousePressNode = DOM::Node();

  if ( d->m_bMousePressed )
    stopAutoScroll();

  // Used to prevent mouseMoveEvent from initiating a drag before
  // the mouse is pressed again.
  d->m_bMousePressed = false;

    // HACK!  FIXME!
    if (d->m_strSelectedURL != QString::null) {
      	urlSelected(d->m_strSelectedURL, 0,0, event->target().string());
    }
    
#ifndef _KWQ_
#define QT_NO_CLIPBOARD 1
#ifndef QT_NO_CLIPBOARD
  QMouseEvent *_mouse = event->qmouseEvent();
  if ((_mouse->button() == MidButton) && (event->url().isNull()))
  {
    QClipboard *cb = QApplication::clipboard();
    cb->setSelectionMode( true );
    QCString plain("plain");
    QString url = cb->text(plain).stripWhiteSpace();
    KURL u(url);
    if ( u.isMalformed() ) {
      // some half-baked guesses for incomplete urls
      // (the same code is in libkonq/konq_dirpart.cc)
      if ( url.startsWith( "ftp." ) )
      {
        url.prepend( "ftp://" );
        u = url;
      }
      else
      {
        url.prepend( "http://" );
        u = url;
      }
    }
    if (u.isValid())
    {
      QString savedReferrer = d->m_referrer;
      d->m_referrer = QString::null; // Disable referrer.
      urlSelected(url, 0,0, "_top");
      d->m_referrer = savedReferrer; // Restore original referrer.
    }
  }
#endif

#ifndef KHTML_NO_SELECTION
  // delete selection in case start and end position are at the same point
  if(d->m_selectionStart == d->m_selectionEnd && d->m_startOffset == d->m_endOffset) {
    d->m_selectionStart = 0;
    d->m_selectionEnd = 0;
    d->m_startOffset = 0;
    d->m_endOffset = 0;
    //emitSelectionChanged();
  } else {
    // we have to get to know if end is before start or not...
    DOM::Node n = d->m_selectionStart;
    d->m_startBeforeEnd = false;
    if( d->m_selectionStart == d->m_selectionEnd ) {
      if( d->m_startOffset < d->m_endOffset )
        d->m_startBeforeEnd = true;
    } else {
      while(!n.isNull()) {
        if(n == d->m_selectionEnd) {
          d->m_startBeforeEnd = true;
          break;
        }
        DOM::Node next = n.firstChild();
        if(next.isNull()) next = n.nextSibling();
        while( next.isNull() && !n.parentNode().isNull() ) {
          n = n.parentNode();
          next = n.nextSibling();
        }
        n = next;
      }
    }
    if(!d->m_startBeforeEnd)
    {
      DOM::Node tmpNode = d->m_selectionStart;
      int tmpOffset = d->m_startOffset;
      d->m_selectionStart = d->m_selectionEnd;
      d->m_startOffset = d->m_endOffset;
      d->m_selectionEnd = tmpNode;
      d->m_endOffset = tmpOffset;
      d->m_startBeforeEnd = true;
    }
    // get selected text and paste to the clipboard
#ifndef QT_NO_CLIPBOARD
    QString text = selectedText();
    text.replace(QRegExp(QChar(0xa0)), " ");
    QClipboard *cb = QApplication::clipboard();
    cb->setSelectionMode( true );
    cb->setText(text);
    cb->setSelectionMode( false );
#endif
    //kdDebug( 6000 ) << "selectedText = " << text << endl;
    //emitSelectionChanged();
  }
#endif
#endif

}

void KHTMLPart::khtmlDrawContentsEvent( khtml::DrawContentsEvent * )
{
}

QString KHTMLPart::documentSource() const
{
    return d->m_documentSource;
}

void KHTMLPart::setBaseURL(const KURL &url)
{
    d->m_baseURL = url;
    if (d->m_baseURL.protocol().startsWith( "http" ) && !d->m_baseURL.host().isEmpty() && d->m_baseURL.path().isEmpty()) {
        d->m_baseURL.setPath( "/" );
        // communicate the change in base URL to the document so that links and subloads work
        if (d->m_doc) {
            d->m_doc->setBaseURL(url.url());
        }
    }
}

void KHTMLPart::setView(KHTMLView *view)
{
    d->m_view = view;
}

void KHTMLPart::nodeActivated(const DOM::Node &aNode)
{
    KWQDEBUG ("name %s = %s\n", (const char *)aNode.nodeName().string(), (const char *)aNode.nodeValue().string());
}

void KHTMLPart::setTitle(const DOMString &title)
{
    [getDataSource() _setTitle:(NSString *)title.string().getCFMutableString()];
}

void KHTMLPart::setDataSource(IFWebDataSource *dataSource)
{
    d->m_dataSource = dataSource; // not retained
}

IFWebDataSource *KHTMLPart::getDataSource()
{
    return (IFWebDataSource *)d->m_dataSource;
}
