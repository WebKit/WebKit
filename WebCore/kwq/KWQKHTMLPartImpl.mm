// -*- c-basic-offset: 2 -*-
/* This file is part of the KDE project
 *
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/*
 * Copyright (C) 2001-2002 Apple Computer, Inc.  All rights reserved.
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

#include <decoder.h>
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
#include <html/html_documentimpl.h>
#include <rendering/render_image.h>
#include <loader.h>
#include <kjs/interpreter.h>
#include <kjs_dom.h>
#include <dom_doc.h>
#include <qcursor.h>
#include <kurl.h>

#include <KWQKHTMLPart.h>

#import <WCPluginWidget.h>

#include <rendering/render_frames.h>

#import <KWQView.h>

#include <WCError.h>
#include <WCWebDataSource.h>

#include <loader.h>

#include <external.h>



WCIFWebDataSourceMakeFunc WCIFWebDataSourceMake;
void WCSetIFWebDataSourceMakeFunc(WCIFWebDataSourceMakeFunc func)
{
    WCIFWebDataSourceMake = func;
}
WCIFErrorMakeFunc WCIFErrorMake;
void WCSetIFErrorMakeFunc(WCIFErrorMakeFunc func)
{
    WCIFErrorMake = func;
}


static bool cache_init = false;

static void recursive(const DOM::Node &pNode, const DOM::Node &node)
{
    DOM::Node cur_child = node.lastChild();

    KWQDEBUG2("cur_child: %s = %s", cur_child.nodeName().string().latin1(), cur_child.nodeValue().string().latin1());

    while(!cur_child.isNull())
    {
        recursive(node, cur_child);
        cur_child = cur_child.previousSibling();
    }
}


// Class KHTMLPartPrivate ================================================================================

class KHTMLPartPrivate
{
public:
    KHTMLView *m_view;
    
    DOM::DocumentImpl *m_doc;
    khtml::Decoder *m_decoder;

    QString m_encoding;
    QFont::CharSet m_charset;
    KHTMLSettings *m_settings;
    
    KURL m_workingURL;
    KURL m_url;
    KURL m_baseURL;
    
    KHTMLPart *m_part;
    id m_handle;

    bool m_bFirstData:1;
    bool m_haveEncoding:1;
    bool m_haveCharset:1;
    bool m_onlyLocalReferences:1;
    bool m_startBeforeEnd:1;
    
    bool m_decodingStarted:1;

    KJSProxy *m_jscript;
    int m_runningScripts;

    QString m_strSelectedURL;
    QString m_referrer;
    QString m_documentSource;

    bool m_bMousePressed;
    DOM::Node m_mousePressNode; //node under the mouse when the mouse was pressed (set in the mouse handler)
    DOM::Node m_selectionStart;
    long m_startOffset;
    DOM::Node m_selectionEnd;
    long m_endOffset;
    QString m_overURL;
   
    QPoint m_dragStartPos;
#ifdef KHTML_NO_SELECTION
    QPoint m_dragLastPos;
#endif

#ifdef _KWQ_TIMING        
    double totalWriteTime;
#endif

    KHTMLPartPrivate(KHTMLPart *part)
    {
        if (!cache_init) {
            khtml::Cache::init();
            cache_init = true;
        }
        m_part = part;
        m_view = 0L;
        
        // Why is this allocated here?
        m_doc = NULL;
        
        m_decoder = 0L;
        m_bFirstData = true;
        //m_settings = new KHTMLSettings(*KHTMLFactory::defaultHTMLSettings());
        m_settings = new KHTMLSettings();
        m_haveEncoding = false;
        m_handle = nil;
        
        m_jscript = 0L;
        m_runningScripts = 0;
        m_onlyLocalReferences = 0;
        
        m_documentSource = "";
        m_decodingStarted = 0;
    }

    ~KHTMLPartPrivate()
    {
        delete m_settings;
        if (m_decoder)
            delete m_decoder;
    }

};

static QString splitUrlTarget(const QString &url, QString *target=0)
{
   QString result = url;
   if(url.left(7) == "target:")
   {
      KURL u(url);
      result = u.ref();
      if (target)
         *target = u.host();
   }
   return result;
}

KHTMLPart::KHTMLPart()
{
    init();
}

KHTMLPart::KHTMLPart(const KURL &url)
{
    init();
}

void KHTMLPart::init()
{
    d = new KHTMLPartPrivate(this);
}

KHTMLPart::~KHTMLPart()
{
    if ( d->m_doc != NULL ) {
        d->m_doc->detach();

	if (d->m_doc->refCount() != 1) {
	    fprintf (stdout, "Warning:  document reference count not 1 as expected,  ref = %d\n", d->m_doc->refCount());
	}
	d->m_doc->deref();
    }

    d->m_doc = 0;

    delete d;
}

bool KHTMLPart::openURL( const KURL &url )
{
    d->m_workingURL = url;
    d->m_url = url;

    d->m_documentSource = "";
    d->m_decodingStarted = 0;
    
    return true;
}

bool KHTMLPart::closeURL()
{
    // Reset the the current working URL to the default URL.
    d->m_workingURL = KURL();

    //d->m_doc = 0;
    
    return true;
}

DOM::HTMLDocument KHTMLPart::htmlDocument() const
{
  if (d->m_doc && d->m_doc->isHTMLDocument())
    return static_cast<HTMLDocumentImpl*>(d->m_doc);
  else
    return static_cast<HTMLDocumentImpl*>(0);
}

DOM::Document KHTMLPart::document() const
{
    return d->m_doc;
}

KParts::BrowserExtension *KHTMLPart::browserExtension() const
{
    _logNeverImplemented();
    return 0L;
}

KHTMLView *KHTMLPart::view() const
{
  return d->m_view;
}

void KHTMLPart::setJScriptEnabled( bool enable )
{
    _logNotYetImplemented();
}

bool KHTMLPart::jScriptEnabled() const
{
    //_logNotYetImplemented();
    return TRUE;
}

bool KHTMLPart::metaRefreshEnabled() const
{
    _logNotYetImplemented();
    return TRUE;
}

KJSProxy *KHTMLPart::jScript()
{
  if ( !d->m_jscript )
  {
    d->m_jscript = kjs_html_init(this);
  }

  return d->m_jscript;
}

QVariant KHTMLPart::executeScript( const QString &script )
{
// DUBIOUS, rather than executing the script this document should be
// passed to the interpreter.
    return executeScript( DOM::Node(), script );
}

QVariant KHTMLPart::executeScript( const DOM::Node &n, const QString &script )
{
    return executeScript(QString::null, 0, n, script);
}

QVariant KHTMLPart::executeScript(QString filename, int baseLine, const DOM::Node &n, const QString &script)
{
// DUBIOUS, rather than executing the script this document should be
// passed to the interpreter.
    KJSProxy *proxy = jScript();
    
    if (!proxy || proxy->paused())
      return QVariant();
    d->m_runningScripts++;
    QVariant ret = proxy->evaluate( filename, baseLine, script, n );
    d->m_runningScripts--;

    // FIXME: implement
#ifndef APPLE_CHANGES
    if (!d->m_runningScripts && d->m_doc && !d->m_doc->parsing() && d->m_submitForm )
	submitFormAgain();
#endif /* not APPLE_CHANGES */
    
    DocumentImpl::updateDocumentsRendering();
    
    //kdDebug(6050) << "KHTMLPart::executeScript - done" << endl;
    return ret;
}

QVariant KHTMLPart::executeScheduledScript()
{
    _logNeverImplemented();
    return QVariant();
}

void KHTMLPart::setJavaEnabled( bool enable )
{
    _logNotYetImplemented();
}

bool KHTMLPart::javaEnabled() const
{
    return [[[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitJavaEnabled"] boolValue];
}

KJavaAppletContext *KHTMLPart::javaContext()
{
    _logNotYetImplemented();
    return 0L;
}

KJavaAppletContext *KHTMLPart::createJavaContext()
{
    _logNotYetImplemented();
    return 0L;
}

void KHTMLPart::setPluginsEnabled( bool enable )
{
    _logNotYetImplemented();
}

bool KHTMLPart::pluginsEnabled() const
{
    return [[[NSUserDefaults standardUserDefaults] objectForKey:@"WebKitPluginsEnabled"] boolValue];
}

void KHTMLPart::setAutoloadImages( bool enable )
{
    _logNeverImplemented();
}

bool KHTMLPart::autoloadImages() const
{
    _logNeverImplemented();
    return FALSE;
}

DOM::HTMLDocumentImpl *KHTMLPart::docImpl() const
{
    _logPartiallyImplemented();
    return dynamic_cast<DOM::HTMLDocumentImpl *>(d->m_doc);
}

DOM::DocumentImpl *KHTMLPart::xmlDocImpl() const
{
    if ( d )
        return d->m_doc;
    return 0;
}

void KHTMLPart::slotData(id handle, const char *bytes, int length)
{
// NOTE: This code emulates the interface used by the original khtml part  
    NSString *encoding;
    QString enc;

    if (!d->m_workingURL.isEmpty()) {
        //begin(d->m_workingURL, d->m_extension->urlArgs().xOffset, d->m_extension->urlArgs().yOffset);
        begin(d->m_workingURL, 0, 0);

	//d->m_doc->docLoader()->setReloading(d->m_bReloading);
        d->m_workingURL = KURL();
    }

    encoding = [handle characterSet];
    if (encoding != NULL) {
      enc = QString::fromCFString((CFStringRef) encoding);
      setEncoding(enc, true);
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
  d->m_url = url;
  KURL baseurl;

  // ### not sure if XHTML documents served as text/xml should use DocumentImpl or HTMLDocumentImpl
  if (args.serviceType == "text/xml")
    d->m_doc = DOMImplementationImpl::instance()->createDocument( d->m_view );
  else
    d->m_doc = DOMImplementationImpl::instance()->createHTMLDocument( d->m_view );

    //DomShared::instanceToCheck = (void *)((DomShared *)d->m_doc);
    d->m_doc->ref();

    d->m_baseURL = KURL();
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

    if (!d->m_workingURL.isEmpty())
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

    d->totalWriteTime = 0;
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
	jScript()->appendSourceFile(d->m_url.url(),decoded);
    Tokenizer* t = d->m_doc->tokenizer();
    if(t)
        t->write( decoded, true );

#ifdef _KWQ_TIMING        
    double thisTime = CFAbsoluteTimeGetCurrent() - start;
    d->totalWriteTime += thisTime;
    KWQDEBUGLEVEL4 (0x200, "%s bytes = %d, seconds = %f, total = %f\n", d->m_url.url().latin1(), len, thisTime, d->totalWriteTime);
#endif
}

void KHTMLPart::write( const QString &str )
{
  if ( str.isNull() )
    return;

  if(d->m_bFirstData) {
      // determine the parse mode
      d->m_doc->setParseMode( DocumentImpl::Strict );
      d->m_bFirstData = false;
  }
  if (jScript())
    jScript()->appendSourceFile(d->m_url.url(),str);
  Tokenizer* t = d->m_doc->tokenizer();
  if(t)
    t->write( str, true );
}

void KHTMLPart::end()
{
#ifndef APPLE_CHANGES
    KWQDEBUG2 ("0x%08x end(): for url %s\n", (unsigned int)this, d->m_url.url().latin1());
    // make sure nothing's left in there...
    if(d->m_decoder)
        write(d->m_decoder->flush());
    d->m_doc->finishParsing();
#endif /* not APPLE_CHANGES */

    //QString str = d->m_doc->recursive_toHTML(1);
    
    d->m_doc->close();
    KURL::clearCaches();
}

KHTMLSettings *KHTMLPart::settings()
{
  return d->m_settings;
}

KURL KHTMLPart::completeURL(const QString &url)
{
  if ( !d->m_doc ) return url;

  if (d->m_decoder)
    return KURL(d->m_doc->completeURL(url), d->m_decoder->codec()->mibEnum());

  return KURL( d->m_doc->completeURL( url ) );
}

void KHTMLPart::scheduleRedirection( int delay, const QString &url )
{
    _logNeverImplemented();
}

bool KHTMLPart::setCharset( const QString &name, bool override = false )
{
    _logNeverImplemented();
    return FALSE;
}

bool KHTMLPart::setEncoding( const QString &name, bool override )
{
    d->m_encoding = name;
    d->m_haveEncoding = override;

    _logPartiallyImplemented();
    return false;
#ifndef APPLE_CHANGES
/* FIXME: do we need any of these bits? */
//    setCharset( name, override );
     d->m_charset = KGlobal::charsets()->charsetForEncoding(name);
     d->m_settings->setCharset( d->m_charset );
     // the script should not be unicode. We need to know the document is eg. arabic to be
     // able to choose a unicode font that contains arabic glyphs.
     d->m_settings->setScript( KGlobal::charsets()->charsetForEncoding( name, true ) );

    if( !m_url.isEmpty() ) {
        // reload document
        closeURL();
        KURL url = m_url;
        m_url = 0;
        openURL(url);
    }

    return true;
#endif /* not APPLE_CHANGES */
}

QString KHTMLPart::encoding()
{
    _logNeverImplemented();
    return d->m_settings->encoding();
}

void KHTMLPart::setUserStyleSheet(const KURL &url)
{
    _logNeverImplemented();
}

void KHTMLPart::setUserStyleSheet(const QString &styleSheet)
{
    _logNeverImplemented();
}

bool KHTMLPart::gotoAnchor( const QString &name )
{
// DUBIOUS, this should be handled by the view, also isn't the anchor a node?
    _logNeverImplemented();
    return FALSE;
}

void KHTMLPart::setFontSizes( const QValueList<int> &newFontSizes )
{
    _logNeverImplemented();
}

QValueList<int> KHTMLPart::fontSizes() const
{
  return d->m_settings->fontSizes();
}

void KHTMLPart::resetFontSizes()
{
    _logNeverImplemented();
}

void KHTMLPart::setStandardFont( const QString &name )
{
    _logNeverImplemented();
}

void KHTMLPart::setFixedFont( const QString &name )
{
    _logNeverImplemented();
}

void KHTMLPart::setURLCursor( const QCursor &c )
{
    _logNeverImplemented();
}

// FIXME: this should be removed
static const QCursor *staticURLCursor = NULL;

const QCursor& KHTMLPart::urlCursor() const
{
    _logNeverImplemented();
    if (staticURLCursor == NULL) {
        staticURLCursor = new QCursor();
    }
    return *staticURLCursor;
}

bool KHTMLPart::onlyLocalReferences() const
{
  return d->m_onlyLocalReferences;
}

void KHTMLPart::setOnlyLocalReferences(bool enable)
{
  d->m_onlyLocalReferences = enable;
}

void KHTMLPart::findTextBegin()
{
// DUBIOUS, perhaps searching should be handled externally
    _logNeverImplemented();
}

bool KHTMLPart::findTextNext( const QRegExp &exp, bool forward )
{
// DUBIOUS, perhaps searching should be handled externally
    _logNeverImplemented();
    return FALSE;
}

bool KHTMLPart::findTextNext( const QString &str, bool forward, bool caseSensitive )
{
// DUBIOUS, perhaps searching should be handled externally
    _logNeverImplemented();
    return FALSE;
}

QString KHTMLPart::selectedText() const
{
// DUBIOUS, perhaps selection should be managed externally
    _logNeverImplemented();
    return QString();
}

bool KHTMLPart::hasSelection() const
{
// DUBIOUS, perhaps selection should be managed externally
    _logNeverImplemented();
    return FALSE;
}

DOM::Range KHTMLPart::selection() const
{
// DUBIOUS, perhaps selection should be managed externally
    _logNeverImplemented();
    return DOM::Range();
}

void KHTMLPart::setSelection( const DOM::Range & )
{
// DUBIOUS, perhaps selection should be managed externally
    _logNeverImplemented();
}

void KHTMLPart::overURL( const QString &url, const QString &target)
{
    _logNeverImplemented();
}

void KHTMLPart::urlSelected( const QString &url, int button, int state, const QString &_target)
{
    _logNeverImplemented();
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
        NSString *urlString;
        
        urlString = QSTRING_TO_NSSTRING (completeURL( url ).url() );
        childURL = [NSURL URLWithString: QSTRING_TO_NSSTRING (completeURL( url ).url() )];
        
        if (childURL == nil){
            NSLog (@"ERROR (probably need to fix CFURL):  unable to create URL for %@\n", urlString);
            return false;
        }
        
        oldDataSource = getDataSource();
        controller = [oldDataSource controller];
        newFrame = [controller createFrameNamed: nsframeName for: nil inParent: oldDataSource];
        if (newFrame == nil){
            // Controller return NO to location change, now what?
            return false;
        }
        [newFrame _setRenderFramePart: frame];
        
        newDataSource = WCIFWebDataSourceMake(childURL);
        [newDataSource _setParent: oldDataSource];
        [newFrame setProvisionalDataSource: newDataSource];
    
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

static int frameNameId = 1;

QString KHTMLPart::requestFrameName()
{
    return QString::fromLatin1("<!--frame %1-->").arg(frameNameId++);
}

bool KHTMLPart::requestObject( khtml::RenderPart *frame, const QString &url, const QString &serviceType,
			       const QStringList &args )
{
  if(url.isEmpty()){
    return FALSE;
  }
  // requestObject can be called multiple times for a single plug-in.
  // The plugins array is an attempt to avoid multiple creations of the same plug-in.
  // FIXME: Can't have multiple plug-ins with the same URL on a page
  if(!plugins.contains(url)) {
    WCPluginWidget *pluginWidget = new WCPluginWidget(completeURL(url).url(), serviceType, args);
    frame->setWidget(pluginWidget);
    plugins.append(url);
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
    NSString *urlString = [NSString stringWithCString:u.url().latin1()];
    NSURL *qurl = [NSURL URLWithString: urlString];
    IFWebFrame *frame;
    
    oldDataSource = getDataSource();
    frame = [oldDataSource frame];
    
    newDataSource = WCIFWebDataSourceMake(qurl);
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
    _logNeverImplemented();
    return FALSE;
}

KHTMLPart *KHTMLPart::parentPart()
{
    _logNeverImplemented();
    return 0L;
}

void KHTMLPart::setJSStatusBarText( const QString &text )
{
    _logNeverImplemented();
}

void KHTMLPart::setJSDefaultStatusBarText( const QString &text )
{
    _logNeverImplemented();
}

QString KHTMLPart::referrer() const
{
     _logNeverImplemented();
     return QString();
}

QString KHTMLPart::lastModified() const
{
     _logNeverImplemented();
     return QString();
}

QString KHTMLPart::jsStatusBarText() const
{
    _logNeverImplemented();
    return QString();
}

QString KHTMLPart::jsDefaultStatusBarText() const
{
    _logNeverImplemented();
    return QString();
}

const QPtrList<KParts::ReadOnlyPart> KHTMLPart::frames() const

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

  if ( !event->url().isNull() )
    d->m_strSelectedURL = event->url().string();
  else
    d->m_strSelectedURL = QString::null;

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
    //popupMenu( splitUrlTarget(d->m_strSelectedURL) );
    d->m_strSelectedURL = QString::null;
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
    d->m_strSelectedURL = "";
    return;
  }
#endif

  QString target;
  QString surl = splitUrlTarget(url.string(), &target);

  // Not clicked -> mouse over stuff
  if ( !d->m_bMousePressed )
  {
    // The mouse is over something
    if ( url.length() )
    {
      // FIXME: this is unused
      //bool shiftPressed = ( _mouse->state() & ShiftButton );

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
            // FIXME: when compiling with -02, compilation breaks with a 
	    // complaint that vx and vy might be uninitialized before use
            r->absolutePosition(absx, absy);
            //view()->contentsToViewport( absx, absy, vx, vy );

            int x(_mouse->x() - vx), y(_mouse->y() - vy);

            d->m_overURL = surl + QString("?%1,%2").arg(x).arg(y);
            //overURL( d->m_overURL, target, shiftPressed );
            return;
          }
        }
      }

      // normal link
      QString target;
      QString surl = splitUrlTarget(url.string(), &target);
      if ( d->m_overURL.isEmpty() || d->m_overURL != surl )
      {
        d->m_overURL = surl;
        //overURL( d->m_overURL, target, shiftPressed );
      }
    }
    else  // Not over a link...
    {
      if( !d->m_overURL.isEmpty() ) // and we were over a link  -> reset to "default statusbar text"
      {
        d->m_overURL = QString::null;
        //emit onURL( QString::null );
        // Default statusbar text can be set from javascript. Otherwise it's empty.
        //emit setStatusBarText( d->m_kjsDefaultStatusBarText );
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
    }
#else
      if ( d->m_doc && d->m_view ) {
        QPoint diff( _mouse->globalPos() - d->m_dragLastPos );

        if ( abs( diff.x() ) > 64 || abs( diff.y() ) > 64 ) {
          d->m_view->scrollBy( -diff.x(), -diff.y() );
          d->m_dragLastPos = _mouse->globalPos();
        }
      }
#endif
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
        IFWebDataSource *oldDataSource, *newDataSource;
        KURL clickedURL(completeURL( splitUrlTarget(d->m_strSelectedURL)));
        NSString *urlString = [NSString stringWithCString:clickedURL.url().latin1()];
        NSURL *url = [NSURL URLWithString: urlString];
        IFWebFrame *frame;
        
        oldDataSource = getDataSource();
        frame = [oldDataSource frame];
        
        newDataSource = WCIFWebDataSourceMake(url);
        [newDataSource _setParent: [oldDataSource parent]];
        
        [frame setProvisionalDataSource: newDataSource];
        [frame startLoading];
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

void KHTMLPart::selectAll()
{
// DUBIOUS, perhaps selection should be managed externally
    _logNeverImplemented();
}

DOM::EventListener *KHTMLPart::createHTMLEventListener( QString code )
{
    _logNeverImplemented();
    return 0L;
}

KHTMLPart *KHTMLPart::opener()
{
    _logNeverImplemented();
    return 0L;
}

void KHTMLPart::setOpener(KHTMLPart *_opener)
{
    _logNeverImplemented();
}

bool KHTMLPart::openedByJS()
{
    _logNeverImplemented();
    return FALSE;
}

void KHTMLPart::setOpenedByJS(bool _openedByJS)
{
    _logNeverImplemented();
}

QString KHTMLPart::documentSource()
{
    return d->m_documentSource;
}

void KHTMLPart::setBaseURL(const KURL &url)
{
    d->m_baseURL = url;
    if (d->m_baseURL.protocol().startsWith( "http" ) && !d->m_baseURL.host().isEmpty() && d->m_baseURL.path().isEmpty()) {
        d->m_baseURL.setPath( "/" );
    }
}

KURL KHTMLPart::baseURL() const
{
    if (d->m_baseURL.isEmpty()) {
        return d->m_workingURL;
    }
    return d->m_baseURL;
}

const KURL & KHTMLPart::url() const
{
    return d->m_workingURL;
}

void KHTMLPart::setView(KHTMLView *view)
{
    d->m_view = view;
}

QWidget *KHTMLPart::widget()
{
    _logNotYetImplemented();
    return 0L;
}


void KHTMLPart::emitUnloadEvent()
{
    _logNeverImplemented();
}

void KHTMLPart::nodeActivated(const DOM::Node &aNode)
{
    KWQDEBUG2 ("name %s = %s\n", (const char *)aNode.nodeName().string(), (const char *)aNode.nodeValue().string());
}

void KHTMLPart::stopAutoScroll()
{
    _logNeverImplemented();
}
