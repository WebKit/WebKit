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
#import <khtmlpart_p.h>
#import <khtmlview.h>

#import <WebCoreBridge.h>
#import <WebCoreFrameBridge.h>
#import <WebCoreViewFactory.h>

#import <kwqdebug.h>

#undef _KWQ_TIMING

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

KWQKHTMLPartImpl::KWQKHTMLPartImpl(KHTMLPart *p)
    : part(p)
    , d(part->d)
    , m_redirectionTimer(0)
    , m_decodingStarted(false)
{
}

KWQKHTMLPartImpl::~KWQKHTMLPartImpl()
{
    killTimer(m_redirectionTimer);
}

bool KWQKHTMLPartImpl::openURLInFrame( const KURL &url, const KParts::URLArgs &urlArgs )
{
    WebCoreFrameBridge *frame;

    if (!urlArgs.frameName.isEmpty()) {
        frame = [bridge frameNamed:urlArgs.frameName.getNSString()];
        if (frame == nil) {
            frame = [bridge mainFrame];
        }
    } else {
        frame = [bridge frame];
    }

    [frame loadURL:url.getNSURL()];

    return true;
}

void KWQKHTMLPartImpl::openURL(const KURL &url)
{
    d->m_workingURL = url;
    part->m_url = url;

    m_documentSource = "";
    m_decodingStarted = false;
}

void KWQKHTMLPartImpl::slotData(NSString *encoding, const char *bytes, int length, bool complete)
{
// NOTE: This code emulates the interface used by the original khtml part  
    QString enc;

    // This flag is used to tell when a load has completed so we can be sure
    // to process the data even if we have not yet determined the proper
    // encoding.
    if (complete) {
        d->m_bComplete = true;    
    }

    if (!d->m_workingURL.isEmpty()) {
        //begin(d->m_workingURL, d->m_extension->urlArgs().xOffset, d->m_extension->urlArgs().yOffset);
        part->begin(d->m_workingURL, 0, 0);

	//d->m_doc->docLoader()->setReloading(d->m_bReloading);
        d->m_workingURL = KURL();
    }

    if (encoding != NULL) {
        enc = QString::fromCFString((CFStringRef) encoding);
        part->setEncoding(enc, true);
    }
    
    KWQ_ASSERT(d->m_doc != NULL);

    part->write(bytes, length);
}

// FIXME: Need to remerge this with code in khtml_part.cpp?
void KWQKHTMLPartImpl::begin( const KURL &url, int xOffset, int yOffset )
{
  KParts::URLArgs args;
  args.xOffset = xOffset;
  args.yOffset = yOffset;
  d->m_extension->setURLArgs( args );

  // d->m_referrer = url.url();
  part->m_url = url;
  KURL baseurl;

  // ### not sure if XHTML documents served as text/xml should use DocumentImpl or HTMLDocumentImpl
  if (args.serviceType == "text/xml")
    d->m_doc = DOM::DOMImplementationImpl::instance()->createDocument( d->m_view );
  else
    d->m_doc = DOM::DOMImplementationImpl::instance()->createHTMLDocument( d->m_view );

    //DomShared::instanceToCheck = (void *)((DomShared *)d->m_doc);
    d->m_doc->ref();

    if (m_baseURL.isEmpty()) {
        m_baseURL = KURL();
    }
    else {
        // If we get here, this means the part has received a redirect before
        // m_doc was created. Update the document with the base URL.
        d->m_doc->setBaseURL(m_baseURL.url());
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
    if (!d->m_workingURL.isEmpty() && m_baseURL.isEmpty())
    {
	// We're not planning to support the KDE chained URL feature, AFAIK
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

// FIXME: Need to remerge this with code in khtml_part.cpp?
void KWQKHTMLPartImpl::write( const char *str, int len )
{
    /* FIXME: hook this code back when we have decoders completely working */
#if 0
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
#endif

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
    
    // FIX ME:  This is very expensive.  We should be using the data object
    // that represents the document, and only constructing the complete
    // string when requested.
    m_documentSource += QString(str, len);

    QString decoded;
    if (m_decodingStarted)
        decoded = d->m_decoder->decode( str, len );
    else    
        decoded = d->m_decoder->decode( m_documentSource, m_documentSource.length() );

    if(decoded.isEmpty()){
        // Check flag to tell whether the load has completed.
        // If we get here, it means that no text encoding was available.
        // Try to process what we have with the default encoding.
        if (d->m_bComplete) {
            decoded = m_documentSource;
        }
        else {
            fprintf (stderr, "WARNING:  DECODER unable to decode string, length = %d, total length = %d\n", len, m_documentSource.length());
            return;
        }
    }

    m_decodingStarted = true;
        
    // Transition from provisional to committed data source at this point.
    
#if FIGURE_OUT_WHAT_APPLY_CHANGES_DOES
    d->m_doc->applyChanges();
#endif    

    // end lines added in lieu of big fixme
    
    if (part->jScript())
	part->jScript()->appendSourceFile(part->m_url.url(),decoded);
    Tokenizer* t = d->m_doc->tokenizer();
    if(t)
        t->write( decoded, true );

#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    d->totalWriteTime += thisTime;
    KWQDEBUGLEVEL (0x200, "%s bytes = %d, seconds = %f, total = %f\n", part->m_url.url().latin1(), len, thisTime, d->totalWriteTime);
#endif
}

// FIXME: Need to remerge this with code in khtml_part.cpp?
void KWQKHTMLPartImpl::end()
{
    KWQ_ASSERT(d->m_doc != NULL);

    d->m_doc->setParsing(false);

    d->m_doc->close();
    KURL::clearCaches();
    
    if (d->m_view)
        d->m_view->complete();
}
 
bool KWQKHTMLPartImpl::gotoBaseAnchor()
{
    if ( !part->m_url.ref().isEmpty() )
        return part->gotoAnchor( part->m_url.ref() );
    return false;
}

// FIXME: Need to remerge this with code in khtml_part.cpp?
void KWQKHTMLPartImpl::scheduleRedirection(int delay, const QString &url)
{
    if( d->m_redirectURL.isEmpty() || delay < d->m_delayRedirect )
    {
        if (delay < 1) {
            delay = 1;
        }
        d->m_delayRedirect = delay;
        d->m_redirectURL = url;
        killTimer(m_redirectionTimer);
        m_redirectionTimer = startTimer(1000 * d->m_delayRedirect);
    }
}

void KWQKHTMLPartImpl::timerEvent(QTimerEvent *e)
{
    if (e->timerId()==m_redirectionTimer)
    {
    	redirectURL();
    	return;
    }
}

void KWQKHTMLPartImpl::redirectURL()
{
  QString u = d->m_redirectURL;
  d->m_delayRedirect = 0;
  d->m_redirectURL = QString::null;
  if ( u.find( QString::fromLatin1( "javascript:" ), 0, false ) == 0 )
  {
    QString script = KURL::decode_string( u.right( u.length() - 11 ) );
    //kdDebug( 6050 ) << "KHTMLPart::slotRedirect script=" << script << endl;
    QVariant res = part->executeScript( script );
    if ( res.type() == QVariant::String ) {
      part->begin( part->url() );
      part->write( res.asString() );
      part->end();
    }
    return;
  }

  KParts::URLArgs args;
#if 0
  if ( urlcmp( u, m_url.url(), true, true ) )
    args.reload = true;

  args.setLockHistory( true );
#endif
  part->urlSelected( u, 0, 0, QString::null, args );
}



void KWQKHTMLPartImpl::urlSelected( const QString &url, int button, int state, const QString &_target, KParts::URLArgs )
{
    KURL clickedURL(part->completeURL( url));
    KURL refLess(clickedURL);
    WebCoreFrameBridge *frame;
	
    if ( url.find( QString::fromLatin1( "javascript:" ), 0, false ) == 0 )
    {
        part->executeScript( url.right( url.length() - 11) );
        return;
    }

    // Open new window on command-click
    if (state & MetaButton) {
        [bridge openNewWindowWithURL:clickedURL.getNSURL()];
        return;
    }

    part->m_url.setRef ("");
    refLess.setRef ("");
    if (refLess.url() == part->m_url.url()){
        part->m_url = clickedURL;
        part->gotoAnchor (clickedURL.ref());
        return;
    }
    
    if (_target.isEmpty()) {
        // If we're the only frame in a frameset then pop the frame.
        if ([[[bridge parent] childFrames] count] == 1) {
            frame = [[bridge parent] frame];
        } else {
            frame = [bridge frame];
        }
    }
    else {
        frame = [bridge descendantFrameNamed:_target.getNSString()];
        if (frame == nil) {
            NSLog (@"WARNING: unable to find frame named %@, creating new window with \"_blank\" name.  New window will not be named until 2959902 is fixed.\n", _target.getNSString());
                frame = [bridge descendantFrameNamed:@"_blank"];
        }
    }
    
    [frame loadURL:clickedURL.getNSURL()];
}

bool KWQKHTMLPartImpl::requestFrame( khtml::RenderPart *frame, const QString &url, const QString &frameName,
                                     const QStringList &params, bool isIFrame )
{
    NSURL *childURL = part->completeURL(url).getNSURL();
    if (childURL == nil) {
        NSLog (@"ERROR (probably need to fix CFURL): unable to create URL with path (base URL %s, relative URL %s)", m_baseURL.prettyURL().ascii(), url.ascii());
        return false;
    }
    
    KWQDEBUGLEVEL(KWQ_LOG_FRAMES, "name %s\n", frameName.ascii());
    
    HTMLIFrameElementImpl *o = static_cast<HTMLIFrameElementImpl *>(frame->element());
    if (![bridge createChildFrameNamed:frameName.getNSString() withURL:childURL
            renderPart:frame allowsScrolling:o->scrollingMode() != QScrollView::AlwaysOff
            marginWidth:o->getMarginWidth() marginHeight:o->getMarginHeight()]) {
        return false;
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

bool KWQKHTMLPartImpl::requestObject(khtml::RenderPart *frame, const QString &url, const QString &serviceType, const QStringList &args)
{
    if (url.isEmpty()) {
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
        viewForPluginWithURL:part->completeURL(url).getNSURL()
                 serviceType:serviceType.getNSString()
                   arguments:argsArray
                     baseURL:m_baseURL.getNSURL()]);
    frame->setWidget(widget);
    
    return true;
}

void KWQKHTMLPartImpl::submitForm( const char *action, const QString &url, const QByteArray &formData, const QString &_target, const QString& contentType, const QString& boundary )
{
  QString target = _target;
  
  //if ( target.isEmpty() )
  //  target = d->m_baseTarget;

  KURL u = part->completeURL( url );

  if ( u.isMalformed() )
  {
    // ### ERROR HANDLING!
    return;
  }

  QString urlstring = u.url();

  if ( urlstring.find( QString::fromLatin1( "javascript:" ), 0, false ) == 0 ) {
    urlstring = KURL::decode_string(urlstring);
    part->executeScript( urlstring.right( urlstring.length() - 11) );
    return;
  }

#ifdef NEED_THIS
  if (!checkLinkSecurity(u,
			 i18n( "<qt>The form will be submitted to <BR><B>%1</B><BR>on your local filesystem.<BR>Do you want to submit the form?" ),
			 i18n( "Submit" )))
    return;
#endif

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
    [[bridge frame] loadURL:u.getNSURL()];

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
    [[bridge frame] postWithURL:u.getNSURL() data:postData];
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

bool KWQKHTMLPartImpl::frameExists( const QString &frameName )
{
    return [bridge frameNamed:frameName.getNSString()] != nil;
}

KHTMLPart *KWQKHTMLPartImpl::findFrame(const QString &frameName)
{
    return [[[bridge frameNamed:frameName.getNSString()] committedBridge] part];
}

QPtrList<KParts::ReadOnlyPart> KWQKHTMLPartImpl::frames() const
{
    QPtrList<KParts::ReadOnlyPart> parts;
    NSEnumerator *e = [[bridge childFrames] objectEnumerator];
    WebCoreFrameBridge *childFrame;
    while ((childFrame = [e nextObject])) {
        KHTMLPart *childPart = [[childFrame committedBridge] part];
        if (childPart)
            parts.append(childPart);
    }
    return parts;
}

QString KWQKHTMLPartImpl::documentSource() const
{
    return m_documentSource;
}

void KWQKHTMLPartImpl::setBaseURL(const KURL &url)
{
    m_baseURL = url;
    if (m_baseURL.protocol().startsWith( "http" ) && !m_baseURL.host().isEmpty() && m_baseURL.path().isEmpty()) {
        m_baseURL.setPath( "/" );
    }
    // communicate the change in base URL to the document so that links and subloads work
    if (d->m_doc) {
        d->m_doc->setBaseURL(url.url());
    }
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


KHTMLPart *KWQKHTMLPartImpl::parentPart()
{
    return [[bridge parent] part];
}

bool KWQKHTMLPartImpl::openedByJS()
{
    return [bridge openedByScript];
}

void KWQKHTMLPartImpl::setOpenedByJS(bool _openedByJS)
{
    [bridge setOpenedByScript:_openedByJS];
}

void KWQKHTMLPartImpl::scheduleClose()
{
    [[bridge window] performSelector:@selector(close) withObject:nil afterDelay:0.0];
}

void KWQKHTMLPartImpl::unfocusWindow()
{
    [bridge unfocusWindow];
}


void KWQKHTMLPartImpl::overURL( const QString &url, const QString &target, int modifierState)
{
  if (url.isEmpty()) {
      setStatusBarText(QString());
      return;
  }

  NSString *message;

  if (url.find(QString::fromLatin1("javascript:"), 0, false) != -1) {
      // FIXME: Is it worthwhile to special-case scripts that do a
      // window.open and nothing else?
      
      NSString *script = url.mid(url.find("javascript:", 0, false) + strlen("javascript:")).getNSString();
      // FIXME: should use curly quotes
      message = [NSString stringWithFormat:@"Run script \"%@\"", script];

      setStatusBarText(QString::fromNSString(message));
      return;
  }

  KURL u = part->completeURL(url);

  if (u.protocol() == QString("mailto")) {
      // FIXME: addressbook integration? probably not worth it...
      
      setStatusBarText(QString::fromNSString([NSString stringWithFormat:@"Send email to %@", KURL::decode_string(u.path()).getNSString()]));
      return;
  }

  NSString *format;

  if (target == QString("_blank")) {
      // FIXME: should use curly quotes
      format = @"Open \"%@\" in a new window";
      
  } else if (!target.isEmpty() &&
             (target != QString("_top")) &&
             (target != QString("_self")) &&
             (target != QString("_parent"))) {
      if (frameExists(target)) {
	  // FIXME: distinguish existing frame in same window from
	  // existing frame name for other window
	  // FIXME: should use curly quotes
          format = @"Go to \"%@\" in another frame";
      } else {
	  // FIXME: should use curly quotes
	  format = @"Open \"%@\" in a new window";
      }
  } else {
      format = @"Go to \"%@\"";
  }

  if ([bridge modifierTrackingEnabled]) {
      if (modifierState & MetaButton) {
	  if (modifierState & ShiftButton) {
	      // FIXME: should use curly quotes
	      format = @"Open \"%@\" in a new window, behind the current window";
	  } else {
	      // FIXME: should use curly quotes
	      format = @"Open \"%@\" in a new window";
	  }
      } else if (modifierState & AltButton) {
	  // FIXME: should use curly quotes
	  format = @"Download \"%@\"";
      }
  }
  
  setStatusBarText(QString::fromNSString([NSString stringWithFormat:format, url.getNSString()]));
}
