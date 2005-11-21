/* This file is part of the KDE project
 *
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004 Apple Computer, Inc.
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

#include "config.h"
#include "khtml_part.h"

#define DIRECT_LINKAGE_TO_ECMA
#define QT_NO_CLIPBOARD
#define QT_NO_DRAGANDDROP

#include "khtml_pagecache.h"

#include "css/csshelper.h"
#include "css/cssproperties.h"
#include "css/cssstyleselector.h"
#include "css/css_computedstyle.h"
#include "css/css_valueimpl.h"
#include "dom/dom_string.h"
#include "editing/markup.h"
#include "editing/htmlediting.h"
#include "editing/SelectionController.h"
#include "editing/visible_position.h"
#include "editing/visible_text.h"
#include "editing/visible_units.h"
#include "html/html_documentimpl.h"
#include "html/html_baseimpl.h"
#include "html/html_miscimpl.h"
#include "html/html_imageimpl.h"
#include "html/html_objectimpl.h"
#include "rendering/render_block.h"
#include "rendering/render_text.h"
#include "rendering/render_frames.h"
#include "misc/loader.h"
#include "xml/dom2_eventsimpl.h"
#include "xml/dom2_rangeimpl.h"
#include "xml/EventNames.h"
#include "xml/xml_tokenizer.h"

using namespace DOM;
using namespace HTMLNames;

#include "khtmlview.h"
#include <kparts/partmanager.h>
#include "ecma/kjs_proxy.h"
#include "ecma/xmlhttprequest.h"
#include "khtml_settings.h"

#include <sys/types.h>
#include <assert.h>
#include <unistd.h>

#include <kstandarddirs.h>
#include <kio/job.h>
#include <kio/global.h>
#include <kdebug.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kcharsets.h>
#include <kmessagebox.h>
#include <kstdaction.h>
#include <kfiledialog.h>
#include <ktrader.h>
#include <kdatastream.h>
#include <ktempfile.h>
#include <kglobalsettings.h>
#include <kurldrag.h>
#include <kapplication.h>
#if !defined(QT_NO_DRAGANDDROP)
#include <kmultipledrag.h>
#endif

#include <ksslcertchain.h>
#include <ksslinfodlg.h>

#include <qclipboard.h>
#include <qfile.h>
#include <qmetaobject.h>
#include <qptrlist.h>
#include <private/qucomextra_p.h>

#include "khtmlpart_p.h"

#if !KHTML_NO_CPLUSPLUS_DOM
#include "dom/html_document.h"
#endif

#include <CoreServices/CoreServices.h>

using namespace DOM::EventNames;

using khtml::ApplyStyleCommand;
using khtml::CHARACTER;
using khtml::ChildFrame;
using khtml::Decoder;
using khtml::DocLoader;
using khtml::EAffinity;
using khtml::EditAction;
using khtml::EditCommandPtr;
using khtml::ETextGranularity;
using khtml::FormData;
using khtml::InlineTextBox;
using khtml::isEndOfDocument;
using khtml::isStartOfDocument;
using khtml::PARAGRAPH;
using khtml::plainText;
using khtml::RenderObject;
using khtml::RenderText;
using khtml::RenderLayer;
using khtml::RenderWidget;
using khtml::SelectionController;
using khtml::Tokenizer;
using khtml::TypingCommand;
using khtml::VisiblePosition;
using khtml::WORD;

using KParts::BrowserInterface;

const int CARET_BLINK_FREQUENCY = 500;

namespace khtml {
    class PartStyleSheetLoader : public CachedObjectClient
    {
    public:
        PartStyleSheetLoader(KHTMLPart *part, DOM::DOMString url, DocLoader* dl)
        {
            m_part = part;
            m_cachedSheet = Cache::requestStyleSheet(dl, url );
            if (m_cachedSheet)
		m_cachedSheet->ref( this );
        }
        virtual ~PartStyleSheetLoader()
        {
            if ( m_cachedSheet ) m_cachedSheet->deref(this);
        }
        virtual void setStyleSheet(const DOM::DOMString&, const DOM::DOMString &sheet)
        {
          if ( m_part )
            m_part->setUserStyleSheet( sheet.qstring() );

            delete this;
        }
        QGuardedPtr<KHTMLPart> m_part;
        khtml::CachedCSSStyleSheet *m_cachedSheet;
    };
}

FrameList::Iterator FrameList::find( const QString &name )
{
    Iterator it = begin();
    Iterator e = end();

    for (; it!=e; ++it )
        if ( (*it).m_name==name )
            break;

    return it;
}

KHTMLPart::KHTMLPart( QWidget *parentWidget, const char *widgetname, QObject *parent, const char *name,
                      GUIProfile prof )
: KParts::ReadOnlyPart( parent, name )
{
    d = 0;
    KHTMLFactory::registerPart( this );
    setInstance( KHTMLFactory::instance(), prof == BrowserViewGUI && !parentPart() );
}


void KHTMLPart::init( KHTMLView *view, GUIProfile prof )
{
  AtomicString::init();
  QualifiedName::init();
  EventNames::init();
  HTMLNames::init(); // FIXME: We should make this happen only when HTML is used.
  if ( prof == DefaultGUI )
    setXMLFile( "khtml.rc" );
  else if ( prof == BrowserViewGUI )
    setXMLFile( "khtml_browser.rc" );

  frameCount = 0;

  d = new KHTMLPartPrivate(parent());

  d->m_view = view;
  setWidget( d->m_view );
  
  d->m_extension = new KHTMLPartBrowserExtension( this );
  d->m_hostExtension = new KHTMLPartBrowserHostExtension( this );

  d->m_bSecurityInQuestion = false;
  d->m_bMousePressed = false;

  // The java, javascript, and plugin settings will be set after the settings
  // have been initialized.
  d->m_bJScriptEnabled = true;
  d->m_bJScriptDebugEnabled = true;
  d->m_bJavaEnabled = true;
  d->m_bPluginsEnabled = true;


  connect( khtml::Cache::loader(), SIGNAL( requestStarted( khtml::DocLoader*, khtml::CachedObject* ) ),
           this, SLOT( slotLoaderRequestStarted( khtml::DocLoader*, khtml::CachedObject* ) ) );
  connect( khtml::Cache::loader(), SIGNAL( requestDone( khtml::DocLoader*, khtml::CachedObject *) ),
           this, SLOT( slotLoaderRequestDone( khtml::DocLoader*, khtml::CachedObject *) ) );
  connect( khtml::Cache::loader(), SIGNAL( requestFailed( khtml::DocLoader*, khtml::CachedObject *) ),
           this, SLOT( slotLoaderRequestDone( khtml::DocLoader*, khtml::CachedObject *) ) );


  connect( &d->m_redirectionTimer, SIGNAL( timeout() ),
           this, SLOT( slotRedirect() ) );

  connect(&d->m_lifeSupportTimer, SIGNAL(timeout()), this, SLOT(slotEndLifeSupport()));

}

KHTMLPart::~KHTMLPart()
{
  //kdDebug(6050) << "KHTMLPart::~KHTMLPart " << this << endl;

  stopAutoScroll();
  cancelRedirection();

  if (!d->m_bComplete)
    closeURL();

  disconnect( khtml::Cache::loader(), SIGNAL( requestStarted( khtml::DocLoader*, khtml::CachedObject* ) ),
           this, SLOT( slotLoaderRequestStarted( khtml::DocLoader*, khtml::CachedObject* ) ) );
  disconnect( khtml::Cache::loader(), SIGNAL( requestDone( khtml::DocLoader*, khtml::CachedObject *) ),
           this, SLOT( slotLoaderRequestDone( khtml::DocLoader*, khtml::CachedObject *) ) );
  disconnect( khtml::Cache::loader(), SIGNAL( requestFailed( khtml::DocLoader*, khtml::CachedObject *) ),
           this, SLOT( slotLoaderRequestDone( khtml::DocLoader*, khtml::CachedObject *) ) );

  clear();

  if ( d->m_view )
  {
    d->m_view->hide();
    d->m_view->viewport()->hide();
    d->m_view->m_part = 0;
  }
  
  delete d->m_hostExtension;

  delete d; d = 0;
  KHTMLFactory::deregisterPart( this );
}

bool KHTMLPart::restoreURL( const KURL &url )
{
  kdDebug( 6050 ) << "KHTMLPart::restoreURL " << url.url() << endl;

  cancelRedirection();

  /*
   * That's not a good idea as it will call closeURL() on all
   * child frames, preventing them from further loading. This
   * method gets called from restoreState() in case of a full frameset
   * restoral, and restoreState() calls closeURL() before restoring
   * anyway.
  kdDebug( 6050 ) << "closing old URL" << endl;
  closeURL();
  */

  d->m_bComplete = false;
  d->m_bLoadEventEmitted = false;
  d->m_workingURL = url;

  // set the java(script) flags according to the current host.
  d->m_bJScriptEnabled = d->m_settings->isJavaScriptEnabled(url.host());
  d->m_bJScriptDebugEnabled = d->m_settings->isJavaScriptDebugEnabled();
  d->m_bJavaEnabled = d->m_settings->isJavaEnabled(url.host());
  d->m_bPluginsEnabled = d->m_settings->isPluginsEnabled(url.host());

  m_url = url;


  emit started( 0L );

  return true;
}


bool KHTMLPart::didOpenURL(const KURL &url)
{
  kdDebug( 6050 ) << "KHTMLPart(" << this << ")::openURL " << url.url() << endl;

  if (d->m_scheduledRedirection == locationChangeScheduledDuringLoad) {
    // We're about to get a redirect that happened before the document was
    // created.  This can happen when one frame may change the location of a 
    // sibling.
    return false;
  }
  
  cancelRedirection();
  
  // clear last edit command
  d->m_lastEditCommand = EditCommandPtr();
  KWQ(this)->clearUndoRedoOperations();
  

  KParts::URLArgs args( d->m_extension->urlArgs() );


  if (!d->m_restored)
  {
    kdDebug( 6050 ) << "closing old URL" << endl;
    closeURL();
  }


  if (d->m_restored)
     d->m_cachePolicy = KIO::CC_Cache;
  else if (args.reload)
     d->m_cachePolicy = KIO::CC_Refresh;
  else
     d->m_cachePolicy = KIO::CC_Verify;

  if ( args.doPost() && (url.protocol().startsWith("http")) )
  {
      d->m_job = KIO::http_post( url, args.postData, false );
      d->m_job->addMetaData("content-type", args.contentType() );
  }
  else
  {
      d->m_job = KIO::get( url, false, false );
      d->m_job->addMetaData("cache", KIO::getCacheControlString(d->m_cachePolicy));
  }

  d->m_job->addMetaData(args.metaData());

  connect( d->m_job, SIGNAL( result( KIO::Job * ) ),
           SLOT( slotFinished( KIO::Job * ) ) );

  connect( d->m_job, SIGNAL(redirection(KIO::Job*, const KURL&) ),
           SLOT( slotRedirection(KIO::Job*,const KURL&) ) );

  d->m_bComplete = false;
  d->m_bLoadingMainResource = true;
  d->m_bLoadEventEmitted = false;

  // delete old status bar msg's from kjs (if it _was_ activated on last URL)
  if( d->m_bJScriptEnabled )
  {
     d->m_kjsStatusBarText = QString::null;
     d->m_kjsDefaultStatusBarText = QString::null;
  }

  // set the javascript flags according to the current url
  d->m_bJScriptDebugEnabled = d->m_settings->isJavaScriptDebugEnabled();
  d->m_bJavaEnabled = d->m_settings->isJavaEnabled(url.host());
  d->m_bPluginsEnabled = d->m_settings->isPluginsEnabled(url.host());

  // initializing m_url to the new url breaks relative links when opening such a link after this call and _before_ begin() is called (when the first
  // data arrives) (Simon)
  m_url = url;
  if(m_url.protocol().startsWith( "http" ) && !m_url.host().isEmpty() &&
     m_url.path().isEmpty()) {
    m_url.setPath("/");
    emit d->m_extension->setLocationBarURL( m_url.prettyURL() );
  }
  // copy to m_workingURL after fixing m_url above
  d->m_workingURL = m_url;

  kdDebug( 6050 ) << "KHTMLPart::openURL now (before started) m_url = " << m_url.url() << endl;

  connect( d->m_job, SIGNAL( speed( KIO::Job*, unsigned long ) ),
           this, SLOT( slotJobSpeed( KIO::Job*, unsigned long ) ) );

  connect( d->m_job, SIGNAL( percent( KIO::Job*, unsigned long ) ),
           this, SLOT( slotJobPercent( KIO::Job*, unsigned long ) ) );

  emit started( 0L );

  return true;
}

void KHTMLPart::didExplicitOpen()
{
  d->m_bComplete = false;
  d->m_bLoadEventEmitted = false;
    
  // Prevent window.open(url) -- eg window.open("about:blank") -- from blowing away results
  // from a subsequent window.document.open / window.document.write call. 
  // Cancelling redirection here works for all cases because document.open 
  // implicitly precedes document.write.
  cancelRedirection(); 
}

void KHTMLPart::stopLoading(bool sendUnload)
{
  if (d->m_doc && d->m_doc->tokenizer())
    d->m_doc->tokenizer()->stopParsing();
    
  if ( d->m_job )
  {
    d->m_job->kill();
    d->m_job = 0;
  }

  if (sendUnload) {
    if ( d->m_doc && d->m_doc->isHTMLDocument() ) {
      HTMLDocumentImpl* hdoc = static_cast<HTMLDocumentImpl*>( d->m_doc );
      
      if ( hdoc->body() && d->m_bLoadEventEmitted && !d->m_bUnloadEventEmitted ) {
        hdoc->body()->dispatchWindowEvent( unloadEvent, false, false );
        if ( d->m_doc )
          d->m_doc->updateRendering();
        d->m_bUnloadEventEmitted = true;
      }
    }
    
    if (d->m_doc && !d->m_doc->inPageCache())
      d->m_doc->removeAllEventListenersFromAllNodes();
  }

  d->m_bComplete = true; // to avoid emitting completed() in slotFinishedParsing() (David)
  d->m_bLoadingMainResource = false;
  d->m_bLoadEventEmitted = true; // don't want that one either
  d->m_cachePolicy = KIO::CC_Verify; // Why here?

  if ( d->m_doc && d->m_doc->parsing() )
  {
    kdDebug( 6050 ) << " was still parsing... calling end " << endl;
    slotFinishedParsing();
    d->m_doc->setParsing(false);
  }

  if ( !d->m_workingURL.isEmpty() )
  {
    // Aborted before starting to render
    kdDebug( 6050 ) << "Aborted before starting to render, reverting location bar to " << m_url.prettyURL() << endl;
    emit d->m_extension->setLocationBarURL( m_url.prettyURL() );
  }

  d->m_workingURL = KURL();

  if (DocumentImpl *doc = d->m_doc) {
    if (DocLoader *docLoader = doc->docLoader())
      khtml::Cache::loader()->cancelRequests(docLoader);
    KJS::XMLHttpRequest::cancelRequests(doc);
  }

  // tell all subframes to stop as well
  ConstFrameIt it = d->m_frames.begin();
  ConstFrameIt end = d->m_frames.end();
  for (; it != end; ++it ) {
      KParts::ReadOnlyPart *part = (*it).m_part;
      if (part) {
          KHTMLPart *khtml_part = static_cast<KHTMLPart *>(part);

          if (khtml_part->inherits("KHTMLPart"))
              khtml_part->stopLoading(sendUnload);
          else
              part->closeURL();
      }
  }

  d->m_bPendingChildRedirection = false;

  // Stop any started redirections as well!! (DA)
  cancelRedirection();

#if !KHTML_NO_CPLUSPLUS_DOM
  // null node activated.
  emit nodeActivated(Node());
#endif
}

bool KHTMLPart::closeURL()
{    
  stopLoading(true);

  return true;
}

KParts::BrowserExtension *KHTMLPart::browserExtension() const
{
  return d->m_extension;
}

KHTMLView *KHTMLPart::view() const
{
  return d->m_view;
}

void KHTMLPart::setJScriptEnabled( bool enable )
{
  if ( !enable && jScriptEnabled() && d->m_jscript ) {
    d->m_jscript->clear();
  }
  d->m_bJScriptForce = enable;
  d->m_bJScriptOverride = true;
}

bool KHTMLPart::jScriptEnabled() const
{
  if ( d->m_bJScriptOverride )
      return d->m_bJScriptForce;
  return d->m_bJScriptEnabled;
}

void KHTMLPart::setMetaRefreshEnabled( bool enable )
{
  d->m_metaRefreshEnabled = enable;
}

bool KHTMLPart::metaRefreshEnabled() const
{
  return d->m_metaRefreshEnabled;
}

// Define this to disable dlopening kjs_html, when directly linking to it.
// You need to edit khtml/Makefile.am to add ./ecma/libkjs_html.la to LIBADD
// and to edit khtml/ecma/Makefile.am to s/kjs_html/libkjs_html/, remove libkhtml from LIBADD,
//        remove LDFLAGS line, and replace kde_module with either lib (shared) or noinst (static)
//#define DIRECT_LINKAGE_TO_ECMA

#ifdef DIRECT_LINKAGE_TO_ECMA
extern "C" { KJSProxy *kjs_html_init(KHTMLPart *khtmlpart); }
#endif

KJSProxy *KHTMLPart::jScript()
{
  if (!jScriptEnabled()){
    return 0;
  }

  if ( !d->m_jscript )
  {
#ifndef DIRECT_LINKAGE_TO_ECMA
    KLibrary *lib = KLibLoader::self()->library("kjs_html");
    if ( !lib ) {
      setJScriptEnabled( false );
      return 0;
    }
    // look for plain C init function
    void *sym = lib->symbol("kjs_html_init");
    if ( !sym ) {
      lib->unload();
      setJScriptEnabled( false );
      return 0;
    }
    typedef KJSProxy* (*initFunction)(KHTMLPart *);
    initFunction initSym = (initFunction) sym;
    d->m_jscript = (*initSym)(this);
    d->m_kjs_lib = lib;
#else
    d->m_jscript = kjs_html_init(this);
    // d->m_kjs_lib remains 0L.
#endif
    if (d->m_bJScriptDebugEnabled)
        d->m_jscript->setDebugEnabled(true);
  }

  return d->m_jscript;
}

void KHTMLPart::replaceContentsWithScriptResult( const KURL &url )
{
  QString script = KURL::decode_string(url.url().mid(strlen("javascript:")));
  QVariant ret = executeScript(script);
  
  if (ret.type() == QVariant::String) {
    begin();
    write(ret.asString());
    end();
  }
}

QVariant KHTMLPart::executeScript( const QString &script, bool forceUserGesture )
{
    return executeScript( 0, script, forceUserGesture );
}

//Enable this to see all JS scripts being executed
//#define KJS_VERBOSE

#if !KHTML_NO_CPLUSPLUS_DOM

QVariant KHTMLPart::executeScript( const DOM::Node &n, const QString &script, bool forceUserGesture )
{
    return executeScript(n.handle(), script, forceUserGesture);
}

#endif

QVariant KHTMLPart::executeScript( DOM::NodeImpl *n, const QString &script, bool forceUserGesture )
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "KHTMLPart::executeScript n=" << n.nodeName().qstring().latin1() << "(" << (n.isNull() ? 0 : n.nodeType()) << ") " << script << endl;
#endif
  KJSProxy *proxy = jScript();

  if (!proxy || proxy->paused())
    return QVariant();
  d->m_runningScripts++;
  // If forceUserGesture is true, then make the script interpreter
  // treat it as if triggered by a user gesture even if there is no
  // current DOM event being processed.
  QVariant ret = proxy->evaluate( forceUserGesture ? QString::null : m_url.url(), 0, script, n );
  d->m_runningScripts--;
  if (!d->m_runningScripts && d->m_doc && !d->m_doc->parsing() && d->m_submitForm )
      submitFormAgain();
    DocumentImpl::updateDocumentsRendering();

#ifdef KJS_VERBOSE
  kdDebug(6070) << "KHTMLPart::executeScript - done" << endl;
#endif
  return ret;
}

bool KHTMLPart::scheduleScript(DOM::NodeImpl *n, const QString& script)
{
    //kdDebug(6050) << "KHTMLPart::scheduleScript "<< script << endl;

    d->scheduledScript = script;
    d->scheduledScriptNode.reset(n);

    return true;
}

QVariant KHTMLPart::executeScheduledScript()
{
  if( d->scheduledScript.isEmpty() )
    return QVariant();

  //kdDebug(6050) << "executing delayed " << d->scheduledScript << endl;

  QVariant ret = executeScript( d->scheduledScriptNode.get(), d->scheduledScript );
  d->scheduledScript = QString();
  d->scheduledScriptNode.reset();

  return ret;
}

void KHTMLPart::setJavaEnabled( bool enable )
{
  d->m_bJavaForce = enable;
  d->m_bJavaOverride = true;
}

bool KHTMLPart::javaEnabled() const
{
#ifndef Q_WS_QWS
  if( d->m_bJavaOverride )
      return d->m_bJavaForce;
  return d->m_bJavaEnabled;
#else
  return false;
#endif
}

KJavaAppletContext *KHTMLPart::javaContext()
{
#ifndef Q_WS_QWS
  return d->m_javaContext;
#else
  return 0;
#endif
}

KJavaAppletContext *KHTMLPart::createJavaContext()
{
#ifndef Q_WS_QWS
  if ( !d->m_javaContext ) {
      d->m_javaContext = new KJavaAppletContext(d->m_dcopobject, this);
  }

  return d->m_javaContext;
#else
  return 0;
#endif
}

void KHTMLPart::setPluginsEnabled( bool enable )
{
  d->m_bPluginsForce = enable;
  d->m_bPluginsOverride = true;
}

bool KHTMLPart::pluginsEnabled() const
{
  if ( d->m_bPluginsOverride )
      return d->m_bPluginsForce;
  return d->m_bPluginsEnabled;
}


void KHTMLPart::slotDebugDOMTree()
{
  if ( d->m_doc && d->m_doc->firstChild() )
    qDebug("%s", createMarkup(d->m_doc->firstChild()).latin1());
}

void KHTMLPart::slotDebugRenderTree()
{
#ifndef NDEBUG
  if ( d->m_doc )
    d->m_doc->renderer()->printTree();
#endif
}

void KHTMLPart::setAutoloadImages( bool enable )
{
  if ( d->m_doc && d->m_doc->docLoader()->autoloadImages() == enable )
    return;

  if ( d->m_doc )
    d->m_doc->docLoader()->setAutoloadImages( enable );

}

bool KHTMLPart::autoloadImages() const
{
  if ( d->m_doc )
    return d->m_doc->docLoader()->autoloadImages();

  return true;
}

void KHTMLPart::clear()
{
  if ( d->m_bCleared )
    return;
  d->m_bCleared = true;

  d->m_bClearing = true;


  d->m_mousePressNode.reset();


  if ( d->m_doc )
    d->m_doc->detach();

  // Moving past doc so that onUnload works.
  if ( d->m_jscript )
    d->m_jscript->clear();

  if ( d->m_view )
    d->m_view->clear();

  // do not dereference the document before the jscript and view are cleared, as some destructors
  // might still try to access the document.
  if ( d->m_doc )
    d->m_doc->deref();
  d->m_doc = 0;

  if (d->m_decoder)
    d->m_decoder->deref();
  d->m_decoder = 0;

  {
    ConstFrameIt it = d->m_frames.begin();
    ConstFrameIt end = d->m_frames.end();
    for(; it != end; ++it )
    {
      if ( (*it).m_part )
      {
        disconnectChild(&*it);
        (*it).m_part->deref();
      }
    }
  }
  d->m_frames.clear();

  {
    ConstFrameIt it = d->m_objects.begin();
    ConstFrameIt end = d->m_objects.end();
    for(; it != end; ++it )
    {
      if ( (*it).m_part )
      {
        (*it).m_part->deref();
      }
    }
  }
  d->m_objects.clear();

#ifndef Q_WS_QWS
  delete d->m_javaContext;
  d->m_javaContext = 0;
#endif

  d->m_scheduledRedirection = noRedirectionScheduled;
  d->m_delayRedirect = 0;
  d->m_redirectURL = QString::null;
  d->m_redirectReferrer = QString::null;
  d->m_redirectLockHistory = true;
  d->m_redirectUserGesture = false;
  d->m_bHTTPRefresh = false;
  d->m_bClearing = false;
  d->m_frameNameId = 1;
  d->m_bFirstData = true;

  d->m_bMousePressed = false;

#ifndef QT_NO_CLIPBOARD
  connect( kapp->clipboard(), SIGNAL( selectionChanged()), SLOT( slotClearSelection()));
#endif


  if ( !d->m_haveEncoding )
    d->m_encoding = QString::null;
#ifdef SPEED_DEBUG
  d->m_parsetime.restart();
#endif
}

bool KHTMLPart::openFile()
{
  return true;
}

DOM::DocumentImpl *KHTMLPart::xmlDocImpl() const
{
    if ( d )
        return d->m_doc;
    return 0;
}

void KHTMLPart::replaceDocImpl(DocumentImpl* newDoc)
{
    if (d) {
        if (d->m_doc) {
            d->m_doc->detach();
            d->m_doc->deref();
        }
        d->m_doc = newDoc;
        if (newDoc) {
            newDoc->ref();
            newDoc->attach();
        }
    }
}

/*bool KHTMLPart::isSSLInUse() const
{
  return d->m_ssl_in_use;
}*/

void KHTMLPart::receivedFirstData()
{
    // Leave indented one extra for easier merging.
    
      //kdDebug( 6050 ) << "begin!" << endl;

    begin( d->m_workingURL, d->m_extension->urlArgs().xOffset, d->m_extension->urlArgs().yOffset );


    d->m_doc->docLoader()->setCachePolicy(d->m_cachePolicy);
    d->m_workingURL = KURL();


    // When the first data arrives, the metadata has just been made available
    QString qData;

    // Support for http-refresh
    qData = d->m_job->queryMetaData("http-refresh");
    if( !qData.isEmpty() && d->m_metaRefreshEnabled )
    {
      kdDebug(6050) << "HTTP Refresh Request: " << qData << endl;
      double delay;
      int pos = qData.find( ';' );
      if ( pos == -1 )
        pos = qData.find( ',' );

      if( pos == -1 )
      {
        delay = qData.stripWhiteSpace().toDouble();
        // We want a new history item if the refresh timeout > 1 second
        scheduleRedirection( delay, m_url.url(), delay <= 1);
      }
      else
      {
        int end_pos = qData.length();
        delay = qData.left(pos).stripWhiteSpace().toDouble();
        while ( qData[++pos] == ' ' );
        if ( qData.find( "url", pos, false ) == pos )
        {
          pos += 3;
          while (qData[pos] == ' ' || qData[pos] == '=' )
              pos++;
          if ( qData[pos] == '"' )
          {
              pos++;
              int index = end_pos-1;
              while( index > pos )
              {
                if ( qData[index] == '"' )
                    break;
                index--;
              }
              if ( index > pos )
                end_pos = index;
          }
        }
        // We want a new history item if the refresh timeout > 1 second
        scheduleRedirection( delay, d->m_doc->completeURL( qData.mid( pos, end_pos ) ), delay <= 1);
      }
      d->m_bHTTPRefresh = true;
    }

    // Support for http last-modified
    d->m_lastModified = d->m_job->queryMetaData("modified");
    //kdDebug() << "KHTMLPart::slotData metadata modified: " << d->m_lastModified << endl;
}


void KHTMLPart::slotFinished( KIO::Job * job )
{
  if (job->error())
  {
    d->m_job = 0L;
    // TODO: what else ?
    checkCompleted();
    return;
  }
  //kdDebug( 6050 ) << "slotFinished" << endl;


  if ( d->m_doc && d->m_doc->docLoader()->expireDate() && m_url.protocol().lower().startsWith("http"))
      KIO::http_update_cache(m_url, false, d->m_doc->docLoader()->expireDate());

  d->m_workingURL = KURL();
  d->m_job = 0L;

  if (d->m_doc->parsing())
      end(); //will emit completed()
}

void KHTMLPart::childBegin()
{
    // We need to do this when the child is created so as to avoid the bogus state of the parent's
    // child->m_bCompleted being false but the child's m_bComplete being true.  If the child gets
    // an error early on, we had trouble where checkingComplete on the child was a NOP because
    // it thought it was already complete, and thus the parent was never signaled, and never set
    // its child->m_bComplete.
    d->m_bComplete = false;
}

void KHTMLPart::begin( const KURL &url, int xOffset, int yOffset )
{
  // If we aren't loading an actual URL, then we need to make sure
  // that we have at least an empty document. createEmptyDocument will
  // do that if we don't have a document already.
  if (d->m_workingURL.isEmpty()) {
    KWQ(this)->createEmptyDocument();
  }

  clear();

  KWQ(this)->partClearedInBegin();

  // Only do this after clearing the part, so that JavaScript can
  // clean up properly if it was on for the last load.
  d->m_bJScriptEnabled = d->m_settings->isJavaScriptEnabled(url.host());

  d->m_bCleared = false;
  d->m_bComplete = false;
  d->m_bLoadEventEmitted = false;
  d->m_bLoadingMainResource = true;

  if(url.isValid()) {
      KHTMLFactory::vLinks()->insert( KWQ(this)->requestedURLString() );
  }

  // ###
  //stopParser();

  KParts::URLArgs args( d->m_extension->urlArgs() );
  args.xOffset = xOffset;
  args.yOffset = yOffset;
  d->m_extension->setURLArgs( args );

  KURL ref(url);
  ref.setUser(QSTRING_NULL);
  ref.setPass(QSTRING_NULL);
  ref.setRef(QSTRING_NULL);
  d->m_referrer = ref.url();
  m_url = url;
  KURL baseurl;

  // We don't need KDE chained URI handling or window caption setting
  if ( !m_url.isEmpty() )
  {
    baseurl = m_url;
  }

  if (args.serviceType == "text/xml" || args.serviceType == "application/xml" || args.serviceType == "application/xhtml+xml" ||
      args.serviceType == "text/xsl" || args.serviceType == "application/rss+xml" || args.serviceType == "application/atom+xml")
    d->m_doc = DOMImplementationImpl::instance()->createDocument( d->m_view );
#if SVG_SUPPORT
  else if (args.serviceType == "image/svg+xml")
    d->m_doc = DOMImplementationImpl::instance()->createKDOMDocument(d->m_view);
#endif
  else
    d->m_doc = DOMImplementationImpl::instance()->createHTMLDocument( d->m_view );

  d->m_doc->ref();
  if (!d->m_doc->attached())
    d->m_doc->attach( );
  d->m_doc->setURL( m_url.url() );
  // We prefer m_baseURL over m_url because m_url changes when we are
  // about to load a new page.
  d->m_doc->setBaseURL( baseurl.url() );
  if (d->m_decoder)
    d->m_doc->setDecoder(d->m_decoder);
  d->m_doc->docLoader()->setShowAnimations( d->m_settings->showAnimations() );

  KWQ(this)->updatePolicyBaseURL();


  setAutoloadImages( d->m_settings->autoLoadImages() );
  QString userStyleSheet = d->m_settings->userStyleSheet();

  if ( !userStyleSheet.isEmpty() )
    setUserStyleSheet( KURL( userStyleSheet ) );

  KWQ(this)->restoreDocumentState();

  d->m_doc->implicitOpen();
  // clear widget
  if (d->m_view)
    d->m_view->resizeContents( 0, 0 );
  connect(d->m_doc,SIGNAL(finishedParsing()),this,SLOT(slotFinishedParsing()));

}

void KHTMLPart::write( const char *str, int len )
{
    if ( !d->m_decoder ) {
        d->m_decoder = new Decoder;
        if (!d->m_encoding.isNull())
            d->m_decoder->setEncoding(d->m_encoding.latin1(),
                d->m_haveEncoding ? Decoder::UserChosenEncoding : Decoder::EncodingFromHTTPHeader);
        else {
            // Inherit the default encoding from the parent frame if there is one.
            const char *defaultEncoding = (parentPart() && parentPart()->d->m_decoder)
                ? parentPart()->d->m_decoder->encoding() : settings()->encoding().latin1();
            d->m_decoder->setEncoding(defaultEncoding, Decoder::DefaultEncoding);
        }
        if (d->m_doc)
            d->m_doc->setDecoder(d->m_decoder);
    }
  if ( len == 0 )
    return;

  if ( len == -1 )
    len = strlen( str );

  QString decoded = d->m_decoder->decode( str, len );

  if(decoded.isEmpty()) return;

  if(d->m_bFirstData) {
      // determine the parse mode
      d->m_doc->determineParseMode( decoded );
      d->m_bFirstData = false;

  //kdDebug(6050) << "KHTMLPart::write haveEnc = " << d->m_haveEncoding << endl;
      // ### this is still quite hacky, but should work a lot better than the old solution
      if(d->m_decoder->visuallyOrdered()) d->m_doc->setVisuallyOrdered();
      d->m_doc->recalcStyle( NodeImpl::Force );
  }

  if (jScript())
    jScript()->appendSourceFile(m_url.url(),decoded);
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
  if (jScript())
    jScript()->appendSourceFile(m_url.url(),str);
  Tokenizer* t = d->m_doc->tokenizer();
  if(t)
    t->write( str, true );
}

void KHTMLPart::end()
{
    d->m_bLoadingMainResource = false;
    endIfNotLoading();
}

void KHTMLPart::endIfNotLoading()
{
    if (d->m_bLoadingMainResource)
        return;

    // make sure nothing's left in there...
    if (d->m_decoder)
        write(d->m_decoder->flush());
    if (d->m_doc)
        d->m_doc->finishParsing();
    else
        // WebKit partially uses WebCore when loading non-HTML docs.  In these cases doc==nil, but
        // WebCore is enough involved that we need to checkCompleted() in order for m_bComplete to
        // become true.  An example is when a subframe is a pure text doc, and that subframe is the
        // last one to complete.
        checkCompleted();
}

void KHTMLPart::stop()
{
    // make sure nothing's left in there...
    if (d->m_doc) {
        if (d->m_doc->tokenizer())
            d->m_doc->tokenizer()->stopParsing();
        d->m_doc->finishParsing();
    } else {
        // WebKit partially uses WebCore when loading non-HTML docs.  In these cases doc==nil, but
        // WebCore is enough involved that we need to checkCompleted() in order for m_bComplete to
        // become true.  An example is when a subframe is a pure text doc, and that subframe is the
        // last one to complete.
        checkCompleted();
    }
}


void KHTMLPart::stopAnimations()
{
  if ( d->m_doc )
    d->m_doc->docLoader()->setShowAnimations( KHTMLSettings::KAnimationDisabled );

  ConstFrameIt it = d->m_frames.begin();
  ConstFrameIt end = d->m_frames.end();
  for (; it != end; ++it )
    if ( !( *it ).m_part.isNull() && ( *it ).m_part->inherits( "KHTMLPart" ) ) {
      KParts::ReadOnlyPart* p = ( *it ).m_part;
      static_cast<KHTMLPart*>( p )->stopAnimations();
    }
}

void KHTMLPart::gotoAnchor()
{
    // If our URL has no ref, then we have no place we need to jump to.
    if (!m_url.hasRef())
        return;

    QString ref = m_url.encodedHtmlRef();
    if (!gotoAnchor(ref)) {
        // Can't use htmlRef() here because it doesn't know which encoding to use to decode.
        // Decoding here has to match encoding in completeURL, which means it has to use the
        // page's encoding rather than UTF-8.
        if (d->m_decoder)
            gotoAnchor(KURL::decode_string(ref, d->m_decoder->codec()));
    }
}

void KHTMLPart::slotFinishedParsing()
{
  d->m_doc->setParsing(false);

  if (!d->m_view)
    return; // We are probably being destructed.
    
  checkCompleted();

  if (!d->m_view)
    return; // We are being destroyed by something checkCompleted called.

  // check if the scrollbars are really needed for the content
  // if not, remove them, relayout, and repaint

  d->m_view->restoreScrollBar();
  gotoAnchor();
}

void KHTMLPart::slotLoaderRequestStarted( khtml::DocLoader* dl, khtml::CachedObject *obj )
{
}

void KHTMLPart::slotLoaderRequestDone( khtml::DocLoader* dl, khtml::CachedObject *obj )
{
  // We really only need to call checkCompleted when our own resources are done loading.
  // So we should check that d->m_doc->docLoader() == dl here.
  // That might help with performance by skipping some unnecessary work, but it's too
  // risky to make that change right now (2005-02-07), because we might be accidentally
  // depending on the extra checkCompleted calls.
  if (d->m_doc) {
    checkCompleted();
  }
}


void KHTMLPart::checkCompleted()
{
//   kdDebug( 6050 ) << "KHTMLPart::checkCompleted() parsing: " << d->m_doc->parsing() << endl;
//   kdDebug( 6050 ) << "                           complete: " << d->m_bComplete << endl;


  // Any frame that hasn't completed yet ?
  ConstFrameIt it = d->m_frames.begin();
  ConstFrameIt end = d->m_frames.end();
  for (; it != end; ++it )
    if ( !(*it).m_bCompleted )
      return;

  // Have we completed before?
  if ( d->m_bComplete )
    return;

  // Are we still parsing?
  if ( d->m_doc && d->m_doc->parsing() )
    return;

  // Still waiting for images/scripts from the loader ?
  int requests = 0;
  if ( d->m_doc && d->m_doc->docLoader() )
    requests = khtml::Cache::loader()->numRequests( d->m_doc->docLoader() );

  if ( requests > 0 )
    return;

  // OK, completed.
  // Now do what should be done when we are really completed.
  d->m_bComplete = true;

  checkEmitLoadEvent(); // if we didn't do it before


  if ( d->m_scheduledRedirection != noRedirectionScheduled )
  {
    // Do not start redirection for frames here! That action is
    // deferred until the parent emits a completed signal.
    if ( parentPart() == 0 )
      d->m_redirectionTimer.start( (int)(1000 * d->m_delayRedirect), true );

    emit completed( true );
  }
  else
  {
    if ( d->m_bPendingChildRedirection )
      emit completed ( true );
    else
      emit completed();
  }


#ifdef SPEED_DEBUG
  kdDebug(6050) << "DONE: " <<d->m_parsetime.elapsed() << endl;
#endif
}

void KHTMLPart::checkEmitLoadEvent()
{
  if ( d->m_bLoadEventEmitted || !d->m_doc || d->m_doc->parsing() ) return;

  ConstFrameIt it = d->m_frames.begin();
  ConstFrameIt end = d->m_frames.end();
  for (; it != end; ++it )
    if ( !(*it).m_bCompleted ) // still got a frame running -> too early
      return;


  // All frames completed -> set their domain to the frameset's domain
  // This must only be done when loading the frameset initially (#22039),
  // not when following a link in a frame (#44162).
  if ( d->m_doc )
  {
    DOMString domain = d->m_doc->domain();
    ConstFrameIt it = d->m_frames.begin();
    ConstFrameIt end = d->m_frames.end();
    for (; it != end; ++it )
    {
      KParts::ReadOnlyPart *p = (*it).m_part;
      if ( p && p->inherits( "KHTMLPart" ))
      {
        KHTMLPart* htmlFrame = static_cast<KHTMLPart *>(p);
        if (htmlFrame->d->m_doc)
        {
          kdDebug() << "KHTMLPart::checkCompleted setting frame domain to " << domain.qstring() << endl;
          htmlFrame->d->m_doc->setDomain( domain );
        }
      }
    }
  }

  d->m_bLoadEventEmitted = true;
  d->m_bUnloadEventEmitted = false;
  if (d->m_doc)
    d->m_doc->implicitClose();
}

const KHTMLSettings *KHTMLPart::settings() const
{
  return d->m_settings;
}

#ifndef KDE_NO_COMPAT
KURL KHTMLPart::baseURL() const
{
  if ( !d->m_doc ) return KURL();

  return d->m_doc->baseURL();
}

QString KHTMLPart::baseTarget() const
{
  if ( !d->m_doc ) return QString::null;

  return d->m_doc->baseTarget();
}
#endif

KURL KHTMLPart::completeURL( const QString &url )
{
  if ( !d->m_doc ) return url;


  return KURL( d->m_doc->completeURL( url ) );
}

void KHTMLPart::scheduleRedirection( double delay, const QString &url, bool doLockHistory)
{
    kdDebug(6050) << "KHTMLPart::scheduleRedirection delay=" << delay << " url=" << url << endl;
    if (delay < 0 || delay > INT_MAX / 1000)
      return;
    if ( d->m_scheduledRedirection == noRedirectionScheduled || delay <= d->m_delayRedirect )
    {
       d->m_scheduledRedirection = redirectionScheduled;
       d->m_delayRedirect = delay;
       d->m_redirectURL = url;
       d->m_redirectReferrer = QString::null;
       d->m_redirectLockHistory = doLockHistory;
       d->m_redirectUserGesture = false;

       d->m_redirectionTimer.stop();
       if ( d->m_bComplete )
         d->m_redirectionTimer.start( (int)(1000 * d->m_delayRedirect), true );
    }
}

void KHTMLPart::scheduleLocationChange(const QString &url, const QString &referrer, bool lockHistory, bool userGesture)
{
    // Handle a location change of a page with no document as a special case.
    // This may happen when a frame changes the location of another frame.
    d->m_scheduledRedirection = d->m_doc ? locationChangeScheduled : locationChangeScheduledDuringLoad;
    
    // If a redirect was scheduled during a load, then stop the current load.
    // Otherwise when the current load transitions from a provisional to a 
    // committed state, pending redirects may be cancelled. 
    if (d->m_scheduledRedirection == locationChangeScheduledDuringLoad) {
        stopLoading(true);   
    }
    
    d->m_delayRedirect = 0;
    d->m_redirectURL = url;
    d->m_redirectReferrer = referrer;
    d->m_redirectLockHistory = lockHistory;
    d->m_redirectUserGesture = userGesture;
    d->m_redirectionTimer.stop();
    if (d->m_bComplete)
        d->m_redirectionTimer.start(0, true);
}

bool KHTMLPart::isScheduledLocationChangePending() const
{
    switch (d->m_scheduledRedirection) {
        case noRedirectionScheduled:
        case redirectionScheduled:
            return false;
        case historyNavigationScheduled:
        case locationChangeScheduled:
        case locationChangeScheduledDuringLoad:
            return true;
    }
    return false;
}

void KHTMLPart::scheduleHistoryNavigation( int steps )
{
    // navigation will always be allowed in the 0 steps case, which is OK because
    // that's supposed to force a reload.
    if (!KWQ(this)->canGoBackOrForward(steps)) {
        cancelRedirection();
        return;
    }

    d->m_scheduledRedirection = historyNavigationScheduled;
    d->m_delayRedirect = 0;
    d->m_redirectURL = QString::null;
    d->m_redirectReferrer = QString::null;
    d->m_scheduledHistoryNavigationSteps = steps;
    d->m_redirectionTimer.stop();
    if (d->m_bComplete)
        d->m_redirectionTimer.start(0, true);
}

void KHTMLPart::cancelRedirection(bool cancelWithLoadInProgress)
{
    if (d) {
        d->m_cancelWithLoadInProgress = cancelWithLoadInProgress;
        d->m_scheduledRedirection = noRedirectionScheduled;
        d->m_redirectionTimer.stop();
    }
}

void KHTMLPart::changeLocation(const QString &URL, const QString &referrer, bool lockHistory, bool userGesture)
{
    if (URL.find("javascript:", 0, false) == 0) {
        QString script = KURL::decode_string(URL.mid(11));
        QVariant result = executeScript(script, userGesture);
        if (result.type() == QVariant::String) {
            begin(url());
            write(result.asString());
            end();
        }
        return;
    }

    KParts::URLArgs args;

    args.setLockHistory(lockHistory);
    if (!referrer.isEmpty())
        args.metaData()["referrer"] = referrer;

    urlSelected(URL, 0, 0, "_self", args);
}

void KHTMLPart::slotRedirect()
{
    if (d->m_scheduledRedirection == historyNavigationScheduled) {
        d->m_scheduledRedirection = noRedirectionScheduled;

        // Special case for go(0) from a frame -> reload only the frame
        // go(i!=0) from a frame navigates into the history of the frame only,
        // in both IE and NS (but not in Mozilla).... we can't easily do that
        // in Konqueror...
        if (d->m_scheduledHistoryNavigationSteps == 0) // add && parentPart() to get only frames, but doesn't matter
            openURL( url() ); /// ## need args.reload=true?
        else {
            if (d->m_extension) {
                BrowserInterface *interface = d->m_extension->browserInterface();
                if (interface)
                    interface->callMethod( "goHistory(int)", d->m_scheduledHistoryNavigationSteps );
            }
        }
        return;
    }

    QString URL = d->m_redirectURL;
    QString referrer = d->m_redirectReferrer;
    bool lockHistory = d->m_redirectLockHistory;
    bool userGesture = d->m_redirectUserGesture;

    d->m_scheduledRedirection = noRedirectionScheduled;
    d->m_delayRedirect = 0;
    d->m_redirectURL = QString::null;
    d->m_redirectReferrer = QString::null;

    changeLocation(URL, referrer, lockHistory, userGesture);
}

void KHTMLPart::slotRedirection(KIO::Job*, const KURL& url)
{
  // the slave told us that we got redirected
  // kdDebug( 6050 ) << "redirection by KIO to " << url.url() << endl;
  emit d->m_extension->setLocationBarURL( url.prettyURL() );
  d->m_workingURL = url;
}


QString KHTMLPart::encoding() const
{
    if(d->m_haveEncoding && !d->m_encoding.isEmpty())
        return d->m_encoding;

    if(d->m_decoder && d->m_decoder->encoding())
        return QString(d->m_decoder->encoding());

    return(settings()->encoding());
}

void KHTMLPart::setUserStyleSheet(const KURL &url)
{
  if ( d->m_doc && d->m_doc->docLoader() )
    (void) new khtml::PartStyleSheetLoader(this, url.url(), d->m_doc->docLoader());
}

void KHTMLPart::setUserStyleSheet(const QString &styleSheet)
{
  if ( d->m_doc )
    d->m_doc->setUserStyleSheet( styleSheet );
}

bool KHTMLPart::gotoAnchor( const QString &name )
{
  if (!d->m_doc)
    return false;

  NodeImpl *n = d->m_doc->getElementById(name);
  if (!n) {
    HTMLCollectionImpl *anchors =
        new HTMLCollectionImpl( d->m_doc, HTMLCollectionImpl::DOC_ANCHORS);
    anchors->ref();
    n = anchors->namedItem(name, !d->m_doc->inCompatMode());
    anchors->deref();
  }

  d->m_doc->setCSSTarget(n); // Setting to null will clear the current target.
  
  // Implement the rule that "" and "top" both mean top of page as in other browsers.
  if (!n && !(name.isEmpty() || name.lower() == "top")) {
    kdDebug(6050) << "KHTMLPart::gotoAnchor node '" << name << "' not found" << endl;
    return false;
  }

  // We need to update the layout before scrolling, otherwise we could
  // really mess things up if an anchor scroll comes at a bad moment.
  if ( d->m_doc ) {
    d->m_doc->updateRendering();
    // Only do a layout if changes have occurred that make it necessary.      
    if ( d->m_view && d->m_doc->renderer() && d->m_doc->renderer()->needsLayout() ) {
      d->m_view->layout();
    }
  }
  
    // Scroll nested layers and frames to reveal the anchor.
    if (n && n->renderer()) {
        // Align to the top and to the closest side (this matches other browsers).
        n->renderer()->enclosingLayer()->scrollRectToVisible(n->getRect(), RenderLayer::gAlignToEdgeIfNeeded, RenderLayer::gAlignTopAlways);
    }

  return true;
}

void KHTMLPart::setStandardFont( const QString &name )
{
    d->m_settings->setStdFontName(name);
}

void KHTMLPart::setFixedFont( const QString &name )
{
    d->m_settings->setFixedFontName(name);
}


QCursor KHTMLPart::urlCursor() const
{
  // Don't load the link cursor until it's actually used.
  // Also, we don't need setURLCursor.
  // This speeds up startup time.
  return KCursor::handCursor();
}

bool KHTMLPart::onlyLocalReferences() const
{
  return d->m_onlyLocalReferences;
}

void KHTMLPart::setOnlyLocalReferences(bool enable)
{
  d->m_onlyLocalReferences = enable;
}


#if !KHTML_NO_CPLUSPLUS_DOM

QString KHTMLPart::text(const DOM::Range &r) const
{
    return plainText(r.handle());
}

#endif

QString KHTMLPart::selectedText() const
{
    return plainText(selection().toRange().get());
}

bool KHTMLPart::hasSelection() const
{
    return d->m_selection.isCaretOrRange();
}

const SelectionController &KHTMLPart::selection() const
{
    return d->m_selection;
}

ETextGranularity KHTMLPart::selectionGranularity() const
{
    return d->m_selectionGranularity;
}

void KHTMLPart::setSelectionGranularity(ETextGranularity granularity) const
{
    d->m_selectionGranularity = granularity;
}

const SelectionController &KHTMLPart::dragCaret() const
{
    return d->m_dragCaret;
}

const SelectionController &KHTMLPart::mark() const
{
    return d->m_mark;
}

void KHTMLPart::setMark(const SelectionController &s)
{
    d->m_mark = s;
}

void KHTMLPart::setSelection(const SelectionController &s, bool closeTyping, bool keepTypingStyle)
{
    if (d->m_selection == s) {
        return;
    }
    
    clearCaretRectIfNeeded();

    SelectionController oldSelection = d->m_selection;

    d->m_selection = s;
    if (!s.isNone())
        setFocusNodeIfNeeded();
    
    selectionLayoutChanged();

    // Always clear the x position used for vertical arrow navigation.
    // It will be restored by the vertical arrow navigation code if necessary.
    d->m_xPosForVerticalArrowNavigation = NoXPosForVerticalArrowNavigation;

    if (closeTyping)
        TypingCommand::closeTyping(lastEditCommand());

    if (!keepTypingStyle)
        clearTypingStyle();
    
    KWQ(this)->respondToChangedSelection(oldSelection, closeTyping);

    emitSelectionChanged();
}

void KHTMLPart::setDragCaret(const SelectionController &dragCaret)
{
    if (d->m_dragCaret != dragCaret) {
        d->m_dragCaret.needsCaretRepaint();
        d->m_dragCaret = dragCaret;
        d->m_dragCaret.needsCaretRepaint();
    }
}

void KHTMLPart::clearSelection()
{
    setSelection(SelectionController());
}

void KHTMLPart::invalidateSelection()
{
    clearCaretRectIfNeeded();
    d->m_selection.setNeedsLayout();
    selectionLayoutChanged();
}

void KHTMLPart::setCaretVisible(bool flag)
{
    if (d->m_caretVisible == flag)
        return;
    clearCaretRectIfNeeded();
    if (flag)
        setFocusNodeIfNeeded();
    d->m_caretVisible = flag;
    selectionLayoutChanged();
}


void KHTMLPart::clearCaretRectIfNeeded()
{
    if (d->m_caretPaint) {
        d->m_caretPaint = false;
        d->m_selection.needsCaretRepaint();
    }        
}

// Helper function that tells whether a particular node is an element that has an entire
// KHTMLPart and KHTMLView, a <frame>, <iframe>, or <object>.
static bool isFrame(const NodeImpl *n)
{
    if (!n)
        return false;
    RenderObject *renderer = n->renderer();
    if (!renderer || !renderer->isWidget())
        return false;
    QWidget *widget = static_cast<RenderWidget *>(renderer)->widget();
    return widget && widget->inherits("KHTMLView");
}

void KHTMLPart::setFocusNodeIfNeeded()
{
    if (!xmlDocImpl() || d->m_selection.isNone() || !d->m_isFocused)
        return;

    NodeImpl *startNode = d->m_selection.start().node();
    NodeImpl *target = startNode ? startNode->rootEditableElement() : 0;
    
    if (target) {
        for ( ; target; target = target->parentNode()) {
            // We don't want to set focus on a subframe when selecting in a parent frame,
            // so add the !isFrame check here. There's probably a better way to make this
            // work in the long term, but this is the safest fix at this time.
            if (target->isMouseFocusable() && !isFrame(target)) {
                xmlDocImpl()->setFocusNode(target);
                return;
            }
        }
        xmlDocImpl()->setFocusNode(0);
    }
}

void KHTMLPart::selectionLayoutChanged()
{
    // kill any caret blink timer now running
    if (d->m_caretBlinkTimer >= 0) {
        killTimer(d->m_caretBlinkTimer);
        d->m_caretBlinkTimer = -1;
    }

    // see if a new caret blink timer needs to be started
    if (d->m_caretVisible && d->m_caretBlinks && 
        d->m_selection.isCaret() && d->m_selection.start().node()->isContentEditable()) {
        d->m_caretBlinkTimer = startTimer(CARET_BLINK_FREQUENCY);
        d->m_caretPaint = true;
        d->m_selection.needsCaretRepaint();
    }

    if (d->m_doc)
        d->m_doc->updateSelection();
}

void KHTMLPart::setXPosForVerticalArrowNavigation(int x)
{
    d->m_xPosForVerticalArrowNavigation = x;
}

int KHTMLPart::xPosForVerticalArrowNavigation() const
{
    return d->m_xPosForVerticalArrowNavigation;
}

void KHTMLPart::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == d->m_caretBlinkTimer && 
        d->m_caretVisible && 
        d->m_caretBlinks && 
        d->m_selection.isCaret()) {
        if (d->m_bMousePressed) {
            if (!d->m_caretPaint) {
                d->m_caretPaint = true;
                d->m_selection.needsCaretRepaint();
            }
        }
        else {
            d->m_caretPaint = !d->m_caretPaint;
            d->m_selection.needsCaretRepaint();
        }
    }
}

void KHTMLPart::paintCaret(QPainter *p, const QRect &rect) const
{
    if (d->m_caretPaint)
        d->m_selection.paintCaret(p, rect);
}

void KHTMLPart::paintDragCaret(QPainter *p, const QRect &rect) const
{
    d->m_dragCaret.paintCaret(p, rect);
}


void KHTMLPart::urlSelected( const QString &url, int button, int state, const QString &_target,
                             KParts::URLArgs args )
{
  bool hasTarget = false;

  QString target = _target;
  if ( target.isEmpty() && d->m_doc )
    target = d->m_doc->baseTarget();
  if ( !target.isEmpty() )
      hasTarget = true;

  if ( url.find( QString::fromLatin1( "javascript:" ), 0, false ) == 0 )
  {
    executeScript( KURL::decode_string( url.right( url.length() - 11) ), true );
    return;
  }

  KURL cURL = completeURL(url);

  if ( !cURL.isValid() )
    // ### ERROR HANDLING
    return;

  //kdDebug( 6000 ) << "urlSelected: complete URL:" << cURL.url() << " target = " << target << endl;


  args.frameName = target;

  if ( d->m_bHTTPRefresh )
  {
    d->m_bHTTPRefresh = false;
    args.metaData()["cache"] = "refresh";
  }


  if (!d->m_referrer.isEmpty())
    args.metaData()["referrer"] = d->m_referrer;
  KWQ(this)->urlSelected(cURL, button, state, args);
}


bool KHTMLPart::requestFrame( khtml::RenderPart *frame, const QString &url, const QString &frameName,
                              const QStringList &paramNames, const QStringList &paramValues, bool isIFrame )
{
//  kdDebug( 6050 ) << "childRequest( ..., " << url << ", " << frameName << " )" << endl;
  FrameIt it = d->m_frames.find( frameName );
  if ( it == d->m_frames.end() )
  {
    khtml::ChildFrame child;
//    kdDebug( 6050 ) << "inserting new frame into frame map " << frameName << endl;
    child.m_name = frameName;
    it = d->m_frames.append( child );
  }

  (*it).m_type = isIFrame ? khtml::ChildFrame::IFrame : khtml::ChildFrame::Frame;
  (*it).m_frame = frame;
  (*it).m_paramValues = paramNames;
  (*it).m_paramNames = paramValues;

  // Support for <frame src="javascript:string">
  if ( url.find( QString::fromLatin1( "javascript:" ), 0, false ) == 0 )
  {
    if (!processObjectRequest(&(*it), "about:blank", "text/html" ))
      return false;

    KHTMLPart *newPart = static_cast<KHTMLPart *>(&*(*it).m_part); 
    newPart->replaceContentsWithScriptResult( url );

    return true;
  }

  return requestObject( &(*it), completeURL( url ));
}

QString KHTMLPart::requestFrameName()
{
    return KWQ(this)->generateFrameName();
}

bool KHTMLPart::requestObject( khtml::RenderPart *frame, const QString &url, const QString &serviceType,
                               const QStringList &paramNames, const QStringList &paramValues )
{
  khtml::ChildFrame child;
  QValueList<khtml::ChildFrame>::Iterator it = d->m_objects.append( child );
  (*it).m_frame = frame;
  (*it).m_type = khtml::ChildFrame::Object;
  (*it).m_paramNames = paramNames;
  (*it).m_paramValues = paramValues;
  (*it).m_hasFallbackContent = frame->hasFallbackContent();

  KURL completedURL;
  if (!url.isEmpty())
    completedURL = completeURL(url);

  KParts::URLArgs args;
  args.serviceType = serviceType;
  return requestObject( &(*it), completedURL, args );
}

bool KHTMLPart::requestObject( khtml::ChildFrame *child, const KURL &url, const KParts::URLArgs &_args )
{
  if ( child->m_bPreloaded )
  {
    // kdDebug(6005) << "KHTMLPart::requestObject preload" << endl;
    if ( child->m_frame && child->m_part && child->m_part->widget() )
      child->m_frame->setWidget( child->m_part->widget() );

    child->m_bPreloaded = false;
    return true;
  }

  KParts::URLArgs args( _args );


  if ( child->m_part && !args.reload && urlcmp( child->m_part->url().url(), url.url(), true, true ) )
    args.serviceType = child->m_serviceType;

  child->m_args = args;
  child->m_args.reload = (d->m_cachePolicy == KIO::CC_Reload) || (d->m_cachePolicy == KIO::CC_Refresh);
  child->m_serviceName = QString::null;
  if (!d->m_referrer.isEmpty() && !child->m_args.metaData().contains( "referrer" ))
    child->m_args.metaData()["referrer"] = d->m_referrer;


  // We want a KHTMLPart if the HTML says <frame src=""> or <frame src="about:blank">
  if ((url.isEmpty() || url.url() == "about:blank") && args.serviceType.isEmpty())
    args.serviceType = QString::fromLatin1( "text/html" );

  return processObjectRequest( child, url, args.serviceType );
}

bool KHTMLPart::processObjectRequest( khtml::ChildFrame *child, const KURL &_url, const QString &mimetype )
{
  //kdDebug( 6050 ) << "KHTMLPart::processObjectRequest trying to create part for " << mimetype << endl;

  // IMPORTANT: create a copy of the url here, because it is just a reference, which was likely to be given
  // by an emitting frame part (emit openURLRequest( blahurl, ... ) . A few lines below we delete the part
  // though -> the reference becomes invalid -> crash is likely
  KURL url( _url );

  // khtmlrun called us this way to indicate a loading error
  if ( d->m_onlyLocalReferences || ( url.isEmpty() && mimetype.isEmpty() ) )
  {
      checkEmitLoadEvent();
      child->m_bCompleted = true;
      return true;
  }

  if (child->m_bNotify)
  {
      child->m_bNotify = false;
      if ( !child->m_args.lockHistory() )
          emit d->m_extension->openURLNotify();
  }

  if ( child->m_part )
  {
    KHTMLPart *part = static_cast<KHTMLPart *>(&*child->m_part);
    if (part && part->inherits("KHTMLPart")) {
      KParts::URLArgs args;
      if (!d->m_referrer.isEmpty())
        args.metaData()["referrer"] = d->m_referrer;
      KWQ(part)->openURLRequest(url, args);
    }
  }
  else
  {
    KParts::ReadOnlyPart *part = KWQ(this)->createPart(*child, url, mimetype);
    KHTMLPart *khtml_part = static_cast<KHTMLPart *>(part);
    if (khtml_part && khtml_part->inherits("KHTMLPart"))
      khtml_part->childBegin();

    if (!part) {
        checkEmitLoadEvent();
        return false;
    }

    //CRITICAL STUFF
    if ( child->m_part )
    {
      disconnectChild(child);
      child->m_part->deref();
    }

    child->m_serviceType = mimetype;
    if ( child->m_frame && part->widget() )
      child->m_frame->setWidget( part->widget() );


    child->m_part = part;
    assert( ((void*) child->m_part) != 0);

    connectChild(child);

  }

  checkEmitLoadEvent();
  // Some JS code in the load event may have destroyed the part
  // In that case, abort
  if ( !child->m_part )
    return false;

  if ( child->m_bPreloaded )
  {
    if ( child->m_frame && child->m_part )
      child->m_frame->setWidget( child->m_part->widget() );

    child->m_bPreloaded = false;
    return true;
  }

  child->m_args.reload = (d->m_cachePolicy == KIO::CC_Reload) || (d->m_cachePolicy == KIO::CC_Refresh);

  // make sure the part has a way to find out about the mimetype.
  // we actually set it in child->m_args in requestObject already,
  // but it's useless if we had to use a KHTMLRun instance, as the
  // point the run object is to find out exactly the mimetype.
  child->m_args.serviceType = mimetype;

  child->m_bCompleted = false;
  if ( child->m_extension )
    child->m_extension->setURLArgs( child->m_args );

  // In these cases, the synchronous load would have finished
  // before we could connect the signals, so make sure to send the 
  // completed() signal for the child by hand
  // FIXME: In this case the KHTMLPart will have finished loading before 
  // it's being added to the child list.  It would be a good idea to
  // create the child first, then invoke the loader separately  
  if (url.isEmpty() || url.url() == "about:blank") {
      ReadOnlyPart *readOnlyPart = child->m_part;
      KHTMLPart *part = static_cast<KHTMLPart *>(readOnlyPart);
      if (part && part->inherits("KHTMLPart")) {
          part->completed();
          part->checkCompleted();
      }
  }
      return true;
}


void KHTMLPart::submitFormAgain()
{
  if( d->m_doc && !d->m_doc->parsing() && d->m_submitForm)
    KHTMLPart::submitForm( d->m_submitForm->submitAction, d->m_submitForm->submitUrl, d->m_submitForm->submitFormData, d->m_submitForm->target, d->m_submitForm->submitContentType, d->m_submitForm->submitBoundary );

  delete d->m_submitForm;
  d->m_submitForm = 0;
  disconnect(this, SIGNAL(completed()), this, SLOT(submitFormAgain()));
}

void KHTMLPart::submitForm( const char *action, const QString &url, const FormData &formData, const QString &_target, const QString& contentType, const QString& boundary )
{
  kdDebug(6000) << this << ": KHTMLPart::submitForm target=" << _target << " url=" << url << endl;
  KURL u = completeURL( url );

  if ( !u.isValid() )
  {
    // ### ERROR HANDLING!
    return;
  }


  QString urlstring = u.url();

  if ( urlstring.find( QString::fromLatin1( "javascript:" ), 0, false ) == 0 ) {
    urlstring = KURL::decode_string(urlstring);
    d->m_executingJavaScriptFormAction = true;
    executeScript( urlstring.right( urlstring.length() - 11) );
    d->m_executingJavaScriptFormAction = false;
    return;
  }


  KParts::URLArgs args;

  if (!d->m_referrer.isEmpty())
     args.metaData()["referrer"] = d->m_referrer;

  args.frameName = _target.isEmpty() ? d->m_doc->baseTarget() : _target ;

  // Handle mailto: forms
  if (u.protocol() == "mailto") {
      // 1)  Check for attach= and strip it
      QString q = u.query().mid(1);
      QStringList nvps = QStringList::split("&", q);
      bool triedToAttach = false;

      for (QStringList::Iterator nvp = nvps.begin(); nvp != nvps.end(); ++nvp) {
         QStringList pair = QStringList::split("=", *nvp);
         if (pair.count() >= 2) {
            if (pair.first().lower() == "attach") {
               nvp = nvps.remove(nvp);
               triedToAttach = true;
            }
         }
      }


      // 2)  Append body=
      QString bodyEnc;
      if (contentType.lower() == "multipart/form-data") {
         // FIXME: is this correct?  I suspect not
         bodyEnc = KURL::encode_string(formData.flattenToString());
      } else if (contentType.lower() == "text/plain") {
         // Convention seems to be to decode, and s/&/\n/
         QString tmpbody = formData.flattenToString();
         tmpbody.replace('&', '\n');
         tmpbody.replace('+', ' ');
         tmpbody = KURL::decode_string(tmpbody);  // Decode the rest of it
         bodyEnc = KURL::encode_string(tmpbody);  // Recode for the URL
      } else {
         bodyEnc = KURL::encode_string(formData.flattenToString());
      }

      nvps.append(QString("body=%1").arg(bodyEnc));
      q = nvps.join("&");
      u.setQuery(q);
  } 

  if ( strcmp( action, "get" ) == 0 ) {
    if (u.protocol() != "mailto")
       u.setQuery( formData.flattenToString() );
    args.setDoPost( false );
  }
  else {
    args.postData = formData;
    args.setDoPost( true );

    // construct some user headers if necessary
    if (contentType.isNull() || contentType == "application/x-www-form-urlencoded")
      args.setContentType( "Content-Type: application/x-www-form-urlencoded" );
    else // contentType must be "multipart/form-data"
      args.setContentType( "Content-Type: " + contentType + "; boundary=" + boundary );
  }

  if ( d->m_doc->parsing() || d->m_runningScripts > 0 ) {
    if( d->m_submitForm ) {
      kdDebug(6000) << "KHTMLPart::submitForm ABORTING!" << endl;
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
  {
    KWQ(this)->submitForm( u, args);
  }
}


void KHTMLPart::slotParentCompleted()
{
  if ( d->m_scheduledRedirection != noRedirectionScheduled && !d->m_redirectionTimer.isActive() )
  {
    // kdDebug(6050) << this << ": Child redirection -> " << d->m_redirectURL << endl;
    d->m_redirectionTimer.start( (int)(1000 * d->m_delayRedirect), true );
  }
}

void KHTMLPart::slotChildStarted( KIO::Job *job )
{
  khtml::ChildFrame *child = childFrame( sender() );

  assert( child );

  child->m_bCompleted = false;

  if ( d->m_bComplete )
  {
    d->m_bComplete = false;
    emit started( job );
  }
}

void KHTMLPart::slotChildCompleted()
{
  slotChildCompleted( false );
}

void KHTMLPart::slotChildCompleted( bool complete )
{
  khtml::ChildFrame *child = childFrame( sender() );

  assert( child );

  child->m_bCompleted = true;
  child->m_args = KParts::URLArgs();

  if ( complete && parentPart() == 0 )
    d->m_bPendingChildRedirection = true;

  checkCompleted();
}


khtml::ChildFrame *KHTMLPart::childFrame( const QObject *obj )
{
    assert( obj->inherits( "KParts::ReadOnlyPart" ) );
    const ReadOnlyPart *part = static_cast<const ReadOnlyPart *>( obj );

    FrameIt it = d->m_frames.begin();
    FrameIt end = d->m_frames.end();
    for (; it != end; ++it )
      if ( static_cast<ReadOnlyPart *>((*it).m_part) == part )
        return &(*it);

    it = d->m_objects.begin();
    end = d->m_objects.end();
    for (; it != end; ++it )
      if ( static_cast<ReadOnlyPart *>((*it).m_part) == part )
        return &(*it);

    return 0L;
}

KHTMLPart *KHTMLPart::findFrame( const QString &f )
{

  // ### http://www.w3.org/TR/html4/appendix/notes.html#notes-frames
  ConstFrameIt it = d->m_frames.find( f );
  if ( it == d->m_frames.end() )
  {
    //kdDebug() << "KHTMLPart::findFrame frame " << f << " not found" << endl;
    return 0L;
  }
  else {
    KParts::ReadOnlyPart *p = (*it).m_part;
    if ( p && p->inherits( "KHTMLPart" ))
    {
      //kdDebug() << "KHTMLPart::findFrame frame " << f << " is a KHTMLPart, ok" << endl;
      return (KHTMLPart*)p;
    }
    else
    {
      return 0L;
    }
  }
}


bool KHTMLPart::frameExists( const QString &frameName )
{
  ConstFrameIt it = d->m_frames.find( frameName );
  if ( it == d->m_frames.end() )
    return false;

  // WABA: We only return true if the child actually has a frame
  // set. Otherwise we might find our preloaded-selve.
  // This happens when we restore the frameset.
  return (!(*it).m_frame.isNull());
}

KHTMLPart *KHTMLPart::parentPart() const
{
  if ( !parent() || !parent()->inherits( "KHTMLPart" ) )
    return 0L;

  return (KHTMLPart *)parent();
}


#if !KHTML_NO_CPLUSPLUS_DOM

DOM::Node KHTMLPart::nodeUnderMouse() const
{
    return d->m_view->nodeUnderMouse();
}

#endif

void KHTMLPart::emitSelectionChanged()
{
}

int KHTMLPart::zoomFactor() const
{
  return d->m_zoomFactor;
}

// ### make the list configurable ?
static const int zoomSizes[] = { 20, 40, 60, 80, 90, 95, 100, 105, 110, 120, 140, 160, 180, 200, 250, 300 };
static const int zoomSizeCount = (sizeof(zoomSizes) / sizeof(int));
static const int minZoom = 20;
static const int maxZoom = 300;

void KHTMLPart::slotIncZoom()
{
  int zoomFactor = d->m_zoomFactor;

  if (zoomFactor < maxZoom) {
    // find the entry nearest to the given zoomsizes
    for (int i = 0; i < zoomSizeCount; ++i)
      if (zoomSizes[i] > zoomFactor) {
        zoomFactor = zoomSizes[i];
        break;
      }
    setZoomFactor(zoomFactor);
  }
}

void KHTMLPart::slotDecZoom()
{
    int zoomFactor = d->m_zoomFactor;
    if (zoomFactor > minZoom) {
      // find the entry nearest to the given zoomsizes
      for (int i = zoomSizeCount-1; i >= 0; --i)
        if (zoomSizes[i] < zoomFactor) {
          zoomFactor = zoomSizes[i];
          break;
        }
      setZoomFactor(zoomFactor);
    }
}

void KHTMLPart::setZoomFactor (int percent)
{
  
  if (d->m_zoomFactor == percent) return;
  d->m_zoomFactor = percent;

  if(d->m_doc) {
    d->m_doc->recalcStyle( NodeImpl::Force );
  }

  ConstFrameIt it = d->m_frames.begin();
  ConstFrameIt end = d->m_frames.end();
  for (; it != end; ++it )
    if ( !( *it ).m_part.isNull() && ( *it ).m_part->inherits( "KHTMLPart" ) ) {
      KParts::ReadOnlyPart* p = ( *it ).m_part;
      static_cast<KHTMLPart*>( p )->setZoomFactor(d->m_zoomFactor);
    }


  if (d->m_doc && d->m_doc->renderer() && d->m_doc->renderer()->needsLayout())
    view()->layout();
}

void KHTMLPart::setJSStatusBarText( const QString &text )
{
   d->m_kjsStatusBarText = text;
   emit setStatusBarText( d->m_kjsStatusBarText );
}

void KHTMLPart::setJSDefaultStatusBarText( const QString &text )
{
   d->m_kjsDefaultStatusBarText = text;
   emit setStatusBarText( d->m_kjsDefaultStatusBarText );
}

QString KHTMLPart::jsStatusBarText() const
{
    return d->m_kjsStatusBarText;
}

QString KHTMLPart::jsDefaultStatusBarText() const
{
   return d->m_kjsDefaultStatusBarText;
}

QString KHTMLPart::referrer() const
{
   return d->m_referrer;
}

QString KHTMLPart::lastModified() const
{
  return d->m_lastModified;
}


void KHTMLPart::reparseConfiguration()
{
  setAutoloadImages( d->m_settings->autoLoadImages() );
  if (d->m_doc)
     d->m_doc->docLoader()->setShowAnimations( d->m_settings->showAnimations() );

  d->m_bJScriptEnabled = d->m_settings->isJavaScriptEnabled(m_url.host());
  d->m_bJScriptDebugEnabled = d->m_settings->isJavaScriptDebugEnabled();
  d->m_bJavaEnabled = d->m_settings->isJavaEnabled(m_url.host());
  d->m_bPluginsEnabled = d->m_settings->isPluginsEnabled(m_url.host());

  QString userStyleSheet = d->m_settings->userStyleSheet();
  if ( !userStyleSheet.isEmpty() )
    setUserStyleSheet( KURL( userStyleSheet ) );
  else
    setUserStyleSheet( QString() );

  if(d->m_doc) d->m_doc->updateStyleSelector();
}

QStringList KHTMLPart::frameNames() const
{
  QStringList res;

  ConstFrameIt it = d->m_frames.begin();
  ConstFrameIt end = d->m_frames.end();
  for (; it != end; ++it )
    if (!(*it).m_bPreloaded)
      res += (*it).m_name;

  return res;
}

QPtrList<KParts::ReadOnlyPart> KHTMLPart::frames() const
{
  QPtrList<KParts::ReadOnlyPart> res;

  ConstFrameIt it = d->m_frames.begin();
  ConstFrameIt end = d->m_frames.end();
  for (; it != end; ++it )
    if (!(*it).m_bPreloaded)
      res.append( (*it).m_part );

  return res;
}

KHTMLPart *KHTMLPart::childFrameNamed(const QString &name) const
{
  FrameList::Iterator it = d->m_frames.find(name);
  if (it != d->m_frames.end())
    return static_cast<KHTMLPart *>(&*(*it).m_part);
  return NULL;
}



void KHTMLPart::setDNDEnabled( bool b )
{
  d->m_bDnd = b;
}

bool KHTMLPart::dndEnabled() const
{
  return d->m_bDnd;
}

bool KHTMLPart::shouldDragAutoNode(DOM::NodeImpl *node, int x, int y) const
{
    // No KDE impl yet
    return false;
}

void KHTMLPart::customEvent( QCustomEvent *event )
{
  if ( khtml::MousePressEvent::test( event ) )
  {
    khtmlMousePressEvent( static_cast<khtml::MousePressEvent *>( event ) );
    return;
  }

  if ( khtml::MouseDoubleClickEvent::test( event ) )
  {
    khtmlMouseDoubleClickEvent( static_cast<khtml::MouseDoubleClickEvent *>( event ) );
    return;
  }

  if ( khtml::MouseMoveEvent::test( event ) )
  {
    khtmlMouseMoveEvent( static_cast<khtml::MouseMoveEvent *>( event ) );
    return;
  }

  if ( khtml::MouseReleaseEvent::test( event ) )
  {
    khtmlMouseReleaseEvent( static_cast<khtml::MouseReleaseEvent *>( event ) );
    return;
  }

  if ( khtml::DrawContentsEvent::test( event ) )
  {
    khtmlDrawContentsEvent( static_cast<khtml::DrawContentsEvent *>( event ) );
    return;
  }

  KParts::ReadOnlyPart::customEvent( event );
}

bool KHTMLPart::isPointInsideSelection(int x, int y)
{
    // Treat a collapsed selection like no selection.
    if (!d->m_selection.isRange())
        return false;
    if (!xmlDocImpl()->renderer()) 
        return false;
    
    RenderObject::NodeInfo nodeInfo(true, true);
    xmlDocImpl()->renderer()->layer()->hitTest(nodeInfo, x, y);
    NodeImpl *innerNode = nodeInfo.innerNode();
    if (!innerNode || !innerNode->renderer())
        return false;
    
    Position pos(innerNode->renderer()->positionForCoordinates(x, y).deepEquivalent());
    if (pos.isNull())
        return false;

    NodeImpl *n = d->m_selection.start().node();
    while (n) {
        if (n == pos.node()) {
            if ((n == d->m_selection.start().node() && pos.offset() < d->m_selection.start().offset()) ||
                (n == d->m_selection.end().node() && pos.offset() > d->m_selection.end().offset())) {
                return false;
            }
            return true;
        }
        if (n == d->m_selection.end().node())
            break;
        n = n->traverseNextNode();
    }

   return false;
}

void KHTMLPart::selectClosestWordFromMouseEvent(QMouseEvent *mouse, NodeImpl *innerNode, int x, int y)
{
    SelectionController selection;

    if (innerNode && innerNode->renderer() && mouseDownMayStartSelect() && innerNode->renderer()->shouldSelect()) {
        VisiblePosition pos(innerNode->renderer()->positionForCoordinates(x, y));
        if (pos.isNotNull()) {
            selection.moveTo(pos);
            selection.expandUsingGranularity(WORD);
        }
    }
    
    if (selection.isRange()) {
        d->m_selectionGranularity = WORD;
        d->m_beganSelectingText = true;
    }
    
    if (shouldChangeSelection(selection)) {
        setSelection(selection);
        startAutoScroll();
    }
}

void KHTMLPart::handleMousePressEventDoubleClick(khtml::MousePressEvent *event)
{
    if (event->qmouseEvent()->button() == LeftButton) {
        if (selection().isRange()) {
            // A double-click when range is already selected
            // should not change the selection.  So, do not call
            // selectClosestWordFromMouseEvent, but do set
            // m_beganSelectingText to prevent khtmlMouseReleaseEvent
            // from setting caret selection.
            d->m_beganSelectingText = true;
        } else {
            selectClosestWordFromMouseEvent(event->qmouseEvent(), event->innerNode(), event->x(), event->y());
        }
    }
}

void KHTMLPart::handleMousePressEventTripleClick(khtml::MousePressEvent *event)
{
    QMouseEvent *mouse = event->qmouseEvent();
    NodeImpl *innerNode = event->innerNode();
    
    if (mouse->button() == LeftButton && innerNode && innerNode->renderer() &&
        mouseDownMayStartSelect() && innerNode->renderer()->shouldSelect()) {
        SelectionController selection;
        VisiblePosition pos(innerNode->renderer()->positionForCoordinates(event->x(), event->y()));
        if (pos.isNotNull()) {
            selection.moveTo(pos);
            selection.expandUsingGranularity(PARAGRAPH);
        }
        if (selection.isRange()) {
            d->m_selectionGranularity = PARAGRAPH;
            d->m_beganSelectingText = true;
        }
        
        if (shouldChangeSelection(selection)) {
            setSelection(selection);
            startAutoScroll();
        }
    }
}

void KHTMLPart::handleMousePressEventSingleClick(khtml::MousePressEvent *event)
{
    QMouseEvent *mouse = event->qmouseEvent();
    NodeImpl *innerNode = event->innerNode();
    
    if (mouse->button() == LeftButton) {
        if (innerNode && innerNode->renderer() &&
            mouseDownMayStartSelect() && innerNode->renderer()->shouldSelect()) {
            SelectionController sel;
            
            // Extend the selection if the Shift key is down, unless the click is in a link.
            bool extendSelection = (mouse->state() & ShiftButton) && (event->url().isNull());

            // Don't restart the selection when the mouse is pressed on an
            // existing selection so we can allow for text dragging.
            if (!extendSelection && isPointInsideSelection(event->x(), event->y())) {
                return;
            }

            VisiblePosition visiblePos(innerNode->renderer()->positionForCoordinates(event->x(), event->y()));
            if (visiblePos.isNull())
                visiblePos = VisiblePosition(innerNode, innerNode->caretMinOffset(), khtml::DOWNSTREAM);
            Position pos = visiblePos.deepEquivalent();
            
            sel = selection();
            if (extendSelection && sel.isCaretOrRange()) {
                sel.clearModifyBias();
                
                // See <rdar://problem/3668157> REGRESSION (Mail): shift-click deselects when selection 
                // was created right-to-left
                Position start = sel.start();
                short before = RangeImpl::compareBoundaryPoints(pos.node(), pos.offset(), start.node(), start.offset());
                if (before <= 0) {
                    sel.setBaseAndExtent(pos, visiblePos.affinity(), sel.end(), sel.endAffinity());
                } else {
                    sel.setBaseAndExtent(start, sel.startAffinity(), pos, visiblePos.affinity());
                }

                if (d->m_selectionGranularity != CHARACTER) {
                    sel.expandUsingGranularity(d->m_selectionGranularity);
                }
                d->m_beganSelectingText = true;
            } else {
                sel = SelectionController(visiblePos);
                d->m_selectionGranularity = CHARACTER;
            }
            
            if (shouldChangeSelection(sel)) {
                setSelection(sel);
                startAutoScroll();
            }
        }
    }
}

void KHTMLPart::khtmlMousePressEvent(khtml::MousePressEvent *event)
{
    DOM::DOMString url = event->url();
    QMouseEvent *mouse = event->qmouseEvent();
    NodeImpl *innerNode = event->innerNode();

    d->m_mousePressNode.reset(innerNode);
    d->m_dragStartPos = mouse->pos();

    if (!event->url().isNull()) {
        d->m_strSelectedURL = d->m_strSelectedURLTarget = QString::null;
    }
    else {
        d->m_strSelectedURL = event->url().qstring();
        d->m_strSelectedURLTarget = event->target().qstring();
    }


    if (mouse->button() == LeftButton || mouse->button() == MidButton) {
        d->m_bMousePressed = true;

#ifdef KHTML_NO_SELECTION
        d->m_dragLastPos = mouse->globalPos();
#else
        d->m_beganSelectingText = false;

        if (mouse->clickCount() == 2) {
            handleMousePressEventDoubleClick(event);
            return;
        }
        
        if (mouse->clickCount() >= 3) {
            handleMousePressEventTripleClick(event);
            return;
        }

        handleMousePressEventSingleClick(event);
#endif // KHTML_NO_SELECTION
    }
}

void KHTMLPart::khtmlMouseDoubleClickEvent( khtml::MouseDoubleClickEvent *event)
{
}


void KHTMLPart::handleMouseMoveEventSelection(khtml::MouseMoveEvent *event)
{
    // Mouse not pressed. Do nothing.
    if (!d->m_bMousePressed)
        return;

#ifdef KHTML_NO_SELECTION
    if (d->m_doc && d->m_view) {
        QPoint diff( mouse->globalPos() - d->m_dragLastPos );
		
        if (abs(diff.x()) > 64 || abs(diff.y()) > 64) {
            d->m_view->scrollBy(-diff.x(), -diff.y());
            d->m_dragLastPos = mouse->globalPos();
        }
    }
    return;   
#else

    QMouseEvent *mouse = event->qmouseEvent();
    NodeImpl *innerNode = event->innerNode();

    if (mouse->state() != LeftButton || !innerNode || !innerNode->renderer() ||
        !mouseDownMayStartSelect() || !innerNode->renderer()->shouldSelect())
    	return;

    // handle making selection
    VisiblePosition pos(innerNode->renderer()->positionForCoordinates(event->x(), event->y()));

    // Don't modify the selection if we're not on a node.
    if (pos.isNull())
        return;

    // Restart the selection if this is the first mouse move. This work is usually
    // done in khtmlMousePressEvent, but not if the mouse press was on an existing selection.
    SelectionController sel = selection();
    sel.clearModifyBias();
    if (!d->m_beganSelectingText) {
        d->m_beganSelectingText = true;
        sel.moveTo(pos);
    }

    sel.setExtent(pos);
    if (d->m_selectionGranularity != CHARACTER) {
        sel.expandUsingGranularity(d->m_selectionGranularity);
    }

    if (shouldChangeSelection(sel)) {
        setSelection(sel);
    }

#endif // KHTML_NO_SELECTION
}

void KHTMLPart::khtmlMouseMoveEvent(khtml::MouseMoveEvent *event)
{

    handleMouseMoveEventSelection(event);		
}

void KHTMLPart::khtmlMouseReleaseEvent( khtml::MouseReleaseEvent *event )
{
    if (d->m_bMousePressed)
        stopAutoScroll();
	
    // Used to prevent mouseMoveEvent from initiating a drag before
    // the mouse is pressed again.
    d->m_bMousePressed = false;

#ifndef QT_NO_CLIPBOARD
    QMouseEvent *_mouse = event->qmouseEvent();
    if ((d->m_guiProfile == BrowserViewGUI) && (_mouse->button() == MidButton) && (event->url().isNull())) {
        QClipboard *cb = QApplication::clipboard();
        cb->setSelectionMode(true);
        QCString plain("plain");
        QString url = cb->text(plain).stripWhiteSpace();
        KURL u(url);
        if (u.isMalformed()) {
            // some half-baked guesses for incomplete urls
            // (the same code is in libkonq/konq_dirpart.cc)
            if (url.startsWith("ftp.")) {
                url.prepend("ftp://");
                u = url;
            }
            else {
                url.prepend("http://");
                u = url;
            }
        }
        if (u.isValid()) {
            QString savedReferrer = d->m_referrer;
            d->m_referrer = QString::null; // Disable referrer.
            urlSelected(url, 0,0, "_top");
            d->m_referrer = savedReferrer; // Restore original referrer.
        }
    }
#endif
  
#ifndef KHTML_NO_SELECTION
	
    // Clear the selection if the mouse didn't move after the last mouse press.
    // We do this so when clicking on the selection, the selection goes away.
    // However, if we are editing, place the caret.
    if (mouseDownMayStartSelect() && !d->m_beganSelectingText
            && d->m_dragStartPos.x() == event->qmouseEvent()->x()
            && d->m_dragStartPos.y() == event->qmouseEvent()->y()
            && d->m_selection.isRange()) {
        SelectionController selection;
        NodeImpl *node = event->innerNode();
        if (node && node->isContentEditable() && node->renderer()) {
            VisiblePosition pos = node->renderer()->positionForCoordinates(event->x(), event->y());
            selection.moveTo(pos);
        }
        if (shouldChangeSelection(selection)) {
            setSelection(selection);
        }
    }

#ifndef QT_NO_CLIPBOARD
    // get selected text and paste to the clipboard
    QString text = selectedText();
    text.replace(QRegExp(QChar(0xa0)), " ");
    QClipboard *cb = QApplication::clipboard();
    cb->setSelectionMode(true);
    disconnect(kapp->clipboard(), SIGNAL(selectionChanged()), this, SLOT(slotClearSelection()));
    cb->setText(text);
    connect(kapp->clipboard(), SIGNAL(selectionChanged()), SLOT(slotClearSelection()));
    cb->setSelectionMode(false);
#endif // QT_NO_CLIPBOARD

    selectFrameElementInParentIfFullySelected();

#endif // KHTML_NO_SELECTION
}

void KHTMLPart::khtmlDrawContentsEvent( khtml::DrawContentsEvent * )
{
}


void KHTMLPart::startAutoScroll()
{
}

void KHTMLPart::stopAutoScroll()
{
}


void KHTMLPart::selectAll()
{
    if (!d->m_doc)
        return;
    
    NodeImpl *startNode = d->m_selection.start().node();
    NodeImpl *root = startNode && startNode->isContentEditable() ? startNode->rootEditableElement() : d->m_doc->documentElement();

    SelectionController sel = SelectionController(Position(root, 0), khtml::DOWNSTREAM, Position(root, root->maxDeepOffset()), khtml::DOWNSTREAM);
    
    if (shouldChangeSelection(sel))
        setSelection(sel);
        
    selectFrameElementInParentIfFullySelected();
}

bool KHTMLPart::shouldChangeSelection(const SelectionController &newselection) const
{
    return KWQ(this)->shouldChangeSelection(d->m_selection, newselection, newselection.startAffinity(), false);
}

bool KHTMLPart::shouldBeginEditing(const RangeImpl *range) const
{
    return true;
}

bool KHTMLPart::shouldEndEditing(const RangeImpl *range) const
{
    return true;
}

bool KHTMLPart::isContentEditable() const 
{
    if (!d->m_doc)
        return false;
    return d->m_doc->inDesignMode();
}

EditCommandPtr KHTMLPart::lastEditCommand()
{
    return d->m_lastEditCommand;
}

void KHTMLPart::appliedEditing(EditCommandPtr &cmd)
{
    if (shouldChangeSelection(cmd.endingSelection())) {
        setSelection(cmd.endingSelection(), false);
    }

    // Now set the typing style from the command. Clear it when done.
    // This helps make the case work where you completely delete a piece
    // of styled text and then type a character immediately after.
    // That new character needs to take on the style of the just-deleted text.
    // FIXME: Improve typing style.
    // See this bug: <rdar://problem/3769899> Implementation of typing style needs improvement
    if (cmd.typingStyle()) {
        setTypingStyle(cmd.typingStyle());
        cmd.setTypingStyle(0);
    }

    // Command will be equal to last edit command only in the case of typing
    if (d->m_lastEditCommand == cmd) {
        assert(cmd.isTypingCommand());
    }
    else {
        // Only register a new undo command if the command passed in is
        // different from the last command
        KWQ(this)->registerCommandForUndo(cmd);
        d->m_lastEditCommand = cmd;
    }
    KWQ(this)->respondToChangedContents();
}

void KHTMLPart::unappliedEditing(EditCommandPtr &cmd)
{
    if (shouldChangeSelection(cmd.startingSelection())) {
        setSelection(cmd.startingSelection(), true);
    }
    KWQ(this)->registerCommandForRedo(cmd);
    KWQ(this)->respondToChangedContents();
    d->m_lastEditCommand = EditCommandPtr::emptyCommand();
}

void KHTMLPart::reappliedEditing(EditCommandPtr &cmd)
{
    if (shouldChangeSelection(cmd.endingSelection())) {
        setSelection(cmd.endingSelection(), true);
    }
    KWQ(this)->registerCommandForUndo(cmd);
    KWQ(this)->respondToChangedContents();
    d->m_lastEditCommand = EditCommandPtr::emptyCommand();
}

CSSMutableStyleDeclarationImpl *KHTMLPart::typingStyle() const
{
    return d->m_typingStyle;
}

void KHTMLPart::setTypingStyle(CSSMutableStyleDeclarationImpl *style)
{
    if (d->m_typingStyle == style)
        return;
        
    CSSMutableStyleDeclarationImpl *old = d->m_typingStyle;
    d->m_typingStyle = style;
    if (d->m_typingStyle)
        d->m_typingStyle->ref();
    if (old)
        old->deref();
}

void KHTMLPart::clearTypingStyle()
{
    setTypingStyle(0);
}


QVariant KHTMLPart::executeScript(QString filename, int baseLine, NodeImpl *n, const QString &script)
{
#ifdef KJS_VERBOSE
  kdDebug(6070) << "executeScript: filename=" << filename << " baseLine=" << baseLine << " script=" << script << endl;
#endif
  KJSProxy *proxy = jScript();

  if (!proxy || proxy->paused())
    return QVariant();
  QVariant ret = proxy->evaluate(filename,baseLine,script, n );
  DocumentImpl::updateDocumentsRendering();
  return ret;
}

void KHTMLPart::slotPartRemoved( KParts::Part *part )
{
//    kdDebug(6050) << "KHTMLPart::slotPartRemoved " << part << endl;
    if ( part == d->m_activeFrame )
        d->m_activeFrame = 0L;
}


#if !KHTML_NO_CPLUSPLUS_DOM

void KHTMLPart::setActiveNode(const DOM::Node &node)
{
    if (!d->m_doc || !d->m_view)
        return;

    // Set the document's active node
    d->m_doc->setFocusNode(node.handle());
}

DOM::Node KHTMLPart::activeNode() const
{
    return DOM::Node(d->m_doc?d->m_doc->focusNode():0);
}

#endif

DOM::EventListener *KHTMLPart::createHTMLEventListener( QString code, NodeImpl *node )
{
  KJSProxy *proxy = jScript();

  if (!proxy)
    return 0;

  return proxy->createHTMLEventHandler( m_url.url(), code, node );
}

KHTMLPart *KHTMLPart::opener()
{
    return d->m_opener;
}

void KHTMLPart::setOpener(KHTMLPart *_opener)
{
    d->m_opener = _opener;
}

bool KHTMLPart::openedByJS()
{
    return d->m_openedByJS;
}

void KHTMLPart::setOpenedByJS(bool _openedByJS)
{
    d->m_openedByJS = _openedByJS;
}

void KHTMLPart::preloadStyleSheet(const QString &url, const QString &stylesheet)
{
    khtml::Cache::preloadStyleSheet(url, stylesheet);
}

void KHTMLPart::preloadScript(const QString &url, const QString &script)
{
    khtml::Cache::preloadScript(url, script);
}


bool KHTMLPart::restored() const
{
  return d->m_restored;
}

void KHTMLPart::incrementFrameCount()
{
  frameCount++;
  if (parentPart()) {
    parentPart()->incrementFrameCount();
  }
}

void KHTMLPart::decrementFrameCount()
{
  frameCount--;
  if (parentPart()) {
    parentPart()->decrementFrameCount();
  }
}

int KHTMLPart::topLevelFrameCount()
{
  if (parentPart()) {
    return parentPart()->topLevelFrameCount();
  }

  return frameCount;
}

bool KHTMLPart::tabsToLinks() const
{
    return true;
}

bool KHTMLPart::tabsToAllControls() const
{
    return true;
}

void KHTMLPart::copyToPasteboard()
{
    KWQ(this)->issueCopyCommand();
}

void KHTMLPart::cutToPasteboard()
{
    KWQ(this)->issueCutCommand();
}

void KHTMLPart::pasteFromPasteboard()
{
    KWQ(this)->issuePasteCommand();
}

void KHTMLPart::pasteAndMatchStyle()
{
    KWQ(this)->issuePasteAndMatchStyleCommand();
}

void KHTMLPart::transpose()
{
    KWQ(this)->issueTransposeCommand();
}

void KHTMLPart::redo()
{
    KWQ(this)->issueRedoCommand();
}

void KHTMLPart::undo()
{
    KWQ(this)->issueUndoCommand();
}


void KHTMLPart::computeAndSetTypingStyle(CSSStyleDeclarationImpl *style, EditAction editingAction)
{
    if (!style || style->length() == 0) {
        clearTypingStyle();
        return;
    }

    // Calculate the current typing style.
    CSSMutableStyleDeclarationImpl *mutableStyle = style->makeMutable();
    mutableStyle->ref();
    if (typingStyle()) {
        typingStyle()->merge(mutableStyle);
        mutableStyle->deref();
        mutableStyle = typingStyle();
        mutableStyle->ref();
    }

    NodeImpl *node = VisiblePosition(selection().start(), selection().startAffinity()).deepEquivalent().node();
    CSSComputedStyleDeclarationImpl computedStyle(node);
    computedStyle.diff(mutableStyle);
    
    // Handle block styles, substracting these from the typing style.
    CSSMutableStyleDeclarationImpl *blockStyle = mutableStyle->copyBlockProperties();
    blockStyle->ref();
    blockStyle->diff(mutableStyle);
    if (xmlDocImpl() && blockStyle->length() > 0) {
        EditCommandPtr cmd(new ApplyStyleCommand(xmlDocImpl(), blockStyle, editingAction));
        cmd.apply();
    }
    blockStyle->deref();
    
    // Set the remaining style as the typing style.
    setTypingStyle(mutableStyle);
    mutableStyle->deref();
}

void KHTMLPart::applyStyle(CSSStyleDeclarationImpl *style, EditAction editingAction)
{
    switch (selection().state()) {
        case SelectionController::NONE:
            // do nothing
            break;
        case SelectionController::CARET: {
            computeAndSetTypingStyle(style, editingAction);
            break;
        }
        case SelectionController::RANGE:
            if (xmlDocImpl() && style) {
                EditCommandPtr cmd(new ApplyStyleCommand(xmlDocImpl(), style, editingAction));
                cmd.apply();
            }
            break;
    }
}

void KHTMLPart::applyParagraphStyle(CSSStyleDeclarationImpl *style, EditAction editingAction)
{
    switch (selection().state()) {
        case SelectionController::NONE:
            // do nothing
            break;
        case SelectionController::CARET:
        case SelectionController::RANGE:
            if (xmlDocImpl() && style) {
                EditCommandPtr cmd(new ApplyStyleCommand(xmlDocImpl(), style, editingAction, ApplyStyleCommand::ForceBlockProperties));
                cmd.apply();
            }
            break;
    }
}

static void updateState(CSSMutableStyleDeclarationImpl *desiredStyle, CSSComputedStyleDeclarationImpl *computedStyle, bool &atStart, KHTMLPart::TriState &state)
{
    QValueListConstIterator<CSSProperty> end;
    for (QValueListConstIterator<CSSProperty> it = desiredStyle->valuesIterator(); it != end; ++it) {
        int propertyID = (*it).id();
        DOMString desiredProperty = desiredStyle->getPropertyValue(propertyID);
        DOMString computedProperty = computedStyle->getPropertyValue(propertyID);
        KHTMLPart::TriState propertyState = strcasecmp(desiredProperty, computedProperty) == 0
            ? KHTMLPart::trueTriState : KHTMLPart::falseTriState;
        if (atStart) {
            state = propertyState;
            atStart = false;
        } else if (state != propertyState) {
            state = KHTMLPart::mixedTriState;
            break;
        }
    }
}

KHTMLPart::TriState KHTMLPart::selectionHasStyle(CSSStyleDeclarationImpl *style) const
{
    bool atStart = true;
    TriState state = falseTriState;

    CSSMutableStyleDeclarationImpl *mutableStyle = style->makeMutable();
    SharedPtr<CSSStyleDeclarationImpl> protectQueryStyle(mutableStyle);

    if (!d->m_selection.isRange()) {
        NodeImpl *nodeToRemove;
        CSSComputedStyleDeclarationImpl *selectionStyle = selectionComputedStyle(nodeToRemove);
        if (!selectionStyle)
            return falseTriState;
        selectionStyle->ref();
        updateState(mutableStyle, selectionStyle, atStart, state);
        selectionStyle->deref();
        if (nodeToRemove) {
            int exceptionCode = 0;
            nodeToRemove->remove(exceptionCode);
            assert(exceptionCode == 0);
        }
    } else {
        for (NodeImpl *node = d->m_selection.start().node(); node; node = node->traverseNextNode()) {
            CSSComputedStyleDeclarationImpl *computedStyle = new CSSComputedStyleDeclarationImpl(node);
            if (computedStyle) {
                computedStyle->ref();
                updateState(mutableStyle, computedStyle, atStart, state);
                computedStyle->deref();
            }
            if (state == mixedTriState)
                break;
            if (node == d->m_selection.end().node())
                break;
        }
    }

    return state;
}

bool KHTMLPart::selectionStartHasStyle(CSSStyleDeclarationImpl *style) const
{
    NodeImpl *nodeToRemove;
    CSSStyleDeclarationImpl *selectionStyle = selectionComputedStyle(nodeToRemove);
    if (!selectionStyle)
        return false;

    CSSMutableStyleDeclarationImpl *mutableStyle = style->makeMutable();

    SharedPtr<CSSStyleDeclarationImpl> protectSelectionStyle(selectionStyle);
    SharedPtr<CSSStyleDeclarationImpl> protectQueryStyle(mutableStyle);

    bool match = true;
    QValueListConstIterator<CSSProperty> end;
    for (QValueListConstIterator<CSSProperty> it = mutableStyle->valuesIterator(); it != end; ++it) {
        int propertyID = (*it).id();
        DOMString desiredProperty = mutableStyle->getPropertyValue(propertyID);
        DOMString selectionProperty = selectionStyle->getPropertyValue(propertyID);
        if (strcasecmp(selectionProperty, desiredProperty) != 0) {
            match = false;
            break;
        }
    }

    if (nodeToRemove) {
        int exceptionCode = 0;
        nodeToRemove->remove(exceptionCode);
        assert(exceptionCode == 0);
    }

    return match;
}

DOMString KHTMLPart::selectionStartStylePropertyValue(int stylePropertyID) const
{
    NodeImpl *nodeToRemove;
    CSSStyleDeclarationImpl *selectionStyle = selectionComputedStyle(nodeToRemove);
    if (!selectionStyle)
        return DOMString();

    selectionStyle->ref();
    DOMString value = selectionStyle->getPropertyValue(stylePropertyID);
    selectionStyle->deref();

    if (nodeToRemove) {
        int exceptionCode = 0;
        nodeToRemove->remove(exceptionCode);
        assert(exceptionCode == 0);
    }

    return value;
}

CSSComputedStyleDeclarationImpl *KHTMLPart::selectionComputedStyle(NodeImpl *&nodeToRemove) const
{
    nodeToRemove = 0;

    if (!xmlDocImpl())
        return 0;

    if (d->m_selection.isNone())
        return 0;

    SharedPtr<RangeImpl> range(d->m_selection.toRange());
    Position pos = range->editingStartPosition();

    ElementImpl *elem = pos.element();
    if (!elem)
        return 0;
    
    ElementImpl *styleElement = elem;
    int exceptionCode = 0;

    if (d->m_typingStyle) {
        styleElement = xmlDocImpl()->createElementNS(xhtmlNamespaceURI, "span", exceptionCode);
        assert(exceptionCode == 0);

        styleElement->ref();
        
        styleElement->setAttribute(styleAttr, d->m_typingStyle->cssText().impl(), exceptionCode);
        assert(exceptionCode == 0);
        
        TextImpl *text = xmlDocImpl()->createEditingTextNode("");
        styleElement->appendChild(text, exceptionCode);
        assert(exceptionCode == 0);

        if (elem->renderer() && elem->renderer()->canHaveChildren()) {
            elem->appendChild(styleElement, exceptionCode);
        } else {
            NodeImpl *parent = elem->parent();
            NodeImpl *next = elem->nextSibling();

            if (next) {
                parent->insertBefore(styleElement, next, exceptionCode);
            } else {
                parent->appendChild(styleElement, exceptionCode);
            }
        }
        assert(exceptionCode == 0);

        styleElement->deref();

        nodeToRemove = styleElement;
    }

    return new CSSComputedStyleDeclarationImpl(styleElement);
}

static CSSMutableStyleDeclarationImpl *editingStyle()
{
    static CSSMutableStyleDeclarationImpl *editingStyle = 0;
    if (!editingStyle) {
        editingStyle = new CSSMutableStyleDeclarationImpl;
        int exceptionCode;
        editingStyle->setCssText("word-wrap: break-word; -khtml-nbsp-mode: space; -khtml-line-break: after-white-space;", exceptionCode);
    }
    return editingStyle;
}

void KHTMLPart::applyEditingStyleToBodyElement() const
{
    if (!d->m_doc)
        return;
        
    SharedPtr<NodeListImpl> list = d->m_doc->getElementsByTagName("body");
    unsigned len = list->length();
    for (unsigned i = 0; i < len; i++) {
        applyEditingStyleToElement(static_cast<ElementImpl *>(list->item(i)));    
    }
}

void KHTMLPart::removeEditingStyleFromBodyElement() const
{
    if (!d->m_doc)
        return;
        
    SharedPtr<NodeListImpl> list = d->m_doc->getElementsByTagName("body");
    unsigned len = list->length();
    for (unsigned i = 0; i < len; i++) {
        removeEditingStyleFromElement(static_cast<ElementImpl *>(list->item(i)));    
    }
}

void KHTMLPart::applyEditingStyleToElement(ElementImpl *element) const
{
    if (!element || !element->isHTMLElement())
        return;
        
    RenderObject *renderer = element->renderer();
    if (!renderer || !renderer->isBlockFlow())
        return;
    
    CSSMutableStyleDeclarationImpl *currentStyle = static_cast<HTMLElementImpl *>(element)->getInlineStyleDecl();
    CSSMutableStyleDeclarationImpl *mergeStyle = editingStyle();
    if (mergeStyle) {
        currentStyle->merge(mergeStyle);
        element->setAttribute(styleAttr, currentStyle->cssText());
    }
}

void KHTMLPart::removeEditingStyleFromElement(ElementImpl *element) const
{
    if (!element || !element->isHTMLElement())
        return;
        
    RenderObject *renderer = element->renderer();
    if (!renderer || !renderer->isBlockFlow())
        return;
    
    CSSMutableStyleDeclarationImpl *currentStyle = static_cast<HTMLElementImpl *>(element)->getInlineStyleDecl();
    bool changed = false;
    changed |= !currentStyle->removeProperty(CSS_PROP_WORD_WRAP, false).isNull();
    changed |= !currentStyle->removeProperty(CSS_PROP__KHTML_NBSP_MODE, false).isNull();
    changed |= !currentStyle->removeProperty(CSS_PROP__KHTML_LINE_BREAK, false).isNull();
    if (changed)
        currentStyle->setChanged();

    element->setAttribute(styleAttr, currentStyle->cssText());
}


bool KHTMLPart::isCharacterSmartReplaceExempt(const QChar &, bool)
{
    // no smart replace
    return true;
}

void KHTMLPart::connectChild(const khtml::ChildFrame *child) const
{
    ReadOnlyPart *part = child->m_part;
    if (part && child->m_type != ChildFrame::Object)
    {
        connect( part, SIGNAL( started( KIO::Job *) ),
                 this, SLOT( slotChildStarted( KIO::Job *) ) );
        connect( part, SIGNAL( completed() ),
                 this, SLOT( slotChildCompleted() ) );
        connect( part, SIGNAL( completed(bool) ),
                 this, SLOT( slotChildCompleted(bool) ) );
        connect( part, SIGNAL( setStatusBarText( const QString & ) ),
                 this, SIGNAL( setStatusBarText( const QString & ) ) );
        connect( this, SIGNAL( completed() ),
                 part, SLOT( slotParentCompleted() ) );
        connect( this, SIGNAL( completed(bool) ),
                 part, SLOT( slotParentCompleted() ) );
    }
}

void KHTMLPart::disconnectChild(const khtml::ChildFrame *child) const
{
    ReadOnlyPart *part = child->m_part;
    if (part && child->m_type != ChildFrame::Object)
    {
        disconnect( part, SIGNAL( started( KIO::Job *) ),
                    this, SLOT( slotChildStarted( KIO::Job *) ) );
        disconnect( part, SIGNAL( completed() ),
                    this, SLOT( slotChildCompleted() ) );
        disconnect( part, SIGNAL( completed(bool) ),
                    this, SLOT( slotChildCompleted(bool) ) );
        disconnect( part, SIGNAL( setStatusBarText( const QString & ) ),
                    this, SIGNAL( setStatusBarText( const QString & ) ) );
        disconnect( this, SIGNAL( completed() ),
                    part, SLOT( slotParentCompleted() ) );
        disconnect( this, SIGNAL( completed(bool) ),
                    part, SLOT( slotParentCompleted() ) );
    }
}

void KHTMLPart::keepAlive()
{
    if (d->m_lifeSupportTimer.isActive())
        return;
    ref();
    d->m_lifeSupportTimer.start(0, true);
}

void KHTMLPart::slotEndLifeSupport()
{
    d->m_lifeSupportTimer.stop();
    deref();
}

// Workaround for the fact that it's hard to delete a frame.
// Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
// Can't do this implicitly as part of every setSelection call because in some contexts it might not be good
// for the focus to move to another frame. So instead we call it from places where we are selecting with the
// mouse or the keyboard after setting the selection.
void KHTMLPart::selectFrameElementInParentIfFullySelected()
{
    // Find the parent frame; if there is none, then we have nothing to do.
    KHTMLPart *parent = parentPart();
    if (!parent)
        return;
    KHTMLView *parentView = parent->view();
    if (!parentView)
        return;

    // Check if the selection contains the entire frame contents; if not, then there is nothing to do.
    if (!d->m_selection.isRange())
        return;
    if (!isStartOfDocument(VisiblePosition(d->m_selection.start(), d->m_selection.startAffinity())))
        return;
    if (!isEndOfDocument(VisiblePosition(d->m_selection.end(), d->m_selection.endAffinity())))
        return;

    // Get to the <iframe> or <frame> (or even <object>) element in the parent frame.
    DocumentImpl *document = xmlDocImpl();
    if (!document)
        return;
    ElementImpl *ownerElement = document->ownerElement();
    if (!ownerElement)
        return;
    NodeImpl *ownerElementParent = ownerElement->parentNode();
    if (!ownerElementParent)
        return;

    // Create compute positions before and after the element.
    unsigned ownerElementNodeIndex = ownerElement->nodeIndex();
    VisiblePosition beforeOwnerElement(VisiblePosition(ownerElementParent, ownerElementNodeIndex, khtml::SEL_DEFAULT_AFFINITY));
    VisiblePosition afterOwnerElement(VisiblePosition(ownerElementParent, ownerElementNodeIndex + 1, khtml::VP_UPSTREAM_IF_POSSIBLE));

    // Focus on the parent frame, and then select from before this element to after.
    if (parent->shouldChangeSelection(SelectionController(beforeOwnerElement, afterOwnerElement))) {
        parentView->setFocus();
        parent->setSelection(SelectionController(beforeOwnerElement, afterOwnerElement));
    }
}

void KHTMLPart::handleFallbackContent()
{
    KHTMLPart *parent = parentPart();
    if (!parent)
        return;
    ChildFrame *childFrame = parent->childFrame(this);
    if (!childFrame || childFrame->m_type != ChildFrame::Object)
        return;
    khtml::RenderPart *renderPart = childFrame->m_frame;
    if (!renderPart)
        return;
    NodeImpl *node = renderPart->element();
    if (!node || !node->hasTagName(objectTag))
        return;
    static_cast<HTMLObjectElementImpl *>(node)->renderFallbackContent();
}

using namespace KParts;
#include "khtml_part.moc"
