/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#include <Foundation/Foundation.h>

#include <job.h>
#include <jobclasses.h>
#include <khtml_settings.h>
#include <khtml_factory.h>
#include <kcharsets.h>
#include <kglobal.h>
#include <html/htmltokenizer.h>
#include <xml/dom_docimpl.h>
#include <html/html_documentimpl.h>
#include <loader.h>

#include <KWQKHTMLPart.h>

#import <WCURICache.h>
#import <WCURICacheData.h>

static bool cache_init = false;

static void recursive(const DOM::Node &pNode, const DOM::Node &node)
{
    DOM::Node cur_child = node.lastChild();

    NSLog(@"cur_child: %s = %s", cur_child.nodeName().string().latin1(), cur_child.nodeValue().string().latin1());

    while(!cur_child.isNull())
    {
        recursive(node, cur_child);
        cur_child = cur_child.previousSibling();
    }
}

// Class KHTMLPartNotificationReceiver ==============================================================

@interface KHTMLPartNotificationReceiver : NSObject
{
    @public
    KHTMLPart *m_part;
}
@end

@implementation KHTMLPartNotificationReceiver

-(void)cacheDataAvailable:(NSNotification *)notification
{
    m_part->slotData([notification object]);
}

-(void)cacheFinished:(NSNotification *)notification
{
    // FIXME: need an implementation for this
    m_part->closeURL();
}

@end


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
    
    KHTMLPart *m_part;
    KHTMLPartNotificationReceiver *m_recv;

    bool m_bFirstData:1;
    bool m_haveEncoding:1;
    bool m_haveCharset:1;

    KHTMLPartPrivate(KHTMLPart *part)
    {
        if (!cache_init) {
            khtml::Cache::init();
            cache_init = true;
        }
        m_part = part;
        m_view = 0L;
        m_doc = new HTMLDocumentImpl();
        m_decoder = 0L;
        m_bFirstData = true;
        //m_settings = new KHTMLSettings(*KHTMLFactory::defaultHTMLSettings());
        m_settings = new KHTMLSettings();
        m_haveEncoding = false;
        m_recv = [[KHTMLPartNotificationReceiver alloc] init];
        m_recv->m_part = part;
    }

    ~KHTMLPartPrivate()
    {
        [m_recv autorelease];   
    }

};


// Class KHTMLPart ======================================================================================

KHTMLPart::KHTMLPart()
{
    init();
}


KHTMLPart::KHTMLPart(const KURL &url )
{
    init();
}

void KHTMLPart::init()
{
    d = new KHTMLPartPrivate(this);
}


KHTMLPart::~KHTMLPart()
{
    delete d;
    _logNotYetImplemented();
}

// NOTE: This code emulates the interface used by the original khtml part  
void KHTMLPart::slotData(id <WCURICacheData> data) 
{
    if (!d->m_workingURL.isEmpty()) {
        //begin(d->m_workingURL, d->m_extension->urlArgs().xOffset, d->m_extension->urlArgs().yOffset);
        begin(d->m_workingURL, 0, 0);
        d->m_workingURL = KURL();
    }
          
    write((const char *)[data cacheData], [data cacheDataSize]);    
}

bool KHTMLPart::openURL( const KURL &url )
{
    // Close the previous URL.
    closeURL();
    
    //if ( args.doPost() && (url.protocol().startsWith("http")) )
    //{
    //    d->m_job = KIO::http_post( url, args.postData, false );
    //    d->m_job->addMetaData("content-type", args.contentType() );
    //}
    //else
    //    d->m_job = KIO::get( url, args.reload, false );
        
    //connect( d->m_job, SIGNAL( result( KIO::Job * ) ),
    //        SLOT( slotFinished( KIO::Job * ) ) );
    //connect( d->m_job, SIGNAL( data( KIO::Job*, const QByteArray &)),
    //        SLOT( slotData( KIO::Job*, const QByteArray &)));
    //connect( d->m_job, SIGNAL(redirection(KIO::Job*, const KURL&) ),
    //        SLOT( slotRedirection(KIO::Job*,const KURL&) ) );
    
    // Initiate request for URL data.
    //d->m_job = KIO::get( url, false, false );
    //d->m_job = KIO::get( url, false, false );
    
    // Keep a reference to the current working URL.
    d->m_workingURL = url;

    id <WCURICache> cache;
    NSString *nsurl;
    
    cache = WCGetDefaultURICache();
    nsurl = [NSString stringWithCString:url.url().latin1()];
    
    id jobID = [cache requestWithString:nsurl requestor:d->m_recv userData:nil];
    
    return true;
}

bool KHTMLPart::closeURL()
{
    //if (d && d->m_doc) {
    //    recursive(0, d->m_doc);
    //}

    // Cancel any pending loads.
    
    // Reset the the current working URL to the default URL.
    d->m_workingURL = KURL();
}


DOM::HTMLDocument KHTMLPart::htmlDocument() const
{
    _logNotYetImplemented();
}


DOM::Document KHTMLPart::document() const
{
    _logNotYetImplemented();
}


void KHTMLPart::setJScriptEnabled( bool enable )
{
    _logNotYetImplemented();
}


bool KHTMLPart::jScriptEnabled() const
{
    _logNotYetImplemented();
}


void KHTMLPart::enableMetaRefresh( bool enable )
{
    _logNotYetImplemented();
}


bool KHTMLPart::metaRefreshEnabled() const
{
    _logNotYetImplemented();
}


// DUBIOUS, rather than executing the script this document should be
// passed to the interpreter.
QVariant KHTMLPart::executeScript( const QString &script )
{
    _logNotYetImplemented();
}


// DUBIOUS, rather than executing the script this document should be
// passed to the interpreter.
QVariant KHTMLPart::executeScript( const DOM::Node &n, const QString &script )
{
    _logNotYetImplemented();
}


void KHTMLPart::setJavaEnabled( bool enable )
{
    _logNotYetImplemented();
}


bool KHTMLPart::javaEnabled() const
{
    _logNotYetImplemented();
}


KJavaAppletContext *KHTMLPart::javaContext()
{
    _logNotYetImplemented();
}


KJavaAppletContext *KHTMLPart::createJavaContext()
{
    _logNotYetImplemented();
}


void KHTMLPart::setPluginsEnabled( bool enable )
{
    _logNotYetImplemented();
}


bool KHTMLPart::pluginsEnabled() const
{
    _logNeverImplemented();
}


void KHTMLPart::setAutoloadImages( bool enable )
{
    _logNeverImplemented();
}


bool KHTMLPart::autoloadImages() const
{
    _logNeverImplemented();
}


void KHTMLPart::setOnlyLocalReferences(bool enable)
{
    _logNeverImplemented();
}


bool KHTMLPart::onlyLocalReferences() const
{
    _logNeverImplemented();
}


void KHTMLPart::begin( const KURL &url, int xOffset, int yOffset)
{
    //d->m_referrer = url.url();
    
    d->m_doc = new HTMLDocumentImpl(d->m_view);
    d->m_doc->ref();
    //FIXME: do we need this? 
    d->m_doc->attach( d->m_view );
    d->m_doc->setURL( url.url() );

    /* FIXME: we'll need to make this work....
    QString userStyleSheet = KHTMLFactory::defaultHTMLSettings()->userStyleSheet();
    if ( !userStyleSheet.isEmpty() ) {
        setUserStyleSheet( KURL( userStyleSheet ) );
    }
    */
    
    d->m_doc->open();    

    _logNotYetImplemented();
}


void KHTMLPart::write( const char *str, int len)
{
    /* 
    FIXME: hook this code back when we have decoders working
    
    if ( !d->m_decoder ) {
        d->m_decoder = new khtml::Decoder();
        if(d->m_encoding != QString::null)
            d->m_decoder->setEncoding(d->m_encoding.latin1(), d->m_haveEncoding);
        else {
            //FIXME: d->m_decoder->setEncoding(settings()->encoding().latin1(), d->m_haveEncoding);
        }
    }
    if ( len == 0 )
        return;
    
    if ( len == -1 )
        len = strlen( str );
    
    QString decoded = d->m_decoder->decode( str, len );
        
    
    if(decoded.isEmpty())
        return;
    
    if(d->m_bFirstData) {
        // determine the parse mode
        d->m_doc->determineParseMode( decoded );
        d->m_bFirstData = false;
    
        //kdDebug(6050) << "KHTMLPart::write haveEnc = " << d->m_haveEncoding << endl;
        // ### this is still quite hacky, but should work a lot better than the old solution
        if(d->m_decoder->visuallyOrdered())
            d->m_doc->setVisuallyOrdered();
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

    // End FIXME 
    */

    // begin lines added in lieu of big fixme
    
    QString decoded = QString(str);
    d->m_doc->applyChanges(true, true);
    
    // end lines added in lieu of big fixme
    
    Tokenizer* t = d->m_doc->tokenizer();
    if(t)
        t->write( decoded, true );
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
    Tokenizer* t = d->m_doc->tokenizer();
    if(t)
        t->write( str, true );
}


void KHTMLPart::end()
{
    // make sure nothing's left in there...
    if(d->m_decoder)
        write(d->m_decoder->flush());
    d->m_doc->finishParsing();
}


void KHTMLPart::setBaseURL( const KURL &url )
{
    _logNeverImplemented();
}


KURL KHTMLPart::baseURL() const
{
    if (d->m_workingURL.isEmpty()) {
        return KURL();
    }
    return d->m_workingURL;
}


void KHTMLPart::setBaseTarget( const QString &target )
{
    _logNeverImplemented();
}


QString KHTMLPart::baseTarget() const
{
    _logNeverImplemented();
}


bool KHTMLPart::setCharset( const QString &name, bool override = false )
{
    _logNeverImplemented();
}


bool KHTMLPart::setEncoding( const QString &name, bool override = false )
{
    _logNeverImplemented();
}



QString KHTMLPart::encoding()
{
    _logNeverImplemented();
}


void KHTMLPart::setUserStyleSheet(const KURL &url)
{
    _logNeverImplemented();
}


void KHTMLPart::setUserStyleSheet(const QString &styleSheet)
{
    _logNeverImplemented();
}


void KHTMLPart::setFontSizes(const QValueList<int> &newFontSizes )
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


// DUBIOUS, this should be handled by the view, also isn't the anchor a node?
bool KHTMLPart::gotoAnchor( const QString &name )
{
    _logNeverImplemented();
}


void KHTMLPart::setURLCursor( const QCursor &c )
{
    _logNeverImplemented();
}


const QCursor& KHTMLPart::urlCursor() const
{
    _logNeverImplemented();
}


// DUBIOUS, perhaps searching should be handled externally
void KHTMLPart::findTextBegin()
{
    _logNeverImplemented();
}


// DUBIOUS, perhaps searching should be handled externally
bool KHTMLPart::findTextNext( const QRegExp &exp, bool forward )
{
    _logNeverImplemented();
}


// DUBIOUS, perhaps searching should be handled externally
bool KHTMLPart::findTextNext( const QString &str, bool forward, bool caseSensitive )
{
    _logNeverImplemented();
}


// DUBIOUS, perhaps selection should be managed externally
QString KHTMLPart::selectedText() const
{
    _logNeverImplemented();
}


// DUBIOUS, perhaps selection should be managed externally
DOM::Range KHTMLPart::selection() const
{
    _logNeverImplemented();
}


// DUBIOUS, perhaps selection should be managed externally
void KHTMLPart::setSelection( const DOM::Range & )
{
    _logNeverImplemented();
}


// DUBIOUS, perhaps selection should be managed externally
bool KHTMLPart::hasSelection() const
{
    _logNeverImplemented();
}


// DUBIOUS, perhaps selection should be managed externally
void KHTMLPart::selectAll()
{
    _logNeverImplemented();
}


void KHTMLPart::setJSStatusBarText( const QString &text )
{
    _logNeverImplemented();
}


void KHTMLPart::setJSDefaultStatusBarText( const QString &text )
{
    _logNeverImplemented();
}


QString KHTMLPart::jsStatusBarText() const
{
    _logNeverImplemented();
}


QString KHTMLPart::jsDefaultStatusBarText() const
{
    _logNeverImplemented();
}


DOM::HTMLDocumentImpl *KHTMLPart::docImpl() const
{
    _logPartiallyImplemented();
    return dynamic_cast<DOM::HTMLDocumentImpl *>(d->m_doc);
}


DOM::DocumentImpl *KHTMLPart::xmlDocImpl() const
{
    _logPartiallyImplemented();
    return d->m_doc;
}


const KHTMLSettings *KHTMLPart::settings() const
{
    return d->m_settings;
}


KJSProxy *KHTMLPart::jScript()
{
    _logNotYetImplemented();
}


KURL KHTMLPart::completeURL( const QString &url, const QString &target = QString::null )
{
    _logNotYetImplemented();
}


const KURL & KHTMLPart::url() const
{
    return d->m_workingURL;
}


void KHTMLPart::scheduleRedirection( int delay, const QString &url )
{
    _logNeverImplemented();
}


KHTMLView *KHTMLPart::view() const
{
    return d->m_view;
}

void KHTMLPart::setView(KHTMLView *view)
{
    d->m_view = view;
}


QWidget *KHTMLPart::widget()
{
    _logNotYetImplemented();
}


KHTMLPart *KHTMLPart::opener()
{
    _logNeverImplemented();
}


KHTMLPart *KHTMLPart::parentPart()
{
    _logNeverImplemented();
}


const QList<KParts::ReadOnlyPart> KHTMLPart::frames() const
{
    _logNeverImplemented();
}


KHTMLPart *KHTMLPart::findFrame( const QString &f )
{
    _logNeverImplemented();
}


void KHTMLPart::setOpener(KHTMLPart *_opener)
{
    _logNeverImplemented();
}


bool KHTMLPart::openedByJS()
{
    _logNeverImplemented();
}


void KHTMLPart::setOpenedByJS(bool _openedByJS)
{
    _logNeverImplemented();
}


KParts::BrowserExtension *KHTMLPart::browserExtension() const
{
    _logNeverImplemented();
}


DOM::EventListener *KHTMLPart::createHTMLEventListener( QString code )
{
    _logNeverImplemented();
}


QString KHTMLPart::requestFrameName()
{
    _logNeverImplemented();
}


bool KHTMLPart::frameExists( const QString &frameName )
{
    _logNeverImplemented();
}


bool KHTMLPart::requestFrame( khtml::RenderPart *frame, const QString &url, const QString &frameName,
                    const QStringList &args, bool isIFrame)
{
    _logNeverImplemented();
}


void KHTMLPart::emitUnloadEvent()
{
    _logNeverImplemented();
}


void KHTMLPart::submitForm( const char *action, const QString &url, const QByteArray &formData,
                        const QString &target, const QString& contentType,
                        const QString& boundary )
{
    _logNeverImplemented();
}


void KHTMLPart::urlSelected( const QString &url, int button = 0, int state = 0,
                        const QString &_target = QString::null )
{
    _logNeverImplemented();
}


bool KHTMLPart::requestObject( khtml::RenderPart *frame, const QString &url, const QString &serviceType,
                    const QStringList &args)
{
    _logNeverImplemented();
}


void KHTMLPart::nodeActivated(const DOM::Node &)
{
    _logNeverImplemented();
}


QVariant KHTMLPart::executeScheduledScript()
{
    _logNeverImplemented();
}


void KHTMLPart::stopAutoScroll()
{
    _logNeverImplemented();
}


void KHTMLPart::overURL( const QString &url, const QString &target )
{
    _logNeverImplemented();
}

